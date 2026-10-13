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

#include "../platform/DllSearchHardening.hpp"
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

enum class ParentState {
    Exited,
    StillRunning,
    Unknown,
};

struct ParentWait {
    ParentState state{ParentState::Unknown};
    // Win32 error captured when the wait failed, or WAIT_TIMEOUT.
    DWORD error{0};
};

[[nodiscard]] ParentWait
WaitForParent(DWORD pid) {
    HANDLE process =
        OpenProcess(
            SYNCHRONIZE,
            FALSE,
            pid);

    if (!process) {
        const DWORD error = GetLastError();
        // ERROR_INVALID_PARAMETER: no process with this id, i.e. it exited.
        if (error == ERROR_INVALID_PARAMETER) {
            return {ParentState::Exited, 0};
        }
        return {ParentState::Unknown, error};
    }

    const DWORD wait =
        WaitForSingleObject(
            process,
            60000);
    const DWORD waitError =
        wait == WAIT_FAILED ? GetLastError() : 0;

    CloseHandle(process);

    if (wait == WAIT_OBJECT_0) {
        return {ParentState::Exited, 0};
    }
    if (wait == WAIT_TIMEOUT) {
        return {ParentState::StillRunning, WAIT_TIMEOUT};
    }
    return {ParentState::Unknown, waitError};
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

[[nodiscard]] HANDLE
CreateHealthEvent(
    std::wstring_view name,
    DWORD& error) {
    error = ERROR_SUCCESS;
    HANDLE token = nullptr;

    if (!OpenProcessToken(
            GetCurrentProcess(),
            TOKEN_QUERY,
            &token)) {
        error = GetLastError();
        return nullptr;
    }

    DWORD required = 0;
    GetTokenInformation(
        token,
        TokenUser,
        nullptr,
        0,
        &required);
    const DWORD sizeError = GetLastError();

    if (required == 0 ||
        sizeError !=
            ERROR_INSUFFICIENT_BUFFER) {
        error = sizeError != ERROR_SUCCESS ? sizeError : ERROR_INVALID_DATA;
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
        error = GetLastError();
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

    const DWORD aclError = SetEntriesInAclW(
            1,
            &access,
            nullptr,
            &acl);
    if (aclError != ERROR_SUCCESS) {
        error = aclError;
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
        error = GetLastError();
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
    // Store the failing API's result before cleanup can change last-error.
    if (!event) {
        error = GetLastError();
    }
    LocalFree(acl);
    return event;
}

void CleanupSelfLater() {
    altrun::win::
        ScheduleTemporaryWorkerSelfCleanup();
}

[[nodiscard]] bool
ChineseUi() {
    return PRIMARYLANGID(
               GetUserDefaultUILanguage()) ==
        LANG_CHINESE;
}

void ShowFailureMessage(
    const std::wstring& text) {
    MessageBoxW(
        nullptr,
        text.c_str(),
        L"Asterun",
        MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
}

// The step that failed, shown to the user. Kept in one place so the text of
// every failure prompt names the same steps.
enum class FailedStep {
    VerifyPackage,
    ObtainElevation,
    PrepareProtectedFolder,
    ExtractPackage,
    CheckContents,
    ReplaceFiles,
    PrepareHealthCheck,
    StartNewVersion,
    WaitNewVersion,
};

[[nodiscard]] const wchar_t*
StepName(
    FailedStep step,
    bool chinese) {
    switch (step) {
    case FailedStep::VerifyPackage:
        return chinese ? L"校验更新包" : L"Verify update package";
    case FailedStep::ObtainElevation:
        return chinese ? L"获取管理员权限" : L"Obtain administrator rights";
    case FailedStep::PrepareProtectedFolder:
        return chinese ? L"准备受保护目录" : L"Prepare protected folder";
    case FailedStep::ExtractPackage:
        return chinese ? L"解压更新包" : L"Extract update package";
    case FailedStep::CheckContents:
        return chinese ? L"检查更新内容" : L"Check update contents";
    case FailedStep::ReplaceFiles:
        return chinese ? L"替换程序文件" : L"Replace program files";
    case FailedStep::PrepareHealthCheck:
        return chinese ? L"准备启动检查" : L"Prepare startup check";
    case FailedStep::StartNewVersion:
        return chinese ? L"启动新版本" : L"Start new version";
    case FailedStep::WaitNewVersion:
        return chinese ? L"等待新版本就绪" : L"Wait for new version";
    }
    return L"";
}

// What became of the one attempt to start the previous version again. A
// process that is still running after the observation window has only been
// seen to start and stay up briefly; that is not a confirmed recovery.
enum class RestartState {
    CreateFailed,
    ExitedDuringObservation,
    RunningAfterObservation,
    QueryFailed,
};

struct RestartObservation {
    RestartState state{RestartState::CreateFailed};
    DWORD error{0};
    DWORD exitCode{0};
};

constexpr DWORD kRestartObservationMilliseconds = 5000;

[[nodiscard]] RestartObservation
RestartPreviousVersion(
    const std::filesystem::path& mainExe) {
    PROCESS_INFORMATION process{};

    if (!LaunchMain(
            mainExe,
            L"",
            process)) {
        return {RestartState::CreateFailed, GetLastError(), 0};
    }

    CloseHandle(process.hThread);

    RestartObservation result{RestartState::RunningAfterObservation, 0, 0};
    const DWORD wait =
        WaitForSingleObject(
            process.hProcess,
            kRestartObservationMilliseconds);

    if (wait == WAIT_OBJECT_0) {
        DWORD exitCode = 0;
        if (GetExitCodeProcess(process.hProcess, &exitCode)) {
            result = {RestartState::ExitedDuringObservation, 0, exitCode};
        } else {
            result = {RestartState::QueryFailed, GetLastError(), 0};
        }
    } else if (wait != WAIT_TIMEOUT) {
        result = {RestartState::QueryFailed, GetLastError(), 0};
    }

    CloseHandle(process.hProcess);
    return result;
}

[[nodiscard]] std::wstring
RestartText(
    const RestartObservation& restart,
    const std::filesystem::path& mainExe,
    bool chinese) {
    if (restart.state == RestartState::RunningAfterObservation) {
        return chinese ? L"已重新启动 Asterun。" : L"Asterun was restarted.";
    }

    std::wstring detail;
    switch (restart.state) {
    case RestartState::CreateFailed:
        detail = (chinese ? L"无法启动，错误代码 " : L"it could not be started, error code ") +
            std::to_wstring(restart.error);
        break;
    case RestartState::ExitedDuringObservation:
        detail = (chinese ? L"启动后很快退出，退出码 " : L"it exited shortly after starting, exit code ") +
            std::to_wstring(restart.exitCode);
        break;
    case RestartState::QueryFailed:
        detail = (chinese ? L"无法查询其运行状态，错误代码 " : L"its state could not be queried, error code ") +
            std::to_wstring(restart.error);
        break;
    case RestartState::RunningAfterObservation:
        break;
    }

    if (chinese) {
        return L"未能确认 Asterun 已恢复运行（" + detail + L"）。请手动启动：" + mainExe.wstring();
    }
    return L"Asterun could not be confirmed as running again (" + detail +
        L"). Start it manually: " + mainExe.wstring();
}

[[nodiscard]] bool
WriteRecoveryErrors(
    const std::filesystem::path& backup,
    const altrun::updater::RollbackResult& recovery) {
    std::ofstream output(
        backup / L"recovery-errors.txt",
        std::ios::binary | std::ios::app);

    if (!output) {
        return false;
    }

    for (const auto& failure : recovery.failures) {
        output << failure.relative.generic_string() << ": "
               << failure.error.message() << '\n';
    }

    output.close();
    return !output.fail();
}

// What is known about a path. Only Absent means "confirmed not there"; a
// query that fails for any other reason is Unknown, never Absent.
enum class PathState {
    Absent,
    PlainDirectory,
    Other,
    Unknown,
};

[[nodiscard]] PathState
QueryPathState(
    const std::filesystem::path& path) {
    if (path.empty()) {
        return PathState::Unknown;
    }

    const DWORD attributes =
        GetFileAttributesW(
            path.c_str());

    if (attributes == INVALID_FILE_ATTRIBUTES) {
        const DWORD error = GetLastError();
        return error == ERROR_FILE_NOT_FOUND ||
                error == ERROR_PATH_NOT_FOUND
            ? PathState::Absent
            : PathState::Unknown;
    }

    if ((attributes & FILE_ATTRIBUTE_DIRECTORY) != 0 &&
        (attributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0) {
        return PathState::PlainDirectory;
    }

    return PathState::Other;
}

// Everything the failure handling needs. protectedWorkRoot is non-empty only
// once this run has created it.
struct FailureContext {
    const Arguments& args;
    const altrun::updater::TransactionPaths& transaction;
    const altrun::updater::TransactionJournal& journal;
    const std::filesystem::path& protectedWorkRoot;
    altrun::win::LockedVerifiedFile& archiveLock;
    bool replacementStarted{false};
};

struct Failure {
    FailedStep step{FailedStep::VerifyPackage};
    // Win32 error saved where the failure happened; 0 when the failing check
    // has no Win32 error (it is then not shown).
    DWORD error{0};
    int exitCode{2};
    // Exit code when the previous program files are not restored (rollback
    // incomplete, or the new version could not be stopped). Unchanged from
    // the previous per-branch code: 3/4 keep 3/4, 5/6 use 7.
    int exitCodeIfNotRestored{2};
    bool newVersionStillRunning{false};
};

// The single place that ends a failed update after the parent has exited:
// restore what was replaced, start the previous version at most once, keep
// every recovery file once replacement has started, show one message, and
// return the existing exit code.
[[nodiscard]] int
FinishFailedUpdate(
    FailureContext& context,
    const Failure& failure) {
    context.archiveLock.Reset();

    enum class Outcome {
        NotReplaced,
        RolledBack,
        RollbackIncomplete,
        NewVersionNotStopped,
    };

    Outcome outcome = Outcome::NotReplaced;
    bool recoveryErrorsWritten = false;

    if (failure.newVersionStillRunning) {
        outcome = Outcome::NewVersionNotStopped;
    } else if (context.replacementStarted) {
        const auto recovery =
            altrun::updater::Rollback(
                context.transaction,
                context.journal);

        if (recovery.Complete()) {
            outcome = Outcome::RolledBack;
        } else {
            outcome = Outcome::RollbackIncomplete;
            recoveryErrorsWritten =
                WriteRecoveryErrors(
                    context.transaction.backup,
                    recovery);
        }
    } else if (!context.protectedWorkRoot.empty() &&
               QueryPathState(context.protectedWorkRoot / L"backup") ==
                   PathState::Absent) {
        // Replacement never started, this run created the protected work
        // root, and its backup path is confirmed absent: it holds no
        // recovery material. If the backup path exists, has an unexpected
        // type or cannot be queried, the work root is kept.
        std::error_code ignored;
        std::filesystem::remove_all(
            context.protectedWorkRoot,
            ignored);
    }

    const bool chinese = ChineseUi();
    const auto mainExe =
        context.args.install /
        L"Asterun.exe";

    std::wstring text;
    switch (outcome) {
    case Outcome::NotReplaced:
        text = chinese
            ? L"更新未完成，未替换已安装的程序文件。"
            : L"The update did not complete. No installed program files were replaced.";
        break;
    case Outcome::RolledBack:
        text = chinese
            ? L"更新失败，已恢复更新前的程序文件。"
            : L"The update failed. The previous program files were restored.";
        break;
    case Outcome::RollbackIncomplete:
        text = chinese
            ? L"更新失败，且未能完全恢复更新前的程序文件。未自动启动 Asterun。"
            : L"The update failed and the previous program files could not be fully restored. "
              L"Asterun was not started.";
        break;
    case Outcome::NewVersionNotStopped:
        text = chinese
            ? L"更新后的程序仍在运行且无法停止，未执行恢复，也未自动启动旧版本。"
            : L"The updated application is still running and could not be stopped. "
              L"No restore was attempted and the previous version was not started.";
        break;
    }

    if (outcome == Outcome::NotReplaced ||
        outcome == Outcome::RolledBack) {
        const auto restart =
            RestartPreviousVersion(mainExe);
        text += L"\n\n";
        text += RestartText(restart, mainExe, chinese);
    }

    if (outcome != Outcome::NotReplaced &&
        QueryPathState(context.transaction.backup) ==
            PathState::PlainDirectory) {
        text += L"\n\n";
        text += chinese ? L"恢复文件保留在：" : L"Recovery files were kept at: ";
        text += context.transaction.backup.wstring();
        if (outcome == Outcome::RollbackIncomplete ||
            outcome == Outcome::NewVersionNotStopped) {
            text += chinese ? L"\n请不要删除该目录。" : L"\nDo not delete this folder.";
        }
    }

    if (outcome == Outcome::RollbackIncomplete) {
        // Name recovery-errors.txt only when it was actually written.
        text += L"\n";
        text += recoveryErrorsWritten
            ? (chinese
                   ? L"失败详情已写入 " + (context.transaction.backup / L"recovery-errors.txt").wstring() + L"。"
                   : L"Failure details were written to " +
                       (context.transaction.backup / L"recovery-errors.txt").wstring() + L".")
            : std::wstring(chinese
                   ? L"未能写入失败详情文件。"
                   : L"The failure details could not be written.");
    }

    text += L"\n\n";
    text += chinese ? L"失败阶段：" : L"Failed step: ";
    text += StepName(failure.step, chinese);
    if (failure.error != 0) {
        text += chinese ? L"\n错误代码：" : L"\nError code: ";
        text += std::to_wstring(failure.error);
    }

    ShowFailureMessage(text);

    if (outcome == Outcome::RollbackIncomplete ||
        outcome == Outcome::NewVersionNotStopped) {
        return failure.exitCodeIfNotRestored;
    }
    return failure.exitCode;
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
        const auto message =
            (ChineseUi()
                 ? std::wstring(
                       L"更新未开始：Windows 无法限制更新程序的 DLL 搜索路径，未修改任何文件。\n\n"
                       L"错误代码：")
                 : std::wstring(
                       L"The update was not started because Windows could not restrict "
                       L"the DLL search path for the updater. No files were changed.\n\n"
                       L"Error code: ")) +
            std::to_wstring(searchError);
        MessageBoxW(nullptr, message.c_str(), L"Asterun", MB_OK | MB_ICONERROR);
        SetLastError(searchError);
        return 2;
    }

    const auto parsed =
        ParseArguments();

    if (!parsed) {
        return 2;
    }

    const Arguments& args =
        *parsed;

    const auto parent =
        WaitForParent(
            args.parentPid);

    if (parent.state != ParentState::Exited) {
        // The previous version may still be running: change nothing and start
        // nothing, only tell the user.
        const bool chinese = ChineseUi();
        std::wstring text =
            chinese
                ? L"更新未开始：Asterun 尚未退出，或无法确认其状态。未修改任何文件。\n请关闭 Asterun 后重新更新。"
                : L"The update did not start because Asterun has not exited, or its state could not be "
                  L"determined. No files were changed.\nClose Asterun and try the update again.";
        if (parent.error != 0) {
            text += chinese ? L"\n\n错误代码：" : L"\n\nError code: ";
            text += std::to_wstring(parent.error);
        }
        ShowFailureMessage(text);
        return 2;
    }

    altrun::win::
        LockedVerifiedFile archiveLock;
    std::filesystem::path
        protectedWorkRoot;
    altrun::updater::
        TransactionPaths transaction{
            .source =
                args.source,
            .install =
                args.install,
            .backup =
                args.backup,
            .version =
                args.version,
        };
    altrun::updater::
        TransactionJournal journal;
    std::error_code cleanupError;

    FailureContext context{
        args,
        transaction,
        journal,
        protectedWorkRoot,
        archiveLock,
    };

    const auto fail =
        [&](FailedStep step,
            DWORD error,
            int exitCode,
            int exitCodeIfNotRestored,
            bool newVersionStillRunning = false) {
            return FinishFailedUpdate(
                context,
                Failure{
                    step,
                    error,
                    exitCode,
                    exitCodeIfNotRestored,
                    newVersionStillRunning,
                });
        };

    std::string expectedSha256;

    if (!AsciiSha256(
            args.sha256,
            expectedSha256)) {
        return fail(FailedStep::VerifyPackage, 0, 2, 2);
    }

    std::uint32_t archiveError = 0;

    if (!altrun::win::
             LockAndVerifySha256(
                 args.archive,
                 expectedSha256,
                 archiveLock,
                 archiveError)) {
        return fail(
            FailedStep::VerifyPackage,
            archiveError != 0 ? archiveError : ERROR_CRC,
            2,
            2);
    }

    if (args.secureReextract) {
        if (!IsElevated()) {
            return fail(FailedStep::ObtainElevation, 0, 2, 2);
        }

        std::uint32_t workError = 0;
        std::filesystem::path createdRoot;

        if (!CreateProtectedWorkRoot(
                args.install,
                createdRoot,
                workError)) {
            // createdRoot may name a directory this run did not create;
            // protectedWorkRoot stays empty so nothing there is removed.
            return fail(
                FailedStep::PrepareProtectedFolder,
                workError != 0 ? workError : ERROR_ACCESS_DENIED,
                2,
                2);
        }

        protectedWorkRoot =
            createdRoot;
        transaction.source =
            protectedWorkRoot /
            L"stage";
        transaction.backup =
            protectedWorkRoot /
            L"backup";

        std::uint32_t extractionError =
            0;

        if (!altrun::win::
                 ExtractZipVerifiedSecure(
                     args.archive,
                     transaction.source,
                     extractionError)) {
            return fail(
                FailedStep::ExtractPackage,
                extractionError != 0 ? extractionError : ERROR_INVALID_DATA,
                2,
                2);
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

    if (!altrun::updater::
             ValidateSource(
                 transaction)) {
        return fail(FailedStep::CheckContents, 0, 2, 2);
    }

    // From here on every failure keeps the backup and the recovery work
    // directory, whatever the rollback result.
    context.replacementStarted = true;

    if (!altrun::updater::
             ApplyPackage(
                 transaction,
                 journal)) {
        return fail(FailedStep::ReplaceFiles, 0, 3, 3);
    }

    // The applied file set and protected backup journal are sufficient for
    // rollback after this point; the extracted source no longer needs to
    // remain live.
    if (args.secureReextract) {
        std::filesystem::remove_all(
            transaction.source,
            cleanupError);
        cleanupError.clear();
    }

    DWORD healthError = ERROR_SUCCESS;
    HANDLE healthEvent =
        CreateHealthEvent(
            args.healthEvent,
            healthError);

    if (!healthEvent) {
        return fail(FailedStep::PrepareHealthCheck, healthError, 4, 4);
    }

    const auto mainExe =
        args.install /
        L"Asterun.exe";

    std::wstring mainArguments =
        L"--post-update-health-event " +
        QuoteArgument(
            args.healthEvent);

    PROCESS_INFORMATION child{};

    if (!LaunchMain(
            mainExe,
            mainArguments,
            child)) {
        const DWORD error = GetLastError();
        CloseHandle(healthEvent);
        return fail(FailedStep::StartNewVersion, error, 5, 7);
    }

    CloseHandle(child.hThread);

    const DWORD wait =
        WaitForSingleObject(
            healthEvent,
            30000);
    const DWORD waitError =
        wait == WAIT_FAILED
            ? GetLastError()
            : (wait == WAIT_TIMEOUT ? static_cast<DWORD>(WAIT_TIMEOUT) : 0);

    CloseHandle(healthEvent);

    if (wait != WAIT_OBJECT_0) {
        TerminateProcess(
            child.hProcess,
            ERROR_GEN_FAILURE);
        const DWORD stopped = WaitForSingleObject(child.hProcess, 5000);
        CloseHandle(child.hProcess);
        return fail(
            FailedStep::WaitNewVersion,
            waitError,
            6,
            7,
            stopped != WAIT_OBJECT_0);
    }

    CloseHandle(child.hProcess);

    archiveLock.Reset();

    if (!protectedWorkRoot.empty()) {
        std::filesystem::remove_all(
            protectedWorkRoot,
            cleanupError);
        cleanupError.clear();
    }

    std::filesystem::remove_all(
        args.backup,
        cleanupError);
    cleanupError.clear();
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
