#pragma once

#include "Command.hpp"
#include "PinyinSearch.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace altrun {

struct UsageStat {
    std::uint64_t launches{0};
    std::int64_t lastUsedUnix{0};
};

struct SearchResult {
    std::size_t commandIndex{0};
    int score{0};
};

using UsageMap = std::unordered_map<std::wstring, UsageStat>;

class SearchEngine {
public:
    explicit SearchEngine(
        std::filesystem::path pinyinDictionaryDirectory = {});

    [[nodiscard]] std::vector<SearchResult> Search(
        const std::vector<Command>& commands,
        const UsageMap& usage,
        std::wstring_view query,
        std::size_t limit = 12,
        bool allowWildcards = false,
        bool allowPinyin = true) const;

    [[nodiscard]] bool PinyinLoaded() const noexcept {
        return pinyin_.Loaded();
    }

    [[nodiscard]] bool PinyinAvailable() const noexcept {
        return pinyin_.Available();
    }

    [[nodiscard]] std::size_t PinyinCacheEntryCount() const noexcept {
        return pinyin_.CacheEntryCount();
    }

    void ReleasePinyinResources() noexcept {
        pinyin_.Unload();
    }

private:
    enum class MatchKind {
        None,
        Exact,
        Prefix,
        BoundaryPrefix,
        Substring,
        Initials,
        TightFuzzy,
        Fuzzy,
        PinyinFull,
        PinyinInitials,
        PinyinHybrid,
    };

    struct TextMatch {
        MatchKind kind{MatchKind::None};
        int score{0};
    };

    [[nodiscard]] static std::wstring Normalize(
        std::wstring_view text);

    [[nodiscard]] static std::vector<std::wstring> QueryTokens(
        std::wstring_view text);

    [[nodiscard]] static bool IsAsciiQuery(
        std::wstring_view normalizedQuery);

    [[nodiscard]] static bool IsWordBoundary(
        std::wstring_view field,
        std::size_t index);

    [[nodiscard]] static bool NormalizedPrefixAt(
        std::wstring_view field,
        std::size_t start,
        std::wstring_view normalizedQuery);

    [[nodiscard]] static TextMatch MatchScore(
        std::wstring_view field,
        std::wstring_view query);

    [[nodiscard]] static TextMatch InitialsMatchScore(
        std::wstring_view initials,
        std::wstring_view normalizedQuery);

    [[nodiscard]] static bool HasPathIntent(
        std::wstring_view query);

    [[nodiscard]] static bool GlobMatch(
        std::wstring_view field,
        std::wstring_view pattern);

    [[nodiscard]] static int WildcardMatchScore(
        std::wstring_view field,
        std::wstring_view normalizedPattern);

    [[nodiscard]] static int UsageScore(
        const UsageStat* stat);

    [[nodiscard]] static bool IsPinyinQuery(
        std::wstring_view normalizedQuery);

    [[nodiscard]] static std::wstring WordInitials(
        std::wstring_view field);

    [[nodiscard]] static TextMatch DerivedInitialMatchScore(
        std::wstring_view field,
        std::wstring_view normalizedQuery);

    [[nodiscard]] static int HybridPinyinPrefixScore(
        const PinyinForms& forms,
        std::wstring_view normalizedQuery);

    [[nodiscard]] TextMatch PinyinMatchScore(
        std::wstring_view field,
        std::wstring_view normalizedQuery) const;

    [[nodiscard]] TextMatch CommandTextScore(
        const Command& command,
        std::wstring_view normalizedQuery,
        bool allowPinyin,
        bool allowTarget) const;

    [[nodiscard]] static int CommandWildcardScore(
        const Command& command,
        std::wstring_view normalizedPattern);

    PinyinSearch pinyin_;
};

} // namespace altrun
