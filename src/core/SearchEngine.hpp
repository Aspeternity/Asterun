#pragma once

#include "Command.hpp"
#include "PinyinSearch.hpp"
#include "RelevancePolicy.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace altrun {

struct UsageStat {
    std::uint64_t launches{0};
    std::int64_t lastUsedUnix{0};
    // Bounded per-query preference evidence; competing choices decay it.
    // Global launches above remain a lifetime counter.
    std::unordered_map<std::wstring, std::uint32_t>
        queryLaunches;
};

struct SearchResult {
    std::size_t commandIndex{0};
    int score{0};
    relevance::Match relevanceMatch;
    int usageScore{0};
};

using UsageMap =
    std::unordered_map<
        std::wstring,
        UsageStat>;

class SearchEngine {
public:
    explicit SearchEngine(
        std::filesystem::path
            pinyinDictionaryDirectory = {});

    [[nodiscard]]
    std::vector<SearchResult> Search(
        std::span<const Command> commands,
        const UsageMap& usage,
        std::wstring_view query,
        std::size_t limit = 12,
        bool allowWildcards = false,
        bool allowPinyin = true) const;

    [[nodiscard]] bool PinyinLoaded()
        const noexcept {
        return pinyin_.Loaded();
    }

    [[nodiscard]] bool PinyinAvailable()
        const noexcept {
        return pinyin_.Available();
    }

    [[nodiscard]] std::size_t
    PinyinCacheEntryCount() const noexcept {
        return pinyin_.CacheEntryCount();
    }

    void ReleasePinyinResources() noexcept {
        pinyin_.Unload();
    }

private:
    [[nodiscard]] static bool GlobMatch(
        std::wstring_view field,
        std::wstring_view pattern);

    [[nodiscard]] static int
    WildcardMatchScore(
        std::wstring_view field,
        std::wstring_view normalizedPattern);

    [[nodiscard]] static int UsageScore(
        const UsageStat* stat,
        std::int64_t nowUnix);

    [[nodiscard]] static int IntentUsageScore(
        const UsageStat* stat,
        const std::wstring& normalizedQuery);

    [[nodiscard]] static bool
    IsPinyinQuery(
        std::wstring_view normalizedQuery);

    [[nodiscard]] static std::wstring
    WordInitials(
        std::wstring_view field);

    [[nodiscard]] static relevance::Match
    DerivedInitialMatchScore(
        std::wstring_view field,
        std::wstring_view normalizedQuery);

    [[nodiscard]] static int
    HybridPinyinPrefixScore(
        const PinyinForms& forms,
        std::wstring_view normalizedQuery);

    [[nodiscard]] relevance::Match
    PinyinMatchScore(
        std::wstring_view field,
        std::wstring_view normalizedQuery) const;

    [[nodiscard]] relevance::Match
    CommandTextScore(
        const Command& command,
        std::wstring_view normalizedQuery,
        bool usePinyin,
        bool allowTarget) const;

    [[nodiscard]] static relevance::Match
    CommandWildcardScore(
        const Command& command,
        std::wstring_view normalizedPattern);

    [[nodiscard]] static bool
    HasDistinctiveCatalogIntent(
        const Command& command,
        std::wstring_view normalizedQuery,
        std::span<const std::wstring>
            normalizedQueryTokens);

    [[nodiscard]] static bool
    AdmitCatalogEntry(
        const Command& command,
        std::wstring_view normalizedQuery,
        std::span<const std::wstring>
            normalizedQueryTokens,
        const relevance::Match& match,
        bool explicitSyntax);

    PinyinSearch pinyin_;
};

} // namespace altrun
