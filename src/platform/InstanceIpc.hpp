#pragma once

#include <windows.h>

namespace altrun::instance_ipc {

inline constexpr wchar_t
    kLauncherWindowClass[] =
        L"ALTRunNext.Launcher";

inline constexpr ULONG_PTR
    kAddShortcutCopyData =
        0xA17A0001;

// Explorer starts the forwarding process with the right to activate a
// window. Transfer that right to the resident process before queuing UI.
// Delivery itself must still work when Windows declines the grant (for
// example, a background script rather than an interactive Explorer action).
[[nodiscard]] inline bool GrantForegroundToWindow(HWND target) noexcept {
    DWORD processId = 0;
    if (!GetWindowThreadProcessId(target, &processId) || !processId) {
        return false;
    }
    return AllowSetForegroundWindow(processId) != FALSE;
}

} // namespace altrun::instance_ipc
