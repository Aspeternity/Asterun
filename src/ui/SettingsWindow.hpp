#pragma once

#include "../core/SettingsLayout.hpp"
#include "UiMetrics.hpp"

#include <windows.h>

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace altrun {

class App;

class SettingsWindow {
public:
    SettingsWindow(
        App& app,
        HINSTANCE instance);
    ~SettingsWindow();

    bool Create();
    void Show();
    void ShowAbout();
    void ApplyLanguage();
    void RefreshFromSettings();
    void OnProgramIndexRefreshCompleted(
        int outcome);
    void OnDynamicProviderStatusChanged();
    void OnUpdateStatusChanged();

private:
    enum class Page {
        General,
        Hotkeys,
        Appearance,
        Providers,
        Data,
        About,
    };

    static constexpr int
        kSidebarWidthLogical =
            ui::kSettingsSidebarWidthLogical;

    static constexpr UINT
        kIdNavGeneral = 51001;
    static constexpr UINT
        kIdNavAppearance = 51002;
    static constexpr UINT
        kIdNavData = 51003;
    static constexpr UINT
        kIdNavAbout = 51004;
    static constexpr UINT
        kIdNavProviders = 51005;
    static constexpr UINT
        kIdNavHotkeys = 51006;

    static constexpr UINT
        kIdStartWithWindows = 51100;
    static constexpr UINT
        kIdHideAfterLaunch = 51101;
    static constexpr UINT
        kIdClearQueryOnShow = 51102;
    static constexpr UINT
        kIdHideOnFocusLost = 51103;
    static constexpr UINT
        kIdShowTrayIcon = 51104;
    static constexpr UINT
        kIdPopupMonitor = 51105;
    static constexpr UINT
        kIdShowOnStartup = 51106;
    static constexpr UINT
        kIdLauncherPlacement = 51107;
    static constexpr UINT
        kIdSettingsPlacement = 51108;

    static constexpr UINT
        kIdWildcardMatching = 51130;

    static constexpr UINT
        kIdHotkeyResetAll = 51705;
    static constexpr UINT
        kIdHotkeyCaptureBase = 51720;
    static constexpr UINT
        kIdHotkeyEnabledBase = 51740;
    static constexpr UINT
        kIdHotkeyResetBase = 51760;
    static constexpr UINT
        kIdNumericQuickLaunch = 51131;
    static constexpr UINT
        kIdExecuteSingleResult = 51132;
    static constexpr UINT
        kIdNumericQuickLaunchOrder = 51133;
    static constexpr UINT
        kIdPinyinSearch = 51134;

    static constexpr UINT
        kIdUiStyle = 51201;
    static constexpr UINT
        kIdLanguage = 51202;
    static constexpr UINT
        kIdShowResultIcons = 51203;

    static constexpr UINT
        kIdOpenDataFolder = 51301;
    static constexpr UINT
        kIdOpenGitHub = 51302;
    static constexpr UINT
        kIdUpdatePrerelease = 51303;
    static constexpr UINT
        kIdUpdateAutoCheck = 51304;
    static constexpr UINT
        kIdUpdateAction = 51305;

    static constexpr UINT
        kIdDataImportTsv = 51502;
    static constexpr UINT
        kIdDataExport = 51504;
    static constexpr UINT
        kIdDataClearUsage = 51505;
    static constexpr UINT
        kIdDataRebuildIndex = 51506;
    static constexpr UINT
        kIdDataResetSettings = 51507;

    static constexpr UINT
        kIdProviderStartMenu = 51601;
    static constexpr UINT
        kIdProviderPackaged = 51602;
    static constexpr UINT
        kIdProviderAppPaths = 51603;
    static constexpr UINT
        kIdProviderPath = 51604;
    static constexpr UINT
        kIdProviderEverything = 51605;
    static constexpr UINT
        kIdProviderGetEverything = 51606;
    static constexpr UINT
        kIdProviderRecheckEverything = 51607;

    static constexpr UINT_PTR
        kProviderStatusTimerId = 0x51690;
    static constexpr UINT_PTR
        kProviderCommitTimerId = 0x51691;

    static LRESULT CALLBACK WindowProc(
        HWND hwnd,
        UINT message,
        WPARAM wParam,
        LPARAM lParam);

    LRESULT HandleMessage(
        UINT message,
        WPARAM wParam,
        LPARAM lParam);

    void CreateControls();
    void CreateGeneralPage();
    void CreateHotkeyPage();
    void CreateAppearancePage();
    void CreateProviderPage();
    void CreateDataPage();
    void CreateAboutPage();
    void ApplyFonts();
    void Layout();
    void PositionForShow();
    void ShowPage(Page page);
    void UpdateNavLabels();
    void UpdatePageHeader();

    void RefreshProviderStatus();
    void CommitPendingProviderChanges();
    void AcquireEverything();
    void RecheckEverything();
    void RefreshDataCompatibilityStatus();
    void RefreshUpdateStatus();
    void TogglePrereleaseUpdates();

    void ToggleGeneralSetting(
        UINT id);
    void ToggleProviderSetting(
        UINT id);
    void ApplyMonitorControl();
    void ApplyWindowPlacementControls();
    void ApplyClassicBehaviorControl(
        UINT id = 0);
    struct HotkeyRowControls {
        std::string actionId;
        HWND title{};
        HWND capture{};
        HWND enabled{};
        HWND reset{};
        HWND status{};
    };

    void RefreshHotkeyPage(
        bool relayout = true);
    void RelayoutHotkeyPage();
    void BeginHotkeyCapture(
        std::string_view actionId);
    void CancelHotkeyCapture(
        bool refresh = true);
    void ApplyCapturedHotkey(
        UINT virtualKey);
    void ToggleHotkeyActionEnabled(
        std::string_view actionId);
    void ResetHotkeyAction(
        std::string_view actionId);
    void ResetAllHotkeys();
    [[nodiscard]] HotkeyRowControls*
    FindHotkeyRow(
        std::string_view actionId);
    [[nodiscard]] const HotkeyRowControls*
    FindHotkeyRow(
        std::string_view actionId) const;
    [[nodiscard]] HotkeyRowControls*
    HotkeyRowFromControlId(
        UINT id,
        UINT baseId);
    void SetHotkeyRowStatus(
        std::string_view actionId,
        std::wstring_view status);
    [[nodiscard]] bool
    HotkeyRowHasAuxiliaryContent(
        const HotkeyRowControls& row) const;
    [[nodiscard]] int
    HotkeyAuxiliaryHeight(
        const HotkeyRowControls& row) const;
    [[nodiscard]] int
    HotkeyRowHeight(
        const HotkeyRowControls& row) const;
    [[nodiscard]] int
    HotkeyGroupHeight(
        bool global) const;
    [[nodiscard]] std::wstring
    HotkeyActionLabel(
        std::string_view actionId) const;
    [[nodiscard]] std::wstring
    HotkeyActionDescription(
        std::string_view actionId) const;
    [[nodiscard]] std::wstring
    FormatHotkeyBinding(
        std::string_view actionId) const;
    void ApplyAppearanceControls();
    void ImportCommands();
    void ExportCommands();
    void ClearUsageHistory();
    void RebuildProgramIndex();
    void RestoreDefaultSettings();
    void DrawGeneralToggle(
        const DRAWITEMSTRUCT& item);
    void DrawNavigationButton(
        const DRAWITEMSTRUCT& item);
    void DrawActionButton(
        const DRAWITEMSTRUCT& item);
    void DrawHotkeyResetLink(
        const DRAWITEMSTRUCT& item);
    void DrawGitHubLink(
        const DRAWITEMSTRUCT& item);
    void DrawUpdateStatus(
        const DRAWITEMSTRUCT& item);
    void DrawHotkeyToggle(
        const DRAWITEMSTRUCT& item,
        std::size_t rowIndex);
    void DrawSwitchGlyph(
        HDC dc,
        const RECT& rect,
        bool checked,
        bool pressed);

    HWND CreateStatic(
        const wchar_t* text,
        DWORD style = SS_LEFT,
        DWORD exStyle = 0);

    HWND CreateButton(
        const wchar_t* text,
        UINT id,
        DWORD style =
            BS_OWNERDRAW);

    HWND CreateCheckboxRow(
        const wchar_t* text,
        UINT id);

    HWND CreateCheckbox(
        const wchar_t* text,
        UINT id);

    [[nodiscard]] bool
    ToggleChecked(UINT id) const;
    [[nodiscard]]
    settings_layout::GeneralLayoutMetrics
    BuildGeneralLayout(
        int scrollOffset) const;
    void UpdateGeneralScrollBar();
    void ScrollGeneral(int delta);

    [[nodiscard]] RECT
    BehaviorCardRect() const;
    [[nodiscard]] RECT
    SearchBehaviorCardRect() const;
    [[nodiscard]] RECT
    PlacementCardRect() const;
    [[nodiscard]] RECT
    ProviderCardRect() const;
    [[nodiscard]] RECT
    PageCardRect(
        int topLogical,
        int heightLogical,
        int maxWidthLogical = 720) const;

    int Scale(int value) const;
    const wchar_t* T(
        const wchar_t* zh,
        const wchar_t* en) const;

    App& app_;
    HINSTANCE instance_{};
    HWND hwnd_{};

    HWND navGeneral_{};
    HWND navHotkeys_{};
    HWND navAppearance_{};
    HWND navProviders_{};
    HWND navData_{};
    HWND navAbout_{};
    HWND brandName_{};
    HWND brandSubtitle_{};
    HWND pageTitle_{};
    HWND pageDescription_{};

    HWND generalBehaviorTitle_{};
    HWND startWithWindows_{};
    HWND showOnStartup_{};
    HWND hideAfterLaunch_{};
    HWND clearQueryOnShow_{};
    HWND hideOnFocusLost_{};
    HWND showTrayIcon_{};
    HWND showResultIcons_{};
    HWND searchBehaviorTitle_{};
    HWND pinyinSearch_{};
    HWND wildcardMatching_{};
    HWND numericQuickLaunch_{};
    HWND executeSingleResult_{};
    HWND numericQuickLaunchOrderLabel_{};
    HWND numericQuickLaunchOrder_{};

    HWND hotkeyGlobalTitle_{};
    HWND hotkeyLauncherTitle_{};
    HWND hotkeyResetAll_{};
    std::vector<HotkeyRowControls>
        hotkeyRows_{};

    HWND placementSectionTitle_{};
    HWND popupMonitorLabel_{};
    HWND popupMonitorDescription_{};
    HWND popupMonitor_{};
    HWND launcherPlacementLabel_{};
    HWND launcherPlacementDescription_{};
    HWND launcherPlacement_{};
    HWND settingsPlacementLabel_{};
    HWND settingsPlacementDescription_{};
    HWND settingsPlacement_{};
    HWND generalNote_{};

    HWND appearanceLauncherTitle_{};
    HWND uiStyleLabel_{};
    HWND uiStyle_{};
    HWND appearanceAppTitle_{};
    HWND languageLabel_{};
    HWND language_{};
    HWND appearanceNote_{};

    HWND providerSectionTitle_{};
    HWND providerFilesTitle_{};
    HWND providerStartMenu_{};
    HWND providerPackaged_{};
    HWND providerAppPaths_{};
    HWND providerPath_{};
    HWND providerEverything_{};
    HWND providerStatus_{};
    HWND providerGetEverything_{};
    HWND providerRecheckEverything_{};
    HWND providerNote_{};

    HWND dataPathLabel_{};
    HWND dataPath_{};
    HWND openDataFolder_{};
    HWND dataTransferLabel_{};
    HWND dataImportTsv_{};
    HWND dataExport_{};
    HWND dataMaintenanceLabel_{};
    HWND dataClearUsage_{};
    HWND dataRebuildIndex_{};
    HWND dataResetSettings_{};
    HWND dataStatus_{};

    HWND aboutName_{};
    HWND aboutVersion_{};
    HWND aboutDescription_{};
    HWND updateSectionTitle_{};
    HWND updateAutoCheck_{};
    HWND updatePrerelease_{};
    HWND updateStatus_{};
    HWND updateAction_{};
    HWND openGitHub_{};

    HFONT normalFont_{};
    HFONT titleFont_{};
    HFONT appNameFont_{};
    HFONT sectionFont_{};
    HBRUSH backgroundBrush_{};
    HBRUSH sidebarBrush_{};
    HBRUSH cardBrush_{};

    UINT dpi_{96};
    Page page_{Page::General};
    bool syncing_{false};
    int generalScrollOffset_{0};
    std::string capturingHotkeyActionId_;
    std::unordered_map<std::string, bool>
        pendingProviderStates_;
    bool providerCommitInProgress_{false};

    std::vector<HWND>
        generalControls_;
    std::vector<HWND>
        hotkeyControls_;
    std::vector<HWND>
        appearanceControls_;
    std::vector<HWND>
        providerControls_;
    std::vector<HWND>
        dataControls_;
    std::vector<HWND>
        aboutControls_;
};

} // namespace altrun
