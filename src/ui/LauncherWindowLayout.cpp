#include "LauncherWindow.hpp"
#include "../app/App.hpp"
#include "UiMetrics.hpp"

#include <uxtheme.h>
#include <algorithm>

namespace altrun {

void LauncherWindow::UpdateControlFrames() {
    if (!edit_ ||
        !list_ ||
        !preview_ ||
        !classicPreview_) {
        return;
    }

    auto setFrame = [](HWND control, LONG_PTR edge) {
        LONG_PTR style =
            GetWindowLongPtrW(
                control,
                GWL_EXSTYLE);
        style &=
            ~(WS_EX_CLIENTEDGE |
              WS_EX_STATICEDGE);
        style |= edge;
        SetWindowLongPtrW(
            control,
            GWL_EXSTYLE,
            style);
        SetWindowPos(
            control,
            nullptr,
            0,
            0,
            0,
            0,
            SWP_NOMOVE |
                SWP_NOSIZE |
                SWP_NOZORDER |
                SWP_NOACTIVATE |
                SWP_FRAMECHANGED);
    };

    if (IsModern()) {
        // Modern Compact owns its visual surfaces in the parent paint pass.
        // Keep native EDIT/LISTBOX behavior, but remove legacy 3-D edges so
        // USER32 chrome cannot diverge from the DPI metrics contract.
        setFrame(
            edit_,
            0);
        setFrame(
            list_,
            0);
        setFrame(
            preview_,
            0);
        setFrame(
            classicPreview_,
            0);

        SetWindowTheme(
            edit_,
            L"Explorer",
            nullptr);
        SetWindowTheme(
            list_,
            L"Explorer",
            nullptr);
        SetWindowTheme(
            preview_,
            L"Explorer",
            nullptr);
        return;
    }

    setFrame(edit_, 0);
    setFrame(list_, 0);
    setFrame(preview_, 0);
    setFrame(classicPreview_, 0);

    SetWindowTheme(edit_, L"", L"");
    SetWindowTheme(list_, L"", L"");
    SetWindowTheme(
        classicPreview_,
        L"",
        L"");
}

int LauncherWindow::DpiScale(int value) const {
    return ui::Scale(
        value,
        dpi_);
}

void LauncherWindow::Layout() {
    if (!hwnd_) return;

    int width = 0;
    int height = 0;

    if (IsModern()) {
        modernDpiMetrics_ =
            ui::ModernCompactLauncherMetricsForDpi(
                dpi_,
                modernLayoutRows_);
        const auto& modern =
            modernDpiMetrics_;

        width =
            modern.clientWidth;
        height =
            modern.clientHeight;

        SetWindowPos(
            hwnd_,
            nullptr,
            0,
            0,
            width,
            height,
            SWP_NOMOVE |
                SWP_NOZORDER |
                SWP_NOACTIVATE);

        MoveWindow(
            edit_,
            modern.searchEdit.left,
            modern.searchEdit.top,
            modern.searchEdit.width,
            modern.searchEdit.height,
            TRUE);

        if (modernLayoutRows_ > 0) {
            ShowWindow(
                list_,
                SW_SHOWNA);
            ShowWindow(
                preview_,
                SW_SHOWNA);

            MoveWindow(
                list_,
                modern.resultsList.left,
                modern.resultsList.top,
                modern.resultsList.width,
                modern.resultsList.height,
                TRUE);
            MoveWindow(
                preview_,
                modern.footer.left,
                modern.footer.top,
                modern.footer.width,
                modern.footer.height,
                TRUE);
        } else {
            ShowWindow(
                list_,
                SW_HIDE);
            ShowWindow(
                preview_,
                SW_HIDE);
            MoveWindow(
                list_,
                0,
                0,
                0,
                0,
                FALSE);
            MoveWindow(
                preview_,
                0,
                0,
                0,
                0,
                FALSE);
        }

        MoveWindow(
            classicPreview_,
            0,
            0,
            0,
            0,
            FALSE);
        return;
    }

    const auto& classicMetrics =
        classicDpiMetrics_;

    width =
        classicMetrics.clientWidth;
    height =
        classicMetrics.clientHeight;

    SetWindowPos(
        hwnd_,
        nullptr,
        0,
        0,
        width,
        height,
        SWP_NOMOVE |
            SWP_NOZORDER |
            SWP_NOACTIVATE);

    MoveWindow(
        edit_,
        classicMetrics.input.left,
        classicMetrics.input.top,
        classicMetrics.input.width,
        classicMetrics.input.height,
        TRUE);

    MoveWindow(
        list_,
        classicMetrics.results.left,
        classicMetrics.results.top,
        classicMetrics.results.width,
        classicMetrics.results.height,
        TRUE);

    MoveWindow(
        preview_,
        0,
        0,
        0,
        0,
        FALSE);

    MoveWindow(
        classicPreview_,
        classicMetrics.command.left,
        classicMetrics.command.top,
        classicMetrics.command.width,
        classicMetrics.command.height,
        TRUE);
}

void LauncherWindow::Reposition() {
    const auto& settings =
        app_.SettingsData();

    RECT rect{};
    GetWindowRect(hwnd_, &rect);

    const int width =
        rect.right - rect.left;
    const int height =
        rect.bottom - rect.top;

    if (settings.launcherPlacement == "last" &&
        settings.launcherLastPositionValid) {

        RECT requested{
            settings.launcherLastX,
            settings.launcherLastY,
            settings.launcherLastX + width,
            settings.launcherLastY + height,
        };

        HMONITOR monitor =
            MonitorFromRect(
                &requested,
                MONITOR_DEFAULTTONEAREST);

        MONITORINFO info{
            sizeof(info)};
        GetMonitorInfoW(
            monitor,
            &info);

        const int workWidth =
            info.rcWork.right -
            info.rcWork.left;
        const int workHeight =
            info.rcWork.bottom -
            info.rcWork.top;

        const int x =
            std::clamp(
                requested.left,
                info.rcWork.left,
                info.rcWork.right -
                    std::min(
                        width,
                        workWidth));

        const int y =
            std::clamp(
                requested.top,
                info.rcWork.top,
                info.rcWork.bottom -
                    std::min(
                        height,
                        workHeight));

        SetWindowPos(
            hwnd_,
            HWND_TOPMOST,
            x,
            y,
            width,
            height,
            SWP_NOACTIVATE);
        return;
    }

    HMONITOR monitor = nullptr;

    if (settings.popupMonitor == "primary") {
        POINT origin{0, 0};
        monitor =
            MonitorFromPoint(
                origin,
                MONITOR_DEFAULTTOPRIMARY);
    } else if (
        settings.popupMonitor == "active") {
        HWND foreground =
            GetForegroundWindow();
        monitor =
            MonitorFromWindow(
                foreground,
                MONITOR_DEFAULTTONEAREST);
    } else {
        POINT cursor{};
        GetCursorPos(&cursor);
        monitor =
            MonitorFromPoint(
                cursor,
                MONITOR_DEFAULTTONEAREST);
    }

    MONITORINFO info{
        sizeof(info)};
    GetMonitorInfoW(
        monitor,
        &info);

    const int workWidth =
        info.rcWork.right -
        info.rcWork.left;
    const int workHeight =
        info.rcWork.bottom -
        info.rcWork.top;

    const int x =
        info.rcWork.left +
        (workWidth - width) / 2;

    const int y =
        settings.launcherPlacement ==
                "center"
            ? info.rcWork.top +
                (workHeight - height) / 2
            : info.rcWork.top +
                std::max(
                    DpiScale(45),
                    (workHeight -
                     height) / 5);

    SetWindowPos(
        hwnd_,
        HWND_TOPMOST,
        x,
        y,
        width,
        height,
        SWP_NOACTIVATE);
}

} // namespace altrun
