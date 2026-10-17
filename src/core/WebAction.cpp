#include "WebAction.hpp"
#include "UrlEncoding.hpp"

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

// The host part of a scheme-less address: everything before the first
// '/', '?', '#' or ':' (port).
[[nodiscard]] std::wstring_view HostOf(std::wstring_view address) {
    return address.substr(0, address.find_first_of(L"/?#:"));
}

// "baidu.com", "www.github.com": two or more non-empty labels made of
// letters, digits and '-'.
[[nodiscard]] bool IsDomainName(std::wstring_view host) {
    if (host.empty() || host.front() == L'.' || host.back() == L'.' ||
        host.find(L"..") != std::wstring_view::npos ||
        host.find(L'.') == std::wstring_view::npos) {
        return false;
    }
    return std::all_of(host.begin(), host.end(), [](wchar_t ch) {
        return (ch >= L'a' && ch <= L'z') || (ch >= L'A' && ch <= L'Z') ||
            (ch >= L'0' && ch <= L'9') || ch == L'-' || ch == L'.';
    });
}

// Top-level domains accepted without "http://" or "www.". Kept to common
// web suffixes so file names (readme.md, setup.exe, notes.txt) are never
// mistaken for addresses.
[[nodiscard]] bool HasCommonWebSuffix(std::wstring_view host) {
    const auto dot = host.find_last_of(L'.');
    const auto suffix = host.substr(dot + 1);
    constexpr std::wstring_view kSuffixes[]{
        L"com", L"cn", L"net", L"org", L"io", L"dev", L"co", L"me",
        L"info", L"edu", L"gov", L"cc", L"tv", L"top", L"xyz", L"ai"};
    return std::any_of(std::begin(kSuffixes), std::end(kSuffixes),
        [&](std::wstring_view known) {
            return EqualInsensitive(suffix, known);
        });
}

[[nodiscard]] std::optional<std::wstring>
NormalizeDirectUrl(std::wstring_view query) {
    query = Trim(query);
    if (query.empty() || HasWhitespace(query)) return std::nullopt;
    if (IsHttpUrl(query)) {
        // "http://" alone (or with an empty host) is not an address yet.
        const auto rest = query.substr(query.find(L"://") + 3);
        if (HostOf(rest).empty()) return std::nullopt;
        return std::wstring(query);
    }

    const std::wstring_view host = HostOf(query);
    const bool www = StartsWithInsensitive(query, L"www.");
    // "www." needs a real domain after it ("www.baidu.com", not "www." or
    // "www.x"); a bare host needs a common web suffix ("github.com").
    const bool address = www
        ? IsDomainName(host.substr(4))
        : IsDomainName(host) && HasCommonWebSuffix(host);
    if (!address) return std::nullopt;

    std::wstring result = L"https://";
    result.append(query);
    return result;
}

[[nodiscard]] std::wstring ResolveQueryTemplate(
    std::wstring_view target,
    std::wstring_view query) {
    std::wstring resolved(target);
    const auto encoded = PercentEncodeUtf8(query);
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
        result.relevanceMatch = {
            relevance::MatchKind::Exact,
            relevance::MatchField::Target,
            1200,
            false,
        };
        result.surfaceClass =
            LaunchSurfaceClass::Action;
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
            PercentEncodeUtf8(tail);
        result.providerId = std::string(providers::kBuiltinWeb);
        result.kind = ResultKind::Action;
        result.title = command.title.empty() ? command.keyword : command.title;
        result.subtitle = tail.empty() ? L"Open web search" : std::wstring(tail);
        result.target = std::move(resolved);
        result.detail = result.target;
        result.score = 1180;
        result.relevanceMatch = {
            relevance::MatchKind::Exact,
            relevance::MatchField::Keyword,
            1180,
            false,
        };
        result.surfaceClass =
            LaunchSurfaceClass::UserCommand;
        result.pinned = command.pinned;
        result.action.kind = LauncherActionKind::OpenUrl;
        result.action.commandIndex = i;
        result.action.payload = result.target;
        results.push_back(std::move(result));
    }

    return results;
}

} // namespace altrun
