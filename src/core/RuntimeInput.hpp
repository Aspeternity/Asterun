#pragma once

#include "Command.hpp"
#include "LauncherResult.hpp"

#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace altrun {

inline constexpr std::wstring_view
    kRuntimeInputPlaceholder =
        L"{input}";

inline constexpr std::wstring_view
    kLegacyQueryPlaceholder =
        L"{query}";

[[nodiscard]] bool
HasRuntimeInputPlaceholder(
    const Command& command);

[[nodiscard]] bool
CanAcceptRuntimeInput(
    const Command& command);

[[nodiscard]] std::wstring
EncodeRuntimeInput(
    RuntimeInputMode mode,
    std::wstring_view input);

[[nodiscard]] Command
ResolveRuntimeInput(
    const Command& command,
    std::wstring_view input);

[[nodiscard]]
std::vector<LauncherResult>
BuildRuntimeInputActionResults(
    std::span<const Command> commands,
    std::wstring_view query,
    std::size_t limit);

} // namespace altrun
