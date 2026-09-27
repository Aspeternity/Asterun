#include "ui/UiMetrics.hpp"

#include <array>
#include <cassert>
#include <iostream>

#ifdef NDEBUG
#error "ui_foundation_tests requires assert() in Release builds"
#endif

using namespace altrun;

int main() {
    assert(ui::kClassicLauncherMetrics.widthLogical == 420);
    assert(ui::kClassicLauncherMetrics.rowHeightLogical == 16);
    assert(ui::kClassicLauncherMetrics.maxResults == 10);

    assert(ui::kModernCompactLauncherMetrics.widthLogical == 620);
    assert(ui::kModernCompactLauncherMetrics.rowHeightLogical == 32);
    assert(ui::kModernCompactLauncherMetrics.maxResults == 9);

    struct ModernDpiExpectation {
        unsigned dpi;
        int clientWidth;
        int clientHeight;
        ui::UiRectMetrics searchSurface;
        ui::UiRectMetrics searchEdit;
        ui::UiRectMetrics resultsSurface;
        ui::UiRectMetrics resultsList;
        ui::UiRectMetrics footer;
        int rowHeight;
        int rowSelectionInsetX;
        int rowSelectionInsetY;
        int rowTextInset;
        int secondaryColumnWidth;
        int rowColumnGap;
        int rowCornerDiameter;
        int selectionAccentWidth;
        int selectionAccentInset;
        int searchCornerDiameter;
    };

    constexpr std::array<
        ModernDpiExpectation,
        5>
        modernDpiExpectations{{
            {
                96u,
                620,
                400,
                {12, 12, 596, 42},
                {24, 17, 572, 32},
                {12, 62, 596, 292},
                {12, 62, 596, 292},
                {18, 362, 584, 24},
                32, 4, 2, 14, 200, 12, 10, 3, 8, 10,
            },
            {
                120u,
                775,
                500,
                {15, 15, 745, 53},
                {30, 21, 715, 40},
                {15, 78, 745, 365},
                {15, 78, 745, 365},
                {23, 453, 730, 30},
                40, 5, 3, 18, 250, 15, 13, 4, 10, 13,
            },
            {
                144u,
                930,
                600,
                {18, 18, 894, 63},
                {36, 26, 858, 48},
                {18, 93, 894, 438},
                {18, 93, 894, 438},
                {27, 543, 876, 36},
                48, 6, 3, 21, 300, 18, 15, 5, 12, 15,
            },
            {
                168u,
                1085,
                700,
                {21, 21, 1043, 74},
                {42, 30, 1001, 56},
                {21, 109, 1043, 511},
                {21, 109, 1043, 511},
                {32, 634, 1022, 42},
                56, 7, 4, 25, 350, 21, 18, 5, 14, 18,
            },
            {
                192u,
                1240,
                800,
                {24, 24, 1192, 84},
                {48, 34, 1144, 64},
                {24, 124, 1192, 584},
                {24, 124, 1192, 584},
                {36, 724, 1168, 48},
                64, 8, 4, 28, 400, 24, 20, 6, 16, 20,
            },
        }};

    const auto assertModernRect =
        [](const ui::UiRectMetrics& actual,
           const ui::UiRectMetrics& expected) {
            assert(actual.left == expected.left);
            assert(actual.top == expected.top);
            assert(actual.width == expected.width);
            assert(actual.height == expected.height);
        };

    for (const auto& expected :
         modernDpiExpectations) {
        const auto actual =
            ui::ModernCompactLauncherMetricsForDpi(
                expected.dpi);
        assert(actual.clientWidth ==
               expected.clientWidth);
        assert(actual.clientHeight ==
               expected.clientHeight);
        assertModernRect(
            actual.searchSurface,
            expected.searchSurface);
        assertModernRect(
            actual.searchEdit,
            expected.searchEdit);
        assertModernRect(
            actual.resultsSurface,
            expected.resultsSurface);
        assertModernRect(
            actual.resultsList,
            expected.resultsList);
        assertModernRect(
            actual.footer,
            expected.footer);
        assert(actual.rowHeight ==
               expected.rowHeight);
        assert(actual.rowSelectionInsetX ==
               expected.rowSelectionInsetX);
        assert(actual.rowSelectionInsetY ==
               expected.rowSelectionInsetY);
        assert(actual.rowTextInset ==
               expected.rowTextInset);
        assert(actual.secondaryColumnWidth ==
               expected.secondaryColumnWidth);
        assert(actual.rowColumnGap ==
               expected.rowColumnGap);
        assert(actual.rowCornerDiameter ==
               expected.rowCornerDiameter);
        assert(actual.selectionAccentWidth ==
               expected.selectionAccentWidth);
        assert(actual.selectionAccentInset ==
               expected.selectionAccentInset);
        assert(actual.searchCornerDiameter ==
               expected.searchCornerDiameter);
        assert(
            actual.resultsList.height >=
            actual.rowHeight *
                static_cast<int>(
                    ui::kModernCompactLauncherMetrics
                        .maxResults));
        assert(
            actual.footer.top >
            actual.resultsSurface.top +
                actual.resultsSurface.height);
        assert(
            actual.footer.top +
                actual.footer.height <
            actual.clientHeight);
    }

    assert(ui::kSettingsClientWidthLogical == 820);
    assert(ui::kSettingsClientHeightLogical == 620);
    assert(ui::kSettingsSidebarWidthLogical == 176);
    assert(ui::kSettingsContentLeftInsetLogical == 38);
    assert(ui::kSettingsContentRightInsetLogical == 34);
    assert(ui::kSettingsToggleRowLogical == 50);

    for (const auto [dpi, expected] :
         std::array<std::pair<unsigned, int>, 5>{
             std::pair{96u, 50},
             std::pair{120u, 63},
             std::pair{144u, 75},
             std::pair{168u, 88},
             std::pair{192u, 100},
         }) {
        assert(ui::Scale(50, dpi) == expected);
    }

    assert(ui::Scale(50, 0) == 50);
    assert(ui::kSettingsComboRowLogical == 54);
    assert(ui::kSettingsCardRadiusLogical == 8);
    assert(ui::kSettingsNavHeightLogical == 40);
    assert(ui::kSettingsNavGapLogical == 4);
    assert(ui::Scale(-10, 144) == -15);

    struct ClassicDpiExpectation {
        unsigned dpi;
        int clientWidth;
        int clientHeight;
        ui::UiRectMetrics input;
        ui::UiRectMetrics results;
        ui::UiRectMetrics command;
        int rowHeight;
        int numberDividerX;
        int shortcutDividerX;
        int titleTextLeft;
        int titleHeight;
        int dragHeight;
        int logoLeft;
        int logoTop;
        int glyphSize;
        int closeSize;
        int closeRightInset;
        int closeTop;
        int cornerDiameter;
    };

    constexpr std::array<
        ClassicDpiExpectation,
        5>
        classicDpiExpectations{{
            {
                96u,
                420,
                250,
                {8, 30, 404, 22},
                {8, 56, 404, 164},
                {8, 226, 404, 16},
                16,
                23,
                230,
                33,
                33,
                30,
                8,
                2,
                25,
                22,
                6,
                4,
                12,
            },
            {
                120u,
                525,
                313,
                {10, 38, 505, 28},
                {10, 70, 505, 205},
                {10, 283, 505, 20},
                20,
                29,
                288,
                41,
                41,
                38,
                10,
                3,
                31,
                28,
                8,
                5,
                15,
            },
            {
                144u,
                630,
                375,
                {12, 45, 606, 33},
                {12, 84, 606, 246},
                {12, 339, 606, 24},
                24,
                35,
                345,
                50,
                50,
                45,
                12,
                3,
                38,
                33,
                9,
                6,
                18,
            },
            {
                168u,
                735,
                438,
                {14, 53, 707, 39},
                {14, 98, 707, 287},
                {14, 396, 707, 28},
                28,
                40,
                403,
                58,
                58,
                53,
                14,
                4,
                44,
                39,
                11,
                7,
                21,
            },
            {
                192u,
                840,
                500,
                {16, 60, 808, 44},
                {16, 112, 808, 328},
                {16, 452, 808, 32},
                32,
                46,
                460,
                66,
                66,
                60,
                16,
                4,
                50,
                44,
                12,
                8,
                24,
            },
        }};

    const auto assertRect =
        [](const ui::UiRectMetrics& actual,
           const ui::UiRectMetrics& expected) {
            assert(actual.left == expected.left);
            assert(actual.top == expected.top);
            assert(actual.width == expected.width);
            assert(actual.height == expected.height);
        };

    for (const auto& expected :
         classicDpiExpectations) {
        const auto actual =
            ui::ClassicLauncherMetricsForDpi(
                expected.dpi);

        assert(actual.clientWidth ==
               expected.clientWidth);
        assert(actual.clientHeight ==
               expected.clientHeight);
        assertRect(actual.input, expected.input);
        assertRect(actual.results, expected.results);
        assertRect(actual.command, expected.command);
        assert(actual.rowHeight ==
               expected.rowHeight);
        assert(actual.numberDividerX ==
               expected.numberDividerX);
        assert(actual.shortcutDividerX ==
               expected.shortcutDividerX);
        assert(actual.titleTextLeft ==
               expected.titleTextLeft);
        assert(actual.titleHeight ==
               expected.titleHeight);
        assert(actual.dragHeight ==
               expected.dragHeight);
        assert(actual.logoLeft ==
               expected.logoLeft);
        assert(actual.logoTop ==
               expected.logoTop);
        assert(actual.glyphSize ==
               expected.glyphSize);
        assert(actual.closeSize ==
               expected.closeSize);
        assert(actual.closeRightInset ==
               expected.closeRightInset);
        assert(actual.closeTop ==
               expected.closeTop);
        assert(actual.cornerDiameter ==
               expected.cornerDiameter);

        assert(
            actual.results.height -
                actual.rowHeight *
                    static_cast<int>(
                        ui::kClassicLauncherMetrics
                            .maxResults) ==
            ui::Scale(4, expected.dpi));
        assert(
            actual.command.top -
                (actual.results.top +
                 actual.results.height) ==
            ui::Scale(6, expected.dpi));
        assert(
            actual.command.top +
                actual.command.height <=
            actual.clientHeight);
    }

    assert(
        ui::kClassicSeparatorPhysicalThickness ==
        1);

    for (const auto [dpi, expectedSize] :
         std::array<std::pair<unsigned, int>, 5>{
             std::pair{96u, 25},
             std::pair{120u, 31},
             std::pair{144u, 38},
             std::pair{168u, 44},
             std::pair{192u, 50},
         }) {
        const int target =
            ui::Scale(25, dpi);
        const auto index =
            ui::ClassicGlyphAssetIndexForTarget(
                target);
        assert(
            ui::kClassicGlyphAssetPixelSizes[
                index] ==
            expectedSize);
    }

    assert(
        ui::ClassicGlyphAssetIndexForTarget(
            26) == 1);
    assert(
        ui::ClassicGlyphAssetIndexForTarget(
            32) == 2);
    assert(
        ui::ClassicGlyphAssetIndexForTarget(
            39) == 3);
    assert(
        ui::ClassicGlyphAssetIndexForTarget(
            45) == 4);
    assert(
        ui::ClassicGlyphAssetIndexForTarget(
            64) == 4);

    std::cout
        << "UI foundation metrics verified\n";
    return 0;
}
