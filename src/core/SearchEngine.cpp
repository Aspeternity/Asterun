#include "SearchEngine.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cwctype>
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

    // Lightweight ordered-subsequence matching remains the final fallback.
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

    return std::max(
        fullScore,
        initialsScore);
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

    const bool usePinyin =
        pinyin_.Available() &&
        IsPinyinQuery(normalizedQuery);

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
            // Empty query behaves like classic ALTRun's frequent/recent list.
            if (stat == nullptr ||
                stat->launches == 0) {
                score -= 100;
            }
        } else {
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

            int pinyinScore = 0;

            if (usePinyin) {
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

            const int textScore =
                std::max({
                    keywordScore + 140,
                    aliasScore + 120,
                    titleScore,
                    targetScore - 120,
                    pinyinScore
                });

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
