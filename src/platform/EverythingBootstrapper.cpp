#include "../platform/WindowsCommandLine.hpp"
#include "EverythingBootstrapper.hpp"
#include "EverythingHttpRequest.hpp"

#include "../core/EverythingBootstrapPolicy.hpp"
#include "../core/ArchiveExtractor.hpp"
#include "SecureElevation.hpp"
#include "SecureArchive.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shldisp.h>
#include <winhttp.h>
#include <wintrust.h>
#include <softpub.h>
#include <wincrypt.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace altrun::win {
namespace {

constexpr wchar_t kEverythingWindowClass[] =
    L"EVERYTHING_TASKBAR_NOTIFICATION";

struct InternetHandle {
    HINTERNET value{nullptr};

    ~InternetHandle() {
        if (value) {
            WinHttpCloseHandle(value);
        }
    }

    InternetHandle() = default;
    InternetHandle(const InternetHandle&) = delete;
    InternetHandle& operator=(
        const InternetHandle&) = delete;
};

struct RegistryKey {
    HKEY value{nullptr};

    ~RegistryKey() {
        if (value) {
            RegCloseKey(value);
        }
    }

    RegistryKey() = default;
    RegistryKey(const RegistryKey&) = delete;
    RegistryKey& operator=(
        const RegistryKey&) = delete;
};

struct ServiceHandle {
    SC_HANDLE value{nullptr};

    ~ServiceHandle() {
        if (value) {
            CloseServiceHandle(value);
        }
    }

    ServiceHandle() = default;
    ServiceHandle(const ServiceHandle&) = delete;
    ServiceHandle& operator=(
        const ServiceHandle&) = delete;
};

[[nodiscard]] EverythingPackageArchitecture
CurrentArchitecture() {
#if defined(_M_ARM64) || defined(__aarch64__)
    return EverythingPackageArchitecture::
        Arm64;
#else
    return EverythingPackageArchitecture::
        X64;
#endif
}

[[nodiscard]] std::wstring_view
ArchitectureDirectorySuffix() {
    return CurrentArchitecture() ==
            EverythingPackageArchitecture::
                Arm64
        ? L"-ARM64"
        : L"-x64";
}

[[nodiscard]] std::wstring
VersionDirectoryName(
    std::wstring_view version) {
    return std::wstring(version) +
        std::wstring(
            ArchitectureDirectorySuffix());
}

[[nodiscard]] bool
IsNumericEverythingVersion(
    std::wstring_view version) {
    int components = 0;
    bool hasDigit = false;

    for (const wchar_t ch : version) {
        if (ch >= L'0' &&
            ch <= L'9') {
            hasDigit = true;
            continue;
        }

        if (ch != L'.' ||
            !hasDigit) {
            return false;
        }

        ++components;
        hasDigit = false;
    }

    return hasDigit &&
        components == 3;
}

[[nodiscard]] std::optional<std::wstring>
VersionFromManagedExecutablePath(
    const std::filesystem::path& executable) {
    std::wstring fileName =
        executable.filename().wstring();

    std::transform(
        fileName.begin(),
        fileName.end(),
        fileName.begin(),
        [](wchar_t ch) {
            return static_cast<wchar_t>(
                std::towlower(ch));
        });

    if (fileName !=
        L"everything.exe") {
        return std::nullopt;
    }

    const std::wstring directory =
        executable.parent_path()
            .filename()
            .wstring();
    const std::wstring suffix(
        ArchitectureDirectorySuffix());

    if (directory.size() <=
            suffix.size() ||
        directory.substr(
            directory.size() -
                suffix.size()) !=
            suffix) {
        return std::nullopt;
    }

    std::wstring version =
        directory.substr(
            0,
            directory.size() -
                suffix.size());

    if (!IsNumericEverythingVersion(
            version)) {
        return std::nullopt;
    }

    return version;
}

void Report(
    EverythingBootstrapSnapshot& snapshot,
    EverythingBootstrapStage stage,
    const EverythingBootstrapProgress&
        progress) {
    snapshot.stage = stage;

    if (progress) {
        progress(snapshot);
    }
}

[[nodiscard]] std::wstring
EnvironmentVariable(
    const wchar_t* name) {
    const DWORD required =
        GetEnvironmentVariableW(
            name,
            nullptr,
            0);

    if (required == 0) {
        return {};
    }

    std::wstring value(
        required,
        L'\0');

    const DWORD written =
        GetEnvironmentVariableW(
            name,
            value.data(),
            required);

    if (written == 0 ||
        written >= required) {
        return {};
    }

    value.resize(written);
    return value;
}

[[nodiscard]] bool
FileExists(
    const std::filesystem::path& path) {
    std::error_code ec;
    return std::filesystem::is_regular_file(
               path,
               ec) &&
        !ec;
}

[[nodiscard]] std::filesystem::path
ManagedEverythingRoot(
    const std::filesystem::path&
        dataDirectory) {
    return dataDirectory /
        L"tools" /
        L"Everything";
}

[[nodiscard]] std::filesystem::path
ManagedEverythingExecutableForVersion(
    const std::filesystem::path&
        dataDirectory,
    std::wstring_view version) {
    return ManagedEverythingRoot(
               dataDirectory) /
        VersionDirectoryName(
            version) /
        L"Everything.exe";
}

[[nodiscard]] std::wstring
LowerPath(
    const std::filesystem::path& path) {
    std::wstring value =
        path.lexically_normal().wstring();

    std::transform(
        value.begin(),
        value.end(),
        value.begin(),
        [](wchar_t c) {
            if (c == L'/') {
                return L'\\';
            }
            return static_cast<wchar_t>(
                std::towlower(c));
        });

    return value;
}

[[nodiscard]] bool
PathStartsWithDirectory(
    const std::filesystem::path& path,
    const std::filesystem::path& directory) {
    if (path.empty() ||
        directory.empty()) {
        return false;
    }

    const std::wstring value =
        LowerPath(path);
    std::wstring prefix =
        LowerPath(directory);

    if (value == prefix) {
        return true;
    }

    if (!prefix.empty() &&
        prefix.back() != L'\\') {
        prefix.push_back(L'\\');
    }

    return value.starts_with(prefix);
}

[[nodiscard]] std::filesystem::path
ManagedEverythingServiceHostRoot() {
    PWSTR programFiles = nullptr;

    const HRESULT result =
        SHGetKnownFolderPath(
            FOLDERID_ProgramFiles,
            KF_FLAG_DEFAULT,
            nullptr,
            &programFiles);

    if (FAILED(result) ||
        !programFiles ||
        !*programFiles) {
        if (programFiles) {
            CoTaskMemFree(
                programFiles);
        }
        return {};
    }

    const std::filesystem::path root =
        std::filesystem::path(
            programFiles) /
        L"Aspeternity" /
        L"Asterun" /
        L"EverythingService";

    CoTaskMemFree(
        programFiles);
    return root;
}

[[nodiscard]] std::filesystem::path
ManagedEverythingServiceExecutableForSource(
    const std::filesystem::path& source) {
    const auto version =
        VersionFromManagedExecutablePath(
            source);
    const auto root =
        ManagedEverythingServiceHostRoot();

    if (!version ||
        root.empty()) {
        return {};
    }

    return root /
        VersionDirectoryName(
            *version) /
        L"Everything.exe";
}

[[nodiscard]] bool
ProtectedManagedEverythingServiceExecutable(
    const std::filesystem::path& executable) {
    const auto root =
        ManagedEverythingServiceHostRoot();

    return !root.empty() &&
        PathStartsWithDirectory(
            executable,
            root) &&
        VersionFromManagedExecutablePath(
            executable)
            .has_value();
}

[[nodiscard]] bool
PortableManagedEverythingExecutable(
    const std::filesystem::path& dataDirectory,
    const std::filesystem::path& executable) {
    return
        PathStartsWithDirectory(
            executable,
            ManagedEverythingRoot(
                dataDirectory)) &&
        VersionFromManagedExecutablePath(
            executable)
            .has_value();
}

struct ExistingCandidate {
    EverythingBootstrapSource source{
        EverythingBootstrapSource::None};
    std::filesystem::path path;
};

void AddCandidate(
    std::vector<ExistingCandidate>& candidates,
    EverythingBootstrapSource source,
    std::filesystem::path path) {
    if (path.empty() ||
        !FileExists(path)) {
        return;
    }

    const std::wstring key =
        LowerPath(path);

    const bool duplicate =
        std::any_of(
            candidates.begin(),
            candidates.end(),
            [&](const ExistingCandidate& item) {
                return LowerPath(item.path) ==
                    key;
            });

    if (!duplicate) {
        candidates.push_back({
            source,
            std::move(path),
        });
    }
}

[[nodiscard]] std::wstring
ReadRegistryDefault(
    HKEY root,
    REGSAM view) {
    RegistryKey key;

    if (RegOpenKeyExW(
            root,
            L"Software\\Microsoft\\Windows\\CurrentVersion\\App Paths\\Everything.exe",
            0,
            KEY_READ | view,
            &key.value) != ERROR_SUCCESS) {
        return {};
    }

    DWORD type = 0;
    DWORD bytes = 0;

    if (RegQueryValueExW(
            key.value,
            nullptr,
            nullptr,
            &type,
            nullptr,
            &bytes) != ERROR_SUCCESS ||
        bytes == 0 ||
        (type != REG_SZ &&
         type != REG_EXPAND_SZ)) {
        return {};
    }

    std::wstring value(
        bytes / sizeof(wchar_t),
        L'\0');

    if (RegQueryValueExW(
            key.value,
            nullptr,
            nullptr,
            &type,
            reinterpret_cast<BYTE*>(
                value.data()),
            &bytes) != ERROR_SUCCESS) {
        return {};
    }

    while (!value.empty() &&
           value.back() == L'\0') {
        value.pop_back();
    }

    if (type == REG_EXPAND_SZ) {
        const DWORD needed =
            ExpandEnvironmentStringsW(
                value.c_str(),
                nullptr,
                0);

        if (needed > 0) {
            std::wstring expanded(
                needed,
                L'\0');

            if (ExpandEnvironmentStringsW(
                    value.c_str(),
                    expanded.data(),
                    needed) > 0) {
                if (!expanded.empty() &&
                    expanded.back() ==
                        L'\0') {
                    expanded.pop_back();
                }
                value =
                    std::move(expanded);
            }
        }
    }

    if (value.size() >= 2 &&
        value.front() == L'"' &&
        value.back() == L'"') {
        value =
            value.substr(
                1,
                value.size() - 2);
    }

    return value;
}

[[nodiscard]]
std::vector<ExistingCandidate>
FindExistingCandidates(
    const std::filesystem::path&
        dataDirectory) {
    std::vector<ExistingCandidate>
        candidates;

    AddCandidate(
        candidates,
        EverythingBootstrapSource::
            Managed,
        ManagedEverythingExecutable(
            dataDirectory));

    for (const auto view :
         std::array<REGSAM, 2>{
             KEY_WOW64_64KEY,
             KEY_WOW64_32KEY}) {
        for (const auto root :
             std::array<HKEY, 2>{
                 HKEY_CURRENT_USER,
                 HKEY_LOCAL_MACHINE}) {
            const std::wstring value =
                ReadRegistryDefault(
                    root,
                    view);

            if (!value.empty()) {
                AddCandidate(
                    candidates,
                    EverythingBootstrapSource::
                        Registry,
                    value);
            }
        }
    }

    for (const auto* variable :
         std::array<const wchar_t*, 3>{
             L"ProgramFiles",
             L"ProgramFiles(x86)",
             L"ProgramW6432"}) {
        const std::wstring root =
            EnvironmentVariable(
                variable);

        if (!root.empty()) {
            AddCandidate(
                candidates,
                EverythingBootstrapSource::
                    ProgramFiles,
                std::filesystem::path(
                    root) /
                    L"Everything" /
                    L"Everything.exe");
        }
    }

    std::array<wchar_t, 32768>
        found{};

    const DWORD length =
        SearchPathW(
            nullptr,
            L"Everything.exe",
            nullptr,
            static_cast<DWORD>(
                found.size()),
            found.data(),
            nullptr);

    if (length > 0 &&
        length < found.size()) {
        AddCandidate(
            candidates,
            EverythingBootstrapSource::Path,
            std::wstring(
                found.data(),
                length));
    }

    return candidates;
}

[[nodiscard]] bool
LaunchEverythingCommand(
    const std::filesystem::path& executable,
    std::wstring_view arguments,
    bool wait,
    std::uint32_t& nativeError) {
    std::wstring command =
        L"\"" +
        executable.wstring() +
        L"\"";

    if (!arguments.empty()) {
        command += L" ";
        command += arguments;
    }

    std::vector<wchar_t> mutableCommand(
        command.begin(),
        command.end());
    mutableCommand.push_back(L'\0');

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};

