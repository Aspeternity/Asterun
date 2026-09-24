#pragma once

#include <windows.h>
#include <commctrl.h>

namespace altrun::ui {

void InitializeNextListView(
    HWND list,
    UINT dpi,
    HFONT bodyFont,
    HFONT headerFont);

[[nodiscard]] COLORREF
NextListRowBackground(
    HWND list,
    int itemIndex,
    bool selected);

[[nodiscard]] COLORREF
NextListRowText(
    bool selected) noexcept;

[[nodiscard]] int
NextListCellPadding(
    UINT dpi) noexcept;

void DrawNextListRowSeparator(
    HDC dc,
    const RECT& row);

} // namespace altrun::ui
