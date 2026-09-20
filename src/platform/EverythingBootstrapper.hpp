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
    ConfiguringManaged,
    StoppingManaged,
    InstallingService,
    RepairingService,
    WaitingForService,
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
    PackageStagingFailed,
    ExtractionFailed,
    ManagedExecutableMissing,
    ManagedStopFailed,
    ManagedConfigFailed,
    ServiceRequired,
    ServiceRepairRequired,
    ServiceElevationCancelled,
    ServiceInstallFailed,
    ServiceRepairFailed,
    ServiceUnavailable,
    ManagedLaunchFailed,
};

enum class ManagedEverythingStopStatus {
    NotInstalled,
    NotRunning,
    Stopped,
    Failed,
};

struct ManagedEverythingStopResult {
    ManagedEverythingStopStatus status{
        ManagedEverythingStopStatus::
            NotInstalled};
    std::uint32_t nativeError{0};
};

struct EverythingServiceRepairResult {
    bool success{false};
    std::uint32_t nativeError{0};
};

enum class ManagedEverythingServicePolicyStatus {
    NotInstalled,
    External,
    AlreadyConfigured,
    Applied,
    ElevationCancelled,
    Failed,
};

struct ManagedEverythingServicePolicyResult {
    ManagedEverythingServicePolicyStatus status{
        ManagedEverythingServicePolicyStatus::
            NotInstalled};
    std::uint32_t nativeError{0};
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

[[nodiscard]] ManagedEverythingStopResult
StopManagedEverything(
    const std::filesystem::path& dataDirectory,
    std::stop_token stopToken = {});

[[nodiscard]] bool
IsManagedEverythingServiceExecutable(
    const std::filesystem::path& dataDirectory,
    const std::filesystem::path& executable);

[[nodiscard]] ManagedEverythingServicePolicyResult
SetManagedEverythingServiceEnabled(
    const std::filesystem::path& dataDirectory,
    bool enabled);

[[nodiscard]] EverythingServiceRepairResult
ApplyManagedEverythingServiceEnabledPolicy(
    const std::filesystem::path& dataDirectory,
    bool enabled);

[[nodiscard]] EverythingServiceRepairResult
RepairManagedEverythingServicePath(
    const std::filesystem::path& dataDirectory);

} // namespace altrun::win
