#include "WinUtil.hpp"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <cwctype>
#include <stdexcept>

namespace altrun::win {

std::wstring Utf8ToWide(std::string_view text) {
    if (text.empty()) return {};
    const int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), nullptr, 0);
    if (size <= 0) return {};
    std::wstring out(static_cast<std::size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), out.data(), size);
    return out;
}

std::string WideToUtf8(std::wstring_view text) {
    if (text.empty()) return {};
    const int size = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    if (size <= 0) return {};
    std::string out(static_cast<std::size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), out.data(), size, nullptr, nullptr);
    return out;
}

std::filesystem::path ExecutableDirectory() {
    std::wstring buffer(32768, L'\0');
    const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (length == 0 || length >= buffer.size()) {
        throw std::runtime_error("GetModuleFileNameW failed");
    }
    buffer.resize(length);
    return std::filesystem::path(buffer).parent_path();
}

std::wstring ExpandEnvironment(std::wstring_view text) {
    if (text.empty()) return {};
    const DWORD required = ExpandEnvironmentStringsW(std::wstring(text).c_str(), nullptr, 0);
    if (required == 0) return std::wstring(text);
    std::wstring out(required, L'\0');
    const DWORD written = ExpandEnvironmentStringsW(std::wstring(text).c_str(), out.data(), required);
    if (written == 0) return std::wstring(text);
    if (!out.empty() && out.back() == L'\0') out.pop_back();
    return out;
}

std::wstring Trim(std::wstring_view text) {
    std::size_t start = 0;
    std::size_t end = text.size();
    while (start < end && std::iswspace(text[start])) ++start;
    while (end > start && std::iswspace(text[end - 1])) --end;
    return std::wstring(text.substr(start, end - start));
}

std::vector<std::wstring> SplitTabs(std::wstring_view line) {
    std::vector<std::wstring> fields;
    std::size_t start = 0;
    while (start <= line.size()) {
        const auto pos = line.find(L'\t', start);
        if (pos == std::wstring_view::npos) {
            fields.emplace_back(line.substr(start));
            break;
        }
        fields.emplace_back(line.substr(start, pos - start));
        start = pos + 1;
    }
    return fields;
}

std::wstring Lower(std::wstring_view text) {
    std::wstring out(text);
    std::transform(out.begin(), out.end(), out.begin(), [](wchar_t c) {
        return static_cast<wchar_t>(std::towlower(c));
    });
    return out;
}

std::wstring CompactKeyword(std::wstring_view text) {
    std::wstring out;
    out.reserve(text.size());
    for (wchar_t c : text) {
        if (std::iswalnum(c) || c >= 0x4E00) {
            out.push_back(static_cast<wchar_t>(std::towlower(c)));
        }
    }
    return out;
}

std::int64_t UnixTimeNow() {
    FILETIME ft{};
    GetSystemTimeAsFileTime(&ft);
    ULARGE_INTEGER value{};
    value.LowPart = ft.dwLowDateTime;
    value.HighPart = ft.dwHighDateTime;
    constexpr unsigned long long kEpochDiff100ns = 116444736000000000ULL;
    return static_cast<std::int64_t>((value.QuadPart - kEpochDiff100ns) / 10000000ULL);
}

std::wstring FormatWin32Error(unsigned long errorCode) {
    wchar_t* buffer = nullptr;
    const DWORD length = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        errorCode,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<wchar_t*>(&buffer),
        0,
        nullptr);

    std::wstring message;
    if (length != 0 && buffer != nullptr) {
        message.assign(buffer, length);
        LocalFree(buffer);
    }
    return Trim(message);
}

} // namespace altrun::win
