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

    const int gap =
        scale(18);

    const bool stackedCards =
        contentWidth <
        scale(680);

    const int cardTop =
        scale(kFirstCardTopLogical);

    const int behaviorWidth =
        stackedCards
            ? contentWidth
            : (contentWidth - gap) / 2;

    const int behaviorBottom =
        cardTop +
        scale(kToggleRowLogical) * 7;

    int searchTitleTop =
        scale(kSectionTitleTopLogical);
    int searchTop =
        cardTop;
    int searchLeft =
        contentLeft +
        behaviorWidth +
        gap;

    if (stackedCards) {
        searchTitleTop =
            behaviorBottom +
            scale(18);
        searchTop =
            searchTitleTop +
            scale(30);
        searchLeft =
            contentLeft;
    }

    const int searchWidth =
        stackedCards
            ? contentWidth
            : contentWidth -
                behaviorWidth -
                gap;

    const int searchBottom =
        searchTop +
        scale(kToggleRowLogical) * 5;

    const int placementTitleTop =
        searchBottom +
        scale(18);

    const int placementTop =
        placementTitleTop +
        scale(30);

    const int placementLeft =
        stackedCards
            ? contentLeft
            : searchLeft;

    const int placementWidth =
        stackedCards
            ? contentWidth
            : searchWidth;

    const int placementBottom =
        placementTop +
        scale(
            ui::kSettingsComboRowLogical) *
            3;

    const int noteTop =
        std::max(
            behaviorBottom,
            placementBottom);

    GeneralLayoutMetrics metrics;

    metrics.behavior = {
        contentLeft,
        cardTop - scrollOffset,
        contentLeft +
            behaviorWidth,
        behaviorBottom -
            scrollOffset,
    };

    metrics.search = {
        searchLeft,
        searchTop - scrollOffset,
        searchLeft +
            searchWidth,
        searchBottom -
            scrollOffset,
    };

    metrics.placement = {
        placementLeft,
        placementTop - scrollOffset,
        placementLeft +
            placementWidth,
        placementBottom -
            scrollOffset,
    };

    metrics.behaviorTitleTop =
        scale(kSectionTitleTopLogical) -
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
    metrics.stackedCards =
        stackedCards;

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
