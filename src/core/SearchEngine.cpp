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

bool SearchEngine::IsAsciiQuery(
    std::wstring_view normalizedQuery) {

    return !normalizedQuery.empty() &&
        std::all_of(
            normalizedQuery.begin(),
            normalizedQuery.end(),
            [](wchar_t ch) {
                return ch <= 0x7F;
            });
}

bool SearchEngine::IsWordBoundary(
    std::wstring_view field,
    std::size_t index) {

    if (index >= field.size()) {
        return false;
    }

    if (index == 0) {
        return true;
    }

    const wchar_t current = field[index];
    const wchar_t previous = field[index - 1];

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
        std::iswlower(field[index + 1]) != 0;

    return currentUpper &&
        (previousLower ||
         (previousUpper &&
          nextLower));
}

bool SearchEngine::NormalizedPrefixAt(
    std::wstring_view field,
    std::size_t start,
    std::wstring_view normalizedQuery) {

    if (start >= field.size() ||
        normalizedQuery.empty()) {
        return false;
    }

    std::size_t fieldIndex = start;
    std::size_t queryIndex = 0;

    while (fieldIndex < field.size() &&
           queryIndex <
               normalizedQuery.size()) {

        const wchar_t ch =
            field[fieldIndex++];

        if (std::iswspace(ch) ||
            ch == L'_' ||
            ch == L'-') {
            continue;
        }

        if (static_cast<wchar_t>(
                std::towlower(ch)) !=
            normalizedQuery[queryIndex]) {
            return false;
        }

        ++queryIndex;
    }

    return queryIndex ==
        normalizedQuery.size();
}

