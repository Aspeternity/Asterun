#include "core/ClassicBehavior.hpp"
#include "core/SettingsLayout.hpp"

#include <array>
#include <cassert>
#include <iostream>

#ifdef NDEBUG
#error "desktop_validation_tests requires assert() in Release builds"
#endif

using namespace altrun;

namespace {

void AssertPositiveRect(
    const settings_layout::Rect& rect) {

    assert(rect.right > rect.left);
    assert(rect.bottom > rect.top);
}

} // namespace

int main() {
    using classic_behavior::
        QuickLaunchIndexForDigit;
    using classic_behavior::
        ShouldExecuteSingleResult;

    for (int digit = 1;
         digit <= 9;
         ++digit) {
        assert(
            QuickLaunchIndexForDigit(
                digit,
                "one-to-zero") ==
            digit - 1);
    }

    assert(
        QuickLaunchIndexForDigit(
            0,
            "one-to-zero") ==
        9);

    for (int digit = 0;
         digit <= 9;
         ++digit) {
        assert(
            QuickLaunchIndexForDigit(
                digit,
                "zero-to-nine") ==
            digit);
    }

    assert(
        QuickLaunchIndexForDigit(
            -1,
            "one-to-zero") ==
        -1);
    assert(
        QuickLaunchIndexForDigit(
            10,
            "zero-to-nine") ==
        -1);

    assert(
        ShouldExecuteSingleResult(
            true, true, false, false, false, 1));
    assert(
        !ShouldExecuteSingleResult(
            false, true, false, false, false, 1));
    assert(
        !ShouldExecuteSingleResult(
            true, false, false, false, false, 1));
    assert(
        !ShouldExecuteSingleResult(
            true, true, true, false, false, 1));
    assert(
        !ShouldExecuteSingleResult(
            true, true, false, true, false, 1));
    assert(
        !ShouldExecuteSingleResult(
            true, true, false, false, false, 0));
    assert(
        !ShouldExecuteSingleResult(
            true, true, false, false, false, 2));

    assert(
        !ShouldExecuteSingleResult(
            true, true, false, false, true, 1));

    assert(
        settings_layout::
            kToggleRowLogical == 50);
    assert(
        ui::kSettingsComboRowLogical == 54);
    assert(
        settings_layout::
            kPageDividerTopLogical == 84);
    assert(
        settings_layout::
            kSectionTitleTopLogical == 108);
    assert(
        settings_layout::
            kFirstCardTopLogical == 140);
    assert(
        settings_layout::
            kContentLeftInsetLogical == 38);
    assert(
        settings_layout::
            kContentRightInsetLogical == 34);
    assert(
        settings_layout::
            kGeneralCardMaxWidthLogical == 560);


    for (const unsigned dpi :
         std::array<unsigned, 4>{
             96, 120, 144, 192}) {

        const auto scale =
            [dpi](int value) {
                return settings_layout::
                    Scale(value, dpi);
            };

        const int clientWidth =
            scale(
                ui::
                    kSettingsClientWidthLogical);
        const int clientHeight =
            scale(
                ui::
                    kSettingsClientHeightLogical);

        const auto layout =
            settings_layout::
                BuildGeneralLayout(
                    clientWidth,
                    dpi,
                    0);

        AssertPositiveRect(
            layout.behavior);
        AssertPositiveRect(
            layout.search);
        AssertPositiveRect(
            layout.placement);

        assert(
            layout.behavior.left ==
            scale(
                ui::
                    kSettingsSidebarWidthLogical) +
                scale(
                    settings_layout::
                        kContentLeftInsetLogical));

        assert(
            layout.behavior.left ==
            layout.search.left);
        assert(
            layout.behavior.left ==
            layout.placement.left);
        assert(
            layout.behavior.right ==
            layout.search.right);
        assert(
            layout.behavior.right ==
            layout.placement.right);

        assert(
            layout.behavior.right -
                layout.behavior.left ==
            scale(
                settings_layout::
                    kGeneralCardMaxWidthLogical));

        assert(
            layout.behavior.bottom -
                layout.behavior.top ==
            scale(
                settings_layout::
                    kToggleRowLogical) *
                4 +
            scale(
                ui::
                    kSettingsComboRowLogical));

        assert(
            layout.search.bottom -
                layout.search.top ==
            scale(
                settings_layout::
                    kToggleRowLogical) *
                4);

        assert(
            layout.search.top >
            layout.behavior.bottom);
        assert(
            layout.placement.top >
            layout.search.bottom);

        const int maxScroll =
            settings_layout::
                MaxScrollOffset(
                    layout,
                    clientHeight,
                    dpi);

        assert(maxScroll > 0);

        assert(
            settings_layout::
                MaxScrollOffset(
                    layout,
                    scale(1100),
                    dpi) ==
            0);

        const int scroll =
            scale(72);

        const auto shifted =
            settings_layout::
                BuildGeneralLayout(
                    clientWidth,
                    dpi,
                    scroll);

        assert(
            shifted.behavior.top ==
            layout.behavior.top -
                scroll);
        assert(
            shifted.search.top ==
            layout.search.top -
                scroll);
        assert(
            shifted.placement.top ==
            layout.placement.top -
                scroll);
        assert(
            shifted.contentBottom ==
            layout.contentBottom);
    }

    const settings_layout::Rect work{
        0, 0, 1920, 1040};

    const auto oversized =
        settings_layout::
            ClampRectToWorkArea(
                {100, 80, 2260, 1680},
                work);

    assert(oversized.left == 0);
    assert(oversized.top == 0);
    assert(oversized.right == 1920);
    assert(oversized.bottom == 1040);

    const auto offscreen =
        settings_layout::
            ClampRectToWorkArea(
                {1700, 900, 2100, 1200},
                work);

    assert(offscreen.left == 1520);
    assert(offscreen.top == 740);
    assert(offscreen.right == 1920);
    assert(offscreen.bottom == 1040);

    std::cout
        << "Desktop validation logic tests passed\n";

    return 0;
}
