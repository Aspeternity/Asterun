#pragma once

#include "ICommandProvider.hpp"
#include "ProviderIds.hpp"

#include <memory>
#include <string>
#include <vector>

namespace altrun {

class ProviderRegistry {
public:
    ProviderRegistry();

    [[nodiscard]] std::vector<ProviderDescriptor>
    Descriptors() const;

    [[nodiscard]] std::vector<ProviderDiscoveryResult>
    Discover(
        const ProviderEnableMap& enabled,
        const std::vector<std::string>&
            selectedIds = {}) const;

    [[nodiscard]] std::vector<ProviderChangeToken>
    ChangeTokens(
        const ProviderEnableMap& enabled) const;

private:
    std::vector<
        std::unique_ptr<ICommandProvider>>
        providers_;
};

} // namespace altrun
