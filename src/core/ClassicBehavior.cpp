#include "ClassicBehavior.hpp"
#include "RelevancePolicy.hpp"

#include <algorithm>
#include <cwctype>

namespace altrun::classic_behavior {

namespace {

[[nodiscard]] bool
IgnoredForPrefix(
    wchar_t ch) noexcept {

    return std::iswspace(ch) != 0 ||
        ch == L'_' ||
        ch == L'-';
}

[[nodiscard]] bool
NormalizedStartsWith(
    std::wstring_view field,
    std::wstring_view query) noexcept {

    if (field.empty() ||
        query.empty()) {
        return false;
    }

    std::size_t fieldIndex = 0;
    std::size_t queryIndex = 0;
    bool matchedAny = false;

    while (true) {
        while (queryIndex < query.size() &&
               IgnoredForPrefix(
                   query[queryIndex])) {
            ++queryIndex;
        }

        if (queryIndex >= query.size()) {
            return matchedAny;
        }

        while (fieldIndex < field.size() &&
               IgnoredForPrefix(
                   field[fieldIndex])) {
            ++fieldIndex;
        }

        if (fieldIndex >= field.size()) {
            return false;
        }

        const wchar_t fieldChar =
            static_cast<wchar_t>(
                std::towlower(
                    field[fieldIndex]));
        const wchar_t queryChar =
            static_cast<wchar_t>(
                std::towlower(
                    query[queryIndex]));

        if (fieldChar != queryChar) {
            return false;
        }

        matchedAny = true;
        ++fieldIndex;
        ++queryIndex;
    }
}

[[nodiscard]] std::wstring_view
BasenameView(
    std::wstring_view path) noexcept {

    const std::size_t separator =
        path.find_last_of(L"\\/");

    return separator ==
            std::wstring_view::npos
        ? path
        : path.substr(separator + 1);
}

[[nodiscard]] bool
CommandContinues(
    const Command& command,
    std::wstring_view query) noexcept {

    if (NormalizedStartsWith(
            command.keyword,
            query) ||
        NormalizedStartsWith(
            command.title,
            query) ||
        NormalizedStartsWith(
            BasenameView(
                command.target),
            query)) {
        return true;
    }

    for (const auto& alias :
         command.aliases) {
        if (NormalizedStartsWith(
                alias,
                query)) {
            return true;
        }
    }

    return false;
}

[[nodiscard]] bool
ResultContinues(
    const LauncherResult& result,
    std::wstring_view query) noexcept {

    return NormalizedStartsWith(
               result.title,
               query) ||
        NormalizedStartsWith(
            BasenameView(
                result.target),
            query);
}

} // namespace

bool StrongNameContinuation(std::wstring_view name, std::wstring_view query) noexcept {
    return NormalizedStartsWith(name, query);
}

std::wstring BuildFilenameContinuationQuery(std::wstring_view query) {
    // Never reinterpret Everything syntax/path expressions as ordinary names.
    if (relevance::HasExplicitSyntax(query) || query.size() > 128) return {};
    std::wstring pattern = L"nopath:regex:\"^";
    bool any = false;
    for (const wchar_t ch : query) {
        if (IgnoredForPrefix(ch)) continue;
        if (ch < L' ' || (ch >= 0xD800 && ch <= 0xDFFF)) return {};
        pattern += L"[\\s_-]*";
        if (ch == L'\"') pattern += L"\\x22";
        else {
            if (std::wstring_view(L"\\.^$|()[]{}*+?").find(ch) != std::wstring_view::npos)
                pattern.push_back(L'\\');
            pattern.push_back(ch);
        }
        any = true;
    }
    pattern += L"\"";
    return any ? pattern : std::wstring{};
}

PendingNumericDecision ResolvePendingNumericIntent(
    ContinuationEvidence evidence, bool probeRequired,
    std::uint64_t elapsedMs) noexcept {
    if (evidence == ContinuationEvidence::Present) return PendingNumericDecision::Text;
    if (probeRequired && evidence == ContinuationEvidence::Unknown)
        return elapsedMs >= kNumericProbeBudgetMs ? PendingNumericDecision::Text : PendingNumericDecision::Wait;
    return elapsedMs >= kNumericIntentGraceMs ? PendingNumericDecision::Execute : PendingNumericDecision::Wait;
}

int QuickLaunchIndexForDigit(
    int digit,
    std::string_view order) noexcept {

    if (digit < 0 || digit > 9) {
        return -1;
    }

    if (order == "zero-to-nine") {
        return digit;
    }

    return digit == 0
        ? 9
        : digit - 1;
}

NumericQuickLaunchDecision
DecideNumericQuickLaunch(
    const NumericQuickLaunchContext&
        context) noexcept {

    if (!context.enabled ||
        context.imeComposing ||
        context.shiftDown ||
        context.winDown) {
        return NumericQuickLaunchDecision::
            Text;
    }

    const bool explicitExecute =
        context.controlDown ||
        context.altDown;

    if (explicitExecute) {
        return context.resultAvailable
            ? NumericQuickLaunchDecision::
                  ExecuteNow
            : NumericQuickLaunchDecision::
                  Text;
    }

    // A bare digit at an empty query must remain usable for modern names such
    // as 7zip, 1Password, 115 and year/version searches. Power users can use
    // Ctrl/Alt+digit when they explicitly want a numbered launch from empty.
    if (context.editingText ||
        context.queryEmpty ||
        !context.resultAvailable ||
        context.recentTextInput ||
        context.strongContinuation) {
        return NumericQuickLaunchDecision::
            Text;
    }

    return NumericQuickLaunchDecision::
        DeferExecute;
}

bool HasStrongCommandContinuation(
    std::span<const Command> commands,
    std::wstring_view query) noexcept {

    if (query.empty()) {
        return false;
    }

    for (const auto& command :
         commands) {
        if (CommandContinues(
                command,
                query)) {
            return true;
        }
    }

    return false;
}

bool HasStrongResultContinuation(
    std::span<const LauncherResult> results,
    std::wstring_view query) noexcept {

    if (query.empty()) {
        return false;
    }

    for (const auto& result :
         results) {
        if (ResultContinues(
                result,
                query)) {
            return true;
        }
    }

    return false;
}

int WrappedSelectionIndex(
    int current,
    int delta,
    std::size_t count,
    bool wrap) noexcept {

    if (count == 0) {
        return -1;
    }

    const int last =
        static_cast<int>(
            count - 1);

    current =
        std::clamp(
            current,
            0,
            last);

    if (!wrap) {
        return std::clamp(
            current + delta,
            0,
            last);
    }

    const int size =
        static_cast<int>(
            count);
    int next =
        (current + delta) %
        size;

    if (next < 0) {
        next += size;
    }

    return next;
}

bool ShouldExecuteSingleResult(
    bool allowImmediateExecution,
    bool enabled,
    bool imeComposing,
    bool queryEmpty,
    bool dynamicQueryPending,
    std::size_t resultCount) noexcept {

    return allowImmediateExecution &&
        enabled &&
        !imeComposing &&
        !queryEmpty &&
        !dynamicQueryPending &&
        resultCount == 1;
}

} // namespace altrun::classic_behavior
