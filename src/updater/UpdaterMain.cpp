#include "../platform/WindowsCommandLine.hpp"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#include <aclapi.h>

#include "../platform/AppIdentity.hpp"
#include "../platform/DllSearchHardening.hpp"
#include "../platform/HelperLanguage.hpp"
#include "../platform/SecureArchive.hpp"
#include "../platform/SecureElevation.hpp"
#include "UpdaterTransaction.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cwchar>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

// Until the request names the installation, follow the Windows display
// language; afterwards, the language chosen in Asterun's settings.
bool gChinese = false;

[[nodiscard]] const wchar_t* Text(
    const wchar_t* chinese,
    const wchar_t* english) {
    return gChinese ? chinese : english;
}

struct Arguments {
    DWORD parentPid{0};
    std::filesystem::path source;
    std::filesystem::path archive;
    std::wstring sha256;
    bool secureReextract{false};
    std::filesystem::path install;
    std::filesystem::path backup;
    std::wstring version;
    std::wstring healthEvent;
};

[[nodiscard]] std::optional<Arguments>
ParseArguments() {
    int argc = 0;
    LPWSTR* argv =
        CommandLineToArgvW(
            GetCommandLineW(),
            &argc);

    if (!argv) {
        return std::nullopt;
    }

    Arguments args;
    bool apply = false;

    const auto cleanup =
        [&]() {
            LocalFree(argv);
        };

    for (int i = 1;
         i < argc;
         ++i) {
        const std::wstring_view key(
            argv[i]);

        if (key == L"--apply") {
            apply = true;
            continue;
        }

        if (i + 1 >= argc) {
            cleanup();
            return std::nullopt;
        }

        const std::wstring_view value(
            argv[++i]);

        if (key ==
            L"--parent-pid") {
            wchar_t* end = nullptr;
            const unsigned long pid =
                std::wcstoul(
                    value.data(),
                    &end,
                    10);

            if (!end ||
                *end != L'\0' ||
                pid == 0) {
                cleanup();
                return std::nullopt;
            }

            args.parentPid =
                static_cast<DWORD>(
                    pid);
        } else if (
            key == L"--source") {
            args.source =
                std::wstring(value);
        } else if (
            key == L"--archive") {
            args.archive =
                std::wstring(value);
        } else if (
            key == L"--sha256") {
            args.sha256 =
                value;
        } else if (
            key ==
                L"--secure-reextract") {
            if (value == L"1") {
                args.secureReextract =
                    true;
            } else if (
                value == L"0") {
                args.secureReextract =
                    false;
            } else {
                cleanup();
                return std::nullopt;
            }
        } else if (
            key == L"--install") {
            args.install =
                std::wstring(value);
        } else if (
            key == L"--backup") {
            args.backup =
                std::wstring(value);
        } else if (
            key == L"--version") {
            args.version =
                value;
        } else if (
            key ==
                L"--health-event") {
            args.healthEvent =
                value;
        } else {
            cleanup();
            return std::nullopt;
        }
    }

    cleanup();

    if (!apply ||
        args.parentPid == 0 ||
        args.source.empty() ||
        args.archive.empty() ||
        args.sha256.empty() ||
        args.install.empty() ||
        args.backup.empty() ||
        args.version.empty() ||
        args.healthEvent.empty()) {
        return std::nullopt;
    }

    return args;
}

[[nodiscard]] bool
AsciiSha256(
    std::wstring_view value,
    std::string& output) {
    if (value.size() != 64) {
        return false;
    }

    output.clear();
    output.reserve(value.size());

    for (const wchar_t ch : value) {
        const bool digit =
            ch >= L'0' &&
            ch <= L'9';
        const bool lower =
            ch >= L'a' &&
            ch <= L'f';
        const bool upper =
            ch >= L'A' &&
            ch <= L'F';

        if (!digit &&
            !lower &&
            !upper) {
            output.clear();
            return false;
        }

        output.push_back(
            static_cast<char>(ch));
    }

    return true;
}

