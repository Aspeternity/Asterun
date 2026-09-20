#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>

#include <algorithm>
#include <array>
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
        args.install.empty() ||
        args.backup.empty() ||
        args.version.empty() ||
        args.healthEvent.empty()) {
        return std::nullopt;
    }

    return args;
}

[[nodiscard]] std::wstring
LowerPath(
    const std::filesystem::path& path) {
    std::wstring value =
        path.lexically_normal()
            .wstring();

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
    return wait == WAIT_OBJECT_0;
}

[[nodiscard]] bool
ReadTrimmedText(
    const std::filesystem::path& path,
    std::wstring& value) {
    std::ifstream input(
        path,
        std::ios::binary);

    if (!input) {
        return false;
    }

    std::string content{
        std::istreambuf_iterator<char>{
            input},
        std::istreambuf_iterator<char>{}};

    while (!content.empty() &&
           (content.back() == '\r' ||
            content.back() == '\n' ||
            content.back() == ' ' ||
            content.back() == '\t')) {
        content.pop_back();
    }

    value.clear();
    value.reserve(content.size());

    for (unsigned char c : content) {
        if (c > 0x7f) {
            return false;
        }
        value.push_back(
            static_cast<wchar_t>(
                c));
    }

    return !value.empty();
}

[[nodiscard]] bool
IsDataRelative(
    const std::filesystem::path& relative) {
    if (relative.empty()) {
        return false;
    }

    const auto first =
        relative.begin();

    if (first == relative.end()) {
        return false;
    }

    std::wstring value =
        first->wstring();

    std::transform(
        value.begin(),
        value.end(),
        value.begin(),
        [](wchar_t c) {
            return static_cast<wchar_t>(
                std::towlower(c));
        });

    return value == L"data";
}

[[nodiscard]] bool
ValidateSource(
    const Arguments& args) {
    std::error_code ec;

    if (!args.source.is_absolute() ||
        !args.install.is_absolute() ||
        !args.backup.is_absolute() ||
        LowerPath(args.source) ==
            LowerPath(args.install) ||
        LowerPath(args.backup) ==
            LowerPath(args.install)) {
        return false;
    }

    for (const auto* name : {
             L"ALTRunNext.exe",
             L"ALTRunNext.Updater.exe",
             L"VERSION",
         }) {
        if (!std::filesystem::
                 is_regular_file(
                     args.source /
                         name,
                     ec) ||
            ec) {
            return false;
        }
    }

    std::wstring stagedVersion;

    return ReadTrimmedText(
               args.source /
                   L"VERSION",
               stagedVersion) &&
        stagedVersion ==
            args.version;
}

[[nodiscard]] bool
CopyOneFile(
    const std::filesystem::path& source,
    const std::filesystem::path& destination,
    const std::filesystem::path& backup,
    bool& wasNew) {
    std::error_code ec;
    wasNew =
        !std::filesystem::exists(
            destination,
            ec);

    if (ec) {
        return false;
    }

    if (!wasNew) {
        if (!std::filesystem::
                 is_regular_file(
                     destination,
                     ec) ||
            ec) {
            return false;
        }

        std::filesystem::
            create_directories(
                backup.parent_path(),
                ec);

        if (ec) {
            return false;
        }

        std::filesystem::copy_file(
            destination,
            backup,
            std::filesystem::
                copy_options::
                    overwrite_existing,
            ec);

        if (ec) {
            return false;
        }
    }

    std::filesystem::create_directories(
        destination.parent_path(),
        ec);

    if (ec) {
        return false;
    }

    std::filesystem::copy_file(
        source,
        destination,
        std::filesystem::
            copy_options::
                overwrite_existing,
        ec);

    if (!ec) {
        return true;
    }

    // The destination can already have been opened/truncated before a copy
    // reports failure. Repair this current item immediately because the
    // caller has not yet appended its transaction record.
    std::error_code recoveryError;

    if (wasNew) {
        std::filesystem::remove(
            destination,
            recoveryError);
    } else {
        std::filesystem::copy_file(
            backup,
            destination,
            std::filesystem::
                copy_options::
                    overwrite_existing,
            recoveryError);
    }

    return false;
}

struct ApplyRecord {
    std::filesystem::path relative;
    bool wasNew{false};
};

