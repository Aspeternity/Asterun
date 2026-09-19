#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace altrun::everything_ipc {

inline constexpr std::uint32_t kCopyDataQuery2W = 18;
inline constexpr std::uint32_t kRequestName = 0x00000001;
inline constexpr std::uint32_t kRequestPath = 0x00000002;
inline constexpr std::uint32_t kRequestFullPathAndName = 0x00000004;
inline constexpr std::uint32_t kDefaultRequestFlags =
    kRequestName | kRequestPath | kRequestFullPathAndName;
inline constexpr std::uint32_t kSortNameAscending = 1;
inline constexpr std::uint32_t kItemFolder = 0x00000001;
inline constexpr std::uint32_t kItemDriveOrRoot = 0x00000002;

#pragma pack(push, 1)
struct Query2Header {
    std::uint32_t replyHwnd;
    std::uint32_t replyCopyDataMessage;
    std::uint32_t searchFlags;
    std::uint32_t offset;
    std::uint32_t maxResults;
    std::uint32_t requestFlags;
    std::uint32_t sortType;
};

struct List2Header {
    std::uint32_t totalItems;
    std::uint32_t numItems;
    std::uint32_t offset;
    std::uint32_t requestFlags;
    std::uint32_t sortType;
};

struct Item2Header {
    std::uint32_t flags;
    std::uint32_t dataOffset;
};
#pragma pack(pop)

static_assert(sizeof(Query2Header) == 28);
static_assert(sizeof(List2Header) == 20);
static_assert(sizeof(Item2Header) == 8);

struct Query2WireRequest {
    std::uint32_t replyHwnd{0};
    std::uint32_t replyToken{0};
    std::uint32_t searchFlags{0};
    std::uint32_t offset{0};
    std::uint32_t maxResults{32};
    std::uint32_t requestFlags{kDefaultRequestFlags};
    std::uint32_t sortType{kSortNameAscending};
    std::u16string search;
};

struct ParsedList2Item {
    bool folder{false};
    bool root{false};
    std::u16string name;
    std::u16string path;
    std::u16string fullPath;
};

struct ParsedList2 {
    std::uint32_t totalItems{0};
    std::uint32_t offset{0};
    std::uint32_t requestFlags{0};
    std::uint32_t sortType{0};
    std::vector<ParsedList2Item> items;
};

template <typename T>
struct ParseResult {
    std::optional<T> value;
    std::string error;

    [[nodiscard]] explicit operator bool() const noexcept {
        return value.has_value();
    }
};

[[nodiscard]] std::vector<std::byte>
EncodeQuery2(const Query2WireRequest& request);

[[nodiscard]] ParseResult<Query2WireRequest>
ParseQuery2(std::span<const std::byte> bytes);

[[nodiscard]] ParseResult<ParsedList2>
ParseList2(std::span<const std::byte> bytes);

} // namespace altrun::everything_ipc