[[nodiscard]] bool
CreateProtectedWorkRoot(
    const std::filesystem::path& install,
    std::filesystem::path& root,
    std::uint32_t& nativeError) {
    const DWORD installAttributes =
        GetFileAttributesW(
            install.c_str());

    if (installAttributes ==
            INVALID_FILE_ATTRIBUTES ||
        (installAttributes &
         FILE_ATTRIBUTE_DIRECTORY) == 0 ||
        (installAttributes &
         FILE_ATTRIBUTE_REPARSE_POINT) !=
            0) {
        nativeError =
            installAttributes ==
                    INVALID_FILE_ATTRIBUTES
                ? static_cast<
                      std::uint32_t>(
                      GetLastError())
                : ERROR_REPARSE_TAG_INVALID;
        return false;
    }

    for (int attempt = 0;
         attempt < 16;
         ++attempt) {
        std::wstring token;

        if (!altrun::win::
                 GenerateSecureToken(
                     token,
                     nativeError)) {
            return false;
        }

        root =
            install /
            (L".asterun-update-work." +
             token);

        if (CreateDirectoryW(
                root.c_str(),
                nullptr)) {
            nativeError = 0;
            return true;
        }

        const DWORD error =
            GetLastError();

        if (error !=
            ERROR_ALREADY_EXISTS) {
            nativeError =
                static_cast<
                    std::uint32_t>(
                    error);
            return false;
        }
    }

    nativeError =
        ERROR_ALREADY_EXISTS;
    return false;
}

// Update.exe holds Asterun's single-instance guard and the update marker
// whenever no Asterun of its own is running. An Asterun started by hand then
// exits at once instead of keeping Asterun.exe mapped, which would make the
// file replacement or a rollback fail. Taken before the old process exits,
// so the named guard never lapses in between.
class InstanceGuard {
public:
    InstanceGuard() = default;
    InstanceGuard(const InstanceGuard&) = delete;
    InstanceGuard& operator=(const InstanceGuard&) = delete;

    ~InstanceGuard() {
        Release();
    }

    void Acquire() {
        if (!instance_) {
            instance_ = CreateMutexW(
                nullptr,
                FALSE,
                altrun::app_identity::kSingleInstanceMutexName);
        }

        if (!marker_) {
            marker_ = CreateMutexW(
                nullptr,
                FALSE,
                altrun::app_identity::kUpdateInProgressMutexName);
        }
    }

    // Before starting any Asterun, which must become the primary instance.
    void Release() {
        for (HANDLE* handle : {&marker_, &instance_}) {
            if (*handle) {
                CloseHandle(*handle);
                *handle = nullptr;
            }
        }
    }

private:
    HANDLE instance_{nullptr};
    HANDLE marker_{nullptr};
};

InstanceGuard gInstanceGuard;

// The old process normally exits within seconds, but closing a managed
// Everything can take much longer. Waiting it out lets the update proceed
// instead of cancelling while that process is still on its way out.
constexpr DWORD kParentExitTimeoutMs = 10 * 60 * 1000;

[[nodiscard]] bool
WaitForParent(DWORD pid) {
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
            kParentExitTimeoutMs);

    CloseHandle(process);
    return wait ==
        WAIT_OBJECT_0;
}

[[nodiscard]] bool
IsElevated() {
    HANDLE token = nullptr;

    if (!OpenProcessToken(
            GetCurrentProcess(),
            TOKEN_QUERY,
            &token)) {
        return false;
    }

    TOKEN_ELEVATION elevation{};
    DWORD bytes = 0;

    const BOOL ok =
        GetTokenInformation(
            token,
            TokenElevation,
            &elevation,
            sizeof(elevation),
            &bytes);

    CloseHandle(token);

    return ok &&
        elevation.TokenIsElevated !=
            0;
}

[[nodiscard]] std::wstring
QuoteArgument(std::wstring_view value) {
    return altrun::win::QuoteWindowsArgument(value);
}
[[nodiscard]] bool
LaunchNormal(
    const std::filesystem::path& exe,
    std::wstring arguments,
    PROCESS_INFORMATION& process) {
    std::wstring command =
        QuoteArgument(
            exe.wstring());

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

    return CreateProcessW(
               exe.c_str(),
               mutableCommand.data(),
               nullptr,
               nullptr,
               FALSE,
               CREATE_UNICODE_ENVIRONMENT,
               nullptr,
               exe.parent_path()
                   .c_str(),
               &startup,
               &process) != FALSE;
}

