#pragma once

#include "Command.hpp"

#include <filesystem>
#include <vector>

namespace altrun {

class ProviderCache {
public:
    explicit ProviderCache(std::filesystem::path path);

    [[nodiscard]] std::vector<Command> Load() const;
    [[nodiscard]] bool Save(
        const std::vector<Command>& commands) const;

    [[nodiscard]] const std::filesystem::path& Path() const noexcept {
        return path_;
    }

private:
    std::filesystem::path path_;
};

} // namespace altrun
