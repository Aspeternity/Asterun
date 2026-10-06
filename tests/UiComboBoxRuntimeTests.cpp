#include "ui/UiComboBox.hpp"

#include <windows.h>

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

    for (const wchar_t* item : {
             L"First",
             L"Second",
             L"Third"}) {
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

    DestroyWindow(
        parent);
    UnregisterClassW(
        kParentClass,
        instance);

    return 0;
}
