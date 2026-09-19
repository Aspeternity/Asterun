#include "platform/WindowsContext.hpp"

#include <windows.h>

#include <cassert>
#include <iostream>

using namespace altrun::win;

namespace {

LRESULT CALLBACK DummyWindowProc(
    HWND hwnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam) {
    return DefWindowProcW(
        hwnd,
        message,
        wParam,
        lParam);
}

} // namespace

int main() {
    const HRESULT comResult =
        CoInitializeEx(
            nullptr,
            COINIT_APARTMENTTHREADED |
                COINIT_DISABLE_OLE1DDE);

    const bool uninitialize =
        SUCCEEDED(comResult);

    const auto empty =
        CaptureWindowsContext(
            nullptr);

    assert(!empty.HasExplorer());
    assert(
        !NavigateExplorerToFolder(
            empty,
            L"C:\\"));

    const wchar_t className[] =
        L"ALTRunNext.WindowsContextRuntimeTest";

    WNDCLASSW windowClass{};
    windowClass.lpfnWndProc =
        DummyWindowProc;
    windowClass.hInstance =
        GetModuleHandleW(nullptr);
    windowClass.lpszClassName =
        className;

    const ATOM atom =
        RegisterClassW(
            &windowClass);

    assert(atom != 0);

    HWND dummy =
        CreateWindowExW(
            0,
            className,
            L"Context Test",
            WS_OVERLAPPED,
            0,
            0,
            100,
            100,
            nullptr,
            nullptr,
            windowClass.hInstance,
            nullptr);

    assert(dummy != nullptr);

    const auto snapshot =
        CaptureWindowsContext(
            dummy);

    assert(!snapshot.HasExplorer());
    assert(
        !NavigateExplorerToFolder(
            snapshot,
            L"C:\\"));

    DestroyWindow(dummy);
    UnregisterClassW(
        className,
        windowClass.hInstance);

    if (uninitialize) {
        CoUninitialize();
    }

    std::cout
        << "Windows context runtime tests passed\n";
    return 0;
}
