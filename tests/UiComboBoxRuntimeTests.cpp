#include "ui/UiComboBox.hpp"
#include "ui/UiTheme.hpp"

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

    RECT comboRect{};
    GetClientRect(
        combo,
        &comboRect);
    const int comboWidth =
        comboRect.right -
        comboRect.left;
    const int comboHeight =
        comboRect.bottom -
        comboRect.top;
    assert(
        comboWidth > 40 &&
        comboHeight > 10);

    HDC paintDc =
        CreateCompatibleDC(
            screen);
    assert(paintDc);

    HBITMAP paintBitmap =
        CreateCompatibleBitmap(
            screen,
            comboWidth,
            comboHeight);
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
    RECT paintRect =
        comboRect;
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

    COMBOBOXINFO comboInfo{};
    comboInfo.cbSize =
        sizeof(comboInfo);
    assert(
        GetComboBoxInfo(
            combo,
            &comboInfo));

    const int selectionProbeX =
        std::clamp(
            comboInfo.rcItem.left + 2,
            0,
            comboWidth - 1);
    const int selectionProbeY =
        std::clamp(
            comboInfo.rcItem.top + 2,
            0,
            comboHeight - 1);
    const int buttonProbeX =
        std::clamp(
            (comboInfo.rcButton.left +
             comboInfo.rcButton.right) / 2,
            0,
            comboWidth - 1);
    const int buttonProbeY =
        std::clamp(
            (comboInfo.rcButton.top +
             comboInfo.rcButton.bottom) / 2,
            0,
            comboHeight - 1);

    // The closed selection field must be painted synchronously during focus
    // transitions, while the native button/chevron area remains untouched.
    assert(
        GetPixel(
            paintDc,
            selectionProbeX,
            selectionProbeY) ==
        RGB(
            248,
            252,
            255));
    assert(
        GetPixel(
            paintDc,
            buttonProbeX,
            buttonProbeY) ==
        sentinel);

    SelectObject(
        paintDc,
        oldBitmap);
    DeleteObject(
        paintBitmap);
    DeleteDC(
        paintDc);

    assert(
        SendMessageW(
            combo,
            CB_SHOWDROPDOWN,
            TRUE,
            0) != CB_ERR);

    HDC expandedDc =
        CreateCompatibleDC(
            screen);
    assert(expandedDc);

    HBITMAP expandedBitmap =
        CreateCompatibleBitmap(
            screen,
            comboWidth,
            comboHeight);
    assert(expandedBitmap);

    HGDIOBJ oldExpandedBitmap =
        SelectObject(
            expandedDc,
            expandedBitmap);

    SendMessageW(
        combo,
        WM_PRINTCLIENT,
        reinterpret_cast<WPARAM>(
            expandedDc),
        PRF_CLIENT);

    constexpr COLORREF
        kClosedChevronColor =
            RGB(
                78,
                86,
                94);
    const COLORREF expandedChevronColor =
        altrun::ui::
            kApplicationPalette.accent;

    int closedChevronPixels = 0;
    int expandedChevronPixels = 0;
    const int arrowLeft =
        comboWidth > 36
            ? comboWidth - 36
            : 0;

    for (int y = 0;
         y < comboHeight;
         ++y) {
        for (int x = arrowLeft;
             x < comboWidth;
             ++x) {
            const COLORREF pixel =
                GetPixel(
                    expandedDc,
                    x,
                    y);

            if (pixel ==
                kClosedChevronColor) {
                ++closedChevronPixels;
            }
            if (pixel ==
                expandedChevronColor) {
                ++expandedChevronPixels;
            }
        }
    }

    assert(
        closedChevronPixels == 0);
    assert(
        expandedChevronPixels > 0);

    SendMessageW(
        combo,
        CB_SHOWDROPDOWN,
        FALSE,
        0);

    SelectObject(
        expandedDc,
        oldExpandedBitmap);
    DeleteObject(
        expandedBitmap);
    DeleteDC(
        expandedDc);
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