SearchEngine::TextMatch SearchEngine::MatchScore(
    std::wstring_view field,
    std::wstring_view query) {

    if (field.empty() || query.empty()) {
        return {};
    }

    const std::wstring f = Normalize(field);
    const std::wstring q = Normalize(query);

    if (f.empty() || q.empty()) {
        return {};
    }

    if (f == q) {
        return {MatchKind::Exact, 1000};
    }

    if (f.starts_with(q)) {
        return {
            MatchKind::Prefix,
            880 -
                static_cast<int>(
                    std::min<std::size_t>(
                        f.size() - q.size(),
                        120)),
        };
    }

    for (std::size_t i = 1;
         i < field.size();
         ++i) {
        if (IsWordBoundary(field, i) &&
            NormalizedPrefixAt(
                field,
                i,
                q)) {
            return {
                MatchKind::BoundaryPrefix,
                760 -
                    static_cast<int>(
                        std::min<std::size_t>(
                            i,
                            100)),
            };
        }
    }

    const bool asciiQuery =
        IsAsciiQuery(q);

    if (!asciiQuery ||
        q.size() >= 3) {
        const auto pos = f.find(q);

        if (pos != std::wstring::npos) {
            return {
                MatchKind::Substring,
                690 -
                    static_cast<int>(
                        std::min<std::size_t>(
                            pos,
                            100)),
            };
        }
    }

    if (!asciiQuery ||
        q.size() < 3) {
        return {};
    }

    std::size_t qi = 0;
    int gaps = 0;
    int run = 0;
    int bestRun = 0;
    std::size_t first = 0;
    std::size_t previous = 0;
    bool havePrevious = false;

    for (std::size_t i = 0;
         i < f.size() && qi < q.size();
         ++i) {
        if (f[i] != q[qi]) {
            continue;
        }

        if (!havePrevious) {
            first = i;
            run = 1;
        } else if (i == previous + 1) {
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
        ++qi;
    }

    if (qi != q.size()) {
        return {};
    }

    const int fuzzyScore =
        430 +
        bestRun * 18 -
        gaps * 12;

    if (q.size() == 3) {
        const std::size_t span =
            previous - first + 1;

        if (gaps > 2 ||
            span > q.size() + 2 ||
            fuzzyScore < 320) {
            return {};
        }

        return {
            MatchKind::TightFuzzy,
            fuzzyScore,
        };
    }

    if (fuzzyScore < 260) {
        return {};
    }

    return {
        MatchKind::Fuzzy,
        fuzzyScore,
    };
}

SearchEngine::TextMatch
SearchEngine::InitialsMatchScore(
    std::wstring_view initials,
    std::wstring_view normalizedQuery) {

    const std::wstring value =
        Normalize(initials);
    const std::wstring query =
        Normalize(normalizedQuery);

    if (value.empty() ||
        query.empty()) {
        return {};
    }

    if (value == query) {
        return {
            MatchKind::Initials,
            965,
        };
    }

    if (value.starts_with(query)) {
        return {
            MatchKind::Initials,
            845 -
                static_cast<int>(
                    std::min<std::size_t>(
                        value.size() -
                            query.size(),
                        120)),
        };
    }

    return {};
}

bool SearchEngine::HasPathIntent(
    std::wstring_view query) {

    if (query.find_first_of(
            L"\\\\/:") !=
        std::wstring_view::npos) {
        return true;
    }

    const std::wstring lower =
        Normalize(query);

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

bool SearchEngine::GlobMatch(
    std::wstring_view field,
    std::wstring_view pattern) {

    const std::wstring value =
        Normalize(field);

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

SearchEngine::TextMatch
SearchEngine::DerivedInitialMatchScore(
    std::wstring_view field,
    std::wstring_view normalizedQuery) {

    const std::wstring initials =
        WordInitials(field);

    if (initials.size() < 2) {
        return {};
    }

    return InitialsMatchScore(
        initials,
        normalizedQuery);
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

SearchEngine::TextMatch
SearchEngine::PinyinMatchScore(
    std::wstring_view field,
    std::wstring_view normalizedQuery) const {

    const PinyinForms* forms =
        pinyin_.FormsFor(field);

    if (!forms) {
        return {};
    }

    TextMatch best{};

    TextMatch full =
        MatchScore(
            forms->full,
            normalizedQuery);

    if (full.score > 0) {
        full.kind =
            MatchKind::PinyinFull;
        full.score =
            std::max(
                1,
                full.score - 45);
        best = full;
    }

    const std::wstring query =
        Normalize(normalizedQuery);
    const std::wstring initials =
        Normalize(forms->initials);

    TextMatch pinyinInitials{};

    if (!query.empty() &&
        !initials.empty()) {
        if (initials == query) {
            pinyinInitials = {
                MatchKind::PinyinInitials,
                980,
            };
        } else if (
            initials.starts_with(query)) {
            pinyinInitials = {
                MatchKind::PinyinInitials,
                860 -
                    static_cast<int>(
                        std::min<std::size_t>(
                            initials.size() -
                                query.size(),
                            120)),
            };
        }
    }

    if (pinyinInitials.score >
        best.score) {
        best = pinyinInitials;
    }

    const int hybridScore =
        HybridPinyinPrefixScore(
            *forms,
            normalizedQuery);

    if (hybridScore > best.score) {
        best = {
            MatchKind::PinyinHybrid,
            hybridScore,
        };
    }

    return best;
}

SearchEngine::TextMatch
SearchEngine::CommandTextScore(
    const Command& command,
    std::wstring_view normalizedQuery,
    bool allowPinyin,
    bool allowTarget) const {

    if (normalizedQuery.empty()) {
        return {};
    }

    TextMatch best{};

    const auto consider =
        [&](TextMatch match,
            int bonus = 0) {
            if (match.score <= 0) {
                return;
            }

            match.score += bonus;

            if (match.score >
                best.score) {
                best = match;
            }
        };

    consider(
        MatchScore(
            command.keyword,
            normalizedQuery),
        140);

    for (const auto& alias :
         command.aliases) {
        consider(
            MatchScore(
                alias,
                normalizedQuery),
            120);
    }

    consider(
        MatchScore(
            command.title,
            normalizedQuery));

    if (allowTarget) {
        TextMatch target =
            MatchScore(
                command.target,
                normalizedQuery);

        if (target.score > 0) {
            target.score =
                std::max(
                    1,
                    target.score - 120);
            consider(target);
        }
    }

    consider(
        DerivedInitialMatchScore(
            command.keyword,
            normalizedQuery),
        45);

    for (const auto& alias :
         command.aliases) {
        consider(
            DerivedInitialMatchScore(
                alias,
                normalizedQuery),
            25);
    }

    consider(
        DerivedInitialMatchScore(
            command.title,
            normalizedQuery));

    if (allowPinyin &&
        pinyin_.Available() &&
        IsPinyinQuery(normalizedQuery)) {
        consider(
            PinyinMatchScore(
                command.keyword,
                normalizedQuery),
            60);

        for (const auto& alias :
             command.aliases) {
            consider(
                PinyinMatchScore(
                    alias,
                    normalizedQuery),
                40);
        }

        consider(
            PinyinMatchScore(
                command.title,
                normalizedQuery));
    }

    return best;
}

int SearchEngine::CommandWildcardScore(
    const Command& command,
    std::wstring_view normalizedPattern) {

    const int keywordScore =
        WildcardMatchScore(
            command.keyword,
            normalizedPattern);

    int aliasScore = 0;

    for (const auto& alias :
         command.aliases) {
        aliasScore =
            std::max(
                aliasScore,
                WildcardMatchScore(
                    alias,
                    normalizedPattern));
    }

    const int titleScore =
        WildcardMatchScore(
            command.title,
            normalizedPattern);

    const int targetScore =
        WildcardMatchScore(
            command.target,
            normalizedPattern);

    return std::max({
        keywordScore > 0
            ? keywordScore + 140
            : 0,
        aliasScore > 0
            ? aliasScore + 120
            : 0,
        titleScore,
        targetScore > 0
            ? std::max(
                  1,
                  targetScore - 120)
            : 0
    });
}

std::vector<SearchResult> SearchEngine::Search(
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
        Normalize(query);

    const auto queryTokens =
        QueryTokens(query);

    const bool wildcardQuery =
        allowWildcards &&
        normalizedQuery
            .find_first_of(L"*?") !=
            std::wstring::npos;

    const bool allowTarget =
        wildcardQuery ||
        HasPathIntent(query);

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
            TextMatch textMatch{};
            int textScore = 0;

            if (wildcardQuery) {
                textScore =
                    CommandWildcardScore(
                        command,
                        normalizedQuery);
            } else {
                textMatch =
                    CommandTextScore(
                        command,
                        normalizedQuery,
                        allowPinyin,
                        allowTarget);
                textScore =
                    textMatch.score;
            }

            if (!wildcardQuery &&
                queryTokens.size() > 1) {
                int weakest =
                    std::numeric_limits<int>::max();
                int total = 0;
                bool allTokensMatched = true;

                for (const auto& token :
                     queryTokens) {
                    const TextMatch tokenMatch =
                        CommandTextScore(
                            command,
                            token,
                            allowPinyin,
                            allowTarget);

                    if (tokenMatch.score <= 0) {
                        allTokensMatched = false;
                        break;
                    }

                    weakest =
                        std::min(
                            weakest,
                            tokenMatch.score);

                    total +=
                        tokenMatch.score;
                }

                if (!allTokensMatched) {
                    if (textMatch.kind !=
                            MatchKind::PinyinHybrid) {
                        continue;
                    }
                } else {
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
