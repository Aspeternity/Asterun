#pragma once

#include <windows.h>

namespace altrun::ui {

[[nodiscard]] bool
ShowNextComboPopup(
    HWND combo,
    UINT dpi);

void HideNextComboPopup(
    HWND combo,
    bool cancel);

void DestroyNextComboPopup(
    HWND combo) noexcept;

[[nodiscard]] bool
IsNextComboPopupVisible(
    HWND combo) noexcept;

[[nodiscard]] bool
HandleNextComboPopupKey(
    HWND combo,
    UINT message,
    WPARAM wParam,
    LPARAM lParam);

[[nodiscard]] bool
HandleNextComboPopupWheel(
    HWND combo,
    UINT message,
    WPARAM wParam,
    LPARAM lParam);

} // namespace altrun::ui
