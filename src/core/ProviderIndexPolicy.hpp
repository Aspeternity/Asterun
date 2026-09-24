#pragma once

#include "ICommandProvider.hpp"
#include "ProviderCache.hpp"
#include "ProviderIds.hpp"

#include <vector>

namespace altrun {

enum class ProviderIndexState {
    Ready,
    Building,
    Degraded,
};

[[nodiscard]] ProviderIndexState
EvaluateProviderIndexState(
    const std::vector<ProviderDescriptor>&
        descriptors,
    const ProviderEnableMap& enabled,
    const ProviderCacheData& cache,
    bool refreshCompleted);

[[nodiscard]] constexpr bool
ProviderIndexSearchable(
    ProviderIndexState state) noexcept {
    return state !=
        ProviderIndexState::Building;
}

} // namespace altrun
