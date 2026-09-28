// Include the production deletion/validation implementation; tests only touch a
// uniquely named temporary fixture, never a user's registration or service.
#define wWinMain UninstallEntryForTests
#include "../src/uninstaller/UninstallMain.cpp"
#undef wWinMain
#include <cassert>
#include <iostream>

int main() {
    int argc = 0;
    auto argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argc == 2 &&
        std::wstring_view(argv[1]) ==
            L"--owned-everything-fixture") {
        LocalFree(argv);
        Sleep(60000);
        return 0;
    }
    if (argc == 7 && std::wstring_view(argv[1]) == L"--broker-fixture") {
        const std::filesystem::path install(argv[2]);
        EventHandle request, done, exitAllowed, worker;
        request.value = OpenEventW(SYNCHRONIZE, FALSE, argv[3]);
        done.value = OpenEventW(EVENT_MODIFY_STATE, FALSE, argv[4]);
        exitAllowed.value = OpenEventW(SYNCHRONIZE, FALSE, argv[5]);
        worker.value = OpenProcess(SYNCHRONIZE, FALSE, std::stoul(argv[6]));
        LocalFree(argv);
        assert(request.value && done.value && exitAllowed.value && worker.value);
        // Model a directory handle owned by the original uninstaller that only
        // closes on exit. The worker must not wait for DELETE access first.
        DirectoryHandle held;
        held.value = CreateFileW(install.c_str(), FILE_READ_ATTRIBUTES,
            FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS, nullptr);
        assert(held.Valid());
        assert(ServeShellReleaseBroker(request.value, worker.value, install,
            done.value, exitAllowed.value));
        return 0;
    }
    LocalFree(argv);
    const auto base = std::filesystem::temp_directory_path() /
        (L"ALTRun-Uninstall-test-" + std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(GetTickCount64()));
    std::filesystem::create_directories(base);
    const auto makeInstall = [&](std::wstring_view name) {
        auto path = base / name;
        std::filesystem::create_directories(path / L"data");
        for (auto file : {L"Asterun.exe", L"VERSION", L"Uninstall.exe"})
            std::ofstream(path / file) << "fixture";
        assert(ValidateInstallRoot(path));
        return path;
    };

    {
        const auto install =
            makeInstall(
                L"owned everything process");
        const auto ownedDirectory =
            install /
            L"data" /
            L"tools" /
            L"Everything" /
            L"1.4.1.1032-x64";
        std::filesystem::create_directories(
            ownedDirectory);
        const auto ownedExecutable =
            ownedDirectory /
            L"Everything.exe";

        assert(
            CopyFileW(
                CurrentExecutable().c_str(),
                ownedExecutable.c_str(),
                FALSE));

        assert(
            IsOwnedEverythingProcessPath(
                install,
                ownedExecutable,
                false));
        assert(
            IsOwnedEverythingProcessPath(
                install,
                ownedExecutable,
                true));

        const auto protectedRoot =
            ManagedEverythingServiceHostRoot();

        if (!protectedRoot.empty()) {
            const auto protectedExecutable =
                protectedRoot /
                L"1.4.1.1032-x64" /
                L"Everything.exe";

            assert(
                !IsOwnedEverythingProcessPath(
                    install,
                    protectedExecutable,
                    false));
            assert(
                IsOwnedEverythingProcessPath(
                    install,
                    protectedExecutable,
                    true));
        }

        assert(
            !IsOwnedEverythingProcessPath(
                install,
                base /
                    L"external" /
                    L"Everything.exe",
                true));

        std::wstring command =
            QuoteArgument(
                ownedExecutable.wstring()) +
            L" --owned-everything-fixture";

        STARTUPINFOW startup{
            sizeof(startup)};
        PROCESS_INFORMATION process{};

        assert(
            CreateProcessW(
                nullptr,
                command.data(),
                nullptr,
                nullptr,
                FALSE,
                CREATE_NO_WINDOW,
                nullptr,
                ownedDirectory.c_str(),
                &startup,
                &process));

        CloseHandle(
            process.hThread);

        // Give the copied fixture time to enter its wait loop before the
        // uninstaller snapshots processes.
        Sleep(250);

        assert(
            TerminateManagedEverythingProcesses(
                install,
                false));
        assert(
            WaitForSingleObject(
                process.hProcess,
                5000) ==
            WAIT_OBJECT_0);

        CloseHandle(
            process.hProcess);

        RemovalFailure cleanup;
        assert(
            RemoveAllWithRetry(
                install,
                cleanup));
    }

    {
        const auto install = makeInstall(L"只读文件 preserve");
        const auto user = install / L"data" / L"commands.json";
        std::ofstream(user) << "keep me";
        const auto readonly = install / L"readonly.txt";
        std::ofstream(readonly) << "read only";
        assert(SetFileAttributesW(readonly.c_str(), FILE_ATTRIBUTE_READONLY));
        RemovalFailure failure;
        assert(RemoveInstallation(install, false, failure, nullptr));
        assert(std::filesystem::exists(user));
        assert(!std::filesystem::exists(readonly));
        assert(!std::filesystem::exists(install / kRecoveryMarker));
    }
    {
        const auto install = makeInstall(L"locked retry");
        const auto locked = install / L"locked.bin";
        std::ofstream(locked) << "lock";
        HANDLE handle = CreateFileW(locked.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        assert(handle != INVALID_HANDLE_VALUE);
        RemovalFailure failure;
        assert(!RemoveInstallation(install, false, failure, nullptr));
        assert(failure.path == locked && failure.error == ERROR_SHARING_VIOLATION);
        assert(ValidateInstallRoot(install));
        assert(HasRecoveryMarker(install));
        CloseHandle(handle);
        assert(RemoveInstallation(install, false, failure, nullptr));
    }
    {
        const auto install = makeInstall(L"broker exit before lease");
        PerformArguments args;
        args.install = install;
        args.deleteData = true;
        const auto prefix = L"Local\\ALTRun-Uninstall-test-" + std::to_wstring(GetCurrentProcessId());
        args.shellReleaseRequest = prefix + L"-request";
        args.shellReleaseDone = prefix + L"-done";
        args.shellLeaseAcquired = prefix + L"-exit";
        EventHandle request, done, exitAllowed;
        request.value = CreateEventW(nullptr, TRUE, FALSE, args.shellReleaseRequest.c_str());
        done.value = CreateEventW(nullptr, TRUE, FALSE, args.shellReleaseDone.c_str());
        exitAllowed.value = CreateEventW(nullptr, TRUE, FALSE, args.shellLeaseAcquired.c_str());
        assert(request.value && done.value && exitAllowed.value);
        auto command = QuoteArgument(CurrentExecutable().wstring()) + L" --broker-fixture " +
            QuoteArgument(install.wstring()) + L" " + QuoteArgument(args.shellReleaseRequest) +
            L" " + QuoteArgument(args.shellReleaseDone) + L" " + QuoteArgument(args.shellLeaseAcquired) +
            L" " + std::to_wstring(GetCurrentProcessId());
        STARTUPINFOW startup{sizeof(startup)};
        PROCESS_INFORMATION process{};
        assert(CreateProcessW(nullptr, command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW,
            nullptr, install.c_str(), &startup, &process));
        args.parentPid = process.dwProcessId;
        DirectoryHandle lease;
        RemovalFailure failure;
        assert(RequestShellRelease(args, lease, failure));
        DWORD code = 1;
        assert(GetExitCodeProcess(process.hProcess, &code) && code == 0);
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
        assert(RemoveInstallation(install, true, failure, &lease));
        assert(!std::filesystem::exists(install));
    }
    {
        const auto install = makeInstall(L"readonly root");
        assert(SetFileAttributesW(install.c_str(), FILE_ATTRIBUTE_READONLY));
        DirectoryHandle lease;
        RemovalFailure failure;
        assert(AcquireDirectoryDeleteLease(install, lease, failure, 100));
        assert(RemoveInstallation(install, true, failure, &lease));
        assert(!std::filesystem::exists(install));
    }
    {
        const auto install = makeInstall(L"partial retry");
        assert(WriteRecoveryMarker(install));
        std::filesystem::remove(install / L"Asterun.exe");
        std::filesystem::remove(install / L"VERSION");
        assert(ValidateInstallRoot(install));
        DirectoryHandle lease;
        RemovalFailure failure;
        assert(AcquireDirectoryDeleteLease(install, lease, failure, 100));
        assert(RemoveInstallation(install, true, failure, &lease));
        assert(!std::filesystem::exists(install));
    }
    {
        const auto install = makeInstall(L"final failure retry");
        RemovalFailure failure;
        // Simulate failure at the final root operation, after the anchors are gone.
        assert(!RemoveInstallation(install, true, failure, nullptr));
        assert(failure.error == ERROR_INVALID_HANDLE);
        assert(HasRecoveryMarker(install) && ValidateInstallRoot(install));
        DirectoryHandle lease;
        assert(AcquireDirectoryDeleteLease(install, lease, failure, 100));
        assert(RemoveInstallation(install, true, failure, &lease));
    }
    {
        // Windows junction creation does not need the symlink privilege.
        const auto install = makeInstall(L"junction");
        const auto outside = base / L"outside";
        std::filesystem::create_directories(outside);
        const auto sentinel = outside / L"keep.txt";
        std::ofstream(sentinel) << "outside installation";
        const auto junction = install / L"linked";
        std::wstring command = L"cmd.exe /d /c mklink /J " + QuoteArgument(junction.wstring()) + L" " + QuoteArgument(outside.wstring());
        STARTUPINFOW startup{sizeof(startup)};
        PROCESS_INFORMATION process{};
        assert(CreateProcessW(nullptr, command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW,
            nullptr, base.c_str(), &startup, &process));
        assert(WaitForSingleObject(process.hProcess, 5000) == WAIT_OBJECT_0);
        DWORD code = 1;
        assert(GetExitCodeProcess(process.hProcess, &code) && code == 0);
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
        assert(!ValidateInstallRoot(junction));
        RemovalFailure failure;
        assert(RemoveAllWithRetry(junction, failure));
        assert(std::filesystem::exists(sentinel));
    }
    RemovalFailure failure;
    assert(RemoveAllWithRetry(base, failure));
    std::cout << "Uninstall production cleanup regressions passed\n";
}
