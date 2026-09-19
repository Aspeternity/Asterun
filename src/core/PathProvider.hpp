#pragma once

#include "ICommandProvider.hpp"

namespace altrun {

class PathProvider final :
    public ICommandProvider {
public:
    [[nodiscard]] const ProviderDescriptor&
    Descriptor() const noexcept override;

    [[nodiscard]] std::vector<Command>
    Discover() const override;

    [[nodiscard]] std::uint64_t
    ChangeToken() const override;
};

} // namespace altrun
