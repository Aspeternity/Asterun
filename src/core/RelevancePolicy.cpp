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

    if (field.empty() || query.empty()) {
        return {};
    }

    const std::wstring normalizedField =
        Normalize(field);
    const std::wstring normalizedQuery =
        Normalize(query);

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

    // One- and two-character ASCII queries are too ambiguous for generic
    // word-boundary recall. Keep exact/field-prefix/initials/pinyin behavior,
    // but require three characters before matching a later word boundary.
    // This prevents short queries from reaching unrelated "... Admin",
    // "... Advanced", "... Additional" or "... Sources" words.
    if (!asciiQuery ||
        normalizedQuery.size() >= 3) {

        for (std::size_t i = 1;
             i < field.size();
             ++i) {
            if (IsWordBoundary(field, i) &&
                NormalizedPrefixAt(
                    field,
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

Match MatchInitials(
    std::wstring_view initials,
    std::wstring_view query) {

    const std::wstring value =
        Normalize(initials);
    const std::wstring normalizedQuery =
        Normalize(query);

    if (value.empty() ||
        normalizedQuery.empty()) {
        return {};
    }

    if (value == normalizedQuery) {
        return {
            MatchKind::Initials,
            MatchField::None,
            965,
            false,
        };
    }

    if (value.starts_with(
            normalizedQuery)) {
        return {
            MatchKind::Initials,
            MatchField::None,
            845 -
                static_cast<int>(
                    std::min<std::size_t>(
                        value.size() -
                            normalizedQuery.size(),
                        120)),
            false,
        };
    }

    return {};
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

    const std::wstring normalized =
        Normalize(query);

    if (normalized.empty()) {
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

    if (explicitSyntax ||
        HasPathIntent(query)) {
        return true;
    }

    const std::size_t length =
        normalized.size();

    switch (surface) {
    case LaunchSurfaceClass::UserCommand:
    case LaunchSurfaceClass::
        PrimaryApplication:
    case LaunchSurfaceClass::Action:
        return true;

    case LaunchSurfaceClass::SystemUtility:
        return length >= 2 &&
            (length > 2 ||
             StrongShortMatch(
                 match.kind));

    case LaunchSurfaceClass::DeveloperTool:
    case LaunchSurfaceClass::CommandLineTool:
        return length >= 2 &&
            (length > 2 ||
             StrongShortMatch(
                 match.kind));

    case LaunchSurfaceClass::Auxiliary:
    case LaunchSurfaceClass::Maintenance:
        return length >= 3 &&
            VeryStrongMatch(
                match.kind);

    case LaunchSurfaceClass::FilesystemItem:
        return length >= 2 &&
            (length > 2 ||
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
