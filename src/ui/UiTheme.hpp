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
        RGB(255, 255, 255),
        RGB(192, 220, 192),
        RGB(0, 0, 128),
        RGB(128, 128, 128),
        RGB(0, 0, 128),
        RGB(0, 0, 128),
        RGB(0, 120, 215),
        RGB(255, 255, 255),
        RGB(0, 0, 128),
        RGB(91, 97, 104),
        RGB(103, 109, 115),
        RGB(255, 255, 255),
        RGB(91, 97, 104),
        RGB(192, 220, 192),
    };

inline constexpr UiPalette
    kModernCompactLauncherPalette{
        RGB(249, 250, 252),
        RGB(255, 255, 255),
        RGB(255, 255, 255),
        RGB(24, 31, 42),
        RGB(112, 118, 128),
        RGB(0, 120, 212),
        RGB(24, 31, 42),
        RGB(232, 241, 251),
        RGB(24, 31, 42),
        RGB(232, 235, 239),
        RGB(218, 222, 228),
        RGB(249, 250, 252),
        RGB(255, 255, 255),
        RGB(225, 235, 248),
        RGB(249, 250, 252),
    };

[[nodiscard]] constexpr const UiPalette&
LauncherPalette(
    UiStyle style) noexcept {

    return style == UiStyle::ModernCompact
        ? kModernCompactLauncherPalette
        : kClassicLauncherPalette;
}

} // namespace altrun::ui
