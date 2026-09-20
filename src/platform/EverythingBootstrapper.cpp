#include "EverythingBootstrapper.hpp"

#include "../core/EverythingBootstrapPolicy.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <bcrypt.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shldisp.h>
#include <winhttp.h>

#include <algorithm>
#include <array>
#include <chrono>
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

struct ComApartment {
    HRESULT result{
        CoInitializeEx(
            nullptr,
            COINIT_APARTMENTTHREADED)};

    ~ComApartment() {
        if (SUCCEEDED(result)) {
            CoUninitialize();
        }
    }

    [[nodiscard]] bool Ready() const {
        return SUCCEEDED(result) ||
            result == RPC_E_CHANGED_MODE;
    }
};

template <typename T>
struct ComPtr {
    T* value{nullptr};

    ~ComPtr() {
        if (value) {
            value->Release();
        }
    }

    ComPtr() = default;
    ComPtr(const ComPtr&) = delete;
    ComPtr& operator=(
        const ComPtr&) = delete;

    [[nodiscard]] T** Put() {
        if (value) {
            value->Release();
            value = nullptr;
        }
        return &value;
    }

    [[nodiscard]] T* Get() const {
        return value;
    }
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

[[nodiscard]] std::wstring
VersionDirectoryName() {
    const auto spec =
        ManagedEverythingPackage(
            CurrentArchitecture());

    return spec.version +
        (CurrentArchitecture() ==
                 EverythingPackageArchitecture::
                     Arm64
             ? L"-ARM64"
             : L"-x64");
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

[[nodiscard]] bool
RunElevatedEverythingCommand(
    const std::filesystem::path& executable,
    std::wstring_view arguments,
    std::uint32_t& nativeError) {
    std::wstring args(arguments);
    std::wstring directory =
        executable.parent_path()
            .wstring();

    SHELLEXECUTEINFOW info{};
    info.cbSize = sizeof(info);
    info.fMask =
        SEE_MASK_NOCLOSEPROCESS |
        SEE_MASK_NOASYNC |
        SEE_MASK_FLAG_NO_UI;
    info.hwnd = nullptr;
    info.lpVerb = L"runas";
    info.lpFile = executable.c_str();
    info.lpParameters =
        args.empty()
            ? nullptr
            : args.c_str();
    info.lpDirectory =
        directory.empty()
            ? nullptr
            : directory.c_str();
    info.nShow = SW_HIDE;

    if (!ShellExecuteExW(&info)) {
        nativeError =
            static_cast<std::uint32_t>(
                GetLastError());
        return false;
    }

    if (!info.hProcess) {
        nativeError =
            ERROR_INVALID_HANDLE;
        return false;
    }

    const DWORD waitResult =
        WaitForSingleObject(
            info.hProcess,
            30000);

    if (waitResult != WAIT_OBJECT_0) {
        nativeError =
            waitResult == WAIT_TIMEOUT
                ? ERROR_TIMEOUT
                : static_cast<std::uint32_t>(
                      GetLastError());
        CloseHandle(info.hProcess);
        return false;
    }

    DWORD exitCode = 0;

    if (!GetExitCodeProcess(
            info.hProcess,
            &exitCode)) {
        nativeError =
            static_cast<std::uint32_t>(
                GetLastError());
        CloseHandle(info.hProcess);
        return false;
    }

    CloseHandle(info.hProcess);

    if (exitCode != 0) {
        nativeError = exitCode;
        return false;
    }

    nativeError = 0;
    return true;
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

        if (nativeError ==
            ERROR_SERVICE_DOES_NOT_EXIST) {
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
WindowOwnedByExecutable(
    HWND hwnd,
    const std::filesystem::path& executable) {
    if (!hwnd) {
        return false;
    }

    DWORD processId = 0;
    GetWindowThreadProcessId(
        hwnd,
        &processId);

    if (processId == 0) {
        return false;
    }

    HANDLE process =
        OpenProcess(
            PROCESS_QUERY_LIMITED_INFORMATION,
            FALSE,
            processId);

    if (!process) {
        return false;
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
        return false;
    }

    return LowerPath(
               std::filesystem::path(
                   std::wstring(
                       buffer.data(),
                       size))) ==
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
            existing) != existing;
    nativeError = 0;
    return true;
}

[[nodiscard]] bool
ConfigureManagedEverything(
    const std::filesystem::path& executable,
    std::uint32_t& nativeError) {
    const auto iniPath =
        executable.parent_path() /
        L"Everything.ini";
    const auto tempPath =
        executable.parent_path() /
        L"Everything.ini.altrun.tmp";

    std::string existing;

    if (!ReadManagedIni(
            iniPath,
            existing,
            nativeError)) {
        return false;
    }

    const std::string configured =
        ApplyManagedEverythingIniPolicy(
            existing);

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
    Cancelled,
    StopFailed,
    ConfigFailed,
    ServiceElevationCancelled,
    ServiceInstallFailed,
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
    const std::filesystem::path& executable,
    bool allowElevation,
    EverythingBootstrapSnapshot& snapshot,
    const EverythingBootstrapProgress&
        progress,
    std::stop_token stopToken) {
    bool configNeedsUpdate = false;
    std::uint32_t nativeError = 0;

    if (!ManagedIniNeedsUpdate(
            executable,
            configNeedsUpdate,
            nativeError)) {
        return {
            ManagedRuntimeResult::
                ConfigFailed,
            nativeError,
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

    const bool managedRunning =
        ManagedDefaultIpcRunning(
            executable);

    if (managedRunning &&
        serviceStatus ==
            ServiceProbe::Running &&
        !configNeedsUpdate) {
        return {};
    }

    if (serviceStatus !=
            ServiceProbe::Running &&
        !allowElevation) {
        return {
            ManagedRuntimeResult::
                NeedsService,
            ERROR_SERVICE_NOT_ACTIVE,
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
            nativeError)) {
        return {
            ManagedRuntimeResult::
                ConfigFailed,
            nativeError,
        };
    }

    if (serviceStatus !=
        ServiceProbe::Running) {
        Report(
            snapshot,
            EverythingBootstrapStage::
                InstallingService,
            progress);

        const std::wstring_view command =
            serviceStatus ==
                    ServiceProbe::Missing
                ? L"-install-service"
                : L"-start-service";

        if (!RunElevatedEverythingCommand(
                executable,
                command,
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
    InternetHandle request;
};

[[nodiscard]] bool
OpenHttpRequest(
    std::wstring_view url,
    HttpRequest& handles,
    std::uint64_t& contentLength,
    std::uint32_t& nativeError) {
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
            L"ALTRunNext/0.7 Managed Everything",
            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
            WINHTTP_NO_PROXY_NAME,
            WINHTTP_NO_PROXY_BYPASS,
            0);

    if (!handles.session.value) {
        nativeError =
            static_cast<std::uint32_t>(
                GetLastError());
        return false;
    }

    WinHttpSetTimeouts(
        handles.session.value,
        5000,
        5000,
        10000,
        10000);

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

    handles.request.value =
        WinHttpOpenRequest(
            handles.connection.value,
            L"GET",
            object.c_str(),
            nullptr,
            WINHTTP_NO_REFERER,
            WINHTTP_DEFAULT_ACCEPT_TYPES,
            WINHTTP_FLAG_SECURE);

    if (!handles.request.value) {
        nativeError =
            static_cast<std::uint32_t>(
                GetLastError());
        return false;
    }

    if (!WinHttpSendRequest(
            handles.request.value,
            WINHTTP_NO_ADDITIONAL_HEADERS,
            0,
            WINHTTP_NO_REQUEST_DATA,
            0,
            0,
            0) ||
        !WinHttpReceiveResponse(
            handles.request.value,
            nullptr)) {
        nativeError =
            static_cast<std::uint32_t>(
                GetLastError());
        return false;
    }

    DWORD status = 0;
    DWORD statusBytes =
        sizeof(status);

    if (!WinHttpQueryHeaders(
            handles.request.value,
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
            handles.request.value,
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
            nativeError) ||
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

        if (!WinHttpQueryDataAvailable(
                handles.request.value,
                &available)) {
            nativeError =
                static_cast<std::uint32_t>(
                    GetLastError());
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

        const std::size_t oldSize =
            output.size();

        output.resize(
            oldSize + available);

        DWORD read = 0;

        if (!WinHttpReadData(
                handles.request.value,
                output.data() +
                    oldSize,
                available,
                &read)) {
            nativeError =
                static_cast<std::uint32_t>(
                    GetLastError());
            return false;
        }

        output.resize(
            oldSize + read);
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
            nativeError) ||
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

    std::vector<char> buffer;

    for (;;) {
        if (stopToken.stop_requested()) {
            nativeError =
                ERROR_CANCELLED;
            return false;
        }

        DWORD available = 0;

        if (!WinHttpQueryDataAvailable(
                handles.request.value,
                &available)) {
            nativeError =
                static_cast<std::uint32_t>(
                    GetLastError());
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

        buffer.resize(available);
        DWORD read = 0;

        if (!WinHttpReadData(
                handles.request.value,
                buffer.data(),
                available,
                &read)) {
            nativeError =
                static_cast<std::uint32_t>(
                    GetLastError());
            return false;
        }

        output.write(
            buffer.data(),
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

[[nodiscard]]
std::optional<std::string>
Sha256File(
    const std::filesystem::path& path,
    std::uint32_t& nativeError) {
    BCRYPT_ALG_HANDLE algorithm =
        nullptr;
    BCRYPT_HASH_HANDLE hash =
        nullptr;

    DWORD objectLength = 0;
    DWORD objectLengthBytes =
        sizeof(objectLength);
    DWORD hashLength = 0;
    DWORD hashLengthBytes =
        sizeof(hashLength);

    if (BCryptOpenAlgorithmProvider(
            &algorithm,
            BCRYPT_SHA256_ALGORITHM,
            nullptr,
            0) != 0) {
        nativeError =
            ERROR_INVALID_FUNCTION;
        return std::nullopt;
    }

    const auto closeAlgorithm =
        [&]() {
            if (algorithm) {
                BCryptCloseAlgorithmProvider(
                    algorithm,
                    0);
                algorithm = nullptr;
            }
        };

    if (BCryptGetProperty(
            algorithm,
            BCRYPT_OBJECT_LENGTH,
            reinterpret_cast<PUCHAR>(
                &objectLength),
            sizeof(objectLength),
            &objectLengthBytes,
            0) != 0 ||
        BCryptGetProperty(
            algorithm,
            BCRYPT_HASH_LENGTH,
            reinterpret_cast<PUCHAR>(
                &hashLength),
            sizeof(hashLength),
            &hashLengthBytes,
            0) != 0 ||
        hashLength != 32) {
        closeAlgorithm();
        nativeError =
            ERROR_INVALID_DATA;
        return std::nullopt;
    }

    std::vector<UCHAR> object(
        objectLength);

    if (BCryptCreateHash(
            algorithm,
            &hash,
            object.data(),
            objectLength,
            nullptr,
            0,
            0) != 0) {
        closeAlgorithm();
        nativeError =
            ERROR_INVALID_FUNCTION;
        return std::nullopt;
    }

    const auto closeHash =
        [&]() {
            if (hash) {
                BCryptDestroyHash(hash);
                hash = nullptr;
            }
        };

    std::ifstream input(
        path,
        std::ios::binary);

    if (!input) {
        closeHash();
        closeAlgorithm();
        nativeError =
            ERROR_OPEN_FAILED;
        return std::nullopt;
    }

    std::array<char, 64 * 1024>
        buffer{};

    while (input) {
        input.read(
            buffer.data(),
            static_cast<
                std::streamsize>(
                buffer.size()));

        const auto count =
            input.gcount();

        if (count > 0 &&
            BCryptHashData(
                hash,
                reinterpret_cast<PUCHAR>(
                    buffer.data()),
                static_cast<ULONG>(
                    count),
                0) != 0) {
            closeHash();
            closeAlgorithm();
            nativeError =
                ERROR_INVALID_DATA;
            return std::nullopt;
        }
    }

    if (!input.eof()) {
        closeHash();
        closeAlgorithm();
        nativeError =
            ERROR_READ_FAULT;
        return std::nullopt;
    }

    std::array<UCHAR, 32>
        digest{};

    if (BCryptFinishHash(
            hash,
            digest.data(),
            static_cast<ULONG>(
                digest.size()),
            0) != 0) {
        closeHash();
        closeAlgorithm();
        nativeError =
            ERROR_INVALID_DATA;
        return std::nullopt;
    }

    closeHash();
    closeAlgorithm();

    constexpr char hex[] =
        "0123456789abcdef";

    std::string value;
    value.reserve(
        digest.size() * 2);

    for (const auto byte : digest) {
        value.push_back(
            hex[(byte >> 4) & 0x0f]);
        value.push_back(
            hex[byte & 0x0f]);
    }

    nativeError = 0;
    return value;
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
ExtractZipWithShell(
    const std::filesystem::path& archive,
    const std::filesystem::path& destination,
    std::uint32_t& nativeError,
    std::stop_token stopToken) {
    std::error_code ec;
    std::filesystem::create_directories(
        destination,
        ec);

    if (ec) {
        nativeError =
            static_cast<std::uint32_t>(
                ec.value());
        return false;
    }

    ComApartment apartment;

    if (!apartment.Ready()) {
        nativeError =
            static_cast<std::uint32_t>(
                apartment.result);
        return false;
    }

    ComPtr<IShellDispatch> shell;

    HRESULT hr =
        CoCreateInstance(
            CLSID_Shell,
            nullptr,
            CLSCTX_INPROC_SERVER,
            IID_IShellDispatch,
        reinterpret_cast<void**>(
            shell.Put()));

    if (FAILED(hr) ||
        !shell.Get()) {
        nativeError =
            static_cast<std::uint32_t>(
                hr);
        return false;
    }

    VARIANT archiveVariant;
    VariantInit(&archiveVariant);
    archiveVariant.vt = VT_BSTR;
    archiveVariant.bstrVal =
        SysAllocString(
            archive.c_str());

    VARIANT destinationVariant;
    VariantInit(&destinationVariant);
    destinationVariant.vt = VT_BSTR;
    destinationVariant.bstrVal =
        SysAllocString(
            destination.c_str());

    if (!archiveVariant.bstrVal ||
        !destinationVariant.bstrVal) {
        VariantClear(
            &archiveVariant);
        VariantClear(
            &destinationVariant);
        nativeError =
            ERROR_NOT_ENOUGH_MEMORY;
        return false;
    }

    ComPtr<Folder> sourceFolder;
    ComPtr<Folder> destinationFolder;

    hr = shell.Get()->NameSpace(
        archiveVariant,
        sourceFolder.Put());

    if (SUCCEEDED(hr)) {
        hr = shell.Get()->NameSpace(
            destinationVariant,
            destinationFolder.Put());
    }

    VariantClear(
        &archiveVariant);
    VariantClear(
        &destinationVariant);

    if (FAILED(hr) ||
        !sourceFolder.Get() ||
        !destinationFolder.Get()) {
        nativeError =
            static_cast<std::uint32_t>(
                FAILED(hr)
                    ? hr
                    : E_FAIL);
        return false;
    }

    ComPtr<FolderItems> items;

    hr = sourceFolder.Get()->Items(
        items.Put());

    if (FAILED(hr) ||
        !items.Get()) {
        nativeError =
            static_cast<std::uint32_t>(
                FAILED(hr)
                    ? hr
                    : E_FAIL);
        return false;
    }

    VARIANT itemVariant;
    VariantInit(&itemVariant);
    itemVariant.vt =
        VT_DISPATCH;
    itemVariant.pdispVal =
        items.Get();
    itemVariant.pdispVal->AddRef();

    VARIANT options;
    VariantInit(&options);
    options.vt = VT_I4;
    options.lVal =
        FOF_SILENT |
        FOF_NOCONFIRMATION |
        FOF_NOERRORUI |
        FOF_NOCONFIRMMKDIR;

    hr = destinationFolder.Get()->
        CopyHere(
            itemVariant,
            options);

    VariantClear(
        &itemVariant);

    if (FAILED(hr)) {
        nativeError =
            static_cast<std::uint32_t>(
                hr);
        return false;
    }

    const auto executable =
        destination /
        L"Everything.exe";

    std::uintmax_t previousSize = 0;
    int stableCount = 0;

    const auto deadline =
        std::chrono::steady_clock::now() +
        std::chrono::seconds(20);

    while (!stopToken.stop_requested() &&
           std::chrono::steady_clock::now() <
               deadline) {
        ec.clear();

        if (std::filesystem::is_regular_file(
                executable,
                ec) &&
            !ec) {
            const auto size =
                std::filesystem::file_size(
                    executable,
                    ec);

            if (!ec && size > 0) {
                if (size ==
                    previousSize) {
                    ++stableCount;
                } else {
                    previousSize =
                        size;
                    stableCount = 0;
                }

                if (stableCount >= 3) {
                    nativeError = 0;
                    return true;
                }
            }
        }

        std::this_thread::sleep_for(
            std::chrono::milliseconds(
                150));
    }

    nativeError =
        stopToken.stop_requested()
            ? ERROR_CANCELLED
            : ERROR_TIMEOUT;
    return false;
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

std::filesystem::path
ManagedEverythingExecutable(
    const std::filesystem::path&
        dataDirectory) {
    return dataDirectory /
        L"tools" /
        L"Everything" /
        VersionDirectoryName() /
        L"Everything.exe";
}

bool EverythingIpcEndpointAvailable() {
    return AnyUsableIpcEndpoint();
}

EverythingBootstrapSnapshot
RunEverythingBootstrap(
    const std::filesystem::path& dataDirectory,
    bool allowDownload,
    EverythingBootstrapProgress progress,
    std::stop_token stopToken) {
    EverythingBootstrapSnapshot snapshot;
    snapshot.running = true;

    const auto managedExecutable =
        ManagedEverythingExecutable(
            dataDirectory);

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
                    executable,
                    allowElevation,
                    snapshot,
                    progress,
                    stopToken);

            switch (outcome.result) {
            case ManagedRuntimeResult::Ready:
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
                managedExecutable) &&
            ManagedDefaultIpcRunning(
                managedExecutable)) {
            return finishManaged(
                managedExecutable,
                allowDownload);
        }

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
            return finishManaged(
                candidate.path,
                allowDownload);
        }

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
    } else if (!allowDownload) {
        return NeedsInstall(
            snapshot,
            EverythingBootstrapFailure::
                NotFound,
            progress);
    }

    const auto spec =
        ManagedEverythingPackage(
            CurrentArchitecture());

    const auto managedDirectory =
        managedExecutable.parent_path();

    const auto toolsRoot =
        managedDirectory
            .parent_path();

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
        ManagedEverythingExecutable(
            dataDirectory);

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

    if (!ExtractZipWithShell(
            verifiedArchive,
            managedDirectory,
            nativeError,
            stopToken)) {
        ec.clear();
        std::filesystem::remove(
            verifiedArchive,
            ec);

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

    if (!FileExists(
            managedExecutable)) {
        return Fail(
            snapshot,
            EverythingBootstrapFailure::
                ManagedExecutableMissing,
            ERROR_FILE_NOT_FOUND,
            progress);
    }

    snapshot.downloaded = true;
    snapshot.executablePath =
        managedExecutable;

    return finishManaged(
        managedExecutable,
        true);
}

} // namespace altrun::win
