#pragma once

#include <filesystem>
#include <optional>

#include <nlohmann/json.hpp>

namespace altrun::config {

inline constexpr int kSchemaVersion = 1;

[[nodiscard]] std::optional<nlohmann::json> LoadJsonWithBackup(
    const std::filesystem::path& path);

[[nodiscard]] bool SaveJsonAtomic(
    const std::filesystem::path& path,
    const nlohmann::json& value);

} // namespace altrun::config
