#pragma once

#include "Command.hpp"

#include <filesystem>
#include <vector>

namespace altrun {

class StartMenuProvider {
public:
    [[nodiscard]] std::vector<Command> Discover() const;

private:
    void ScanPath(
        const std::filesystem::path& root,
        std::vector<Command>& output) const;
};

} // namespace altrun
