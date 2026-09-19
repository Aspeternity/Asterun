#pragma once

#include "Command.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace altrun {

struct ProviderCacheEntry {
    std::int64_t generatedAtUnix{0};
    std::vector<Command> commands;
};

using ProviderCacheData =
    std::unordered_map<
        std::string,
        ProviderCacheEntry>;

class ProviderCache {
public:
    explicit ProviderCache(
        std::filesystem::path path);

    [[nodiscard]] ProviderCacheData
    Load() const;

    [[nodiscard]] bool Save(
        const ProviderCacheData& data) const;

    [[nodiscard]] const std::filesystem::path&
    Path() const noexcept {
        return path_;
    }

private:
    std::filesystem::path path_;
};

} // namespace altrun
