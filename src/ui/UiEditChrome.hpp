#pragma once

#include "UiMetrics.hpp"
#include "UiTheme.hpp"

#include <commctrl.h>
#include <windows.h>

#include <algorithm>

namespace altrun::ui {

// Keep EDIT's native text/caret/IME code. Reserve its own non-client frame;
// unlike a parent-painted outline, this cannot be hidden by sibling labels
// or exposed between independent parent and child paint passes.
inline constexpr UINT_PTR kNativeEditChromeSubclassId = 0xED17;

[[nodiscard]] inline UINT NativeEditChromeDpi(
    HWND edit, DWORD_PTR context) noexcept {
    const auto* dpi = reinterpret_cast<const UINT*>(context);
    if (dpi && *dpi) return *dpi;
    const UINT windowDpi = GetDpiForWindow(edit);
    return windowDpi ? windowDpi : 96;
}

[[nodiscard]] inline int NativeEditFontHeight(HWND edit) noexcept {
    const HDC dc = GetDC(edit);
    if (!dc) return 0;
    const auto font = reinterpret_cast<HFONT>(
        SendMessageW(edit, WM_GETFONT, 0, 0));
    const HGDIOBJ previous = font ? SelectObject(dc, font) : nullptr;
    TEXTMETRICW metrics{};
    const bool measured = GetTextMetricsW(dc, &metrics) != FALSE;
    if (previous && previous != HGDI_ERROR) SelectObject(dc, previous);
    ReleaseDC(edit, dc);
    return measured ? static_cast<int>(metrics.tmHeight) : 0;
}

inline void PaintNativeEditChrome(HWND edit, UINT dpi) noexcept {
    RECT window{}, client{};
    if (!GetWindowRect(edit, &window) ||
        !GetClientRect(edit, &client)) return;
    const int width = window.right - window.left;
    const int height = window.bottom - window.top;
    if (width <= 0 || height <= 0) return;

    POINT origin{};
    if (!ClientToScreen(edit, &origin)) return;
    const RECT inside{
        origin.x - window.left,
        origin.y - window.top,
        origin.x - window.left + client.right,
        origin.y - window.top + client.bottom
    };

    const HDC dc = GetWindowDC(edit);
    if (!dc) return;
    HBRUSH background = CreateSolidBrush(kApplicationPalette.controlBackground);
    if (background) {
        const RECT strips[]{
            {0, 0, width, inside.top},
            {0, inside.bottom, width, height},
            {0, inside.top, inside.left, inside.bottom},
            {inside.right, inside.top, width, inside.bottom}
        };
        for (const RECT& strip : strips)
            if (strip.right > strip.left && strip.bottom > strip.top)
                FillRect(dc, &strip, background);
        DeleteObject(background);
    }

    const COLORREF color = IsWindowEnabled(edit) && GetFocus() == edit
        ? kApplicationPalette.accent : kApplicationPalette.frame;
    const HPEN pen = CreatePen(PS_SOLID, 1, color);
    if (pen) {
        const HGDIOBJ oldPen = SelectObject(dc, pen);
        const HGDIOBJ oldBrush = SelectObject(dc, GetStockObject(NULL_BRUSH));
        const int radius = Scale(6, dpi);
        RoundRect(dc, 1, 1, width - 1, height - 1, radius, radius);
        SelectObject(dc, oldBrush);
        SelectObject(dc, oldPen);
        DeleteObject(pen);
    }
    ReleaseDC(edit, dc);
}

inline LRESULT CALLBACK NativeEditChromeSubclass(
    HWND edit, UINT message, WPARAM wParam, LPARAM lParam,
    UINT_PTR id, DWORD_PTR context) {
    switch (message) {
    case WM_NCCALCSIZE: {
        const LRESULT result = DefSubclassProc(edit, message, wParam, lParam);
        if (!lParam) return result;
        auto* rect = wParam
            ? &reinterpret_cast<NCCALCSIZE_PARAMS*>(lParam)->rgrc[0]
            : reinterpret_cast<RECT*>(lParam);
        if (!rect) return result;

        // Single-line EDIT does not implement EM_SETRECT. Center its native
        // client/caret area instead; do not draw or offset the text ourselves.
        const int fontHeight = NativeEditFontHeight(edit);
        const int available = rect->bottom - rect->top;
        if (fontHeight > 0 && available > fontHeight) {
            const int extra = available - fontHeight;
            rect->top += extra / 2;
            rect->bottom -= extra - extra / 2;
        }
        // RECT stores LONG coordinates. Keep both arithmetic branches int
        // so MSVC can deduce the std::min/std::max template arguments.
        const int clientWidth = static_cast<int>(
            rect->right - rect->left);
        const int side = std::min(
            Scale(4, NativeEditChromeDpi(edit, context)),
            std::max(0, (clientWidth -
                Scale(32, NativeEditChromeDpi(edit, context))) / 2));
        rect->left += side;
        rect->right -= side;
        return result;
    }
    case WM_NCPAINT:
        PaintNativeEditChrome(edit, NativeEditChromeDpi(edit, context));
        return 0;
    case WM_SETFOCUS:
    case WM_KILLFOCUS:
    case WM_ENABLE: {
        const LRESULT result = DefSubclassProc(edit, message, wParam, lParam);
        // Only this EDIT's non-client area changes. Never invalidate parent.
        RedrawWindow(edit, nullptr, nullptr,
            RDW_INVALIDATE | RDW_FRAME | RDW_NOERASE);
        return result;
    }
    case WM_NCHITTEST: {
        const LRESULT result = DefSubclassProc(edit, message, wParam, lParam);
        // The padded area is still part of the clickable EDIT.
        return result == HTBORDER ? HTCLIENT : result;
    }
    case WM_NCDESTROY:
        RemoveWindowSubclass(edit, NativeEditChromeSubclass, id);
        break;
    }
    return DefSubclassProc(edit, message, wParam, lParam);
}

[[nodiscard]] inline bool AttachNativeEditChrome(
    HWND edit, const UINT* dpi) noexcept {
    if (!edit || !dpi || !SetWindowSubclass(
            edit, NativeEditChromeSubclass, kNativeEditChromeSubclassId,
            reinterpret_cast<DWORD_PTR>(dpi))) {
        return false;
    }

    // Borderless EDIT starts with a full-window client. Recalculate its
    // real non-client region after installing our subclass, before layout
    // and before the parent is revealed. No intermediate paint is needed.
    return SetWindowPos(edit, nullptr, 0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE |
        SWP_FRAMECHANGED | SWP_NOREDRAW) != FALSE;
}

} // namespace altrun::ui
