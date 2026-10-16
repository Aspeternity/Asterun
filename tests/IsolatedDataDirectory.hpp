#pragma once

#include <windows.h>

#include <filesystem>
#include <string>
#include <system_error>

namespace altrun::test {

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