[[nodiscard]] bool
LaunchWithShellToken(
    const std::filesystem::path& exe,
    std::wstring arguments,
    PROCESS_INFORMATION& process) {
    const HWND shell =
        GetShellWindow();

    if (!shell) {
        return false;
    }

    DWORD shellPid = 0;
    GetWindowThreadProcessId(
        shell,
        &shellPid);

    if (shellPid == 0) {
        return false;
    }

    HANDLE shellProcess =
        OpenProcess(
            PROCESS_QUERY_LIMITED_INFORMATION,
            FALSE,
            shellPid);

    if (!shellProcess) {
        return false;
    }

    HANDLE shellToken = nullptr;

    if (!OpenProcessToken(
            shellProcess,
            TOKEN_DUPLICATE |
                TOKEN_ASSIGN_PRIMARY |
                TOKEN_QUERY,
            &shellToken)) {
        CloseHandle(shellProcess);
        return false;
    }

    HANDLE primaryToken = nullptr;

    const BOOL duplicated =
        DuplicateTokenEx(
            shellToken,
            MAXIMUM_ALLOWED,
            nullptr,
            SecurityImpersonation,
            TokenPrimary,
            &primaryToken);

    CloseHandle(shellToken);
    CloseHandle(shellProcess);

    if (!duplicated) {
        return false;
    }

    std::wstring command =
        QuoteArgument(
            exe.wstring());

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

    const BOOL created =
        CreateProcessWithTokenW(
            primaryToken,
            LOGON_WITH_PROFILE,
            exe.c_str(),
            mutableCommand.data(),
            CREATE_UNICODE_ENVIRONMENT,
            nullptr,
            exe.parent_path()
                .c_str(),
            &startup,
            &process);

    CloseHandle(primaryToken);

    return created != FALSE;
}

[[nodiscard]] bool
LaunchMain(
    const std::filesystem::path& exe,
    std::wstring arguments,
    PROCESS_INFORMATION& process) {
    if (IsElevated()) {
        if (LaunchWithShellToken(
                exe,
                arguments,
                process)) {
            return true;
        }
    }

    return LaunchNormal(
        exe,
        std::move(arguments),
        process);
}

// Do not discard the recovery source before the old EXE relaunch result.
[[nodiscard]] bool RestartPreviousApp(
    const std::filesystem::path& install) {
    gInstanceGuard.Release();
    PROCESS_INFORMATION restored{};
    if (!LaunchMain(
            install / L"Asterun.exe",
            L"",
            restored)) {
        return false;
    }
    CloseHandle(restored.hThread);
    CloseHandle(restored.hProcess);
    return true;
}

// An updater failure is user-visible. Tests dismiss the updater-owned
// message dialog by its exact HWND; do not add CI/session heuristics here.
void ShowFailureDialog(const std::wstring& message) {
    MessageBoxW(
        nullptr, message.c_str(),
        L"Asterun", MB_OK | MB_ICONERROR);
}

void ShowUpdateFailure(
    std::wstring_view reason,
    bool restarted,
    const std::filesystem::path& recovery = {}) {
    std::wstring message = Text(
        L"Asterun 更新失败：",
        L"The Asterun update failed: ");
    message.append(reason);
    message += restarted
        ? Text(L"\n已重新启动之前的版本。",
               L"\nThe previous version was restarted.")
        : Text(L"\n无法自动重新启动之前的版本。",
               L"\nThe previous version could not be restarted automatically.");
    if (!recovery.empty()) {
        message += Text(
            L"\n恢复文件已保留在：\n",
            L"\nRecovery files were retained at:\n");
        message += recovery.wstring();
    }
    ShowFailureDialog(message);
}

