#pragma once

#include <cstddef>
#include <optional>
#include <span>

namespace altrun {

struct ExplorerContextCandidateState {
    // A navigable source context only needs a real Explorer shell view.
    // The source itself may be a virtual Shell namespace location.
    bool hasShellView{false};
    bool focusMatched{false};
    bool visible{false};
};

[[nodiscard]] std::optional<std::size_t>
SelectExplorerContextCandidate(
    std::span<
        const ExplorerContextCandidateState>
        candidates);

} // namespace altrun
