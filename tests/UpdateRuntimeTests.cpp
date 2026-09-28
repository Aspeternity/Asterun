#include "updater/UpdaterTransaction.hpp"
#ifdef _WIN32
#include "platform/SecureArchive.hpp"
#endif

#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace fs = std::filesystem;
using altrun::updater::ApplyPackage;
using altrun::updater::Rollback;
using altrun::updater::TransactionJournal;
using altrun::updater::TransactionPaths;
using altrun::updater::ValidateSource;

namespace {

#ifdef _WIN32
std::wstring QuoteArgument(
    std::wstring_view value) {
    std::wstring result = L"\"";
    std::size_t slashes = 0;

    for (const wchar_t ch : value) {
        if (ch == L'\\') {
            ++slashes;
            continue;
        }

        if (ch == L'\"') {
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
        result.push_back(ch);
    }

    result.append(
        slashes * 2,
        L'\\');
    result.push_back(L'\"');
    return result;
}

bool CreateJunction(
    const fs::path& junction,
    const fs::path& target,
    const fs::path& workingDirectory) {
    std::wstring command =
        L"cmd.exe /d /c mklink /J " +
        QuoteArgument(
            junction.wstring()) +
        L" " +
        QuoteArgument(
            target.wstring());

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};

    if (!CreateProcessW(
            nullptr,
            command.data(),
            nullptr,
            nullptr,
            FALSE,
            CREATE_NO_WINDOW,
            nullptr,
            workingDirectory.c_str(),
            &startup,
            &process)) {
        return false;
    }

    const DWORD wait =
        WaitForSingleObject(
            process.hProcess,
            5000);
    DWORD exitCode = 1;
    const bool ok =
        wait == WAIT_OBJECT_0 &&
        GetExitCodeProcess(
            process.hProcess,
            &exitCode) &&
        exitCode == 0;

    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return ok;
}
#endif

void Write(
    const fs::path& path,
    std::string_view value) {
    std::error_code ec;
    fs::create_directories(
        path.parent_path(),
        ec);
    assert(!ec);

    std::ofstream output(
        path,
        std::ios::binary |
            std::ios::trunc);
    assert(output);
    output << value;
    output.flush();
    assert(output.good());
}

std::string Read(
    const fs::path& path) {
    std::ifstream input(
        path,
        std::ios::binary);
    assert(input);

    return {
        std::istreambuf_iterator<char>{
            input},
        std::istreambuf_iterator<char>{},
    };
}

void PopulateSource(
    const fs::path& source) {
    Write(
        source / "ALTRunNext.exe",
        "new-app");
    Write(
        source / "Update.exe",
        "new-updater");
    Write(
        source / "Uninstall.exe",
        "new-uninstaller");
    Write(
        source / "VERSION",
        "0.8.0-beta.2\n");
}

void PopulateInstall(
    const fs::path& install) {
    Write(
        install / "ALTRunNext.exe",
        "old-app");
    Write(
        install / "Update.exe",
        "old-updater");
    Write(
        install / "Uninstall.exe",
        "old-uninstaller");
    Write(
        install / "VERSION",
        "0.8.0-beta.1\n");
}

TransactionPaths Paths(
    const fs::path& root) {
    return {
        .source =
            fs::absolute(
                root / "source"),
        .install =
            fs::absolute(
                root / "install"),
        .backup =
            fs::absolute(
                root / "backup"),
        .version =
            L"0.8.0-beta.2",
    };
}

} // namespace

