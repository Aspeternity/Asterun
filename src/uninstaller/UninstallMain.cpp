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
#include <tlhelp32.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cwchar>
#include <cwctype>
#include <filesystem>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace {

constexpr wchar_t kLauncherClass[] =
    L"ALTRunNext.Launcher";
constexpr wchar_t kLauncherTitle[] =
    L"ALTRun Next";
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
    std::wstring shellLeaseAcquired;
};

struct RemovalFailure {
    DWORD error{ERROR_SUCCESS};
    std::filesystem::path path;
    std::wstring lockOwners;
};

std::filesystem::path gRemovalFailurePath;
std::wstring gRemovalFailureLockOwners;

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
QuoteArgument(
    std::wstring_view value) {
    std::wstring result = L"\"";
    std::size_t slashes = 0;

    for (const wchar_t c : value) {
        if (c == L'\\') {
            ++slashes;
            continue;
        }

        if (c == L'\"') {
            result.append(
                slashes * 2 + 1,
                L'\\');
            result.push_back(L'\"');
            slashes = 0;
            continue;
        }

        result.append(
            slashes,
            L'\\');
        slashes = 0;
        result.push_back(c);
    }

    result.append(
        slashes * 2,
        L'\\');
    result.push_back(L'\"');
    return result;
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
    CloseHandle(process);
    return wait == WAIT_OBJECT_0;
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

    for (const auto* name : {
             L"ALTRunNext.exe",
             L"VERSION",
         }) {
        ec.clear();

        if (!std::filesystem::
                 is_regular_file(
                     install / name,
                     ec) ||
            ec) {
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
        L"ALTRunNext";

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
                        L"ALTRunNext.exe")) {
                RegDeleteValueW(
                    key,
                    valueName);
            }
        }
    }

    RegCloseKey(key);
}

