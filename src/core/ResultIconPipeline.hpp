#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace altrun {

inline constexpr std::size_t
    kResultIconCacheCapacity = 96;

struct ResultIconRequestStamp {
    std::uint64_t
        searchGeneration{0};
    std::uint64_t
        iconEpoch{0};
    int pixelSize{0};
};

[[nodiscard]] std::wstring
MakeResultIconCacheKey(
    std::wstring_view source,
    int pixelSize);

[[nodiscard]] bool
ShouldAcceptResultIconCompletion(
    bool showResultIcons,
    const ResultIconRequestStamp& completed,
    std::uint64_t currentSearchGeneration,
    std::uint64_t currentIconEpoch);

} // namespace altrun