int main() {
    const auto root =
        fs::temp_directory_path() /
        "ALTRunNext-UpdateTransactionTests";

    std::error_code ec;
    fs::remove_all(root, ec);
    ec.clear();
    fs::create_directories(root, ec);
    assert(!ec);

    {
        const auto caseRoot =
            root / "success-rollback";
        const auto paths =
            Paths(caseRoot);

        fs::create_directories(
            paths.source,
            ec);
        assert(!ec);
        fs::create_directories(
            paths.install,
            ec);
        assert(!ec);

        PopulateSource(
            paths.source);
        PopulateInstall(
            paths.install);

        Write(
            paths.source /
                "dict" /
                "new.txt",
            "dictionary");
        Write(
            paths.source /
                "shared" /
                "new.txt",
            "new-shared");
        Write(
            paths.source /
                "data" /
                "must-not-copy.txt",
            "package-data");
        Write(
            paths.install /
                "shared" /
                "keep.txt",
            "keep");
        Write(
            paths.install /
                "data" /
                "user.json",
            "personal");

        assert(ValidateSource(paths));

        TransactionJournal journal;
        assert(
            ApplyPackage(
                paths,
                journal));

        assert(
            Read(
                paths.install /
                "ALTRunNext.exe") ==
            "new-app");
        assert(
            Read(
                paths.install /
                "dict" /
                "new.txt") ==
            "dictionary");
        assert(
            Read(
                paths.install /
                "data" /
                "user.json") ==
            "personal");
        assert(
            !fs::exists(
                paths.install /
                "data" /
                "must-not-copy.txt"));

        assert(Rollback(paths, journal).Complete());

        assert(
            Read(
                paths.install /
                "ALTRunNext.exe") ==
            "old-app");
        assert(
            Read(
                paths.install /
                "Update.exe") ==
            "old-updater");
        assert(
            Read(
                paths.install /
                "Uninstall.exe") ==
            "old-uninstaller");
        assert(
            Read(
                paths.install /
                "VERSION") ==
            "0.8.0-beta.1\n");
        assert(
            !fs::exists(
                paths.install /
                "dict"));
        assert(
            fs::is_directory(
                paths.install /
                "shared"));
        assert(
            Read(
                paths.install /
                "shared" /
                "keep.txt") ==
            "keep");
        assert(
            !fs::exists(
                paths.install /
                "shared" /
                "new.txt"));
        assert(
            Read(
                paths.install /
                "data" /
                "user.json") ==
            "personal");
    }

    {
        const auto caseRoot =
            root / "mid-apply-failure";
        const auto paths =
            Paths(caseRoot);

        fs::create_directories(
            paths.source,
            ec);
        assert(!ec);
        fs::create_directories(
            paths.install,
            ec);
        assert(!ec);

        PopulateSource(
            paths.source);
        PopulateInstall(
            paths.install);

        Write(
            paths.source /
                "00-first.txt",
            "first");
        Write(
            paths.source /
                "99-conflict.txt",
            "cannot-land");
        fs::create_directories(
            paths.install /
                "99-conflict.txt",
            ec);
        assert(!ec);

        TransactionJournal journal;

        assert(
            !ApplyPackage(
                paths,
                journal));
        assert(
            !journal.files.empty());

        assert(Rollback(paths, journal).Complete());

        assert(
            !fs::exists(
                paths.install /
                "00-first.txt"));
        assert(
            fs::is_directory(
                paths.install /
                "99-conflict.txt"));
        assert(
            Read(
                paths.install /
                "ALTRunNext.exe") ==
            "old-app");
    }

    {
        const auto caseRoot =
            root / "source-reparse";
        const auto paths =
            Paths(caseRoot);

        fs::create_directories(
            paths.source,
            ec);
        assert(!ec);
        fs::create_directories(
            paths.install,
            ec);
        assert(!ec);

        PopulateSource(
            paths.source);
        PopulateInstall(
            paths.install);

        const auto external =
            caseRoot / "external";
        fs::create_directories(
            external,
            ec);
        assert(!ec);
        Write(
            external / "outside.txt",
            "outside");

        const auto linked =
            paths.source /
            "linked-dir";

#ifdef _WIN32
        const bool linkCreated =
            CreateJunction(
                linked,
                external,
                caseRoot);
#else
        ec.clear();
        fs::create_directory_symlink(
            external,
            linked,
            ec);
        const bool linkCreated =
            !ec;
#endif

        if (linkCreated) {
            TransactionJournal
                journal;
            assert(
                !ApplyPackage(
                    paths,
                    journal));
            assert(
                !fs::exists(
                    paths.install /
                        "linked-dir"));
            assert(
                Read(
                    external /
                        "outside.txt") ==
                "outside");
        }
    }

#ifdef _WIN32
    {
        const auto archive =
            root / "verified-archive.bin";
        Write(
            archive,
            "abc");

        altrun::win::
            LockedVerifiedFile locked;
        std::uint32_t nativeError = 0;

        assert(
            altrun::win::
                LockAndVerifySha256(
                    fs::absolute(
                        archive),
                    "ba7816bf8f01cfea414140de5dae2223"
                    "b00361a396177a9cb410ff61f20015ad",
                    locked,
                    nativeError));
        assert(locked.Valid());

        HANDLE writer =
            CreateFileW(
                archive.c_str(),
                GENERIC_WRITE,
                FILE_SHARE_READ,
                nullptr,
                OPEN_EXISTING,
                FILE_ATTRIBUTE_NORMAL,
                nullptr);
        assert(
            writer ==
                INVALID_HANDLE_VALUE ||
            writer == nullptr);
        assert(
            GetLastError() ==
                ERROR_SHARING_VIOLATION);

        SetLastError(ERROR_SUCCESS);
        assert(
            !DeleteFileW(
                archive.c_str()));
        assert(
            GetLastError() ==
                ERROR_SHARING_VIOLATION);

        locked.Reset();
        assert(
            DeleteFileW(
                archive.c_str()));
    }
#endif

    {
        const auto paths = Paths(root / "rollback-failure");
        fs::create_directories(paths.source);
        fs::create_directories(paths.install);
        PopulateSource(paths.source);
        PopulateInstall(paths.install);
        TransactionJournal journal;
        assert(ApplyPackage(paths, journal));
        fs::remove(paths.install / "ALTRunNext.exe");
        fs::create_directory(paths.install / "ALTRunNext.exe");
        Write(paths.install / "ALTRunNext.exe" / "block", "blocked");
        const auto result = Rollback(paths, journal);
        assert(!result.Complete());
        assert(result.failures.front().relative == fs::path("ALTRunNext.exe"));
        assert(Read(paths.backup / "ALTRunNext.exe") == "old-app");
        fs::remove_all(paths.install / "ALTRunNext.exe");
        assert(Rollback(paths, journal).Complete());
        assert(Read(paths.install / "ALTRunNext.exe") == "old-app");
    }

    fs::remove_all(root, ec);
    return 0;
}
