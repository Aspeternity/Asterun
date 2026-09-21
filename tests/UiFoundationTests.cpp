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

    assert(ui::kSettingsSidebarWidthLogical == 208);
    assert(ui::kSettingsContentLeftInsetLogical == 38);
    assert(ui::kSettingsContentRightInsetLogical == 34);
    assert(ui::kSettingsToggleRowLogical == 62);

    for (const auto [dpi, expected] :
         std::array<std::pair<unsigned, int>, 4>{
             std::pair{96u, 62},
             std::pair{120u, 78},
             std::pair{144u, 93},
             std::pair{192u, 124},
         }) {
        assert(ui::Scale(62, dpi) == expected);
    }

    assert(ui::Scale(62, 0) == 62);
    assert(ui::kSettingsComboRowLogical == 68);
    assert(ui::kSettingsCardRadiusLogical == 8);
    assert(ui::kSettingsNavHeightLogical == 40);
    assert(ui::kSettingsNavGapLogical == 4);
    assert(ui::Scale(-10, 144) == -15);

    std::cout
        << "UI foundation metrics verified\n";
    return 0;
}
