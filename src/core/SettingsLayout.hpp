#pragma once

#include "../ui/UiMetrics.hpp"

namespace altrun::settings_layout {

inline constexpr int
    kContentLeftInsetLogical =
        ui::kSettingsContentLeftInsetLogical;
inline constexpr int
    kContentRightInsetLogical =
        ui::kSettingsContentRightInsetLogical;
inline constexpr int
    kToggleRowLogical =
        ui::kSettingsToggleRowLogical;

inline constexpr int
    kPageTitleTopLogical = 22;
inline constexpr int
    kPageDividerTopLogical = 84;
inline constexpr int
    kSectionTitleTopLogical = 108;
inline constexpr int
    kFirstCardTopLogical = 140;
inline constexpr int
    kGeneralCardMaxWidthLogical = 560;

struct Rect {
    int left{};
    int top{};
    int right{};
    int bottom{};
};

struct Point {
    int x{};
    int y{};
};

struct GeneralLayoutMetrics {
    Rect behavior{};
    Rect search{};
    Rect placement{};
    int behaviorTitleTop{};
    int searchTitleTop{};
    int placementTitleTop{};
    int noteTop{};
    int contentBottom{};
};

[[nodiscard]] int Scale(
    int logicalValue,
    unsigned dpi) noexcept;

[[nodiscard]] GeneralLayoutMetrics
BuildGeneralLayout(
    int clientWidth,
    unsigned dpi,
    int scrollOffset,
    int sidebarWidthLogical = ui::kSettingsSidebarWidthLogical) noexcept;

[[nodiscard]] int MaxScrollOffset(
    const GeneralLayoutMetrics& fullLayout,
    int clientHeight,
    unsigned dpi) noexcept;

[[nodiscard]] Rect ClampRectToWorkArea(
    Rect requested,
    Rect workArea) noexcept;

[[nodiscard]] Point ResolveWindowOrigin(
    Rect workArea,
    int requestedWidth,
    int requestedHeight,
    bool nearTop,
    int nearTopOffset) noexcept;

} // namespace altrun::settings_layout
