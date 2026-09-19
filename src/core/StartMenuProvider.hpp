#pragma once

#include "ICommandProvider.hpp"

#include <filesystem>
#include <vector>

namespace altrun {

class StartMenuProvider final : public ICommandProvider {
public:
    [[nodiscard]] std::vector<Command>
    Discover() const override;

private:
    void ScanPath(
        const std::filesystem::path& root,
        std::vector<Command>& output) const;
};

} // namespace altrun
