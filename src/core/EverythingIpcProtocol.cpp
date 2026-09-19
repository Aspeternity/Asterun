#include "EverythingIpcProtocol.hpp"

#include <limits>

namespace altrun::everything_ipc {
namespace {

[[nodiscard]] std::uint32_t ReadU32(
    std::span<const std::byte> bytes,
    std::size_t offset) {
    const auto* p =
        reinterpret_cast<const unsigned char*>(bytes.data() + offset);
    return static_cast<std::uint32_t>(p[0]) |
        (static_cast<std::uint32_t>(p[1]) << 8U) |
        (static_cast<std::uint32_t>(p[2]) << 16U) |
        (static_cast<std::uint32_t>(p[3]) << 24U);
}

void WriteU32(
    std::vector<std::byte>& bytes,
    std::size_t offset,
    std::uint32_t value) {
    bytes[offset + 0] =
        static_cast<std::byte>(value & 0xffU);
    bytes[offset + 1] =
        static_cast<std::byte>((value >> 8U) & 0xffU);
    bytes[offset + 2] =
        static_cast<std::byte>((value >> 16U) & 0xffU);
    bytes[offset + 3] =
        static_cast<std::byte>((value >> 24U) & 0xffU);
}

[[nodiscard]] bool CanRead(
    std::span<const std::byte> bytes,
    std::size_t offset,
    std::size_t size) {
    return offset <= bytes.size() &&
        size <= bytes.size() - offset;
}

[[nodiscard]] ParseResult<std::u16string>
ReadUtf16Field(
    std::span<const std::byte> bytes,
    std::size_t* cursor) {
    if (!cursor ||
        !CanRead(
            bytes,
            *cursor,
            sizeof(std::uint32_t))) {
        return {
            std::nullopt,
            "truncated UTF-16 field length",
        };
    }

    const auto length = ReadU32(bytes, *cursor);
    *cursor += sizeof(std::uint32_t);

    if (length >
        (std::numeric_limits<std::size_t>::max() / 2U) -
            1U) {
        return {
            std::nullopt,
            "UTF-16 field length overflow",
        };
    }

    const auto units =
        static_cast<std::size_t>(length) + 1U;
    const auto byteCount = units * 2U;
    if (!CanRead(bytes, *cursor, byteCount)) {
        return {
            std::nullopt,
            "truncated UTF-16 field data",
        };
    }

    std::u16string value;
    value.reserve(length);
    for (std::size_t i = 0;
         i < static_cast<std::size_t>(length);
         ++i) {
        const auto pos = *cursor + i * 2U;
        const auto lo =
            static_cast<unsigned char>(bytes[pos]);
        const auto hi =
            static_cast<unsigned char>(
                bytes[pos + 1U]);
        value.push_back(static_cast<char16_t>(
            static_cast<std::uint16_t>(lo) |
            (static_cast<std::uint16_t>(hi) << 8U)));
    }

    const auto nullPos =
        *cursor +
        static_cast<std::size_t>(length) * 2U;
    if (bytes[nullPos] != std::byte{0} ||
        bytes[nullPos + 1U] != std::byte{0}) {
        return {
            std::nullopt,
            "UTF-16 field is not null terminated",
        };
    }

    *cursor += byteCount;
    return {std::move(value), {}};
}

} // namespace

std::vector<std::byte>
EncodeQuery2(const Query2WireRequest& request) {
    const auto searchBytes =
        (request.search.size() + 1U) * 2U;
    std::vector<std::byte> bytes(
        sizeof(Query2Header) + searchBytes);

    WriteU32(bytes, 0, request.replyHwnd);
    WriteU32(bytes, 4, request.replyToken);
    WriteU32(bytes, 8, request.searchFlags);
    WriteU32(bytes, 12, request.offset);
    WriteU32(bytes, 16, request.maxResults);
    WriteU32(bytes, 20, request.requestFlags);
    WriteU32(bytes, 24, request.sortType);

    auto cursor = sizeof(Query2Header);
    for (const auto ch : request.search) {
        const auto unit =
            static_cast<std::uint16_t>(ch);
        bytes[cursor++] =
            static_cast<std::byte>(unit & 0xffU);
        bytes[cursor++] =
            static_cast<std::byte>(
                (unit >> 8U) & 0xffU);
    }
    bytes[cursor++] = std::byte{0};
    bytes[cursor] = std::byte{0};

    return bytes;
}

ParseResult<Query2WireRequest>
ParseQuery2(std::span<const std::byte> bytes) {
    if (!CanRead(
            bytes,
            0,
            sizeof(Query2Header) + 2U)) {
        return {
            std::nullopt,
            "query2 payload is truncated",
        };
    }

    Query2WireRequest request;
    request.replyHwnd = ReadU32(bytes, 0);
    request.replyToken = ReadU32(bytes, 4);
    request.searchFlags = ReadU32(bytes, 8);
    request.offset = ReadU32(bytes, 12);
    request.maxResults = ReadU32(bytes, 16);
    request.requestFlags = ReadU32(bytes, 20);
    request.sortType = ReadU32(bytes, 24);

    auto cursor = sizeof(Query2Header);
    bool terminated = false;
    while (CanRead(bytes, cursor, 2U)) {
        const auto lo =
            static_cast<unsigned char>(
                bytes[cursor]);
        const auto hi =
            static_cast<unsigned char>(
                bytes[cursor + 1U]);
        cursor += 2U;
        const auto unit =
            static_cast<std::uint16_t>(lo) |
            (static_cast<std::uint16_t>(hi) << 8U);
        if (unit == 0U) {
            terminated = true;
            break;
        }
        request.search.push_back(
            static_cast<char16_t>(unit));
    }

    if (!terminated) {
        return {
            std::nullopt,
            "query2 search string is not null terminated",
        };
    }

    return {std::move(request), {}};
}

ParseResult<ParsedList2>
ParseList2(std::span<const std::byte> bytes) {
    if (!CanRead(
            bytes,
            0,
            sizeof(List2Header))) {
        return {
            std::nullopt,
            "list2 header is truncated",
        };
    }

    ParsedList2 list;
    list.totalItems = ReadU32(bytes, 0);
    const auto numItems = ReadU32(bytes, 4);
    list.offset = ReadU32(bytes, 8);
    list.requestFlags = ReadU32(bytes, 12);
    list.sortType = ReadU32(bytes, 16);

    if ((list.requestFlags &
         ~kDefaultRequestFlags) != 0U) {
        return {
            std::nullopt,
            "list2 contains unsupported request flags",
        };
    }

    if (numItems >
        (std::numeric_limits<std::size_t>::max() -
         sizeof(List2Header)) /
            sizeof(Item2Header)) {
        return {
            std::nullopt,
            "list2 item count overflow",
        };
    }

    const auto itemTableSize =
        static_cast<std::size_t>(numItems) *
        sizeof(Item2Header);
    if (!CanRead(
            bytes,
            sizeof(List2Header),
            itemTableSize)) {
        return {
            std::nullopt,
            "list2 item table is truncated",
        };
    }

    list.items.reserve(numItems);
    for (std::uint32_t i = 0;
         i < numItems;
         ++i) {
        const auto itemOffset =
            sizeof(List2Header) +
            static_cast<std::size_t>(i) *
                sizeof(Item2Header);
        const auto flags =
            ReadU32(bytes, itemOffset);
        auto cursor = static_cast<std::size_t>(
            ReadU32(bytes, itemOffset + 4U));

        if (cursor <
                sizeof(List2Header) +
                    itemTableSize ||
            cursor > bytes.size()) {
            return {
                std::nullopt,
                "list2 item data offset is invalid",
            };
        }

        ParsedList2Item item;
        item.folder =
            (flags &
             (kItemFolder |
              kItemDriveOrRoot)) != 0U;

        if ((list.requestFlags &
             kRequestName) != 0U) {
            auto field =
                ReadUtf16Field(bytes, &cursor);
            if (!field) {
                return {
                    std::nullopt,
                    "invalid name field: " +
                        field.error,
                };
            }
            item.name =
                std::move(*field.value);
        }
        if ((list.requestFlags &
             kRequestPath) != 0U) {
            auto field =
                ReadUtf16Field(bytes, &cursor);
            if (!field) {
                return {
                    std::nullopt,
                    "invalid path field: " +
                        field.error,
                };
            }
            item.path =
                std::move(*field.value);
        }
        if ((list.requestFlags &
             kRequestFullPathAndName) != 0U) {
            auto field =
                ReadUtf16Field(bytes, &cursor);
            if (!field) {
                return {
                    std::nullopt,
                    "invalid full-path field: " +
                        field.error,
                };
            }
            item.fullPath =
                std::move(*field.value);
        }

        list.items.push_back(
            std::move(item));
    }

    return {std::move(list), {}};
}

} // namespace altrun::everything_ipc