    const std::wstring directory =
        executable.parent_path()
            .wstring();

    if (!CreateProcessW(
            executable.c_str(),
            mutableCommand.data(),
            nullptr,
            nullptr,
            FALSE,
            CREATE_UNICODE_ENVIRONMENT |
                CREATE_NO_WINDOW,
            nullptr,
            directory.empty()
                ? nullptr
                : directory.c_str(),
            &startup,
            &process)) {
        nativeError =
            static_cast<std::uint32_t>(
                GetLastError());
        return false;
    }

    CloseHandle(process.hThread);

    if (wait) {
        const DWORD waitResult =
            WaitForSingleObject(
                process.hProcess,
                15000);

        if (waitResult != WAIT_OBJECT_0) {
            nativeError =
                waitResult == WAIT_TIMEOUT
                    ? ERROR_TIMEOUT
                    : static_cast<std::uint32_t>(
                          GetLastError());
            CloseHandle(process.hProcess);
            return false;
        }

        DWORD exitCode = 0;

        if (!GetExitCodeProcess(
                process.hProcess,
                &exitCode)) {
            nativeError =
                static_cast<std::uint32_t>(
                    GetLastError());
            CloseHandle(process.hProcess);
            return false;
        }

        if (exitCode != 0) {
            nativeError = exitCode;
            CloseHandle(process.hProcess);
            return false;
        }
    }

    CloseHandle(process.hProcess);
    nativeError = 0;
    return true;
}

[[nodiscard]] bool
LaunchEverything(
    const std::filesystem::path& executable,
    std::uint32_t& nativeError) {
    return LaunchEverythingCommand(
        executable,
        L"-startup -first-instance",
        false,
        nativeError);
}

[[nodiscard]] std::wstring
QuoteElevationArgument(std::wstring_view value) {
    return QuoteWindowsArgument(value);
}
[[nodiscard]] bool
RunGuardedElevatedExecutable(
    const std::filesystem::path& executable,
    std::wstring_view arguments,
    std::uint32_t& nativeError) {
    SecuredExecutable secured;

    if (!LockExecutableForElevation(
            executable,
            secured,
            nativeError)) {
        return false;
    }

    HANDLE process = nullptr;

    if (!LaunchSecuredExecutable(
            secured,
            arguments,
            true,
            SW_HIDE,
            process,
            nativeError)) {
        return false;
    }

    // The guard is required through UAC consent and process creation. Once
    // ShellExecuteEx returns a real process handle, the child image has been
    // opened and the original path can be released.
    secured.Reset();

    const DWORD waitResult =
        WaitForSingleObject(
            process,
            30000);

    if (waitResult !=
        WAIT_OBJECT_0) {
        nativeError =
            waitResult ==
                    WAIT_TIMEOUT
                ? ERROR_TIMEOUT
                : static_cast<
                      std::uint32_t>(
                      GetLastError());
        CloseHandle(process);
        return false;
    }

    DWORD exitCode = 0;

    if (!GetExitCodeProcess(
            process,
            &exitCode)) {
        nativeError =
            static_cast<std::uint32_t>(
                GetLastError());
        CloseHandle(process);
        return false;
    }

    CloseHandle(process);

    if (exitCode != 0) {
        nativeError = exitCode;
        return false;
    }

    nativeError = 0;
    return true;
}

[[nodiscard]]
std::filesystem::path
CurrentExecutableForElevation(
    std::uint32_t& nativeError) {
    std::array<wchar_t, 32768>
        executableBuffer{};

    const DWORD length =
        GetModuleFileNameW(
            nullptr,
            executableBuffer.data(),
            static_cast<DWORD>(
                executableBuffer.size()));

    if (length == 0 ||
        static_cast<std::size_t>(
            length) >=
            executableBuffer.size()) {
        nativeError =
            length == 0
                ? static_cast<
                      std::uint32_t>(
                      GetLastError())
                : ERROR_INSUFFICIENT_BUFFER;
        return {};
    }

    nativeError = 0;
    return std::filesystem::path(
        std::wstring(
            executableBuffer.data(),
            length));
}

[[nodiscard]] bool
RunElevatedServiceRepairHelper(
    const std::filesystem::path&
        managedSource,
    std::uint32_t& nativeError) {
    SecuredExecutable sourceGuard;

    // The normal-integrity launcher keeps the exact managed Everything source
    // locked for the entire elevated helper lifetime. The helper receives the
    // same explicit path, so a newer/malicious sibling cannot win a rescan
    // race while UAC is pending.
    if (!LockExecutableForElevation(
            managedSource,
            sourceGuard,
            nativeError)) {
        return false;
    }

    const auto executable =
        CurrentExecutableForElevation(
            nativeError);

    if (executable.empty()) {
        return false;
    }

    const std::wstring arguments =
        L"--repair-managed-everything-service " +
        QuoteElevationArgument(
            managedSource.wstring());

    return RunGuardedElevatedExecutable(
        executable,
        arguments,
        nativeError);
}

[[nodiscard]] bool
RunElevatedServicePolicyHelper(
    bool enabled,
    const std::filesystem::path&
        managedSource,
    std::uint32_t& nativeError) {
    SecuredExecutable sourceGuard;

    if (enabled &&
        !LockExecutableForElevation(
            managedSource,
            sourceGuard,
            nativeError)) {
        return false;
    }

    const auto executable =
        CurrentExecutableForElevation(
            nativeError);

    if (executable.empty()) {
        return false;
    }

    std::wstring arguments =
        enabled
            ? L"--set-managed-everything-service enabled"
            : L"--set-managed-everything-service disabled";

    if (enabled) {
        arguments += L" ";
        arguments +=
            QuoteElevationArgument(
                managedSource.wstring());
    }

    return RunGuardedElevatedExecutable(
        executable,
        arguments,
        nativeError);
}

struct NamedIpcSearch {
    std::uint32_t count{0};
};

BOOL CALLBACK CountNamedIpc(
    HWND hwnd,
    LPARAM lParam) {
    auto* search =
        reinterpret_cast<
            NamedIpcSearch*>(
                lParam);

    std::array<wchar_t, 512>
        className{};

    const int length =
        GetClassNameW(
            hwnd,
            className.data(),
            static_cast<int>(
                className.size()));

    if (length <= 0) {
        return TRUE;
    }

    const std::wstring_view value(
        className.data(),
        static_cast<std::size_t>(
            length));

    constexpr std::wstring_view
        prefix =
            L"EVERYTHING_TASKBAR_NOTIFICATION_(";

    if (value.starts_with(prefix) &&
        value.ends_with(L")")) {
        ++search->count;
    }

    return TRUE;
}

[[nodiscard]] bool
AnyUsableIpcEndpoint() {
    if (FindWindowW(
            kEverythingWindowClass,
            nullptr)) {
        return true;
    }

    NamedIpcSearch search;

    EnumWindows(
        CountNamedIpc,
        reinterpret_cast<LPARAM>(
            &search));

    return search.count == 1;
}

[[nodiscard]] bool
WaitForIpc(
    std::stop_token stopToken) {
    constexpr auto timeout =
        std::chrono::seconds(8);
    constexpr auto interval =
        std::chrono::milliseconds(100);

    const auto deadline =
        std::chrono::steady_clock::now() +
        timeout;

    while (!stopToken.stop_requested() &&
           std::chrono::steady_clock::now() <
               deadline) {
        if (AnyUsableIpcEndpoint()) {
            return true;
        }

        std::this_thread::sleep_for(
            interval);
    }

    return false;
}

enum class ServiceProbe {
    Missing,
    Stopped,
    Starting,
    Running,
    Error,
};

[[nodiscard]] ServiceProbe
ProbeEverythingService(
    std::uint32_t& nativeError) {
    ServiceHandle manager;
    manager.value =
        OpenSCManagerW(
            nullptr,
            nullptr,
            SC_MANAGER_CONNECT);

    if (!manager.value) {
        nativeError =
            static_cast<std::uint32_t>(
                GetLastError());
        return ServiceProbe::Error;
    }

    ServiceHandle service;
    service.value =
        OpenServiceW(
            manager.value,
            L"Everything",
            SERVICE_QUERY_STATUS);

    if (!service.value) {
        nativeError =
            static_cast<std::uint32_t>(
                GetLastError());

        if (IsEverythingServiceMissingError(
                nativeError)) {
            nativeError = 0;
            return ServiceProbe::Missing;
        }

        return ServiceProbe::Error;
    }

    SERVICE_STATUS_PROCESS status{};
    DWORD bytesNeeded = 0;

    if (!QueryServiceStatusEx(
            service.value,
            SC_STATUS_PROCESS_INFO,
            reinterpret_cast<LPBYTE>(
                &status),
            sizeof(status),
            &bytesNeeded)) {
        nativeError =
            static_cast<std::uint32_t>(
                GetLastError());
        return ServiceProbe::Error;
    }

    nativeError = 0;

    if (status.dwCurrentState ==
        SERVICE_RUNNING) {
        return ServiceProbe::Running;
    }

    if (status.dwCurrentState ==
        SERVICE_START_PENDING) {
        return ServiceProbe::Starting;
    }

    return ServiceProbe::Stopped;
}

[[nodiscard]] bool
QueryEverythingServiceExecutable(
    std::filesystem::path& executable,
    bool& executableExists,
    std::uint32_t& nativeError) {
    ServiceHandle manager;
    manager.value =
        OpenSCManagerW(
            nullptr,
            nullptr,
            SC_MANAGER_CONNECT);

    if (!manager.value) {
        nativeError =
            static_cast<std::uint32_t>(
                GetLastError());
        return false;
    }

    ServiceHandle service;
    service.value =
        OpenServiceW(
            manager.value,
            L"Everything",
            SERVICE_QUERY_CONFIG);

    if (!service.value) {
        nativeError =
            static_cast<std::uint32_t>(
                GetLastError());
        return false;
    }

    DWORD required = 0;

    QueryServiceConfigW(
        service.value,
        nullptr,
        0,
        &required);

    if (required == 0 ||
        GetLastError() !=
            ERROR_INSUFFICIENT_BUFFER) {
        nativeError =
            static_cast<std::uint32_t>(
                GetLastError());
        return false;
    }

    std::vector<std::max_align_t>
        buffer(
            (required +
             sizeof(std::max_align_t) - 1) /
            sizeof(std::max_align_t));

    auto* config =
        reinterpret_cast<
            QUERY_SERVICE_CONFIGW*>(
                buffer.data());

    if (!QueryServiceConfigW(
            service.value,
            config,
            required,
            &required)) {
        nativeError =
            static_cast<std::uint32_t>(
                GetLastError());
        return false;
    }

    if (!config->lpBinaryPathName) {
        nativeError =
            ERROR_INVALID_DATA;
        return false;
    }

    auto parsed =
        ExtractEverythingServiceExecutable(
            config->lpBinaryPathName);

    if (parsed.empty()) {
        nativeError =
            ERROR_INVALID_DATA;
        return false;
    }

    const DWORD expandedSize =
        ExpandEnvironmentStringsW(
            parsed.c_str(),
            nullptr,
            0);

    if (expandedSize == 0) {
        nativeError =
            static_cast<std::uint32_t>(
                GetLastError());
        return false;
    }

    std::vector<wchar_t>
        expanded(expandedSize);

    const DWORD written =
        ExpandEnvironmentStringsW(
            parsed.c_str(),
            expanded.data(),
            expandedSize);

    if (written == 0 ||
        written > expandedSize) {
        nativeError =
            written == 0
                ? static_cast<std::uint32_t>(
                      GetLastError())
                : ERROR_INSUFFICIENT_BUFFER;
        return false;
    }

    executable =
        std::filesystem::path(
            expanded.data());
    executableExists =
        FileExists(executable);
    nativeError = 0;
    return true;
}

