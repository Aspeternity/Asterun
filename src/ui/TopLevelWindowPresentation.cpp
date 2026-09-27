#include "TopLevelWindowPresentation.hpp"

#include "UiMetrics.hpp"
#include "../core/SettingsLayout.hpp"

#include <dwmapi.h>

#include <algorithm>

namespace altrun::window_presentation {

namespace {

[[nodiscard]] bool IsPlacementOwner(HWND owner) noexcept {
    return owner && IsWindow(owner) &&
        IsWindowVisible(owner) && !IsIconic(owner);
}

[[nodiscard]] HMONITOR ResolveOwnerOrCursorMonitor(
    HWND owner) noexcept {

    if (IsPlacementOwner(owner)) {
        return MonitorFromWindow(
            owner,
            MONITOR_DEFAULTTONEAREST);
    }

    POINT cursor{};
    if (!GetCursorPos(
            &cursor)) {
        cursor = {0, 0};
    }

    return MonitorFromPoint(
        cursor,
        MONITOR_DEFAULTTONEAREST);
}

[[nodiscard]] bool ReadMonitorInfo(
    HMONITOR monitor,
    MONITORINFO& info) noexcept {

    if (!monitor) {
        return false;
    }

    info = {};
    info.cbSize =
        sizeof(info);

    return GetMonitorInfoW(
               monitor,
               &info) != FALSE;
}

[[nodiscard]] settings_layout::Rect WorkRect(
    const MONITORINFO& info) noexcept {

    return {
        static_cast<int>(
            info.rcWork.left),
        static_cast<int>(
            info.rcWork.top),
        static_cast<int>(
            info.rcWork.right),
        static_cast<int>(
            info.rcWork.bottom),
    };
}

[[nodiscard]] settings_layout::Rect
CenteredRect(
    HWND owner,
    const MONITORINFO& info,
    int width,
    int height) noexcept {

    const auto work =
        WorkRect(info);

    settings_layout::Rect requested{};

    RECT ownerRect{};
    if (IsPlacementOwner(owner) &&
        GetWindowRect(
            owner,
            &ownerRect)) {
        const int ownerWidth =
            ownerRect.right -
            ownerRect.left;
        const int ownerHeight =
            ownerRect.bottom -
            ownerRect.top;

        requested.left =
            static_cast<int>(
                ownerRect.left) +
            (ownerWidth - width) / 2;
        requested.top =
            static_cast<int>(
                ownerRect.top) +
            (ownerHeight - height) / 2;
    } else {
        const auto origin =
            settings_layout::
                ResolveWindowOrigin(
                    work,
                    width,
                    height,
                    false,
                    0);

        requested.left =
            origin.x;
        requested.top =
            origin.y;
    }

    requested.right =
        requested.left + width;
    requested.bottom =
        requested.top + height;

    return settings_layout::
        ClampRectToWorkArea(
            requested,
            work);
}

} // namespace

ScopedRedrawSuspend::ScopedRedrawSuspend(HWND hwnd) noexcept {
    if (hwnd && IsWindowVisible(hwnd)) {
        suspended_ = hwnd;
        SendMessageW(hwnd, WM_SETREDRAW, FALSE, 0);
    }
}

ScopedRedrawSuspend::~ScopedRedrawSuspend() {
    Resume();
}

void ScopedRedrawSuspend::Resume() noexcept {
    const HWND hwnd = suspended_;
    suspended_ = nullptr;
    if (hwnd && IsWindow(hwnd)) {
        SendMessageW(hwnd, WM_SETREDRAW, TRUE, 0);
    }
}

bool SetCloaked(
    HWND hwnd,
    bool cloaked) noexcept {

    if (!hwnd) {
        return false;
    }

    const BOOL value =
        cloaked
            ? TRUE
            : FALSE;

    return SUCCEEDED(
        DwmSetWindowAttribute(
            hwnd,
            DWMWA_CLOAK,
            &value,
            sizeof(value)));
}

void Configure(
    HWND hwnd) noexcept {

    if (!hwnd) {
        return;
    }

    const BOOL disableTransitions =
        TRUE;

    DwmSetWindowAttribute(
        hwnd,
        DWMWA_TRANSITIONS_FORCEDISABLED,
        &disableTransitions,
        sizeof(disableTransitions));
}

UINT ProbeMonitorDpi(
    HINSTANCE instance,
    HMONITOR monitor) noexcept {

    MONITORINFO info{};
    if (!ReadMonitorInfo(
            monitor,
            info)) {
        return 96;
    }

    const int monitorWidth =
        static_cast<int>(
            info.rcMonitor.right -
            info.rcMonitor.left);
    const int monitorHeight =
        static_cast<int>(
            info.rcMonitor.bottom -
            info.rcMonitor.top);

    const int x =
        static_cast<int>(
            info.rcMonitor.left) +
        std::max(
            0,
            monitorWidth / 2);
    const int y =
        static_cast<int>(
            info.rcMonitor.top) +
        std::max(
            0,
            monitorHeight / 2);

    HWND probe =
        CreateWindowExW(
            WS_EX_TOOLWINDOW |
                WS_EX_NOACTIVATE,
            L"STATIC",
            L"",
            WS_POPUP,
            x,
            y,
            1,
            1,
            nullptr,
            nullptr,
            instance,
            nullptr);

    if (!probe) {
        return 96;
    }

    const UINT dpi =
        GetDpiForWindow(
            probe);

    DestroyWindow(
        probe);

    return dpi != 0
        ? dpi
        : 96;
}

CreationGeometry ResolveOwnedPopupGeometry(
    HWND owner,
    HINSTANCE instance,
    int widthLogical,
    int heightLogical) noexcept {

    CreationGeometry geometry;

    const HMONITOR monitor =
        ResolveOwnerOrCursorMonitor(
            owner);

    MONITORINFO info{};
    if (!ReadMonitorInfo(
            monitor,
            info)) {
        geometry.outer = {
            0,
            0,
            std::max(
                1,
                widthLogical),
            std::max(
                1,
                heightLogical),
        };
        return geometry;
    }

    geometry.dpi =
        ProbeMonitorDpi(
            instance,
            monitor);

    const int workWidth =
        static_cast<int>(
            info.rcWork.right -
            info.rcWork.left);
    const int workHeight =
        static_cast<int>(
            info.rcWork.bottom -
            info.rcWork.top);

    const int width =
        std::min(
            ui::Scale(
                std::max(
                    1,
                    widthLogical),
                geometry.dpi),
            std::max(
                1,
                workWidth));
    const int height =
        std::min(
            ui::Scale(
                std::max(
                    1,
                    heightLogical),
                geometry.dpi),
            std::max(
                1,
                workHeight));

    const auto resolved =
        CenteredRect(
            owner,
            info,
            width,
            height);

    geometry.outer = {
        resolved.left,
        resolved.top,
        resolved.right,
        resolved.bottom,
    };

    return geometry;
}

void CenterExistingWindow(
    HWND hwnd,
    HWND owner) noexcept {

    if (!hwnd ||
        !IsWindow(hwnd)) {
        return;
    }

    const HMONITOR monitor =
        ResolveOwnerOrCursorMonitor(
            owner);

    MONITORINFO info{};
    if (!ReadMonitorInfo(
            monitor,
            info)) {
        return;
    }

    RECT current{};
    if (!GetWindowRect(
            hwnd,
            &current)) {
        return;
    }

    const auto resolved =
        CenteredRect(
            owner,
            info,
            static_cast<int>(
                current.right -
                current.left),
            static_cast<int>(
                current.bottom -
                current.top));

    SetWindowPos(
        hwnd,
        nullptr,
        resolved.left,
        resolved.top,
        0,
        0,
        SWP_NOSIZE |
            SWP_NOZORDER |
            SWP_NOACTIVATE);
}

void RevealFullyPainted(
    HWND hwnd) noexcept {

    if (!hwnd ||
        !IsWindow(hwnd)) {
        return;
    }

    const bool cloaked =
        SetCloaked(
            hwnd,
            true);

    // Expose the already-positioned window without activating it. Every
    // caller performs the single intended foreground transition only after
    // the cloak is removed. The previous SW_SHOW/SW_SHOWNORMAL path used to
    // activate a still-cloaked HWND and callers then activated it again,
    // producing an avoidable two-step foreground transition on tray opens.
    SetWindowPos(
        hwnd,
        nullptr,
        0,
        0,
        0,
        0,
        SWP_NOMOVE |
            SWP_NOSIZE |
            SWP_NOZORDER |
            SWP_NOACTIVATE |
            SWP_SHOWWINDOW);

    RedrawWindow(
        hwnd,
        nullptr,
        nullptr,
        RDW_INVALIDATE |
            RDW_ERASE |
            RDW_FRAME |
            RDW_ALLCHILDREN |
            RDW_UPDATENOW);

    if (cloaked) {
        DwmFlush();

        (void)SetCloaked(
            hwnd,
            false);

        DwmFlush();
    }
}

void HideForDestroy(
    HWND hwnd) noexcept {

    if (!hwnd ||
        !IsWindow(hwnd)) {
        return;
    }

    const bool cloaked =
        SetCloaked(
            hwnd,
            true);

    if (cloaked) {
        DwmFlush();
    }

    SetWindowPos(
        hwnd,
        nullptr,
        0,
        0,
        0,
        0,
        SWP_NOMOVE |
            SWP_NOSIZE |
            SWP_NOZORDER |
            SWP_NOACTIVATE |
            SWP_HIDEWINDOW);
}

} // namespace altrun::window_presentation
