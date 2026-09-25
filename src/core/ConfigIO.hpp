#pragma once

#include <filesystem>
#include <optional>

#include <nlohmann/json.hpp>

namespace altrun::config {

inline constexpr int kSettingsSchemaVersion = 10;
inline constexpr int kCommandsSchemaVersion = 2;
inline constexpr int kUsageSchemaVersion = 2;

enum class JsonLoadStatus {
    MissingOrInvalid,
    LoadedPrimary,
    RecoveredBackup,
    UnsupportedSchema,
};

struct JsonLoadResult {
    JsonLoadStatus status{
        JsonLoadStatus::MissingOrInvalid};
    std::optional<nlohmann::json> value;
    int schemaVersion{0};

    [[nodiscard]] bool HasValue() const noexcept {
        return value.has_value();
    }
};

[[nodiscard]] JsonLoadResult
LoadJsonWithBackup(
    const std::filesystem::path& path,
    int maxSupportedSchemaVersion);

[[nodiscard]] std::optional<nlohmann::json>
LoadJsonWithBackup(
    const std::filesystem::path& path);

[[nodiscard]] bool SaveJsonAtomic(
    const std::filesystem::path& path,
    const nlohmann::json& value);

} // namespace altrun::config
