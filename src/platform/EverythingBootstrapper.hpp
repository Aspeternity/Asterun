#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <stop_token>

namespace altrun::win {

enum class EverythingBootstrapStage {
    Idle,
    Discovering,
    StartingExisting,
    DownloadingManifest,
    DownloadingPackage,
    VerifyingPackage,
    ExtractingPackage,
    StartingManaged,
    WaitingForIpc,
    Ready,
    NeedsInstall,
    Failed,
};

enum class EverythingBootstrapSource {
    None,
    Managed,
    Registry,
    ProgramFiles,
    Path,
    Downloaded,
};

enum class EverythingBootstrapFailure {
    None,
    Cancelled,
    NotFound,
    IpcUnavailable,
    CreateDirectoryFailed,
    ExistingLaunchFailed,
    ManifestDownloadFailed,
    PackageChecksumMissing,
    PackageDownloadFailed,
    PackageHashFailed,
    PackageHashMismatch,
    ExtractionFailed,
    ManagedExecutableMissing,
    ManagedLaunchFailed,
};

struct EverythingBootstrapSnapshot {
    EverythingBootstrapStage stage{
        EverythingBootstrapStage::Idle};
    EverythingBootstrapSource source{
        EverythingBootstrapSource::None};
    EverythingBootstrapFailure failure{
        EverythingBootstrapFailure::None};
    bool running{false};
    bool downloaded{false};
    std::uint64_t downloadedBytes{0};
    std::uint64_t totalBytes{0};
    std::uint32_t nativeError{0};
    std::filesystem::path executablePath;
};

using EverythingBootstrapProgress =
    std::function<void(
        const EverythingBootstrapSnapshot&)>;

[[nodiscard]] EverythingBootstrapSnapshot
RunEverythingBootstrap(
    const std::filesystem::path& dataDirectory,
    bool allowDownload,
    EverythingBootstrapProgress progress,
    std::stop_token stopToken = {});

[[nodiscard]] std::filesystem::path
ManagedEverythingExecutable(
    const std::filesystem::path& dataDirectory);

[[nodiscard]] bool
EverythingIpcEndpointAvailable();

} // namespace altrun::win
