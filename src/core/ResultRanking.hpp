#pragma once

#include "LauncherResult.hpp"

#include <string_view>

namespace altrun {

[[nodiscard]] int ResultKindWeight(
    ResultKind kind) noexcept;

[[nodiscard]] int ProviderRankWeight(
    std::string_view providerId) noexcept;

[[nodiscard]] int UnifiedRankScore(
    const LauncherResult& result) noexcept;

[[nodiscard]] int ScoreDynamicResultText(
    const LauncherResult& result,
    std::wstring_view query);

} // namespace altrun
