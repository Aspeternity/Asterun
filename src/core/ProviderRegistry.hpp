#pragma once

#include "ICommandProvider.hpp"
#include "ProviderIds.hpp"

#include <memory>
#include <vector>

namespace altrun {

class ProviderRegistry {
public:
    ProviderRegistry();

    [[nodiscard]] std::vector<ProviderDescriptor>
    Descriptors() const;

    [[nodiscard]] std::vector<ProviderDiscoveryResult>
    Discover(
        const ProviderEnableMap& enabled) const;

private:
    std::vector<
        std::unique_ptr<ICommandProvider>>
        providers_;
};

} // namespace altrun
