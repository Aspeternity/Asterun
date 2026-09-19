#include "core/EverythingIpcProtocol.hpp"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

using namespace altrun::everything_ipc;

namespace {

void AppendU32(
    std::vector<std::byte>& bytes,
    std::uint32_t value) {
    bytes.push_back(
        static_cast<std::byte>(value & 0xffU));
    bytes.push_back(
        static_cast<std::byte>(
            (value >> 8U) & 0xffU));
    bytes.push_back(
        static_cast<std::byte>(
            (value >> 16U) & 0xffU));
    bytes.push_back(
        static_cast<std::byte>(
            (value >> 24U) & 0xffU));
}

void AppendUtf16Field(
    std::vector<std::byte>& bytes,
    std::u16string_view text) {
    AppendU32(
        bytes,
        static_cast<std::uint32_t>(
            text.size()));
    for (const auto ch : text) {
        const auto value =
            static_cast<std::uint16_t>(ch);
        bytes.push_back(
            static_cast<std::byte>(
                value & 0xffU));
        bytes.push_back(
            static_cast<std::byte>(
                (value >> 8U) & 0xffU));
    }
    bytes.push_back(std::byte{0});
    bytes.push_back(std::byte{0});
}

std::vector<std::byte> BuildList2() {
    constexpr std::uint32_t count = 2;
    std::vector<std::byte> bytes;
    bytes.reserve(256);

    AppendU32(bytes, 42);
    AppendU32(bytes, count);
    AppendU32(bytes, 0);
    AppendU32(bytes, kDefaultRequestFlags);
    AppendU32(bytes, kSortNameAscending);

    const auto itemTableOffset =
        bytes.size();
    for (std::uint32_t i = 0;
         i < count;
         ++i) {
        AppendU32(bytes, 0);
        AppendU32(bytes, 0);
    }

    auto writeAt = [&](
        std::size_t offset,
        std::uint32_t value) {
        bytes[offset + 0] =
            static_cast<std::byte>(
                value & 0xffU);
        bytes[offset + 1] =
            static_cast<std::byte>(
                (value >> 8U) & 0xffU);
        bytes[offset + 2] =
            static_cast<std::byte>(
                (value >> 16U) & 0xffU);
        bytes[offset + 3] =
            static_cast<std::byte>(
                (value >> 24U) & 0xffU);
    };

    const auto firstData =
        static_cast<std::uint32_t>(
            bytes.size());
    writeAt(
        itemTableOffset + 0,
        0);
    writeAt(
        itemTableOffset + 4,
        firstData);
    AppendUtf16Field(
        bytes,
        u"CKD论文终稿.docx");
    AppendUtf16Field(
        bytes,
        u"D:\\Research\\CKD");
    AppendUtf16Field(
        bytes,
        u"D:\\Research\\CKD\\CKD论文终稿.docx");

    const auto secondData =
        static_cast<std::uint32_t>(
            bytes.size());
    writeAt(
        itemTableOffset + 8,
        kItemFolder);
    writeAt(
        itemTableOffset + 12,
        secondData);
    AppendUtf16Field(
        bytes,
        u"资料");
    AppendUtf16Field(
        bytes,
        u"D:\\Research");
    AppendUtf16Field(
        bytes,
        u"D:\\Research\\资料");

    return bytes;
}

} // namespace

