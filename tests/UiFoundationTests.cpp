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

    assert(ui::kSettingsSidebarWidthLogical == 176);
    assert(ui::kSettingsContentLeftInsetLogical == 38);
    assert(ui::kSettingsContentRightInsetLogical == 34);
    assert(ui::kSettingsToggleRowLogical == 50);

    for (const auto [dpi, expected] :
         std::array<std::pair<unsigned, int>, 4>{
             std::pair{96u, 50},
             std::pair{120u, 63},
             std::pair{144u, 75},
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

    std::cout
        << "UI foundation metrics verified\n";
    return 0;
}
