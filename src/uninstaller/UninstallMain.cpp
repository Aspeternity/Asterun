#include "../platform/WindowsCommandLine.hpp"
#include "../ui/Feedback.hpp"
#include "../platform/SecureElevation.hpp"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <ole2.h>
#include <exdisp.h>
#include <restartmanager.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <shlobj.h>
#include <tlhelp32.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cwchar>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace {

constexpr wchar_t kLauncherClass[] =
    L"Asterun.Launcher";
constexpr wchar_t kLauncherTitle[] =
    L"Asterun";
constexpr wchar_t kEverythingService[] =
    L"Everything";

struct ServiceHandle {
    SC_HANDLE value{nullptr};

    ~ServiceHandle() {
        if (value) {
            CloseServiceHandle(value);
        }
    }
};

struct EventHandle {
    HANDLE value{nullptr};

    ~EventHandle() {
        if (value) {
            CloseHandle(value);
        }
    }

    EventHandle() = default;
    EventHandle(const EventHandle&) = delete;
    EventHandle& operator=(const EventHandle&) = delete;
};

struct DirectoryHandle {
    HANDLE value{INVALID_HANDLE_VALUE};

    ~DirectoryHandle() {
        Reset();
    }

    DirectoryHandle() = default;
    DirectoryHandle(const DirectoryHandle&) = delete;
    DirectoryHandle& operator=(const DirectoryHandle&) = delete;

    [[nodiscard]] bool Valid() const {
        return value !=
                INVALID_HANDLE_VALUE &&
            value != nullptr;
    }

    void Reset() {
        if (Valid()) {
            CloseHandle(value);
        }
        value = INVALID_HANDLE_VALUE;
    }
};

struct PerformArguments {
    DWORD parentPid{0};
    std::filesystem::path install;
    bool deleteData{false};
    std::wstring shellReleaseRequest;
    std::wstring shellReleaseDone;
    // Legacy option name; now acknowledges that the broker may exit.
    std::wstring shellLeaseAcquired;
};

struct RemovalFailure {
    DWORD error{ERROR_SUCCESS};
    std::filesystem::path path;
    std::wstring lockOwners;
};

[[nodiscard]] bool
RemoveAllWithRetry(
    const std::filesystem::path& path,
    RemovalFailure& failure);

std::filesystem::path gRemovalFailurePath;
std::wstring gRemovalFailureLockOwners;
std::wstring gUninstallStage;

[[nodiscard]] bool
ChineseUi() {
    return PRIMARYLANGID(
               GetUserDefaultUILanguage()) ==
        LANG_CHINESE;
}

