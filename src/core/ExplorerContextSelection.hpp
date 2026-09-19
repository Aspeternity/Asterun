#pragma once

#include <cstddef>
#include <optional>
#include <span>

namespace altrun {

struct ExplorerContextCandidateState {
    bool hasFilesystemFolder{false};
    bool focusMatched{false};
    bool visible{false};
};

[[nodiscard]] std::optional<std::size_t>
SelectExplorerContextCandidate(
    std::span<
        const ExplorerContextCandidateState>
        candidates);

} // namespace altrun
