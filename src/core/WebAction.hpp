#pragma once

#include "Command.hpp"
#include "LauncherResult.hpp"

#include <cstddef>
#include <span>
#include <string_view>
#include <vector>

namespace altrun {

[[nodiscard]] std::vector<LauncherResult>
BuildWebActionResults(
    std::span<const Command> commands,
    std::wstring_view query,
    std::size_t limit);

} // namespace altrun
