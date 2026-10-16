#pragma once

#include "UiMetrics.hpp"
#include "UiTheme.hpp"

#include <windows.h>

namespace altrun::ui {

// Native EDIT continues to handle caret, IME, selection, clipboard and UIA.
// Its parent (which has WS_CLIPCHILDREN) draws only the outside frame.
inline constexpr int kNativeEditFrameInsetLogical = 2;
inline constexpr int kNativeEditCornerLogical = 6;

[[nodiscard]] inline RECT NativeEditFrameRect(
    HWND parent, HWND edit, UINT dpi) noexcept {

    RECT bounds{};
    if (!parent || !edit || GetParent(edit) != parent ||
        !GetWindowRect(edit, &bounds)) {
        return {};
    }

    MapWindowPoints(
        nullptr, parent,
        reinterpret_cast<POINT*>(&bounds), 2);
    const int inset = Scale(kNativeEditFrameInsetLogical, dpi);
    InflateRect(&bounds, inset, inset);
    return bounds;
}

inline void InvalidateNativeEditFrame(
    HWND parent, HWND edit, UINT dpi) noexcept {

    const RECT bounds = NativeEditFrameRect(parent, edit, dpi);
    if (!IsRectEmpty(&bounds)) {
        // Never erase or repaint the native EDIT's text/caret on focus.
        InvalidateRect(parent, &bounds, FALSE);
    }
}

inline void DrawNativeEditFrame(
    HDC dc, HWND parent, HWND edit, UINT dpi) noexcept {

    if (!dc || !edit || !IsWindowVisible(edit)) return;
    const RECT bounds = NativeEditFrameRect(parent, edit, dpi);
    if (IsRectEmpty(&bounds)) return;

    const COLORREF border = IsWindowEnabled(edit) && GetFocus() == edit
        ? kApplicationPalette.accent
        : kApplicationPalette.frame;
    HPEN pen = CreatePen(PS_SOLID, 1, border);
    if (!pen) return;

    const HGDIOBJ previousPen = SelectObject(dc, pen);
    const HGDIOBJ previousBrush =
        SelectObject(dc, GetStockObject(NULL_BRUSH));

    // Outline only: do not draw over the text-bearing child rectangle.
    const int radius = Scale(kNativeEditCornerLogical, dpi);
    RoundRect(dc, bounds.left, bounds.top,
        bounds.right, bounds.bottom, radius, radius);

    SelectObject(dc, previousBrush);
    SelectObject(dc, previousPen);
    DeleteObject(pen);
}

} // namespace altrun::ui
