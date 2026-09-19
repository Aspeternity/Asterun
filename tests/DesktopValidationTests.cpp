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

    for (const unsigned dpi :
         std::array<unsigned, 4>{
             96, 120, 144, 192}) {

        const auto scale =
            [dpi](int value) {
                return settings_layout::
                    Scale(value, dpi);
            };

        const int wideWidth =
            scale(1040);

        const auto wide =
            settings_layout::
                BuildGeneralLayout(
                    wideWidth,
                    dpi,
                    0);

        assert(!wide.stackedCards);
        assert(!wide.compactHotkeys);
        AssertPositiveRect(wide.behavior);
        AssertPositiveRect(wide.search);
        AssertPositiveRect(wide.monitor);
        assert(
            wide.search.left >
            wide.behavior.right);
        assert(
            wide.behavior.right <=
            wideWidth - scale(42));
        assert(
            wide.search.right <=
            wideWidth - scale(42));
        assert(
            wide.monitor.right <=
            wideWidth - scale(42));
        assert(
            settings_layout::
                MaxScrollOffset(
                    wide,
                    scale(800),
                    dpi) ==
            0);

        const int narrowWidth =
            scale(820);

        const auto narrow =
            settings_layout::
                BuildGeneralLayout(
                    narrowWidth,
                    dpi,
                    0);

        assert(narrow.stackedCards);
        assert(narrow.compactHotkeys);
        AssertPositiveRect(narrow.behavior);
        AssertPositiveRect(narrow.search);
        AssertPositiveRect(narrow.monitor);
        assert(
            narrow.search.left ==
            narrow.behavior.left);
        assert(
            narrow.search.top >
            narrow.behavior.bottom);
        assert(
            narrow.search.right <=
            narrowWidth - scale(42));
        assert(
            narrow.monitor.right <=
            narrowWidth - scale(42));

        const int maxScroll =
            settings_layout::
                MaxScrollOffset(
                    narrow,
                    scale(680),
                    dpi);

        assert(maxScroll > 0);

        const int scroll =
            scale(72);

        const auto shifted =
            settings_layout::
                BuildGeneralLayout(
                    narrowWidth,
                    dpi,
                    scroll);

        assert(
            shifted.behavior.top ==
            narrow.behavior.top -
                scroll);
        assert(
            shifted.search.top ==
            narrow.search.top -
                scroll);
        assert(
            shifted.monitor.top ==
            narrow.monitor.top -
                scroll);
        assert(
            shifted.contentBottom ==
            narrow.contentBottom);
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
