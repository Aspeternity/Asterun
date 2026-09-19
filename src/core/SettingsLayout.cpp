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
        scale(42);

    const int contentRight =
        std::max(
            contentLeft +
                scale(260),
            clientWidth -
                scale(42));

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
        scale(46);

    const int searchRowHeight =
        scale(46);

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
        searchRowHeight * 4;

    const int cardsBottom =
        std::max(
            behaviorBottom,
            searchBottom);

    const bool compactHotkeys =
        contentWidth <
        scale(650);

    const int hotkeySectionTop =
        cardsBottom +
        scale(16);

    const int primaryRowTop =
        hotkeySectionTop +
        scale(28);

    const int primaryKeyRowTop =
        compactHotkeys
            ? primaryRowTop +
                scale(32)
            : primaryRowTop;

    const int primaryStatusTop =
        compactHotkeys
            ? primaryRowTop +
                scale(66)
            : primaryRowTop +
                scale(32);

    const int auxiliaryRowTop =
        compactHotkeys
            ? primaryRowTop +
                scale(94)
            : primaryRowTop +
                scale(58);

    const int auxiliaryKeyRowTop =
        compactHotkeys
            ? primaryRowTop +
                scale(126)
            : auxiliaryRowTop;

    const int auxiliaryStatusTop =
        compactHotkeys
            ? primaryRowTop +
                scale(160)
            : auxiliaryRowTop +
                scale(32);

    const int popupSectionTop =
        compactHotkeys
            ? primaryRowTop +
                scale(190)
            : auxiliaryRowTop +
                scale(62);

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
