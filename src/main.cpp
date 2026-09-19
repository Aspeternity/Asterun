#include "app/App.hpp"

#include <objbase.h>

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
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
