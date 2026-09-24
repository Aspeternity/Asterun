#pragma once

#include <windows.h>

namespace altrun::ui {

[[nodiscard]] HWND CreateNextComboBox(
    HWND parent,
    HINSTANCE instance,
    UINT id,
    COLORREF hostBackground);

void ApplyNextComboBoxMetrics(
    HWND combo,
    UINT dpi);

[[nodiscard]] int
MeasureNextComboBoxPreferredWidth(
    HWND combo,
    UINT dpi,
    int minimumLogical = 132,
    int maximumLogical = 280);

[[nodiscard]] UINT
NextComboBoxItemHeight(
    UINT dpi) noexcept;

void DrawNextComboBoxItem(
    const DRAWITEMSTRUCT& item,
    UINT dpi);

[[nodiscard]] LRESULT
ColorNextComboBoxList(
    HDC dc);

} // namespace altrun::ui
