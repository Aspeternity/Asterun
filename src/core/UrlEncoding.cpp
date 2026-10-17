#include "UrlEncoding.hpp"

#include <cstdint>
#include <type_traits>

namespace altrun {
namespace {

void AppendUtf8(std::string& output, std::uint32_t codePoint) {
    if (codePoint <= 0x7Fu) {
        output.push_back(static_cast<char>(codePoint));
    } else if (codePoint <= 0x7FFu) {
        output.push_back(static_cast<char>(0xC0u | (codePoint >> 6)));
        output.push_back(static_cast<char>(0x80u | (codePoint & 0x3Fu)));
    } else if (codePoint <= 0xFFFFu) {
        output.push_back(static_cast<char>(0xE0u | (codePoint >> 12)));
        output.push_back(static_cast<char>(0x80u | ((codePoint >> 6) & 0x3Fu)));
        output.push_back(static_cast<char>(0x80u | (codePoint & 0x3Fu)));
    } else {
        output.push_back(static_cast<char>(0xF0u | (codePoint >> 18)));
        output.push_back(static_cast<char>(0x80u | ((codePoint >> 12) & 0x3Fu)));
        output.push_back(static_cast<char>(0x80u | ((codePoint >> 6) & 0x3Fu)));
        output.push_back(static_cast<char>(0x80u | (codePoint & 0x3Fu)));
    }
}

[[nodiscard]] std::string Utf8Bytes(std::wstring_view value) {
    std::string output;
    output.reserve(value.size() * 2);

    for (std::size_t i = 0; i < value.size(); ++i) {
        std::uint32_t codePoint = static_cast<std::uint32_t>(
            static_cast<std::make_unsigned_t<wchar_t>>(value[i]));

        if (codePoint >= 0xD800u && codePoint <= 0xDBFFu &&
            i + 1 < value.size()) {
            const auto low = static_cast<std::uint32_t>(value[i + 1]);
            if (low >= 0xDC00u && low <= 0xDFFFu) {
                codePoint = 0x10000u +
                    ((codePoint - 0xD800u) << 10) +
                    (low - 0xDC00u);
                ++i;
            } else {
                codePoint = 0xFFFDu;
            }
        } else if ((codePoint >= 0xD800u && codePoint <= 0xDFFFu) ||
                   codePoint > 0x10FFFFu) {
            codePoint = 0xFFFDu;
        }

        AppendUtf8(output, codePoint);
    }

    return output;
}

} // namespace

std::wstring PercentEncodeUtf8(std::wstring_view value) {
    constexpr wchar_t kHex[] = L"0123456789ABCDEF";
    const auto bytes = Utf8Bytes(value);

    std::wstring output;
    output.reserve(bytes.size() * 3);

    for (const unsigned char byte : bytes) {
        const bool unreserved =
            (byte >= 'A' && byte <= 'Z') ||
            (byte >= 'a' && byte <= 'z') ||
            (byte >= '0' && byte <= '9') ||
            byte == '-' || byte == '.' || byte == '_' || byte == '~';

        if (unreserved) {
            output.push_back(static_cast<wchar_t>(byte));
            continue;
        }

        output.push_back(L'%');
        output.push_back(kHex[(byte >> 4) & 0x0F]);
        output.push_back(kHex[byte & 0x0F]);
    }

    return output;
}

} // namespace altrun
