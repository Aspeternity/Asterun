#pragma once

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

inline constexpr LauncherMetrics
    kModernCompactLauncherMetrics{
        620,
        32,
        9,
    };

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
