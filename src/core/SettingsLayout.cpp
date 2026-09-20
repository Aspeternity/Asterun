#include "SettingsLayout.hpp"

#include <algorithm>
#include <cstdint>

namespace altrun::settings_layout {

int Scale(
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
        scale(650);

    const int cardTop =
        scale(170);

    const int behaviorRowHeight =
        scale(kToggleRowLogical);

    const int searchRowHeight =
        scale(kToggleRowLogical);

    const int behaviorWidth =
        stackedCards
            ? contentWidth
            : (contentWidth - gap) / 2;

    const int behaviorBottom =
        cardTop +
        behaviorRowHeight * 6;

    int searchTitleTop =
        scale(138);

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
            scale(32);

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
        searchRowHeight * 5;

    const int cardsBottom =
        std::max(
            behaviorBottom,
            searchBottom);

    // v0.6 alpha.6 moves all hotkey editing to its own Settings page.
    // Keep the legacy metric fields populated for compatibility with the
    // existing layout contract, but collapse the General-page hotkey area.
    const bool compactHotkeys =
        false;

    const int hotkeySectionTop =
        cardsBottom +
        scale(16);

    const int primaryRowTop =
        hotkeySectionTop;
    const int primaryKeyRowTop =
        hotkeySectionTop;
    const int primaryStatusTop =
        hotkeySectionTop;
    const int auxiliaryRowTop =
        hotkeySectionTop;
    const int auxiliaryKeyRowTop =
        hotkeySectionTop;
    const int auxiliaryStatusTop =
        hotkeySectionTop;

    const int popupSectionTop =
        cardsBottom +
        scale(16);

    const int monitorTop =
        popupSectionTop +
        scale(30);

    const int monitorBottom =
        monitorTop +
        scale(54);

    const int noteTop =
        monitorBottom +
        scale(8);

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

    metrics.monitor = {
        contentLeft,
        monitorTop - scrollOffset,
        contentRight,
        monitorBottom -
            scrollOffset,
    };

    metrics.behaviorTitleTop =
        scale(138) -
        scrollOffset;

    metrics.searchTitleTop =
        searchTitleTop -
        scrollOffset;

    metrics.hotkeySectionTop =
        hotkeySectionTop -
        scrollOffset;

    metrics.primaryRowTop =
        primaryRowTop -
        scrollOffset;

    metrics.primaryKeyRowTop =
        primaryKeyRowTop -
        scrollOffset;

    metrics.primaryStatusTop =
        primaryStatusTop -
        scrollOffset;

    metrics.auxiliaryRowTop =
        auxiliaryRowTop -
        scrollOffset;

    metrics.auxiliaryKeyRowTop =
        auxiliaryKeyRowTop -
        scrollOffset;

    metrics.auxiliaryStatusTop =
        auxiliaryStatusTop -
        scrollOffset;

    metrics.popupSectionTop =
        popupSectionTop -
        scrollOffset;

    metrics.noteTop =
        noteTop -
        scrollOffset;

    metrics.contentBottom =
        noteTop +
        scale(28);

    metrics.stackedCards =
        stackedCards;

    metrics.compactHotkeys =
        compactHotkeys;

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
