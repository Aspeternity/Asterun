#include "ResultMerger.hpp"

#include <algorithm>

namespace altrun {

std::vector<LauncherResult>
MergeLauncherResultsStaticFirst(
    std::span<const LauncherResult>
        staticResults,
    std::span<const LauncherResult>
        dynamicResults,
    std::size_t limit) {
    std::vector<LauncherResult> merged;
    merged.reserve(
        std::min(
            limit,
            staticResults.size() +
                dynamicResults.size()));

    auto appendUnique =
        [&](const LauncherResult& result) {
            if (merged.size() >= limit) {
                return;
            }

            const auto duplicate =
                std::any_of(
                    merged.begin(),
                    merged.end(),
                    [&](const LauncherResult&
                            existing) {
                        if (!result.id.empty() &&
                            existing.providerId ==
                                result.providerId &&
                            existing.id ==
                                result.id) {
                            return true;
                        }

                        return SameLauncherTarget(
                            existing,
                            result);
                    });

            if (!duplicate) {
                merged.push_back(result);
            }
        };

    for (const auto& result :
         staticResults) {
        appendUnique(result);
    }

    for (const auto& result :
         dynamicResults) {
        appendUnique(result);
    }

    return merged;
}

} // namespace altrun
