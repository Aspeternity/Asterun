#pragma once

#include "../ResourceIds.h"

#include <windows.h>

namespace altrun::ui {

[[nodiscard]] inline HICON LoadApplicationIcon(
    HINSTANCE instance,
    bool small = false) {

    const int metricX =
        small ? SM_CXSMICON : SM_CXICON;
    const int metricY =
        small ? SM_CYSMICON : SM_CYICON;

    HICON icon = static_cast<HICON>(
        LoadImageW(
            instance,
            MAKEINTRESOURCEW(
                IDI_ALTRUN_APP),
            IMAGE_ICON,
            GetSystemMetrics(metricX),
            GetSystemMetrics(metricY),
            LR_DEFAULTCOLOR |
                LR_SHARED));

    if (!icon) {
        icon = LoadIconW(
            nullptr,
            IDI_APPLICATION);
    }

    return icon;
}

} // namespace altrun::ui
