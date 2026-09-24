#pragma once

#include "LauncherResult.hpp"

#include <string_view>

namespace altrun {

[[nodiscard]] int ResultKindWeight(
    ResultKind kind) noexcept;

[[nodiscard]] int ProviderRankWeight(
    std::string_view providerId) noexcept;

// Compatibility/debug scalar only. Final ordering is owned by
// BetterLauncherResult and the structured relevance fields.
[[nodiscard]] int UnifiedRankScore(
    const LauncherResult& result) noexcept;

[[nodiscard]] bool RankDynamicResultText(
    LauncherResult& result,
    std::wstring_view query);

[[nodiscard]] int ScoreDynamicResultText(
    const LauncherResult& result,
    std::wstring_view query);

[[nodiscard]] bool BetterLauncherResult(
    const LauncherResult& left,
    const LauncherResult& right) noexcept;

} // namespace altrun
