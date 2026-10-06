#include "ui/UiMetrics.hpp"
#include "ui/LauncherInteraction.hpp"

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
    assert(ui::kModernCompactLauncherMetrics.maxResults == 10);

    using ui::launcher_interaction::
        StableModernVisibleRows;
    using ui::launcher_interaction::
        StableSelectionIndex;

    // Pending Everything work may expand the shell, but it must not
    // transiently collapse below the already-visible row count.
    assert(
        StableModernVisibleRows(
            10,
            1,
            true) == 10);
    assert(
        StableModernVisibleRows(
            3,
            6,
            true) == 6);
    assert(
        StableModernVisibleRows(
            6,
            1,
            false) == 1);
    assert(
        StableModernVisibleRows(
            99,
            99,
            true) == 10);

    // Async result replacement follows a surviving identity. If that result
    // disappears, keep the nearest valid row rather than jumping to row 1.
    assert(
        StableSelectionIndex(
            5,
            2,
            7) == 2);
    assert(
        StableSelectionIndex(
            5,
            -1,
            3) == 2);
    assert(
        StableSelectionIndex(
            9,
            -1,
            10) == 9);
    assert(
        StableSelectionIndex(
            -1,
            -1,
            4) == 0);
    assert(
        StableSelectionIndex(
            3,
            -1,
            0) == -1);

    struct ModernDpiExpectation {
        unsigned dpi;
        int maxWidth;
        int maxHeight;
        int rowHeight;
    };

    constexpr std::array<
        ModernDpiExpectation,
        5>
        modernDpiExpectations{{
            {96u, 620, 428, 32},
            {120u, 775, 535, 40},
            {144u, 930, 642, 48},
            {168u, 1085, 749, 56},
            {192u, 1240, 856, 64},
        }};

    for (const auto& expected :
         modernDpiExpectations) {
        const auto full =
            ui::ModernCompactLauncherMetricsForDpi(
                expected.dpi,
                10);
        assert(full.clientWidth ==
               expected.maxWidth);
        assert(full.clientHeight ==
               expected.maxHeight);
        assert(full.rowHeight ==
               expected.rowHeight);
        assert(full.searchGlyph.width > 0);
        assert(full.searchEdit.left >
               full.searchGlyph.left);
        assert(
            full.searchEdit.height ==
            ui::Scale(
                20,
                expected.dpi));
        const int searchCenter2 =
            full.searchSurface.top * 2 +
            full.searchSurface.height;
        const int editCenter2 =
            full.searchEdit.top * 2 +
            full.searchEdit.height;
        const int glyphCenter2 =
            full.searchGlyph.top * 2 +
            full.searchGlyph.height;
        assert(
            editCenter2 >=
                searchCenter2 - 1 &&
            editCenter2 <=
                searchCenter2 + 1);
        assert(
            glyphCenter2 >=
                searchCenter2 - 1 &&
            glyphCenter2 <=
                searchCenter2 + 1);
        assert(
            full.searchSurface.height ==
            ui::Scale(
                38,
                expected.dpi));
        assert(
            full.resultsSurface.top ==
            ui::Scale(
                58,
                expected.dpi));
        assert(full.resultsSurface.height > 0);
        assert(full.resultsList.height ==
               full.rowHeight * 10);
        assert(
            full.shortcutHintWidth ==
            ui::Scale(
                34,
                expected.dpi));
        assert(
            full.shortcutHintGap ==
            ui::Scale(
                12,
                expected.dpi));
        assert(full.footerSurface.height > 0);
        assert(full.footer.width > 0);
        assert(full.footerAction.width > 0);
        assert(
            full.footer.left +
                full.footer.width <=
            full.footerAction.left);
        assert(full.resultsList.left >
               full.resultsSurface.left);
        assert(
            full.resultsList.left +
                full.resultsList.width <
            full.resultsSurface.left +
                full.resultsSurface.width);

        const auto six =
            ui::ModernCompactLauncherMetricsForDpi(
                expected.dpi,
                6);
        assert(six.clientWidth ==
               full.clientWidth);
        assert(six.clientHeight ==
               full.clientHeight -
                   full.rowHeight * 4);
        assert(six.resultsList.height ==
               six.rowHeight * 6);
        assert(six.footerSurface.top <
               full.footerSurface.top);

        const auto one =
            ui::ModernCompactLauncherMetricsForDpi(
                expected.dpi,
                1);
        assert(one.clientHeight ==
               full.clientHeight -
                   full.rowHeight * 9);
        assert(one.resultsList.height ==
               one.rowHeight);
        assert(one.footerSurface.height > 0);

        const auto empty =
            ui::ModernCompactLauncherMetricsForDpi(
                expected.dpi,
                0);
        assert(empty.clientWidth ==
               full.clientWidth);
        assert(empty.clientHeight <
               one.clientHeight);
        assert(empty.resultsSurface.height == 0);
        assert(empty.resultsList.height == 0);
        assert(empty.footerSurface.height == 0);
        assert(empty.footer.height == 0);
        assert(empty.footerAction.height == 0);

        const auto clamped =
            ui::ModernCompactLauncherMetricsForDpi(
                expected.dpi,
                99);
        assert(clamped.clientHeight ==
               full.clientHeight);
        assert(clamped.resultsList.height ==
               full.resultsList.height);

        // Every visible-row transition after the first row is exactly one
        // row-height step at every supported DPI.
        auto previous =
            ui::ModernCompactLauncherMetricsForDpi(
                expected.dpi,
                1);
        for (std::size_t rows = 2;
             rows <= 10;
             ++rows) {
            const auto current =
                ui::ModernCompactLauncherMetricsForDpi(
                    expected.dpi,
                    rows);
            assert(
                current.clientHeight -
                    previous.clientHeight ==
                expected.rowHeight);
            assert(
                current.resultsList.height ==
                static_cast<int>(rows) *
                    expected.rowHeight);
            previous = current;
        }
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

    assert(
        ui::NextComboBoxVisibleItems(
            0) == 0);
    assert(
        ui::NextComboBoxVisibleItems(
            3) == 3);
    assert(
        ui::NextComboBoxVisibleItems(
            5) == 5);
    assert(
        ui::NextComboBoxVisibleItems(
            8) == 6);
    assert(
        ui::NextComboBoxVisibleItems(
            8,
            4) == 4);

    for (const unsigned dpi :
         std::array<unsigned, 5>{
             96u,
             120u,
             144u,
             168u,
             192u}) {
        const int item =
            ui::Scale(
                ui::kNextComboBoxItemHeightLogical,
                dpi);
        const int chrome =
            ui::Scale(
                ui::kNextComboBoxChromeHeightLogical,
                dpi);

        assert(
            ui::NextComboBoxDropHeightForDpi(
                0,
                dpi) ==
            item + chrome);
        assert(
            ui::NextComboBoxDropHeightForDpi(
                3,
                dpi) ==
            item * 4 + chrome);
        assert(
            ui::NextComboBoxDropHeightForDpi(
                5,
                dpi) ==
            item * 6 + chrome);
        assert(
            ui::NextComboBoxDropHeightForDpi(
                8,
                dpi) ==
            item * 7 + chrome);
    }

    assert(
        ui::NextComboBoxDropHeightForDpi(
            3,
            96) == 134);
    assert(
        ui::NextComboBoxDropHeightForDpi(
            5,
            96) == 194);
    assert(
        ui::NextComboBoxDropHeightForDpi(
            8,
            96) == 224);

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
