#include "ClassicBehavior.hpp"

namespace altrun::classic_behavior {

int QuickLaunchIndexForDigit(
    int digit,
    std::string_view order) noexcept {

    if (digit < 0 || digit > 9) {
        return -1;
    }

    if (order == "zero-to-nine") {
        return digit;
    }

    return digit == 0
        ? 9
        : digit - 1;
}

bool ShouldExecuteSingleResult(
    bool allowImmediateExecution,
    bool enabled,
    bool imeComposing,
    bool queryEmpty,
    bool dynamicQueryPending,
    std::size_t resultCount) noexcept {

    return allowImmediateExecution &&
        enabled &&
        !imeComposing &&
        !queryEmpty &&
        !dynamicQueryPending &&
        resultCount == 1;
}

} // namespace altrun::classic_behavior
