#pragma once

#include "Command.hpp"

#include <vector>

namespace altrun {

class ICommandProvider {
public:
    virtual ~ICommandProvider() = default;

    [[nodiscard]] virtual std::vector<Command>
    Discover() const = 0;
};

} // namespace altrun
