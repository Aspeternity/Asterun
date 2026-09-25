#pragma once

#include "../core/LaunchRole.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace altrun::win {

struct ShortcutTarget {
    std::wstring target;
    std::wstring arguments;
    std::wstring workingDirectory;
    std::wstring shellParsingName;
    LaunchTargetKind targetKind{
        LaunchTargetKind::Unknown};
};

[[nodiscard]] LaunchTargetKind
InspectLaunchTarget(
    std::wstring_view target);

[[nodiscard]] ExecutableMetadata
InspectExecutableMetadata(
    std::wstring_view target);

[[nodiscard]] std::optional<
    ShortcutTarget>
InspectShellLink(
    const std::filesystem::path& path);

[[nodiscard]] std::optional<
    LaunchSurfaceClass>
ClassifyShellActivationSurface(
    std::wstring_view target,
    std::wstring_view arguments,
    std::wstring_view shellParsingName);

} // namespace altrun::win
