#include "ClipboardAction.hpp"

#include "ProviderIds.hpp"

#include <cwctype>
#include <string>
#include <string_view>
#include <utility>

namespace altrun {
namespace {

[[nodiscard]] std::wstring_view
Trim(std::wstring_view value) {
    while (!value.empty() &&
           std::iswspace(
               value.front())) {
        value.remove_prefix(1);
    }

    while (!value.empty() &&
           std::iswspace(
               value.back())) {
        value.remove_suffix(1);
    }

    return value;
}

[[nodiscard]] bool
EqualInsensitive(
    std::wstring_view left,
    std::wstring_view right) {
    if (left.size() != right.size()) {
        return false;
    }

    for (std::size_t i = 0;
         i < left.size();
         ++i) {
        if (std::towlower(left[i]) !=
            std::towlower(right[i])) {
            return false;
        }
    }

    return true;
}

[[nodiscard]] bool
IsCopyAlias(
    std::wstring_view alias) {
    return
        EqualInsensitive(
            alias,
            L"copy") ||
        EqualInsensitive(
            alias,
            L"clip") ||
        alias == L"复制";
}

} // namespace

std::vector<LauncherResult>
BuildClipboardActionResults(
    std::wstring_view query,
    std::size_t limit,
    std::wstring_view actionTitle) {
    std::vector<LauncherResult>
        results;

    if (limit == 0) {
        return results;
    }

    query = Trim(query);

    if (query.empty()) {
        return results;
    }

    const auto separator =
        query.find_first_of(
            L" \t");

    if (separator ==
        std::wstring_view::npos) {
        return results;
    }

    const auto alias =
        query.substr(
            0,
            separator);

    if (!IsCopyAlias(alias)) {
        return results;
    }

    auto payload =
        Trim(
            query.substr(
                separator + 1));

    if (payload.empty()) {
        return results;
    }

    LauncherResult result;
    result.id =
        L"builtin.clipboard:copy-text";
    result.providerId =
        std::string(
            providers::
                kBuiltinClipboard);
    result.kind =
        ResultKind::Action;
    result.title =
        actionTitle.empty()
            ? L"Copy text"
            : std::wstring(
                  actionTitle);
    result.subtitle =
        std::wstring(payload);
    result.target =
        std::wstring(payload);
    result.detail =
        std::wstring(payload);
    result.score = 1500;
    result.relevanceMatch = {
        relevance::MatchKind::Exact,
        relevance::MatchField::Keyword,
        1500,
        false,
    };
    result.surfaceClass =
        LaunchSurfaceClass::Action;
    result.action.kind =
        LauncherActionKind::
            CopyText;
    result.action.payload =
        std::wstring(payload);

    results.push_back(
        std::move(result));

    return results;
}

} // namespace altrun
