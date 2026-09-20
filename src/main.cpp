#include "app/App.hpp"
#include "platform/WinUtil.hpp"

#include <objbase.h>
#include <shellapi.h>

#include <string>
#include <string_view>

namespace {

struct StartupArguments {
    bool repairManagedEverything{
        false};
    std::wstring updateHealthEvent;
    bool valid{true};
};

StartupArguments ParseArguments() {
    StartupArguments result;

    int argc = 0;
    LPWSTR* argv =
        CommandLineToArgvW(
            GetCommandLineW(),
            &argc);

    if (!argv) {
        result.valid = false;
        return result;
    }

    if (argc == 1) {
        LocalFree(argv);
        return result;
    }

    if (argc == 2 &&
        std::wstring_view(argv[1]) ==
            L"--repair-managed-everything-service") {
        result.repairManagedEverything =
            true;
        LocalFree(argv);
        return result;
    }

    if (argc == 3 &&
        std::wstring_view(argv[1]) ==
            L"--post-update-health-event" &&
        argv[2] &&
        *argv[2] != L'\0') {
        result.updateHealthEvent =
            argv[2];
        LocalFree(argv);
        return result;
    }

    result.valid = false;
    LocalFree(argv);
    return result;
}

} // namespace

int WINAPI wWinMain(
    HINSTANCE instance,
    HINSTANCE,
    PWSTR,
    int) {
    const auto arguments =
        ParseArguments();

    if (!arguments.valid) {
        return ERROR_INVALID_PARAMETER;
    }

    if (arguments
            .repairManagedEverything) {
        const auto result =
            altrun::win::
                RepairManagedEverythingServicePath(
                    altrun::win::
                        ExecutableDirectory() /
                    "data");

        if (result.success) {
            return 0;
        }

        return static_cast<int>(
            result.nativeError != 0
                ? result.nativeError
                : ERROR_GEN_FAILURE);
    }

    SetProcessDpiAwarenessContext(
        DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    const HRESULT comResult =
        CoInitializeEx(
            nullptr,
            COINIT_APARTMENTTHREADED |
                COINIT_DISABLE_OLE1DDE);

    altrun::App app(
        instance,
        arguments.updateHealthEvent);
    const int result = app.Run();

    if (SUCCEEDED(comResult)) {
        CoUninitialize();
    }

    return result;
}
