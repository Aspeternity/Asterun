#include "SearchEngine.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cwctype>

namespace altrun {

std::wstring SearchEngine::Normalize(std::wstring_view text) {
    std::wstring out;
    out.reserve(text.size());

    for (const wchar_t ch : text) {
        if (std::iswspace(ch) || ch == L'_' || ch == L'-') {
            continue;
        }
        out.push_back(static_cast<wchar_t>(std::towlower(ch)));
    }
    return out;
}

int SearchEngine::MatchScore(std::wstring_view field, std::wstring_view query) {
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
        return 880 - static_cast<int>(std::min<std::size_t>(f.size() - q.size(), 120));
    }

    const auto pos = f.find(q);
    if (pos != std::wstring::npos) {
        return 690 - static_cast<int>(std::min<std::size_t>(pos, 100));
    }

    // Lightweight ordered-subsequence match. This gives a useful v0.1 fuzzy
    // search without pulling a heavyweight search dependency into the core.
    std::size_t qi = 0;
    int gaps = 0;
    int run = 0;
    int bestRun = 0;
    std::size_t previous = 0;
    bool havePrevious = false;

    for (std::size_t i = 0; i < f.size() && qi < q.size(); ++i) {
        if (f[i] == q[qi]) {
            if (havePrevious) {
                if (i == previous + 1) {
                    ++run;
                } else {
                    gaps += static_cast<int>(i - previous - 1);
                    run = 1;
                }
            } else {
                run = 1;
            }
            bestRun = std::max(bestRun, run);
            previous = i;
            havePrevious = true;
            ++qi;
        }
    }

    if (qi != q.size()) {
        return 0;
    }

    return std::max(180, 430 + bestRun * 18 - gaps * 8);
}

int SearchEngine::UsageScore(const UsageStat* stat) {
    if (stat == nullptr || stat->launches == 0) {
        return 0;
    }

    int score = static_cast<int>(std::min<double>(140.0, std::log2(static_cast<double>(stat->launches) + 1.0) * 24.0));

    if (stat->lastUsedUnix > 0) {
        const auto now = std::chrono::system_clock::now();
        const auto nowUnix = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
        const auto age = std::max<std::int64_t>(0, nowUnix - stat->lastUsedUnix);
        const auto days = age / 86400;
        if (days == 0) score += 90;
        else if (days <= 2) score += 70;
        else if (days <= 7) score += 45;
        else if (days <= 30) score += 20;
    }

    return score;
}

std::vector<SearchResult> SearchEngine::Search(
    const std::vector<Command>& commands,
    const UsageMap& usage,
    std::wstring_view query,
    std::size_t limit) const {

    std::vector<SearchResult> results;
    results.reserve(std::min(commands.size(), limit * 3));

    const std::wstring normalizedQuery = Normalize(query);

    for (std::size_t i = 0; i < commands.size(); ++i) {
        const auto& command = commands[i];
        const auto usageIt = usage.find(command.id);
        const UsageStat* stat = usageIt == usage.end() ? nullptr : &usageIt->second;

        int score = command.basePriority + UsageScore(stat);

        if (normalizedQuery.empty()) {
            // Empty query behaves like classic ALTRun's "frequent/recent" list.
            if (stat == nullptr || stat->launches == 0) {
                score -= 100;
            }
        } else {
            const int keywordScore = MatchScore(command.keyword, normalizedQuery);
            const int titleScore = MatchScore(command.title, normalizedQuery);
            const int targetScore = MatchScore(command.target, normalizedQuery);

            const int textScore = std::max({keywordScore + 140, titleScore, targetScore - 120});
            if (textScore <= 0) {
                continue;
            }
            score += textScore;
        }

        results.push_back({i, score});
    }

    std::stable_sort(results.begin(), results.end(), [&](const SearchResult& a, const SearchResult& b) {
        if (a.score != b.score) {
            return a.score > b.score;
        }
        const auto& ca = commands[a.commandIndex];
        const auto& cb = commands[b.commandIndex];
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