[[nodiscard]] HANDLE
CreateHealthEvent(
    std::wstring_view name) {
    HANDLE token = nullptr;

    if (!OpenProcessToken(
            GetCurrentProcess(),
            TOKEN_QUERY,
            &token)) {
        return nullptr;
    }

    DWORD required = 0;
    GetTokenInformation(
        token,
        TokenUser,
        nullptr,
        0,
        &required);

    if (required == 0 ||
        GetLastError() !=
            ERROR_INSUFFICIENT_BUFFER) {
        CloseHandle(token);
        return nullptr;
    }

    std::vector<std::byte>
        storage(required);
    auto* tokenUser =
        reinterpret_cast<TOKEN_USER*>(
            storage.data());

    if (!GetTokenInformation(
            token,
            TokenUser,
            tokenUser,
            required,
            &required)) {
        CloseHandle(token);
        return nullptr;
    }

    CloseHandle(token);

    EXPLICIT_ACCESSW access{};
    access.grfAccessPermissions =
        EVENT_MODIFY_STATE;
    access.grfAccessMode =
        SET_ACCESS;
    access.grfInheritance =
        NO_INHERITANCE;
    access.Trustee.TrusteeForm =
        TRUSTEE_IS_SID;
    access.Trustee.TrusteeType =
        TRUSTEE_IS_USER;
    access.Trustee.ptstrName =
        static_cast<LPWSTR>(
            tokenUser->User.Sid);

    PACL acl = nullptr;

    if (SetEntriesInAclW(
            1,
            &access,
            nullptr,
            &acl) !=
        ERROR_SUCCESS) {
        return nullptr;
    }

    SECURITY_DESCRIPTOR descriptor{};

    if (!InitializeSecurityDescriptor(
            &descriptor,
            SECURITY_DESCRIPTOR_REVISION) ||
        !SetSecurityDescriptorDacl(
            &descriptor,
            TRUE,
            acl,
            FALSE)) {
        LocalFree(acl);
        return nullptr;
    }

    SECURITY_ATTRIBUTES attributes{};
    attributes.nLength =
        sizeof(attributes);
    attributes.lpSecurityDescriptor =
        &descriptor;
    attributes.bInheritHandle = FALSE;

    HANDLE event =
        CreateEventW(
            &attributes,
            TRUE,
            FALSE,
            std::wstring(name).c_str());

    LocalFree(acl);
    return event;
}

void CleanupSelfLater() {
    altrun::win::
        ScheduleTemporaryWorkerSelfCleanup();
}

} // namespace

