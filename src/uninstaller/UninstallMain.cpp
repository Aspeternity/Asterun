#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
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

struct PerformArguments {
    DWORD parentPid{0};
    std::filesystem::path install;
    bool deleteData{false};
};

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
        } else {
            LocalFree(argv);
            return false;
        }
    }

    LocalFree(argv);

    return perform &&
        result.parentPid != 0 &&
        !result.install.empty() &&
        deleteDataSeen;
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

[[nodiscard]] bool
RemoveInstallation(
    const std::filesystem::path& install,
    bool deleteData) {
    std::error_code ec;
    const auto data =
        install /
        L"data";

    std::filesystem::remove_all(
        data /
            L"tools" /
            L"Everything",
        ec);
    if (ec) {
        return false;
    }

    ec.clear();
    std::filesystem::remove_all(
        data /
            L"update",
        ec);
    if (ec) {
        return false;
    }

    ec.clear();
    std::filesystem::remove(
        data /
            L"tools",
        ec);
    ec.clear();

    if (deleteData) {
        std::filesystem::remove_all(
            install,
            ec);
        return !ec;
    }

    for (std::filesystem::
             directory_iterator
             it(install, ec),
         end;
         !ec && it != end;
         it.increment(ec)) {
        if (LowerPath(
                it->path()
                    .filename()) ==
            L"data") {
            continue;
        }

        std::error_code removeError;
        std::filesystem::remove_all(
            it->path(),
            removeError);

        if (removeError) {
            return false;
        }
    }

    return !ec;
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
    if (!WaitForProcess(
            args.parentPid,
            30000) ||
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

    if (!RemoveInstallation(
            args.install,
            args.deleteData)) {
        return 6;
    }

    const std::wstring message =
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
            MB_ICONINFORMATION);

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

    if (info.hProcess) {
        CloseHandle(
            info.hProcess);
    }

    RemoveStartupRegistration(
        install);

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

            MessageBoxW(
                nullptr,
                message.c_str(),
                L"ALTRun Next",
                MB_OK |
                    MB_ICONERROR);
            CleanupSelfLater();
        }

        return result;
    }

    return BeginUninstall();
}
