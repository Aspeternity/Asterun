#include "ResultIconPipeline.hpp"

#include <algorithm>

namespace altrun {

std::wstring MakeResultIconCacheKey(
    std::wstring_view source,
    int pixelSize) {
    std::wstring key(source);
    key.push_back(
        static_cast<wchar_t>(
            0x001F));
    key += std::to_wstring(
        std::max(pixelSize, 0));
    return key;
}

bool ShouldAcceptResultIconCompletion(
    bool showResultIcons,
    const ResultIconRequestStamp& completed,
    std::uint64_t currentSearchGeneration,
    std::uint64_t currentIconEpoch) {
    return showResultIcons &&
        completed.searchGeneration ==
            currentSearchGeneration &&
        completed.iconEpoch ==
            currentIconEpoch &&
        completed.pixelSize > 0;
}

} // namespace altrun
