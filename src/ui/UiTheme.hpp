#pragma once

#include "../core/Settings.hpp"

#include <windows.h>

namespace altrun::ui {

struct UiPalette {
    COLORREF windowBackground{};
    COLORREF controlBackground{};
    COLORREF accentBackground{};
    COLORREF text{};
    COLORREF mutedText{};
    COLORREF accent{};
    COLORREF keyword{};
    COLORREF selectionBackground{};
    COLORREF selectionText{};
    COLORREF separator{};
    COLORREF frame{};
    COLORREF sidebarBackground{};
    COLORREF cardBackground{};
    COLORREF pressedBackground{};
    COLORREF bottomBackground{};
};

inline constexpr UiPalette
    kApplicationPalette{
        RGB(255, 255, 255),
        RGB(255, 255, 255),
        RGB(249, 250, 252),
        RGB(31, 41, 55),
        RGB(100, 107, 116),
        RGB(0, 120, 212),
        RGB(0, 120, 212),
        RGB(232, 241, 250),
        RGB(31, 41, 55),
        RGB(225, 229, 235),
        RGB(225, 229, 235),
        RGB(246, 247, 249),
        RGB(249, 250, 252),
        RGB(243, 246, 249),
        RGB(249, 250, 252),
    };

inline constexpr UiPalette
    kClassicLauncherPalette{
        RGB(103, 109, 115),
        RGB(244, 245, 247),
        RGB(186, 214, 190),
        RGB(38, 41, 145),
        RGB(104, 119, 109),
        RGB(38, 41, 145),
        RGB(38, 41, 145),
        RGB(4, 119, 210),
        RGB(255, 255, 255),
        RGB(43, 45, 148),
        RGB(91, 97, 104),
        RGB(103, 109, 115),
        RGB(244, 245, 247),
        RGB(91, 97, 104),
        RGB(181, 208, 184),
    };

inline constexpr UiPalette
    kModernCompactLauncherPalette{
        RGB(246, 247, 249),
        RGB(255, 255, 255),
        RGB(255, 255, 255),
        RGB(31, 41, 55),
        RGB(107, 114, 128),
        RGB(37, 99, 235),
        RGB(37, 99, 235),
        RGB(229, 239, 255),
        RGB(15, 23, 42),
        RGB(229, 231, 235),
        RGB(205, 210, 218),
        RGB(246, 247, 249),
        RGB(255, 255, 255),
        RGB(229, 231, 235),
        RGB(246, 247, 249),
    };

[[nodiscard]] constexpr const UiPalette&
LauncherPalette(
    UiStyle style) noexcept {

    return style == UiStyle::ModernCompact
        ? kModernCompactLauncherPalette
        : kClassicLauncherPalette;
}

} // namespace altrun::ui
