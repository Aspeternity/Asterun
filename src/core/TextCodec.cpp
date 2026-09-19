#include "TextCodec.hpp"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <codecvt>
#include <locale>
#endif

namespace altrun::text {

std::string ToUtf8(std::wstring_view value) {
    if (value.empty()) return {};

#ifdef _WIN32
    const int size = WideCharToMultiByte(
        CP_UTF8,
        0,
        value.data(),
        static_cast<int>(value.size()),
        nullptr,
        0,
        nullptr,
        nullptr);

    if (size <= 0) return {};

    std::string out(static_cast<std::size_t>(size), '\0');
    WideCharToMultiByte(
        CP_UTF8,
        0,
        value.data(),
        static_cast<int>(value.size()),
        out.data(),
        size,
        nullptr,
        nullptr);
    return out;
#else
    std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
    return converter.to_bytes(value.data(), value.data() + value.size());
#endif
}

std::wstring FromUtf8(std::string_view value) {
    if (value.empty()) return {};

#ifdef _WIN32
    const int size = MultiByteToWideChar(
        CP_UTF8,
        MB_ERR_INVALID_CHARS,
        value.data(),
        static_cast<int>(value.size()),
        nullptr,
        0);

    if (size <= 0) return {};

    std::wstring out(static_cast<std::size_t>(size), L'\0');
    MultiByteToWideChar(
        CP_UTF8,
        MB_ERR_INVALID_CHARS,
        value.data(),
        static_cast<int>(value.size()),
        out.data(),
        size);
    return out;
#else
    std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
    return converter.from_bytes(value.data(), value.data() + value.size());
#endif
}

} // namespace altrun::text
