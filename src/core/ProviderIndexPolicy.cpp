#include "ProviderIndexPolicy.hpp"

namespace altrun {

ProviderIndexState
EvaluateProviderIndexState(
    const std::vector<ProviderDescriptor>&
        descriptors,
    const ProviderEnableMap& enabled,
    const ProviderCacheData& cache,
    bool refreshCompleted) {

    for (const auto& descriptor :
         descriptors) {
        if (!providers::IsEnabled(
                enabled,
                descriptor.id,
                descriptor.defaultEnabled)) {
            continue;
        }

        if (cache.find(descriptor.id) ==
            cache.end()) {
            return refreshCompleted
                ? ProviderIndexState::
                      Degraded
                : ProviderIndexState::
                      Building;
        }
    }

    return ProviderIndexState::Ready;
}

} // namespace altrun
