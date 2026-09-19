#pragma once

#include "Command.hpp"

#include <string>
#include <vector>

namespace altrun {

struct ProviderDescriptor {
    std::string id;
    std::wstring name;
    bool defaultEnabled{true};
    int priority{0};
};

struct ProviderDiscoveryResult {
    std::string id;
    bool success{false};
    std::vector<Command> commands;
    std::wstring error;
};

class ICommandProvider {
public:
    virtual ~ICommandProvider() = default;

    [[nodiscard]] virtual const ProviderDescriptor&
    Descriptor() const noexcept = 0;

    [[nodiscard]] virtual std::vector<Command>
    Discover() const = 0;
};

} // namespace altrun
