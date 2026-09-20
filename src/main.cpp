#include "app/App.hpp"
#include "platform/WinUtil.hpp"

#include <objbase.h>

#include <string_view>

int WINAPI wWinMain(
    HINSTANCE instance,
    HINSTANCE,
    PWSTR commandLine,
    int) {
    const std::wstring_view command =
        commandLine
            ? std::wstring_view(
                  commandLine)
            : std::wstring_view{};

    if (altrun::win::Trim(command) ==
        L"--repair-managed-everything-service") {
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

    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    // The core currently uses Shell APIs only, but COM initialization here keeps
    // the process ready for .lnk metadata, UWP indexing and future plugins.
    const HRESULT comResult = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

    altrun::App app(instance);
    const int result = app.Run();

    if (SUCCEEDED(comResult)) {
        CoUninitialize();
    }
    return result;
}
