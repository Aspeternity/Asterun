#pragma once

#include <windows.h>

namespace altrun::window_presentation {

struct CreationGeometry {
    RECT outer{};
    UINT dpi{96};
};

// DefWindowProc's WM_SETREDRAW(TRUE) adds WS_VISIBLE. Never send it to
// an initially hidden HWND. Nested guards do not resume an outer guard.
class ScopedRedrawSuspend {
public:
    explicit ScopedRedrawSuspend(HWND hwnd) noexcept;
    ~ScopedRedrawSuspend();
    ScopedRedrawSuspend(const ScopedRedrawSuspend&) = delete;
    ScopedRedrawSuspend& operator=(const ScopedRedrawSuspend&) = delete;
    void Resume() noexcept;
private:
    HWND suspended_{};
};

[[nodiscard]] bool SetCloaked(
    HWND hwnd,
    bool cloaked) noexcept;

void Configure(
    HWND hwnd) noexcept;

[[nodiscard]] UINT ProbeMonitorDpi(
    HINSTANCE instance,
    HMONITOR monitor) noexcept;

[[nodiscard]] CreationGeometry
ResolveOwnedPopupGeometry(
    HWND owner,
    HINSTANCE instance,
    int widthLogical,
    int heightLogical) noexcept;

void CenterExistingWindow(
    HWND hwnd,
    HWND owner) noexcept;

void RevealFullyPainted(
    HWND hwnd) noexcept;

void HideForDestroy(
    HWND hwnd) noexcept;

} // namespace altrun::window_presentation
