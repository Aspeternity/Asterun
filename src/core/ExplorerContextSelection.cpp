#include "ExplorerContextSelection.hpp"

#include <vector>

namespace altrun {

std::optional<std::size_t>
SelectExplorerContextCandidate(
    std::span<
        const ExplorerContextCandidateState>
        candidates) {
    std::vector<std::size_t> valid;
    std::vector<std::size_t> focused;
    std::vector<std::size_t> visible;

    valid.reserve(candidates.size());
    focused.reserve(candidates.size());
    visible.reserve(candidates.size());

    for (std::size_t i = 0;
         i < candidates.size();
         ++i) {
        const auto& candidate =
            candidates[i];

        if (!candidate
                 .hasFilesystemFolder) {
            continue;
        }

        valid.push_back(i);

        if (candidate.focusMatched) {
            focused.push_back(i);
        }

        if (candidate.visible) {
            visible.push_back(i);
        }
    }

    // Focus is the strongest signal for the active Explorer tab/view.
    if (focused.size() == 1) {
        return focused.front();
    }

    if (focused.size() > 1) {
        return std::nullopt;
    }

    // If focus sits in the address bar/navigation chrome, the active shell
    // view can still be the only visible candidate.
    if (visible.size() == 1) {
        return visible.front();
    }

    if (visible.size() > 1) {
        return std::nullopt;
    }

    // A single unambiguous Explorer candidate is safe even without a focus
    // or visibility signal. Never guess between multiple candidates.
    if (valid.size() == 1) {
        return valid.front();
    }

    return std::nullopt;
}

} // namespace altrun
