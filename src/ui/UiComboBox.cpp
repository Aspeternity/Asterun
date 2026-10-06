#include "UiComboBox.hpp"
#include "UiComboPopup.hpp"

#include "UiMetrics.hpp"
#include "UiTheme.hpp"

#include <commctrl.h>

#include <algorithm>
#include <string>

namespace altrun::ui {
namespace {

constexpr UINT_PTR kNextComboSubclassId =
    0xC0B0;

[[nodiscard]] UINT ComboDpi(
    HWND combo) noexcept {

    const UINT dpi =
        combo
            ? GetDpiForWindow(combo)
            : 0;

    return dpi != 0
        ? dpi
        : 96;
}

constexpr wchar_t
    kNextComboDroppedProperty[] =
        L"Asterun.NextCombo.Dropped";

constexpr wchar_t
    kNextComboHoveredProperty[] =
        L"Asterun.NextCombo.Hovered";

void SetTrackedDroppedState(
    HWND combo,
    bool dropped) {

    if (!combo) {
        return;
    }

    SetPropW(
        combo,
        kNextComboDroppedProperty,
        reinterpret_cast<HANDLE>(
            static_cast<ULONG_PTR>(
                dropped ? 2 : 1)));
}

[[nodiscard]] bool
TrackedDroppedState(
    HWND combo) {

    const HANDLE value =
        combo
            ? GetPropW(
                  combo,
                  kNextComboDroppedProperty)
            : nullptr;

    if (value) {
        return
            reinterpret_cast<ULONG_PTR>(
                value) == 2;
    }

    return
        combo &&
        SendMessageW(
            combo,
            CB_GETDROPPEDSTATE,
            0,
            0) != 0;
}

void SetTrackedHoveredState(
    HWND combo,
    bool hovered) {

    if (!combo) {
        return;
    }

    SetPropW(
        combo,
        kNextComboHoveredProperty,
        reinterpret_cast<HANDLE>(
            static_cast<ULONG_PTR>(
                hovered ? 2 : 1)));
}

[[nodiscard]] bool
TrackedHoveredState(
    HWND combo) {

    const HANDLE value =
        combo
            ? GetPropW(
                  combo,
                  kNextComboHoveredProperty)
            : nullptr;

    return value &&
        reinterpret_cast<ULONG_PTR>(
            value) == 2;
}

[[nodiscard]] COLORREF
NextComboFillColor(
    HWND combo) {

    const bool active =
        combo &&
        (GetFocus() == combo ||
         TrackedDroppedState(
             combo));
    const bool hovered =
        TrackedHoveredState(
            combo);

    return active
        ? RGB(248, 252, 255)
        : hovered
            ? RGB(251, 252, 253)
            : RGB(255, 255, 255);
}

[[nodiscard]] HFONT ComboFont(
    HWND combo) noexcept {

    HFONT font =
        reinterpret_cast<HFONT>(
            SendMessageW(
                combo,
                WM_GETFONT,
                0,
                0));

    if (!font) {
        font =
            reinterpret_cast<HFONT>(
                GetStockObject(
                    DEFAULT_GUI_FONT));
    }

    return font;
}

void DrawNextComboBoxSurfaceDirect(
    HWND combo,
    HDC dc,
    COLORREF hostBackground) {

    RECT rect{};
    GetClientRect(
        combo,
        &rect);

    HBRUSH outer =
        CreateSolidBrush(
            hostBackground);
    FillRect(
        dc,
        &rect,
        outer);
    DeleteObject(
        outer);

    RECT surface =
        rect;
    InflateRect(
        &surface,
        -1,
        -1);

    const UINT dpi =
        ComboDpi(combo);
    const bool enabled =
        IsWindowEnabled(combo) != FALSE;
    const bool dropped =
        TrackedDroppedState(
            combo);
    const bool active =
        GetFocus() == combo ||
        dropped;

    const bool hovered =
        TrackedHoveredState(
            combo);

    const COLORREF borderColor =
        active
            ? kApplicationPalette.accent
            : kApplicationPalette.frame;
    const COLORREF fillColor =
        NextComboFillColor(
            combo);

    HBRUSH fill =
        CreateSolidBrush(
            fillColor);
    HPEN border =
        CreatePen(
            PS_SOLID,
            1,
            borderColor);

    HGDIOBJ oldBrush =
        SelectObject(
            dc,
            fill);
    HGDIOBJ oldPen =
        SelectObject(
            dc,
            border);

    const int radius =
        Scale(6, dpi);

    RoundRect(
        dc,
        surface.left,
        surface.top,
        surface.right,
        surface.bottom,
        radius,
        radius);

    SelectObject(
        dc,
        oldBrush);
    SelectObject(
        dc,
        oldPen);
    DeleteObject(
        fill);
    DeleteObject(
        border);

    const int arrowAreaWidth =
        Scale(36, dpi);
    RECT arrowArea{
        std::max(
            surface.left,
            surface.right -
                arrowAreaWidth),
        surface.top,
        surface.right,
        surface.bottom,
    };

    POINT cursor{};
    POINT clientCursor{};
    bool arrowHovered = false;

    if (enabled &&
        hovered &&
        GetCursorPos(
            &cursor)) {
        clientCursor =
            cursor;
        arrowHovered =
            ScreenToClient(
                combo,
                &clientCursor) &&
            PtInRect(
                &arrowArea,
                clientCursor);
    }

    if (arrowHovered || dropped) {
        RECT buttonRect =
            arrowArea;
        InflateRect(
            &buttonRect,
            -Scale(4, dpi),
            -Scale(4, dpi));

        HBRUSH buttonFill =
            CreateSolidBrush(
                dropped
                    ? RGB(237, 247, 254)
                    : RGB(244, 247, 249));
        HGDIOBJ oldButtonBrush =
            SelectObject(
                dc,
                buttonFill);
        HGDIOBJ oldButtonPen =
            SelectObject(
                dc,
                GetStockObject(
                    NULL_PEN));

        const int buttonRadius =
            Scale(6, dpi);

        RoundRect(
            dc,
            buttonRect.left,
            buttonRect.top,
            buttonRect.right,
            buttonRect.bottom,
            buttonRadius,
            buttonRadius);

        SelectObject(
            dc,
            oldButtonPen);
        SelectObject(
            dc,
            oldButtonBrush);
        DeleteObject(
            buttonFill);
    }

    const int arrowCenterX =
        arrowArea.left +
        (arrowArea.right -
         arrowArea.left) / 2;
    const int arrowCenterY =
        arrowArea.top +
        (arrowArea.bottom -
         arrowArea.top) / 2;

    const COLORREF arrowColor =
        enabled
            ? dropped
                ? kApplicationPalette.accent
                : RGB(78, 86, 94)
            : RGB(166, 172, 179);

    HPEN arrowPen =
        CreatePen(
            PS_SOLID,
            std::max(
                1,
                Scale(2, dpi)),
            arrowColor);
    oldPen =
        SelectObject(
            dc,
            arrowPen);

    const int arrowHalfWidth =
        Scale(5, dpi);
    const int arrowHalfHeight =
        Scale(3, dpi);

    if (dropped) {
        const int apexY =
            arrowCenterY -
            arrowHalfHeight;
        const int armY =
            arrowCenterY +
            arrowHalfHeight / 2;

        MoveToEx(
            dc,
            arrowCenterX -
                arrowHalfWidth,
            armY,
            nullptr);
        LineTo(
            dc,
            arrowCenterX,
            apexY);

        MoveToEx(
            dc,
            arrowCenterX,
            apexY,
            nullptr);
        LineTo(
            dc,
            arrowCenterX +
                arrowHalfWidth,
            armY);
    } else {
        const int apexY =
            arrowCenterY +
            arrowHalfHeight;
        const int armY =
            arrowCenterY -
            arrowHalfHeight / 2;

        MoveToEx(
            dc,
            arrowCenterX -
                arrowHalfWidth,
            armY,
            nullptr);
        LineTo(
            dc,
            arrowCenterX,
            apexY);

        MoveToEx(
            dc,
            arrowCenterX,
            apexY,
            nullptr);
        LineTo(
            dc,
            arrowCenterX +
                arrowHalfWidth,
            armY);
    }

    SelectObject(
        dc,
        oldPen);
    DeleteObject(
        arrowPen);

    wchar_t text[512]{};
    const LRESULT selected =
        SendMessageW(
            combo,
            CB_GETCURSEL,
            0,
            0);

    if (selected != CB_ERR) {
        SendMessageW(
            combo,
            CB_GETLBTEXT,
            static_cast<WPARAM>(
                selected),
            reinterpret_cast<LPARAM>(
                text));
    }

    RECT textRect{
        surface.left +
            Scale(12, dpi),
        surface.top,
        arrowArea.left -
            Scale(6, dpi),
        surface.bottom,
    };

    SetBkMode(
        dc,
        TRANSPARENT);
    SetTextColor(
        dc,
        enabled
            ? kApplicationPalette.text
            : kApplicationPalette.mutedText);

    HGDIOBJ oldFont =
        SelectObject(
            dc,
            ComboFont(combo));

    DrawTextW(
        dc,
        text,
        -1,
        &textRect,
        DT_LEFT |
            DT_VCENTER |
            DT_SINGLELINE |
            DT_END_ELLIPSIS |
            DT_NOPREFIX);

    SelectObject(
        dc,
        oldFont);
}

void DrawNextComboBoxSurface(
    HWND combo,
    HDC dc,
    COLORREF hostBackground) {

    RECT rect{};
    GetClientRect(
        combo,
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

        DrawNextComboBoxSurfaceDirect(
            combo,
            dc,
            hostBackground);
        return;
    }

    HGDIOBJ oldBitmap =
        SelectObject(
            buffer,
            bitmap);

    DrawNextComboBoxSurfaceDirect(
        combo,
        buffer,
        hostBackground);

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

LRESULT CALLBACK NextComboSubclassProc(
    HWND hwnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam,
    UINT_PTR subclassId,
    DWORD_PTR refData) {

    const COLORREF hostBackground =
        static_cast<COLORREF>(
            refData);

    switch (message) {
    case WM_PAINT: {
        PAINTSTRUCT paint{};
        HDC dc =
            BeginPaint(
                hwnd,
                &paint);

        DrawNextComboBoxSurface(
            hwnd,
            dc,
            hostBackground);

        EndPaint(
            hwnd,
            &paint);
        return 0;
    }

    case WM_PRINTCLIENT:
        DrawNextComboBoxSurface(
            hwnd,
            reinterpret_cast<HDC>(
                wParam),
            hostBackground);
        return 0;

    case WM_MOUSEMOVE: {
        TRACKMOUSEEVENT track{
            sizeof(track),
            TME_LEAVE,
            hwnd,
            0,
        };
        TrackMouseEvent(
            &track);

        const LRESULT result =
            DefSubclassProc(
                hwnd,
                message,
                wParam,
                lParam);

        if (!TrackedHoveredState(
                hwnd)) {
            SetTrackedHoveredState(
                hwnd,
                true);
            InvalidateRect(
                hwnd,
                nullptr,
                FALSE);
        }

        return result;
    }

    case WM_MOUSELEAVE: {
        const LRESULT result =
            DefSubclassProc(
                hwnd,
                message,
                wParam,
                lParam);

        if (TrackedHoveredState(
                hwnd)) {
            SetTrackedHoveredState(
                hwnd,
                false);
            InvalidateRect(
                hwnd,
                nullptr,
                FALSE);
        }

        return result;
    }

    case WM_MOUSEWHEEL:
    case WM_MOUSEHWHEEL: {
        if (HandleNextComboPopupWheel(
                hwnd,
                message,
                wParam,
                lParam)) {
            return 0;
        }

        HWND parent =
            GetParent(
                hwnd);

        if (parent) {
            SendMessageW(
                parent,
                message,
                wParam,
                lParam);
        }

        return 0;
    }

    case CB_GETDROPPEDSTATE:
        return IsNextComboPopupVisible(
                   hwnd)
            ? TRUE
            : FALSE;

    case CB_SHOWDROPDOWN: {
        const bool show =
            wParam != FALSE;

        if (show) {
            SetFocus(
                hwnd);
            SetTrackedDroppedState(
                hwnd,
                true);

            if (!ShowNextComboPopup(
                    hwnd,
                    ComboDpi(
                        hwnd))) {
                SetTrackedDroppedState(
                    hwnd,
                    false);
                InvalidateRect(
                    hwnd,
                    nullptr,
                    FALSE);
                return FALSE;
            }

            InvalidateRect(
                hwnd,
                nullptr,
                FALSE);
            return TRUE;
        }

        HideNextComboPopup(
            hwnd,
            true);
        SetTrackedDroppedState(
            hwnd,
            false);
        InvalidateRect(
            hwnd,
            nullptr,
            FALSE);
        return TRUE;
    }

    case WM_KEYDOWN:
    case WM_SYSKEYDOWN: {
        if (HandleNextComboPopupKey(
                hwnd,
                message,
                wParam,
                lParam)) {
            return 0;
        }

        const bool altDown =
            (GetKeyState(
                 VK_MENU) &
             0x8000) != 0;

        if (wParam == VK_F4 ||
            (altDown &&
             wParam == VK_DOWN)) {
            SetFocus(
                hwnd);
            SetTrackedDroppedState(
                hwnd,
                true);

            if (!ShowNextComboPopup(
                    hwnd,
                    ComboDpi(
                        hwnd))) {
                SetTrackedDroppedState(
                    hwnd,
                    false);
            }

            InvalidateRect(
                hwnd,
                nullptr,
                FALSE);
            return 0;
        }

        return DefSubclassProc(
            hwnd,
            message,
            wParam,
            lParam);
    }

    case WM_SETFOCUS: {
        const LRESULT result =
            DefSubclassProc(
                hwnd,
                message,
                wParam,
                lParam);

        RedrawWindow(
            hwnd,
            nullptr,
            nullptr,
            RDW_INVALIDATE |
                RDW_NOERASE |
                RDW_UPDATENOW);
        return result;
    }

    case WM_KILLFOCUS: {
        HideNextComboPopup(
            hwnd,
            true);
        SetTrackedDroppedState(
            hwnd,
            false);

        const LRESULT result =
            DefSubclassProc(
                hwnd,
                message,
                wParam,
                lParam);

        RedrawWindow(
            hwnd,
            nullptr,
            nullptr,
            RDW_INVALIDATE |
                RDW_NOERASE |
                RDW_UPDATENOW);
        return result;
    }

    case WM_LBUTTONDOWN:
    case WM_LBUTTONDBLCLK: {
        SetFocus(
            hwnd);

        if (IsNextComboPopupVisible(
                hwnd)) {
            HideNextComboPopup(
                hwnd,
                true);
            SetTrackedDroppedState(
                hwnd,
                false);
        } else {
            SetTrackedDroppedState(
                hwnd,
                true);

            if (!ShowNextComboPopup(
                    hwnd,
                    ComboDpi(
                        hwnd))) {
                SetTrackedDroppedState(
                    hwnd,
                    false);
            }
        }

        InvalidateRect(
            hwnd,
            nullptr,
            FALSE);
        return 0;
    }

    case WM_LBUTTONUP:
        return 0;

    case WM_ENABLE: {
        if (wParam == FALSE &&
            IsNextComboPopupVisible(
                hwnd)) {
            HideNextComboPopup(
                hwnd,
                true);
            SetTrackedDroppedState(
                hwnd,
                false);
        }

        const LRESULT result =
            DefSubclassProc(
                hwnd,
                message,
                wParam,
                lParam);

        InvalidateRect(
            hwnd,
            nullptr,
            FALSE);
        return result;
    }

    case WM_SHOWWINDOW:
        if (wParam == FALSE &&
            IsNextComboPopupVisible(
                hwnd)) {
            HideNextComboPopup(
                hwnd,
                true);
            SetTrackedDroppedState(
                hwnd,
                false);
        }
        return DefSubclassProc(
            hwnd,
            message,
            wParam,
            lParam);

    case WM_WINDOWPOSCHANGED: {
        const bool hadPopup =
            IsNextComboPopupVisible(
                hwnd);

        const LRESULT result =
            DefSubclassProc(
                hwnd,
                message,
                wParam,
                lParam);

        if (hadPopup) {
            HideNextComboPopup(
                hwnd,
                true);
            SetTrackedDroppedState(
                hwnd,
                false);
            InvalidateRect(
                hwnd,
                nullptr,
                FALSE);
        }

        return result;
    }

    case CB_SETCURSEL:
    case WM_SETFONT: {
        const LRESULT result =
            DefSubclassProc(
                hwnd,
                message,
                wParam,
                lParam);

        InvalidateRect(
            hwnd,
            nullptr,
            FALSE);
        return result;
    }

    case WM_NCDESTROY:
        DestroyNextComboPopup(
            hwnd);
        RemovePropW(
            hwnd,
            kNextComboHoveredProperty);
        RemovePropW(
            hwnd,
            kNextComboDroppedProperty);
        RemoveWindowSubclass(
            hwnd,
            NextComboSubclassProc,
            subclassId);
        break;

    default:
        break;
    }

    return DefSubclassProc(
        hwnd,
        message,
        wParam,
        lParam);
}

} // namespace

HWND CreateNextComboBox(
    HWND parent,
    HINSTANCE instance,
    UINT id,
    COLORREF hostBackground) {

    HWND combo =
        CreateWindowExW(
            0,
            L"COMBOBOX",
            L"",
            WS_CHILD |
                WS_VISIBLE |
                WS_TABSTOP |
                CBS_DROPDOWNLIST |
                CBS_OWNERDRAWFIXED |
                CBS_HASSTRINGS,
            0,
            0,
            0,
            0,
            parent,
            reinterpret_cast<HMENU>(
                static_cast<UINT_PTR>(
                    id)),
            instance,
            nullptr);

    if (combo) {
        SetWindowSubclass(
            combo,
            NextComboSubclassProc,
            kNextComboSubclassId,
            static_cast<DWORD_PTR>(
                hostBackground));
        SetTrackedDroppedState(
            combo,
            false);
        SetTrackedHoveredState(
            combo,
            false);
    }

    return combo;
}

void ApplyNextComboBoxMetrics(
    HWND combo,
    UINT dpi) {

    if (!combo) {
        return;
    }

    const LPARAM itemHeight =
        static_cast<LPARAM>(
            NextComboBoxItemHeight(
                dpi));

    SendMessageW(
        combo,
        CB_SETITEMHEIGHT,
        static_cast<WPARAM>(-1),
        itemHeight);

    if (SendMessageW(
            combo,
            CB_GETCOUNT,
            0,
            0) > 0) {
        SendMessageW(
            combo,
            CB_SETITEMHEIGHT,
            0,
            itemHeight);
    }

    InvalidateRect(
        combo,
        nullptr,
        FALSE);
}

void MoveNextComboBox(
    HWND combo,
    int x,
    int y,
    int width,
    UINT dpi,
    BOOL repaint) {

    if (!combo) {
        return;
    }

    ApplyNextComboBoxMetrics(
        combo,
        dpi);

    const LRESULT countResult =
        SendMessageW(
            combo,
            CB_GETCOUNT,
            0,
            0);
    const std::size_t itemCount =
        countResult > 0
            ? static_cast<std::size_t>(
                  countResult)
            : 0;
    const std::size_t visibleItems =
        NextComboBoxVisibleItems(
            itemCount);

    if (visibleItems > 0) {
        SendMessageW(
            combo,
            CB_SETMINVISIBLE,
            static_cast<WPARAM>(
                visibleItems),
            0);
    }

    // The ComboBox owns a separate LISTBOX window. Do not give every short
    // list a permanent native scrollbar; only expose one when the shared
    // visible-item cap actually hides entries.
    COMBOBOXINFO comboInfo{};
    comboInfo.cbSize =
        sizeof(comboInfo);

    if (GetComboBoxInfo(
            combo,
            &comboInfo) &&
        comboInfo.hwndList) {
        const bool needsScroll =
            itemCount >
            visibleItems;

        LONG_PTR listStyle =
            GetWindowLongPtrW(
                comboInfo.hwndList,
                GWL_STYLE);
        const LONG_PTR desiredStyle =
            needsScroll
                ? listStyle |
                      WS_VSCROLL
                : listStyle &
                      ~static_cast<LONG_PTR>(
                          WS_VSCROLL);

        if (desiredStyle !=
            listStyle) {
            SetWindowLongPtrW(
                comboInfo.hwndList,
                GWL_STYLE,
                desiredStyle);
        }

        ShowScrollBar(
            comboInfo.hwndList,
            SB_VERT,
            needsScroll);
    }

    const int height =
        NextComboBoxDropHeightForDpi(
            itemCount,
            dpi);

    if (repaint) {
        MoveWindow(
            combo,
            x,
            y,
            width,
            height,
            TRUE);
    } else {
        // Settings scroll commits one final content-pane repaint. Prevent
        // USER32 from copying stale ComboBox client pixels to the new
        // position while the control is moving between those frames.
        SetWindowPos(
            combo,
            nullptr,
            x,
            y,
            width,
            height,
            SWP_NOZORDER |
                SWP_NOACTIVATE |
                SWP_NOREDRAW |
                SWP_NOCOPYBITS);
    }
}

void RefreshNextComboBoxState(
    HWND combo,
    UINT notification) {

    if (!combo) {
        return;
    }

    switch (notification) {
    case CBN_DROPDOWN:
        SetTrackedDroppedState(
            combo,
            true);
        InvalidateRect(
            combo,
            nullptr,
            FALSE);
        break;

    case CBN_CLOSEUP:
    case CBN_SELENDOK:
    case CBN_SELENDCANCEL:
        SetTrackedDroppedState(
            combo,
            false);
        InvalidateRect(
            combo,
            nullptr,
            FALSE);
        break;

    case CBN_SELCHANGE:
        InvalidateRect(
            combo,
            nullptr,
            FALSE);
        break;

    default:
        break;
    }
}

int MeasureNextComboBoxPreferredWidth(
    HWND combo,
    UINT dpi,
    int minimumLogical,
    int maximumLogical) {

    const int logicalMinimum =
        std::max(
            0,
            minimumLogical);
    const int logicalMaximum =
        std::max(
            logicalMinimum,
            maximumLogical);
    const int minimum =
        Scale(
            logicalMinimum,
            dpi);
    const int maximum =
        Scale(
            logicalMaximum,
            dpi);
    const int fallback =
        std::clamp(
            Scale(
                std::max(
                    logicalMinimum,
                    150),
                dpi),
            minimum,
            maximum);

    if (!combo) {
        return fallback;
    }

    HDC dc =
        GetDC(combo);

    if (!dc) {
        return fallback;
    }

    HGDIOBJ oldFont =
        SelectObject(
            dc,
            ComboFont(combo));

    int widest = 0;
    const LRESULT count =
        SendMessageW(
            combo,
            CB_GETCOUNT,
            0,
            0);

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

        if (length <= 0 ||
            length == CB_ERR) {
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
            continue;
        }

        text.resize(
            static_cast<std::size_t>(
                length));

        SIZE extent{};

        if (GetTextExtentPoint32W(
                dc,
                text.c_str(),
                static_cast<int>(
                    text.size()),
                &extent)) {
            widest =
                std::max(
                    widest,
                    static_cast<int>(
                        extent.cx));
        }
    }