[[nodiscard]] std::filesystem::path
CurrentExecutable() {
    std::array<wchar_t, 32768> buffer{};
    const DWORD length =
        GetModuleFileNameW(
            nullptr,
            buffer.data(),
            static_cast<DWORD>(
                buffer.size()));

    if (length == 0 ||
        length >= buffer.size()) {
        return {};
    }

    return std::filesystem::path(
        std::wstring(
            buffer.data(),
            length));
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

    while (value.size() > 3 &&
           value.back() == L'\\') {
        value.pop_back();
    }

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

[[nodiscard]] std::wstring
QuoteArgument(std::wstring_view value) {
    return altrun::win::QuoteWindowsArgument(value);
}
[[nodiscard]] std::wstring
ExtractExecutable(
    std::wstring_view command) {
    while (!command.empty() &&
           std::iswspace(
               command.front())) {
        command.remove_prefix(1);
    }

    if (command.empty()) {
        return {};
    }

    if (command.front() == L'\"') {
        command.remove_prefix(1);
        const auto end =
            command.find(L'\"');
        return end ==
                   std::wstring_view::npos
            ? std::wstring{}
            : std::wstring(
                  command.substr(0, end));
    }

    std::wstring lowered(command);
    std::transform(
        lowered.begin(),
        lowered.end(),
        lowered.begin(),
        [](wchar_t c) {
            return static_cast<wchar_t>(
                std::towlower(c));
        });

    const auto exe =
        lowered.find(L".exe");

    if (exe ==
        std::wstring::npos) {
        return {};
    }

    return std::wstring(
        command.substr(0, exe + 4));
}

[[nodiscard]] std::filesystem::path
ExpandExecutable(
    std::wstring_view value) {
    if (value.empty()) {
        return {};
    }

    const std::wstring input(value);
    const DWORD required =
        ExpandEnvironmentStringsW(
            input.c_str(),
            nullptr,
            0);

    if (required == 0) {
        return {};
    }

    std::vector<wchar_t>
        expanded(required);

    const DWORD written =
        ExpandEnvironmentStringsW(
            input.c_str(),
            expanded.data(),
            required);

    if (written == 0 ||
        written > required) {
        return {};
    }

    return std::filesystem::path(
        expanded.data());
}

[[nodiscard]] bool
ProcessPath(
    DWORD pid,
    std::filesystem::path& path) {
    HANDLE process =
        OpenProcess(
            PROCESS_QUERY_LIMITED_INFORMATION,
            FALSE,
            pid);

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

    if (!ok || size == 0) {
        return false;
    }

    path =
        std::filesystem::path(
            std::wstring(
                buffer.data(),
                size));
    return true;
}

[[nodiscard]] bool
WaitForProcess(
    DWORD pid,
    DWORD timeout) {
    HANDLE process =
        OpenProcess(
            SYNCHRONIZE,
            FALSE,
            pid);

    if (!process) {
        return GetLastError() ==
            ERROR_INVALID_PARAMETER;
    }

    const DWORD wait =
        WaitForSingleObject(
            process,
            timeout);
    const DWORD error = wait == WAIT_FAILED ? GetLastError() : ERROR_TIMEOUT;
    CloseHandle(process);
    if (wait == WAIT_OBJECT_0) return true;
    SetLastError(error);
    return false;
}

constexpr wchar_t kRecoveryMarker[] = L".asterun-uninstall-recovery";
constexpr char kRecoverySignature[] = "Asterun uninstall recovery v1";

[[nodiscard]] bool IsPlainFile(const std::filesystem::path& path) {
    const DWORD attributes = GetFileAttributesW(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES &&
        !(attributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT));
}

[[nodiscard]] bool HasRecoveryMarker(const std::filesystem::path& install) {
    if (!IsPlainFile(install / kRecoveryMarker) || !IsPlainFile(install / L"Uninstall.exe")) return false;
    std::ifstream input(install / kRecoveryMarker);
    std::string signature;
    std::getline(input, signature);
    return signature == kRecoverySignature;
}

[[nodiscard]] bool WriteRecoveryMarker(const std::filesystem::path& install) {
    const auto marker = install / kRecoveryMarker;
    const DWORD attributes = GetFileAttributesW(marker.c_str());
    if (attributes != INVALID_FILE_ATTRIBUTES &&
        (attributes & (FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DIRECTORY))) return false;
    std::ofstream out(marker, std::ios::trunc);
    out << kRecoverySignature << '\n';
    out.flush();
    return out.good();
}

// Keep a usable retry entry if a lock/ACL blocks cleanup after the EXE was removed.
void RestoreUninstallEntry(const std::filesystem::path& install) {
    const DWORD error = GetLastError();
    const auto destination = install / L"Uninstall.exe";
    const auto current = CurrentExecutable();
    const DWORD attributes = GetFileAttributesW(destination.c_str());
    if (!current.empty() && attributes == INVALID_FILE_ATTRIBUTES)
        (void)CopyFileW(current.c_str(), destination.c_str(), TRUE);
    (void)WriteRecoveryMarker(install);
    SetLastError(error);
}

[[nodiscard]] bool
ValidateInstallRoot(
    const std::filesystem::path& install) {
    std::error_code ec;

    if (install.empty() ||
        !install.is_absolute() ||
        install == install.root_path() ||
        !std::filesystem::is_directory(
            install,
            ec) ||
        ec) {
        return false;
    }

    const DWORD rootAttributes = GetFileAttributesW(install.c_str());
    if (rootAttributes == INVALID_FILE_ATTRIBUTES ||
        (rootAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) return false;
    if (HasRecoveryMarker(install)) return true;

    for (const auto* name : {
             L"Asterun.exe",
             L"VERSION",
         }) {
        ec.clear();

        if (!IsPlainFile(install / name)) {
            return false;
        }
    }

    return true;
}

[[nodiscard]] bool
ParsePerformArguments(
    PerformArguments& result) {
    int argc = 0;
    LPWSTR* argv =
        CommandLineToArgvW(
            GetCommandLineW(),
            &argc);

    if (!argv) {
        return false;
    }

    bool perform = false;
    bool deleteDataSeen = false;

    for (int i = 1;
         i < argc;
         ++i) {
        const std::wstring_view key(
            argv[i]);

        if (key == L"--perform") {
            perform = true;
            continue;
        }

        if (i + 1 >= argc) {
            LocalFree(argv);
            return false;
        }

        const std::wstring_view value(
            argv[++i]);

        if (key == L"--parent-pid") {
            wchar_t* end = nullptr;
            const unsigned long pid =
                std::wcstoul(
                    value.data(),
                    &end,
                    10);

            if (!end ||
                *end != L'\0' ||
                pid == 0) {
                LocalFree(argv);
                return false;
            }

            result.parentPid =
                static_cast<DWORD>(
                    pid);
        } else if (
            key == L"--install") {
            result.install =
                std::wstring(value);
        } else if (
            key == L"--delete-data") {
            if (value == L"1") {
                result.deleteData = true;
            } else if (
                value == L"0") {
                result.deleteData = false;
            } else {
                LocalFree(argv);
                return false;
            }

            deleteDataSeen = true;
        } else if (
            key == L"--shell-release-request") {
            result.shellReleaseRequest =
                std::wstring(value);
        } else if (
            key == L"--shell-release-done") {
            result.shellReleaseDone =
                std::wstring(value);
        } else if (
            key == L"--shell-lease-acquired") {
            result.shellLeaseAcquired =
                std::wstring(value);
        } else {
            LocalFree(argv);
            return false;
        }
    }

    LocalFree(argv);

    const bool hasReleaseRequest =
        !result.shellReleaseRequest.empty();
    const bool hasReleaseDone =
        !result.shellReleaseDone.empty();
    const bool hasLeaseAcquired =
        !result.shellLeaseAcquired.empty();

    return perform &&
        result.parentPid != 0 &&
        !result.install.empty() &&
        deleteDataSeen &&
        hasReleaseRequest ==
            hasReleaseDone &&
        hasReleaseDone ==
            hasLeaseAcquired &&
        (!result.deleteData ||
         (hasReleaseRequest &&
          hasReleaseDone &&
          hasLeaseAcquired));
}

void RemoveStartupRegistration(
    const std::filesystem::path& install) {
    constexpr wchar_t keyPath[] =
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
    constexpr wchar_t valueName[] =
        L"Asterun";

    HKEY key = nullptr;

    if (RegOpenKeyExW(
            HKEY_CURRENT_USER,
            keyPath,
            0,
            KEY_QUERY_VALUE |
                KEY_SET_VALUE,
            &key) != ERROR_SUCCESS) {
        return;
    }

    DWORD type = 0;
    DWORD bytes = 0;

    if (RegQueryValueExW(
            key,
            valueName,
            nullptr,
            &type,
            nullptr,
            &bytes) == ERROR_SUCCESS &&
        bytes > sizeof(wchar_t) &&
        (type == REG_SZ ||
         type == REG_EXPAND_SZ)) {
        std::vector<wchar_t>
            buffer(
                bytes /
                    sizeof(wchar_t) +
                1,
                L'\0');

        if (RegQueryValueExW(
                key,
                valueName,
                nullptr,
                &type,
                reinterpret_cast<BYTE*>(
                    buffer.data()),
                &bytes) ==
            ERROR_SUCCESS) {
            std::filesystem::path
                registered =
                    ExpandExecutable(
                        ExtractExecutable(
                            buffer.data()));

            if (!registered.empty() &&
                LowerPath(registered) ==
                    LowerPath(
                        install /
                        L"Asterun.exe")) {
                RegDeleteValueW(
                    key,
                    valueName);
            }
        }
    }

    RegCloseKey(key);
}

[[nodiscard]] bool StopOwnedProcess(DWORD pid, const std::filesystem::path& expected) {
    EventHandle process;
    process.value = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!process.value) return GetLastError() == ERROR_INVALID_PARAMETER;
    if (WaitForSingleObject(process.value, 0) == WAIT_OBJECT_0) return true;
    std::array<wchar_t, 32768> buffer{};
    DWORD size = static_cast<DWORD>(buffer.size());
    if (!QueryFullProcessImageNameW(process.value, 0, buffer.data(), &size))
        return WaitForSingleObject(process.value, 0) == WAIT_OBJECT_0;
    // Revalidate the opened handle, not just a potentially recycled snapshot PID.
    if (LowerPath(std::filesystem::path(std::wstring(buffer.data(), size))) != LowerPath(expected)) return true;
    if (!TerminateProcess(process.value, ERROR_CANCELLED) &&
        WaitForSingleObject(process.value, 0) != WAIT_OBJECT_0) return false;
    const DWORD waited = WaitForSingleObject(process.value, 5000);
    if (waited == WAIT_OBJECT_0) return true;
    SetLastError(waited == WAIT_TIMEOUT ? ERROR_TIMEOUT : ERROR_GEN_FAILURE);
    return false;
}

[[nodiscard]] bool
GracefullyCloseAsterun(
    const std::filesystem::path& install) {
    const auto expected =
        install /
        L"Asterun.exe";

    HWND hwnd =
        FindWindowW(
            kLauncherClass,
            kLauncherTitle);

    if (hwnd) {
        DWORD pid = 0;
        GetWindowThreadProcessId(
            hwnd,
            &pid);

        std::filesystem::path actual;

        if (pid != 0 &&
            ProcessPath(
                pid,
                actual) &&
            LowerPath(actual) ==
                LowerPath(expected)) {
            DWORD_PTR ignored = 0;

            SendMessageTimeoutW(
                hwnd,
                WM_CLOSE,
                0,
                0,
                SMTO_ABORTIFHUNG |
                    SMTO_BLOCK,
                5000,
                &ignored);

            if (WaitForProcess(
                    pid,
                    15000)) {
                return true;
            }
        }
    }

    HANDLE snapshot =
        CreateToolhelp32Snapshot(
            TH32CS_SNAPPROCESS,
            0);

    if (snapshot ==
        INVALID_HANDLE_VALUE) {
        return false;
    }

    PROCESSENTRY32W entry{};
    entry.dwSize =
        sizeof(entry);

    BOOL more =
        Process32FirstW(
            snapshot,
            &entry);

    bool success = true;

    while (more) {
        std::filesystem::path actual;

        if (ProcessPath(
                entry.th32ProcessID,
                actual) &&
            LowerPath(actual) ==
                LowerPath(expected)) {
            if (!StopOwnedProcess(entry.th32ProcessID, actual)) success = false;
        }

        more =
            Process32NextW(
                snapshot,
                &entry);
    }

    CloseHandle(snapshot);
    return success;
}

[[nodiscard]] bool
QueryEverythingServiceExecutable(
    SC_HANDLE service,
    std::filesystem::path& executable) {
    DWORD required = 0;

    QueryServiceConfigW(
        service,
        nullptr,
        0,
        &required);

    if (required == 0 ||
        GetLastError() !=
            ERROR_INSUFFICIENT_BUFFER) {
        return false;
    }

    std::vector<std::max_align_t>
        storage(
            (required +
             sizeof(std::max_align_t) - 1) /
            sizeof(std::max_align_t));

    auto* config =
        reinterpret_cast<
            QUERY_SERVICE_CONFIGW*>(
                storage.data());

    if (!QueryServiceConfigW(
            service,
            config,
            required,
            &required) ||
        !config->lpBinaryPathName) {
        return false;
    }

    executable =
        ExpandExecutable(
            ExtractExecutable(
                config->
                    lpBinaryPathName));

    return !executable.empty();
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

struct ServiceCleanupResult {
    bool success{true};
    bool removed{false};
    bool protectedServiceHost{false};
    DWORD error{ERROR_SUCCESS};
};

[[nodiscard]] ServiceCleanupResult
StopAndDeleteOwnedEverythingService(
    const std::filesystem::path& install) {
    ServiceCleanupResult result;

    ServiceHandle manager;
    manager.value =
        OpenSCManagerW(
            nullptr,
            nullptr,
            SC_MANAGER_CONNECT);

    if (!manager.value) {
        result.success = false;
        result.error =
            GetLastError();
        return result;
    }

    ServiceHandle service;
    service.value =
        OpenServiceW(
            manager.value,
            kEverythingService,
            SERVICE_QUERY_CONFIG);

    if (!service.value) {
        const DWORD error =
            GetLastError();

        if (error == ERROR_SERVICE_DOES_NOT_EXIST || error == ERROR_SERVICE_MARKED_FOR_DELETE) {
            return result;
        }

        result.success = false;
        result.error = error;
        return result;
    }

    std::filesystem::path
        serviceExecutable;

    if (!QueryEverythingServiceExecutable(
            service.value,
            serviceExecutable)) {
        result.success = false;
        result.error =
            GetLastError() != ERROR_SUCCESS
                ? GetLastError()
                : ERROR_INVALID_DATA;
        return result;
    }

    const auto managedRoot =
        install /
        L"data" /
        L"tools" /
        L"Everything";
    const auto protectedRoot =
        ManagedEverythingServiceHostRoot();

    const bool ownedPortable =
        PathStartsWithDirectory(
            serviceExecutable,
            managedRoot);
    const bool ownedProtected =
        !protectedRoot.empty() &&
        PathStartsWithDirectory(
            serviceExecutable,
            protectedRoot);

    if (!ownedPortable &&
        !ownedProtected) {
        return result;
    }

    result.protectedServiceHost = ownedProtected;
    ServiceHandle controlled;
    controlled.value = OpenServiceW(manager.value, kEverythingService,
        SERVICE_QUERY_CONFIG | SERVICE_QUERY_STATUS | SERVICE_STOP | DELETE);
    if (!controlled.value) {
        const DWORD error = GetLastError();
        if (error == ERROR_SERVICE_MARKED_FOR_DELETE || error == ERROR_SERVICE_DOES_NOT_EXIST) return result;
        result.success = false;
        result.error = error;
        return result;
    }
    std::filesystem::path controlledExecutable;
    if (!QueryEverythingServiceExecutable(controlled.value, controlledExecutable) ||
        LowerPath(controlledExecutable) != LowerPath(serviceExecutable)) {
        result.success = false;
        result.error = ERROR_RETRY;
        return result;
    }
    std::swap(service.value, controlled.value);

    SERVICE_STATUS_PROCESS status{};
    DWORD bytes = 0;

    if (!QueryServiceStatusEx(
            service.value,
            SC_STATUS_PROCESS_INFO,
            reinterpret_cast<LPBYTE>(
                &status),
            sizeof(status),
            &bytes)) {
        result.success = false;
        result.error =
            GetLastError();
        return result;
    }

    const DWORD servicePid =
        status.dwProcessId;

    if (status.dwCurrentState !=
        SERVICE_STOPPED) {
        SERVICE_STATUS stopStatus{};

        if (status.dwCurrentState != SERVICE_STOP_PENDING &&
            !ControlService(service.value, SERVICE_CONTROL_STOP, &stopStatus)) {
            const DWORD error =
                GetLastError();

            if (error !=
                ERROR_SERVICE_NOT_ACTIVE) {
                result.success = false;
                result.error = error;
                return result;
            }
        }

        const auto deadline =
            std::chrono::steady_clock::now() +
            std::chrono::seconds(20);

        do {
            std::this_thread::sleep_for(
                std::chrono::milliseconds(
                    150));

            if (!QueryServiceStatusEx(
                    service.value,
                    SC_STATUS_PROCESS_INFO,
                    reinterpret_cast<LPBYTE>(
                        &status),
                    sizeof(status),
                    &bytes)) {
                result.success = false;
                result.error =
                    GetLastError();
                return result;
            }

            if (status.dwCurrentState ==
                SERVICE_STOPPED) {
                break;
            }
        } while (
            std::chrono::steady_clock::now() <
            deadline);

        if (status.dwCurrentState !=
            SERVICE_STOPPED) {
            result.success = false;
            result.error =
                ERROR_SERVICE_REQUEST_TIMEOUT;
            return result;
        }
    }

    // SERVICE_STOPPED can be observed slightly before the hosting process
    // has fully torn down. Revalidate the captured PID against the exact
    // owned service image and wait/terminate it before deleting the service
    // entry or protected host files.
    if (servicePid != 0 &&
        !StopOwnedProcess(
            servicePid,
            serviceExecutable)) {
        result.success = false;
        result.error =
            GetLastError() != ERROR_SUCCESS
                ? GetLastError()
                : ERROR_TIMEOUT;
        return result;
    }

    if (!DeleteService(
            service.value)) {
        const DWORD error =
            GetLastError();

        if (error !=
            ERROR_SERVICE_MARKED_FOR_DELETE) {
            result.success = false;
            result.error = error;
            return result;
        }
    }

    result.removed = true;
    return result;
}

[[nodiscard]] bool
IsOwnedEverythingProcessPath(
    const std::filesystem::path& install,
    const std::filesystem::path& actual,
    bool includeProtectedServiceHost) {
    if (LowerPath(
            actual.filename()) !=
        L"everything.exe") {
        return false;
    }

    const auto portableRoot =
        install /
        L"data" /
        L"tools" /
        L"Everything";

    if (PathStartsWithDirectory(
            actual,
            portableRoot)) {
        return true;
    }

    if (!includeProtectedServiceHost) {
        return false;
    }

    const auto protectedRoot =
        ManagedEverythingServiceHostRoot();

    return !protectedRoot.empty() &&
        PathStartsWithDirectory(
            actual,
            protectedRoot);
}

[[nodiscard]] bool
TerminateManagedEverythingProcesses(
    const std::filesystem::path& install,
    bool includeProtectedServiceHost = false) {
    // A single Toolhelp snapshot is not a sufficient uninstall barrier:
    // Everything may still be completing service/client shutdown while the
    // snapshot is being walked. Repeat a few bounded passes and only touch
    // executables whose resolved image path is inside Asterun-owned roots.
    for (int pass = 0;
         pass < 4;
         ++pass) {
        HANDLE snapshot =
            CreateToolhelp32Snapshot(
                TH32CS_SNAPPROCESS,
                0);

        if (snapshot ==
            INVALID_HANDLE_VALUE) {
            return false;
        }

        PROCESSENTRY32W entry{};
        entry.dwSize =
            sizeof(entry);
        BOOL more =
            Process32FirstW(
                snapshot,
                &entry);

        bool foundOwned = false;
        bool success = true;

        while (more) {
            std::filesystem::path actual;

            if (ProcessPath(
                    entry.th32ProcessID,
                    actual) &&
                IsOwnedEverythingProcessPath(
                    install,
                    actual,
                    includeProtectedServiceHost)) {
                foundOwned = true;

                if (!StopOwnedProcess(
                        entry.th32ProcessID,
                        actual)) {
                    success = false;
                }
            }

            more =
                Process32NextW(
                    snapshot,
                    &entry);
        }

        CloseHandle(snapshot);

        if (!success) {
            return false;
        }

        if (!foundOwned) {
            return true;
        }

        std::this_thread::sleep_for(
            std::chrono::milliseconds(
                150));
    }

    // Final verification: never show uninstall success while an Asterun-owned
    // Everything image is still alive.
    HANDLE snapshot =
        CreateToolhelp32Snapshot(
            TH32CS_SNAPPROCESS,
            0);

    if (snapshot ==
        INVALID_HANDLE_VALUE) {
        return false;
    }

    PROCESSENTRY32W entry{};
    entry.dwSize =
        sizeof(entry);
    BOOL more =
        Process32FirstW(
            snapshot,
            &entry);
    bool remaining = false;

    while (more) {
        std::filesystem::path actual;

        if (ProcessPath(
                entry.th32ProcessID,
                actual) &&
            IsOwnedEverythingProcessPath(
                install,
                actual,
                includeProtectedServiceHost)) {
            remaining = true;
            break;
        }

        more =
            Process32NextW(
                snapshot,
                &entry);
    }

    CloseHandle(snapshot);

    if (remaining) {
        SetLastError(
            ERROR_BUSY);
        return false;
    }

    return true;
}

[[nodiscard]] bool
CleanupManagedEverythingServiceHostFiles(
    RemovalFailure& failure) {
    const auto root =
        ManagedEverythingServiceHostRoot();

    if (root.empty()) {
        failure = {
            ERROR_PATH_NOT_FOUND,
            {},
            {}};
        return false;
    }

    if (!RemoveAllWithRetry(
            root,
            failure)) {
        return false;
    }

    const auto altrun =
        root.parent_path();
    const auto vendor =
        altrun.parent_path();

    std::error_code ec;
    std::filesystem::remove(
        altrun,
        ec);
    ec.clear();
    std::filesystem::remove(
        vendor,
        ec);

    return true;
}

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

[[nodiscard]] bool
RemoveOwnedSendToShortcutAt(
    const std::filesystem::path& shortcut,
    const std::filesystem::path& install) {
    const DWORD attributes =
        GetFileAttributesW(
            shortcut.c_str());

    if (attributes ==
        INVALID_FILE_ATTRIBUTES) {
        const DWORD error =
            GetLastError();
        return error ==
                   ERROR_FILE_NOT_FOUND ||
            error ==
                   ERROR_PATH_NOT_FOUND;
    }

    // Never follow a directory or reparse point while cleaning a Shell
    // integration path. Asterun creates a regular .lnk file here.
    if (attributes &
        (FILE_ATTRIBUTE_DIRECTORY |
         FILE_ATTRIBUTE_REPARSE_POINT)) {
        return true;
    }

    ComApartment apartment;

    if (!apartment.Ready()) {
        return false;
    }

    IShellLinkW* shellLink =
        nullptr;

    if (FAILED(
            CoCreateInstance(
                CLSID_ShellLink,
                nullptr,
                CLSCTX_INPROC_SERVER,
                IID_PPV_ARGS(
                    &shellLink))) ||
        !shellLink) {
        return false;
    }

    IPersistFile* persist =
        nullptr;

    if (FAILED(
            shellLink->QueryInterface(
                IID_PPV_ARGS(
                    &persist))) ||
        !persist) {
        shellLink->Release();
        return false;
    }

    bool owned = false;

    if (SUCCEEDED(
            persist->Load(
                shortcut.c_str(),
                STGM_READ))) {
        std::array<wchar_t, 32768>
            target{};
        std::array<wchar_t, 32768>
            arguments{};

        const bool targetOk =
            SUCCEEDED(
                shellLink->GetPath(
                    target.data(),
                    static_cast<int>(
                        target.size()),
                    nullptr,
                    SLGP_RAWPATH)) &&
            target.front() != L'\0';
        const bool argumentsOk =
            SUCCEEDED(
                shellLink->GetArguments(
                    arguments.data(),
                    static_cast<int>(
                        arguments.size())));

        // The stable identity of Asterun's SendTo entry is the target
        // executable plus its dedicated --add-shortcut action. Working
        // directory and icon metadata can be normalized by the Shell and are
        // not ownership signals.
        owned =
            targetOk &&
            argumentsOk &&
            LowerPath(
                std::filesystem::path(
                    target.data())) ==
                LowerPath(
                    install /
                    L"Asterun.exe") &&
            std::wstring_view(
                arguments.data()) ==
                L"--add-shortcut";
    }

    persist->Release();
    shellLink->Release();

    if (!owned) {
        return true;
    }

    if (DeleteFileW(
            shortcut.c_str())) {
        return true;
    }

    const DWORD error =
        GetLastError();
    return error ==
               ERROR_FILE_NOT_FOUND ||
        error ==
               ERROR_PATH_NOT_FOUND;
}

void RemoveSendToRegistration(
    const std::filesystem::path& install) {
    PWSTR sendToRaw =
        nullptr;

    const HRESULT result =
        SHGetKnownFolderPath(
            FOLDERID_SendTo,
            KF_FLAG_DEFAULT,
            nullptr,
            &sendToRaw);

    if (FAILED(result) ||
        !sendToRaw ||
        !*sendToRaw) {
        if (sendToRaw) {
            CoTaskMemFree(
                sendToRaw);
        }
        return;
    }

    std::filesystem::path shortcut(
        sendToRaw);
    CoTaskMemFree(
        sendToRaw);
    shortcut /=
        L"Asterun.lnk";

    (void)
        RemoveOwnedSendToShortcutAt(
            shortcut,
            install);
}

[[nodiscard]] bool
FileUrlToPath(
    BSTR url,
    std::filesystem::path& path) {
    if (!url || !*url) {
        return false;
    }

    std::array<wchar_t, 32768>
        buffer{};
    DWORD length =
        static_cast<DWORD>(
            buffer.size());

    if (FAILED(
            PathCreateFromUrlW(
                url,
                buffer.data(),
                &length,
                0)) ||
        length == 0) {
        return false;
    }

    path =
        std::filesystem::path(
            std::wstring(
                buffer.data(),
                length));
    return true;
}

[[nodiscard]] std::filesystem::path
ExplorerParkingDirectory(
    const std::filesystem::path& install) {
    const auto parent =
        install.parent_path();
    const auto grandparent =
        parent.parent_path();

    // Parking directly in the immediate parent can cause Explorer to
    // immediately enumerate/select the Asterun folder we are about to delete.
    // Prefer one level farther out so the installation root is not a visible
    // child of the active Shell view.
    if (!grandparent.empty() &&
        LowerPath(
            grandparent) !=
            LowerPath(parent) &&
        !PathStartsWithDirectory(
            grandparent,
            install)) {
        return grandparent;
    }

    std::array<wchar_t, 32768>
        temp{};
    const DWORD length =
        GetTempPathW(
            static_cast<DWORD>(
                temp.size()),
            temp.data());

    if (length > 0 &&
        length < temp.size()) {
        return std::filesystem::path(
            temp.data());
    }

    return parent;
}

[[nodiscard]] int
NavigateExplorerAwayFromInstall(
    const std::filesystem::path& install) {
    const auto parking =
        ExplorerParkingDirectory(
            install);

    if (parking.empty()) {
        return 0;
    }

    ComApartment apartment;

    if (!apartment.Ready()) {
        return 0;
    }

    IShellWindows* windows =
        nullptr;

    if (FAILED(
            CoCreateInstance(
                CLSID_ShellWindows,
                nullptr,
                CLSCTX_LOCAL_SERVER,
                IID_PPV_ARGS(
                    &windows))) ||
        !windows) {
        return 0;
    }

    long count = 0;
    (void)windows->get_Count(
        &count);

    int navigated = 0;

    for (long i = 0;
         i < count;
         ++i) {
        VARIANT index;
        VariantInit(&index);
        index.vt = VT_I4;
        index.lVal = i;

        IDispatch* dispatch =
            nullptr;

        if (FAILED(
                windows->Item(
                    index,
                    &dispatch)) ||
            !dispatch) {
            continue;
        }

        IWebBrowser2* browser =
            nullptr;
        const HRESULT query =
            dispatch->QueryInterface(
                IID_PPV_ARGS(
                    &browser));
        dispatch->Release();

        if (FAILED(query) ||
            !browser) {
            continue;
        }

        BSTR locationUrl =
            nullptr;
        std::filesystem::path
            location;

        const HRESULT locationResult =
            browser->get_LocationURL(
                &locationUrl);

        const bool insideInstall =
            SUCCEEDED(locationResult) &&
            FileUrlToPath(
                locationUrl,
                location) &&
            PathStartsWithDirectory(
                location,
                install);

        if (locationUrl) {
            SysFreeString(
                locationUrl);
        }

        if (insideInstall) {
            BSTR target =
                SysAllocString(
                    parking.c_str());

            if (target) {
                VARIANT empty;
                VariantInit(&empty);

                if (SUCCEEDED(
                        browser->Navigate(
                            target,
                            &empty,
                            &empty,
                            &empty,
                            &empty))) {
                    ++navigated;
                }

                SysFreeString(
                    target);
            }
        }

        browser->Release();
    }

    windows->Release();

    if (navigated > 0) {
        // Explorer navigation is asynchronous. The elevated worker will not
        // delete anything until it independently acquires the installation
        // root DELETE lease.
        std::this_thread::sleep_for(
            std::chrono::milliseconds(
                350));
    }

    return navigated;
}

[[nodiscard]] bool
AcquireDirectoryDeleteLease(
    const std::filesystem::path& path,
    DirectoryHandle& lease,
    RemovalFailure& failure,
    DWORD timeoutMs);

[[nodiscard]] bool
DeleteDirectoryThroughLease(
    DirectoryHandle& lease,
    const std::filesystem::path& path,
    RemovalFailure& failure);

[[nodiscard]] bool
RequestShellRelease(
    const PerformArguments& args,
    DirectoryHandle& rootLease,
    RemovalFailure& failure) {
    if (!args.deleteData) {
        return true;
    }

    EventHandle request;
    request.value =
        OpenEventW(
            EVENT_MODIFY_STATE,
            FALSE,
            args.shellReleaseRequest.c_str());

    EventHandle done;
    done.value =
        OpenEventW(
            SYNCHRONIZE,
            FALSE,
            args.shellReleaseDone.c_str());

    EventHandle leaseAcquired;
    leaseAcquired.value =
        OpenEventW(
            EVENT_MODIFY_STATE,
            FALSE,
            args.shellLeaseAcquired.c_str());

    if (!request.value ||
        !done.value ||
        !leaseAcquired.value) {
        const DWORD error =
            GetLastError();
        failure.error =
            error != ERROR_SUCCESS
                ? error
                : ERROR_INVALID_HANDLE;
        failure.path =
            args.install;
        SetLastError(
            failure.error);
        return false;
    }

    if (!SetEvent(
            request.value)) {
        failure.error =
            GetLastError();
        failure.path =
            args.install;
        return false;
    }

    const DWORD released =
        WaitForSingleObject(
            done.value,
            20000);

    if (released !=
        WAIT_OBJECT_0) {
        failure.error =
            released ==
                    WAIT_TIMEOUT
                ? ERROR_TIMEOUT
                : ERROR_GEN_FAILURE;
        failure.path =
            args.install;
        SetLastError(
            failure.error);
        return false;
    }

    // Let the original EXE exit before opening the DELETE lease. Its image,
    // working-directory and shell handles must not participate in this wait.
    if (!SetEvent(
            leaseAcquired.value)) {
        failure.error =
            GetLastError();
        failure.path =
            args.install;
        rootLease.Reset();
        return false;
    }

    if (!WaitForProcess(
            args.parentPid,
            15000)) {
        failure.error =
            ERROR_TIMEOUT;
        failure.path =
            args.install;
        rootLease.Reset();
        SetLastError(
            failure.error);
        return false;
    }

    return AcquireDirectoryDeleteLease(args.install, rootLease, failure, 8000);
}

[[nodiscard]] bool
ServeShellReleaseBroker(
    HANDLE requestEvent,
    HANDLE workerProcess,
    const std::filesystem::path& install,
    HANDLE doneEvent,
    HANDLE leaseAcquiredEvent) {
    const HANDLE handles[] = {
        requestEvent,
        workerProcess,
    };

    const DWORD wait =
        WaitForMultipleObjects(
            2,
            handles,
            FALSE,
            INFINITE); // The worker handle is the lifetime/failed-start signal.

    if (wait ==
        WAIT_OBJECT_0) {
        (void)NavigateExplorerAwayFromInstall(
            install);

        const auto parking =
            ExplorerParkingDirectory(
                install);

        if (!parking.empty()) {
            (void)SetCurrentDirectoryW(
                parking.c_str());
        }

        // Do not try to acquire DELETE access from the broker here. Parking in
        // the immediate parent previously made Explorer enumerate/select the
        // Asterun folder and turned the broker's lease attempt into a frequent
        // self-inflicted sharing timeout. Signal release immediately; the
        // elevated worker acknowledges release, waits for this broker to exit,
        // then acquires the lease before any destructive cleanup.
        if (!SetEvent(
                doneEvent)) {
            return false;
        }

        const HANDLE handoffHandles[] = {
            leaseAcquiredEvent,
            workerProcess,
        };

        const DWORD handoff =
            WaitForMultipleObjects(
                2,
                handoffHandles,
                FALSE,
                20000);

        if (handoff ==
                WAIT_OBJECT_0 ||
            handoff ==
                WAIT_OBJECT_0 + 1) {
            return true;
        }

        return false;
    }

    if (wait ==
        WAIT_OBJECT_0 + 1) {
        return true;
    }

    return false;
}

[[nodiscard]] std::wstring
RestartManagerLockOwners(
    const std::filesystem::path& path) {
    DWORD session = 0;
    WCHAR key[
        CCH_RM_SESSION_KEY + 1]{};

    if (RmStartSession(
            &session,
            0,
            key) != ERROR_SUCCESS) {
        return {};
    }

    const auto finish =
        [&]() {
            RmEndSession(
                session);
        };

    LPCWSTR resources[] = {
        path.c_str(),
    };

    if (RmRegisterResources(
            session,
            1,
            resources,
            0,
            nullptr,
            0,
            nullptr) !=
        ERROR_SUCCESS) {
        finish();
        return {};
    }

    UINT needed = 0;
    UINT count = 0;
    DWORD rebootReasons = 0;

    DWORD result =
        RmGetList(
            session,
            &needed,
            &count,
            nullptr,
            &rebootReasons);

    if (result !=
            ERROR_MORE_DATA ||
        needed == 0) {
        finish();
        return {};
    }

    std::vector<RM_PROCESS_INFO>
        processes(needed);
    count = needed;

    result =
        RmGetList(
            session,
            &needed,
            &count,
            processes.data(),
            &rebootReasons);

    finish();

    if (result != ERROR_SUCCESS ||
        count == 0) {
        return {};
    }

    std::wstring owners;

    for (UINT i = 0;
         i < count;
         ++i) {
        if (!owners.empty()) {
            owners += L", ";
        }

        const auto& process =
            processes[i];

        if (process.strAppName[0] !=
            L'\0') {
            owners +=
                process.strAppName;
        } else {
            owners +=
                L"PID";
        }

        owners += L" (PID ";
        owners +=
            std::to_wstring(
                process.Process
                    .dwProcessId);
        owners += L")";
    }

    return owners;
}

[[nodiscard]] DWORD
NativeFilesystemError(
    const std::error_code& error) {
    if (!error) {
        return ERROR_SUCCESS;
    }

    const int value =
        error.value();

    return value > 0
        ? static_cast<DWORD>(value)
        : ERROR_GEN_FAILURE;
}

[[nodiscard]] bool
IsTransientRemovalError(
    DWORD error) {
    return error ==
            ERROR_SHARING_VIOLATION ||
        error ==
            ERROR_LOCK_VIOLATION ||
        error ==
            ERROR_ACCESS_DENIED ||
        error ==
            ERROR_DIR_NOT_EMPTY ||
        error ==
            ERROR_BUSY;
}

[[nodiscard]] bool
AcquireDirectoryDeleteLease(
    const std::filesystem::path& path,
    DirectoryHandle& lease,
    RemovalFailure& failure,
    DWORD timeoutMs) {
    lease.Reset();

    constexpr auto delay =
        std::chrono::milliseconds(100);
    const auto started =
        std::chrono::steady_clock::now();

    for (;;) {
        HANDLE handle =
            CreateFileW(
                path.c_str(),
                DELETE |
                    FILE_READ_ATTRIBUTES |
                    FILE_WRITE_ATTRIBUTES |
                    SYNCHRONIZE,
                FILE_SHARE_READ |
                    FILE_SHARE_WRITE |
                    FILE_SHARE_DELETE,
                nullptr,
                OPEN_EXISTING,
                FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT,
                nullptr);

        if (handle !=
            INVALID_HANDLE_VALUE) {
            lease.value =
                handle;
            return true;
        }

        const DWORD error =
            GetLastError();

        if (!IsTransientRemovalError(
                error)) {
            failure.error =
                error != ERROR_SUCCESS
                    ? error
                    : ERROR_GEN_FAILURE;
            failure.path = path;
            failure.lockOwners =
                RestartManagerLockOwners(
                    path);
            return false;
        }

        const auto elapsed =
            std::chrono::duration_cast<
                std::chrono::milliseconds>(
                std::chrono::
                    steady_clock::now() -
                started);

        if (elapsed.count() >=
            static_cast<long long>(
                timeoutMs)) {
            failure.error =
                error != ERROR_SUCCESS
                    ? error
                    : ERROR_SHARING_VIOLATION;
            failure.path = path;
            failure.lockOwners =
                RestartManagerLockOwners(
                    path);
            return false;
        }

        std::this_thread::sleep_for(
            delay);
    }
}

[[nodiscard]] bool
DeleteDirectoryThroughLease(
    DirectoryHandle& lease,
    const std::filesystem::path& path,
    RemovalFailure& failure) {
    if (!lease.Valid()) {
        failure.error =
            ERROR_INVALID_HANDLE;
        failure.path = path;
        return false;
    }

    // Unlike RemoveOneWithRetry, this path deletes through a handle. A
    // read-only installation root must be made writable through that same
    // validated handle, without following a substituted path/reparse target.
    FILE_BASIC_INFO basic{};
    if (!GetFileInformationByHandleEx(lease.value, FileBasicInfo, &basic, sizeof(basic))) {
        failure = {GetLastError(), path, {}};
        return false;
    }
    if (basic.FileAttributes & FILE_ATTRIBUTE_READONLY) {
        basic.FileAttributes &= ~FILE_ATTRIBUTE_READONLY;
        if (!SetFileInformationByHandle(lease.value, FileBasicInfo, &basic, sizeof(basic))) {
            failure = {GetLastError(), path, {}};
            return false;
        }
    }

    FILE_DISPOSITION_INFO disposition{};
    disposition.DeleteFile =
        TRUE;

    for (int attempt = 0; ; ++attempt) {
        if (SetFileInformationByHandle(lease.value, FileDispositionInfo, &disposition, sizeof(disposition))) break;
        const DWORD error = GetLastError();
        if (!IsTransientRemovalError(error) || attempt >= 24) {
            failure = {error != ERROR_SUCCESS ? error : ERROR_GEN_FAILURE, path,
                RestartManagerLockOwners(path)};
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    lease.Reset();
    return true;
}

[[nodiscard]] bool
RemoveOneWithRetry(
    const std::filesystem::path& path,
    RemovalFailure& failure) {
    constexpr int attempts = 25;
    constexpr auto delay =
        std::chrono::milliseconds(200);

    for (int attempt = 0; attempt < attempts; ++attempt) {
        const DWORD attributes = GetFileAttributesW(path.c_str());
        if (attributes == INVALID_FILE_ATTRIBUTES) {
            const DWORD error = GetLastError();
            if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND) return true;
            failure.error = error;
            failure.path = path;
            return false;
        }
        // Do not change attributes on a reparse target outside this installation.
        if ((attributes & FILE_ATTRIBUTE_READONLY) && !(attributes & FILE_ATTRIBUTE_REPARSE_POINT))
            (void)SetFileAttributesW(path.c_str(), attributes & ~FILE_ATTRIBUTE_READONLY);
        const BOOL removed = (attributes & FILE_ATTRIBUTE_DIRECTORY)
            ? RemoveDirectoryW(path.c_str()) : DeleteFileW(path.c_str());
        if (removed) return true;
        const DWORD error = GetLastError();
        if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND) return true;

        if (!IsTransientRemovalError(
                error) ||
            attempt + 1 >= attempts) {
            failure.error =
                error != ERROR_SUCCESS
                    ? error
                    : ERROR_GEN_FAILURE;
            failure.path = path;

            std::error_code typeError;
            if (std::filesystem::
                    is_regular_file(
                        path,
                        typeError) &&
                !typeError) {
                failure.lockOwners =
                    RestartManagerLockOwners(
                        path);
            }

            return false;
        }

        std::this_thread::sleep_for(
            delay);
    }

    failure.error =
        ERROR_GEN_FAILURE;
    failure.path = path;
    return false;
}

[[nodiscard]] bool
RemoveAllWithRetry(
    const std::filesystem::path& path,
    RemovalFailure& failure) {
    const DWORD attributes = GetFileAttributesW(path.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        const DWORD error = GetLastError();
        if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND) return true;
        failure = {error, path, {}};
        return false;
    }
    // Junctions and all other directory reparse points are links, not subtrees.
    if (!(attributes & FILE_ATTRIBUTE_DIRECTORY) || (attributes & FILE_ATTRIBUTE_REPARSE_POINT))
        return RemoveOneWithRetry(path, failure);
    std::error_code ec;
    std::vector<std::filesystem::path> children;
    for (std::filesystem::directory_iterator it(path, ec), end; !ec && it != end; it.increment(ec))
        children.push_back(it->path());
    if (ec) {
        failure = {NativeFilesystemError(ec), path, {}};
        return false;
    }
    for (const auto& child : children)
        if (!RemoveAllWithRetry(child, failure)) return false;
    return RemoveOneWithRetry(path, failure);
}

[[nodiscard]] bool
RemoveInstallation(
    const std::filesystem::path& install,
    bool deleteData,
    RemovalFailure& failure,
    DirectoryHandle* rootLease) {
    if (!WriteRecoveryMarker(install)) {
        failure = {ERROR_ACCESS_DENIED, install / kRecoveryMarker, {}};
        return false;
    }
    bool completed = false;
    struct RecoveryGuard {
        const std::filesystem::path& install;
        bool& completed;
        ~RecoveryGuard() { if (!completed) RestoreUninstallEntry(install); }
    } recovery{install, completed};
    const auto data = install / L"data";
    const DWORD dataAttributes = GetFileAttributesW(data.c_str());
    const DWORD toolsAttributes = GetFileAttributesW((data / L"tools").c_str());
    const bool linkedData = dataAttributes != INVALID_FILE_ATTRIBUTES &&
        (dataAttributes & FILE_ATTRIBUTE_REPARSE_POINT);
    const bool linkedTools = toolsAttributes != INVALID_FILE_ATTRIBUTES &&
        (toolsAttributes & FILE_ATTRIBUTE_REPARSE_POINT);

    // A stopped service can race with final image/database handle release.
    // Antivirus/indexing can also briefly hold a freshly stopped Everything
    // file. Retry only normal transient Windows delete failures.
    if (!linkedData && !linkedTools && !RemoveAllWithRetry(
            data /
                L"tools" /
                L"Everything",
            failure)) {
        return false;
    }

    if (!linkedData && !RemoveAllWithRetry(
            data /
                L"update",
            failure)) {
        return false;
    }

    if (!linkedData && !linkedTools) {
        std::error_code ignored;
        std::filesystem::remove(
            data /
                L"tools",
            ignored);
    }

    std::error_code ec;
    std::vector<std::filesystem::path>
        entries;

    for (std::filesystem::
             directory_iterator
             it(install, ec),
         end;
         !ec && it != end;
         it.increment(ec)) {
        if (!deleteData &&
            LowerPath(
                it->path()
                    .filename()) ==
                L"data") {
            continue;
        }

        if (it->path().filename() != kRecoveryMarker) entries.push_back(it->path());
    }

    if (ec) {
        failure.error =
            NativeFilesystemError(ec);
        failure.path = install;
        return false;
    }

    // Delete retry/validation anchors last, independent of directory enumeration order.
    const auto anchor = [](const auto& path) {
        const auto name = LowerPath(path.filename());
        return name == L"asterun.exe" || name == L"uninstall.exe" || name == L"version";
    };
    std::stable_sort(entries.begin(), entries.end(), [&](const auto& a, const auto& b) {
        return anchor(a) < anchor(b);
    });
    for (const auto& entry : entries) {
        if (!RemoveAllWithRetry(
                entry,
                failure)) {
            return false;
        }
    }

    if (!RemoveOneWithRetry(install / kRecoveryMarker, failure)) return false;
    if (deleteData) {
        if (!rootLease) {
            failure.error =
                ERROR_INVALID_HANDLE;
            failure.path =
                install;
            return false;
        }

        completed = DeleteDirectoryThroughLease(*rootLease, install, failure);
        return completed;
    }
    completed = true;
    return true;
}

void CleanupSelfLater() {
    altrun::win::
        ScheduleTemporaryWorkerSelfCleanup();
}

[[nodiscard]] int
PerformUninstall(
    const PerformArguments& args) {
    gUninstallStage = ChineseUi() ? L"确认安装目录" : L"Validate installation";
    // Preserve-data mode can use the original simple parent-exit handshake.
    // Full-remove mode keeps the normal-integrity parent alive as an Explorer
    // broker until the elevated worker reaches the actual deletion phase.
    if (!args.deleteData && !WaitForProcess(args.parentPid, 30000)) {
        return 2;
    }
    if (!ValidateInstallRoot(args.install)) {
        SetLastError(ERROR_INVALID_DATA);
        return 2;
    }

    gUninstallStage = ChineseUi() ? L"退出 Asterun" : L"Close Asterun";
    if (!GracefullyCloseAsterun(
            args.install)) {
        return 3;
    }

    // Shut down the standard-user client before touching the service. This
    // prevents a live managed client from racing service teardown.
    gUninstallStage = ChineseUi() ? L"退出托管 Everything" : L"Close managed Everything";
    if (!TerminateManagedEverythingProcesses(
            args.install,
            false)) {
        return 5;
    }

    gUninstallStage = ChineseUi() ? L"停止并删除 Everything 服务" : L"Stop and remove Everything service";
    const auto service =
        StopAndDeleteOwnedEverythingService(
            args.install);

    if (!service.success) {
        SetLastError(
            service.error);
        return 4;
    }

    // SCM can report a stopped/deleted service before its process image has
    // fully disappeared. Verify both the portable client root and Asterun's
    // protected service-host root before deleting any files.
    gUninstallStage = ChineseUi() ? L"确认 Everything 已完全退出" : L"Verify Everything has exited";
    if (!TerminateManagedEverythingProcesses(
            args.install,
            true)) {
        return 5;
    }

    RemovalFailure removalFailure;

    // The Program Files host is Asterun-owned regardless of whether this
    // uninstall just removed the active protected service or is cleaning an
    // orphan left by an older portable-service migration. Failure here is
    // uninstall failure; do not silently claim that Everything was removed.
    gUninstallStage = ChineseUi() ? L"删除 Everything 服务宿主" : L"Remove Everything service host";
    if (!CleanupManagedEverythingServiceHostFiles(
            removalFailure)) {
        gRemovalFailurePath =
            removalFailure.path;
        gRemovalFailureLockOwners =
            removalFailure.lockOwners;
        SetLastError(
            removalFailure.error !=
                    ERROR_SUCCESS
                ? removalFailure.error
                : ERROR_GEN_FAILURE);
        return 8;
    }
    DirectoryHandle rootLease;

    gUninstallStage = ChineseUi() ? L"释放安装目录占用" : L"Release installation directory";
    if (args.deleteData &&
        !RequestShellRelease(
            args,
            rootLease,
            removalFailure)) {
        gRemovalFailurePath =
            removalFailure.path;
        gRemovalFailureLockOwners =
            removalFailure.lockOwners;
        SetLastError(
            removalFailure.error !=
                    ERROR_SUCCESS
                ? removalFailure.error
                : ERROR_GEN_FAILURE);
        return 7;
    }

    gUninstallStage = ChineseUi() ? L"删除安装文件" : L"Remove installation files";
    if (!RemoveInstallation(
            args.install,
            args.deleteData,
            removalFailure,
            args.deleteData
                ? &rootLease
                : nullptr)) {
        gRemovalFailurePath =
            removalFailure.path;
        gRemovalFailureLockOwners =
            removalFailure.lockOwners;

        SetLastError(
            removalFailure.error !=
                    ERROR_SUCCESS
                ? removalFailure.error
                : ERROR_GEN_FAILURE);
        return 6;
    }

    std::wstring message =
        args.deleteData
            ? (ChineseUi()
                   ? L"Asterun 已卸载完成。\n\n托管 Everything、后台服务和用户数据均已移除。"
                   : L"Asterun has been uninstalled.\n\nManaged Everything, its service, and user data were removed.")
            : (ChineseUi()
                   ? L"Asterun 已卸载完成。\n\n托管 Everything 和后台服务已移除；用户数据仍保留在原目录的 data 文件夹中。"
                   : L"Asterun has been uninstalled.\n\nManaged Everything and its service were removed. User data remains in the original data folder.");

    altrun::ui::ShowMessage(
        nullptr,
        message.c_str(),
        L"Asterun",
        MB_OK |
            MB_ICONINFORMATION |
            MB_SETFOREGROUND |
            MB_TOPMOST);

    CleanupSelfLater();
    return 0;
}

[[nodiscard]] int
BeginUninstall() {
    const auto current =
        CurrentExecutable();

    if (current.empty()) {
        return 10;
    }

    const auto install =
        current.parent_path();

    if (!ValidateInstallRoot(
            install)) {
        altrun::ui::ShowMessage(
            nullptr,
            ChineseUi()
                ? L"无法确认 Asterun 安装目录，卸载已取消。"
                : L"The Asterun installation directory could not be validated. Uninstall was cancelled.",
            L"Asterun",
            MB_OK |
                MB_ICONERROR);
        return 11;
    }

    const bool chinese =
        ChineseUi();

    const auto dataChoice =
        altrun::ui::ChooseUninstallData(
            nullptr,
            chinese);

    if (dataChoice ==
        altrun::ui::
            UninstallDataChoice::Cancel) {
        return 0;
    }

    const bool deleteData =
        dataChoice ==
        altrun::ui::
            UninstallDataChoice::Delete;

    if (deleteData &&
        !altrun::ui::
             ConfirmPermanentUserDataDeletion(
                 nullptr,
                 chinese)) {
        return 0;
    }

    EventHandle shellReleaseRequest;
    EventHandle shellReleaseDone;
    EventHandle shellLeaseAcquired;
    std::wstring shellReleaseRequestName;
    std::wstring shellReleaseDoneName;
    std::wstring shellLeaseAcquiredName;

    if (deleteData) {
        std::wstring brokerToken;
        std::uint32_t tokenError = 0;

        if (!altrun::win::
                 GenerateSecureToken(
                     brokerToken,
                     tokenError)) {
            SetLastError(
                tokenError != 0
                    ? tokenError
                    : ERROR_GEN_FAILURE);
            return 15;
        }

        shellReleaseRequestName =
            L"Local\\Asterun.Uninstall.ReleaseRequest." +
            brokerToken;
        shellReleaseDoneName =
            L"Local\\Asterun.Uninstall.ReleaseDone." +
            brokerToken;
        shellLeaseAcquiredName =
            L"Local\\Asterun.Uninstall.LeaseAcquired." +
            brokerToken;

        shellReleaseRequest.value =
            CreateEventW(
                nullptr,
                TRUE,
                FALSE,
                shellReleaseRequestName.c_str());
        shellReleaseDone.value =
            CreateEventW(
                nullptr,
                TRUE,
                FALSE,
                shellReleaseDoneName.c_str());
        shellLeaseAcquired.value =
            CreateEventW(
                nullptr,
                TRUE,
                FALSE,
                shellLeaseAcquiredName.c_str());

        if (!shellReleaseRequest.value ||
            !shellReleaseDone.value ||
            !shellLeaseAcquired.value) {
            return 15;
        }
    }

    std::wstring arguments =
        L"--perform --parent-pid " +
        std::to_wstring(
            GetCurrentProcessId()) +
        L" --install " +
        QuoteArgument(
            install.wstring()) +
        L" --delete-data " +
        (deleteData
             ? L"1"
             : L"0");

    if (deleteData) {
        arguments +=
            L" --shell-release-request " +
            QuoteArgument(
                shellReleaseRequestName) +
            L" --shell-release-done " +
            QuoteArgument(
                shellReleaseDoneName) +
            L" --shell-lease-acquired " +
            QuoteArgument(
                shellLeaseAcquiredName);
    }

    altrun::win::
        SecuredExecutable worker;
    std::uint32_t launchError = 0;

    if (!altrun::win::
             CreateSecuredTemporaryExecutableCopy(
                 current,
                 worker,
                 launchError)) {
        SetLastError(
            launchError != 0
                ? launchError
                : ERROR_GEN_FAILURE);
        return 13;
    }

    const auto workerDirectory =
        worker.temporaryDirectory;

    if (!SetCurrentDirectoryW(
            workerDirectory.c_str())) {
        const DWORD error =
            GetLastError();

        worker.RemoveTemporaryNow();

        altrun::ui::ShowMessage(
            nullptr,
            ChineseUi()
                ? L"无法释放安装目录，请关闭占用目录的程序后重试。"
                : L"Could not release the installation directory. Close programs using it and try again.",
            L"Asterun",
            MB_OK |
                MB_ICONERROR);

        SetLastError(error);
        return 17;
    }

    HANDLE workerProcess = nullptr;

    if (!altrun::win::
             LaunchSecuredExecutable(
                 worker,
                 arguments,
                 true,
                 SW_SHOWNORMAL,
                 workerProcess,
                 launchError)) {
        worker.RemoveTemporaryNow();

        if (launchError !=
            ERROR_CANCELLED) {
            altrun::ui::ShowMessage(
                nullptr,
                ChineseUi()
                    ? L"无法启动管理员卸载程序。"
                    : L"Could not start the elevated uninstaller.",
                L"Asterun",
                MB_OK |
                    MB_ICONERROR);
        }

        return launchError ==
                   ERROR_CANCELLED
            ? 0
            : 14;
    }

    // The UAC/process-creation window is now closed. Keep the random worker
    // for the child to run from; it schedules its own file and dedicated
    // temporary directory for cleanup.
    worker.Reset();

    RemoveStartupRegistration(
        install);
    RemoveSendToRegistration(
        install);

    if (deleteData &&
        workerProcess) {
        const bool brokerOk =
            ServeShellReleaseBroker(
                shellReleaseRequest.value,
                workerProcess,
                install,
                shellReleaseDone.value,
                shellLeaseAcquired.value);

        CloseHandle(
            workerProcess);

        if (!brokerOk) {
            altrun::ui::ShowMessage(
                nullptr,
                ChineseUi()
                    ? L"无法完成卸载前的资源管理器释放。"
                    : L"Could not complete the Explorer release handshake before uninstall.",
                L"Asterun",
                MB_OK |
                    MB_ICONERROR |
                    MB_SETFOREGROUND |
                    MB_TOPMOST);
            return 16;
        }

        return 0;
    }

    if (workerProcess) {
        CloseHandle(
            workerProcess);
    }

    return 0;
}

} // namespace