[[nodiscard]] bool
QueryEverythingServiceStartType(
    std::uint32_t& startType,
    std::uint32_t& nativeError) {
    ServiceHandle manager;
    manager.value =
        OpenSCManagerW(
            nullptr,
            nullptr,
            SC_MANAGER_CONNECT);

    if (!manager.value) {
        nativeError =
            static_cast<std::uint32_t>(
                GetLastError());
        return false;
    }

    ServiceHandle service;
    service.value =
        OpenServiceW(
            manager.value,
            L"Everything",
            SERVICE_QUERY_CONFIG);

    if (!service.value) {
        nativeError =
            static_cast<std::uint32_t>(
                GetLastError());
        return false;
    }

    DWORD required = 0;

    QueryServiceConfigW(
        service.value,
        nullptr,
        0,
        &required);

    if (required == 0 ||
        GetLastError() !=
            ERROR_INSUFFICIENT_BUFFER) {
        nativeError =
            static_cast<std::uint32_t>(
                GetLastError());
        return false;
    }

    std::vector<std::max_align_t>
        buffer(
            (required +
             sizeof(std::max_align_t) - 1) /
            sizeof(std::max_align_t));

    auto* config =
        reinterpret_cast<
            QUERY_SERVICE_CONFIGW*>(
                buffer.data());

    if (!QueryServiceConfigW(
            service.value,
            config,
            required,
            &required)) {
        nativeError =
            static_cast<std::uint32_t>(
                GetLastError());
        return false;
    }

    startType =
        config->dwStartType;
    nativeError = 0;
    return true;
}

[[nodiscard]] bool
WaitForEverythingService(
    std::stop_token stopToken,
    std::uint32_t& nativeError) {
    const auto deadline =
        std::chrono::steady_clock::now() +
        std::chrono::seconds(15);

    while (!stopToken.stop_requested() &&
           std::chrono::steady_clock::now() <
               deadline) {
        const auto status =
            ProbeEverythingService(
                nativeError);

        if (status ==
            ServiceProbe::Running) {
            nativeError = 0;
            return true;
        }

        if (status ==
            ServiceProbe::Error) {
            return false;
        }

        std::this_thread::sleep_for(
            std::chrono::milliseconds(
                150));
    }

    nativeError =
        stopToken.stop_requested()
            ? ERROR_CANCELLED
            : ERROR_SERVICE_REQUEST_TIMEOUT;
    return false;
}

[[nodiscard]] bool
WaitForEverythingServiceStopped(
    std::stop_token stopToken,
    std::uint32_t& nativeError) {
    ServiceHandle manager;
    manager.value =
        OpenSCManagerW(
            nullptr,
            nullptr,
            SC_MANAGER_CONNECT);

    if (!manager.value) {
        nativeError =
            static_cast<std::uint32_t>(
                GetLastError());
        return false;
    }

    ServiceHandle service;
    service.value =
        OpenServiceW(
            manager.value,
            L"Everything",
            SERVICE_QUERY_STATUS);

    if (!service.value) {
        nativeError =
            static_cast<std::uint32_t>(
                GetLastError());

        if (IsEverythingServiceMissingError(
                nativeError)) {
            nativeError = 0;
            return true;
        }

        return false;
    }

    const auto deadline =
        std::chrono::steady_clock::now() +
        std::chrono::seconds(15);

    while (!stopToken.stop_requested() &&
           std::chrono::steady_clock::now() <
               deadline) {
        SERVICE_STATUS_PROCESS status{};
        DWORD bytesNeeded = 0;

        if (!QueryServiceStatusEx(
                service.value,
                SC_STATUS_PROCESS_INFO,
                reinterpret_cast<LPBYTE>(
                    &status),
                sizeof(status),
                &bytesNeeded)) {
            nativeError =
                static_cast<std::uint32_t>(
                    GetLastError());
            return false;
        }

        if (status.dwCurrentState ==
            SERVICE_STOPPED) {
            nativeError = 0;
            return true;
        }

        std::this_thread::sleep_for(
            std::chrono::milliseconds(
                150));
    }

    nativeError =
        stopToken.stop_requested()
            ? ERROR_CANCELLED
            : ERROR_SERVICE_REQUEST_TIMEOUT;
    return false;
}

[[nodiscard]] std::optional<
    std::filesystem::path>
ExecutableForWindow(
    HWND hwnd) {
    if (!hwnd) {
        return std::nullopt;
    }

    DWORD processId = 0;
    GetWindowThreadProcessId(
        hwnd,
        &processId);

    if (processId == 0) {
        return std::nullopt;
    }

    HANDLE process =
        OpenProcess(
            PROCESS_QUERY_LIMITED_INFORMATION,
            FALSE,
            processId);

    if (!process) {
        return std::nullopt;
    }

    std::array<wchar_t, 32768>
        buffer{};
    DWORD size =
        static_cast<DWORD>(
            buffer.size());

    const BOOL ok =
        QueryFullProcessImageNameW(
            process,
            0,
            buffer.data(),
            &size);

    CloseHandle(process);

    if (!ok ||
        size == 0) {
        return std::nullopt;
    }

    return std::filesystem::path(
        std::wstring(
            buffer.data(),
            size));
}

[[nodiscard]] std::optional<
    std::filesystem::path>
DefaultIpcExecutable() {
    return ExecutableForWindow(
        FindWindowW(
            kEverythingWindowClass,
            nullptr));
}

[[nodiscard]] bool
WindowOwnedByExecutable(
    HWND hwnd,
    const std::filesystem::path& executable) {
    const auto actual =
        ExecutableForWindow(hwnd);

    return actual &&
        LowerPath(*actual) ==
            LowerPath(executable);
}

[[nodiscard]] bool
ManagedDefaultIpcRunning(
    const std::filesystem::path& executable) {
    return WindowOwnedByExecutable(
        FindWindowW(
            kEverythingWindowClass,
            nullptr),
        executable);
}

[[nodiscard]] std::optional<
    std::filesystem::path>
ActiveManagedEverythingExecutable(
    const std::filesystem::path&
        dataDirectory) {
    const auto active =
        DefaultIpcExecutable();

    if (!active ||
        !IsManagedEverythingServiceExecutable(
            dataDirectory,
            *active)) {
        return std::nullopt;
    }

    return *active;
}

[[nodiscard]] bool
WaitForManagedDefaultIpcToExit(
    const std::filesystem::path& executable,
    std::stop_token stopToken) {
    const auto deadline =
        std::chrono::steady_clock::now() +
        std::chrono::seconds(8);

    while (!stopToken.stop_requested() &&
           std::chrono::steady_clock::now() <
               deadline) {
        if (!ManagedDefaultIpcRunning(
                executable)) {
            return true;
        }

        std::this_thread::sleep_for(
            std::chrono::milliseconds(
                100));
    }

    return false;
}

[[nodiscard]] bool
ReadManagedIni(
    const std::filesystem::path& iniPath,
    std::string& content,
    std::uint32_t& nativeError) {
    std::error_code ec;

    if (!std::filesystem::exists(
            iniPath,
            ec)) {
        if (ec) {
            nativeError =
                static_cast<std::uint32_t>(
                    ec.value());
            return false;
        }

        content.clear();
        nativeError = 0;
        return true;
    }

    std::ifstream input(
        iniPath,
        std::ios::binary);

    if (!input) {
        nativeError =
            ERROR_OPEN_FAILED;
        return false;
    }

    content.assign(
        std::istreambuf_iterator<char>(
            input),
        std::istreambuf_iterator<char>());

    if (!input.good() &&
        !input.eof()) {
        nativeError =
            ERROR_READ_FAULT;
        return false;
    }

    nativeError = 0;
    return true;
}

[[nodiscard]] bool
ManagedIniNeedsUpdate(
    const std::filesystem::path& executable,
    bool showTrayIcon,
    bool& needsUpdate,
    std::uint32_t& nativeError) {
    const auto iniPath =
        executable.parent_path() /
        L"Everything.ini";

    std::string existing;

    if (!ReadManagedIni(
            iniPath,
            existing,
            nativeError)) {
        return false;
    }

    needsUpdate =
        ApplyManagedEverythingIniPolicy(
            existing,
            showTrayIcon) != existing;
    nativeError = 0;
    return true;
}

[[nodiscard]] bool
ConfigureManagedEverything(
    const std::filesystem::path& executable,
    bool showTrayIcon,
    std::uint32_t& nativeError) {
    const auto iniPath =
        executable.parent_path() /
        L"Everything.ini";
    const auto tempPath =
        executable.parent_path() /
        L"Everything.ini.asterun.tmp";

    std::string existing;

    if (!ReadManagedIni(
            iniPath,
            existing,
            nativeError)) {
        return false;
    }

    const std::string configured =
        ApplyManagedEverythingIniPolicy(
            existing,
            showTrayIcon);

    if (configured == existing) {
        nativeError = 0;
        return true;
    }

    {
        std::ofstream output(
            tempPath,
            std::ios::binary |
                std::ios::trunc);

        if (!output) {
            nativeError =
                ERROR_OPEN_FAILED;
            return false;
        }

        output.write(
            configured.data(),
            static_cast<
                std::streamsize>(
                configured.size()));
        output.flush();

        if (!output) {
            nativeError =
                ERROR_WRITE_FAULT;
            return false;
        }
    }

    if (!MoveFileExW(
            tempPath.c_str(),
            iniPath.c_str(),
            MOVEFILE_REPLACE_EXISTING |
                MOVEFILE_WRITE_THROUGH)) {
        nativeError =
            static_cast<std::uint32_t>(
                GetLastError());

        std::error_code cleanup;
        std::filesystem::remove(
            tempPath,
            cleanup);
        return false;
    }

    nativeError = 0;
    return true;
}

enum class ManagedRuntimeResult {
    Ready,
    NeedsService,
    NeedsServiceRepair,
    Cancelled,
    StopFailed,
    ConfigFailed,
    ServiceElevationCancelled,
    ServiceInstallFailed,
    ServiceRepairFailed,
    ServiceUnavailable,
    LaunchFailed,
    IpcUnavailable,
};

struct ManagedRuntimeOutcome {
    ManagedRuntimeResult result{
        ManagedRuntimeResult::Ready};
    std::uint32_t nativeError{0};
};

