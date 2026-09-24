#pragma once

#include <array>

#include <windows.h>
#include <commctrl.h>

namespace altrun::ui {

struct NextListColumnResizePolicy {
    int resizableColumnCount{0};
    int elasticColumn{-1};
    std::array<int, 4>
        minimumLogicalWidths{};
};

void InitializeNextListView(
    HWND list,
    UINT dpi,
    HFONT bodyFont,
    HFONT headerFont);

void ConfigureNextListColumnResize(
    HWND list,
    const NextListColumnResizePolicy&
        policy);

[[nodiscard]] bool
NextListHasUserAdjustedColumns(
    HWND list) noexcept;

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
