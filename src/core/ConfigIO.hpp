#pragma once

#include <filesystem>
#include <optional>

#include <nlohmann/json.hpp>

namespace altrun::config {

inline constexpr int kSettingsSchemaVersion = 12;
inline constexpr int kCommandsSchemaVersion = 2;
inline constexpr int kUsageSchemaVersion = 2;

enum class JsonLoadStatus {
    MissingOrInvalid,
    InvalidExisting,
    LoadedPrimary,
    RecoveredBackup,
    UnsupportedSchema,
};

struct JsonLoadResult {
    JsonLoadStatus status{
        JsonLoadStatus::MissingOrInvalid};
    std::optional<nlohmann::json> value;
    int schemaVersion{0};

    bool primaryRepaired{false};
};

using JsonValidator = bool (*)(const nlohmann::json&);

[[nodiscard]] JsonLoadResult
LoadJsonWithBackup(
    const std::filesystem::path& path,
    int maxSupportedSchemaVersion,
    JsonValidator validator = nullptr);

[[nodiscard]] std::optional<nlohmann::json>
LoadJsonWithBackup(
    const std::filesystem::path& path);

[[nodiscard]] bool SaveJsonAtomic(
    const std::filesystem::path& path,
    const nlohmann::json& value,
    JsonValidator validator = nullptr);

} // namespace altrun::config
