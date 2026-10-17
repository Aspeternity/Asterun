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

// System and cache locations whose files only crowd out what the user is
// looking for: the drive-root Windows folder, AppData caches (except
// AppData\Local\Programs), the Recycle Bin, System Volume Information,
// Package Cache, node_modules and .git. Not applied to queries with an
// explicit path or Everything syntax (the user asked for that location).
[[nodiscard]] bool IsNoiseFilesystemPath(
    std::wstring_view fullPath);

// Ranks a filesystem result for `query` and returns false when it should
// not be shown (no admissible match, or a noise path for a plain query).
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