[[nodiscard]] ManagedRuntimeOutcome
StartManagedEverything(
    const std::filesystem::path& dataDirectory,
    const std::filesystem::path& executable,
    bool allowElevation,
    bool showTrayIcon,
    EverythingBootstrapSnapshot& snapshot,
    const EverythingBootstrapProgress&
        progress,
    std::stop_token stopToken) {
    bool configNeedsUpdate = false;
    std::uint32_t nativeError = 0;

    if (!ManagedIniNeedsUpdate(
            executable,
            showTrayIcon,
            configNeedsUpdate,
            nativeError)) {
        return {
            ManagedRuntimeResult::
                ConfigFailed,
            nativeError,
        };
    }

    const auto expectedServiceExecutable =
        ManagedEverythingServiceExecutableForSource(
            executable);

    if (expectedServiceExecutable.empty()) {
        return {
            ManagedRuntimeResult::
                ServiceUnavailable,
            ERROR_PATH_NOT_FOUND,
        };
    }

    auto serviceStatus =
        ProbeEverythingService(
            nativeError);

    if (serviceStatus ==
        ServiceProbe::Error) {
        return {
            ManagedRuntimeResult::
                ServiceUnavailable,
            nativeError,
        };
    }

    if (serviceStatus ==
        ServiceProbe::Starting) {
        Report(
            snapshot,
            EverythingBootstrapStage::
                WaitingForService,
            progress);

        if (!WaitForEverythingService(
                stopToken,
                nativeError)) {
            return {
                stopToken.stop_requested()
                    ? ManagedRuntimeResult::
                          Cancelled
                    : ManagedRuntimeResult::
                          ServiceUnavailable,
                nativeError,
            };
        }

        serviceStatus =
            ServiceProbe::Running;
    }

    bool externalService = false;
    bool servicePathStale = false;
    bool servicePathNeedsRepair = false;

    if (serviceStatus !=
        ServiceProbe::Missing) {
        std::filesystem::path
            serviceExecutable;
        bool serviceExecutableExists =
            false;

        if (!QueryEverythingServiceExecutable(
                serviceExecutable,
                serviceExecutableExists,
                nativeError)) {
            return {
                ManagedRuntimeResult::
                    ServiceUnavailable,
                nativeError,
            };
        }

        const bool owned =
            IsManagedEverythingServiceExecutable(
                dataDirectory,
                serviceExecutable);

        externalService = !owned;
        servicePathStale =
            !serviceExecutableExists;

        if (externalService) {
            // An external/user-managed Everything service is outside ALTRun's
            // ownership boundary. A running one can serve the managed client,
            // but ALTRun never starts, retargets or reconfigures a stopped
            // external service.
            if (serviceStatus !=
                ServiceProbe::Running) {
                return {
                    ManagedRuntimeResult::
                        ServiceUnavailable,
                    ERROR_ACCESS_DENIED,
                };
            }
        } else {
            servicePathNeedsRepair =
                servicePathStale ||
                LowerPath(
                    serviceExecutable) !=
                    LowerPath(
                        expectedServiceExecutable) ||
                !FileExists(
                    expectedServiceExecutable);
        }
    }

    const bool managedRunning =
        ManagedDefaultIpcRunning(
            executable);

    if (managedRunning &&
        serviceStatus ==
            ServiceProbe::Running &&
        !configNeedsUpdate &&
        !servicePathNeedsRepair) {
        return {};
    }

    const bool needsOwnedServiceAction =
        !externalService &&
        (serviceStatus !=
             ServiceProbe::Running ||
         servicePathNeedsRepair);

    if (needsOwnedServiceAction &&
        !allowElevation) {
        return {
            servicePathNeedsRepair
                ? ManagedRuntimeResult::
                      NeedsServiceRepair
                : ManagedRuntimeResult::
                      NeedsService,
            static_cast<std::uint32_t>(
                servicePathStale
                    ? ERROR_FILE_NOT_FOUND
                    : ERROR_SERVICE_NOT_ACTIVE),
        };
    }

    if (managedRunning) {
        Report(
            snapshot,
            EverythingBootstrapStage::
                StoppingManaged,
            progress);

        if (!LaunchEverythingCommand(
                executable,
                L"-exit",
                true,
                nativeError) ||
            !WaitForManagedDefaultIpcToExit(
                executable,
                stopToken)) {
            return {
                stopToken.stop_requested()
                    ? ManagedRuntimeResult::
                          Cancelled
                    : ManagedRuntimeResult::
                          StopFailed,
                stopToken.stop_requested()
                    ? ERROR_CANCELLED
                    : (nativeError != 0
                           ? nativeError
                           : ERROR_TIMEOUT),
            };
        }
    }

    Report(
        snapshot,
        EverythingBootstrapStage::
            ConfiguringManaged,
        progress);

    if (!ConfigureManagedEverything(
            executable,
            showTrayIcon,
            nativeError)) {
        return {
            ManagedRuntimeResult::
                ConfigFailed,
            nativeError,
        };
    }

    bool serviceActionTaken = false;

    if (!externalService &&
        (serviceStatus ==
             ServiceProbe::Missing ||
         servicePathNeedsRepair)) {
        Report(
            snapshot,
            serviceStatus ==
                    ServiceProbe::Missing
                ? EverythingBootstrapStage::
                      InstallingService
                : EverythingBootstrapStage::
                      RepairingService,
            progress);

        if (!RunElevatedServiceRepairHelper(
                executable,
                nativeError)) {
            return {
                nativeError ==
                        ERROR_CANCELLED
                    ? ManagedRuntimeResult::
                          ServiceElevationCancelled
                    : (serviceStatus ==
                               ServiceProbe::Missing
                           ? ManagedRuntimeResult::
                                 ServiceInstallFailed
                           : ManagedRuntimeResult::
                                 ServiceRepairFailed),
                nativeError,
            };
        }

        serviceActionTaken = true;
    } else if (
        !externalService &&
        serviceStatus !=
            ServiceProbe::Running) {
        Report(
            snapshot,
            EverythingBootstrapStage::
                InstallingService,
            progress);

        if (!RunElevatedServicePolicyHelper(
                true,
                executable,
                nativeError)) {
            return {
                nativeError ==
                        ERROR_CANCELLED
                    ? ManagedRuntimeResult::
                          ServiceElevationCancelled
                    : ManagedRuntimeResult::
                          ServiceInstallFailed,
                nativeError,
            };
        }

        serviceActionTaken = true;
    }

    if (serviceActionTaken) {
        Report(
            snapshot,
            EverythingBootstrapStage::
                WaitingForService,
            progress);

        if (!WaitForEverythingService(
                stopToken,
                nativeError)) {
            return {
                stopToken.stop_requested()
                    ? ManagedRuntimeResult::
                          Cancelled
                    : ManagedRuntimeResult::
                          ServiceUnavailable,
                nativeError,
            };
        }
    }

    if (stopToken.stop_requested()) {
        return {
            ManagedRuntimeResult::
                Cancelled,
            ERROR_CANCELLED,
        };
    }

    Report(
        snapshot,
        EverythingBootstrapStage::
            StartingManaged,
        progress);

    if (!LaunchEverything(
            executable,
            nativeError)) {
        return {
            ManagedRuntimeResult::
                LaunchFailed,
            nativeError,
        };
    }

    Report(
        snapshot,
        EverythingBootstrapStage::
            WaitingForIpc,
        progress);

    if (!WaitForIpc(
            stopToken)) {
        return {
            stopToken.stop_requested()
                ? ManagedRuntimeResult::
                      Cancelled
                : ManagedRuntimeResult::
                      IpcUnavailable,
            static_cast<std::uint32_t>(
                stopToken.stop_requested()
                    ? ERROR_CANCELLED
                    : ERROR_TIMEOUT),
        };
    }

    return {};
}

[[nodiscard]] bool
CrackHttpsUrl(
    std::wstring_view url,
    std::wstring& host,
    INTERNET_PORT& port,
    std::wstring& object) {
    std::wstring copy(url);

    URL_COMPONENTSW parts{};
    parts.dwStructSize =
        sizeof(parts);
    parts.dwSchemeLength =
        static_cast<DWORD>(-1);
    parts.dwHostNameLength =
        static_cast<DWORD>(-1);
    parts.dwUrlPathLength =
        static_cast<DWORD>(-1);
    parts.dwExtraInfoLength =
        static_cast<DWORD>(-1);

    if (!WinHttpCrackUrl(
            copy.c_str(),
            0,
            0,
            &parts) ||
        parts.nScheme !=
            INTERNET_SCHEME_HTTPS ||
        !parts.lpszHostName ||
        parts.dwHostNameLength == 0) {
        return false;
    }

    host.assign(
        parts.lpszHostName,
        parts.dwHostNameLength);

    object.assign(
        parts.lpszUrlPath
            ? parts.lpszUrlPath
            : L"/",
        parts.dwUrlPathLength);

    if (parts.lpszExtraInfo &&
        parts.dwExtraInfoLength > 0) {
        object.append(
            parts.lpszExtraInfo,
            parts.dwExtraInfoLength);
    }

    if (object.empty()) {
        object = L"/";
    }

    port = parts.nPort;
    return true;
}

struct HttpRequest {
    InternetHandle session;
    InternetHandle connection;
    everything_http::Request request;
};

[[nodiscard]] bool
OpenHttpRequest(
    std::wstring_view url,
    HttpRequest& handles,
    std::uint64_t& contentLength,
    std::uint32_t& nativeError,
    std::stop_token stopToken) {
    if (stopToken.stop_requested()) {
        nativeError = ERROR_CANCELLED;
        return false;
    }
    std::wstring host;
    std::wstring object;
    INTERNET_PORT port = 0;

    if (!CrackHttpsUrl(
            url,
            host,
            port,
            object)) {
        nativeError =
            ERROR_INVALID_PARAMETER;
        return false;
    }

    handles.session.value =
        WinHttpOpen(
            L"Asterun/0.7 Managed Everything",
            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
            WINHTTP_NO_PROXY_NAME,
            WINHTTP_NO_PROXY_BYPASS,
            WINHTTP_FLAG_ASYNC);

    if (!handles.session.value) {
        nativeError =
            static_cast<std::uint32_t>(
                GetLastError());
        return false;
    }

    if (!WinHttpSetTimeouts(handles.session.value, 5000, 5000, 10000, 10000)) {
        nativeError = GetLastError();
        return false;
    }

    handles.connection.value =
        WinHttpConnect(
            handles.session.value,
            host.c_str(),
            port,
            0);

    if (!handles.connection.value) {
        nativeError =
            static_cast<std::uint32_t>(
                GetLastError());
        return false;
    }

    if (!handles.request.Attach(
        WinHttpOpenRequest(
            handles.connection.value,
            L"GET",
            object.c_str(),
            nullptr,
            WINHTTP_NO_REFERER,
            WINHTTP_DEFAULT_ACCEPT_TYPES,
            WINHTTP_FLAG_SECURE), nativeError)) {
        return false;
    }

    if (!handles.request.Send(stopToken, nativeError) ||
        !handles.request.Receive(stopToken, nativeError)) {
        return false;
    }

    DWORD status = 0;
    DWORD statusBytes =
        sizeof(status);

    if (!WinHttpQueryHeaders(
            handles.request.Get(),
            WINHTTP_QUERY_STATUS_CODE |
                WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX,
            &status,
            &statusBytes,
            WINHTTP_NO_HEADER_INDEX) ||
        status < 200 ||
        status >= 300) {
        nativeError =
            status >= 400
                ? status
                : static_cast<
                      std::uint32_t>(
                      GetLastError());
        return false;
    }

    DWORD length = 0;
    DWORD lengthBytes =
        sizeof(length);

    if (WinHttpQueryHeaders(
            handles.request.Get(),
            WINHTTP_QUERY_CONTENT_LENGTH |
                WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX,
            &length,
            &lengthBytes,
            WINHTTP_NO_HEADER_INDEX)) {
        contentLength = length;
    } else {
        contentLength = 0;
    }

    if (stopToken.stop_requested()) {
        nativeError = ERROR_CANCELLED;
        return false;
    }

    nativeError = 0;
    return true;
}

[[nodiscard]] bool
DownloadText(
    std::wstring_view url,
    std::string& output,
    std::uint32_t& nativeError,
    std::stop_token stopToken) {
    constexpr std::uint64_t
        kMaximumBytes = 512 * 1024;

    HttpRequest handles;
    std::uint64_t contentLength = 0;

    if (!OpenHttpRequest(
            url,
            handles,
            contentLength,
            nativeError,
            stopToken) ||
        contentLength > kMaximumBytes) {
        if (contentLength >
            kMaximumBytes) {
            nativeError =
                ERROR_FILE_TOO_LARGE;
        }
        return false;
    }

    output.clear();

    for (;;) {
        if (stopToken.stop_requested()) {
            nativeError =
                ERROR_CANCELLED;
            return false;
        }

        DWORD available = 0;

        if (!handles.request.Available(available, stopToken, nativeError)) {
            return false;
        }

        if (available == 0) {
            break;
        }

        if (output.size() +
                available >
            kMaximumBytes) {
            nativeError =
                ERROR_FILE_TOO_LARGE;
            return false;
        }

        DWORD read = 0;
        if (!handles.request.Read(available, read, stopToken, nativeError)) {
            return false;
        }
        output.append(handles.request.Data(), read);
    }

    nativeError = 0;
    return !output.empty();
}

