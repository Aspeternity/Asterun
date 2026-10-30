#pragma once

#include <windows.h>

namespace altrun::ui {

// The dotted focus rectangle is a keyboard cue. Owner-drawn buttons draw it
// only without ODS_NOFOCUSRECT, i.e. while the window's UISF_HIDEFOCUS state
// is clear. Windows clears that state after Tab/Alt, and also when a modal
// dialog or shell picker hands focus back, so a mouse-driven session would
// otherwise keep showing the rectangle. These helpers put the cue back in
// keyboard-only mode; the next Tab/Alt press shows it again.

inline constexpr UINT kHideFocusCuesMessage = WM_APP + 0x3C1;

inline void HideFocusCues(HWND window) noexcept {
    const HWND root = window ? GetAncestor(window, GA_ROOT) : nullptr;
    if (!root) return;
    SendMessageW(root, WM_CHANGEUISTATE,
        MAKEWPARAM(UIS_SET, UISF_HIDEFOCUS), 0);
    // Owner-drawn BUTTONs do not always repaint on WM_UPDATEUISTATE.
    if (HWND focused = GetFocus(); focused && IsChild(root, focused)) {
        InvalidateRect(focused, nullptr, FALSE);
    }
}

// Message-pump hook: hide the cue before a mouse press is dispatched, so the
// clicked button takes focus with ODS_NOFOCUSRECT already set.
inline void HideFocusCuesOnMouseDown(const MSG& msg) noexcept {
    switch (msg.message) {
    case WM_LBUTTONDOWN:
    case WM_LBUTTONDBLCLK:
    case WM_RBUTTONDOWN:
    case WM_RBUTTONDBLCLK:
    case WM_MBUTTONDOWN:
    case WM_MBUTTONDBLCLK:
        HideFocusCues(msg.hwnd);
        break;
    default:
        break;
    }
}

// Window-procedure hook for top-level windows with focusable owner-drawn
// controls. Reactivation (closing a modal dialog, a picker or a message box)
// restores focus with the cue shown; hide it once that sequence has settled.
// Returns true when the message was fully handled.
inline bool HandleFocusCueMessage(
    HWND window, UINT message, WPARAM wParam) noexcept {
    if (!window) return false;
    if (message == WM_ACTIVATE && LOWORD(wParam) != WA_INACTIVE) {
        PostMessageW(window, kHideFocusCuesMessage, 0, 0);
        return false;
    }
    if (message == kHideFocusCuesMessage) {
        HideFocusCues(window);
        return true;
    }
    return false;
}

}  // namespace altrun::ui
