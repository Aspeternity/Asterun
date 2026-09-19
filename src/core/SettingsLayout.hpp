#pragma once

namespace altrun::settings_layout {

struct Rect {
    int left{};
    int top{};
    int right{};
    int bottom{};
};

struct GeneralLayoutMetrics {
    Rect behavior{};
    Rect search{};
    Rect monitor{};
    int behaviorTitleTop{};
    int searchTitleTop{};
    int hotkeySectionTop{};
    int primaryRowTop{};
    int primaryKeyRowTop{};
    int primaryStatusTop{};
    int auxiliaryRowTop{};
    int auxiliaryKeyRowTop{};
    int auxiliaryStatusTop{};
    int popupSectionTop{};
    int noteTop{};
    int contentBottom{};
    bool stackedCards{false};
    bool compactHotkeys{false};
};

[[nodiscard]] int Scale(
    int logicalValue,
    unsigned dpi) noexcept;

[[nodiscard]] GeneralLayoutMetrics
BuildGeneralLayout(
    int clientWidth,
    unsigned dpi,
    int scrollOffset,
    int sidebarWidthLogical = 190) noexcept;

[[nodiscard]] int MaxScrollOffset(
    const GeneralLayoutMetrics& fullLayout,
    int clientHeight,
    unsigned dpi) noexcept;

[[nodiscard]] Rect ClampRectToWorkArea(
    Rect requested,
    Rect workArea) noexcept;

} // namespace altrun::settings_layout
