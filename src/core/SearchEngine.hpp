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
    // Exponentially decayed launch count as of lastUsedUnix: each launch
    // adds 1, and the value halves every kFrecencyHalfLifeDays. 0 means
    // "not tracked yet" (older data); DecayedFrecency then seeds it from
    // the lifetime count.
    double frecency{0.0};
};

inline constexpr double kFrecencyHalfLifeDays = 14.0;

// Current decayed launch count of `stat` at `nowUnix`.
[[nodiscard]] double DecayedFrecency(
    const UsageStat& stat,
    std::int64_t nowUnix) noexcept;

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
    struct PreparedField {
        std::wstring normalized;
        std::wstring initials;
    };
    struct PreparedCommand {
        PreparedField keyword;
        PreparedField title;
        PreparedField target;
        std::vector<PreparedField> aliases;
        std::vector<PreparedField> distinctiveTokens;
    };
    using PreparedIndex = std::vector<PreparedCommand>;

    [[nodiscard]] static PreparedIndex PrepareIndex(
        std::span<const Command> commands);

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
        bool allowPinyin = true,
        const PreparedIndex* prepared = nullptr) const;

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

    // Learned per-query habit (0, or 8..32) for a non-empty normalized
    // query: launches on this exact query, or on a related one (one is a
    // prefix of the other). Shared by catalog search and file results.
    [[nodiscard]] static int IntentUsageScore(
        const UsageStat* stat,
        const std::wstring& normalizedQuery);

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

    [[nodiscard]] static bool
    IsPinyinQuery(
        std::wstring_view normalizedQuery);

    [[nodiscard]] static std::wstring
    WordInitials(
        std::wstring_view field);

    [[nodiscard]] static relevance::Match
    DerivedInitialMatchScore(
        std::wstring_view field,
        std::wstring_view normalizedQuery,
        const std::wstring* prepared = nullptr);

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
        bool allowTarget,
        const PreparedCommand* prepared = nullptr) const;

    [[nodiscard]] static relevance::Match
    CommandWildcardScore(
        const Command& command,
        std::wstring_view normalizedPattern);

    [[nodiscard]] static bool
    HasDistinctiveCatalogIntent(
        const Command& command,
        std::wstring_view normalizedQuery,
        std::span<const std::wstring>
            normalizedQueryTokens,
        const PreparedCommand* prepared = nullptr);

    // Best match of `normalizedQuery` against the command's cached
    // distinctive tokens (exact, or prefix for StrongMatchOnly entries),
    // reported on the Alias field.
    [[nodiscard]] static relevance::Match
    DistinctiveIntentMatch(
        const Command& command,
        const PreparedCommand* prepared,
        std::wstring_view normalizedQuery,
        std::span<const std::wstring> intentTokens);

    [[nodiscard]] static bool
    AdmitCatalogEntry(
        const Command& command,
        std::wstring_view normalizedQuery,
        std::span<const std::wstring>
            normalizedQueryTokens,
        const relevance::Match& match,
        bool explicitSyntax,
        const PreparedCommand* prepared = nullptr,
        bool launchedBefore = false);

    PinyinSearch pinyin_;
};

} // namespace altrun