[[nodiscard]] bool
ApplyPackage(
    const Arguments& args,
    std::vector<ApplyRecord>& records) {
    std::error_code ec;

    std::filesystem::remove_all(
        args.backup,
        ec);
    ec.clear();

    std::filesystem::create_directories(
        args.backup,
        ec);

    if (ec) {
        return false;
    }

    for (std::filesystem::
             recursive_directory_iterator
             it(args.source, ec),
         end;
         !ec && it != end;
         it.increment(ec)) {
        const auto relative =
            std::filesystem::relative(
                it->path(),
                args.source,
                ec);

        if (ec ||
            relative.empty() ||
            relative ==
                std::filesystem::path(
                    L".")) {
            return false;
        }

        if (IsDataRelative(
                relative)) {
            if (it->is_directory(ec) &&
                !ec) {
                it.disable_recursion_pending();
            }
            ec.clear();
            continue;
        }

        if (it->is_symlink(ec)) {
            return false;
        }

        if (it->is_directory(ec)) {
            ec.clear();
            std::filesystem::
                create_directories(
                    args.install /
                        relative,
                    ec);
            if (ec) {
                return false;
            }
            continue;
        }

        if (ec ||
            !it->is_regular_file(ec) ||
            ec) {
            return false;
        }

        bool wasNew = false;

        if (!CopyOneFile(
                it->path(),
                args.install /
                    relative,
                args.backup /
                    relative,
                wasNew)) {
            return false;
        }

        records.push_back({
            relative,
            wasNew,
        });
    }

    return !ec;
}

void Rollback(
    const Arguments& args,
    const std::vector<ApplyRecord>&
        records) {
    std::error_code ec;

    for (auto it =
             records.rbegin();
         it != records.rend();
         ++it) {
        const auto destination =
            args.install /
            it->relative;

        if (it->wasNew) {
            std::filesystem::remove(
                destination,
                ec);
            ec.clear();
            continue;
        }

        const auto backup =
            args.backup /
            it->relative;

        if (std::filesystem::
                is_regular_file(
                    backup,
                    ec) &&
            !ec) {
            std::filesystem::copy_file(
                backup,
                destination,
                std::filesystem::
                    copy_options::
                        overwrite_existing,
                ec);
        }

        ec.clear();
    }
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
    SECURITY_DESCRIPTOR descriptor{};

    if (!InitializeSecurityDescriptor(
            &descriptor,
            SECURITY_DESCRIPTOR_REVISION) ||
        !SetSecurityDescriptorDacl(
            &descriptor,
            TRUE,
            nullptr,
            FALSE)) {
        return nullptr;
    }

    SECURITY_ATTRIBUTES attributes{};
    attributes.nLength =
        sizeof(attributes);
    attributes.lpSecurityDescriptor =
        &descriptor;
    attributes.bInheritHandle = FALSE;

    return CreateEventW(
        &attributes,
        TRUE,
        FALSE,
        std::wstring(name).c_str());
}

void CleanupSelfLater() {
    std::array<wchar_t, 32768>
        path{};

    const DWORD length =
        GetModuleFileNameW(
            nullptr,
            path.data(),
            static_cast<DWORD>(
                path.size()));

    if (length > 0 &&
        length < path.size()) {
        MoveFileExW(
            path.data(),
            nullptr,
            MOVEFILE_DELAY_UNTIL_REBOOT);
    }
}

} // namespace

int WINAPI wWinMain(
    HINSTANCE,
    HINSTANCE,
    PWSTR,
    int) {
    const auto parsed =
        ParseArguments();

    if (!parsed ||
        !ValidateSource(*parsed) ||
        !WaitForParent(
            parsed->parentPid)) {
        return 2;
    }

    const Arguments& args =
        *parsed;

    std::vector<ApplyRecord>
        records;

    if (!ApplyPackage(
            args,
            records)) {
        Rollback(args, records);
        return 3;
    }

    HANDLE healthEvent =
        CreateHealthEvent(
            args.healthEvent);

    if (!healthEvent) {
        Rollback(args, records);
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
        Rollback(args, records);

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
        WaitForSingleObject(
            child.hProcess,
            5000);
        CloseHandle(child.hProcess);

        Rollback(args, records);

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

    std::error_code ec;
    std::filesystem::remove_all(
        args.backup,
        ec);
    ec.clear();
    std::filesystem::remove_all(
        args.source,
        ec);

    CleanupSelfLater();
    return 0;
}
