#pragma once

#include <algorithm>
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
        10,
    };

struct ModernCompactLauncherDpiMetrics {
    int clientWidth{};
    int clientHeight{};
    UiRectMetrics searchSurface{};
    UiRectMetrics searchGlyph{};
    UiRectMetrics searchEdit{};
    UiRectMetrics resultsSurface{};
    UiRectMetrics resultsList{};
    UiRectMetrics footerSurface{};
    UiRectMetrics footer{};
    UiRectMetrics footerAction{};
    int rowHeight{};
    int rowSelectionInsetX{};
    int rowSelectionInsetY{};
    int rowTextInset{};
    int secondaryMinWidth{};
    int rowColumnGap{};
    int shortcutHintWidth{};
    int shortcutHintGap{};
    int rowCornerDiameter{};
    int selectionAccentWidth{};
    int selectionAccentInset{};
    int searchCornerDiameter{};
    int resultsCornerDiameter{};
    int footerCornerDiameter{};
};

[[nodiscard]] constexpr
ModernCompactLauncherDpiMetrics
ModernCompactLauncherMetricsForDpi(
    unsigned dpi,
    std::size_t visibleRows =
        kModernCompactLauncherMetrics
            .maxResults) noexcept {

    const std::size_t rows =
        visibleRows >
                kModernCompactLauncherMetrics
                    .maxResults
            ? kModernCompactLauncherMetrics
                  .maxResults
            : visibleRows;

    const int clientWidth =
        Scale(620, dpi);
    const int rowHeight =
        Scale(32, dpi);

    const UiRectMetrics searchSurface{
        Scale(12, dpi),
        Scale(12, dpi),
        Scale(596, dpi),
        Scale(38, dpi),
    };

    const int searchGlyphSize =
        Scale(16, dpi);
    const UiRectMetrics searchGlyph{
        Scale(25, dpi),
        searchSurface.top +
            (searchSurface.height -
             searchGlyphSize) /
                2,
        searchGlyphSize,
        searchGlyphSize,
    };

    // A native single-line EDIT does not expose vertical text alignment.
    // Keep its child HWND close to the font line height and center the HWND
    // inside the search surface instead of making a 32px-tall top-aligned
    // edit client.
    const int searchEditHeight =
        Scale(20, dpi);
    const UiRectMetrics searchEdit{
        Scale(48, dpi),
        searchSurface.top +
            (searchSurface.height -
             searchEditHeight) /
                2,
        Scale(548, dpi),
        searchEditHeight,
    };

    if (rows == 0) {
        return {
            clientWidth,
            searchSurface.top +
                searchSurface.height +
                Scale(12, dpi),
            searchSurface,
            searchGlyph,
            searchEdit,
            {},
            {},
            {},
            {},
            {},
            rowHeight,
            Scale(4, dpi),
            Scale(2, dpi),
            Scale(14, dpi),
            Scale(150, dpi),
            Scale(12, dpi),
            Scale(34, dpi),
            Scale(12, dpi),
            Scale(10, dpi),
            Scale(3, dpi),
            Scale(8, dpi),
            Scale(10, dpi),
            Scale(12, dpi),
            Scale(10, dpi),
        };
    }

    const int clientHeight =
        Scale(428, dpi) -
        static_cast<int>(
            kModernCompactLauncherMetrics
                .maxResults -
            rows) *
            rowHeight;

    const UiRectMetrics footerSurface{
        Scale(12, dpi),
        clientHeight -
            Scale(40, dpi),
        Scale(596, dpi),
        Scale(28, dpi),
    };

    const UiRectMetrics footerAction{
        footerSurface.left +
            footerSurface.width -
            Scale(62, dpi),
        footerSurface.top +
            Scale(4, dpi),
        Scale(52, dpi),
        Scale(20, dpi),
    };

    const UiRectMetrics footer{
        footerSurface.left +
            Scale(12, dpi),
        footerSurface.top +
            Scale(2, dpi),
        footerAction.left -
            Scale(8, dpi) -
            (footerSurface.left +
             Scale(12, dpi)),
        Scale(24, dpi),
    };

    const int resultsTop =
        Scale(58, dpi);
    const int resultsBottom =
        footerSurface.top -
        Scale(6, dpi);

    const UiRectMetrics resultsSurface{
        Scale(12, dpi),
        resultsTop,
        Scale(596, dpi),
        resultsBottom -
            resultsTop,
    };

    const int verticalInset =
        std::max(
            0,
            (resultsSurface.height -
             static_cast<int>(rows) *
                 rowHeight) /
                2);

    const UiRectMetrics resultsList{
        resultsSurface.left +
            Scale(4, dpi),
        resultsSurface.top +
            verticalInset,
        resultsSurface.width -
            Scale(8, dpi),
        static_cast<int>(rows) *
            rowHeight,
    };

    return {
        clientWidth,
        clientHeight,
        searchSurface,
        searchGlyph,
        searchEdit,
        resultsSurface,
        resultsList,
        footerSurface,
        footer,
        footerAction,
        rowHeight,
        Scale(4, dpi),
        Scale(2, dpi),
        Scale(14, dpi),
        Scale(150, dpi),
        Scale(12, dpi),
        Scale(34, dpi),
        Scale(12, dpi),
        Scale(10, dpi),
        Scale(3, dpi),
        Scale(8, dpi),
        Scale(10, dpi),
        Scale(12, dpi),
        Scale(10, dpi),
    };
}

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

// Shared Next ComboBox geometry. Keep every Asterun dropdown on one sizing
// contract so short lists never grow a pointless scrollbar and future
// consumers do not invent window-specific dropdown heights.
inline constexpr int
    kNextComboBoxItemHeightLogical = 30;
inline constexpr int
    kNextComboBoxChromeHeightLogical = 14;
inline constexpr int
    kNextComboPopupGapLogical = 4;
inline constexpr int
    kNextComboPopupPaddingLogical = 4;
inline constexpr int
    kNextComboPopupRadiusLogical = 10;
inline constexpr int
    kNextComboPopupRowHeightLogical = 36;
inline constexpr int
    kNextComboPopupRowRadiusLogical = 6;
inline constexpr int
    kNextComboPopupTextInsetLogical = 16;
inline constexpr int
    kNextComboPopupAccentWidthLogical = 3;
inline constexpr int
    kNextComboPopupAccentHeightLogical = 16;
inline constexpr int
    kNextComboPopupScrollbarWidthLogical = 4;
inline constexpr int
    kNextComboPopupScrollbarInsetLogical = 4;
inline constexpr std::size_t
    kNextComboBoxMaxVisibleItems = 6;

[[nodiscard]] constexpr std::size_t
NextComboBoxVisibleItems(
    std::size_t itemCount,
    std::size_t maxVisibleItems =
        kNextComboBoxMaxVisibleItems) noexcept {

    return maxVisibleItems == 0
        ? 0
        : std::min(
              itemCount,
              maxVisibleItems);
}

[[nodiscard]] constexpr int
NextComboBoxDropHeightForDpi(
    std::size_t itemCount,
    unsigned dpi,
    std::size_t maxVisibleItems =
        kNextComboBoxMaxVisibleItems) noexcept {

    const auto visibleItems =
        NextComboBoxVisibleItems(
            itemCount,
            maxVisibleItems);

    return
        Scale(
            kNextComboBoxItemHeightLogical,
            dpi) *
            (1 +
             static_cast<int>(
                 visibleItems)) +
        Scale(
            kNextComboBoxChromeHeightLogical,
            dpi);
}

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
