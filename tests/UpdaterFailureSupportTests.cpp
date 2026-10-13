// Compile the actual internal support functions in a test translation unit.
// The console entry below never calls wWinMain, FinishFailedUpdate or any UI.
// No replacement implementation, product test switch or API interception.
#include "../src/updater/UpdaterMain.cpp"

#include <cassert>
#include <chrono>
#include <iostream>

namespace fs = std::filesystem;

namespace {

void WriteTestFile(const fs::path& path, const std::string& value) {
    std::ofstream stream(path, std::ios::binary);
    stream << value;
    stream.close();
    assert(!stream.fail());
}

DWORD CheckOneRecordedStart(const fs::path& records) {
    std::ifstream stream(records / L"launches.txt", std::ios::binary);
    std::string line, extra;
    assert(std::getline(stream, line));
    assert(!std::getline(stream, extra));
    assert(line.starts_with("pid="));
    const auto commandOffset = line.find(" cmdline=");
    assert(commandOffset != std::string::npos);
    const auto command = line.substr(commandOffset + 9);
    const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
        command.data(), static_cast<int>(command.size()), nullptr, 0);
    assert(length > 0);
    std::wstring wide(static_cast<std::size_t>(length), L'\0');
    assert(MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
        command.data(), static_cast<int>(command.size()), wide.data(), length) == length);
    int count = 0;
    auto* arguments = CommandLineToArgvW(wide.c_str(), &count);
    assert(arguments && count == 1);
    assert(fs::path(arguments[0]).filename() == L"Asterun.exe");
    LocalFree(arguments);
    return static_cast<DWORD>(std::stoul(line.substr(4)));
}

void CheckRestart(const fs::path& root, const fs::path& helper,
                  const std::string& mode, RestartState expected, DWORD exitCode) {
    const auto caseRoot = root / mode;
    const auto install = caseRoot / L"install";
    const auto records = caseRoot / L"records";
    fs::create_directories(install);
    fs::create_directories(records);
    const auto exe = install / L"Asterun.exe";
    fs::copy_file(helper, exe);
    WriteTestFile(records / L"mode", mode);

    const auto before = std::chrono::steady_clock::now();
    const auto observed = RestartPreviousVersion(exe);
    assert(observed.state == expected);
    assert(observed.error == ERROR_SUCCESS);
    const DWORD pid = CheckOneRecordedStart(records);
    if (expected == RestartState::ExitedDuringObservation) {
        assert(observed.exitCode == exitCode);
    } else {
        assert(std::chrono::steady_clock::now() - before >= std::chrono::seconds(5));
        // The helper has a finite lifetime. Observe only the PID it recorded;
        // verify its image before waiting, never terminate or enumerate others.
        HANDLE child = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        assert(child);
        std::array<wchar_t, 32768> image{};
        DWORD size = static_cast<DWORD>(image.size());
        assert(QueryFullProcessImageNameW(child, 0, image.data(), &size));
        assert(fs::equivalent(fs::path(std::wstring(image.data(), size)), exe));
        assert(WaitForSingleObject(child, 20000) == WAIT_OBJECT_0);
        DWORD code = STILL_ACTIVE;
        assert(GetExitCodeProcess(child, &code) && code == 0);
        CloseHandle(child);
    }
    std::cout << "RestartPreviousVersion " << mode << ": PASS; exactly one no-argument start\n";
}

} // namespace

int wmain(int argc, wchar_t** argv) {
    assert(argc == 2);
    const fs::path helper = fs::absolute(argv[1]);
    assert(fs::is_regular_file(helper));
    const auto suffix = std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(GetTickCount64());
    const auto root = fs::temp_directory_path() / (L"asterun-failure-support-" + suffix);
    assert(fs::create_directory(root)); // Never reuse or clear an existing root.

    const auto missing = root / L"missing.exe";
    const auto failed = RestartPreviousVersion(missing);
    assert(failed.state == RestartState::CreateFailed);
    assert(failed.error == ERROR_FILE_NOT_FOUND);
    std::cout << "RestartPreviousVersion missing executable: PASS\n";
    CheckRestart(root, helper, "exit0", RestartState::ExitedDuringObservation, 0);
    CheckRestart(root, helper, "exit3", RestartState::ExitedDuringObservation, 3);
    CheckRestart(root, helper, "stay15", RestartState::RunningAfterObservation, 0);

    assert(QueryPathState(missing) == PathState::Absent);
    assert(QueryPathState(root) == PathState::PlainDirectory);
    assert(QueryPathState(helper) == PathState::Other);
    assert(QueryPathState({}) == PathState::Unknown);
    std::cout << "QueryPathState missing/directory/file/empty: PASS\n";

    const auto eventName = L"Local\\Asterun-failure-support-" + suffix;
    DWORD error = ERROR_INVALID_DATA;
    HANDLE event = CreateHealthEvent(eventName, error);
    assert(event && error == ERROR_SUCCESS);
    assert(SetEvent(event));
    assert(WaitForSingleObject(event, 0) == WAIT_OBJECT_0);
    CloseHandle(event);

    const auto mutexName = eventName + L"-mutex";
    HANDLE mutex = CreateMutexW(nullptr, FALSE, mutexName.c_str());
    assert(mutex && GetLastError() != ERROR_ALREADY_EXISTS);
    error = ERROR_SUCCESS;
    event = CreateHealthEvent(mutexName, error);
    assert(!event && error == ERROR_INVALID_HANDLE);
    CloseHandle(mutex);
    std::cout << "CreateHealthEvent success and object-type collision/error 6: PASS\n";

    fs::remove_all(root);
    std::cout << "Direct support functions only: failure finalization and dialogs NOT exercised.\n";
    return 0;
}
