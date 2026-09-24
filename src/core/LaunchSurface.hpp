#pragma once

#include <string_view>

namespace altrun {

enum class LaunchSurfaceClass {
    UserCommand,
    PrimaryApplication,
    SystemUtility,
    DeveloperTool,
    CommandLineTool,
    Auxiliary,
    Maintenance,
    FilesystemItem,
    Action,
};

[[nodiscard]] const char*
LaunchSurfaceName(
    LaunchSurfaceClass surface) noexcept;

[[nodiscard]] LaunchSurfaceClass
ParseLaunchSurface(
    std::string_view value,
    LaunchSurfaceClass fallback =
        LaunchSurfaceClass::
            PrimaryApplication) noexcept;

[[nodiscard]] LaunchSurfaceClass
ClassifyApplicationSurface(
    std::wstring_view title,
    std::wstring_view target);

} // namespace altrun