[[nodiscard]] bool
DownloadFile(
    std::wstring_view url,
    const std::filesystem::path& path,
    EverythingBootstrapSnapshot& snapshot,
    const EverythingBootstrapProgress&
        progress,
    std::uint32_t& nativeError,
    std::stop_token stopToken) {
    constexpr std::uint64_t
        kMaximumBytes =
            32ULL * 1024ULL * 1024ULL;

    HttpRequest handles;
    std::uint64_t contentLength = 0;

    if (!OpenHttpRequest(
            url,
            handles,
            contentLength,
            nativeError,
            stopToken) ||
        contentLength > kMaximumBytes) {
        if (contentLength >
            kMaximumBytes) {
            nativeError =
                ERROR_FILE_TOO_LARGE;
        }
        return false;
    }

    std::ofstream output(
        path,
        std::ios::binary |
            std::ios::trunc);

    if (!output) {
        nativeError =
            ERROR_OPEN_FAILED;
        return false;
    }

    snapshot.totalBytes =
        contentLength;
    snapshot.downloadedBytes = 0;
    Report(
        snapshot,
        EverythingBootstrapStage::
            DownloadingPackage,
        progress);

    for (;;) {
        if (stopToken.stop_requested()) {
            nativeError =
                ERROR_CANCELLED;
            return false;
        }

        DWORD available = 0;

        if (!handles.request.Available(available, stopToken, nativeError)) {
            return false;
        }

        if (available == 0) {
            break;
        }

        if (snapshot.downloadedBytes +
                available >
            kMaximumBytes) {
            nativeError =
                ERROR_FILE_TOO_LARGE;
            return false;
        }

        DWORD read = 0;
        if (!handles.request.Read(available, read, stopToken, nativeError)) {
            return false;
        }

        output.write(
            handles.request.Data(),
            static_cast<
                std::streamsize>(
                read));

        if (!output) {
            nativeError =
                ERROR_WRITE_FAULT;
            return false;
        }

        snapshot.downloadedBytes +=
            read;

        if (progress) {
            progress(snapshot);
        }
    }

    output.flush();

    if (!output) {
        nativeError =
            ERROR_WRITE_FAULT;
        return false;
    }

    nativeError = 0;
    return snapshot.downloadedBytes > 0;
}

[[nodiscard]] std::string
NarrowAscii(
    std::wstring_view value) {
    std::string result;
    result.reserve(value.size());

    for (const auto c : value) {
        if (c > 0x7f) {
            return {};
        }
        result.push_back(
            static_cast<char>(c));
    }

    return result;
}

[[nodiscard]] bool
ExtractZipVerified(const std::filesystem::path& archive,
    const std::filesystem::path& destination, std::uint32_t& nativeError,
    std::stop_token stopToken) {
    std::error_code error;
    if (!altrun::ExtractArchive(archive, destination, error, stopToken)) {
        nativeError = stopToken.stop_requested() ? ERROR_CANCELLED : ERROR_INVALID_DATA;
        return false;
    }
    nativeError = 0;
    return true;
}

// The downloaded ZIP is checked against voidtools' checksum manifest, but a
// portable source can be modified later. Authenticate the bytes after they
// have been copied into the administrator-owned staging directory and before
// promoting them to the service executable.
[[nodiscard]] bool
VerifyEverythingPublisher(const std::filesystem::path& staged,
                          std::uint32_t& nativeError) {
    WINTRUST_FILE_INFO file{};
    file.cbStruct = sizeof(file);
    file.pcwszFilePath = staged.c_str();

    WINTRUST_DATA trust{};
    trust.cbStruct = sizeof(trust);
    trust.dwUIChoice = WTD_UI_NONE;
    trust.fdwRevocationChecks = WTD_REVOKE_NONE;
    trust.dwUnionChoice = WTD_CHOICE_FILE;
    trust.pFile = &file;
    trust.dwStateAction = WTD_STATEACTION_VERIFY;
    trust.dwProvFlags = WTD_REVOCATION_CHECK_NONE;
    GUID action = WINTRUST_ACTION_GENERIC_VERIFY_V2;
    const LONG result = WinVerifyTrust(nullptr, &action, &trust);

    bool publisherMatches = false;
    if (result == ERROR_SUCCESS && trust.hWVTStateData) {
        const HMODULE library = GetModuleHandleW(L"wintrust.dll");
        const auto provider = library ? reinterpret_cast<decltype(&WTHelperProvDataFromStateData)>(
            GetProcAddress(library, "WTHelperProvDataFromStateData")) : nullptr;
        const auto signer = library ? reinterpret_cast<decltype(&WTHelperGetProvSignerFromChain)>(
            GetProcAddress(library, "WTHelperGetProvSignerFromChain")) : nullptr;
        if (provider && signer) {
            if (auto* data = provider(trust.hWVTStateData)) {
                if (auto* chain = signer(data, 0, FALSE, 0);
                    chain && chain->csCertChain && chain->pasCertChain[0].pCert) {
                    wchar_t organization[128]{};
                    const auto length = CertGetNameStringW(chain->pasCertChain[0].pCert,
                        CERT_NAME_ATTR_TYPE, 0,
                        const_cast<char*>(szOID_ORGANIZATION_NAME), organization,
                        static_cast<DWORD>(std::size(organization)));
                    publisherMatches = length > 1 &&
                        (_wcsicmp(organization, L"voidtools PTY LTD") == 0 ||
                         _wcsicmp(organization, L"voidtools") == 0);
                }
            }
        }
    }
    trust.dwStateAction = WTD_STATEACTION_CLOSE;
    if (trust.hWVTStateData) WinVerifyTrust(nullptr, &action, &trust);
    nativeError = result != ERROR_SUCCESS ? static_cast<std::uint32_t>(result) :
        publisherMatches ? ERROR_SUCCESS : TRUST_E_SUBJECT_NOT_TRUSTED;
    return result == ERROR_SUCCESS && publisherMatches;
}

[[nodiscard]] bool
CopyManagedEverythingToProtectedHost(
    const std::filesystem::path& source,
    const std::filesystem::path& destination,
    std::uint32_t& nativeError) {
    if (source.empty() ||
        destination.empty() ||
        !source.is_absolute() ||
        !destination.is_absolute()) {
        nativeError =
            ERROR_INVALID_PARAMETER;
        return false;
    }

    HANDLE input =
        CreateFileW(
            source.c_str(),
            GENERIC_READ,
            FILE_SHARE_READ,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL |
                FILE_FLAG_OPEN_REPARSE_POINT |
                FILE_FLAG_SEQUENTIAL_SCAN,
            nullptr);

    if (input ==
            INVALID_HANDLE_VALUE ||
        input == nullptr) {
        nativeError =
            static_cast<std::uint32_t>(
                GetLastError());
        return false;
    }

    FILE_ATTRIBUTE_TAG_INFO sourceInfo{};

    if (!GetFileInformationByHandleEx(
            input,
            FileAttributeTagInfo,
            &sourceInfo,
            sizeof(sourceInfo)) ||
        (sourceInfo.FileAttributes &
         (FILE_ATTRIBUTE_DIRECTORY |
          FILE_ATTRIBUTE_REPARSE_POINT)) !=
            0) {
        nativeError =
            GetLastError() !=
                    ERROR_SUCCESS
                ? static_cast<
                      std::uint32_t>(
                      GetLastError())
                : ERROR_REPARSE_TAG_INVALID;
        CloseHandle(input);
        return false;
    }

    std::error_code ec;
    const auto parent =
        destination.parent_path();

    std::filesystem::create_directories(
        parent,
        ec);

    if (ec) {
        nativeError =
            static_cast<std::uint32_t>(
                ec.value());
        CloseHandle(input);
        return false;
    }

    const DWORD parentAttributes =
        GetFileAttributesW(
            parent.c_str());

    if (parentAttributes ==
            INVALID_FILE_ATTRIBUTES ||
        (parentAttributes &
         FILE_ATTRIBUTE_DIRECTORY) == 0 ||
        (parentAttributes &
         FILE_ATTRIBUTE_REPARSE_POINT) !=
            0) {
        nativeError =
            parentAttributes ==
                    INVALID_FILE_ATTRIBUTES
                ? static_cast<
                      std::uint32_t>(
                      GetLastError())
                : ERROR_REPARSE_TAG_INVALID;
        CloseHandle(input);
        return false;
    }

    std::wstring token;

    if (!GenerateSecureToken(
            token,
            nativeError)) {
        CloseHandle(input);
        return false;
    }

    const auto staged =
        parent /
        (L".Everything.asterun." +
         token +
         L".tmp");

    HANDLE output =
        CreateFileW(
            staged.c_str(),
            GENERIC_WRITE,
            0,
            nullptr,
            CREATE_NEW,
            FILE_ATTRIBUTE_NOT_CONTENT_INDEXED,
            nullptr);

    if (output ==
            INVALID_HANDLE_VALUE ||
        output == nullptr) {
        nativeError =
            static_cast<std::uint32_t>(
                GetLastError());
        CloseHandle(input);
        return false;
    }

    bool success = true;
    std::array<std::byte, 64 * 1024>
        buffer{};

    for (;;) {
        DWORD read = 0;

        if (!ReadFile(
                input,
                buffer.data(),
                static_cast<DWORD>(
                    buffer.size()),
                &read,
                nullptr)) {
            nativeError =
                static_cast<std::uint32_t>(
                    GetLastError());
            success = false;
            break;
        }

        if (read == 0) {
            break;
        }

        DWORD offset = 0;

        while (offset < read) {
            DWORD written = 0;

            if (!WriteFile(
                    output,
                    buffer.data() +
                        offset,
                    read - offset,
                    &written,
                    nullptr) ||
                written == 0) {
                nativeError =
                    static_cast<std::uint32_t>(
                        GetLastError());
                success = false;
                break;
            }

            offset += written;
        }

        if (!success) {
            break;
        }
    }

    if (success &&
        !FlushFileBuffers(output)) {
        nativeError =
            static_cast<std::uint32_t>(
                GetLastError());
        success = false;
    }

    CloseHandle(output);
    CloseHandle(input);

    if (!success) {
        DeleteFileW(staged.c_str());
        return false;
    }

    if (!VerifyEverythingPublisher(staged, nativeError)) {
        DeleteFileW(staged.c_str());
        return false;
    }

    if (!MoveFileExW(
            staged.c_str(),
            destination.c_str(),
            MOVEFILE_REPLACE_EXISTING |
                MOVEFILE_WRITE_THROUGH)) {
        nativeError =
            static_cast<std::uint32_t>(
                GetLastError());
        DeleteFileW(staged.c_str());
        return false;
    }

    nativeError = 0;
    return true;
}

[[nodiscard]] EverythingBootstrapSnapshot
Fail(
    EverythingBootstrapSnapshot snapshot,
    EverythingBootstrapFailure failure,
    std::uint32_t nativeError,
    const EverythingBootstrapProgress&
        progress) {
    snapshot.stage =
        EverythingBootstrapStage::Failed;
    snapshot.failure = failure;
    snapshot.nativeError = nativeError;
    snapshot.running = false;

    if (progress) {
        progress(snapshot);
    }

    return snapshot;
}

[[nodiscard]] EverythingBootstrapSnapshot
NeedsInstall(
    EverythingBootstrapSnapshot snapshot,
    EverythingBootstrapFailure failure,
    const EverythingBootstrapProgress&
        progress) {
    snapshot.stage =
        EverythingBootstrapStage::
            NeedsInstall;
    snapshot.failure = failure;
    snapshot.running = false;

    if (progress) {
        progress(snapshot);
    }

    return snapshot;
}

} // namespace

bool
IsEverythingServiceMissingError(
    std::uint32_t nativeError) noexcept {
    return nativeError ==
               ERROR_SERVICE_DOES_NOT_EXIST ||
        nativeError ==
               ERROR_FILE_NOT_FOUND;
}

std::filesystem::path
ManagedEverythingExecutable(
    const std::filesystem::path&
        dataDirectory) {
    const auto root =
        ManagedEverythingRoot(
            dataDirectory);

    std::error_code ec;
    std::wstring bestVersion;
    std::filesystem::path
        bestExecutable;

    if (std::filesystem::is_directory(
            root,
            ec) &&
        !ec) {
        for (std::filesystem::
                 directory_iterator it(
                     root,
                     ec),
             end;
             !ec && it != end;
             it.increment(ec)) {
            if (!it->is_directory(ec) ||
                ec) {
                ec.clear();
                continue;
            }

            const auto executable =
                it->path() /
                L"Everything.exe";

            if (!FileExists(
                    executable)) {
                continue;
            }

            const auto version =
                VersionFromManagedExecutablePath(
                    executable);

            if (!version) {
                continue;
            }

            if (bestVersion.empty() ||
                CompareEverythingVersions(
                    *version,
                    bestVersion) > 0) {
                bestVersion = *version;
                bestExecutable =
                    executable;
            }
        }
    }

    if (!bestExecutable.empty()) {
        return bestExecutable;
    }

    return ManagedEverythingExecutableForVersion(
        dataDirectory,
        PinnedManagedEverythingVersion());
}