[[nodiscard]] bool
GracefullyCloseALTRun(
    const std::filesystem::path& install) {
    const auto expected =
        install /
        L"ALTRunNext.exe";

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
            HANDLE process =
                OpenProcess(
                    PROCESS_TERMINATE |
                        SYNCHRONIZE,
                    FALSE,
                    entry.th32ProcessID);

            if (!process) {
                success = false;
            } else {
                if (!TerminateProcess(
                        process,
                        ERROR_CANCELLED)) {
                    success = false;
                } else {
                    WaitForSingleObject(
                        process,
                        5000);
                }
                CloseHandle(process);
            }
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
DetachedAlpha91Root() {
    std::array<wchar_t, 32768>
        programFiles{};
    const DWORD length =
        GetEnvironmentVariableW(
            L"ProgramFiles",
            programFiles.data(),
            static_cast<DWORD>(
                programFiles.size()));

    if (length == 0 ||
        length >=
            programFiles.size()) {
        return {};
    }

    return std::filesystem::path(
               std::wstring(
                   programFiles.data(),
                   length)) /
        L"Aspeternity" /
        L"ALTRunNext" /
        L"EverythingService";
}

struct ServiceCleanupResult {
    bool success{true};
    bool removed{false};
    bool detachedAlpha91{false};
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
            SERVICE_QUERY_CONFIG |
                SERVICE_QUERY_STATUS |
                SERVICE_STOP |
                DELETE);

    if (!service.value) {
        const DWORD error =
            GetLastError();

        if (error ==
            ERROR_SERVICE_DOES_NOT_EXIST) {
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
    const auto detachedRoot =
        DetachedAlpha91Root();

    const bool ownedPortable =
        PathStartsWithDirectory(
            serviceExecutable,
            managedRoot);
    const bool ownedDetached =
        !detachedRoot.empty() &&
        PathStartsWithDirectory(
            serviceExecutable,
            detachedRoot);

    if (!ownedPortable &&
        !ownedDetached) {
        return result;
    }

    result.detachedAlpha91 =
        ownedDetached;

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

    if (status.dwCurrentState !=
        SERVICE_STOPPED) {
        SERVICE_STATUS stopStatus{};

        if (!ControlService(
                service.value,
                SERVICE_CONTROL_STOP,
                &stopStatus)) {
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
TerminateManagedEverythingProcesses(
    const std::filesystem::path& install) {
    const auto root =
        install /
        L"data" /
        L"tools" /
        L"Everything";

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
            LowerPath(
                actual.filename()) ==
                L"everything.exe" &&
            PathStartsWithDirectory(
                actual,
                root)) {
            HANDLE process =
                OpenProcess(
                    PROCESS_TERMINATE |
                        SYNCHRONIZE,
                    FALSE,
                    entry.th32ProcessID);

            if (!process) {
                success = false;
            } else {
                if (!TerminateProcess(
                        process,
                        ERROR_CANCELLED)) {
                    success = false;
                } else {
                    WaitForSingleObject(
                        process,
                        5000);
                }
                CloseHandle(process);
            }
        }

        more =
            Process32NextW(
                snapshot,
                &entry);
    }

    CloseHandle(snapshot);
    return success;
}

void CleanupDetachedAlpha91Files() {
    const auto root =
        DetachedAlpha91Root();

    if (root.empty()) {
        return;
    }

    std::error_code ec;
    std::filesystem::remove_all(
        root,
        ec);

    if (ec) {
        return;
    }

    const auto altrun =
        root.parent_path();
    const auto vendor =
        altrun.parent_path();

    ec.clear();
    std::filesystem::remove(
        altrun,
        ec);
    ec.clear();
    std::filesystem::remove(
        vendor,
        ec);
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
    // immediately enumerate/select the ALTRun folder we are about to delete.
    // Prefer one level farther out so the installation root is not a visible
    // child of the active Shell view.
    if (!grandparent.empty() &&
        NormalizePath(
            grandparent) !=
            NormalizePath(parent) &&
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

    // The normal-integrity broker still holds its own DELETE-capable directory
    // handle at this point. Acquire the elevated worker's matching lease before
    // acknowledging the handoff, so there is never a window in which Explorer
    // or another Shell component can reopen the root without FILE_SHARE_DELETE.
    if (!AcquireDirectoryDeleteLease(
            args.install,
            rootLease,
            failure,
            8000)) {
        SetLastError(
            failure.error);
        return false;
    }

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

    return true;
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
            60000);

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
        // ALTRun folder and turned the broker's lease attempt into a frequent
        // self-inflicted sharing timeout. Signal release immediately; the
        // elevated worker owns the lease acquisition and performs it before
        // any destructive cleanup.
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
                    SYNCHRONIZE,
                FILE_SHARE_READ |
                    FILE_SHARE_WRITE |
                    FILE_SHARE_DELETE,
                nullptr,
                OPEN_EXISTING,
                FILE_FLAG_BACKUP_SEMANTICS,
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

    FILE_DISPOSITION_INFO disposition{};
    disposition.DeleteFile =
        TRUE;

    if (!SetFileInformationByHandle(
            lease.value,
            FileDispositionInfo,
            &disposition,
            sizeof(disposition))) {
        const DWORD error =
            GetLastError();

        failure.error =
            error != ERROR_SUCCESS
                ? error
                : ERROR_GEN_FAILURE;
        failure.path = path;
        return false;
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

    for (int attempt = 0;
         attempt < attempts;
         ++attempt) {
        std::error_code ec;

        const bool removed =
            std::filesystem::remove(
                path,
                ec);

        if (!ec) {
            return removed ||
                !std::filesystem::exists(
                    path,
                    ec);
        }

        const DWORD error =
            NativeFilesystemError(ec);

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
    std::error_code ec;

    if (!std::filesystem::exists(
            path,
            ec)) {
        return !ec;
    }

    const bool isSymlink =
        std::filesystem::is_symlink(
            path,
            ec);

    if (ec) {
        failure.error =
            NativeFilesystemError(ec);
        failure.path = path;
        return false;
    }

    const bool isDirectory =
        std::filesystem::is_directory(
            path,
            ec);

    if (ec) {
        failure.error =
            NativeFilesystemError(ec);
        failure.path = path;
        return false;
    }

    if (!isDirectory ||
        isSymlink) {
        return RemoveOneWithRetry(
            path,
            failure);
    }

    std::vector<std::filesystem::path>
        files;
    std::vector<std::filesystem::path>
        directories;

    {
        std::filesystem::
            recursive_directory_iterator
            it(
                path,
                std::filesystem::
                    directory_options::
                        skip_permission_denied,
                ec);
        const std::filesystem::
            recursive_directory_iterator
            endIterator;

        while (!ec &&
               it != endIterator) {
            std::error_code typeError;
            const bool childSymlink =
                it->is_symlink(
                    typeError);

            if (typeError) {
                failure.error =
                    NativeFilesystemError(
                        typeError);
                failure.path =
                    it->path();
                return false;
            }

            const bool childDirectory =
                it->is_directory(
                    typeError);

            if (typeError) {
                failure.error =
                    NativeFilesystemError(
                        typeError);
                failure.path =
                    it->path();
                return false;
            }

            if (childDirectory &&
                !childSymlink) {
                directories.push_back(
                    it->path());
            } else {
                files.push_back(
                    it->path());
            }

            it.increment(ec);
        }
    }

    if (ec) {
        failure.error =
            NativeFilesystemError(ec);
        failure.path = path;
        return false;
    }

    for (const auto& file : files) {
        if (!RemoveOneWithRetry(
                file,
                failure)) {
            return false;
        }
    }

    std::sort(
        directories.begin(),
        directories.end(),
        [](const auto& left,
           const auto& right) {
            return left.native().size() >
                right.native().size();
        });

    for (const auto& directory :
         directories) {
        if (!RemoveOneWithRetry(
                directory,
                failure)) {
            return false;
        }
    }

    return RemoveOneWithRetry(
        path,
        failure);
}

[[nodiscard]] bool
RemoveInstallation(
    const std::filesystem::path& install,
    bool deleteData,
    RemovalFailure& failure,
    DirectoryHandle* rootLease) {
    const auto data =
        install /
        L"data";

    // A stopped service can race with final image/database handle release.
    // Antivirus/indexing can also briefly hold a freshly stopped Everything
    // file. Retry only normal transient Windows delete failures.
    if (!RemoveAllWithRetry(
            data /
                L"tools" /
                L"Everything",
            failure)) {
        return false;
    }

    if (!RemoveAllWithRetry(
            data /
                L"update",
            failure)) {
        return false;
    }

    {
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

        entries.push_back(
            it->path());
    }

    if (ec) {
        failure.error =
            NativeFilesystemError(ec);
        failure.path = install;
        return false;
    }

    for (const auto& entry : entries) {
        if (!RemoveAllWithRetry(
                entry,
                failure)) {
            return false;
        }
    }

    if (deleteData) {
        if (!rootLease) {
            failure.error =
                ERROR_INVALID_HANDLE;
            failure.path =
                install;
            return false;
        }

        return DeleteDirectoryThroughLease(
            *rootLease,
            install,
            failure);
    }

    return true;
}

void CleanupSelfLater() {
    const auto current =
        CurrentExecutable();

    if (!current.empty()) {
        MoveFileExW(
            current.c_str(),
            nullptr,
            MOVEFILE_DELAY_UNTIL_REBOOT);
    }
}

[[nodiscard]] int
PerformUninstall(
    const PerformArguments& args) {
    // Preserve-data mode can use the original simple parent-exit handshake.
    // Full-remove mode keeps the normal-integrity parent alive as an Explorer
    // broker until the elevated worker reaches the actual deletion phase.
    if ((!args.deleteData &&
         !WaitForProcess(
             args.parentPid,
             30000)) ||
        !ValidateInstallRoot(
            args.install)) {
        return 2;
    }

    if (!GracefullyCloseALTRun(
            args.install)) {
        return 3;
    }

    const auto service =
        StopAndDeleteOwnedEverythingService(
            args.install);

    if (!service.success) {
        SetLastError(
            service.error);
        return 4;
    }

    if (!TerminateManagedEverythingProcesses(
            args.install)) {
        return 5;
    }

    if (service.detachedAlpha91) {
        CleanupDetachedAlpha91Files();
    } else {
        // Also clean a harmless alpha.9.1 orphan if migration already moved
        // the service back to the portable tree.
        CleanupDetachedAlpha91Files();
    }

    RemovalFailure removalFailure;
    DirectoryHandle rootLease;

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
                   ? L"ALTRun Next 已卸载完成。\n\n托管 Everything、后台服务和用户数据均已移除。"
                   : L"ALTRun Next has been uninstalled.\n\nManaged Everything, its service, and user data were removed.")
            : (ChineseUi()
                   ? L"ALTRun Next 已卸载完成。\n\n托管 Everything 和后台服务已移除；用户数据仍保留在原目录的 data 文件夹中。"
                   : L"ALTRun Next has been uninstalled.\n\nManaged Everything and its service were removed. User data remains in the original data folder.");

    MessageBoxW(
        nullptr,
        message.c_str(),
        L"ALTRun Next",
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
        MessageBoxW(
            nullptr,
            ChineseUi()
                ? L"无法确认 ALTRun Next 安装目录，卸载已取消。"
                : L"The ALTRun Next installation directory could not be validated. Uninstall was cancelled.",
            L"ALTRun Next",
            MB_OK |
                MB_ICONERROR);
        return 11;
    }

    const int confirm =
        MessageBoxW(
            nullptr,
            ChineseUi()
                ? L"将卸载 ALTRun Next，并移除由 ALTRun Next 安装的 Everything 客户端及 Windows Service。\n\n你自己安装的外部 Everything 不会被修改。\n\n是否继续？"
                : L"This will uninstall ALTRun Next and remove the Everything client and Windows Service installed by ALTRun Next.\n\nExternal Everything installations are not modified.\n\nContinue?",
            L"卸载 ALTRun Next / Uninstall ALTRun Next",
            MB_YESNO |
                MB_ICONWARNING |
                MB_DEFBUTTON2);

    if (confirm != IDYES) {
        return 0;
    }

    const int dataChoice =
        MessageBoxW(
            nullptr,
            ChineseUi()
                ? L"是否同时删除设置、快捷词、使用记录等用户数据？\n\n“是” = 全部删除\n“否” = 保留 data 用户数据\n“取消” = 退出卸载"
                : L"Also delete settings, shortcuts, usage history and other user data?\n\nYes = delete everything\nNo = preserve user data in data\nCancel = stop uninstalling",
            L"用户数据 / User data",
            MB_YESNOCANCEL |
                MB_ICONQUESTION |
                MB_DEFBUTTON2);

    if (dataChoice == IDCANCEL) {
        return 0;
    }

    const bool deleteData =
        dataChoice == IDYES;

    std::array<wchar_t, 32768>
        tempPath{};
    const DWORD tempLength =
        GetTempPathW(
            static_cast<DWORD>(
                tempPath.size()),
            tempPath.data());

    if (tempLength == 0 ||
        tempLength >=
            tempPath.size()) {
        return 12;
    }

    const auto tempExe =
        std::filesystem::path(
            tempPath.data()) /
        (L"ALTRunNext-Uninstall." +
         std::to_wstring(
             GetCurrentProcessId()) +
         L"." +
         std::to_wstring(
             GetTickCount64()) +
         L".exe");

    EventHandle shellReleaseRequest;
    EventHandle shellReleaseDone;
    EventHandle shellLeaseAcquired;
    std::wstring shellReleaseRequestName;
    std::wstring shellReleaseDoneName;
    std::wstring shellLeaseAcquiredName;

    if (deleteData) {
        const auto brokerToken =
            std::to_wstring(
                GetCurrentProcessId()) +
            L"." +
            std::to_wstring(
                GetTickCount64());

        shellReleaseRequestName =
            L"Local\\ALTRunNext.Uninstall.ReleaseRequest." +
            brokerToken;
        shellReleaseDoneName =
            L"Local\\ALTRunNext.Uninstall.ReleaseDone." +
            brokerToken;
        shellLeaseAcquiredName =
            L"Local\\ALTRunNext.Uninstall.LeaseAcquired." +
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

    std::error_code ec;
    std::filesystem::copy_file(
        current,
        tempExe,
        std::filesystem::
            copy_options::
                overwrite_existing,
        ec);

    if (ec) {
        return 13;
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

    SHELLEXECUTEINFOW info{};
    info.cbSize = sizeof(info);
    info.fMask =
        SEE_MASK_NOCLOSEPROCESS |
        SEE_MASK_NOASYNC;
    info.lpVerb = L"runas";
    info.lpFile =
        tempExe.c_str();
    info.lpParameters =
        arguments.c_str();
    info.lpDirectory =
        tempExe.parent_path()
            .c_str();
    info.nShow = SW_SHOWNORMAL;

    if (!ShellExecuteExW(
            &info)) {
        const DWORD error =
            GetLastError();
        std::filesystem::remove(
            tempExe,
            ec);

        if (error !=
            ERROR_CANCELLED) {
            MessageBoxW(
                nullptr,
                ChineseUi()
                    ? L"无法启动管理员卸载程序。"
                    : L"Could not start the elevated uninstaller.",
                L"ALTRun Next",
                MB_OK |
                    MB_ICONERROR);
        }

        return error ==
                   ERROR_CANCELLED
            ? 0
            : 14;
    }

    RemoveStartupRegistration(
        install);

    if (deleteData &&
        info.hProcess) {
        const bool brokerOk =
            ServeShellReleaseBroker(
                shellReleaseRequest.value,
                info.hProcess,
                install,
                shellReleaseDone.value,
                shellLeaseAcquired.value);

        CloseHandle(
            info.hProcess);

        if (!brokerOk) {
            MessageBoxW(
                nullptr,
                ChineseUi()
                    ? L"无法完成卸载前的资源管理器释放。"
                    : L"Could not complete the Explorer release handshake before uninstall.",
                L"ALTRun Next",
                MB_OK |
                    MB_ICONERROR |
                    MB_SETFOREGROUND |
                    MB_TOPMOST);
            return 16;
        }

        return 0;
    }

    if (info.hProcess) {
        CloseHandle(
            info.hProcess);
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

            MessageBoxW(
                nullptr,
                message.c_str(),
                L"ALTRun Next",
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
