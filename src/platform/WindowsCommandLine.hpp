#pragma once

#include <string>
#include <string_view>

namespace altrun::win {

// Quote one argument for the Windows CommandLineToArgvW parsing convention.
[[nodiscard]] inline std::wstring QuoteWindowsArgument(std::wstring_view value) {
    std::wstring result = L"\"";
    std::size_t slashes = 0;
    for (const wchar_t ch : value) {
        if (ch == L'\\') {
            ++slashes;
        } else if (ch == L'"') {
            result.append(slashes * 2 + 1, L'\\');
            result.push_back(L'"');
            slashes = 0;
        } else {
            result.append(slashes, L'\\');
            result.push_back(ch);
            slashes = 0;
        }
    }
    result.append(slashes * 2, L'\\');
    result.push_back(L'"');
    return result;
}

} // namespace altrun::win