std::filesystem::path
ManagedEverythingServiceExecutable(
    const std::filesystem::path&
        dataDirectory) {
    const auto source =
        ManagedEverythingExecutable(
            dataDirectory);

    return
        ManagedEverythingServiceExecutableForSource(
            source);
}

bool EverythingIpcEndpointAvailable() {
    return AnyUsableIpcEndpoint();
}

bool IsManagedEverythingRunning(
    const std::filesystem::path& dataDirectory) {
    return ActiveManagedEverythingExecutable(
               dataDirectory)
        .has_value();
}

bool
IsManagedEverythingServiceExecutable(
    const std::filesystem::path& dataDirectory,
    const std::filesystem::path& executable) {
    if (executable.empty()) {
        return false;
    }

    return
        PortableManagedEverythingExecutable(
            dataDirectory,
            executable) ||
        ProtectedManagedEverythingServiceExecutable(
            executable);
}

ManagedEverythingServicePolicyResult
SetManagedEverythingServiceEnabled(
    const std::filesystem::path& dataDirectory,
    bool enabled) {
    std::uint32_t nativeError = 0;
    const auto serviceStatus =
        ProbeEverythingService(
            nativeError);

    if (serviceStatus ==
        ServiceProbe::Missing) {
        return {
            ManagedEverythingServicePolicyStatus::
                NotInstalled,
            0,
        };
    }

    if (serviceStatus ==
        ServiceProbe::Error) {
        return {
            ManagedEverythingServicePolicyStatus::
                Failed,
            nativeError,
        };
    }

    // An existing service must be classified by ownership before the
    // current portable directory's managed source is considered. A stopped
    // external/user-managed Everything service is still external even when
    // this ALTRun directory has never downloaded Everything.
    std::filesystem::path
        serviceExecutable;
    bool serviceExecutableExists =
        false;

    if (!QueryEverythingServiceExecutable(
            serviceExecutable,
            serviceExecutableExists,
            nativeError)) {
        if (IsEverythingServiceMissingError(
                nativeError)) {
            return {
                ManagedEverythingServicePolicyStatus::
                    NotInstalled,
                0,
            };
        }

        return {
            ManagedEverythingServicePolicyStatus::
                Failed,
            nativeError,
        };
    }

    if (!IsManagedEverythingServiceExecutable(
            dataDirectory,
            serviceExecutable)) {
        return {
            ManagedEverythingServicePolicyStatus::
                External,
            0,
        };
    }

    const auto managedExecutable =
        ManagedEverythingExecutable(
            dataDirectory);
    const auto expectedServiceExecutable =
        ManagedEverythingServiceExecutableForSource(
            managedExecutable);

    if (enabled &&
        (!FileExists(
             managedExecutable) ||
         expectedServiceExecutable.empty())) {
        // The service belongs to ALTRun, but this portable directory has no
        // verified client/source yet. Persist the provider choice and let the
        // explicit Get-and-start flow reacquire the source before any
        // privileged service repair is attempted.
        return {
            ManagedEverythingServicePolicyStatus::
                NotInstalled,
            0,
        };
    }

    std::uint32_t startType = 0;

    if (!QueryEverythingServiceStartType(
            startType,
            nativeError)) {
        if (IsEverythingServiceMissingError(
                nativeError)) {
            return {
                ManagedEverythingServicePolicyStatus::
                    NotInstalled,
                0,
            };
        }

        return {
            ManagedEverythingServicePolicyStatus::
                Failed,
            nativeError,
        };
    }

    const bool running =
        serviceStatus ==
            ServiceProbe::Running ||
        serviceStatus ==
            ServiceProbe::Starting;
    const bool correctProtectedHost =
        enabled &&
        serviceExecutableExists &&
        !expectedServiceExecutable.empty() &&
        LowerPath(
            serviceExecutable) ==
            LowerPath(
                expectedServiceExecutable);
    const bool desired =
        enabled
            ? (running &&
               startType ==
                   SERVICE_AUTO_START &&
               correctProtectedHost)
            : (!running &&
               startType ==
                   SERVICE_DISABLED);

    if (desired) {
        return {
            ManagedEverythingServicePolicyStatus::
                AlreadyConfigured,
            0,
        };
    }

    if (!RunElevatedServicePolicyHelper(
            enabled,
            managedExecutable,
            nativeError)) {
        return {
            nativeError ==
                    ERROR_CANCELLED
                ? ManagedEverythingServicePolicyStatus::
                      ElevationCancelled
                : ManagedEverythingServicePolicyStatus::
                      Failed,
            nativeError,
        };
    }

    return {
        ManagedEverythingServicePolicyStatus::
            Applied,
        0,
    };
}

EverythingServiceRepairResult
ApplyManagedEverythingServiceEnabledPolicy(
    const std::filesystem::path& dataDirectory,
    bool enabled) {
    return
        ApplyManagedEverythingServiceEnabledPolicy(
            dataDirectory,
            enabled,
            ManagedEverythingExecutable(
                dataDirectory));
}

EverythingServiceRepairResult
ApplyManagedEverythingServiceEnabledPolicy(
    const std::filesystem::path& dataDirectory,
    bool enabled,
    const std::filesystem::path&
        managedSource) {
    if (enabled &&
        (!PortableManagedEverythingExecutable(
             dataDirectory,
             managedSource) ||
         !FileExists(
             managedSource))) {
        return {
            false,
            ERROR_FILE_NOT_FOUND,
        };
    }

    ServiceHandle manager;
    manager.value =
        OpenSCManagerW(
            nullptr,
            nullptr,
            SC_MANAGER_CONNECT);

    if (!manager.value) {
        return {
            false,
            static_cast<std::uint32_t>(
                GetLastError()),
        };
    }

    ServiceHandle service;
    service.value =
        OpenServiceW(
            manager.value,
            L"Everything",
            SERVICE_QUERY_STATUS |
                SERVICE_QUERY_CONFIG |
                SERVICE_CHANGE_CONFIG |
                SERVICE_START |
                SERVICE_STOP);

    if (!service.value) {
        const auto error =
            static_cast<std::uint32_t>(
                GetLastError());

        if (IsEverythingServiceMissingError(
                error)) {
            return {
                true,
                0,
            };
        }

        return {
            false,
            error,
        };
    }

    std::filesystem::path
        serviceExecutable;
    bool serviceExecutableExists =
        false;
    std::uint32_t queryError = 0;

    if (!QueryEverythingServiceExecutable(
            serviceExecutable,
            serviceExecutableExists,
            queryError)) {
        return {
            false,
            queryError,
        };
    }

    if (!IsManagedEverythingServiceExecutable(
            dataDirectory,
            serviceExecutable)) {
        return {
            false,
            ERROR_ACCESS_DENIED,
        };
    }

    if (enabled) {
        const auto expectedServiceExecutable =
            ManagedEverythingServiceExecutableForSource(
                managedSource);

        if (expectedServiceExecutable.empty() ||
            !serviceExecutableExists ||
            LowerPath(
                serviceExecutable) !=
                LowerPath(
                    expectedServiceExecutable) ||
            !FileExists(
                expectedServiceExecutable)) {
            return
                RepairManagedEverythingServicePath(
                    dataDirectory,
                    managedSource);
        }
    }

    std::uint32_t originalStartType = 0;

    if (!QueryEverythingServiceStartType(
            originalStartType,
            queryError)) {
        return {
            false,
            queryError,
        };
    }

    SERVICE_STATUS_PROCESS status{};
    DWORD bytesNeeded = 0;

    if (!QueryServiceStatusEx(
            service.value,
            SC_STATUS_PROCESS_INFO,
            reinterpret_cast<LPBYTE>(
                &status),
            sizeof(status),
            &bytesNeeded)) {
        return {
            false,
            static_cast<std::uint32_t>(
                GetLastError()),
        };
    }

    if (status.dwCurrentState ==
        SERVICE_START_PENDING) {
        std::uint32_t waitError = 0;

        if (!WaitForEverythingService(
                {},
                waitError)) {
            return {
                false,
                waitError,
            };
        }

        status.dwCurrentState =
            SERVICE_RUNNING;
    }

    if (status.dwCurrentState ==
        SERVICE_STOP_PENDING) {
        std::uint32_t waitError = 0;

        if (!WaitForEverythingServiceStopped(
                {},
                waitError)) {
            return {
                false,
                waitError,
            };
        }

        status.dwCurrentState =
            SERVICE_STOPPED;
    }

    if (!ChangeServiceConfigW(
            service.value,
            SERVICE_NO_CHANGE,
            enabled
                ? SERVICE_AUTO_START
                : SERVICE_DISABLED,
            SERVICE_NO_CHANGE,
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            nullptr)) {
        return {
            false,
            static_cast<std::uint32_t>(
                GetLastError()),
        };
    }

    const auto rollbackStartType =
        [&]() {
            (void)ChangeServiceConfigW(
                service.value,
                SERVICE_NO_CHANGE,
                originalStartType,
                SERVICE_NO_CHANGE,
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                nullptr);
        };

    if (!enabled &&
        status.dwCurrentState !=
            SERVICE_STOPPED) {
        SERVICE_STATUS stopStatus{};

        if (!ControlService(
                service.value,
                SERVICE_CONTROL_STOP,
                &stopStatus)) {
            const auto error =
                static_cast<std::uint32_t>(
                    GetLastError());

            if (error !=
                ERROR_SERVICE_NOT_ACTIVE) {
                rollbackStartType();
                return {
                    false,
                    error,
                };
            }
        }

        std::uint32_t waitError = 0;

        if (!WaitForEverythingServiceStopped(
                {},
                waitError)) {
            rollbackStartType();
            return {
                false,
                waitError,
            };
        }

        status.dwCurrentState =
            SERVICE_STOPPED;
    }

    if (enabled &&
        status.dwCurrentState !=
            SERVICE_RUNNING) {
        if (!StartServiceW(
                service.value,
                0,
                nullptr)) {
            const auto error =
                static_cast<std::uint32_t>(
                    GetLastError());

            if (error !=
                ERROR_SERVICE_ALREADY_RUNNING) {
                rollbackStartType();
                return {
                    false,
                    error,
                };
            }
        }

        std::uint32_t waitError = 0;

        if (!WaitForEverythingService(
                {},
                waitError)) {
            rollbackStartType();
            return {
                false,
                waitError,
            };
        }
    }

    return {
        true,
        0,
    };
}

EverythingServiceRepairResult
RepairManagedEverythingServicePath(
    const std::filesystem::path& dataDirectory) {
    return
        RepairManagedEverythingServicePath(
            dataDirectory,
            ManagedEverythingExecutable(
                dataDirectory));
}

