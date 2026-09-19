#include "SearchEngine.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cwctype>
#include <limits>
#include <numeric>
#include <utility>

namespace altrun {

SearchEngine::SearchEngine(
    std::filesystem::path pinyinDictionaryDirectory)
    : pinyin_(
          std::move(pinyinDictionaryDirectory)) {}

std::wstring SearchEngine::Normalize(
    std::wstring_view text) {

    std::wstring out;
    out.reserve(text.size());

    for (const wchar_t ch : text) {
        if (std::iswspace(ch) ||
            ch == L'_' ||
            ch == L'-') {
            continue;
        }

        out.push_back(
            static_cast<wchar_t>(
                std::towlower(ch)));
    }

    return out;
}

std::vector<std::wstring> SearchEngine::QueryTokens(
    std::wstring_view text) {

    std::vector<std::wstring> tokens;
    std::wstring current;

    auto flush = [&]() {
        const std::wstring normalized =
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
            tokens.push_back(normalized);
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

int SearchEngine::MatchScore(
    std::wstring_view field,
    std::wstring_view query) {

    if (field.empty() || query.empty()) {
        return 0;
    }

    const std::wstring f = Normalize(field);
    const std::wstring q = Normalize(query);

    if (f.empty() || q.empty()) {
        return 0;
    }

    if (f == q) {
        return 1000;
    }

    if (f.starts_with(q)) {
        return 880 -
            static_cast<int>(
                std::min<std::size_t>(
                    f.size() - q.size(),
                    120));
    }

    const auto pos = f.find(q);

    if (pos != std::wstring::npos) {
        return 690 -
            static_cast<int>(
                std::min<std::size_t>(
                    pos,
                    100));
    }

    std::size_t qi = 0;
    int gaps = 0;
    int run = 0;
    int bestRun = 0;
    std::size_t previous = 0;
    bool havePrevious = false;

    for (std::size_t i = 0;
         i < f.size() && qi < q.size();
         ++i) {

        if (f[i] != q[qi]) {
            continue;
        }

        if (havePrevious) {
            if (i == previous + 1) {
                ++run;
            } else {
                gaps +=
                    static_cast<int>(
                        i - previous - 1);
                run = 1;
            }
        } else {
            run = 1;
        }

        bestRun =
            std::max(bestRun, run);

        previous = i;
        havePrevious = true;
        ++qi;
    }

    if (qi != q.size()) {
        return 0;
    }

    return std::max(
        180,
        430 +
            bestRun * 18 -
            gaps * 8);
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

bool SearchEngine::IsPinyinQuery(
    std::wstring_view normalizedQuery) {

    if (normalizedQuery.empty()) {
        return false;
    }

    bool hasLetter = false;

    for (const wchar_t ch : normalizedQuery) {
        if (ch > 0x7F) {
            return false;
        }

        if ((ch >= L'a' && ch <= L'z') ||
            (ch >= L'A' && ch <= L'Z')) {
            hasLetter = true;
            continue;
        }

        if (ch >= L'0' && ch <= L'9') {
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

    for (std::size_t i = 0; i < field.size(); ++i) {
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
            std::iswlower(field[i + 1]) != 0;

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

int SearchEngine::DerivedInitialMatchScore(
    std::wstring_view field,
    std::wstring_view normalizedQuery) {

    const std::wstring initials =
        WordInitials(field);

    if (initials.size() < 2) {
        return 0;
    }

    const int score =
        MatchScore(
            initials,
            normalizedQuery);

    return score > 0
        ? std::max(1, score - 35)
        : 0;
}

int SearchEngine::HybridPinyinPrefixScore(
    const PinyinForms& forms,
    std::wstring_view normalizedQuery) {

    if (normalizedQuery.empty() ||
        forms.syllables.empty()) {
        return 0;
    }

    constexpr int kImpossible =
        std::numeric_limits<int>::min() / 4;

    std::vector<int> states(
        normalizedQuery.size() + 1,
        kImpossible);

    states[0] = 0;

    int bestComplete = kImpossible;

    for (const auto& syllable : forms.syllables) {
        if (syllable.empty()) {
            continue;
        }

        std::vector<int> next(
            normalizedQuery.size() + 1,
            kImpossible);

        for (std::size_t pos = 0;
             pos <= normalizedQuery.size();
             ++pos) {

            if (states[pos] == kImpossible) {
                continue;
            }

            if (pos == normalizedQuery.size()) {
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

        if (states[normalizedQuery.size()] !=
            kImpossible) {
            bestComplete =
                std::max(
                    bestComplete,
                    states[normalizedQuery.size()]);
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

int SearchEngine::PinyinMatchScore(
    std::wstring_view field,
    std::wstring_view normalizedQuery) const {

    const PinyinForms* forms =
        pinyin_.FormsFor(field);

    if (!forms) {
        return 0;
    }

    int fullScore =
        MatchScore(
            forms->full,
            normalizedQuery);

    int initialsScore =
        MatchScore(
            forms->initials,
            normalizedQuery);

    if (fullScore > 0) {
        fullScore =
            std::max(
                1,
                fullScore - 45);
    }

    if (initialsScore > 0) {
        initialsScore =
            std::max(
                1,
                initialsScore - 20);
    }

    const int hybridScore =
        HybridPinyinPrefixScore(
            *forms,
            normalizedQuery);

    return std::max({
        fullScore,
        initialsScore,
        hybridScore
    });
}

int SearchEngine::CommandTextScore(
    const Command& command,
    std::wstring_view normalizedQuery) const {

    if (normalizedQuery.empty()) {
        return 0;
    }

    const int keywordScore =
        MatchScore(
            command.keyword,
            normalizedQuery);

    const int titleScore =
        MatchScore(
            command.title,
            normalizedQuery);

    const int targetScore =
        MatchScore(
            command.target,
            normalizedQuery);

    int aliasScore = 0;

    for (const auto& alias :
         command.aliases) {
        aliasScore =
            std::max(
                aliasScore,
                MatchScore(
                    alias,
                    normalizedQuery));
    }

    const int keywordInitial =
        DerivedInitialMatchScore(
            command.keyword,
            normalizedQuery);

    const int titleInitial =
        DerivedInitialMatchScore(
            command.title,
            normalizedQuery);

    int aliasInitial = 0;

    for (const auto& alias :
         command.aliases) {
        aliasInitial =
            std::max(
                aliasInitial,
                DerivedInitialMatchScore(
                    alias,
                    normalizedQuery));
    }

    int pinyinScore = 0;

    if (pinyin_.Available() &&
        IsPinyinQuery(normalizedQuery)) {

        const int keywordPinyin =
            PinyinMatchScore(
                command.keyword,
                normalizedQuery);

        const int titlePinyin =
            PinyinMatchScore(
                command.title,
                normalizedQuery);

        int aliasPinyin = 0;

        for (const auto& alias :
             command.aliases) {
            aliasPinyin =
                std::max(
                    aliasPinyin,
                    PinyinMatchScore(
                        alias,
                        normalizedQuery));
        }

        pinyinScore =
            std::max({
                keywordPinyin > 0
                    ? keywordPinyin + 60
                    : 0,
                aliasPinyin > 0
                    ? aliasPinyin + 40
                    : 0,
                titlePinyin
            });
    }

    return std::max({
        keywordScore > 0
            ? keywordScore + 140
            : 0,
        aliasScore > 0
            ? aliasScore + 120
            : 0,
        titleScore,
        targetScore > 0
            ? std::max(1, targetScore - 120)
            : 0,
        keywordInitial > 0
            ? keywordInitial + 45
            : 0,
        aliasInitial > 0
            ? aliasInitial + 25
            : 0,
        titleInitial,
        pinyinScore
    });
}

std::vector<SearchResult> SearchEngine::Search(
    const std::vector<Command>& commands,
    const UsageMap& usage,
    std::wstring_view query,
    std::size_t limit) const {

    std::vector<SearchResult> results;

    results.reserve(
        std::min(
            commands.size(),
            limit * 3));

    const std::wstring normalizedQuery =
        Normalize(query);

    const auto queryTokens =
        QueryTokens(query);

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

        int score =
            command.basePriority +
            UsageScore(stat);

        if (command.pinned) {
            score += 220;
        }

        if (normalizedQuery.empty()) {
            if (stat == nullptr ||
                stat->launches == 0) {
                score -= 100;
            }
        } else {
            int textScore =
                CommandTextScore(
                    command,
                    normalizedQuery);

            if (queryTokens.size() > 1) {
                int weakest =
                    std::numeric_limits<int>::max();

                int total = 0;
                bool allTokensMatched = true;

                for (const auto& token :
                     queryTokens) {
                    const int tokenScore =
                        CommandTextScore(
                            command,
                            token);

                    if (tokenScore <= 0) {
                        allTokensMatched = false;
                        break;
                    }

                    weakest =
                        std::min(
                            weakest,
                            tokenScore);

                    total += tokenScore;
                }

                if (allTokensMatched) {
                    const int average =
                        total /
                        static_cast<int>(
                            queryTokens.size());

                    const int multiTokenScore =
                        std::min(
                            1180,
                            weakest +
                                average / 5 +
                                40);

                    textScore =
                        std::max(
                            textScore,
                            multiTokenScore);
                }
            }

            if (textScore <= 0) {
                continue;
            }

            score += textScore;
        }

        results.push_back({i, score});
    }

    std::stable_sort(
        results.begin(),
        results.end(),
        [&](const SearchResult& a,
            const SearchResult& b) {

            if (a.score != b.score) {
                return a.score > b.score;
            }

            const auto& ca =
                commands[a.commandIndex];

            const auto& cb =
                commands[b.commandIndex];

            if (ca.pinned != cb.pinned) {
                return ca.pinned;
            }

            if (ca.sortOrder != cb.sortOrder) {
                return ca.sortOrder < cb.sortOrder;
            }

            if (ca.keyword != cb.keyword) {
                return ca.keyword < cb.keyword;
            }

            return ca.title < cb.title;
        });

    if (results.size() > limit) {
        results.resize(limit);
    }

    return results;
}

} // namespace altrun
