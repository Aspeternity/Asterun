// Test-only stand-in for Asterun.exe, used by tests/UpdaterFailureTests.ps1.
// It is never packaged: the release packaging copies only the three product
// executables, and the package contract rejects any other top-level entry.
//
// Each start appends one line to a record file OUTSIDE the installation and
// its data folder, so recording never changes what the test compares:
//
//   <unique-case-root>\records\launches.txt
//     pid=<pid> integrity=S-1-16-<rid> elevationType=<1|2|3> cmdline=<UTF-8>
//
// The executable must be copied to <unique-case-root>\install\Asterun.exe.
// The harness creates the sibling records directory before launching it.
// Its mode file selects stay (default), stay15, exit0 or exit3. Deriving this path
// from the executable avoids shared TEMP names and environment differences
// when the updater starts the process using the shell token.
// The helper never creates a shared record directory.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <array>
#include <filesystem>
#include <string>
#include <vector>

namespace {

[[nodiscard]] std::string Utf8(const std::wstring& value) {
    if (value.empty()) {
        return {};
    }
    const int size = WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()),
                                         nullptr, 0, nullptr, nullptr);
    std::string result(static_cast<std::size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), size,
                        nullptr, nullptr);
    return result;
}

[[nodiscard]] std::wstring TokenSummary() {
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
        return L"integrity=unknown elevationType=unknown";
    }
    std::wstring integrity = L"unknown";
    DWORD length = 0;
    GetTokenInformation(token, TokenIntegrityLevel, nullptr, 0, &length);
    if (length > 0) {
        std::vector<BYTE> buffer(length);
        if (GetTokenInformation(token, TokenIntegrityLevel, buffer.data(), length, &length)) {
            const auto* label = reinterpret_cast<const TOKEN_MANDATORY_LABEL*>(buffer.data());
            const UCHAR count = *GetSidSubAuthorityCount(label->Label.Sid);
            integrity = L"S-1-16-" + std::to_wstring(*GetSidSubAuthority(label->Label.Sid, count - 1u));
        }
    }
    TOKEN_ELEVATION_TYPE type{};
    DWORD returned = 0;
    std::wstring elevation = L"unknown";
    if (GetTokenInformation(token, TokenElevationType, &type, sizeof(type), &returned)) {
        elevation = std::to_wstring(static_cast<int>(type));
    }
    CloseHandle(token);
    return L"integrity=" + integrity + L" elevationType=" + elevation;
}

[[nodiscard]] std::string ReadMode(const std::filesystem::path& path) {
    HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                              FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return "stay";
    }
    std::array<char, 16> buffer{};
    DWORD read = 0;
    ReadFile(file, buffer.data(), static_cast<DWORD>(buffer.size() - 1), &read, nullptr);
    CloseHandle(file);
    std::string mode(buffer.data(), read);
    while (!mode.empty() && (mode.back() == '\r' || mode.back() == '\n' || mode.back() == ' ')) {
        mode.pop_back();
    }
    return mode;
}

} // namespace

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    std::array<wchar_t, 32768> module{};
    const DWORD moduleLength = GetModuleFileNameW(nullptr, module.data(), static_cast<DWORD>(module.size()));
    if (moduleLength == 0 || moduleLength >= module.size()) {
        return 90;
    }
    const std::filesystem::path executable(std::wstring(module.data(), moduleLength));
    const auto install = executable.parent_path();
    const auto folder = install.parent_path() / L"records";
    const DWORD attributes = GetFileAttributesW(folder.c_str());
    if (install.filename() != L"install" ||
        attributes == INVALID_FILE_ATTRIBUTES ||
        (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0 ||
        (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
        return 91;
    }

    const std::wstring line = L"pid=" + std::to_wstring(GetCurrentProcessId()) + L" " + TokenSummary() +
        L" cmdline=" + GetCommandLineW() + L"\n";
    const std::string bytes = Utf8(line);
    const auto record = folder / L"launches.txt";
    HANDLE file = CreateFileW(record.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                              OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return 92;
    }
    DWORD written = 0;
    const BOOL ok = WriteFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr);
    CloseHandle(file);
    if (!ok || written != bytes.size()) {
        return 93;
    }

    const std::string mode = ReadMode(folder / L"mode");
    if (mode == "exit0") {
        return 0;
    }
    if (mode == "exit3") {
        return 3;
    }
    if (mode == "stay15") {
        // Bounded lifetime for direct restart-observation tests: they can
        // wait for natural exit without finding or terminating a process.
        Sleep(15000);
        return 0;
    }
    Sleep(INFINITE);
    return 0;
}
