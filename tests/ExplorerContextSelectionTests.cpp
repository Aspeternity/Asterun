#include "core/ExplorerContextSelection.hpp"

#include <array>
#include <cassert>
#include <iostream>

using namespace altrun;

int main() {
    {
        const std::array candidates{
            ExplorerContextCandidateState{
                true, false, false},
        };

        const auto selected =
            SelectExplorerContextCandidate(
                candidates);

        assert(selected);
        assert(*selected == 0);
    }

    {
        const std::array candidates{
            ExplorerContextCandidateState{
                true, false, true},
            ExplorerContextCandidateState{
                true, true, false},
        };

        const auto selected =
            SelectExplorerContextCandidate(
                candidates);

        assert(selected);
        assert(*selected == 1);
    }

    {
        const std::array candidates{
            ExplorerContextCandidateState{
                true, false, true},
            ExplorerContextCandidateState{
                true, false, false},
        };

        const auto selected =
            SelectExplorerContextCandidate(
                candidates);

        assert(selected);
        assert(*selected == 0);
    }

    {
        const std::array candidates{
            ExplorerContextCandidateState{
                true, true, true},
            ExplorerContextCandidateState{
                true, true, false},
        };

        assert(
            !SelectExplorerContextCandidate(
                candidates));
    }

    {
        const std::array candidates{
            ExplorerContextCandidateState{
                true, false, true},
            ExplorerContextCandidateState{
                true, false, true},
        };

        assert(
            !SelectExplorerContextCandidate(
                candidates));
    }

    {
        const std::array candidates{
            ExplorerContextCandidateState{
                false, true, true},
            ExplorerContextCandidateState{
                false, false, false},
        };

        assert(
            !SelectExplorerContextCandidate(
                candidates));
    }

    std::cout
        << "Explorer context selection tests passed\n";
    return 0;
}
