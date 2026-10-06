#include "UiComboPopup.hpp"

#include "UiComboBox.hpp"
#include "UiMetrics.hpp"
#include "UiTheme.hpp"

#include <commctrl.h>

#include <algorithm>
#include <string>
#include <vector>

namespace altrun::ui {
namespace {

constexpr wchar_t kPopupClassName[] =
    L"Asterun.NextComboPopup";
constexpr wchar_t kPopupProperty[] =
    L"Asterun.NextCombo.Popup";

struct PopupState {
    HWND hwnd{};
    HWND combo{};
    UINT dpi{96};
    HFONT font{};
    std::vector<std::wstring> items;
    int selectedIndex{-1};
    int activeIndex{-1};
    int hoverIndex{-1};
    int topIndex{0};
    int visibleRows{0};
    bool trackingMouse{false};
};

[[nodiscard]] int S(
    int logical,
    UINT dpi) noexcept {

    return Scale(
        logical,
        dpi);
}

[[nodiscard]] HWND PopupForCombo(
    HWND combo) noexcept {

    return combo
        ? reinterpret_cast<HWND>(
              GetPropW(
                  combo,
                  kPopupProperty))
        : nullptr;
}

[[nodiscard]] PopupState* StateForPopup(
    HWND popup) noexcept {

    return popup
        ? reinterpret_cast<PopupState*>(
              GetWindowLongPtrW(
                  popup,
                  GWLP_USERDATA))
        : nullptr;
}

void NotifyCombo(
    HWND combo,
    UINT notification) {

    if (!combo) {
        return;
    }

    HWND parent =
        GetParent(
            combo);

    if (!parent) {
        return;
    }

    SendMessageW(
        parent,
        WM_COMMAND,
        MAKEWPARAM(
            static_cast<UINT>(
                GetDlgCtrlID(
                    combo)),
            notification),
        reinterpret_cast<LPARAM>(
            combo));
}

void ApplyRoundedRegion(
    HWND popup,
    UINT dpi) {

    if (!popup) {
        return;
    }

    RECT rect{};
    GetClientRect(
        popup,
        &rect);

    const int radius =
        std::max(
            2,
            S(
                kNextComboPopupRadiusLogical,
                dpi));

    HRGN region =
        CreateRoundRectRgn(
            0,
            0,
            rect.right + 1,
            rect.bottom + 1,
            radius * 2,
            radius * 2);

    if (!region) {
        return;
    }

    if (SetWindowRgn(
            popup,
            region,
            FALSE) == 0) {
        DeleteObject(
            region);
    }
}

[[nodiscard]] int MaximumTopIndex(
    const PopupState& state) noexcept {

    return std::max(
        0,
        static_cast<int>(
            state.items.size()) -
            state.visibleRows);
}

void ClampTopIndex(
    PopupState& state) {

    state.topIndex =
        std::clamp(
            state.topIndex,
            0,
            MaximumTopIndex(
                state));
}

void EnsureActiveVisible(
    PopupState& state) {

    if (state.activeIndex < 0 ||
        state.visibleRows <= 0) {
        return;
    }

    if (state.activeIndex <
        state.topIndex) {
        state.topIndex =
            state.activeIndex;
    } else if (state.activeIndex >=
               state.topIndex +
                   state.visibleRows) {
        state.topIndex =
            state.activeIndex -
            state.visibleRows +
            1;
    }

    ClampTopIndex(
        state);
}

[[nodiscard]] RECT RowRect(
    const PopupState& state,
    int visibleRow) {

    RECT client{};
    GetClientRect(
        state.hwnd,
        &client);

    const int padding =
        S(
            kNextComboPopupPaddingLogical,
            state.dpi);
    const int rowHeight =
        S(
            kNextComboPopupRowHeightLogical,
            state.dpi);
    const int scrollbarReserve =
        static_cast<int>(
            state.items.size()) >
                state.visibleRows
            ? S(
                  kNextComboPopupScrollbarWidthLogical +
                      kNextComboPopupScrollbarInsetLogical * 2,
                  state.dpi)
            : 0;

    return {
        padding,
        padding +
            visibleRow *
                rowHeight,
        std::max(
            padding,
            client.right -
                padding -
                scrollbarReserve),
        padding +
            (visibleRow + 1) *
                rowHeight,
    };
}

[[nodiscard]] int RowFromPoint(
    const PopupState& state,
    POINT point) {

    for (int row = 0;
         row < state.visibleRows;
         ++row) {
        const int itemIndex =
            state.topIndex +
            row;

        if (itemIndex >=
            static_cast<int>(
                state.items.size())) {
            break;
        }

        RECT rect =
            RowRect(
                state,
                row);

        if (PtInRect(
                &rect,
                point)) {
            return itemIndex;
        }
    }

    return -1;
}

void DrawRoundedFill(
    HDC dc,
    const RECT& rect,
    COLORREF color,
    int radius) {

    HBRUSH brush =
        CreateSolidBrush(
            color);
    HGDIOBJ oldBrush =
        SelectObject(
            dc,
            brush);
    HGDIOBJ oldPen =
        SelectObject(
            dc,
            GetStockObject(
                NULL_PEN));

    RoundRect(
        dc,
        rect.left,
        rect.top,
        rect.right,
        rect.bottom,
        radius,
        radius);

    SelectObject(
        dc,
        oldPen);
    SelectObject(
        dc,
        oldBrush);
    DeleteObject(
        brush);
}

void DrawPopupDirect(
    PopupState& state,
    HDC dc) {

    RECT client{};
    GetClientRect(
        state.hwnd,
        &client);

    HBRUSH background =
        CreateSolidBrush(
            kApplicationPalette
                .controlBackground);
    FillRect(
        dc,
        &client,
        background);
    DeleteObject(
        background);

    HPEN frame =
        CreatePen(
            PS_SOLID,
            1,
            kApplicationPalette.frame);
    HGDIOBJ oldPen =
        SelectObject(
            dc,
            frame);
    HGDIOBJ oldBrush =
        SelectObject(
            dc,
            GetStockObject(
                HOLLOW_BRUSH));

    const int popupRadius =
        S(
            kNextComboPopupRadiusLogical,
            state.dpi);

    RoundRect(
        dc,
        client.left,
        client.top,
        client.right - 1,
        client.bottom - 1,
        popupRadius,
        popupRadius);

    SelectObject(
        dc,
        oldBrush);
    SelectObject(
        dc,
        oldPen);
    DeleteObject(
        frame);

    HGDIOBJ oldFont =
        SelectObject(
            dc,
            state.font
                ? state.font
                : GetStockObject(
                      DEFAULT_GUI_FONT));

    SetBkMode(
        dc,
        TRANSPARENT);

    const int rowRadius =
        S(
            kNextComboPopupRowRadiusLogical,
            state.dpi);
    const int textInset =
        S(
            kNextComboPopupTextInsetLogical,
            state.dpi);

    for (int row = 0;
         row < state.visibleRows;
         ++row) {
        const int itemIndex =
            state.topIndex +
            row;

        if (itemIndex < 0 ||
            itemIndex >=
                static_cast<int>(
                    state.items.size())) {
            break;
        }

        RECT rowRect =
            RowRect(
                state,
                row);
        RECT visual =
            rowRect;

        InflateRect(
            &visual,
            -S(2, state.dpi),
            -S(2, state.dpi));

        const bool selected =
            itemIndex ==
            state.selectedIndex;
        const bool hovered =
            itemIndex ==
            state.hoverIndex ||
            itemIndex ==
            state.activeIndex;

        if (selected) {
            DrawRoundedFill(
                dc,
                visual,
                kApplicationPalette
                    .selectionBackground,
                rowRadius);
        } else if (hovered) {
            DrawRoundedFill(
                dc,
                visual,
                kApplicationPalette
                    .pressedBackground,
                rowRadius);
        }

        if (selected) {
            const int accentWidth =
                S(
                    kNextComboPopupAccentWidthLogical,
                    state.dpi);
            const int accentHeight =
                std::min(
                    visual.bottom -
                        visual.top -
                        S(8, state.dpi),
                    S(
                        kNextComboPopupAccentHeightLogical,
                        state.dpi));
            const int centerY =
                visual.top +
                (visual.bottom -
                 visual.top) / 2;

            RECT accent{
                visual.left +
                    S(4, state.dpi),
                centerY -
                    accentHeight / 2,
                visual.left +
                    S(4, state.dpi) +
                    accentWidth,
                centerY +
                    (accentHeight -
                     accentHeight / 2),
            };

            DrawRoundedFill(
                dc,
                accent,
                kApplicationPalette.accent,
                std::max(
                    1,
                    accentWidth));
        }

        RECT textRect =
            rowRect;
        textRect.left +=
            textInset;
        textRect.right -=
            S(8, state.dpi);

        SetTextColor(
            dc,
            kApplicationPalette.text);

        DrawTextW(
            dc,
            state.items[
                static_cast<std::size_t>(
                    itemIndex)]
                .c_str(),
            -1,
            &textRect,
            DT_LEFT |
                DT_VCENTER |
                DT_SINGLELINE |
                DT_END_ELLIPSIS |
                DT_NOPREFIX);
    }

    if (static_cast<int>(
            state.items.size()) >
        state.visibleRows) {
        const int width =
            S(
                kNextComboPopupScrollbarWidthLogical,
                state.dpi);
        const int inset =
            S(
                kNextComboPopupScrollbarInsetLogical,
                state.dpi);
        const int trackTop =
            inset +
            S(
                kNextComboPopupPaddingLogical,
                state.dpi);
        const int trackBottom =
            client.bottom -
            inset -
            S(
                kNextComboPopupPaddingLogical,
                state.dpi);
        const int trackHeight =
            std::max(
                1,
                trackBottom -
                    trackTop);
        const int count =
            static_cast<int>(
                state.items.size());
        const int thumbHeight =
            std::max(
                S(24, state.dpi),
                trackHeight *
                    state.visibleRows /
                    std::max(
                        1,
                        count));
        const int travel =
            std::max(
                0,
                trackHeight -
                    thumbHeight);
        const int maxTop =
            std::max(
                1,
                MaximumTopIndex(
                    state));
        const int thumbTop =
            trackTop +
            travel *
                state.topIndex /
                maxTop;

        RECT thumb{
            client.right -
                inset -
                width,
            thumbTop,
            client.right -
                inset,
            thumbTop +
                thumbHeight,
        };

        DrawRoundedFill(
            dc,
            thumb,
            RGB(
                154,
                162,
                171),
            std::max(
                1,
                width));
    }

    SelectObject(
        dc,
        oldFont);
}

void DrawPopup(
    PopupState& state,
    HDC dc) {

    RECT rect{};
    GetClientRect(
        state.hwnd,
        &rect);

    const int width =
        rect.right -
        rect.left;
    const int height =
        rect.bottom -
        rect.top;

    if (width <= 0 ||
        height <= 0) {
        return;
    }

    HDC buffer =
        CreateCompatibleDC(
            dc);
    HBITMAP bitmap =
        buffer
            ? CreateCompatibleBitmap(
                  dc,
                  width,
                  height)
            : nullptr;

    if (!buffer ||
        !bitmap) {
        if (bitmap) {
            DeleteObject(
                bitmap);
        }
        if (buffer) {
            DeleteDC(
                buffer);
        }

        DrawPopupDirect(
            state,
            dc);
        return;
    }

    HGDIOBJ oldBitmap =
        SelectObject(
            buffer,
            bitmap);

    DrawPopupDirect(
        state,
        buffer);

    BitBlt(
        dc,
        0,
        0,
        width,
        height,
        buffer,
        0,
        0,
        SRCCOPY);

    SelectObject(
        buffer,
        oldBitmap);
    DeleteObject(
        bitmap);
    DeleteDC(
        buffer);
}

void ClosePopup(
    PopupState& state,
    bool cancel,
    int commitIndex) {

    HWND combo =
        state.combo;
    HWND popup =
        state.hwnd;

    if (combo &&
        !cancel &&
        commitIndex >= 0 &&
        commitIndex <
            static_cast<int>(
                state.items.size())) {
        const LRESULT oldSelection =
            SendMessageW(
                combo,
                CB_GETCURSEL,
                0,
                0);

        if (oldSelection !=
            commitIndex) {
            SendMessageW(
                combo,
                CB_SETCURSEL,
                static_cast<WPARAM>(
                    commitIndex),
                0);
            NotifyCombo(
                combo,
                CBN_SELCHANGE);
        }

        NotifyCombo(
            combo,
            CBN_SELENDOK);
    } else if (combo) {
        NotifyCombo(
            combo,
            CBN_SELENDCANCEL);
    }

    if (combo) {
        RemovePropW(
            combo,
            kPopupProperty);
        NotifyCombo(
            combo,
            CBN_CLOSEUP);
        InvalidateRect(
            combo,
            nullptr,
            FALSE);
    }

    if (popup &&
        IsWindow(
            popup)) {
        DestroyWindow(
            popup);
    }
}

void MoveActive(
    PopupState& state,
    int next) {

    if (state.items.empty()) {
        return;
    }

    state.activeIndex =
        std::clamp(
            next,
            0,
            static_cast<int>(
                state.items.size()) -
                1);

    EnsureActiveVisible(
        state);
    InvalidateRect(
        state.hwnd,
        nullptr,
        FALSE);
}

LRESULT CALLBACK PopupProc(
    HWND hwnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam) {

    PopupState* state =
        StateForPopup(
            hwnd);

    if (message ==
        WM_NCCREATE) {
        auto* create =
            reinterpret_cast<
                CREATESTRUCTW*>(
                lParam);
        state =
            static_cast<PopupState*>(
                create->lpCreateParams);

        if (!state) {
            return FALSE;
        }

        state->hwnd =
            hwnd;
        SetWindowLongPtrW(
            hwnd,
            GWLP_USERDATA,
            reinterpret_cast<LONG_PTR>(
                state));
        return TRUE;
    }

    if (!state) {
        return DefWindowProcW(
            hwnd,
            message,
            wParam,
            lParam);
    }

    switch (message) {
    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT: {
        PAINTSTRUCT paint{};
        HDC dc =
            BeginPaint(
                hwnd,
                &paint);
        DrawPopup(
            *state,
            dc);
        EndPaint(
            hwnd,
            &paint);
        return 0;
    }

    case WM_PRINTCLIENT:
        DrawPopup(
            *state,
            reinterpret_cast<HDC>(
                wParam));
        return 0;

    case WM_MOUSEACTIVATE:
        return MA_NOACTIVATE;

    case WM_MOUSEMOVE: {
        TRACKMOUSEEVENT track{
            sizeof(track),
            TME_LEAVE,
            hwnd,
            0,
        };
        TrackMouseEvent(
            &track);

        POINT point{
            GET_X_LPARAM(
                lParam),
            GET_Y_LPARAM(
                lParam),
        };
        const int next =
            RowFromPoint(
                *state,
                point);

        if (next !=
            state->hoverIndex) {
            state->hoverIndex =
                next;
            InvalidateRect(
                hwnd,
                nullptr,
                FALSE);
        }

        return 0;
    }

    case WM_MOUSELEAVE:
        if (state->hoverIndex !=
            -1) {
            state->hoverIndex =
                -1;
            InvalidateRect(
                hwnd,
                nullptr,
                FALSE);
        }
        return 0;

    case WM_LBUTTONUP: {
        POINT point{
            GET_X_LPARAM(
                lParam),
            GET_Y_LPARAM(
                lParam),
        };
        const int index =
            RowFromPoint(
                *state,
                point);

        if (index >= 0) {
            ClosePopup(
                *state,
                false,
                index);
        }

        return 0;
    }

    case WM_MOUSEWHEEL: {
        if (static_cast<int>(
                state->items.size()) <=
            state->visibleRows) {
            return 0;
        }

        const int delta =
            GET_WHEEL_DELTA_WPARAM(
                wParam);
        const int steps =
            std::max(
                1,
                std::abs(delta) /
                    WHEEL_DELTA);
        const int direction =
            delta > 0
                ? -1
                : 1;

        state->topIndex +=
            direction *
            steps;
        ClampTopIndex(
            *state);
        state->hoverIndex =
            -1;
        InvalidateRect(
            hwnd,
            nullptr,
            FALSE);
        return 0;
    }

    case WM_NCDESTROY: {
        if (state->combo &&
            PopupForCombo(
                state->combo) ==
                hwnd) {
            RemovePropW(
                state->combo,
                kPopupProperty);
        }

        SetWindowLongPtrW(
            hwnd,
            GWLP_USERDATA,
            0);
        delete state;
        return 0;
    }

    default:
        break;
    }

    return DefWindowProcW(
        hwnd,
        message,
        wParam,
        lParam);
}

[[nodiscard]] bool EnsurePopupClass(
    HINSTANCE instance) {

    WNDCLASSEXW existing{
        sizeof(existing),
    };

    if (GetClassInfoExW(
            instance,
            kPopupClassName,
            &existing)) {
        return true;
    }

    WNDCLASSEXW wc{
        sizeof(wc),
    };
    wc.style =
        CS_HREDRAW |
        CS_VREDRAW |
        CS_DROPSHADOW;
    wc.lpfnWndProc =
        PopupProc;
    wc.hInstance =
        instance;
    wc.hCursor =
        LoadCursorW(
            nullptr,
            IDC_ARROW);
    wc.lpszClassName =
        kPopupClassName;

    return RegisterClassExW(
               &wc) != 0 ||
        GetLastError() ==
            ERROR_CLASS_ALREADY_EXISTS;
}

[[nodiscard]] std::vector<std::wstring>
ReadItems(
    HWND combo) {

    std::vector<std::wstring> items;

    const LRESULT count =
        SendMessageW(
            combo,
            CB_GETCOUNT,
            0,
            0);

    if (count <= 0 ||
        count == CB_ERR) {
        return items;
    }

    items.reserve(
        static_cast<std::size_t>(
            count));

    for (LRESULT index = 0;
         index < count;
         ++index) {
        const LRESULT length =
            SendMessageW(
                combo,
                CB_GETLBTEXTLEN,
                static_cast<WPARAM>(
                    index),
                0);

        if (length < 0 ||
            length == CB_ERR) {
            items.emplace_back();
            continue;
        }

        std::wstring text(
            static_cast<std::size_t>(
                length) + 1,
            L'\0');

        if (SendMessageW(
                combo,
                CB_GETLBTEXT,
                static_cast<WPARAM>(
                    index),
                reinterpret_cast<LPARAM>(
                    text.data())) ==
            CB_ERR) {
            items.emplace_back();
            continue;
        }

        text.resize(
            static_cast<std::size_t>(
                length));
        items.push_back(
            std::move(text));
    }

    return items;
}

} // namespace

bool ShowNextComboPopup(
    HWND combo,
    UINT dpi) {

    if (!combo ||
        !IsWindowEnabled(
            combo)) {
        return false;
    }

    if (IsNextComboPopupVisible(
            combo)) {
        return true;
    }

    auto items =
        ReadItems(
            combo);

    if (items.empty()) {
        return false;
    }

    HINSTANCE instance =
        reinterpret_cast<HINSTANCE>(
            GetWindowLongPtrW(
                combo,
                GWLP_HINSTANCE));

    if (!EnsurePopupClass(
            instance)) {
        return false;
    }

    auto* state =
        new PopupState{};
    state->combo =
        combo;
    state->dpi =
        dpi != 0
            ? dpi
            : 96;
    state->font =
        reinterpret_cast<HFONT>(
            SendMessageW(
                combo,
                WM_GETFONT,
                0,
                0));
    state->items =
        std::move(items);
    state->visibleRows =
        static_cast<int>(
            NextComboBoxVisibleItems(
                state->items.size()));

    const LRESULT selected =
        SendMessageW(
            combo,
            CB_GETCURSEL,
            0,
            0);
    state->selectedIndex =
        selected != CB_ERR
            ? static_cast<int>(
                  selected)
            : -1;
    state->activeIndex =
        state->selectedIndex >= 0
            ? state->selectedIndex
            : 0;

    EnsureActiveVisible(
        *state);

    RECT comboRect{};
    GetWindowRect(
        combo,
        &comboRect);

    const int width =
        std::max(
            1,
            comboRect.right -
                comboRect.left);
    const int padding =
        S(
            kNextComboPopupPaddingLogical,
            state->dpi);
    const int rowHeight =
        S(
            kNextComboPopupRowHeightLogical,
            state->dpi);
    const int gap =
        S(
            kNextComboPopupGapLogical,
            state->dpi);
    const int height =
        padding * 2 +
        rowHeight *
            state->visibleRows;

    MONITORINFO monitorInfo{
        sizeof(monitorInfo),
    };
    GetMonitorInfoW(
        MonitorFromWindow(
            combo,
            MONITOR_DEFAULTTONEAREST),
        &monitorInfo);

    int x =
        comboRect.left;
    int y =
        comboRect.bottom +
        gap;

    if (y + height >
        monitorInfo.rcWork.bottom) {
        const int above =
            comboRect.top -
            gap -
            height;

        if (above >=
            monitorInfo.rcWork.top) {
            y =
                above;
        } else {
            y =
                std::max(
                    monitorInfo.rcWork.top,
                    monitorInfo.rcWork.bottom -
                        height);
        }
    }

    x =
        std::clamp(
            x,
            monitorInfo.rcWork.left,
            std::max(
                monitorInfo.rcWork.left,
                monitorInfo.rcWork.right -
                    width));

    HWND owner =
        GetAncestor(
            combo,
            GA_ROOT);

    HWND popup =
        CreateWindowExW(
            WS_EX_TOOLWINDOW |
                WS_EX_NOACTIVATE,
            kPopupClassName,
            L"",
            WS_POPUP,
            x,
            y,
            width,
            height,
            owner,
            nullptr,
            instance,
            state);

    if (!popup) {
        delete state;
        return false;
    }

    SetPropW(
        combo,
        kPopupProperty,
        popup);

    ApplyRoundedRegion(
        popup,
        state->dpi);

    SetWindowPos(
        popup,
        HWND_TOP,
        x,
        y,
        width,
        height,
        SWP_NOACTIVATE |
            SWP_SHOWWINDOW);

    NotifyCombo(
        combo,
        CBN_DROPDOWN);

    InvalidateRect(
        combo,
        nullptr,
        FALSE);

    return true;
}

void HideNextComboPopup(
    HWND combo,
    bool cancel) {

    HWND popup =
        PopupForCombo(
            combo);
    PopupState* state =
        StateForPopup(
            popup);

    if (!state) {
        return;
    }

    ClosePopup(
        *state,
        cancel,
        cancel
            ? -1
            : state->activeIndex);
}

void DestroyNextComboPopup(
    HWND combo) noexcept {

    HWND popup =
        PopupForCombo(
            combo);

    if (!popup) {
        return;
    }

    RemovePropW(
        combo,
        kPopupProperty);

    if (IsWindow(
            popup)) {
        DestroyWindow(
            popup);
    }
}

bool IsNextComboPopupVisible(
    HWND combo) noexcept {

    HWND popup =
        PopupForCombo(
            combo);

    return popup &&
        IsWindow(
            popup) &&
        IsWindowVisible(
            popup);
}

bool HandleNextComboPopupKey(
    HWND combo,
    UINT message,
    WPARAM wParam,
    LPARAM) {

    HWND popup =
        PopupForCombo(
            combo);
    PopupState* state =
        StateForPopup(
            popup);

    if (!state) {
        return false;
    }

    if (message != WM_KEYDOWN &&
        message != WM_SYSKEYDOWN) {
        return false;
    }

    switch (wParam) {
    case VK_ESCAPE:
        HideNextComboPopup(
            combo,
            true);
        return true;

    case VK_RETURN:
        ClosePopup(
            *state,
            false,
            state->activeIndex);
        return true;

    case VK_UP:
        MoveActive(
            *state,
            state->activeIndex - 1);
        return true;

    case VK_DOWN:
        MoveActive(
            *state,
            state->activeIndex + 1);
        return true;

    case VK_HOME:
        MoveActive(
            *state,
            0);
        return true;

    case VK_END:
        MoveActive(
            *state,
            static_cast<int>(
                state->items.size()) -
                1);
        return true;

    case VK_PRIOR:
        MoveActive(
            *state,
            state->activeIndex -
                std::max(
                    1,
                    state->visibleRows));
        return true;

    case VK_NEXT:
        MoveActive(
            *state,
            state->activeIndex +
                std::max(
                    1,
                    state->visibleRows));
        return true;

    case VK_F4:
        HideNextComboPopup(
            combo,
            true);
        return true;

    case VK_TAB:
        HideNextComboPopup(
            combo,
            true);
        return false;

    default:
        break;
    }

    return false;
}

bool HandleNextComboPopupWheel(
    HWND combo,
    UINT message,
    WPARAM wParam,
    LPARAM lParam) {

    HWND popup =
        PopupForCombo(
            combo);

    if (!popup ||
        !IsWindowVisible(
            popup)) {
        return false;
    }

    if (message != WM_MOUSEWHEEL) {
        return false;
    }

    SendMessageW(
        popup,
        message,
        wParam,
        lParam);
    return true;
}

} // namespace altrun::ui