EverythingServiceRepairResult
RepairManagedEverythingServicePath(
    const std::filesystem::path& dataDirectory,
    const std::filesystem::path&
        managedSource) {
    if (!PortableManagedEverythingExecutable(
            dataDirectory,
            managedSource) ||
        !FileExists(
            managedSource)) {
        return {
            false,
            ERROR_FILE_NOT_FOUND,
        };
    }

    const auto serviceExecutable =
        ManagedEverythingServiceExecutableForSource(
            managedSource);

    if (serviceExecutable.empty()) {
        return {
            false,
            ERROR_PATH_NOT_FOUND,
        };
    }

    ServiceHandle manager;
    manager.value =
        OpenSCManagerW(
            nullptr,
            nullptr,
            SC_MANAGER_CONNECT);

    if (!manager.value) {
        return {
            false,
            static_cast<std::uint32_t>(
                GetLastError()),
        };
    }

    ServiceHandle service;
    service.value =
        OpenServiceW(
            manager.value,
            L"Everything",
            SERVICE_QUERY_STATUS |
                SERVICE_QUERY_CONFIG |
                SERVICE_CHANGE_CONFIG |
                SERVICE_START |
                SERVICE_STOP);

    bool serviceExists =
        service.value != nullptr;

    if (!serviceExists) {
        const DWORD error =
            GetLastError();

        if (!IsEverythingServiceMissingError(
                static_cast<std::uint32_t>(
                    error))) {
            return {
                false,
                static_cast<std::uint32_t>(
                    error),
            };
        }
    }

    SERVICE_STATUS_PROCESS status{};
    std::filesystem::path
        previousExecutable;
    bool previousExecutableExists =
        false;

    if (serviceExists) {
        DWORD bytesNeeded = 0;

        if (!QueryServiceStatusEx(
                service.value,
                SC_STATUS_PROCESS_INFO,
                reinterpret_cast<LPBYTE>(
                    &status),
                sizeof(status),
                &bytesNeeded)) {
            return {
                false,
                static_cast<std::uint32_t>(
                    GetLastError()),
            };
        }

        std::uint32_t queryError = 0;

        if (!QueryEverythingServiceExecutable(
                previousExecutable,
                previousExecutableExists,
                queryError)) {
            return {
                false,
                queryError,
            };
        }

        if (!IsManagedEverythingServiceExecutable(
                dataDirectory,
                previousExecutable)) {
            return {
                false,
                ERROR_ACCESS_DENIED,
            };
        }

        const bool alreadyProtected =
            previousExecutableExists &&
            LowerPath(
                previousExecutable) ==
                LowerPath(
                    serviceExecutable) &&
            FileExists(
                serviceExecutable);

        if (alreadyProtected &&
            status.dwCurrentState ==
                SERVICE_RUNNING) {
            return {
                true,
                0,
            };
        }

        if (status.dwCurrentState ==
            SERVICE_START_PENDING) {
            std::uint32_t waitError = 0;

            if (!WaitForEverythingService(
                    {},
                    waitError)) {
                return {
                    false,
                    waitError,
                };
            }

            status.dwCurrentState =
                SERVICE_RUNNING;
        }

        if (status.dwCurrentState ==
            SERVICE_STOP_PENDING) {
            std::uint32_t waitError = 0;

            if (!WaitForEverythingServiceStopped(
                    {},
                    waitError)) {
                return {
                    false,
                    waitError,
                };
            }

            status.dwCurrentState =
                SERVICE_STOPPED;
        }

        const bool needsHostRefresh =
            !previousExecutableExists ||
            LowerPath(
                previousExecutable) !=
                LowerPath(
                    serviceExecutable) ||
            !FileExists(
                serviceExecutable);

        if (needsHostRefresh &&
            status.dwCurrentState !=
                SERVICE_STOPPED) {
            SERVICE_STATUS stopStatus{};

            if (!ControlService(
                    service.value,
                    SERVICE_CONTROL_STOP,
                    &stopStatus)) {
                const auto error =
                    static_cast<std::uint32_t>(
                        GetLastError());

                if (error !=
                    ERROR_SERVICE_NOT_ACTIVE) {
                    return {
                        false,
                        error,
                    };
                }
            }

            std::uint32_t waitError = 0;

            if (!WaitForEverythingServiceStopped(
                    {},
                    waitError)) {
                return {
                    false,
                    waitError,
                };
            }

            status.dwCurrentState =
                SERVICE_STOPPED;
        }
    }

    std::uint32_t copyError = 0;

    if (!FileExists(
            serviceExecutable) ||
        !serviceExists ||
        LowerPath(
            previousExecutable) !=
            LowerPath(
                serviceExecutable)) {
        if (!CopyManagedEverythingToProtectedHost(
                managedSource,
                serviceExecutable,
                copyError)) {
            return {
                false,
                copyError,
            };
        }
    }

    if (!serviceExists) {
        std::uint32_t installError = 0;

        if (!LaunchEverythingCommand(
                serviceExecutable,
                L"-install-service",
                true,
                installError)) {
            return {
                false,
                installError,
            };
        }

        if (!WaitForEverythingService(
                {},
                installError)) {
            return {
                false,
                installError,
            };
        }

        return {
            true,
            0,
        };
    }

    const std::wstring binaryPath =
        L"\"" +
        serviceExecutable.wstring() +
        L"\" -svc";

    if (!ChangeServiceConfigW(
            service.value,
            SERVICE_NO_CHANGE,
            SERVICE_AUTO_START,
            SERVICE_NO_CHANGE,
            binaryPath.c_str(),
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            nullptr)) {
        return {
            false,
            static_cast<std::uint32_t>(
                GetLastError()),
        };
    }

    if (status.dwCurrentState !=
        SERVICE_RUNNING) {
        if (!StartServiceW(
                service.value,
                0,
                nullptr)) {
            const auto error =
                static_cast<std::uint32_t>(
                    GetLastError());

            if (error !=
                ERROR_SERVICE_ALREADY_RUNNING) {
                return {
                    false,
                    error,
                };
            }
        }

        std::uint32_t waitError = 0;

        if (!WaitForEverythingService(
                {},
                waitError)) {
            return {
                false,
                waitError,
            };
        }
    }

    return {
        true,
        0,
    };
}

ManagedEverythingStopResult
StopManagedEverything(
    const std::filesystem::path& dataDirectory,
    std::stop_token stopToken) {
    const auto activeExecutable =
        ActiveManagedEverythingExecutable(
            dataDirectory);
    const auto executable =
        activeExecutable.value_or(
            ManagedEverythingExecutable(
                dataDirectory));

    if (!FileExists(executable)) {
        return {
            ManagedEverythingStopStatus::
                NotInstalled,
            0,
        };
    }

    // Ownership is the safety boundary: never issue Everything's global
    // -exit command unless the active default IPC window belongs to the
    // exact executable under Asterun's managed tools directory.
    if (!ManagedDefaultIpcRunning(
            executable)) {
        return {
            ManagedEverythingStopStatus::
                NotRunning,
            0,
        };
    }

    std::uint32_t nativeError = 0;

    if (!LaunchEverythingCommand(
            executable,
            L"-exit",
            true,
            nativeError)) {
        return {
            ManagedEverythingStopStatus::
                Failed,
            nativeError,
        };
    }

    if (!WaitForManagedDefaultIpcToExit(
            executable,
            stopToken)) {
        return {
            ManagedEverythingStopStatus::
                Failed,
            static_cast<std::uint32_t>(
                stopToken.stop_requested()
                    ? ERROR_CANCELLED
                    : ERROR_TIMEOUT),
        };
    }

    return {
        ManagedEverythingStopStatus::
            Stopped,
        0,
    };
}

EverythingBootstrapSnapshot
CheckManagedEverythingUpdate(
    const std::filesystem::path& dataDirectory,
    EverythingBootstrapProgress progress,
    std::stop_token stopToken) {
    EverythingBootstrapSnapshot snapshot;
    snapshot.running = true;
    snapshot.source =
        EverythingBootstrapSource::
            Managed;
    const auto activeExecutable =
        ActiveManagedEverythingExecutable(
            dataDirectory);
    snapshot.executablePath =
        activeExecutable.value_or(
            ManagedEverythingExecutable(
                dataDirectory));

    if (!FileExists(
            snapshot.executablePath)) {
        return NeedsInstall(
            snapshot,
            EverythingBootstrapFailure::
                NotFound,
            progress);
    }

    if (const auto version =
            VersionFromManagedExecutablePath(
                snapshot.executablePath)) {
        snapshot.installedVersion =
            *version;
    }

    Report(
        snapshot,
        EverythingBootstrapStage::
            ResolvingStableVersion,
        progress);

    std::string metadata;
    std::uint32_t nativeError = 0;

    if (!DownloadText(
            EverythingStableUpdateMetadataUrl(),
            metadata,
            nativeError,
            stopToken)) {
        return Fail(
            snapshot,
            stopToken.stop_requested()
                ? EverythingBootstrapFailure::
                      Cancelled
                : EverythingBootstrapFailure::
                      ManifestDownloadFailed,
            stopToken.stop_requested()
                ? ERROR_CANCELLED
                : nativeError,
            progress);
    }

    const auto stableVersion =
        ParseEverythingStableUpdateVersion(
            metadata);

    if (!stableVersion) {
        return Fail(
            snapshot,
            EverythingBootstrapFailure::
                ManifestDownloadFailed,
            ERROR_INVALID_DATA,
            progress);
    }

    snapshot.selectedVersion =
        *stableVersion;
    snapshot.availableVersion =
        *stableVersion;
    snapshot.usedPinnedVersionFallback =
        false;
    snapshot.updateAvailable =
        snapshot.installedVersion.empty() ||
        CompareEverythingVersions(
            snapshot.availableVersion,
            snapshot.installedVersion) > 0;
    snapshot.stage =
        EverythingBootstrapStage::Ready;
    snapshot.failure =
        EverythingBootstrapFailure::None;
    snapshot.nativeError = 0;
    snapshot.running = false;

    if (progress) {
        progress(snapshot);
    }

    return snapshot;
}

