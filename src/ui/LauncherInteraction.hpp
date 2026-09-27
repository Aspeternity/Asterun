#pragma once

#include "UiMetrics.hpp"

#include <algorithm>
#include <cstddef>

namespace altrun::ui::launcher_interaction {

// Keep the Modern shell stable while an asynchronous filesystem query is
// pending. A new query may expand immediately when its synchronous results
// need more rows, but it does not collapse below the already-visible height
// until the matching dynamic reply settles.
[[nodiscard]] constexpr std::size_t
StableModernVisibleRows(
    std::size_t currentRows,
    std::size_t resultCount,
    bool dynamicQueryPending) noexcept {

    const std::size_t limit =
        kModernCompactLauncherMetrics
            .maxResults;

    const std::size_t current =
        std::min(
            currentRows,
            limit);
    const std::size_t desired =
        std::min(
            resultCount,
            limit);

    return dynamicQueryPending
        ? std::max(
              current,
              desired)
        : desired;
}

// Preserve the user's row intent across asynchronous result replacement. If
// the exact selected identity survives, follow it to its new index. If it
// disappears, keep the nearest valid row instead of unexpectedly jumping to
// the first result.
[[nodiscard]] constexpr int
StableSelectionIndex(
    int previousIndex,
    int matchedIdentityIndex,
    std::size_t resultCount) noexcept {

    if (resultCount == 0) {
        return -1;
    }

    const int last =
        static_cast<int>(
            resultCount - 1);

    if (matchedIdentityIndex >= 0 &&
        matchedIdentityIndex <= last) {
        return matchedIdentityIndex;
    }

    if (previousIndex < 0) {
        return 0;
    }

    return std::clamp(
        previousIndex,
        0,
        last);
}

} // namespace altrun::ui::launcher_interaction
