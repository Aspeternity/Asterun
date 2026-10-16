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

// Override the closed (selection-field) outer height in physical pixels so
// a combo can sit at the same height as neighbouring text fields. 0 restores
// the shared default. Remembered across ApplyNextComboBoxMetrics calls.
void SetNextComboBoxClosedHeight(
    HWND combo,
    int pixels);

// The override set above, or 0 when the combo uses the shared default.
// The drop-down popup sizes its rows from it so a compact field opens a
// matching compact list.
[[nodiscard]] int
NextComboBoxClosedHeightOverride(
    HWND combo) noexcept;

void MoveNextComboBox(
    HWND combo,
    int x,
    int y,
    int width,
    UINT dpi,
    BOOL repaint = TRUE);

void RefreshNextComboBoxState(
    HWND combo,
    UINT notification);

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
