#pragma once

#include <string>
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

// The program name a system tool is commonly launched by ("msconfig" for
// System Configuration, "taskschd" for Task Scheduler): the stem of its
// .exe/.msc/.cpl target, or of the .msc/.cpl argument when the target is a
// host such as mmc.exe or control.exe. Empty when it adds nothing to the
// title or is not a plain short ASCII name.
[[nodiscard]] std::wstring
SystemToolAlias(
    std::wstring_view title,
    std::wstring_view resolvedTarget,
    std::wstring_view arguments);

} // namespace altrun
