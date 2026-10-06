#include "ui/UiComboBox.hpp"

#include <windows.h>

#include <array>
#include <cassert>

namespace {

constexpr wchar_t kParentClass[] =
    L"Asterun.UiComboBoxRuntimeTest";
int gWheelMessages = 0;
int gSelectionNotifications = 0;

LRESULT CALLBACK ParentProc(
    HWND hwnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam) {

    if (message == WM_MOUSEWHEEL ||
        message == WM_MOUSEHWHEEL) {
        ++gWheelMessages;
        return 0;
    }

    if (message == WM_COMMAND &&
        HIWORD(wParam) ==
            CBN_SELCHANGE) {
        ++gSelectionNotifications;
        return 0;
    }

    return DefWindowProcW(
        hwnd,
        message,
        wParam,
        lParam);
}

} // namespace

int main() {
    HINSTANCE instance =
        GetModuleHandleW(
            nullptr);

    WNDCLASSW wc{};
    wc.lpfnWndProc =
        ParentProc;
    wc.hInstance =
        instance;
    wc.lpszClassName =
        kParentClass;

    assert(
        RegisterClassW(
            &wc) != 0 ||
        GetLastError() ==
            ERROR_CLASS_ALREADY_EXISTS);

    HWND parent =
        CreateWindowExW(
            0,
            kParentClass,
            L"",
            WS_OVERLAPPED,
            0,
            0,
            400,
            240,
            nullptr,
            nullptr,
            instance,
            nullptr);
    assert(parent);

    HWND combo =
        altrun::ui::
            CreateNextComboBox(
                parent,
                instance,
                100,
                RGB(
                    255,
                    255,
                    255));
    assert(combo);

    constexpr std::array<const wchar_t*, 3>
        items{
            L"First",
            L"Second",
            L"Third",
        };

    for (const wchar_t* item :
         items) {
        assert(
            SendMessageW(
                combo,
                CB_ADDSTRING,
                0,
                reinterpret_cast<LPARAM>(
                    item)) != CB_ERR);
    }

    altrun::ui::
        MoveNextComboBox(
            combo,
            10,
            10,
            180,
            96,
            FALSE);

    assert(
        SendMessageW(
            combo,
            CB_SETCURSEL,
            1,
            0) == 1);
    assert(
        SendMessageW(
            combo,
            CB_GETCURSEL,
            0,
            0) == 1);

    SetFocus(
        combo);

    gWheelMessages = 0;
    gSelectionNotifications = 0;

    SendMessageW(
        combo,
        WM_MOUSEWHEEL,
        MAKEWPARAM(
            0,
            WHEEL_DELTA),
        0);

    assert(
        gWheelMessages == 1);
    assert(
        gSelectionNotifications == 0);
    assert(
        SendMessageW(
            combo,
            CB_GETCURSEL,
            0,
            0) == 1);

    SendMessageW(
        combo,
        WM_MOUSEWHEEL,
        MAKEWPARAM(
            0,
            static_cast<WORD>(
                -WHEEL_DELTA)),
        0);

    assert(
        gWheelMessages == 2);
    assert(
        gSelectionNotifications == 0);
    assert(
        SendMessageW(
            combo,
            CB_GETCURSEL,
            0,
            0) == 1);

    SendMessageW(
        combo,
        WM_MOUSEHWHEEL,
        MAKEWPARAM(
            0,
            WHEEL_DELTA),
        0);

    assert(
        gWheelMessages == 3);
    assert(
        gSelectionNotifications == 0);
    assert(
        SendMessageW(
            combo,
            CB_GETCURSEL,
            0,
            0) == 1);

    HDC screen =
        GetDC(
            parent);
    assert(screen);

    HDC paintDc =
        CreateCompatibleDC(
            screen);
    assert(paintDc);

    HBITMAP paintBitmap =
        CreateCompatibleBitmap(
            screen,
            32,
            32);
    assert(paintBitmap);

    HGDIOBJ oldBitmap =
        SelectObject(
            paintDc,
            paintBitmap);

    const COLORREF sentinel =
        RGB(
            17,
            34,
            51);
    RECT paintRect{
        0,
        0,
        32,
        32,
    };
    HBRUSH sentinelBrush =
        CreateSolidBrush(
            sentinel);
    FillRect(
        paintDc,
        &paintRect,
        sentinelBrush);
    DeleteObject(
        sentinelBrush);

    DRAWITEMSTRUCT closedField{};
    closedField.CtlType =
        ODT_COMBOBOX;
    closedField.CtlID =
        100;
    closedField.itemID =
        1;
    closedField.itemState =
        ODS_COMBOBOXEDIT |
        ODS_SELECTED |
        ODS_FOCUS;
    closedField.hwndItem =
        combo;
    closedField.hDC =
        paintDc;
    closedField.rcItem =
        paintRect;

    altrun::ui::
        DrawNextComboBoxItem(
            closedField,
            96);

    assert(
        GetPixel(
            paintDc,
            4,
            4) ==
        sentinel);
    assert(
        GetPixel(
            paintDc,
            20,
            20) ==
        sentinel);

    SelectObject(
        paintDc,
        oldBitmap);
    DeleteObject(
        paintBitmap);
    DeleteDC(
        paintDc);
    ReleaseDC(
        parent,
        screen);

    DestroyWindow(
        parent);
    UnregisterClassW(
        kParentClass,
        instance);

    return 0;
}
