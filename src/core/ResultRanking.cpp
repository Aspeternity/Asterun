#include "ResultRanking.hpp"

#include "ProviderIds.hpp"

#include <algorithm>
#include <cwctype>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace altrun {
namespace {

[[nodiscard]] std::wstring Compact(
    std::wstring_view value) {
    std::wstring result;
    result.reserve(value.size());

    for (const wchar_t ch : value) {
        if (std::iswalnum(ch) ||
            ch >= 0x4E00) {
            result.push_back(
                static_cast<wchar_t>(
                    std::towlower(ch)));
        }
    }

    return result;
}

[[nodiscard]]
std::vector<std::wstring>
QueryTokens(
    std::wstring_view query) {
    std::vector<std::wstring> tokens;
    std::wstring current;

    auto flush = [&]() {
        auto compact =
            Compact(current);
        current.clear();

        if (compact.empty()) {
            return;
        }

        if (std::find(
                tokens.begin(),
                tokens.end(),
                compact) ==
            tokens.end()) {
            tokens.push_back(
                std::move(compact));
        }
    };

    for (const wchar_t ch : query) {
        if (std::iswspace(ch)) {
            flush();
        } else {
            current.push_back(ch);
        }
    }

    flush();
    return tokens;
}

[[nodiscard]] int CompactMatchScore(
    std::wstring_view field,
    std::wstring_view compactQuery) {
    if (field.empty() ||
        compactQuery.empty()) {
        return 0;
    }

    const auto compactField =
        Compact(field);

    if (compactField.empty()) {
        return 0;
    }

    if (compactField ==
        compactQuery) {
        return 1100;
    }

    if (compactField.starts_with(
            compactQuery)) {
        return 940 -
            static_cast<int>(
                std::min<std::size_t>(
                    compactField.size() -
                        compactQuery.size(),
                    100));
    }

    const auto position =
        compactField.find(
            compactQuery);

    if (position !=
        std::wstring::npos) {
        return 760 -
            static_cast<int>(
                std::min<std::size_t>(
                    position,
                    120));
    }

    std::size_t queryIndex = 0;
    int gaps = 0;
    std::size_t previous = 0;
    bool havePrevious = false;

    for (std::size_t i = 0;
         i < compactField.size() &&
         queryIndex <
             compactQuery.size();
         ++i) {
        if (compactField[i] !=
            compactQuery[queryIndex]) {
            continue;
        }

        if (havePrevious &&
            i > previous + 1) {
            gaps +=
                static_cast<int>(
                    i - previous - 1);
        }

        previous = i;
        havePrevious = true;
        ++queryIndex;
    }

    if (queryIndex !=
        compactQuery.size()) {
        return 0;
    }

    return std::max(
        260,
        470 -
            std::min(
                gaps * 7,
                210));
}

[[nodiscard]] std::wstring FileStem(
    std::wstring_view title) {
    const auto slash =
        title.find_last_of(L"\\/");

    const auto name =
        slash == std::wstring_view::npos
            ? title
            : title.substr(slash + 1);

    const auto dot =
        name.find_last_of(L'.');

    if (dot == std::wstring_view::npos ||
        dot == 0) {
        return std::wstring(name);
    }

    return std::wstring(
        name.substr(0, dot));
}

[[nodiscard]] int ScoreOneToken(
    const LauncherResult& result,
    std::wstring_view compactQuery) {
    const int titleScore =
        CompactMatchScore(
            result.title,
            compactQuery);

    int stemScore = 0;

    if (result.kind ==
            ResultKind::File) {
        stemScore =
            CompactMatchScore(
                FileStem(
                    result.title),
                compactQuery);

        if (stemScore > 0) {
            stemScore =
                std::min(
                    1070,
                    stemScore + 20);
        }
    }

    int subtitleScore =
        CompactMatchScore(
            result.subtitle,
            compactQuery);

    if (subtitleScore > 0) {
        subtitleScore =
            std::max(
                1,
                subtitleScore - 330);
    }

    int targetScore =
        CompactMatchScore(
            result.target,
            compactQuery);

    if (targetScore > 0) {
        targetScore =
            std::max(
                1,
                targetScore - 260);
    }

    return std::max({
        titleScore,
        stemScore,
        subtitleScore,
        targetScore,
    });
}

} // namespace

int ResultKindWeight(
    ResultKind kind) noexcept {
    switch (kind) {
    case ResultKind::UserCommand:
        return 60;
    case ResultKind::Application:
        return 30;
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

    return 0;
}

int UnifiedRankScore(
    const LauncherResult& result) noexcept {
    return result.score +
        ResultKindWeight(result.kind) +
        ProviderRankWeight(
            result.providerId);
}

int ScoreDynamicResultText(
    const LauncherResult& result,
    std::wstring_view query) {
    const auto compactQuery =
        Compact(query);

    if (compactQuery.empty()) {
        return 0;
    }

    int score =
        ScoreOneToken(
            result,
            compactQuery);

    const auto tokens =
        QueryTokens(query);

    if (tokens.size() > 1) {
        int weakest =
            std::numeric_limits<int>::max();
        int total = 0;
        bool allMatched = true;

        for (const auto& token :
             tokens) {
            const int tokenScore =
                ScoreOneToken(
                    result,
                    token);

            if (tokenScore <= 0) {
                allMatched = false;
                break;
            }

            weakest =
                std::min(
                    weakest,
                    tokenScore);
            total += tokenScore;
        }

        if (allMatched) {
            const int average =
                total /
                static_cast<int>(
                    tokens.size());

            score =
                std::max(
                    score,
                    std::min(
                        1120,
                        weakest +
                            average / 7 +
                            35));
        }
    }

    // Everything already matched this item using its own query engine.
    // Keep advanced/syntax matches visible even when our lightweight local
    // scorer cannot interpret the query expression.
    return score > 0
        ? score
        : 240;
}

} // namespace altrun
