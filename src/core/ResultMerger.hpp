#pragma once

#include "LauncherResult.hpp"

#include <cstddef>
#include <span>
#include <vector>

namespace altrun {

[[nodiscard]]
std::vector<LauncherResult>
MergeLauncherResultsStaticFirst(
    std::span<const LauncherResult>
        staticResults,
    std::span<const LauncherResult>
        dynamicResults,
    std::size_t limit);

} // namespace altrun
