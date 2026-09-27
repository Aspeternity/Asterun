#include "ResultRanking.hpp"

#include "ProviderIds.hpp"
#include "RelevancePolicy.hpp"

#include <algorithm>
#include <string>

namespace altrun {
namespace {

[[nodiscard]] std::wstring_view FileStem(
    std::wstring_view title) {

    const auto slash =
        title.find_last_of(L"\\");

    const auto slash2 =
        title.find_last_of(L"/");

    std::size_t position =
        std::wstring_view::npos;

    if (slash == std::wstring_view::npos) {
        position = slash2;
    } else if (
        slash2 ==
        std::wstring_view::npos) {
        position = slash;
    } else {
        position = std::max(
            slash,
            slash2);
    }

    const auto name =
        position ==
                std::wstring_view::npos
            ? title
            : title.substr(
                  position + 1);

    const auto dot =
        name.find_last_of(L'.');

    if (dot ==
            std::wstring_view::npos ||
        dot == 0) {
        return name;
    }

    return name.substr(0, dot);
}

[[nodiscard]] relevance::Match
WithField(
    relevance::Match match,
    relevance::MatchField field) {

    if (match) {
        match.field = field;
    }

    return match;
}

[[nodiscard]] relevance::Match
ScoreOneToken(
    const LauncherResult& result,
    std::wstring_view normalizedQuery,
    bool allowTarget) {

    relevance::Match best{};

    const auto consider =
        [&](relevance::Match match) {
            if (match &&
                relevance::BetterMatch(
                    match,
                    best)) {
                best = match;
            }
        };

    consider(
        WithField(
            relevance::MatchTextNormalizedQuery(
                result.title,
                normalizedQuery),
            relevance::MatchField::Title));

    if (result.kind ==
        ResultKind::File) {

        consider(
            WithField(
                relevance::MatchTextNormalizedQuery(
                    FileStem(
                        result.title),
                    normalizedQuery),
                relevance::MatchField::
                    FileStem));
    }

    consider(
        WithField(
            relevance::MatchTextNormalizedQuery(
                result.subtitle,
                normalizedQuery),
            relevance::MatchField::
                Subtitle));

    if (allowTarget) {
        consider(
            WithField(
                relevance::MatchTextNormalizedQuery(
                    result.target,
                    normalizedQuery),
                relevance::MatchField::
                    Target));
    }

    return best;
}

[[nodiscard]] relevance::Match
LegacyMatch(
    const LauncherResult& result) {

    if (result.relevanceMatch) {
        return result.relevanceMatch;
    }

    if (result.score <= 0) {
        return {};
    }

    relevance::MatchKind kind =
        relevance::MatchKind::Fuzzy;

    if (result.score >= 1100) {
        kind =
            relevance::MatchKind::Exact;
    } else if (result.score >= 850) {
        kind =
            relevance::MatchKind::Prefix;
    } else if (result.score >= 650) {
        kind =
            relevance::MatchKind::
                Substring;
    }

    return {
        kind,
        relevance::MatchField::Title,
        result.score,
        false,
    };
}

[[nodiscard]] LaunchSurfaceClass
EffectiveSurface(
    const LauncherResult& result) {

    if (result.surfaceClass !=
        LaunchSurfaceClass::Action) {
        return result.surfaceClass;
    }

    switch (result.kind) {
    case ResultKind::UserCommand:
        return LaunchSurfaceClass::
            UserCommand;
    case ResultKind::Application:
        return LaunchSurfaceClass::
            PrimaryApplication;
    case ResultKind::File:
    case ResultKind::Folder:
        return LaunchSurfaceClass::
            FilesystemItem;
    case ResultKind::Action:
        return LaunchSurfaceClass::
            Action;
    }

    return LaunchSurfaceClass::Action;
}

} // namespace

int ResultKindWeight(
    ResultKind kind) noexcept {

    switch (kind) {
    case ResultKind::UserCommand:
        return 60;
    case ResultKind::Application:
        return 30;
    case ResultKind::Action:
        return 25;
    case ResultKind::Folder:
        return 8;
    case ResultKind::File:
        return 0;
    }

    return 0;
}

int ProviderRankWeight(
    std::string_view providerId) noexcept {

    if (providerId ==
        "user.commands") {
        return 20;
    }

    if (providerId ==
        providers::kStartMenu) {
        return 12;
    }

    if (providerId ==
        providers::kPackaged) {
        return 9;
    }

    if (providerId ==
        providers::kAppPaths) {
        return 6;
    }

    if (providerId ==
        providers::kBuiltinWeb) {
        return 10;
    }

    if (providerId ==
        providers::kBuiltinClipboard) {
        return 16;
    }

    return 0;
}

int UnifiedRankScore(
    const LauncherResult& result) noexcept {

    return result.score +
        ResultKindWeight(result.kind) +
        ProviderRankWeight(
            result.providerId);
}

bool RankDynamicResultText(
    LauncherResult& result,
    std::wstring_view query) {

    if (!relevance::
            ShouldRunDynamicFilesystemQuery(
                query)) {
        return false;
    }

    const std::wstring normalizedQuery =
        relevance::Normalize(query);

    const bool explicitSyntax =
        relevance::HasExplicitSyntax(
            query);

    const bool allowTarget =
        relevance::HasPathIntent(
            query);

    relevance::Match match =
        ScoreOneToken(
            result,
            normalizedQuery,
            allowTarget);

    const auto tokens =
        relevance::QueryTokens(query);

    if (tokens.size() > 1 &&
        !explicitSyntax) {

        relevance::Match weakest{};
        bool haveWeakest = false;
        bool allMatched = true;
        int total = 0;

        for (const auto& token :
             tokens) {

            const auto tokenMatch =
                ScoreOneToken(
                    result,
                    token,
                    allowTarget);

            if (!tokenMatch) {
                allMatched = false;
                break;
            }

            if (!haveWeakest ||
                relevance::BetterMatch(
                    weakest,
                    tokenMatch)) {
                weakest = tokenMatch;
                haveWeakest = true;
            }

            total += tokenMatch.score;
        }

        if (!allMatched) {
            return false;
        }

        const int average =
            total /
            static_cast<int>(
                tokens.size());

        weakest.score =
            std::min(
                1120,
                weakest.score +
                    average / 7 +
                    35);

        if (relevance::BetterMatch(
                weakest,
                match)) {
            match = weakest;
        }
    }

    if (!match &&
        explicitSyntax) {
        match = {
            relevance::MatchKind::
                SyntaxFallback,
            relevance::MatchField::Target,
            240,
            false,
        };
    }

    if (!match ||
        !relevance::AdmitLaunchSurfaceNormalized(
            LaunchSurfaceClass::
                FilesystemItem,
            normalizedQuery,
            match,
            explicitSyntax)) {
        return false;
    }

    result.relevanceMatch = match;
    result.surfaceClass =
        LaunchSurfaceClass::
            FilesystemItem;
    result.usageScore = 0;
    result.pinned = false;
    result.score = match.score;

    return true;
}

int ScoreDynamicResultText(
    const LauncherResult& result,
    std::wstring_view query) {

    LauncherResult ranked = result;

    return RankDynamicResultText(
               ranked,
               query)
        ? ranked.score
        : 0;
}

bool BetterLauncherResult(
    const LauncherResult& left,
    const LauncherResult& right) noexcept {

    const relevance::RankContext leftRank{
        left.pinned,
        left.kind ==
            ResultKind::UserCommand,
        LegacyMatch(left),
        EffectiveSurface(left),
        left.usageScore,
        ResultKindWeight(left.kind),
        ProviderRankWeight(
            left.providerId),
    };

    const relevance::RankContext rightRank{
        right.pinned,
        right.kind ==
            ResultKind::UserCommand,
        LegacyMatch(right),
        EffectiveSurface(right),
        right.usageScore,
        ResultKindWeight(right.kind),
        ProviderRankWeight(
            right.providerId),
    };

    const int comparison =
        relevance::CompareRankContext(
            leftRank,
            rightRank);

    if (comparison != 0) {
        return comparison > 0;
    }

    return left.score > right.score;
}

} // namespace altrun
