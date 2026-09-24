#pragma once

#include "LaunchSurface.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace altrun::relevance {

enum class MatchKind {
    None,
    Exact,
    Prefix,
    Initials,
    BoundaryPrefix,
    HybridPinyin,
    Wildcard,
    Substring,
    TightFuzzy,
    Fuzzy,
    SyntaxFallback,
};

enum class MatchField {
    None,
    Keyword,
    Alias,
    Title,
    FileStem,
    Subtitle,
    Target,
};

struct Match {
    MatchKind kind{MatchKind::None};
    MatchField field{MatchField::None};
    int score{0};
    bool pinyin{false};

    [[nodiscard]] explicit
    operator bool() const noexcept {
        return kind != MatchKind::None &&
            score > 0;
    }
};

struct RankContext {
    bool pinned{false};
    bool explicitUser{false};
    Match match;
    LaunchSurfaceClass surface{
        LaunchSurfaceClass::
            PrimaryApplication};
    int usageScore{0};
    int kindWeight{0};
    int providerWeight{0};
};

[[nodiscard]] std::wstring Normalize(
    std::wstring_view text);

[[nodiscard]] std::vector<std::wstring>
QueryTokens(
    std::wstring_view text);

[[nodiscard]] Match MatchText(
    std::wstring_view field,
    std::wstring_view query);

[[nodiscard]] Match MatchInitials(
    std::wstring_view initials,
    std::wstring_view query);

[[nodiscard]] bool HasPathIntent(
    std::wstring_view query);

[[nodiscard]] bool HasExplicitSyntax(
    std::wstring_view query);

[[nodiscard]] bool
ShouldRunDynamicFilesystemQuery(
    std::wstring_view query);

[[nodiscard]] bool AdmitLaunchSurface(
    LaunchSurfaceClass surface,
    std::wstring_view query,
    const Match& match,
    bool explicitSyntax = false);

[[nodiscard]] int MatchKindTier(
    MatchKind kind) noexcept;

[[nodiscard]] int MatchFieldTier(
    MatchField field) noexcept;

[[nodiscard]] int LaunchSurfaceTier(
    LaunchSurfaceClass surface) noexcept;

[[nodiscard]] bool BetterMatch(
    const Match& left,
    const Match& right) noexcept;

// Returns >0 when left ranks ahead of right, <0 when right ranks ahead.
[[nodiscard]] int CompareRankContext(
    const RankContext& left,
    const RankContext& right) noexcept;

} // namespace altrun::relevance
