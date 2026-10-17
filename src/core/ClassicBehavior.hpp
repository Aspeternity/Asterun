#pragma once

#include "Command.hpp"
#include "LauncherResult.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <string>

namespace altrun::classic_behavior {

inline constexpr std::uint64_t
    kNumericTypingWindowMs = 420;

inline constexpr std::uint64_t
    kNumericIntentGraceMs = 90;

// Unknown includes unavailable, timeout, unsupported syntax and truncated evidence.
enum class ContinuationEvidence { Unknown, Present, Absent };
inline constexpr std::uint64_t kNumericProbeBudgetMs = 240;
[[nodiscard]] bool StrongNameContinuation(std::wstring_view name, std::wstring_view query) noexcept;
[[nodiscard]] std::wstring BuildFilenameContinuationQuery(std::wstring_view query);
enum class PendingNumericDecision { Wait, Text, Execute };
[[nodiscard]] PendingNumericDecision ResolvePendingNumericIntent(
    ContinuationEvidence evidence, bool probeRequired,
    std::uint64_t elapsedMs) noexcept;

enum class NumericQuickLaunchDecision {
    Text,
    ExecuteNow,
    DeferExecute,
};

struct NumericQuickLaunchContext {
    bool enabled{false};
    bool imeComposing{false};
    bool controlDown{false};
    bool altDown{false};
    bool shiftDown{false};
    bool winDown{false};
    bool queryEmpty{true};
    bool recentTextInput{false};
    bool strongContinuation{false};
    bool resultAvailable{false};
    bool editingText{false};
    // The query ends with whitespace: the user is starting another word
    // ("photoshop 2024", "g iphone 15"), so a bare digit is text.
    bool afterSeparator{false};
    // The visible results include an action that consumes the typed text
    // (web search/URL, copy text, shortcut runtime input): digits belong to
    // that argument.
    bool typingArgument{false};
};

// Results whose action is built from the text being typed: built-in web
// search/URL and copy-text actions, and user shortcuts receiving runtime
// input. They are not a stable target while the user is still typing.
[[nodiscard]] bool ConsumesTypedText(
    const LauncherResult& result) noexcept;

[[nodiscard]] int QuickLaunchIndexForDigit(
    int digit,
    std::string_view order) noexcept;

[[nodiscard]] NumericQuickLaunchDecision
DecideNumericQuickLaunch(
    const NumericQuickLaunchContext&
        context) noexcept;

[[nodiscard]] bool
HasStrongCommandContinuation(
    std::span<const Command> commands,
    std::wstring_view query) noexcept;

[[nodiscard]] bool
HasStrongResultContinuation(
    std::span<const LauncherResult> results,
    std::wstring_view query);

[[nodiscard]] int WrappedSelectionIndex(
    int current,
    int delta,
    std::size_t count,
    bool wrap) noexcept;

// Whether the one remaining result may be launched without Enter: it must
// be a clear match (exact, name/word start, initials, pinyin) and not an
// action built from text the user is still typing (web search, URL, copy
// text, shortcut runtime input).
[[nodiscard]] bool SingleResultAutoExecutable(
    const LauncherResult& result) noexcept;

[[nodiscard]] bool ShouldExecuteSingleResult(
    bool allowImmediateExecution,
    bool enabled,
    bool imeComposing,
    bool queryEmpty,
    bool dynamicQueryPending,
    std::size_t resultCount) noexcept;

} // namespace altrun::classic_behavior
