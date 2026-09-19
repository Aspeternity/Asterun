#pragma once

#include <cstddef>
#include <string_view>

namespace altrun::classic_behavior {

[[nodiscard]] int QuickLaunchIndexForDigit(
    int digit,
    std::string_view order) noexcept;

[[nodiscard]] bool ShouldExecuteSingleResult(
    bool allowImmediateExecution,
    bool enabled,
    bool imeComposing,
    bool queryEmpty,
    std::size_t resultCount) noexcept;

} // namespace altrun::classic_behavior
