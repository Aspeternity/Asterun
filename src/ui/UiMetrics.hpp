#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace altrun::ui {

[[nodiscard]] constexpr int Scale(
    int logicalValue,
    unsigned dpi) noexcept {

    if (dpi == 0) {
        dpi = 96;
    }

    const std::int64_t product =
        static_cast<std::int64_t>(
            logicalValue) *
        static_cast<std::int64_t>(dpi);

    if (product >= 0) {
        return static_cast<int>(
            (product + 48) / 96);
    }

    return static_cast<int>(
        (product - 48) / 96);
}

struct LauncherMetrics {
    int widthLogical{};
    int rowHeightLogical{};
    std::size_t maxResults{};
};

inline constexpr LauncherMetrics
    kClassicLauncherMetrics{
        420,
        16,
        10,
    };


struct UiRectMetrics {
    int left{};
    int top{};
    int width{};
    int height{};
};

struct ClassicLauncherDpiMetrics {
    int clientWidth{};
    int clientHeight{};
    UiRectMetrics input{};
    UiRectMetrics results{};
    UiRectMetrics command{};
    int rowHeight{};
    int numberDividerX{};
    int shortcutDividerX{};
    int titleTextLeft{};
    int titleHeight{};
    int dragHeight{};
    int logoLeft{};
    int logoTop{};
    int glyphSize{};
    int closeSize{};
    int closeRightInset{};
    int closeTop{};
    int cornerDiameter{};
};

inline constexpr int
    kClassicSeparatorPhysicalThickness = 1;

inline constexpr std::array<int, 5>
    kClassicGlyphAssetPixelSizes{
        25,
        31,
        38,
        44,
        50,
    };

[[nodiscard]] constexpr std::size_t
ClassicGlyphAssetIndexForTarget(
    int targetPixelSize) noexcept {

    for (std::size_t index = 0;
         index <
         kClassicGlyphAssetPixelSizes.size();
         ++index) {
        if (targetPixelSize <=
            kClassicGlyphAssetPixelSizes[index]) {
            return index;
        }
    }

    return
        kClassicGlyphAssetPixelSizes.size() -
        1;
}

[[nodiscard]] constexpr
ClassicLauncherDpiMetrics
ClassicLauncherMetricsForDpi(
    unsigned dpi) noexcept {

    return {
        Scale(420, dpi),
        Scale(250, dpi),
        {
            Scale(8, dpi),
            Scale(30, dpi),
            Scale(404, dpi),
            Scale(22, dpi),
        },
        {
            Scale(8, dpi),
            Scale(56, dpi),
            Scale(404, dpi),
            Scale(164, dpi),
        },
        {
            Scale(8, dpi),
            Scale(226, dpi),
            Scale(404, dpi),
            Scale(16, dpi),
        },
        Scale(16, dpi),
        Scale(23, dpi),
        Scale(230, dpi),
        Scale(33, dpi),
        Scale(33, dpi),
        Scale(30, dpi),
        Scale(8, dpi),
        Scale(2, dpi),
        Scale(25, dpi),
        Scale(22, dpi),
        Scale(6, dpi),
        Scale(4, dpi),
        Scale(12, dpi),
    };
}

inline constexpr LauncherMetrics
    kModernCompactLauncherMetrics{
        620,
        32,
        9,
    };

inline constexpr int
    kSettingsClientWidthLogical = 820;
inline constexpr int
    kSettingsClientHeightLogical = 620;
inline constexpr int
    kSettingsSidebarWidthLogical = 176;
inline constexpr int
    kSettingsContentLeftInsetLogical = 38;
inline constexpr int
    kSettingsContentRightInsetLogical = 34;
inline constexpr int
    kSettingsToggleRowLogical = 50;

inline constexpr int
    kSettingsComboRowLogical = 54;
inline constexpr int
    kSettingsCardRadiusLogical = 8;
inline constexpr int
    kSettingsNavHeightLogical = 40;
inline constexpr int
    kSettingsNavGapLogical = 4;

inline constexpr int
    kStandardControlHeightLogical = 34;
inline constexpr int
    kCompactControlHeightLogical = 30;
inline constexpr int
    kSmallGapLogical = 8;
inline constexpr int
    kMediumGapLogical = 12;
inline constexpr int
    kLargeGapLogical = 18;

} // namespace altrun::ui
