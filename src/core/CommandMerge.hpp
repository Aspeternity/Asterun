#pragma once

#include "Command.hpp"

#include <cstddef>
#include <vector>

namespace altrun {

struct CommandMergeStats {
    std::size_t acceptedUser{0};
    std::size_t acceptedStartMenu{0};
    std::size_t acceptedPackaged{0};
    std::size_t acceptedAppPaths{0};
    std::size_t acceptedPath{0};

    std::size_t suppressedStartMenu{0};
    std::size_t suppressedPackaged{0};
    std::size_t suppressedAppPaths{0};
    std::size_t suppressedPath{0};

    [[nodiscard]] std::size_t
    Accepted(CommandSource source) const noexcept;

    [[nodiscard]] std::size_t
    Suppressed(CommandSource source) const noexcept;
};

struct CommandMergeResult {
    std::vector<Command> commands;
    CommandMergeStats stats;
};

[[nodiscard]] CommandMergeResult
MergeCommands(
    const std::vector<Command>& userCommands,
    const std::vector<Command>& providerCommands);

} // namespace altrun
