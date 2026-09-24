#include "core/LaunchSurface.hpp"

#include <cassert>
#include <iostream>

using namespace altrun;

int main() {
    assert(
        ClassifyApplicationSurface(
            L"IDMHelp",
            L"C:\\Tools\\IDMHelp.exe") ==
        LaunchSurfaceClass::Auxiliary);

    assert(
        ClassifyApplicationSurface(
            L"GPUViewHelp",
            L"C:\\Windows Kits\\GPUViewHelp.exe") ==
        LaunchSurfaceClass::Auxiliary);

    assert(
        ClassifyApplicationSurface(
            L"GrabberHelp",
            L"C:\\Tools\\GrabberHelp.exe") ==
        LaunchSurfaceClass::Auxiliary);

    assert(
        ClassifyApplicationSurface(
            L"GetHelp",
            L"shell:AppsFolder\\Package!GetHelp") ==
        LaunchSurfaceClass::Auxiliary);

    assert(
        ClassifyApplicationSurface(
            L"PlatformExperienceShell",
            L"Package!PlatformExperienceShell") ==
        LaunchSurfaceClass::Auxiliary);

    assert(
        ClassifyApplicationSurface(
            L"PAD Browser",
            L"Package!PAD.BrowserNativeMessaging") ==
        LaunchSurfaceClass::Auxiliary);

    assert(
        ClassifyApplicationSurface(
            L"Visual Studio Code",
            L"C:\\Apps\\Code.exe") ==
        LaunchSurfaceClass::
            PrimaryApplication);

    assert(
        ClassifyApplicationSurface(
            L"PowerShell",
            L"C:\\Windows\\System32\\WindowsPowerShell\\powershell.exe") ==
        LaunchSurfaceClass::
            PrimaryApplication);

    assert(
        ClassifyApplicationSurface(
            L"Developer Command Prompt",
            L"C:\\Developer Tools\\VsDevCmd.bat") ==
        LaunchSurfaceClass::DeveloperTool);

    assert(
        ClassifyApplicationSurface(
            L"Product Updater",
            L"C:\\Apps\\Updater.exe") ==
        LaunchSurfaceClass::Maintenance);

    std::cout
        << "Launch surface classifier tests passed\n";
    return 0;
}
