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
            (L".altrun-update-work." +
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
            60000);

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
    return QuoteWindowsArgument(value);
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
    const auto parsed =
        ParseArguments();

    if (!parsed) {
        return 2;
    }

    const Arguments& args =
        *parsed;

    if (!WaitForParent(
            args.parentPid)) {
        return 2;
    }

    std::string expectedSha256;

    if (!AsciiSha256(
            args.sha256,
            expectedSha256)) {
        return 2;
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
        return 2;
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
            return 2;
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
            return 2;
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
            return 2;
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
        return 2;
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
            const auto message = L"The update could not be fully restored. Recovery files were kept at:\n" +
                transaction.backup.wstring();
            MessageBoxW(nullptr, message.c_str(), L"ALTRun Next", MB_OK | MB_ICONERROR);
        }
        return recovery.Complete();
    };

    if (!altrun::updater::
             ApplyPackage(
                 transaction,
                 journal)) {
        restore();
        cleanupWork();
        return 3;
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
        restore();
        cleanupWork();
        return 4;
    }

    const auto mainExe =
        args.install /
        L"ALTRunNext.exe";

    std::wstring mainArguments =
        L"--post-update-health-event " +
        QuoteArgument(
            args.healthEvent);

    PROCESS_INFORMATION child{};

    if (!LaunchMain(
            mainExe,
            mainArguments,
            child)) {
        CloseHandle(healthEvent);
        const bool restoredCompletely = restore();
        cleanupWork();
        if (!restoredCompletely) return 7;

        PROCESS_INFORMATION restored{};
        if (LaunchMain(
                mainExe,
                L"",
                restored)) {
            CloseHandle(restored.hThread);
            CloseHandle(restored.hProcess);
        }

        return 5;
    }

    CloseHandle(child.hThread);

    const DWORD wait =
        WaitForSingleObject(
            healthEvent,
            30000);

    CloseHandle(healthEvent);

    if (wait != WAIT_OBJECT_0) {
        TerminateProcess(
            child.hProcess,
            ERROR_GEN_FAILURE);
        const DWORD stopped = WaitForSingleObject(child.hProcess, 5000);
        if (stopped != WAIT_OBJECT_0) {
            CloseHandle(child.hProcess);
            preserveRecovery = true;
            const auto message = L"The updated application is still running. Recovery files were kept at:\n" +
                transaction.backup.wstring();
            MessageBoxW(nullptr, message.c_str(), L"ALTRun Next", MB_OK | MB_ICONERROR);
            cleanupWork();
            return 7;
        }
        CloseHandle(child.hProcess);

        const bool restoredCompletely = restore();
        cleanupWork();
        if (!restoredCompletely) return 7;

        PROCESS_INFORMATION restored{};
        if (LaunchMain(
                mainExe,
                L"",
                restored)) {
            CloseHandle(restored.hThread);
            CloseHandle(restored.hProcess);
        }

        return 6;
    }

    CloseHandle(child.hProcess);

    cleanupWork();

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
