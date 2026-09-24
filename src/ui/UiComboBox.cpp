#include "UiComboBox.hpp"

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

void DrawNextComboBoxSurface(
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
    const bool active =
        GetFocus() == combo ||
        SendMessageW(
            combo,
            CB_GETDROPPEDSTATE,
            0,
            0) != 0;

    POINT cursor{};
    RECT screenRect{};
    const bool hovered =
        GetCursorPos(&cursor) &&
        GetWindowRect(
            combo,
            &screenRect) &&
        PtInRect(
            &screenRect,
            cursor);

    const COLORREF borderColor =
        active
            ? kApplicationPalette.accent
            : kApplicationPalette.frame;
    const COLORREF fillColor =
        active
            ? RGB(248, 252, 255)
            : hovered
                ? RGB(250, 251, 253)
                : RGB(255, 255, 255);

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

    const int arrowCenterX =
        surface.right -
        Scale(17, dpi);
    const int arrowCenterY =
        surface.top +
        (surface.bottom -
         surface.top) / 2;

    const COLORREF arrowColor =
        enabled
            ? RGB(92, 100, 108)
            : RGB(166, 172, 179);

    HPEN arrowPen =
        CreatePen(
            PS_SOLID,
            std::max(
                1,
                Scale(1, dpi)),
            arrowColor);
    oldPen =
        SelectObject(
            dc,
            arrowPen);

    MoveToEx(
        dc,
        arrowCenterX -
            Scale(4, dpi),
        arrowCenterY -
            Scale(2, dpi),
        nullptr);
    LineTo(
        dc,
        arrowCenterX,
        arrowCenterY +
            Scale(2, dpi));
    LineTo(
        dc,
        arrowCenterX +
            Scale(4, dpi),
        arrowCenterY -
            Scale(2, dpi));

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
        arrowCenterX -
            Scale(12, dpi),
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

        InvalidateRect(
            hwnd,
            nullptr,
            FALSE);
        return result;
    }

    case WM_MOUSELEAVE:
        InvalidateRect(
            hwnd,
            nullptr,
            FALSE);
        return 0;

    case CB_SETCURSEL:
    case CB_SHOWDROPDOWN:
    case WM_SETFONT:
    case WM_SETFOCUS:
    case WM_KILLFOCUS:
    case WM_ENABLE:
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP: {
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
                CBS_HASSTRINGS |
                CBS_NOINTEGRALHEIGHT |
                WS_VSCROLL,
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

    const int chrome =
        Scale(52, dpi);

    return std::clamp(
        widest + chrome,
        minimum,
        maximum);
}

UINT NextComboBoxItemHeight(
    UINT dpi) noexcept {

    return static_cast<UINT>(
        Scale(30, dpi));
}

void DrawNextComboBoxItem(
    const DRAWITEMSTRUCT& item,
    UINT dpi) {

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
