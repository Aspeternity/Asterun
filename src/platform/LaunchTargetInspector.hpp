#pragma once

#include "../core/LaunchCandidate.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace altrun::win {

struct ShortcutTarget {
    std::wstring target;
    std::wstring arguments;
    std::wstring workingDirectory;
    LaunchTargetKind targetKind{
        LaunchTargetKind::Unknown};
};

[[nodiscard]] LaunchTargetKind
InspectLaunchTarget(
    std::wstring_view target);

[[nodiscard]] std::optional<
    ShortcutTarget>
InspectShellLink(
    const std::filesystem::path& path);

} // namespace altrun::win
