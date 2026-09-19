#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace altrun::win {

std::wstring Utf8ToWide(std::string_view text);
std::string WideToUtf8(std::wstring_view text);
std::filesystem::path ExecutableDirectory();
std::wstring ExpandEnvironment(std::wstring_view text);
std::wstring Trim(std::wstring_view text);
std::vector<std::wstring> SplitTabs(std::wstring_view line);
std::wstring Lower(std::wstring_view text);
std::wstring CompactKeyword(std::wstring_view text);
std::int64_t UnixTimeNow();
std::wstring FormatWin32Error(unsigned long errorCode);

} // namespace altrun::win
