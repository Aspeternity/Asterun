#include "LaunchSurface.hpp"

#include <algorithm>
#include <array>
#include <cwctype>
#include <string>

namespace altrun {
namespace {

[[nodiscard]] std::wstring Lower(
    std::wstring_view value) {
    std::wstring result(value);

    std::transform(
        result.begin(),
        result.end(),
        result.begin(),
        [](wchar_t ch) {
            return static_cast<wchar_t>(
                std::towlower(ch));
        });

    return result;
}

[[nodiscard]] std::wstring Compact(
    std::wstring_view value) {
    std::wstring result;
    result.reserve(value.size());

    for (const wchar_t ch : value) {
        if (std::iswalnum(ch) ||
            ch >= 0x4E00) {
            result.push_back(
                static_cast<wchar_t>(
                    std::towlower(ch)));
        }
    }

    return result;
}

[[nodiscard]] bool EndsWithAny(
    std::wstring_view compact,
    std::initializer_list<
        std::wstring_view> suffixes) {

    return std::any_of(
        suffixes.begin(),
        suffixes.end(),
        [&](std::wstring_view suffix) {
            return compact.ends_with(
                suffix);
        });
}

[[nodiscard]] bool ContainsAny(
    std::wstring_view value,
    std::initializer_list<
        std::wstring_view> needles) {

    return std::any_of(
        needles.begin(),
        needles.end(),
        [&](std::wstring_view needle) {
            return value.find(needle) !=
                std::wstring_view::npos;
        });
}

} // namespace

const char* LaunchSurfaceName(
    LaunchSurfaceClass surface) noexcept {

    switch (surface) {
    case LaunchSurfaceClass::UserCommand:
        return "user-command";
    case LaunchSurfaceClass::
        PrimaryApplication:
        return "primary-app";
    case LaunchSurfaceClass::SystemUtility:
        return "system-utility";
    case LaunchSurfaceClass::DeveloperTool:
        return "developer-tool";
    case LaunchSurfaceClass::CommandLineTool:
        return "command-line";
    case LaunchSurfaceClass::Auxiliary:
        return "auxiliary";
    case LaunchSurfaceClass::Maintenance:
        return "maintenance";
    case LaunchSurfaceClass::FilesystemItem:
        return "filesystem";
    case LaunchSurfaceClass::Action:
        return "action";
    }

    return "primary-app";
}

LaunchSurfaceClass ParseLaunchSurface(
    std::string_view value,
    LaunchSurfaceClass fallback) noexcept {

    if (value == "user-command") {
        return LaunchSurfaceClass::
            UserCommand;
    }
    if (value == "primary-app") {
        return LaunchSurfaceClass::
            PrimaryApplication;
    }
    if (value == "system-utility") {
        return LaunchSurfaceClass::
            SystemUtility;
    }
    if (value == "developer-tool") {
        return LaunchSurfaceClass::
            DeveloperTool;
    }
    if (value == "command-line") {
        return LaunchSurfaceClass::
            CommandLineTool;
    }
    if (value == "auxiliary") {
        return LaunchSurfaceClass::
            Auxiliary;
    }
    if (value == "maintenance") {
        return LaunchSurfaceClass::
            Maintenance;
    }
    if (value == "filesystem") {
        return LaunchSurfaceClass::
            FilesystemItem;
    }
    if (value == "action") {
        return LaunchSurfaceClass::Action;
    }

    return fallback;
}

LaunchSurfaceClass
ClassifyApplicationSurface(
    std::wstring_view title,
    std::wstring_view target) {

    const std::wstring lowerTitle =
        Lower(title);
    const std::wstring lowerTarget =
        Lower(target);

    const std::wstring compactTitle =
        Compact(title);
    const std::wstring compactTarget =
        Compact(target);

    if (ContainsAny(
            lowerTitle,
            {
                L"uninstall",
                L"uninstaller",
                L"repair",
                L"modify",
                L"updater",
                L"卸载",
                L"修复",
            }) ||
        EndsWithAny(
            compactTitle,
            {
                L"uninstall",
                L"uninstaller",
                L"repair",
                L"updater",
                L"update",
            })) {
        return LaunchSurfaceClass::
            Maintenance;
    }

    if (ContainsAny(
            lowerTitle,
            {
                L"documentation",
                L"release notes",
                L"readme",
                L"manual",
                L"support",
                L"帮助",
                L"支持",
                L"文档",
            }) ||
        EndsWithAny(
            compactTitle,
            {
                L"help",
                L"helper",
                L"service",
                L"host",
                L"broker",
                L"handler",
                L"crashpad",
                L"crashhandler",
                L"nativemessaging",
                L"nativemessage",
                L"experienceshell",
                L"backgroundtask",
            }) ||
        ContainsAny(
            compactTarget,
            {
                L"nativemessaging",
                L"crashpad",
                L"crashhandler",
                L"experiencehost",
                L"experienceshell",
                L"backgroundtask",
                L"runtimebroker",
            })) {
        return LaunchSurfaceClass::
            Auxiliary;
    }

    if (ContainsAny(
            lowerTitle,
            {
                L"developer",
                L"debugger",
                L"debuggable",
                L" sdk",
                L"sdk ",
            }) ||
        ContainsAny(
            lowerTarget,
            {
                L"\\windows kits\\",
                L"\\developer tools\\",
                L"\\visual studio tools\\",
                L"\\sdk\\",
            })) {
        return LaunchSurfaceClass::
            DeveloperTool;
    }

    return LaunchSurfaceClass::
        PrimaryApplication;
}

} // namespace altrun
