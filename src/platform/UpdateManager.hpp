#pragma once

#include "../core/UpdatePolicy.hpp"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <stop_token>
#include <string>

namespace altrun::win {

enum class UpdateStage {
    Idle,
    Checking,
    UpToDate,
    Available,
    Downloading,
    Verifying,
    Extracting,
    ReadyToInstall,
    Applying,
    Failed,
};

enum class UpdateFailure {
    None,
    Cancelled,
    ManifestDownloadFailed,
    ManifestInvalid,
    UnsupportedArchitecture,
    AssetDownloadFailed,
    AssetHashFailed,
    AssetHashMismatch,
    ExtractionFailed,
    StagedPackageInvalid,
    UpdaterMissing,
    LaunchUpdaterFailed,
};

struct UpdateSnapshot {
    UpdateStage stage{
        UpdateStage::Idle};
    UpdateFailure failure{
        UpdateFailure::None};
    bool running{false};
    std::string currentVersion;
    std::string availableVersion;
    std::uint64_t downloadedBytes{0};
    std::uint64_t totalBytes{0};
    std::uint32_t nativeError{0};
    std::filesystem::path
        stagingDirectory;
};

using UpdateProgress =
    std::function<void(
        const UpdateSnapshot&)>;

struct UpdateCheckResult {
    UpdateSnapshot snapshot;
    std::optional<UpdateManifest>
        manifest;
};

struct UpdatePrepareResult {
    UpdateSnapshot snapshot;
};

[[nodiscard]] bool
UpdateAutoCheckDue(
    const std::filesystem::path& dataDirectory,
    std::int64_t nowUnixSeconds,
    std::int64_t intervalSeconds =
        24 * 60 * 60);

[[nodiscard]] UpdateCheckResult
CheckForUpdate(
    const std::filesystem::path& dataDirectory,
    std::string_view currentVersion,
    UpdateChannel channel,
    UpdateProgress progress,
    std::stop_token stopToken = {});

[[nodiscard]] UpdatePrepareResult
PrepareUpdate(
    const std::filesystem::path& dataDirectory,
    UpdateChannel channel,
    const UpdateManifest& manifest,
    std::string_view currentVersion,
    UpdateProgress progress,
    std::stop_token stopToken = {});

[[nodiscard]] bool
LaunchPreparedUpdate(
    const std::filesystem::path& baseDirectory,
    const std::filesystem::path& dataDirectory,
    const UpdateSnapshot& snapshot,
    std::string_view currentVersion,
    std::uint32_t parentProcessId,
    std::uint32_t& nativeError);

} // namespace altrun::win
