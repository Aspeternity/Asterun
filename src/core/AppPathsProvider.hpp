#pragma once

#include "ICommandProvider.hpp"

namespace altrun {

class AppPathsProvider final : public ICommandProvider {
public:
    [[nodiscard]] const ProviderDescriptor&
    Descriptor() const noexcept override;

    [[nodiscard]] std::vector<Command>
    Discover() const override;
};

} // namespace altrun