EverythingBootstrapSnapshot
RunEverythingBootstrap(
    const std::filesystem::path& dataDirectory,
    bool allowDownload,
    EverythingBootstrapProgress progress,
    std::stop_token stopToken,
    bool showManagedTrayIcon,
    bool forceManagedUpdate) {
    EverythingBootstrapSnapshot snapshot;
    snapshot.running = true;

    const auto activeManagedExecutable =
        ActiveManagedEverythingExecutable(
            dataDirectory);
    const auto existingManagedExecutable =
        activeManagedExecutable.value_or(
            ManagedEverythingExecutable(
                dataDirectory));

    if (FileExists(
            existingManagedExecutable)) {
        if (const auto version =
                VersionFromManagedExecutablePath(
                    existingManagedExecutable)) {
            snapshot.installedVersion =
                *version;
        }
    }

    const auto finishManaged =
        [&](const std::filesystem::path&
                executable,
            bool allowElevation)
            -> EverythingBootstrapSnapshot {
            snapshot.source =
                EverythingBootstrapSource::
                    Managed;
            snapshot.executablePath =
                executable;

            const auto outcome =
                StartManagedEverything(
                    dataDirectory,
                    executable,
                    allowElevation,
                    showManagedTrayIcon,
                    snapshot,
                    progress,
                    stopToken);

            switch (outcome.result) {
            case ManagedRuntimeResult::Ready:
                if (const auto version =
                        VersionFromManagedExecutablePath(
                            executable)) {
                    snapshot.installedVersion =
                        *version;
                }

                snapshot.updateAvailable =
                    !snapshot.availableVersion.empty() &&
                    !snapshot.installedVersion.empty() &&
                    CompareEverythingVersions(
                        snapshot.availableVersion,
                        snapshot.installedVersion) > 0;
                snapshot.stage =
                    EverythingBootstrapStage::
                        Ready;
                snapshot.failure =
                    EverythingBootstrapFailure::
                        None;
                snapshot.running = false;
                snapshot.nativeError = 0;

                if (progress) {
                    progress(snapshot);
                }
                return snapshot;

            case ManagedRuntimeResult::
                NeedsService:
                return NeedsInstall(
                    snapshot,
                    EverythingBootstrapFailure::
                        ServiceRequired,
                    progress);

            case ManagedRuntimeResult::
                NeedsServiceRepair:
                return NeedsInstall(
                    snapshot,
                    EverythingBootstrapFailure::
                        ServiceRepairRequired,
                    progress);

            case ManagedRuntimeResult::
                Cancelled:
                return Fail(
                    snapshot,
                    EverythingBootstrapFailure::
                        Cancelled,
                    outcome.nativeError,
                    progress);

            case ManagedRuntimeResult::
                StopFailed:
                return Fail(
                    snapshot,
                    EverythingBootstrapFailure::
                        ManagedStopFailed,
                    outcome.nativeError,
                    progress);

            case ManagedRuntimeResult::
                ConfigFailed:
                return Fail(
                    snapshot,
                    EverythingBootstrapFailure::
                        ManagedConfigFailed,
                    outcome.nativeError,
                    progress);

            case ManagedRuntimeResult::
                ServiceElevationCancelled:
                return Fail(
                    snapshot,
                    EverythingBootstrapFailure::
                        ServiceElevationCancelled,
                    outcome.nativeError,
                    progress);

            case ManagedRuntimeResult::
                ServiceInstallFailed:
                return Fail(
                    snapshot,
                    EverythingBootstrapFailure::
                        ServiceInstallFailed,
                    outcome.nativeError,
                    progress);

            case ManagedRuntimeResult::
                ServiceRepairFailed:
                return Fail(
                    snapshot,
                    EverythingBootstrapFailure::
                        ServiceRepairFailed,
                    outcome.nativeError,
                    progress);

            case ManagedRuntimeResult::
                ServiceUnavailable:
                return Fail(
                    snapshot,
                    EverythingBootstrapFailure::
                        ServiceUnavailable,
                    outcome.nativeError,
                    progress);

            case ManagedRuntimeResult::
                LaunchFailed:
                return Fail(
                    snapshot,
                    EverythingBootstrapFailure::
                        ManagedLaunchFailed,
                    outcome.nativeError,
                    progress);

            case ManagedRuntimeResult::
                IpcUnavailable:
                return Fail(
                    snapshot,
                    EverythingBootstrapFailure::
                        IpcUnavailable,
                    outcome.nativeError,
                    progress);
            }

            return Fail(
                snapshot,
                EverythingBootstrapFailure::
                    ServiceUnavailable,
                ERROR_INVALID_DATA,
                progress);
        };

    Report(
        snapshot,
        EverythingBootstrapStage::
            Discovering,
        progress);

    if (AnyUsableIpcEndpoint()) {
        if (FileExists(
                existingManagedExecutable) &&
            ManagedDefaultIpcRunning(
                existingManagedExecutable)) {
            if (!forceManagedUpdate) {
                return finishManaged(
                    existingManagedExecutable,
                    allowDownload);
            }
        } else {
            // A user-managed/external Everything owns the active endpoint.
            // Never replace or reconfigure it, even when an update operation
            // was requested for ALTRun's managed copy.
            snapshot.stage =
                EverythingBootstrapStage::
                    Ready;
            snapshot.running = false;
            snapshot.failure =
                EverythingBootstrapFailure::
                    None;

            if (progress) {
                progress(snapshot);
            }

            return snapshot;
        }
    }

    const auto candidates =
        FindExistingCandidates(
            dataDirectory);

    if (!candidates.empty()) {
        const auto& candidate =
            candidates.front();

        snapshot.source =
            candidate.source;
        snapshot.executablePath =
            candidate.path;

        if (candidate.source ==
            EverythingBootstrapSource::
                Managed) {
            if (!forceManagedUpdate) {
                return finishManaged(
                    candidate.path,
                    allowDownload);
            }
        }

        if (candidate.source ==
                EverythingBootstrapSource::
                    Managed &&
            forceManagedUpdate) {
            // Continue below to stable-version resolution/download.
        } else {
            Report(
                snapshot,
                EverythingBootstrapStage::
                    StartingExisting,
                progress);

            std::uint32_t launchError = 0;

            if (LaunchEverything(
                    candidate.path,
                    launchError)) {
            Report(
                snapshot,
                EverythingBootstrapStage::
                    WaitingForIpc,
                progress);

            if (WaitForIpc(
                    stopToken)) {
                snapshot.stage =
                    EverythingBootstrapStage::
                        Ready;
                snapshot.running = false;
                snapshot.failure =
                    EverythingBootstrapFailure::
                        None;

                if (progress) {
                    progress(snapshot);
                }

                return snapshot;
            }
        } else if (!allowDownload) {
            return Fail(
                snapshot,
                EverythingBootstrapFailure::
                    ExistingLaunchFailed,
                launchError,
                progress);
        }

        if (stopToken.stop_requested()) {
            return Fail(
                snapshot,
                EverythingBootstrapFailure::
                    Cancelled,
                ERROR_CANCELLED,
                progress);
        }

            if (!allowDownload) {
                return NeedsInstall(
                    snapshot,
                    EverythingBootstrapFailure::
                        IpcUnavailable,
                    progress);
            }
        }
    } else if (!allowDownload) {
        return NeedsInstall(
            snapshot,
            EverythingBootstrapFailure::
                NotFound,
            progress);
    }

    snapshot.selectedVersion =
        std::wstring(
            PinnedManagedEverythingVersion());
    snapshot.usedPinnedVersionFallback =
        true;

    Report(
        snapshot,
        EverythingBootstrapStage::
            ResolvingStableVersion,
        progress);

    std::string stableMetadata;
    std::uint32_t stableMetadataError = 0;

    if (DownloadText(
            EverythingStableUpdateMetadataUrl(),
            stableMetadata,
            stableMetadataError,
            stopToken)) {
        if (const auto stableVersion =
                ParseEverythingStableUpdateVersion(
                    stableMetadata)) {
            snapshot.selectedVersion =
                *stableVersion;
            snapshot.availableVersion =
                *stableVersion;
            snapshot.usedPinnedVersionFallback =
                false;
        } else if (forceManagedUpdate) {
            return Fail(
                snapshot,
                EverythingBootstrapFailure::
                    ManifestDownloadFailed,
                ERROR_INVALID_DATA,
                progress);
        }
    } else if (
        stopToken.stop_requested()) {
        return Fail(
            snapshot,
            EverythingBootstrapFailure::
                Cancelled,
            ERROR_CANCELLED,
            progress);
    } else if (forceManagedUpdate) {
        return Fail(
            snapshot,
            EverythingBootstrapFailure::
                ManifestDownloadFailed,
            stableMetadataError,
            progress);
    }

    // Failure to resolve the online stable release is intentionally not a
    // setup failure. The pinned build is a CI-validated known-good fallback;
    // its package and official SHA-256 manifest are still verified below.
    snapshot.updateAvailable =
        !snapshot.availableVersion.empty() &&
        !snapshot.installedVersion.empty() &&
        CompareEverythingVersions(
            snapshot.availableVersion,
            snapshot.installedVersion) > 0;

    if (forceManagedUpdate &&
        !snapshot.updateAvailable &&
        FileExists(
            existingManagedExecutable)) {
        return finishManaged(
            existingManagedExecutable,
            true);
    }

    const auto spec =
        ManagedEverythingPackage(
            CurrentArchitecture(),
            snapshot.selectedVersion);

    const auto targetManagedExecutable =
        ManagedEverythingExecutableForVersion(
            dataDirectory,
            snapshot.selectedVersion);
    const auto managedDirectory =
        targetManagedExecutable.parent_path();

    const auto toolsRoot =
        ManagedEverythingRoot(
            dataDirectory);

    std::error_code ec;
    std::filesystem::create_directories(
        toolsRoot,
        ec);

    if (ec) {
        return Fail(
            snapshot,
            EverythingBootstrapFailure::
                CreateDirectoryFailed,
            static_cast<std::uint32_t>(
                ec.value()),
            progress);
    }

    Report(
        snapshot,
        EverythingBootstrapStage::
            DownloadingManifest,
        progress);

    std::string manifest;
    std::uint32_t nativeError = 0;

    if (!DownloadText(
            spec.checksumManifestUrl,
            manifest,
            nativeError,
            stopToken)) {
        return Fail(
            snapshot,
            stopToken.stop_requested()
                ? EverythingBootstrapFailure::
                    Cancelled
                : EverythingBootstrapFailure::
                    ManifestDownloadFailed,
            nativeError,
            progress);
    }

    const std::string packageName =
        NarrowAscii(
            spec.fileName);

    const auto expectedHash =
        FindSha256ForFile(
            manifest,
            packageName);

    if (!expectedHash) {
        return Fail(
            snapshot,
            EverythingBootstrapFailure::
                PackageChecksumMissing,
            ERROR_INVALID_DATA,
            progress);
    }

    const auto archiveNames =
        ManagedEverythingArchiveNames(
            spec);
    const auto downloadArchive =
        toolsRoot /
        archiveNames.downloadFileName;
    const auto verifiedArchive =
        toolsRoot /
        archiveNames.verifiedZipFileName;

    // Keep partial/unverified bytes under a non-ZIP extension so they
    // can never be opened accidentally. Windows Shell's ZIP namespace,
    // however, recognizes archives by extension, so only the verified
    // package is promoted to the real .zip name before extraction.
    ec.clear();
    std::filesystem::remove(
        downloadArchive,
        ec);
    ec.clear();
    std::filesystem::remove(
        verifiedArchive,
        ec);

    snapshot.source =
        EverythingBootstrapSource::
            Downloaded;
    snapshot.executablePath =
        targetManagedExecutable;

    if (!DownloadFile(
            spec.downloadUrl,
            downloadArchive,
            snapshot,
            progress,
            nativeError,
            stopToken)) {
        ec.clear();
        std::filesystem::remove(
            downloadArchive,
            ec);

        return Fail(
            snapshot,
            stopToken.stop_requested()
                ? EverythingBootstrapFailure::
                    Cancelled
                : EverythingBootstrapFailure::
                    PackageDownloadFailed,
            nativeError,
            progress);
    }

    Report(
        snapshot,
        EverythingBootstrapStage::
            VerifyingPackage,
        progress);

    const auto actualHash =
        Sha256File(
            downloadArchive,
            nativeError);

    if (!actualHash) {
        ec.clear();
        std::filesystem::remove(
            downloadArchive,
            ec);

        return Fail(
            snapshot,
            EverythingBootstrapFailure::
                PackageHashFailed,
            nativeError,
            progress);
    }

    if (*actualHash !=
        *expectedHash) {
        ec.clear();
        std::filesystem::remove(
            downloadArchive,
            ec);

        return Fail(
            snapshot,
            EverythingBootstrapFailure::
                PackageHashMismatch,
            ERROR_CRC,
            progress);
    }

    ec.clear();
    std::filesystem::rename(
        downloadArchive,
        verifiedArchive,
        ec);

    if (ec) {
        const auto stageError =
            static_cast<std::uint32_t>(
                ec.value());

        std::error_code cleanupError;
        std::filesystem::remove(
            downloadArchive,
            cleanupError);
        cleanupError.clear();
        std::filesystem::remove(
            verifiedArchive,
            cleanupError);

        return Fail(
            snapshot,
            EverythingBootstrapFailure::
                PackageStagingFailed,
            stageError,
            progress);
    }

    Report(
        snapshot,
        EverythingBootstrapStage::
            ExtractingPackage,
        progress);

    std::wstring extractionToken;
    if (!GenerateSecureToken(extractionToken, nativeError)) {
        return Fail(snapshot, EverythingBootstrapFailure::ExtractionFailed,
                    nativeError, progress);
    }
    const auto stagingDirectory = toolsRoot /
        (L".asterun-extract-" + extractionToken);

    if (!ExtractZipVerified(
            verifiedArchive,
            stagingDirectory,
            nativeError,
            stopToken)) {
        ec.clear();
        std::filesystem::remove(
            verifiedArchive,
            ec);
        ec.clear();
        std::filesystem::remove_all(stagingDirectory, ec);

        return Fail(
            snapshot,
            stopToken.stop_requested()
                ? EverythingBootstrapFailure::
                    Cancelled
                : EverythingBootstrapFailure::
                    ExtractionFailed,
            nativeError,
            progress);
    }

    ec.clear();
    std::filesystem::remove(
        verifiedArchive,
        ec);

    ec.clear();
    std::filesystem::rename(stagingDirectory, managedDirectory, ec);
    if (ec) {
        const auto stagingError = static_cast<std::uint32_t>(ec.value());
        std::error_code cleanupError;
        std::filesystem::remove_all(stagingDirectory, cleanupError);
        return Fail(snapshot, EverythingBootstrapFailure::ExtractionFailed,
                    stagingError, progress);
    }

    if (!FileExists(
            targetManagedExecutable)) {
        return Fail(
            snapshot,
            EverythingBootstrapFailure::
                ManagedExecutableMissing,
            ERROR_FILE_NOT_FOUND,
            progress);
    }

    snapshot.downloaded = true;
    snapshot.executablePath =
        targetManagedExecutable;

    if (FileExists(
            existingManagedExecutable) &&
        LowerPath(
            existingManagedExecutable) !=
            LowerPath(
                targetManagedExecutable) &&
        ManagedDefaultIpcRunning(
            existingManagedExecutable)) {
        Report(
            snapshot,
            EverythingBootstrapStage::
                StoppingManaged,
            progress);

        if (!LaunchEverythingCommand(
                existingManagedExecutable,
                L"-exit",
                true,
                nativeError) ||
            !WaitForManagedDefaultIpcToExit(
                existingManagedExecutable,
                stopToken)) {
            return Fail(
                snapshot,
                stopToken.stop_requested()
                    ? EverythingBootstrapFailure::
                          Cancelled
                    : EverythingBootstrapFailure::
                          ManagedStopFailed,
                stopToken.stop_requested()
                    ? ERROR_CANCELLED
                    : (nativeError != 0
                           ? nativeError
                           : ERROR_TIMEOUT),
                progress);
        }
    }

    return finishManaged(
        targetManagedExecutable,
        true);
}

} // namespace altrun::win
