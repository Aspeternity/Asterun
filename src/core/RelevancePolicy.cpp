#include "RelevancePolicy.hpp"

#include <algorithm>
#include <cwctype>
#include <initializer_list>

namespace altrun::relevance {
namespace {

[[nodiscard]] bool IsAsciiQuery(
    std::wstring_view query) {
    return !query.empty() &&
        std::all_of(
            query.begin(),
            query.end(),
            [](wchar_t ch) {
                return ch <= 0x7F;
            });
}

[[nodiscard]] bool IsWordBoundary(
    std::wstring_view field,
    std::size_t index) {

    if (index >= field.size()) {
        return false;
    }

    if (index == 0) {
        return true;
    }

    const wchar_t current = field[index];
    const wchar_t previous =
        field[index - 1];

    if (!std::iswalnum(previous)) {
        return true;
    }

    const bool currentDigit =
        std::iswdigit(current) != 0;
    const bool previousDigit =
        std::iswdigit(previous) != 0;

    if (currentDigit != previousDigit) {
        return true;
    }

    if (current > 0x7F ||
        previous > 0x7F) {
        return false;
    }

    const bool currentUpper =
        std::iswupper(current) != 0;
    const bool previousLower =
        std::iswlower(previous) != 0;
    const bool previousUpper =
        std::iswupper(previous) != 0;
    const bool nextLower =
        index + 1 < field.size() &&
        field[index + 1] <= 0x7F &&
        std::iswlower(
            field[index + 1]) != 0;

    return currentUpper &&
        (previousLower ||
         (previousUpper &&
          nextLower));
}

[[nodiscard]] bool NormalizedPrefixAt(
    std::wstring_view field,
    std::size_t start,
    std::wstring_view query) {

    std::size_t fieldIndex = start;
    std::size_t queryIndex = 0;

    while (fieldIndex < field.size() &&
           queryIndex < query.size()) {

        const wchar_t ch =
            field[fieldIndex++];

        if (std::iswspace(ch) ||
            ch == L'_' ||
            ch == L'-') {
            continue;
        }

        if (static_cast<wchar_t>(
                std::towlower(ch)) !=
            query[queryIndex]) {
            return false;
        }

        ++queryIndex;
    }

    return queryIndex == query.size();
}

[[nodiscard]] bool StrongShortMatch(
    MatchKind kind) noexcept {
    switch (kind) {
    case MatchKind::Exact:
    case MatchKind::Prefix:
    case MatchKind::Initials:
    case MatchKind::BoundaryPrefix:
    case MatchKind::HybridPinyin:
    case MatchKind::Wildcard:
        return true;
    default:
        return false;
    }
}

[[nodiscard]] bool VeryStrongMatch(
    MatchKind kind) noexcept {
    switch (kind) {
    case MatchKind::Exact:
    case MatchKind::Prefix:
    case MatchKind::BoundaryPrefix:
    case MatchKind::Wildcard:
        return true;
    default:
        return false;
    }
}

[[nodiscard]] Match MatchPreparedText(
    std::wstring_view boundaryField,
    std::wstring_view normalizedField,
    std::wstring_view normalizedQuery) {

    if (normalizedField.empty() ||
        normalizedQuery.empty()) {
        return {};
    }

    if (normalizedField ==
        normalizedQuery) {
        return {
            MatchKind::Exact,
            MatchField::None,
            1000,
            false,
        };
    }

    if (normalizedField.starts_with(
            normalizedQuery)) {
        return {
            MatchKind::Prefix,
            MatchField::None,
            880 -
                static_cast<int>(
                    std::min<std::size_t>(
                        normalizedField.size() -
                            normalizedQuery.size(),
                        120)),
            false,
        };
    }

    const bool asciiQuery =
        IsAsciiQuery(
            normalizedQuery);

    if (!asciiQuery ||
        normalizedQuery.size() >= 3) {

        for (std::size_t i = 1;
             i < boundaryField.size();
             ++i) {
            if (IsWordBoundary(
                    boundaryField,
                    i) &&
                NormalizedPrefixAt(
                    boundaryField,
                    i,
                    normalizedQuery)) {
                return {
                    MatchKind::
                        BoundaryPrefix,
                    MatchField::None,
                    760 -
                        static_cast<int>(
                            std::min<
                                std::size_t>(
                                i,
                                100)),
                    false,
                };
            }
        }
    }

    if (!asciiQuery ||
        normalizedQuery.size() >= 3) {
        const auto position =
            normalizedField.find(
                normalizedQuery);

        if (position !=
            std::wstring::npos) {
            return {
                MatchKind::Substring,
                MatchField::None,
                690 -
                    static_cast<int>(
                        std::min<
                            std::size_t>(
                            position,
                            100)),
                false,
            };
        }
    }

    if (!asciiQuery ||
        normalizedQuery.size() < 3) {
        return {};
    }

    std::size_t queryIndex = 0;
    int gaps = 0;
    int run = 0;
    int bestRun = 0;
    std::size_t first = 0;
    std::size_t previous = 0;
    bool havePrevious = false;

    for (std::size_t i = 0;
         i < normalizedField.size() &&
         queryIndex <
             normalizedQuery.size();
         ++i) {

        if (normalizedField[i] !=
            normalizedQuery[queryIndex]) {
            continue;
        }

        if (!havePrevious) {
            first = i;
            run = 1;
        } else if (
            i == previous + 1) {
            ++run;
        } else {
            gaps +=
                static_cast<int>(
                    i - previous - 1);
            run = 1;
        }

        bestRun =
            std::max(bestRun, run);

        previous = i;
        havePrevious = true;
        ++queryIndex;
    }

    if (queryIndex !=
        normalizedQuery.size()) {
        return {};
    }

    const int fuzzyScore =
        430 +
        bestRun * 18 -
        gaps * 12;

    if (normalizedQuery.size() == 3) {
        const std::size_t span =
            previous - first + 1;

        if (gaps > 2 ||
            span >
                normalizedQuery.size() + 2 ||
            fuzzyScore < 320) {
            return {};
        }

        return {
            MatchKind::TightFuzzy,
            MatchField::None,
            fuzzyScore,
            false,
        };
    }

    if (fuzzyScore < 260) {
        return {};
    }

    return {
        MatchKind::Fuzzy,
        MatchField::None,
        fuzzyScore,
        false,
    };
}

[[nodiscard]] Match MatchPreparedInitials(
    std::wstring_view normalizedInitials,
    std::wstring_view normalizedQuery) {

    if (normalizedInitials.empty() ||
        normalizedQuery.empty()) {
        return {};
    }

    if (normalizedInitials ==
        normalizedQuery) {
        return {
            MatchKind::Initials,
            MatchField::None,
            965,
            false,
        };
    }

    if (normalizedInitials.starts_with(
            normalizedQuery)) {
        return {
            MatchKind::Initials,
            MatchField::None,
            845 -
                static_cast<int>(
                    std::min<std::size_t>(
                        normalizedInitials.size() -
                            normalizedQuery.size(),
                        120)),
            false,
        };
    }

    return {};
}

} // namespace

std::wstring Normalize(
    std::wstring_view text) {
    std::wstring result;
    result.reserve(text.size());

    for (const wchar_t ch : text) {
        if (std::iswspace(ch) ||
            ch == L'_' ||
            ch == L'-') {
            continue;
        }

        result.push_back(
            static_cast<wchar_t>(
                std::towlower(ch)));
    }

    return result;
}

std::vector<std::wstring>
QueryTokens(
    std::wstring_view text) {

    std::vector<std::wstring> tokens;
    std::wstring current;

    auto flush = [&]() {
        std::wstring normalized =
            Normalize(current);
        current.clear();

        if (normalized.empty()) {
            return;
        }

        if (std::find(
                tokens.begin(),
                tokens.end(),
                normalized) ==
            tokens.end()) {
            tokens.push_back(
                std::move(normalized));
        }
    };

    for (const wchar_t ch : text) {
        if (std::iswspace(ch)) {
            flush();
        } else {
            current.push_back(ch);
        }
    }

    flush();
    return tokens;
}

Match MatchText(
    std::wstring_view field,
    std::wstring_view query) {

    if (field.empty() ||
        query.empty()) {
        return {};
    }

    const std::wstring normalizedQuery =
        Normalize(query);

    return MatchTextNormalizedQuery(
        field,
        normalizedQuery);
}

Match MatchTextNormalizedQuery(
    std::wstring_view field,
    std::wstring_view normalizedQuery) {

    if (field.empty() ||
        normalizedQuery.empty()) {
        return {};
    }

    const std::wstring normalizedField =
        Normalize(field);

    return MatchPreparedText(
        field,
        normalizedField,
        normalizedQuery);
}

Match MatchNormalizedText(
    std::wstring_view normalizedField,
    std::wstring_view normalizedQuery) {

    return MatchPreparedText(
        normalizedField,
        normalizedField,
        normalizedQuery);
}

Match MatchInitials(
    std::wstring_view initials,
    std::wstring_view query) {

    if (initials.empty() ||
        query.empty()) {
        return {};
    }

    const std::wstring normalizedQuery =
        Normalize(query);

    return MatchInitialsNormalizedQuery(
        initials,
        normalizedQuery);
}

Match MatchInitialsNormalizedQuery(
    std::wstring_view initials,
    std::wstring_view normalizedQuery) {

    if (initials.empty() ||
        normalizedQuery.empty()) {
        return {};
    }

    const std::wstring normalizedInitials =
        Normalize(initials);

    return MatchPreparedInitials(
        normalizedInitials,
        normalizedQuery);
}

Match MatchNormalizedInitials(
    std::wstring_view normalizedInitials,
    std::wstring_view normalizedQuery) {

    return MatchPreparedInitials(
        normalizedInitials,
        normalizedQuery);
}

bool HasPathIntent(
    std::wstring_view query) {

    if (query.find_first_of(
            L"\\/") !=
        std::wstring_view::npos) {
        return true;
    }

    const std::wstring lower =
        Normalize(query);

    if (lower.size() >= 2 &&
        std::iswalpha(lower[0]) &&
        lower[1] == L':') {
        return true;
    }

    for (const std::wstring_view extension :
         {
             L".exe",
             L".com",
             L".bat",
             L".cmd",
             L".lnk",
         }) {
        if (lower.find(extension) !=
            std::wstring::npos) {
            return true;
        }
    }

    return false;
}

bool HasExplicitSyntax(
    std::wstring_view query) {

    return
        query.find_first_of(L"*?") !=
            std::wstring_view::npos ||
        query.find(L':') !=
            std::wstring_view::npos ||
        HasPathIntent(query);
}

bool ShouldRunDynamicFilesystemQuery(
    std::wstring_view query) {

    if (HasExplicitSyntax(query)) {
        return true;
    }

    return Normalize(query).size() >= 2;
}

bool AdmitLaunchSurface(
    LaunchSurfaceClass surface,
    std::wstring_view query,
    const Match& match,
    bool explicitSyntax) {

    const bool pathIntent =
        HasPathIntent(query);

    return AdmitLaunchSurfaceNormalized(
        surface,
        Normalize(query),
        match,
        explicitSyntax ||
            pathIntent);
}

bool AdmitLaunchSurfaceNormalized(
    LaunchSurfaceClass surface,
    std::wstring_view normalizedQuery,
    const Match& match,
    bool explicitSyntax) {

    if (normalizedQuery.empty()) {
        return
            surface ==
                LaunchSurfaceClass::
                    UserCommand ||
            surface ==
                LaunchSurfaceClass::
                    PrimaryApplication;
    }

    if (!match) {
        return false;
    }

    if (explicitSyntax) {
        return true;
    }

    const std::size_t length =
        normalizedQuery.size();

    // The two-character strong-match gate exists to suppress noisy short
    // ASCII filesystem/tool queries such as "he". Applying the same gate to
    // CJK text rejects valid contiguous substrings (for example searching
    // "男主" inside "系统男主"), because those naturally classify as
    // Substring rather than Prefix. Keep the precision gate ASCII-only.
    const bool requireStrongShortMatch =
        length <= 2 &&
        IsAsciiQuery(
            normalizedQuery);

    switch (surface) {
    case LaunchSurfaceClass::UserCommand:
    case LaunchSurfaceClass::
        PrimaryApplication:
    case LaunchSurfaceClass::Action:
        return true;

    case LaunchSurfaceClass::SystemUtility:
        return length >= 2 &&
            (!requireStrongShortMatch ||
             StrongShortMatch(
                 match.kind));

    case LaunchSurfaceClass::DeveloperTool:
    case LaunchSurfaceClass::CommandLineTool:
        return length >= 2 &&
            (!requireStrongShortMatch ||
             StrongShortMatch(
                 match.kind));

    case LaunchSurfaceClass::Auxiliary:
    case LaunchSurfaceClass::Maintenance:
        return length >= 3 &&
            VeryStrongMatch(
                match.kind);

    case LaunchSurfaceClass::FilesystemItem:
        return length >= 2 &&
            (!requireStrongShortMatch ||
             StrongShortMatch(
                 match.kind));
    }

    return false;
}

int MatchKindTier(
    MatchKind kind) noexcept {

    switch (kind) {
    case MatchKind::Exact:
        return 100;
    case MatchKind::Prefix:
        return 90;
    case MatchKind::Initials:
        return 85;
    case MatchKind::BoundaryPrefix:
        return 82;
    case MatchKind::HybridPinyin:
        return 80;
    case MatchKind::Wildcard:
        return 78;
    case MatchKind::Substring:
        return 65;
    case MatchKind::TightFuzzy:
        return 50;
    case MatchKind::Fuzzy:
        return 35;
    case MatchKind::SyntaxFallback:
        return 10;
    case MatchKind::None:
        return 0;
    }

    return 0;
}

int MatchFieldTier(
    MatchField field) noexcept {

    switch (field) {
    case MatchField::Keyword:
        return 60;
    case MatchField::Alias:
        return 55;
    case MatchField::Title:
        return 50;
    case MatchField::FileStem:
        return 48;
    case MatchField::Subtitle:
        return 25;
    case MatchField::Target:
        return 15;
    case MatchField::None:
        return 0;
    }

    return 0;
}

int LaunchSurfaceTier(
    LaunchSurfaceClass surface) noexcept {

    switch (surface) {
    case LaunchSurfaceClass::UserCommand:
        return 90;
    case LaunchSurfaceClass::
        PrimaryApplication:
        return 80;
    case LaunchSurfaceClass::Action:
        return 75;
    case LaunchSurfaceClass::SystemUtility:
        return 60;
    case LaunchSurfaceClass::FilesystemItem:
        return 50;
    case LaunchSurfaceClass::DeveloperTool:
        return 40;
    case LaunchSurfaceClass::CommandLineTool:
        return 35;
    case LaunchSurfaceClass::Auxiliary:
        return 20;
    case LaunchSurfaceClass::Maintenance:
        return 10;
    }

    return 0;
}

bool BetterMatch(
    const Match& left,
    const Match& right) noexcept {

    const int leftKind =
        MatchKindTier(left.kind);
    const int rightKind =
        MatchKindTier(right.kind);

    if (leftKind != rightKind) {
        return leftKind > rightKind;
    }

    const int leftField =
        MatchFieldTier(left.field);
    const int rightField =
        MatchFieldTier(right.field);

    if (leftField != rightField) {
        return leftField > rightField;
    }

    if (left.pinyin != right.pinyin) {
        return !left.pinyin;
    }

    return left.score > right.score;
}

int CompareRankContext(
    const RankContext& left,
    const RankContext& right) noexcept {

    const int leftExplicit =
        left.pinned
            ? 2
            : (left.explicitUser ? 1 : 0);

    const int rightExplicit =
        right.pinned
            ? 2
            : (right.explicitUser ? 1 : 0);

    if (leftExplicit != rightExplicit) {
        return leftExplicit >
                rightExplicit
            ? 1
            : -1;
    }

    const int leftMatch =
        MatchKindTier(
            left.match.kind);
    const int rightMatch =
        MatchKindTier(
            right.match.kind);

    if (leftMatch != rightMatch) {
        return leftMatch > rightMatch
            ? 1
            : -1;
    }

    const int leftSurface =
        LaunchSurfaceTier(
            left.surface);
    const int rightSurface =
        LaunchSurfaceTier(
            right.surface);

    if (leftSurface != rightSurface) {
        return leftSurface >
                rightSurface
            ? 1
            : -1;
    }

    const int leftField =
        MatchFieldTier(
            left.match.field);
    const int rightField =
        MatchFieldTier(
            right.match.field);

    if (leftField != rightField) {
        return leftField > rightField
            ? 1
            : -1;
    }

    if (left.match.pinyin !=
        right.match.pinyin) {
        return !left.match.pinyin
            ? 1
            : -1;
    }

    // Both results already agree on intent tier, surface, field and pinyin.
    // A derived acronym being complete versus a prefix is a cold-start
    // preference, not a permanent barrier to an explicitly learned choice.
    // Compare independent keys (not a pairwise score-distance exception) so
    // this remains a strict weak ordering for any number of candidates.
    if (left.match.kind == MatchKind::Initials &&
        right.match.kind == MatchKind::Initials &&
        left.match.field != MatchField::Target) {
        const int leftHabit = std::clamp(left.usageScore, 0, 32);
        const int rightHabit = std::clamp(right.usageScore, 0, 32);
        if (leftHabit != rightHabit) {
            return leftHabit > rightHabit ? 1 : -1;
        }
    }

    // Once intent strength, surface and field agree, small title-length
    // differences should not permanently defeat a demonstrated habit.
    // Keep the bonus bounded so a materially better text match wins; exact
    // and explicit-syntax matches retain their original score comparison.
    const bool comparableIntent =
        left.match.kind == right.match.kind &&
        left.match.kind != MatchKind::None &&
        left.match.kind != MatchKind::Exact &&
        left.match.kind != MatchKind::Wildcard &&
        left.match.kind != MatchKind::SyntaxFallback &&
        left.match.field != MatchField::Target;

    if (comparableIntent) {
        const int leftScore = left.match.score +
            std::clamp(left.usageScore, 0, 32);
        const int rightScore = right.match.score +
            std::clamp(right.usageScore, 0, 32);

        if (leftScore != rightScore) {
            return leftScore > rightScore ? 1 : -1;
        }
    }

    if (left.match.score !=
        right.match.score) {
        return left.match.score >
                right.match.score
            ? 1
            : -1;
    }

    if (left.usageScore !=
        right.usageScore) {
        return left.usageScore >
                right.usageScore
            ? 1
            : -1;
    }

    if (left.kindWeight !=
        right.kindWeight) {
        return left.kindWeight >
                right.kindWeight
            ? 1
            : -1;
    }

    if (left.providerWeight !=
        right.providerWeight) {
        return left.providerWeight >
                right.providerWeight
            ? 1
            : -1;
    }

    return 0;
}

} // namespace altrun::relevance