int WINAPI wWinMain(
    HINSTANCE,
    HINSTANCE,
    PWSTR,
    int) {
    // Before any other work: later DLL loads in this (possibly elevated)
    // process must not search the application or temporary directory.
    if (const DWORD searchError =
            altrun::win::RestrictDefaultDllSearchToSystem32();
        searchError != ERROR_SUCCESS) {
        gChinese = altrun::helper_language::PreferChinese({});
        const auto message =
            std::wstring(Text(
                L"更新未开始：Windows 无法限制更新程序的 DLL 搜索路径，未修改任何文件。\n\n错误代码：",
                L"The update was not started because Windows could not restrict "
                L"the DLL search path for the updater. No files were changed.\n\n"
                L"Error code: ")) +
            std::to_wstring(searchError);
        ShowFailureDialog(message);
        SetLastError(searchError);
        return 2;
    }

    const auto parsed =
        ParseArguments();

    gChinese = altrun::helper_language::PreferChinese(
        parsed ? parsed->install : std::filesystem::path{});

    if (!parsed) {
        ShowFailureDialog(Text(
            L"Update.exe 需要由 Asterun 发起的有效更新请求。未修改任何文件。",
            L"Update.exe requires a valid update request from Asterun. No files were changed."));
        return 2;
    }

    const Arguments& args =
        *parsed;

    gInstanceGuard.Acquire();

    if (!WaitForParent(
            args.parentPid)) {
        gInstanceGuard.Release();
        ShowFailureDialog(Text(
            L"之前的 Asterun 进程没有退出。更新已取消，未修改任何文件。",
            L"The previous Asterun process did not exit. The update was cancelled before changing any files."));
        return 2;
    }

    // The parent launcher has exited. On failure before any file replacement,
    // bring its intact executable back rather than leaving it stopped.
    const auto failBeforeApply =
        [&](std::wstring_view reason) {
            const bool restarted =
                RestartPreviousApp(args.install);
            ShowUpdateFailure(reason, restarted);
            return restarted ? 2 : 7;
        };

    std::string expectedSha256;

    if (!AsciiSha256(
            args.sha256,
            expectedSha256)) {
        return failBeforeApply(
            Text(L"更新包校验值无效。", L"Invalid package checksum."));
    }

    altrun::win::
        LockedVerifiedFile archiveLock;
    std::uint32_t archiveError = 0;

    if (!altrun::win::
             LockAndVerifySha256(
                 args.archive,
                 expectedSha256,
                 archiveLock,
                 archiveError)) {
        SetLastError(
            archiveError != 0
                ? archiveError
                : ERROR_CRC);
        return failBeforeApply(
            Text(L"更新包完整性校验失败。", L"Package integrity verification failed."));
    }

    std::filesystem::path
        protectedWorkRoot;
    std::filesystem::path
        transactionSource =
            args.source;
    std::filesystem::path
        transactionBackup =
            args.backup;
    std::error_code cleanupError;

    bool preserveRecovery = false;
    const auto cleanupWork =
        [&]() {
            archiveLock.Reset();

            if (!preserveRecovery && !protectedWorkRoot
                     .empty()) {
                std::filesystem::
                    remove_all(
                        protectedWorkRoot,
                        cleanupError);
                cleanupError.clear();
            }
        };

    if (args.secureReextract) {
        if (!IsElevated()) {
            cleanupWork();
            return failBeforeApply(
                Text(L"未获得管理员权限。", L"Administrator permission was not granted."));
        }

        std::uint32_t workError = 0;

        if (!CreateProtectedWorkRoot(
                args.install,
                protectedWorkRoot,
                workError)) {
            SetLastError(
                workError != 0
                    ? workError
                    : ERROR_ACCESS_DENIED);
            // No work root was created. The candidate path must not be
            // removed: it could belong to an unrelated existing directory.
            return failBeforeApply(
                Text(L"无法创建受保护的更新目录。", L"Could not create the protected update directory."));
        }

        transactionSource =
            protectedWorkRoot /
            L"stage";
        transactionBackup =
            protectedWorkRoot /
            L"backup";

        std::uint32_t extractionError =
            0;

        if (!altrun::win::
                 ExtractZipVerifiedSecure(
                     args.archive,
                     transactionSource,
                     extractionError)) {
            SetLastError(
                extractionError != 0
                    ? extractionError
                    : ERROR_INVALID_DATA);
            cleanupWork();
            return failBeforeApply(
                Text(L"安全解压更新包失败。", L"Secure package extraction failed."));
        }

        // The source tree now lives under the protected install root.
        // The user-writable archive can be released and deleted before any
        // elevated destination replacement starts.
        archiveLock.Reset();
        std::filesystem::remove(
            args.archive,
            cleanupError);
        cleanupError.clear();
    } else {
        // No privilege boundary is crossed for a writable portable install.
        // Still re-verify the archive after the old process exits so a stale
        // or modified handoff never gets applied accidentally.
        archiveLock.Reset();
    }

    altrun::updater::
        TransactionPaths transaction{
            .source =
                transactionSource,
            .install =
                args.install,
            .backup =
                transactionBackup,
            .version =
                args.version,
        };

    if (!altrun::updater::
             ValidateSource(
                 transaction)) {
        cleanupWork();
        return failBeforeApply(
            Text(L"待安装的更新文件无效。", L"The staged update files are invalid."));
    }

    altrun::updater::
        TransactionJournal journal;

    const auto restore = [&]() {
        const auto recovery = altrun::updater::Rollback(transaction, journal);
        if (!recovery.Complete()) {
            preserveRecovery = true;
            const auto log = transaction.backup / L"recovery-errors.txt";
            std::ofstream output(log, std::ios::binary | std::ios::app);
            for (const auto& failure : recovery.failures) {
                output << failure.relative.generic_string() << ": "
                       << failure.error.message() << '\n';
            }
            const auto message = std::wstring(Text(
                                     L"更新未能完全还原。恢复文件已保留在：\n",
                                     L"The update could not be fully restored. Recovery files were kept at:\n")) +
                transaction.backup.wstring();
            ShowFailureDialog(message);
        }
        return recovery.Complete();
    };

    // A successful rollback also needs a successful old-process relaunch.
    // In elevated mode retain the protected backup until that is confirmed.
    const auto failAfterApply =
        [&](int status, std::wstring_view reason) {
            gInstanceGuard.Acquire();
            if (!restore()) {
                cleanupWork();
                return 7;
            }
            const bool restarted =
                RestartPreviousApp(args.install);
            if (!restarted) {
                preserveRecovery = true;
            }
            cleanupWork();
            ShowUpdateFailure(
                reason,
                restarted,
                !restarted && !journal.files.empty()
                    ? transaction.backup
                    : std::filesystem::path{});
            return restarted ? status : 7;
        };

    if (!altrun::updater::
             ApplyPackage(
                 transaction,
                 journal)) {
        return failAfterApply(
            3, Text(L"文件替换未能完成。", L"File replacement did not complete."));
    }

    // The applied file set and protected backup journal are sufficient for
    // rollback after this point; the extracted source no longer needs to
    // remain live.
    if (args.secureReextract) {
        std::filesystem::remove_all(
            transactionSource,
            cleanupError);
        cleanupError.clear();
    }

    HANDLE healthEvent =
        CreateHealthEvent(
            args.healthEvent);

    if (!healthEvent) {
        return failAfterApply(
            4, Text(L"无法创建启动健康检查事件。", L"Could not create the startup health event."));
    }

    const auto mainExe =
        args.install /
        L"Asterun.exe";

    std::wstring mainArguments =
        L"--post-update-health-event " +
        QuoteArgument(
            args.healthEvent);

    PROCESS_INFORMATION child{};

    gInstanceGuard.Release();

    if (!LaunchMain(
            mainExe,
            mainArguments,
            child)) {
        CloseHandle(healthEvent);
        return failAfterApply(
            5, Text(L"无法启动更新后的程序。", L"Could not start the updated application."));
    }

    CloseHandle(child.hThread);

    // Also watch the process: a build that exits or crashes during startup
    // is rolled back at once instead of after the full health timeout. A
    // signal sent just before exiting still wins (lowest index).
    const HANDLE startup[]{
        healthEvent,
        child.hProcess,
    };
    const DWORD wait =
        WaitForMultipleObjects(
            2,
            startup,
            FALSE,
            30000);

    CloseHandle(healthEvent);

    if (wait == WAIT_OBJECT_0 + 1) {
        CloseHandle(child.hProcess);
        return failAfterApply(
            6, Text(L"更新后的程序在启动时退出。", L"The updated application exited during startup."));
    }

    if (wait != WAIT_OBJECT_0) {
        TerminateProcess(
            child.hProcess,
            ERROR_GEN_FAILURE);
        const DWORD stopped = WaitForSingleObject(child.hProcess, 5000);
        if (stopped != WAIT_OBJECT_0) {
            CloseHandle(child.hProcess);
            preserveRecovery = true;
            const auto message = std::wstring(Text(
                                     L"更新后的程序仍在运行。恢复文件已保留在：\n",
                                     L"The updated application is still running. Recovery files were kept at:\n")) +
                transaction.backup.wstring();
            ShowFailureDialog(message);
            cleanupWork();
            return 7;
        }
        CloseHandle(child.hProcess);

        return failAfterApply(
            6, Text(L"更新后的程序未通过启动健康检查。", L"The updated application failed its startup health check."));
    }

    CloseHandle(child.hProcess);

    cleanupWork();

    // The new build is healthy, so files the package no longer ships can go
    // without a way back.
    altrun::updater::PruneObsoletePackageFiles(
        transaction,
        journal);

    // Elevated updates back up into protectedWorkRoot, not args.backup.
    // Only the portable path owns and may delete the supplied backup.
    if (!args.secureReextract) {
        std::filesystem::remove_all(
            transaction.backup,
            cleanupError);
        cleanupError.clear();
    }
    std::filesystem::remove_all(
        args.source,
        cleanupError);
    cleanupError.clear();
    std::filesystem::remove(
        args.archive,
        cleanupError);

    CleanupSelfLater();
    return 0;
}
