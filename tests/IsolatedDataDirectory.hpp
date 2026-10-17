#pragma once

#include <windows.h>

#include <cwchar>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace altrun::test {

// Removes "<prefix>-<pid>" directories under %TEMP% left by earlier runs of a
// test whose assertion aborted before its own cleanup. A directory whose PID
// still names a live process is kept: it may be a run in progress.
inline void RemoveAbandonedTestDirectories(std::wstring_view prefix) {
    const auto root = std::filesystem::temp_directory_path();
    const std::wstring head = std::wstring(prefix) + L"-";
    std::vector<std::filesystem::path> abandoned;
    std::error_code error;

    for (std::filesystem::directory_iterator it(root, error), end;
         !error && it != end;
         it.increment(error)) {
        const std::wstring name = it->path().filename().wstring();
        if (name.size() <= head.size() ||
            name.compare(0, head.size(), head) != 0) {
            continue;
        }

        wchar_t* stop = nullptr;
        const unsigned long pid =
            std::wcstoul(name.c_str() + head.size(), &stop, 10);
        if (pid == 0 || !stop || *stop != L'\0' ||
            pid == GetCurrentProcessId()) {
            continue;
        }

        const HANDLE process = OpenProcess(
            PROCESS_QUERY_LIMITED_INFORMATION, FALSE, static_cast<DWORD>(pid));
        if (process) {
            CloseHandle(process);
            continue;
        }
        if (GetLastError() == ERROR_INVALID_PARAMETER) {
            abandoned.push_back(it->path());
        }
    }

    for (const auto& path : abandoned) {
        std::error_code ignored;
        std::filesystem::remove_all(path, ignored);
    }
}

// Runtime tests run from the same folder as the developer's portable build,
// so a real App must never resolve <exe>/data. Each process gets its own
// temporary data directory, removed when the test finishes.
class IsolatedDataDirectory {
public:
    explicit IsolatedDataDirectory(const wchar_t* name)
        : path_(
              std::filesystem::temp_directory_path() /
              (std::wstring(L"asterun-") + name + L"-" +
               std::to_wstring(GetCurrentProcessId()))) {
        RemoveAbandonedTestDirectories(std::wstring(L"asterun-") + name);
        std::error_code error;
        std::filesystem::remove_all(path_, error);
        std::filesystem::create_directories(path_);
    }

    ~IsolatedDataDirectory() {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
    }

    IsolatedDataDirectory(const IsolatedDataDirectory&) = delete;
    IsolatedDataDirectory& operator=(const IsolatedDataDirectory&) = delete;

    [[nodiscard]] const std::filesystem::path& Path() const noexcept {
        return path_;
    }

private:
    std::filesystem::path path_;
};

} // namespace altrun::test