int main() {
    Query2WireRequest query;
    query.replyHwnd = 0x12345678U;
    query.replyToken = 0xA5010001U;
    query.maxResults = 32;
    query.search = u"论文 docker";

    const auto encoded =
        EncodeQuery2(query);
    assert(
        encoded.size() ==
        sizeof(Query2Header) +
            (query.search.size() + 1U) *
                2U);

    const auto decoded =
        ParseQuery2(encoded);
    assert(decoded);
    assert(
        decoded.value->replyHwnd ==
        query.replyHwnd);
    assert(
        decoded.value->replyToken ==
        query.replyToken);
    assert(
        decoded.value->requestFlags ==
        kDefaultRequestFlags);
    assert(
        decoded.value->sortType ==
        kSortNameAscending);
    assert(
        decoded.value->search ==
        query.search);

    const auto listBytes = BuildList2();
    const auto list =
        ParseList2(listBytes);
    assert(list);
    assert(list.value->totalItems == 42);
    assert(list.value->items.size() == 2);
    assert(!list.value->items[0].folder);
    assert(
        list.value->items[0].name ==
        u"CKD论文终稿.docx");
    assert(
        list.value->items[0].path ==
        u"D:\\Research\\CKD");
    assert(
        list.value->items[0].fullPath ==
        u"D:\\Research\\CKD\\CKD论文终稿.docx");
    assert(list.value->items[1].folder);
    assert(
        list.value->items[1].name ==
        u"资料");

    auto rootListBytes = listBytes;
    const auto secondFlagsPos =
        sizeof(List2Header) +
        sizeof(Item2Header);
    const std::uint32_t rootFlags =
        kItemFolder |
        kItemDriveOrRoot;
    rootListBytes[secondFlagsPos + 0] =
        static_cast<std::byte>(
            rootFlags & 0xffU);
    rootListBytes[secondFlagsPos + 1] =
        static_cast<std::byte>(
            (rootFlags >> 8U) & 0xffU);
    rootListBytes[secondFlagsPos + 2] =
        static_cast<std::byte>(
            (rootFlags >> 16U) & 0xffU);
    rootListBytes[secondFlagsPos + 3] =
        static_cast<std::byte>(
            (rootFlags >> 24U) & 0xffU);

    const auto rootList =
        ParseList2(rootListBytes);
    assert(rootList);
    assert(
        rootList.value->items[1].folder);
    assert(
        rootList.value->items[1].root);

    auto tooManyItems = listBytes;
    tooManyItems[0] = std::byte{1};
    tooManyItems[1] = std::byte{0};
    tooManyItems[2] = std::byte{0};
    tooManyItems[3] = std::byte{0};
    assert(!ParseList2(tooManyItems));

    auto badRange = listBytes;
    // totalItems = 2, offset = 1, numItems = 2 => offset + count > total.
    badRange[0] = std::byte{2};
    badRange[1] = std::byte{0};
    badRange[2] = std::byte{0};
    badRange[3] = std::byte{0};
    badRange[8] = std::byte{1};
    badRange[9] = std::byte{0};
    badRange[10] = std::byte{0};
    badRange[11] = std::byte{0};
    assert(!ParseList2(badRange));

    auto truncated = listBytes;
    truncated.resize(
        truncated.size() - 1U);
    assert(!ParseList2(truncated));

    auto badOffset = listBytes;
    const auto invalid =
        static_cast<std::uint32_t>(
            badOffset.size() + 20U);
    const auto pos =
        sizeof(List2Header) + 4U;
    badOffset[pos + 0] =
        static_cast<std::byte>(
            invalid & 0xffU);
    badOffset[pos + 1] =
        static_cast<std::byte>(
            (invalid >> 8U) & 0xffU);
    badOffset[pos + 2] =
        static_cast<std::byte>(
            (invalid >> 16U) & 0xffU);
    badOffset[pos + 3] =
        static_cast<std::byte>(
            (invalid >> 24U) & 0xffU);
    assert(!ParseList2(badOffset));

    auto badFlags = listBytes;
    const auto flagsPos = 12U;
    const std::uint32_t unsupportedFlags =
        kDefaultRequestFlags | 0x10U;
    badFlags[flagsPos + 0] =
        static_cast<std::byte>(
            unsupportedFlags & 0xffU);
    badFlags[flagsPos + 1] =
        static_cast<std::byte>(
            (unsupportedFlags >> 8U) &
            0xffU);
    badFlags[flagsPos + 2] =
        static_cast<std::byte>(
            (unsupportedFlags >> 16U) &
            0xffU);
    badFlags[flagsPos + 3] =
        static_cast<std::byte>(
            (unsupportedFlags >> 24U) &
            0xffU);
    assert(!ParseList2(badFlags));

    auto unterminatedQuery = encoded;
    unterminatedQuery[
        unterminatedQuery.size() - 1U] =
        std::byte{1};
    assert(!ParseQuery2(
        unterminatedQuery));

    std::cout
        << "Everything IPC protocol tests passed\n";
    return 0;
}
