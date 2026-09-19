#pragma once

#include "LauncherResult.hpp"

#include <cstddef>
#include <string_view>
#include <vector>

namespace altrun {

[[nodiscard]]
std::vector<LauncherResult>
BuildClipboardActionResults(
    std::wstring_view query,
    std::size_t limit,
    std::wstring_view actionTitle);

} // namespace altrun