int WINAPI wWinMain(
    HINSTANCE,
    HINSTANCE,
    PWSTR,
    int) {
    PerformArguments args;

    if (ParsePerformArguments(
            args)) {
        const int result =
            PerformUninstall(args);

        if (result != 0) {
            const DWORD error =
                GetLastError();

            std::wstring message =
                ChineseUi()
                    ? L"卸载未能完成。\n\n错误代码："
                    : L"Uninstall could not be completed.\n\nError code: ";

            message +=
                std::to_wstring(
                    error != ERROR_SUCCESS
                        ? error
                        : static_cast<DWORD>(
                              result));

            if (!gUninstallStage.empty()) {
                message += ChineseUi() ? L"\n\n失败阶段：" : L"\n\nFailed step: ";
                message += gUninstallStage;
            }

            if (!gRemovalFailurePath
                     .empty()) {
                message +=
                    ChineseUi()
                        ? L"\n\n失败路径："
                        : L"\n\nFailed path: ";
                message +=
                    gRemovalFailurePath
                        .wstring();
            }

            if (!gRemovalFailureLockOwners
                     .empty()) {
                message +=
                    ChineseUi()
                        ? L"\n\n可能占用进程："
                        : L"\n\nPossible lock owner(s): ";
                message +=
                    gRemovalFailureLockOwners;
            }

            altrun::ui::ShowMessage(
                nullptr,
                message.c_str(),
                L"Asterun",
                MB_OK |
                    MB_ICONERROR |
                    MB_SETFOREGROUND |
                    MB_TOPMOST);
            CleanupSelfLater();
        }

        return result;
    }

    return BeginUninstall();
}
