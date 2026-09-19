#include "platform/WindowsContext.hpp"

#include <objbase.h>
#include <windows.h>

#include <algorithm>
#include <cassert>
#include <iostream>
#include <string>
#include <vector>

using namespace altrun::win;

namespace {

HWND gTotalCommanderPath{};
std::vector<unsigned char>
    gTotalCommanderCopyData;

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

LRESULT CALLBACK
TotalCommanderWindowProc(
    HWND hwnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam) {
    if (message == WM_USER + 50) {
        if (wParam == 1000) {
            return 1;
        }

        if (wParam == 9) {
            return reinterpret_cast<LRESULT>(
                gTotalCommanderPath);
        }

        return 0;
    }

    if (message == WM_COPYDATA) {
        const auto* copy =
            reinterpret_cast<
                const COPYDATASTRUCT*>(
                lParam);

        if (!copy ||
            copy->dwData !=
                static_cast<ULONG_PTR>('C') +
                    256u *
                        static_cast<
                            ULONG_PTR>('D') ||
            !copy->lpData ||
            copy->cbData == 0) {
            return FALSE;
        }

        const auto* begin =
            static_cast<
                const unsigned char*>(
                copy->lpData);

        gTotalCommanderCopyData.assign(
            begin,
            begin + copy->cbData);

        return TRUE;
    }

    return DefWindowProcW(
        hwnd,
        message,
        wParam,
        lParam);
}

[[nodiscard]] std::wstring
DecodeTotalCommanderPath(
    const std::vector<unsigned char>&
        payload) {
    assert(payload.size() >= 7);
    assert(payload[0] == 0xEF);
    assert(payload[1] == 0xBB);
    assert(payload[2] == 0xBF);

    auto carriage =
        std::find(
            payload.begin() + 3,
            payload.end(),
            static_cast<unsigned char>(
                '\r'));

    assert(carriage !=
        payload.end());

    const std::string utf8(
        reinterpret_cast<const char*>(
            &payload[3]),
        static_cast<std::size_t>(
            carriage -
            (payload.begin() + 3)));

    const int required =
        MultiByteToWideChar(
            CP_UTF8,
            MB_ERR_INVALID_CHARS,
            utf8.data(),
            static_cast<int>(
                utf8.size()),
            nullptr,
            0);

    assert(required > 0);

    std::wstring result(
        static_cast<std::size_t>(
            required),
        L'\0');

    assert(
        MultiByteToWideChar(
            CP_UTF8,
            MB_ERR_INVALID_CHARS,
            utf8.data(),
            static_cast<int>(
                utf8.size()),
            result.data(),
            required) ==
        required);

    const auto suffix =
        static_cast<std::size_t>(
            carriage -
            payload.begin());

    assert(
        suffix + 3 <
        payload.size());
    assert(payload[suffix + 1] == 0);
    assert(payload[suffix + 2] == 'S');
    assert(payload[suffix + 3] == 0);

    return result;
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
    assert(!empty.HasFileDialog());
    assert(!empty.HasTotalCommander());
    assert(
        empty.CurrentFilesystemFolder()
            .empty());
    assert(
        !NavigateExplorerToFolder(
            empty,
            L"C:\\"));
    assert(
        !NavigateFileDialogToFolder(
            empty,
            L"C:\\"));
    assert(
        !NavigateTotalCommanderToFolder(
            empty,
            L"C:\\"));

    const HINSTANCE instance =
        GetModuleHandleW(nullptr);

    const wchar_t dummyClass[] =
        L"ALTRunNext.WindowsContextRuntimeTest";

    WNDCLASSW windowClass{};
    windowClass.lpfnWndProc =
        DummyWindowProc;
    windowClass.hInstance =
        instance;
    windowClass.lpszClassName =
        dummyClass;

    assert(
        RegisterClassW(
            &windowClass) != 0);

    HWND dummy =
        CreateWindowExW(
            0,
            dummyClass,
            L"Context Test",
            WS_OVERLAPPED,
            0,
            0,
            100,
            100,
            nullptr,
            nullptr,
            instance,
            nullptr);

    assert(dummy != nullptr);

    const auto snapshot =
        CaptureWindowsContext(
            dummy);

    assert(!snapshot.HasExplorer());
    assert(!snapshot.HasFileDialog());
    assert(!snapshot.HasTotalCommander());

    DestroyWindow(dummy);
    UnregisterClassW(
        dummyClass,
        instance);

    // Total Commander 9+ exposes active-panel and path-control queries via
    // WM_USER+50. This fake window exercises the same cross-window protocol
    // without requiring Total Commander to be installed on the CI runner.
    const wchar_t tcClass[] =
        L"TTOTAL_CMD";

    WNDCLASSW tcWindowClass{};
    tcWindowClass.lpfnWndProc =
        TotalCommanderWindowProc;
    tcWindowClass.hInstance =
        instance;
    tcWindowClass.lpszClassName =
        tcClass;

    assert(
        RegisterClassW(
            &tcWindowClass) != 0);

    HWND tcWindow =
        CreateWindowExW(
            0,
            tcClass,
            L"Total Commander Fake",
            WS_OVERLAPPED,
            0,
            0,
            400,
            300,
            nullptr,
            nullptr,
            instance,
            nullptr);

    assert(tcWindow != nullptr);

    gTotalCommanderPath =
        CreateWindowExW(
            0,
            L"STATIC",
            L"D:\\Workspace\\*.*",
            WS_CHILD,
            0,
            0,
            200,
            20,
            tcWindow,
            nullptr,
            instance,
            nullptr);

    assert(
        gTotalCommanderPath !=
        nullptr);

    const auto tcContext =
        CaptureWindowsContext(
            tcWindow);

    assert(
        tcContext
            .HasTotalCommander());
    assert(
        tcContext
            .totalCommanderActivePanel ==
        1);
    assert(
        tcContext
            .CurrentFilesystemFolder() ==
        L"D:\\Workspace");

    const std::wstring target =
        L"D:\\\u76ee\u6807 Folder";

    gTotalCommanderCopyData.clear();

    assert(
        NavigateTotalCommanderToFolder(
            tcContext,
            target));

    assert(
        DecodeTotalCommanderPath(
            gTotalCommanderCopyData) ==
        target);

    DestroyWindow(tcWindow);
    gTotalCommanderPath = nullptr;
    UnregisterClassW(
        tcClass,
        instance);

    if (uninitialize) {
        CoUninitialize();
    }

    std::cout
        << "Windows context runtime tests passed\n";
    return 0;
}
