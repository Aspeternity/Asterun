#include "SettingsWindow.hpp"
#include "UiComboBox.hpp"
#include "../core/HotkeyRegistry.hpp"

#include <algorithm>
#include <array>
#include <iterator>

namespace altrun {

void SettingsWindow::Layout() {
    if (!hwnd_) return;

    UpdatePageScrollBar();

    RECT client{};
    GetClientRect(
        hwnd_,
        &client);

    const int sidebar =
        Scale(
            kSidebarWidthLogical);
    const int sidebarMargin =
        Scale(16);
    const int navWidth =
        sidebar -
        sidebarMargin * 2;
    const int navHeight =
        Scale(
            ui::kSettingsNavHeightLogical);
    const int navGap =
        Scale(
            ui::kSettingsNavGapLogical);

    MoveWindow(
        brandName_,
        sidebarMargin,
        Scale(14),
        navWidth,
        Scale(34),
        TRUE);
    MoveWindow(
        brandSubtitle_,
        sidebarMargin,
        Scale(47),
        navWidth,
        Scale(30),
        TRUE);

    std::array<HWND, 5> primaryNav{
        navGeneral_,
        navHotkeys_,
        navProviders_,
        navAppearance_,
        navData_,
    };

    const int navTop =
        Scale(96);

    for (std::size_t i = 0;
         i < primaryNav.size();
         ++i) {
        MoveWindow(
            primaryNav[i],
            sidebarMargin,
            navTop +
                static_cast<int>(i) *
                    (navHeight + navGap),
            navWidth,
            navHeight,
            TRUE);
    }

    const int aboutY =
        std::max(
            navTop +
                static_cast<int>(
                    primaryNav.size()) *
                    (navHeight + navGap) +
                Scale(20),
            static_cast<int>(
                client.bottom) -
                navHeight -
                Scale(22));

    MoveWindow(
        navAbout_,
        sidebarMargin,
        aboutY,
        navWidth,
        navHeight,
        TRUE);

    LayoutCurrentPage();
}

void SettingsWindow::LayoutCurrentPage(BOOL repaint) {
    if (!hwnd_) {
        return;
    }

    RECT client{};
    GetClientRect(
        hwnd_,
        &client);

    const int sidebar =
        Scale(
            kSidebarWidthLogical);
    const int contentLeft =
        sidebar +
        Scale(
            settings_layout::
                kContentLeftInsetLogical);
    const int contentRight =
        client.right -
        Scale(
            settings_layout::
                kContentRightInsetLogical);
    const int contentWidth =
        std::max(
            Scale(360),
            contentRight -
                contentLeft);
    const int pageScroll =
        page_ == Page::General
            ? generalScrollOffset_
            : 0;

    MoveWindow(
        pageTitle_,
        contentLeft,
        Scale(
            settings_layout::
                kPageTitleTopLogical) -
            pageScroll,
        contentWidth,
        Scale(40),
        repaint);

    switch (page_) {
    case Page::General:
        LayoutGeneral(repaint);
        break;
    case Page::Hotkeys:
        LayoutHotkeys(
            client,
            contentLeft,
            contentWidth,
            repaint);
        break;
    case Page::Providers:
        LayoutProviders(
            contentLeft,
            contentWidth);
        break;
    case Page::Appearance:
        LayoutAppearance(
            contentLeft,
            contentWidth);
        break;
    case Page::Data:
        LayoutData(
            contentLeft,
            contentWidth);
        break;
    case Page::About:
        LayoutAbout(
            contentLeft,
            contentWidth);
        break;
    }
}

RECT SettingsWindow::ContentPaneRect() const {
    RECT client{};

    if (!hwnd_) {
        return client;
    }

    GetClientRect(
        hwnd_,
        &client);

    client.left =
        std::min(
            client.right,
            static_cast<LONG>(
                Scale(
                    kSidebarWidthLogical) +
                1));
    return client;
}

void SettingsWindow::RedrawCurrentPage() {
    if (!hwnd_) {
        return;
    }

    RECT content =
        ContentPaneRect();

    RedrawWindow(
        hwnd_,
        &content,
        nullptr,
        RDW_INVALIDATE |
            RDW_NOERASE |
            RDW_ALLCHILDREN |
            RDW_UPDATENOW);
}

void SettingsWindow::LayoutGeneral(BOOL repaint) {
        const auto metrics =
            BuildGeneralLayout(
                generalScrollOffset_);

        MoveWindow(
            generalBehaviorTitle_,
            metrics.behavior.left,
            metrics.behaviorTitleTop,
            metrics.behavior.right -
                metrics.behavior.left,
            Scale(26),
            TRUE);

        MoveWindow(
            searchBehaviorTitle_,
            metrics.search.left,
            metrics.searchTitleTop,
            metrics.search.right -
                metrics.search.left,
            Scale(26),
            TRUE);

        const int toggleHeight = Scale(settings_layout::kToggleRowLogical);
        const int comboRowHeight = Scale(ui::kSettingsComboRowLogical);
        const int behaviorX = metrics.behavior.left + Scale(1);
        const int behaviorWidth = metrics.behavior.right - metrics.behavior.left - Scale(2);

        MoveWindow(startWithWindows_, behaviorX, metrics.behavior.top + Scale(1),
            behaviorWidth, toggleHeight, repaint);

        const int startupTop = metrics.behavior.top + toggleHeight;
        const int startupComboWidth =
            ui::MeasureNextComboBoxPreferredWidth(
                startupBehavior_,
                dpi_);
        const int startupComboX =
            metrics.behavior.right -
            startupComboWidth -
            Scale(18);
        const int startupLabelX =
            metrics.behavior.left +
            Scale(18);
        MoveWindow(
            startupBehaviorLabel_,
            startupLabelX,
            startupTop + Scale(15),
            std::max(
                Scale(150),
                startupComboX -
                    startupLabelX -
                    Scale(16)),
            Scale(24),
            TRUE);
        ui::MoveNextComboBox(
            startupBehavior_,
            startupComboX,
            startupTop + Scale(10),
            startupComboWidth,
            dpi_,
            repaint);

        const int trayTop = startupTop + comboRowHeight;
        MoveWindow(showTrayIcon_, behaviorX, trayTop, behaviorWidth, toggleHeight, repaint);
        MoveWindow(soundEnabled_, behaviorX, trayTop + toggleHeight,
            behaviorWidth, toggleHeight, repaint);
        MoveWindow(addToSendToMenu_, behaviorX, trayTop + 2 * toggleHeight,
            behaviorWidth, toggleHeight, repaint);

        const int searchX = metrics.search.left + Scale(1);
        const int searchWidth = metrics.search.right - metrics.search.left - Scale(2);
        std::array<HWND, 3> searchRows{
            pinyinSearch_, numericQuickLaunch_, executeSingleResult_,
        };
        for (std::size_t i = 0; i < searchRows.size(); ++i) {
            MoveWindow(searchRows[i], searchX,
                metrics.search.top + Scale(1) + static_cast<int>(i) * toggleHeight,
                searchWidth, toggleHeight, repaint);
        }

        MoveWindow(
            placementSectionTitle_,
            metrics.placement.left,
            metrics.placementTitleTop,
            metrics.placement.right -
                metrics.placement.left,
            Scale(26),
            TRUE);

        const int rowHeight =
            Scale(
                ui::kSettingsComboRowLogical);
        const int labelX =
            metrics.placement.left +
            Scale(18);

        const std::array<
            std::pair<HWND, HWND>,
            4>
            placementRows{{
                {
                    popupMonitorLabel_,
                    popupMonitor_,
                },
                {
                    launcherPlacementLabel_,
                    launcherPlacement_,
                },
                {
                    settingsPlacementLabel_,
                    settingsPlacement_,
                },
                {
                    shortcutManagerPlacementLabel_,
                    shortcutManagerPlacement_,
                },
            }};

        for (std::size_t i = 0;
             i < placementRows.size();
             ++i) {
            const int top =
                metrics.placement.top +
                static_cast<int>(i) *
                    rowHeight;
            const HWND combo =
                placementRows[i].second;
            const int comboWidth =
                ui::MeasureNextComboBoxPreferredWidth(
                    combo,
                    dpi_);
            const int comboX =
                metrics.placement.right -
                comboWidth -
                Scale(18);

            MoveWindow(
                placementRows[i].first,
                labelX,
                top + Scale(15),
                std::max(
                    Scale(150),
                    comboX -
                        labelX -
                        Scale(16)),
                Scale(24),
                TRUE);

            ui::MoveNextComboBox(
                combo,
                comboX,
                top + Scale(10),
                comboWidth,
                dpi_,
                repaint);
        }
    }

void SettingsWindow::LayoutHotkeys(RECT client, int contentLeft, int contentWidth, BOOL repaint) {
        const int width =
            std::min(
                contentWidth,
                Scale(560));
        const int cardLeft =
            contentLeft;
        const int cardRight =
            contentLeft + width;
        const int baseRowHeight =
            Scale(54);
        const int captureWidth =
            Scale(166);
        const int resetWidth =
            Scale(84);
        const int resetGap =
            Scale(12);
        const int toggleWidth =
            Scale(48);
        const int controlGap =
            Scale(10);
        const int inner =
            Scale(18);
        const int scroll =
            hotkeyScrollOffset_;

        const int globalTitleTop =
            Scale(108) -
            scroll;
        const int globalCardTop =
            Scale(140) -
            scroll;
        const int globalHeight =
            HotkeyGroupHeight(true);
        const int globalCardBottom =
            globalCardTop +
            globalHeight;
        const int launcherTitleTop =
            globalCardBottom +
            Scale(22);
        const int launcherCardTop =
            launcherTitleTop +
            Scale(32);

        MoveWindow(
            hotkeyGlobalTitle_,
            contentLeft,
            globalTitleTop,
            width,
            Scale(26),
            TRUE);

        MoveWindow(
            hotkeyLauncherTitle_,
            contentLeft,
            launcherTitleTop,
            width,
            Scale(26),
            TRUE);

        int globalTop =
            globalCardTop;
        int launcherTop =
            launcherCardTop;

        for (auto& row :
             hotkeyRows_) {
            const auto* action =
                FindHotkeyAction(
                    row.actionId);

            if (!action) {
                continue;
            }

            const bool global =
                action->scope ==
                    HotkeyScope::Global;
            const int top =
                global
                    ? globalTop
                    : launcherTop;
            const int rowHeight =
                HotkeyRowHeight(row);

            const int toggleX =
                cardRight -
                inner -
                toggleWidth;
            const int captureX =
                toggleX -
                controlGap -
                captureWidth;
            const int resetX =
                captureX -
                resetGap -
                resetWidth;

            MoveWindow(
                row.title,
                cardLeft + inner,
                top +
                    (baseRowHeight -
                     Scale(24)) / 2,
                std::max(
                    Scale(150),
                    resetX -
                        cardLeft -
                        inner -
                        Scale(14)),
                Scale(24),
                TRUE);

            MoveWindow(
                row.capture,
                captureX,
                top +
                    (baseRowHeight -
                     Scale(34)) / 2,
                captureWidth,
                Scale(34),
                TRUE);

            if (row.enabled) {
                MoveWindow(
                    row.enabled,
                    toggleX,
                    top +
                        (baseRowHeight -
                         Scale(32)) / 2,
                    toggleWidth,
                    Scale(32),
                    TRUE);
            }

            const int auxiliaryTop =
                top +
                baseRowHeight;
            const int auxiliaryHeight =
                HotkeyAuxiliaryHeight(row);
            const int auxiliaryWidth =
                cardRight -
                inner -
                captureX;

            if (row.statusVisible &&
                auxiliaryHeight > 0) {
                MoveWindow(
                    row.status,
                    captureX,
                    auxiliaryTop +
                        Scale(4),
                    auxiliaryWidth,
                    std::max(
                        Scale(22),
                        auxiliaryHeight -
                            Scale(8)),
                    TRUE);
            } else {
                SetWindowRgn(
                    row.status,
                    nullptr,
                    FALSE);
                ShowWindow(
                    row.status,
                    SW_HIDE);
                MoveWindow(
                    row.status,
                    captureX,
                    auxiliaryTop,
                    0,
                    0,
                    FALSE);
            }

            MoveWindow(
                row.reset,
                resetX,
                top +
                    (baseRowHeight -
                     Scale(26)) / 2,
                resetWidth,
                Scale(26),
                TRUE);

            if (global) {
                globalTop +=
                    rowHeight;
            } else {
                launcherTop +=
                    rowHeight;
            }
        }

        const int resetAllWidth =
            Scale(184);
        const int resetAllHeight =
            Scale(34);
        const int resetAllTop =
            std::max(
                Scale(120),
                static_cast<int>(
                    client.bottom) -
                    Scale(18) -
                    resetAllHeight);

        MoveWindow(
            hotkeyResetAll_,
            cardRight -
                inner -
                resetAllWidth,
            resetAllTop,
            resetAllWidth,
            resetAllHeight,
            TRUE);

        ClipHotkeyControlsToViewport(repaint);
    }

void SettingsWindow::LayoutProviders(int contentLeft, int contentWidth) {
        const int width =
            std::min(
                contentWidth,
                Scale(720));

        MoveWindow(
            providerSectionTitle_,
            contentLeft,
            Scale(108),
            width,
            Scale(26),
            TRUE);

        const RECT appCard =
            ProviderCardRect();
        const int rowHeight =
            Scale(
                settings_layout::
                    kToggleRowLogical);

        std::array<HWND, 4> appRows{
            providerStartMenu_,
            providerPackaged_,
            providerAppPaths_,
            providerPath_,
        };

        for (std::size_t i = 0;
             i < appRows.size();
             ++i) {
            MoveWindow(
                appRows[i],
                appCard.left +
                    Scale(1),
                appCard.top +
                    Scale(1) +
                    static_cast<int>(i) *
                        rowHeight,
                appCard.right -
                    appCard.left -
                    Scale(2),
                rowHeight,
                TRUE);
        }

        MoveWindow(
            providerFilesTitle_,
            contentLeft,
            Scale(362),
            width,
            Scale(26),
            TRUE);

        const int filesTop =
            Scale(394);

        MoveWindow(
            providerEverything_,
            contentLeft + Scale(1),
            filesTop + Scale(1),
            width - Scale(2),
            rowHeight,
            TRUE);

        MoveWindow(
            managedEverythingTrayIcon_,
            contentLeft + Scale(1),
            filesTop + Scale(1) +
                rowHeight,
            width - Scale(2),
            rowHeight,
            TRUE);

        const int statusTop = filesTop + rowHeight * (providerTrayVisible_ ? 2 : 1) + Scale(10);
        const int actionsTop = statusTop + Scale(50);
        MoveWindow(
            providerStatus_,
            contentLeft + Scale(18),
            statusTop,
            width - Scale(36),
            Scale(42),
            TRUE);

        MoveWindow(
            providerGetEverything_,
            contentLeft + Scale(18),
            actionsTop,
            Scale(210),
            Scale(34),
            TRUE);

        MoveWindow(
            providerUpdateEverything_,
            contentLeft + Scale(18),
            actionsTop,
            Scale(210),
            Scale(34),
            TRUE);

        MoveWindow(
            providerRecheckEverything_,
            contentLeft + Scale(240),
            actionsTop,
            Scale(128),
            Scale(34),
            TRUE);
    }

void SettingsWindow::LayoutAppearance(int contentLeft, int contentWidth) {
        const int width =
            std::min(
                contentWidth,
                Scale(560));
        const int inner =
            Scale(18);
        const int styleComboWidth =
            ui::MeasureNextComboBoxPreferredWidth(
                uiStyle_,
                dpi_);
        const int styleComboX =
            contentLeft +
            width -
            inner -
            styleComboWidth;
        const int languageComboWidth =
            ui::MeasureNextComboBoxPreferredWidth(
                language_,
                dpi_);
        const int languageComboX =
            contentLeft +
            width -
            inner -
            languageComboWidth;

        MoveWindow(
            appearanceLauncherTitle_,
            contentLeft,
            Scale(108),
            width,
            Scale(26),
            TRUE);

        MoveWindow(
            uiStyleLabel_,
            contentLeft + inner,
            Scale(157),
            std::max(
                Scale(160),
                styleComboX -
                    contentLeft -
                    inner -
                    Scale(16)),
            Scale(24),
            TRUE);
        ui::MoveNextComboBox(
            uiStyle_,
            styleComboX,
            Scale(158),
            styleComboWidth,
            dpi_);

        MoveWindow(
            appearanceAppTitle_,
            contentLeft,
            Scale(232),
            width,
            Scale(26),
            TRUE);

        MoveWindow(
            languageLabel_,
            contentLeft + inner,
            Scale(281),
            std::max(
                Scale(160),
                languageComboX -
                    contentLeft -
                    inner -
                    Scale(16)),
            Scale(24),
            TRUE);
        ui::MoveNextComboBox(
            language_,
            languageComboX,
            Scale(282),
            languageComboWidth,
            dpi_);
    }

void SettingsWindow::LayoutData(int contentLeft, int contentWidth) {
        const int width =
            std::min(
                contentWidth,
                Scale(560));
        const int inner =
            Scale(18);
        const int gap =
            Scale(12);

        MoveWindow(
            dataPathLabel_,
            contentLeft,
            Scale(108),
            width,
            Scale(26),
            TRUE);

        const int openWidth =
            Scale(144);

        MoveWindow(
            dataPath_,
            contentLeft + inner,
            Scale(158),
            width -
                inner * 3 -
                openWidth,
            Scale(24),
            TRUE);
        MoveWindow(
            openDataFolder_,
            contentLeft +
                width -
                inner -
                openWidth,
            Scale(152),
            openWidth,
            Scale(34),
            TRUE);

        MoveWindow(
            dataTransferLabel_,
            contentLeft,
            Scale(232),
            width,
            Scale(26),
            TRUE);

        const int transferWidth =
            (width -
             inner * 2 -
             gap) / 2;

        MoveWindow(
            dataImportTsv_,
            contentLeft + inner,
            Scale(278),
            transferWidth,
            Scale(34),
            TRUE);
        MoveWindow(
            dataExport_,
            contentLeft +
                inner +
                transferWidth +
                gap,
            Scale(278),
            transferWidth,
            Scale(34),
            TRUE);

        MoveWindow(
            dataMaintenanceLabel_,
            contentLeft,
            Scale(360),
            width,
            Scale(26),
            TRUE);

        const int maintenanceWidth =
            (width -
             inner * 2 -
             gap * 2) / 3;
        const int maintenanceTop =
            Scale(406);

        MoveWindow(
            dataClearUsage_,
            contentLeft + inner,
            maintenanceTop,
            maintenanceWidth,
            Scale(34),
            TRUE);
        MoveWindow(
            dataRebuildIndex_,
            contentLeft +
                inner +
                maintenanceWidth +
                gap,
            maintenanceTop,
            maintenanceWidth,
            Scale(34),
            TRUE);
        MoveWindow(
            dataResetSettings_,
            contentLeft +
                inner +
                (maintenanceWidth +
                 gap) * 2,
            maintenanceTop,
            maintenanceWidth,
            Scale(34),
            TRUE);

        MoveWindow(
            dataStatus_,
            contentLeft,
            Scale(478),
            width,
            Scale(54),
            TRUE);
    }

void SettingsWindow::LayoutAbout(int contentLeft, int contentWidth) {
        const int width =
            std::min(
                contentWidth,
                Scale(560));
        const int inner =
            Scale(18);
        const int actionWidth =
            Scale(140);

        MoveWindow(
            aboutName_,
            contentLeft,
            Scale(108),
            width,
            Scale(42),
            TRUE);

        int versionWidth =
            Scale(132);

        if (HDC dc = GetDC(hwnd_)) {
            wchar_t versionText[96]{};
            GetWindowTextW(
                aboutVersion_,
                versionText,
                static_cast<int>(
                    std::size(
                        versionText)));

            HGDIOBJ oldFont =
                SelectObject(
                    dc,
                    normalFont_);

            SIZE size{};
            if (GetTextExtentPoint32W(
                    dc,
                    versionText,
                    lstrlenW(
                        versionText),
                    &size)) {
                versionWidth =
                    size.cx;
            }

            SelectObject(
                dc,
                oldFont);
            ReleaseDC(
                hwnd_,
                dc);
        }

        const int versionRowTop =
            Scale(148);
        const int versionRowHeight =
            Scale(24);

        MoveWindow(
            aboutVersion_,
            contentLeft,
            versionRowTop,
            versionWidth + Scale(2),
            versionRowHeight,
            TRUE);

        MoveWindow(
            openGitHub_,
            contentLeft +
                versionWidth +
                Scale(14),
            versionRowTop,
            Scale(82),
            versionRowHeight,
            TRUE);

        MoveWindow(
            aboutDescription_,
            contentLeft,
            Scale(174),
            width,
            Scale(24),
            TRUE);

        MoveWindow(
            updateSectionTitle_,
            contentLeft,
            Scale(214),
            width,
            Scale(26),
            TRUE);

        const int updateTop =
            Scale(246);
        const int rowHeight =
            Scale(
                settings_layout::
                    kToggleRowLogical);

        MoveWindow(
            updateAutoCheck_,
            contentLeft + Scale(1),
            updateTop + Scale(1),
            width - Scale(2),
            rowHeight,
            TRUE);

        MoveWindow(
            updatePrerelease_,
            contentLeft + Scale(1),
            updateTop +
                rowHeight,
            width - Scale(2),
            rowHeight,
            TRUE);

        const int statusRowTop =
            updateTop +
            rowHeight * 2;
        const int actionHeight =
            Scale(34);
        const int statusHeight =
            Scale(42);
        const int rowCenterOffset =
            Scale(32);

        MoveWindow(
            updateStatus_,
            contentLeft + inner,
            statusRowTop +
                rowCenterOffset -
                statusHeight / 2,
            width -
                inner * 3 -
                actionWidth,
            statusHeight,
            TRUE);

        MoveWindow(
            updateAction_,
            contentLeft +
                width -
                inner -
                actionWidth,
            statusRowTop +
                rowCenterOffset -
                actionHeight / 2,
            actionWidth,
            actionHeight,
            TRUE);
    }

} // namespace altrun
