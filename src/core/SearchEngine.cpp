#include "SearchEngine.hpp"

#include <algorithm>
#include <bit>
#include <chrono>
#include <cmath>
#include <cwctype>
#include <limits>
#include <utility>

namespace altrun {

SearchEngine::SearchEngine(
    std::filesystem::path
        pinyinDictionaryDirectory)
    : pinyin_(
          std::move(
              pinyinDictionaryDirectory)) {}

bool SearchEngine::GlobMatch(
    std::wstring_view field,
    std::wstring_view pattern) {

    const std::wstring value =
        relevance::Normalize(field);

    if (value.empty() ||
        pattern.empty()) {
        return false;
    }

    std::size_t valueIndex = 0;
    std::size_t patternIndex = 0;
    std::size_t starIndex =
        std::wstring::npos;
    std::size_t starMatch = 0;

    while (valueIndex < value.size()) {
        if (patternIndex <
                pattern.size() &&
            (pattern[patternIndex] ==
                 L'?' ||
             pattern[patternIndex] ==
                 value[valueIndex])) {

            ++patternIndex;
            ++valueIndex;
            continue;
        }

        if (patternIndex <
                pattern.size() &&
            pattern[patternIndex] ==
                L'*') {

            starIndex =
                patternIndex++;
            starMatch =
                valueIndex;
            continue;
        }

        if (starIndex !=
            std::wstring::npos) {

            patternIndex =
                starIndex + 1;
            valueIndex =
                ++starMatch;
            continue;
        }

        return false;
    }

    while (patternIndex <
               pattern.size() &&
           pattern[patternIndex] ==
               L'*') {
        ++patternIndex;
    }

    return patternIndex ==
        pattern.size();
}

int SearchEngine::WildcardMatchScore(
    std::wstring_view field,
    std::wstring_view normalizedPattern) {

    if (!GlobMatch(
            field,
            normalizedPattern)) {
        return 0;
    }

    const auto literalCount =
        static_cast<int>(
            std::count_if(
                normalizedPattern.begin(),
                normalizedPattern.end(),
                [](wchar_t ch) {
                    return ch != L'*' &&
                           ch != L'?';
                }));

    return 820 +
        std::min(
            literalCount * 12,
            160);
}

int SearchEngine::UsageScore(
    const UsageStat* stat) {

    if (stat == nullptr ||
        stat->launches == 0) {
        return 0;
    }

    int score =
        static_cast<int>(
            std::min<double>(
                140.0,
                std::log2(
                    static_cast<double>(
                        stat->launches) +
                    1.0) *
                    24.0));

    if (stat->lastUsedUnix > 0) {
        const auto now =
            std::chrono::system_clock::now();

        const auto nowUnix =
            std::chrono::duration_cast<
                std::chrono::seconds>(
                now.time_since_epoch())
                .count();

        const auto age =
            std::max<std::int64_t>(
                0,
                nowUnix -
                    stat->lastUsedUnix);

        const auto days =
            age / 86400;

        if (days == 0) score += 90;
        else if (days <= 2) score += 70;
        else if (days <= 7) score += 45;
        else if (days <= 30) score += 20;
    }

    return score;
}

int SearchEngine::IntentUsageScore(
    const UsageStat* stat) {

    // A single accidental launch is not a preference. Frequency alone
    // supplies a small, stable tie-break among comparable text matches;
    // the empty-query ordering keeps its existing recency behavior.
    if (stat == nullptr || stat->launches < 2) {
        return 0;
    }

    return std::min(
        32,
        8 + static_cast<int>(
            std::bit_width(stat->launches) - 1) * 8);
}

bool SearchEngine::IsPinyinQuery(
    std::wstring_view normalizedQuery) {

    if (normalizedQuery.empty()) {
        return false;
    }

    bool hasLetter = false;

    for (const wchar_t ch :
         normalizedQuery) {
        if (ch > 0x7F) {
            return false;
        }

        if ((ch >= L'a' &&
             ch <= L'z') ||
            (ch >= L'A' &&
             ch <= L'Z')) {
            hasLetter = true;
            continue;
        }

        if (ch >= L'0' &&
            ch <= L'9') {
            continue;
        }

        return false;
    }

    return hasLetter;
}

std::wstring SearchEngine::WordInitials(
    std::wstring_view field) {

    std::wstring initials;
    bool boundary = true;
    wchar_t previous = 0;

    for (std::size_t i = 0;
         i < field.size();
         ++i) {

        const wchar_t ch = field[i];

        if (!std::iswalnum(ch) ||
            ch > 0x7F) {
            boundary = true;
            previous = 0;
            continue;
        }

        const bool upper =
            std::iswupper(ch) != 0;

        const bool previousLower =
            previous != 0 &&
            std::iswlower(previous) != 0;

        const bool previousUpper =
            previous != 0 &&
            std::iswupper(previous) != 0;

        const bool nextLower =
            i + 1 < field.size() &&
            field[i + 1] <= 0x7F &&
            std::iswlower(
                field[i + 1]) != 0;

        const bool camelBoundary =
            upper &&
            (previousLower ||
             (previousUpper &&
              nextLower));

        if (boundary ||
            camelBoundary) {
            initials.push_back(
                static_cast<wchar_t>(
                    std::towlower(ch)));
        }

        boundary = false;
        previous = ch;
    }

    return initials;
}

relevance::Match
SearchEngine::DerivedInitialMatchScore(
    std::wstring_view field,
    std::wstring_view normalizedQuery) {

    const std::wstring initials =
        WordInitials(field);

    if (initials.size() < 2) {
        return {};
    }

    auto match =
        relevance::MatchInitials(
            initials,
            normalizedQuery);

    if (match) {
        match.score =
            std::max(
                1,
                match.score - 35);
    }

    return match;
}

int SearchEngine::HybridPinyinPrefixScore(
    const PinyinForms& forms,
    std::wstring_view normalizedQuery) {

    if (normalizedQuery.empty() ||
        forms.syllables.empty()) {
        return 0;
    }

    constexpr int kImpossible =
        std::numeric_limits<int>::min() /
        4;

    std::vector<int> states(
        normalizedQuery.size() + 1,
        kImpossible);

    states[0] = 0;

    int bestComplete = kImpossible;

    for (const auto& syllable :
         forms.syllables) {

        if (syllable.empty()) {
            continue;
        }

        std::vector<int> next(
            normalizedQuery.size() + 1,
            kImpossible);

        for (std::size_t pos = 0;
             pos <=
                 normalizedQuery.size();
             ++pos) {

            if (states[pos] ==
                kImpossible) {
                continue;
            }

            if (pos ==
                normalizedQuery.size()) {
                bestComplete =
                    std::max(
                        bestComplete,
                        states[pos]);
                continue;
            }

            const auto remaining =
                normalizedQuery.substr(pos);

            if (remaining.size() >=
                    syllable.size() &&
                remaining.starts_with(
                    syllable)) {

                const std::size_t newPos =
                    pos + syllable.size();

                next[newPos] =
                    std::max(
                        next[newPos],
                        states[pos] +
                            static_cast<int>(
                                syllable.size()) *
                                8 +
                            12);
            }

            if (remaining.front() ==
                syllable.front()) {

                next[pos + 1] =
                    std::max(
                        next[pos + 1],
                        states[pos] + 5);
            }

            if (remaining.size() <
                    syllable.size() &&
                syllable.starts_with(
                    remaining)) {

                bestComplete =
                    std::max(
                        bestComplete,
                        states[pos] +
                            static_cast<int>(
                                remaining.size()) *
                                7 +
                            6);
            }
        }

        states = std::move(next);

        if (states[
                normalizedQuery.size()] !=
            kImpossible) {

            bestComplete =
                std::max(
                    bestComplete,
                    states[
                        normalizedQuery
                            .size()]);
        }
    }

    if (bestComplete == kImpossible) {
        return 0;
    }

    return 875 +
        std::min(
            75,
            std::max(
                0,
                bestComplete));
}

relevance::Match
SearchEngine::PinyinMatchScore(
    std::wstring_view field,
    std::wstring_view normalizedQuery)
    const {

    const PinyinForms* forms =
        pinyin_.FormsFor(field);

    if (!forms) {
        return {};
    }

    relevance::Match best{};

    auto full =
        relevance::MatchText(
            forms->full,
            normalizedQuery);

    if (full) {
        full.pinyin = true;
        full.score =
            std::max(
                1,
                full.score - 45);

        if (full.score > best.score) {
            best = full;
        }
    }

    auto initials =
        relevance::MatchInitials(
            forms->initials,
            normalizedQuery);

    if (initials) {
        initials.pinyin = true;
        initials.score =
            std::max(
                1,
                initials.score - 20);

        if (initials.score > best.score) {
            best = initials;
        }
    }

    const int hybridScore =
        HybridPinyinPrefixScore(
            *forms,
            normalizedQuery);

    if (hybridScore > 0) {
        relevance::Match hybrid{
            relevance::MatchKind::
                HybridPinyin,
            relevance::MatchField::None,
            hybridScore,
            true,
        };

        if (hybrid.score > best.score) {
            best = hybrid;
        }
    }

    return best;
}

relevance::Match
SearchEngine::CommandTextScore(
    const Command& command,
    std::wstring_view normalizedQuery,
    bool allowPinyin,
    bool allowTarget) const {

    if (normalizedQuery.empty()) {
        return {};
    }

    relevance::Match best{};

    const auto consider =
        [&](relevance::Match match,
            relevance::MatchField field) {
            if (!match) {
                return;
            }

            match.field = field;

            if (relevance::BetterMatch(
                    match,
                    best)) {
                best = match;
            }
        };

    consider(
        relevance::MatchText(
            command.keyword,
            normalizedQuery),
        relevance::MatchField::
            Keyword);

    for (const auto& alias :
         command.aliases) {
        consider(
            relevance::MatchText(
                alias,
                normalizedQuery),
            relevance::MatchField::
                Alias);
    }

    consider(
        relevance::MatchText(
            command.title,
            normalizedQuery),
        relevance::MatchField::Title);

    if (allowTarget) {
        consider(
            relevance::MatchText(
                command.target,
                normalizedQuery),
            relevance::MatchField::
                Target);
    }

    consider(
        DerivedInitialMatchScore(
            command.keyword,
            normalizedQuery),
        relevance::MatchField::
            Keyword);

    for (const auto& alias :
         command.aliases) {
        consider(
            DerivedInitialMatchScore(
                alias,
                normalizedQuery),
            relevance::MatchField::
                Alias);
    }

    consider(
        DerivedInitialMatchScore(
            command.title,
            normalizedQuery),
        relevance::MatchField::Title);

    if (allowPinyin &&
        pinyin_.Available() &&
        IsPinyinQuery(normalizedQuery)) {

        consider(
            PinyinMatchScore(
                command.keyword,
                normalizedQuery),
            relevance::MatchField::
                Keyword);

        for (const auto& alias :
             command.aliases) {
            consider(
                PinyinMatchScore(
                    alias,
                    normalizedQuery),
                relevance::MatchField::
                    Alias);
        }

        consider(
            PinyinMatchScore(
                command.title,
                normalizedQuery),
            relevance::MatchField::
                Title);
    }

    return best;
}

relevance::Match
SearchEngine::CommandWildcardScore(
    const Command& command,
    std::wstring_view normalizedPattern) {

    relevance::Match best{};

    const auto consider =
        [&](std::wstring_view field,
            relevance::MatchField matchField) {

            const int score =
                WildcardMatchScore(
                    field,
                    normalizedPattern);

            if (score <= 0) {
                return;
            }

            relevance::Match match{
                relevance::MatchKind::
                    Wildcard,
                matchField,
                score,
                false,
            };

            if (relevance::BetterMatch(
                    match,
                    best)) {
                best = match;
            }
        };

    consider(
        command.keyword,
        relevance::MatchField::Keyword);

    for (const auto& alias :
         command.aliases) {
        consider(
            alias,
            relevance::MatchField::Alias);
    }

    consider(
        command.title,
        relevance::MatchField::Title);

    consider(
        command.target,
        relevance::MatchField::Target);

    return best;
}

bool SearchEngine::HasDistinctiveCatalogIntent(
    const Command& command,
    std::wstring_view query) {

    if (command.distinctiveTokens.empty()) {
        return false;
    }

    const std::wstring normalizedQuery =
        relevance::Normalize(query);

    if (normalizedQuery.empty()) {
        return false;
    }

    // StrongMatchOnly re-admission must express this entry's own identity,
    // not merely the shared catalog family. Distinctive tokens are generated
    // upstream, but provider/context combinations can conservatively retain
    // overlapping family text. Guard that boundary here using only the cached
    // catalogGroupKey: no role inference or I/O enters the keystroke path.
    std::wstring compactQuery;
    compactQuery.reserve(
        normalizedQuery.size());

    for (const wchar_t ch :
         normalizedQuery) {
        if (std::iswalnum(ch) ||
            ch >= 0x4E00) {
            compactQuery.push_back(ch);
        }
    }

    constexpr std::wstring_view
        familyMarker = L"family:";

    const std::size_t familyStart =
        command.catalogGroupKey.find(
            familyMarker);

    if (!compactQuery.empty() &&
        familyStart !=
            std::wstring::npos) {

        const std::size_t valueStart =
            familyStart +
            familyMarker.size();

        const std::size_t valueEnd =
            command.catalogGroupKey.find(
                L'|',
                valueStart);

        std::wstring family =
            command.catalogGroupKey.substr(
                valueStart,
                valueEnd ==
                        std::wstring::npos
                    ? std::wstring::npos
                    : valueEnd -
                          valueStart);

        std::transform(
            family.begin(),
            family.end(),
            family.begin(),
            [](wchar_t ch) {
                return
                    static_cast<wchar_t>(
                        std::towlower(ch));
            });

        if (!family.empty() &&
            family.find(compactQuery) !=
                std::wstring::npos) {
            return false;
        }
    }

    const auto queryTokens =
        relevance::QueryTokens(query);

    for (const auto& distinctive :
         command.distinctiveTokens) {

        const std::wstring normalizedDistinctive =
            relevance::Normalize(
                distinctive);

        if (normalizedDistinctive.size() < 2) {
            continue;
        }

        // Covers compact queries such as "contosoperformance" while still
        // requiring the query to name the auxiliary entry's distinctive
        // intent rather than only the shared product/family name.
        if (normalizedQuery.find(
                normalizedDistinctive) !=
            std::wstring::npos) {
            return true;
        }

        for (const auto& token :
             queryTokens) {
            if (token.size() < 2) {
                continue;
            }

            if (normalizedDistinctive ==
                    token ||
                normalizedDistinctive
                    .starts_with(token)) {
                return true;
            }
        }
    }

    return false;
}

bool SearchEngine::AdmitCatalogEntry(
    const Command& command,
    std::wstring_view query,
    const relevance::Match& match,
    bool explicitSyntax) {

    // A user-authored shortcut is explicit intent and must never be hidden by
    // automatically inferred catalog roles.
    if (command.source ==
        CommandSource::User) {
        return true;
    }

    switch (command.catalogVisibility) {
    case CatalogVisibility::Normal:
        return true;

    case CatalogVisibility::Hidden:
        return false;

    case CatalogVisibility::StrongMatchOnly:
        break;
    }

    if (relevance::Normalize(query).empty()) {
        return false;
    }

    if (explicitSyntax) {
        return true;
    }

    // If the user typed the complete discovered entry name/keyword/alias,
    // that is explicit enough even when grouping metadata could not derive
    // distinctive tokens.
    if (match.kind ==
            relevance::MatchKind::Exact &&
        match.field !=
            relevance::MatchField::Target) {
        return true;
    }

    return HasDistinctiveCatalogIntent(
        command,
        query);
}

std::vector<SearchResult>
SearchEngine::Search(
    const std::vector<Command>& commands,
    const UsageMap& usage,
    std::wstring_view query,
    std::size_t limit,
    bool allowWildcards,
    bool allowPinyin) const {

    std::vector<SearchResult> results;

    results.reserve(
        std::min(
            commands.size(),
            limit * 3));

    const std::wstring normalizedQuery =
        relevance::Normalize(query);

    const auto queryTokens =
        relevance::QueryTokens(query);

    const bool wildcardQuery =
        allowWildcards &&
        normalizedQuery.find_first_of(
            L"*?") !=
            std::wstring::npos;

    const bool allowTarget =
        wildcardQuery ||
        relevance::HasPathIntent(query);

    const bool explicitSyntax =
        wildcardQuery ||
        relevance::HasExplicitSyntax(
            query);

    for (std::size_t i = 0;
         i < commands.size();
         ++i) {

        const auto& command = commands[i];

        const auto usageIt =
            usage.find(command.id);

        const UsageStat* stat =
            usageIt == usage.end()
                ? nullptr
                : &usageIt->second;

        const int usageScore =
            normalizedQuery.empty()
                ? UsageScore(stat)
                : (explicitSyntax || allowTarget
                       ? 0
                       : IntentUsageScore(stat));

        relevance::Match match{};

        if (normalizedQuery.empty()) {
            if (!AdmitCatalogEntry(
                    command,
                    query,
                    match,
                    false) ||
                !relevance::
                    AdmitLaunchSurface(
                        command.surfaceClass,
                        query,
                        match,
                        false)) {
                continue;
            }
        } else {
            match =
                wildcardQuery
                    ? CommandWildcardScore(
                          command,
                          normalizedQuery)
                    : CommandTextScore(
                          command,
                          normalizedQuery,
                          allowPinyin,
                          allowTarget);

            // Cached distinctive identity is stronger evidence than a
            // generic later-word short-prefix match. Any entry may therefore
            // match an exact distinctive token (for example a short opaque
            // "Z5"), while StrongMatchOnly entries additionally allow a
            // distinctive prefix so explicit intent such as "rout" can
            // recover a restrictive "routing" surface. This remains cache-only
            // matching: role inference and I/O stay out of the keystroke path.
            if (!wildcardQuery) {

                for (const auto& distinctive :
                     command.distinctiveTokens) {

                    auto intentMatch =
                        relevance::MatchText(
                            distinctive,
                            normalizedQuery);

                    if (!intentMatch) {
                        continue;
                    }

                    const bool exactDistinctive =
                        intentMatch.kind ==
                        relevance::MatchKind::Exact;
                    const bool restrictiveIntent =
                        command.catalogVisibility ==
                        CatalogVisibility::
                            StrongMatchOnly;

                    // A cached token must not manufacture an Exact/Alias
                    // escape hatch around StrongMatchOnly admission. Validate
                    // restrictive intent first so family-overlapping tokens
                    // cannot bypass AdmitCatalogEntry's family boundary.
                    if (restrictiveIntent &&
                        !HasDistinctiveCatalogIntent(
                            command,
                            query)) {
                        continue;
                    }

                    if (!exactDistinctive &&
                        !restrictiveIntent) {
                        continue;
                    }

                    intentMatch.field =
                        relevance::MatchField::
                            Alias;

                    if (relevance::BetterMatch(
                            intentMatch,
                            match)) {
                        match = intentMatch;
                    }
                }
            }

            if (!wildcardQuery &&
                queryTokens.size() > 1) {

                relevance::Match weakest{};
                int total = 0;
                bool haveWeakest = false;
                bool allTokensMatched = true;

                for (const auto& token :
                     queryTokens) {

                    auto tokenMatch =
                        CommandTextScore(
                            command,
                            token,
                            allowPinyin,
                            allowTarget);

                    // Multi-token intent must use the same already-cached
                    // distinctive identity as a standalone query. This is
                    // especially important for exact two-character residuals:
                    // global short BoundaryPrefix recall stays closed, while
                    // "family q7" can still express the restrictive entry's
                    // own identity. StrongMatchOnly prefixes remain subject to
                    // the family/distinctive admission boundary.
                    if (!tokenMatch) {
                        for (const auto& distinctive :
                             command.distinctiveTokens) {

                            auto intentMatch =
                                relevance::MatchText(
                                    distinctive,
                                    token);

                            if (!intentMatch) {
                                continue;
                            }

                            const bool exactDistinctive =
                                intentMatch.kind ==
                                relevance::MatchKind::
                                    Exact;
                            const bool restrictiveIntent =
                                command.catalogVisibility ==
                                CatalogVisibility::
                                    StrongMatchOnly;

                            if (restrictiveIntent &&
                                !HasDistinctiveCatalogIntent(
                                    command,
                                    token)) {
                                continue;
                            }

                            if (!exactDistinctive &&
                                !restrictiveIntent) {
                                continue;
                            }

                            intentMatch.field =
                                relevance::MatchField::
                                    Alias;

                            tokenMatch =
                                intentMatch;
                            break;
                        }
                    }

                    if (!tokenMatch) {
                        allTokensMatched =
                            false;
                        break;
                    }

                    if (!haveWeakest ||
                        relevance::BetterMatch(
                            weakest,
                            tokenMatch)) {
                        weakest = tokenMatch;
                        haveWeakest = true;
                    }

                    total +=
                        tokenMatch.score;
                }

                if (!allTokensMatched) {
                    if (match.kind !=
                        relevance::MatchKind::
                            HybridPinyin) {
                        continue;
                    }
                } else {
                    const int average =
                        total /
                        static_cast<int>(
                            queryTokens.size());

                    weakest.score =
                        std::min(
                            1180,
                            weakest.score +
                                average / 5 +
                                40);

                    if (relevance::BetterMatch(
                            weakest,
                            match)) {
                        match = weakest;
                    }
                }
            }

            if (!match ||
                !AdmitCatalogEntry(
                    command,
                    query,
                    match,
                    explicitSyntax) ||
                !relevance::
                    AdmitLaunchSurface(
                        command.surfaceClass,
                        query,
                        match,
                        explicitSyntax)) {
                continue;
            }
        }

        results.push_back({
            i,
            normalizedQuery.empty()
                ? usageScore
                : match.score,
            match,
            usageScore,
        });
    }

    std::stable_sort(
        results.begin(),
        results.end(),
        [&](const SearchResult& left,
            const SearchResult& right) {

            const auto& leftCommand =
                commands[
                    left.commandIndex];
            const auto& rightCommand =
                commands[
                    right.commandIndex];

            const relevance::RankContext
                leftRank{
                    leftCommand.pinned,
                    leftCommand.source ==
                        CommandSource::User,
                    left.relevanceMatch,
                    leftCommand.surfaceClass,
                    left.usageScore,
                    0,
                    leftCommand.basePriority,
                };

            const relevance::RankContext
                rightRank{
                    rightCommand.pinned,
                    rightCommand.source ==
                        CommandSource::User,
                    right.relevanceMatch,
                    rightCommand.surfaceClass,
                    right.usageScore,
                    0,
                    rightCommand.basePriority,
                };

            const int rank =
                relevance::
                    CompareRankContext(
                        leftRank,
                        rightRank);

            if (rank != 0) {
                return rank > 0;
            }

            if (leftCommand.sortOrder !=
                rightCommand.sortOrder) {
                return leftCommand.sortOrder <
                    rightCommand.sortOrder;
            }

            if (leftCommand.keyword !=
                rightCommand.keyword) {
                return leftCommand.keyword <
                    rightCommand.keyword;
            }

            return leftCommand.title <
                rightCommand.title;
        });

    if (results.size() > limit) {
        results.resize(limit);
    }

    return results;
}

} // namespace altrun
