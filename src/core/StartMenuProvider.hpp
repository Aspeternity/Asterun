#pragma once

#include "ICommandProvider.hpp"

#include <filesystem>
#include <vector>

namespace altrun {

class StartMenuProvider final :
    public ICommandProvider {
public:
    [[nodiscard]] const ProviderDescriptor&
    Descriptor() const noexcept override;

    [[nodiscard]] std::vector<Command>
    Discover() const override;

    [[nodiscard]] std::uint64_t
    ChangeToken() const override;

private:
    void ScanPath(
        const std::filesystem::path& root,
        std::vector<Command>& output) const;

    void FingerprintPath(
        const std::filesystem::path& root,
        std::vector<std::uint64_t>& items) const;
};

} // namespace altrun
