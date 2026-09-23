#include "SettingsLayout.hpp"

#include <algorithm>
#include <cstdint>

namespace altrun::settings_layout {

int Scale(
    int logicalValue,
    unsigned dpi) noexcept {

    return ui::Scale(
        logicalValue,
        dpi);
}



GeneralLayoutMetrics BuildGeneralLayout(
    int clientWidth,
    unsigned dpi,
    int scrollOffset,
    int sidebarWidthLogical) noexcept {

    const auto scale =
        [dpi](int value) {
            return Scale(value, dpi);
        };

    const int contentLeft =
        scale(sidebarWidthLogical) +
        scale(kContentLeftInsetLogical);

    const int contentRight =
        std::max(
            contentLeft +
                scale(260),
            clientWidth -
                scale(kContentRightInsetLogical));

    const int contentWidth =
        contentRight -
        contentLeft;

    const int cardWidth =
        std::min(
            contentWidth,
            scale(
                kGeneralCardMaxWidthLogical));

    const int cardLeft =
        contentLeft;
    const int cardRight =
        cardLeft + cardWidth;

    const int behaviorTitleTop =
        scale(kSectionTitleTopLogical);
    const int behaviorTop =
        scale(kFirstCardTopLogical);
    const int behaviorBottom =
        behaviorTop +
        scale(kToggleRowLogical) * 7;

    const int searchTitleTop =
        behaviorBottom +
        scale(18);
    const int searchTop =
        searchTitleTop +
        scale(30);
    const int searchBottom =
        searchTop +
        scale(kToggleRowLogical) * 5;

    const int placementTitleTop =
        searchBottom +
        scale(18);
    const int placementTop =
        placementTitleTop +
        scale(30);
    const int placementBottom =
        placementTop +
        scale(
            ui::kSettingsComboRowLogical) *
            4;

    const int noteTop =
        placementBottom;

    GeneralLayoutMetrics metrics;

    metrics.behavior = {
        cardLeft,
        behaviorTop - scrollOffset,
        cardRight,
        behaviorBottom - scrollOffset,
    };

    metrics.search = {
        cardLeft,
        searchTop - scrollOffset,
        cardRight,
        searchBottom - scrollOffset,
    };

    metrics.placement = {
        cardLeft,
        placementTop - scrollOffset,
        cardRight,
        placementBottom - scrollOffset,
    };

    metrics.behaviorTitleTop =
        behaviorTitleTop -
        scrollOffset;
    metrics.searchTitleTop =
        searchTitleTop -
        scrollOffset;
    metrics.placementTitleTop =
        placementTitleTop -
        scrollOffset;
    metrics.noteTop =
        noteTop -
        scrollOffset;
    metrics.contentBottom =
        noteTop +
        scale(12);

    return metrics;
}

int MaxScrollOffset(
    const GeneralLayoutMetrics& fullLayout,
    int clientHeight,
    unsigned dpi) noexcept {

    return std::max(
        0,
        fullLayout.contentBottom +
            Scale(10, dpi) -
            clientHeight);
}

Rect ClampRectToWorkArea(
    Rect requested,
    Rect workArea) noexcept {

    const int workWidth =
        std::max(
            0,
            workArea.right -
                workArea.left);

    const int workHeight =
        std::max(
            0,
            workArea.bottom -
                workArea.top);

    if (workWidth == 0 ||
        workHeight == 0) {
        return requested;
    }

    const int requestedWidth =
        std::max(
            0,
            requested.right -
                requested.left);

    const int requestedHeight =
        std::max(
            0,
            requested.bottom -
                requested.top);

    const int width =
        std::min(
            requestedWidth,
            workWidth);

    const int height =
        std::min(
            requestedHeight,
            workHeight);

    const int left =
        std::clamp(
            requested.left,
            workArea.left,
            workArea.right -
                width);

    const int top =
        std::clamp(
            requested.top,
            workArea.top,
            workArea.bottom -
                height);

    return {
        left,
        top,
        left + width,
        top + height,
    };
}

} // namespace altrun::settings_layout
