#pragma once

#include "Command.hpp"
#include "PinyinSearch.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
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
        std::size_t limit = 12) const;

    [[nodiscard]] bool PinyinAvailable() const noexcept {
        return pinyin_.Available();
    }

private:
    [[nodiscard]] static std::wstring Normalize(
        std::wstring_view text);

    [[nodiscard]] static int MatchScore(
        std::wstring_view field,
        std::wstring_view query);

    [[nodiscard]] static int UsageScore(
        const UsageStat* stat);

    [[nodiscard]] static bool IsPinyinQuery(
        std::wstring_view normalizedQuery);

    [[nodiscard]] int PinyinMatchScore(
        std::wstring_view field,
        std::wstring_view normalizedQuery) const;

    PinyinSearch pinyin_;
};

} // namespace altrun