    SelectObject(
        dc,
        oldFont);
    ReleaseDC(
        combo,
        dc);

    // Left text inset + fixed dropdown affordance + breathing room.
    // Keep this in sync with DrawNextComboBoxSurface().
    const int chrome =
        Scale(58, dpi);

    return std::clamp(
        widest + chrome,
        minimum,
        maximum);
}

UINT NextComboBoxItemHeight(
    UINT dpi) noexcept {

    return static_cast<UINT>(
        Scale(
            kNextComboBoxItemHeightLogical,
            dpi));
}

void DrawNextComboBoxItem(
    const DRAWITEMSTRUCT& item,
    UINT dpi) {

    // CBS_OWNERDRAWFIXED asks the parent to paint the closed selection
    // field synchronously during focus transitions. Returning TRUE without
    // painting it creates a transient blank frame before our subclass WM_PAINT
    // runs. Paint only the native selection rectangle here; the shared
    // subclass remains the sole owner of the border and chevron area.
    if ((item.itemState &
         ODS_COMBOBOXEDIT) != 0) {
        RECT rect =
            item.rcItem;

        COMBOBOXINFO comboInfo{};
        comboInfo.cbSize =
            sizeof(comboInfo);

        if (GetComboBoxInfo(
                item.hwndItem,
                &comboInfo)) {
            RECT clipped{};
            if (IntersectRect(
                    &clipped,
                    &rect,
                    &comboInfo.rcItem)) {
                rect =
                    clipped;
            }
        }

        HBRUSH fill =
            CreateSolidBrush(
                NextComboFillColor(
                    item.hwndItem));
        FillRect(
            item.hDC,
            &rect,
            fill);
        DeleteObject(
            fill);

        if (item.itemID !=
            static_cast<UINT>(-1)) {
            wchar_t text[512]{};
            SendMessageW(
                item.hwndItem,
                CB_GETLBTEXT,
                item.itemID,
                reinterpret_cast<LPARAM>(
                    text));

            RECT textRect =
                rect;
            textRect.left +=
                Scale(12, dpi);
            textRect.right -=
                Scale(6, dpi);

            SetBkMode(
                item.hDC,
                TRANSPARENT);
            SetTextColor(
                item.hDC,
                IsWindowEnabled(
                    item.hwndItem)
                    ? kApplicationPalette.text
                    : kApplicationPalette.mutedText);

            HGDIOBJ oldFont =
                SelectObject(
                    item.hDC,
                    ComboFont(
                        item.hwndItem));

            DrawTextW(
                item.hDC,
                text,
                -1,
                &textRect,
                DT_LEFT |
                    DT_VCENTER |
                    DT_SINGLELINE |
                    DT_END_ELLIPSIS |
                    DT_NOPREFIX);

            SelectObject(
                item.hDC,
                oldFont);
        }

        return;
    }

    RECT rect =
        item.rcItem;

    const bool selected =
        (item.itemState &
         ODS_SELECTED) != 0;
    const bool disabled =
        (item.itemState &
         ODS_DISABLED) != 0;

    const COLORREF background =
        selected
            ? RGB(231, 242, 252)
            : RGB(255, 255, 255);

    HBRUSH fill =
        CreateSolidBrush(
            background);
    FillRect(
        item.hDC,
        &rect,
        fill);
    DeleteObject(
        fill);

    if (item.itemID ==
            static_cast<UINT>(-1)) {
        return;
    }

    wchar_t text[512]{};

    SendMessageW(
        item.hwndItem,
        CB_GETLBTEXT,
        item.itemID,
        reinterpret_cast<LPARAM>(
            text));

    RECT textRect =
        rect;
    textRect.left +=
        Scale(12, dpi);
    textRect.right -=
        Scale(12, dpi);

    SetBkMode(
        item.hDC,
        TRANSPARENT);
    SetTextColor(
        item.hDC,
        disabled
            ? kApplicationPalette.mutedText
            : kApplicationPalette.text);

    HGDIOBJ oldFont =
        SelectObject(
            item.hDC,
            ComboFont(
                item.hwndItem));

    DrawTextW(
        item.hDC,
        text,
        -1,
        &textRect,
        DT_LEFT |
            DT_VCENTER |
            DT_SINGLELINE |
            DT_END_ELLIPSIS |
            DT_NOPREFIX);

    SelectObject(
        item.hDC,
        oldFont);
}

LRESULT ColorNextComboBoxList(
    HDC dc) {

    SetTextColor(
        dc,
        kApplicationPalette.text);
    SetBkColor(
        dc,
        RGB(255, 255, 255));

    return reinterpret_cast<LRESULT>(
        GetStockObject(
            WHITE_BRUSH));
}

} // namespace altrun::ui
