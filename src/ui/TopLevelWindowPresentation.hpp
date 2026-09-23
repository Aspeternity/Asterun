#pragma once

#include <windows.h>

namespace altrun::window_presentation {

struct CreationGeometry {
    RECT outer{};
    UINT dpi{96};
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
    HWND hwnd,
    int showCommand) noexcept;

void HideForDestroy(
    HWND hwnd) noexcept;

} // namespace altrun::window_presentation
