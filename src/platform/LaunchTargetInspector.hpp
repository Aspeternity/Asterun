#pragma once

#include "../core/LaunchCandidate.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace altrun::win {

enum class ExecutableInspectionStage {
    NotRun,
    OpenFailed,
    DosHeaderReadFailed,
    InvalidDosHeader,
    InvalidPeOffset,
    SeekFailed,
    NtHeaderReadFailed,
    InvalidPeSignature,
    UnsupportedOptionalMagic,
    OptionalHeaderReadFailed,
    GuiExecutable,
    ConsoleExecutable,
    UnsupportedSubsystem,
};

[[nodiscard]] const char*
ExecutableInspectionStageName(
    ExecutableInspectionStage stage) noexcept;

struct ExecutableInspection {
    LaunchTargetKind legacyKind{
        LaunchTargetKind::Unknown};
    LaunchTargetKind currentKind{
        LaunchTargetKind::Unknown};
    ExecutableInspectionStage stage{
        ExecutableInspectionStage::
            NotRun};
    std::uint16_t optionalMagic{0};
    std::uint16_t subsystem{0};
    bool fallbackUsed{false};
    bool fileExists{false};
    bool getBinaryTypeSucceeded{false};
    std::uint32_t binaryType{0};
};

struct LaunchTargetInspection {
    std::wstring target;
    LaunchTargetKind inferredKind{
        LaunchTargetKind::Unknown};
    ExecutableInspection executable;
    LaunchTargetKind finalKind{
        LaunchTargetKind::Unknown};
};

enum class ShellLinkInspectionStage {
    ComUnavailable,
    CreateInstanceFailed,
    PersistInterfaceFailed,
    LoadFailed,
    TargetResolutionFailed,
    Resolved,
};

[[nodiscard]] const char*
ShellLinkInspectionStageName(
    ShellLinkInspectionStage stage) noexcept;

struct ShortcutTarget {
    std::wstring target;
    std::wstring arguments;
    std::wstring workingDirectory;
    LaunchTargetKind targetKind{
        LaunchTargetKind::Unknown};
};

struct ShellLinkInspection {
    ShellLinkInspectionStage stage{
        ShellLinkInspectionStage::
            TargetResolutionFailed};
    long nativeResult{0};
    std::optional<ShortcutTarget> shortcut;
    LaunchTargetInspection targetInspection;
};

[[nodiscard]] LaunchTargetInspection
InspectLaunchTargetDetailed(
    std::wstring_view target);

[[nodiscard]] LaunchTargetKind
InspectLaunchTarget(
    std::wstring_view target);

[[nodiscard]] ShellLinkInspection
InspectShellLinkDetailed(
    const std::filesystem::path& path);

[[nodiscard]] std::optional<
    ShortcutTarget>
InspectShellLink(
    const std::filesystem::path& path);

} // namespace altrun::win
