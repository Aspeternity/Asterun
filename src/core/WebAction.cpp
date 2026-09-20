#include "WebAction.hpp"

#include "ProviderIds.hpp"

#include <algorithm>
#include <cstdint>
#include <cwctype>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace altrun {
namespace {

constexpr std::wstring_view kQueryPlaceholder = L"{query}";

[[nodiscard]] std::wstring_view Trim(std::wstring_view value) {
    while (!value.empty() && std::iswspace(value.front())) value.remove_prefix(1);
    while (!value.empty() && std::iswspace(value.back())) value.remove_suffix(1);
    return value;
}

[[nodiscard]] bool EqualInsensitive(
    std::wstring_view left,
    std::wstring_view right) {
    if (left.size() != right.size()) return false;
    for (std::size_t i = 0; i < left.size(); ++i) {
        if (std::towlower(left[i]) != std::towlower(right[i])) return false;
    }
    return true;
}

[[nodiscard]] bool StartsWithInsensitive(
    std::wstring_view value,
    std::wstring_view prefix) {
    return value.size() >= prefix.size() &&
        EqualInsensitive(value.substr(0, prefix.size()), prefix);
}

[[nodiscard]] bool HasWhitespace(std::wstring_view value) {
    return std::any_of(value.begin(), value.end(), [](wchar_t ch) {
        return std::iswspace(ch) != 0;
    });
}

[[nodiscard]] bool IsHttpUrl(std::wstring_view value) {
    return StartsWithInsensitive(value, L"http://") ||
        StartsWithInsensitive(value, L"https://");
}

[[nodiscard]] std::optional<std::wstring>
NormalizeDirectUrl(std::wstring_view query) {
    query = Trim(query);
    if (query.empty() || HasWhitespace(query)) return std::nullopt;
    if (IsHttpUrl(query)) return std::wstring(query);
    if (StartsWithInsensitive(query, L"www.")) {
        std::wstring result = L"https://";
        result.append(query);
        return result;
    }
    return std::nullopt;
}

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
        const auto unit =
            static_cast<std::make_unsigned_t<wchar_t>>(
                value[i]);
        std::uint32_t codePoint =
            static_cast<std::uint32_t>(unit);

        if (codePoint >= 0xD800u && codePoint <= 0xDBFFu && i + 1 < value.size()) {
            const auto low = static_cast<std::uint32_t>(value[i + 1]);
            if (low >= 0xDC00u && low <= 0xDFFFu) {
                codePoint = 0x10000u +
                    ((codePoint - 0xD800u) << 10) +
                    (low - 0xDC00u);
                ++i;
            } else {
                codePoint = 0xFFFDu;
            }
        } else if (
            (codePoint >= 0xDC00u && codePoint <= 0xDFFFu) ||
            codePoint > 0x10FFFFu) {
            codePoint = 0xFFFDu;
        }

        AppendUtf8(output, codePoint);
    }

    return output;
}

[[nodiscard]] std::wstring PercentEncodeQuery(std::wstring_view value) {
    constexpr wchar_t kHex[] = L"0123456789ABCDEF";
    const auto utf8 = Utf8Bytes(value);
    std::wstring output;
    output.reserve(utf8.size() * 3);

    for (const unsigned char byte : utf8) {
        const bool unreserved =
            (byte >= 'A' && byte <= 'Z') ||
            (byte >= 'a' && byte <= 'z') ||
            (byte >= '0' && byte <= '9') ||
            byte == '-' || byte == '.' || byte == '_' || byte == '~';

        if (unreserved) {
            output.push_back(static_cast<wchar_t>(byte));
        } else {
            output.push_back(L'%');
            output.push_back(kHex[(byte >> 4) & 0x0F]);
            output.push_back(kHex[byte & 0x0F]);
        }
    }

    return output;
}

[[nodiscard]] std::wstring ResolveQueryTemplate(
    std::wstring_view target,
    std::wstring_view query) {
    std::wstring resolved(target);
    const auto encoded = PercentEncodeQuery(query);
    std::size_t position = 0;

    while ((position = resolved.find(kQueryPlaceholder, position)) !=
           std::wstring::npos) {
        resolved.replace(position, kQueryPlaceholder.size(), encoded);
        position += encoded.size();
    }

    return resolved;
}

[[nodiscard]] bool MatchesAlias(
    const Command& command,
    std::wstring_view token) {
    if (EqualInsensitive(command.keyword, token)) return true;
    return std::any_of(
        command.aliases.begin(),
        command.aliases.end(),
        [&](const std::wstring& alias) {
            return EqualInsensitive(alias, token);
        });
}

} // namespace

std::vector<LauncherResult>
BuildWebActionResults(
    std::span<const Command> commands,
    std::wstring_view query,
    std::size_t limit) {
    std::vector<LauncherResult> results;
    if (limit == 0) return results;

    const auto trimmed = Trim(query);
    if (trimmed.empty()) return results;

    if (const auto direct = NormalizeDirectUrl(trimmed)) {
        LauncherResult result;
        result.id = L"builtin.web:url:" + *direct;
        result.providerId = std::string(providers::kBuiltinWeb);
        result.kind = ResultKind::Action;
        result.title = *direct;
        result.subtitle = L"Open URL";
        result.target = *direct;
        result.detail = *direct;
        result.score = 1200;
        result.action.kind = LauncherActionKind::OpenUrl;
        result.action.payload = *direct;
        results.push_back(std::move(result));
        if (results.size() >= limit) return results;
    }

    const auto separator = trimmed.find_first_of(L" \t\r\n");
    const auto token = separator == std::wstring_view::npos
        ? trimmed
        : trimmed.substr(0, separator);
    const auto tail = separator == std::wstring_view::npos
        ? std::wstring_view{}
        : Trim(trimmed.substr(separator + 1));

    for (std::size_t i = 0;
         i < commands.size() && results.size() < limit;
         ++i) {
        const auto& command = commands[i];

        if (!command.enabled ||
            command.source != CommandSource::User ||
            command.runtimeInputMode !=
                RuntimeInputMode::None ||
            command.type != CommandType::Url ||
            command.target.find(kQueryPlaceholder) == std::wstring::npos ||
            !MatchesAlias(command, token)) {
            continue;
        }

        auto resolved = ResolveQueryTemplate(command.target, tail);
        if (!IsHttpUrl(resolved)) continue;

        LauncherResult result;
        result.id = L"builtin.web:search:" + command.id + L":" +
            PercentEncodeQuery(tail);
        result.providerId = std::string(providers::kBuiltinWeb);
        result.kind = ResultKind::Action;
        result.title = command.title.empty() ? command.keyword : command.title;
        result.subtitle = tail.empty() ? L"Open web search" : std::wstring(tail);
        result.target = std::move(resolved);
        result.iconSource =
            command.icon.empty() ||
            command.icon == L"auto"
                ? command.target
                : command.icon;
        result.detail = result.target;
        result.score = 1180;
        result.action.kind = LauncherActionKind::OpenUrl;
        result.action.commandIndex = i;
        result.action.payload = result.target;
        results.push_back(std::move(result));
    }

    return results;
}

} // namespace altrun
