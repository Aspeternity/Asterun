#pragma once

#include "../core/Command.hpp"
#include "../core/SettingsLayout.hpp"

#include <windows.h>

#include <string>
#include <string_view>
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
    void ApplyLanguage();
    void RefreshFromSettings();
    void RefreshCommands();
    void OnProgramIndexRefreshCompleted(
        int outcome);
    void OnDynamicProviderStatusChanged();

private:
    enum class Page {
        Commands,
        General,
        Appearance,
        Providers,
        Data,
        About,
    };

    static constexpr int
        kSidebarWidthLogical = 190;

    static constexpr UINT
        kIdNavCommands = 51000;
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
        kIdHotkeyCtrl = 51110;
    static constexpr UINT
        kIdHotkeyAlt = 51111;
    static constexpr UINT
        kIdHotkeyShift = 51112;
    static constexpr UINT
        kIdHotkeyWin = 51113;
    static constexpr UINT
        kIdHotkeyKey = 51114;
    static constexpr UINT
        kIdHotkeyApply = 51115;
    static constexpr UINT
        kIdAuxHotkeyEnabled = 51116;
    static constexpr UINT
        kIdAuxHotkeyCtrl = 51117;
    static constexpr UINT
        kIdAuxHotkeyAlt = 51118;
    static constexpr UINT
        kIdAuxHotkeyShift = 51119;
    static constexpr UINT
        kIdAuxHotkeyWin = 51120;
    static constexpr UINT
        kIdAuxHotkeyKey = 51121;
    static constexpr UINT
        kIdAuxHotkeyApply = 51122;

    static constexpr UINT
        kIdWildcardMatching = 51130;
    static constexpr UINT
        kIdNumericQuickLaunch = 51131;
    static constexpr UINT
        kIdExecuteSingleResult = 51132;
    static constexpr UINT
        kIdNumericQuickLaunchOrder = 51133;

    static constexpr UINT
        kIdUiStyle = 51201;
    static constexpr UINT
        kIdLanguage = 51202;

    static constexpr UINT
        kIdOpenDataFolder = 51301;
    static constexpr UINT
        kIdOpenGitHub = 51302;

    static constexpr UINT
        kIdCommandSearch = 51401;
    static constexpr UINT
        kIdCommandNew = 51402;
    static constexpr UINT
        kIdCommandList = 51403;
    static constexpr UINT
        kIdCommandMoveUp = 51404;
    static constexpr UINT
        kIdCommandMoveDown = 51405;
    static constexpr UINT
        kIdCommandName = 51410;
    static constexpr UINT
        kIdCommandKeyword = 51411;
    static constexpr UINT
        kIdCommandAliases = 51412;
    static constexpr UINT
        kIdCommandType = 51413;
    static constexpr UINT
        kIdCommandTarget = 51414;
    static constexpr UINT
        kIdCommandBrowseTarget = 51415;
    static constexpr UINT
        kIdCommandArguments = 51416;
    static constexpr UINT
        kIdCommandWorkdir = 51417;
    static constexpr UINT
        kIdCommandBrowseWorkdir = 51418;
    static constexpr UINT
        kIdCommandEnabled = 51419;
    static constexpr UINT
        kIdCommandAdmin = 51420;
    static constexpr UINT
        kIdCommandPinned = 51421;
    static constexpr UINT
        kIdCommandTest = 51422;
    static constexpr UINT
        kIdCommandDelete = 51423;
    static constexpr UINT
        kIdCommandCancel = 51424;
    static constexpr UINT
        kIdCommandSave = 51425;

    static constexpr UINT
        kIdDataOpenFolder = 51501;
    static constexpr UINT
        kIdDataImportTsv = 51502;
    static constexpr UINT
        kIdDataImportLegacy = 51503;
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
    void CreateCommandPage();
    void CreateGeneralPage();
    void CreateAppearancePage();
    void CreateProviderPage();
    void CreateDataPage();
    void CreateAboutPage();
    void ApplyFonts();
    void Layout();
    void CenterOnCurrentMonitor();
    void ShowPage(Page page);
    void UpdateNavLabels();
    void UpdatePageHeader();

    void RefreshCommandList(
        std::wstring_view preferredId = {});
    void RefreshProviderStatus();
    void OpenEverythingDownloadPage();
    void RefreshDataCompatibilityStatus();
    void LoadCommandEditor(
        std::wstring_view id);
    void BeginNewCommand();
    void ClearCommandEditor();
    void SetCommandEditorEnabled(
        bool enabled);
    void MarkEditorDirty();
    bool ConfirmDiscardChanges();
    bool SaveCommandEditor();
    void DeleteEditingCommand();
    void MoveEditingCommand(
        int direction);
    void TestEditingCommand();
    void BrowseCommandTarget();
    void BrowseCommandWorkingDirectory();
    [[nodiscard]] Command
    CollectCommandEditor() const;
    [[nodiscard]] std::vector<std::wstring>
    ParseAliases(
        std::wstring_view text) const;

    void ToggleGeneralSetting(
        UINT id);
    void ToggleProviderSetting(
        UINT id);
    void ApplyMonitorControl();
    void ApplyHotkeyControl();
    void ApplyAuxiliaryHotkeyControl();
    void ApplyClassicBehaviorControl(
        UINT id = 0);
    void RefreshHotkeyControls();
    void ApplyAppearanceControls();
    void ImportCommands(
        bool legacyMode);
    void ExportCommands();
    void ClearUsageHistory();
    void RebuildProgramIndex();
    void RestoreDefaultSettings();
    void DrawGeneralToggle(
        const DRAWITEMSTRUCT& item);
    void DrawNavigationButton(
        const DRAWITEMSTRUCT& item);

    HWND CreateStatic(
        const wchar_t* text,
        DWORD style = SS_LEFT,
        DWORD exStyle = 0);

    HWND CreateButton(
        const wchar_t* text,
        UINT id,
        DWORD style =
            BS_PUSHBUTTON | BS_FLAT);

    HWND CreateCheckboxRow(
        const wchar_t* text,
        UINT id);

    HWND CreateCheckbox(
        const wchar_t* text,
        UINT id);

    HWND CreateEdit(
        UINT id,
        DWORD style =
            ES_AUTOHSCROLL);

    [[nodiscard]] std::wstring
    ControlText(HWND control) const;
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
    MonitorCardRect() const;
    [[nodiscard]] RECT
    ProviderCardRect() const;

    int Scale(int value) const;
    const wchar_t* T(
        const wchar_t* zh,
        const wchar_t* en) const;

    App& app_;
    HINSTANCE instance_{};
    HWND hwnd_{};

    HWND navCommands_{};
    HWND navGeneral_{};
    HWND navAppearance_{};
    HWND navProviders_{};
    HWND navData_{};
    HWND navAbout_{};
    HWND pageTitle_{};
    HWND pageDescription_{};

    HWND commandSearch_{};
    HWND commandNew_{};
    HWND commandList_{};
    HWND commandMoveUp_{};
    HWND commandMoveDown_{};
    HWND commandEditorTitle_{};
    HWND commandNameLabel_{};
    HWND commandName_{};
    HWND commandKeywordLabel_{};
    HWND commandKeyword_{};
    HWND commandAliasesLabel_{};
    HWND commandAliases_{};
    HWND commandTypeLabel_{};
    HWND commandType_{};
    HWND commandTargetLabel_{};
    HWND commandTarget_{};
    HWND commandBrowseTarget_{};
    HWND commandArgumentsLabel_{};
    HWND commandArguments_{};
    HWND commandWorkdirLabel_{};
    HWND commandWorkdir_{};
    HWND commandBrowseWorkdir_{};
    HWND commandEnabled_{};
    HWND commandAdmin_{};
    HWND commandPinned_{};
    HWND commandTest_{};
    HWND commandDelete_{};
    HWND commandCancel_{};
    HWND commandSave_{};
    HWND commandStatus_{};

    HWND generalBehaviorTitle_{};
    HWND startWithWindows_{};
    HWND showOnStartup_{};
    HWND hideAfterLaunch_{};
    HWND clearQueryOnShow_{};
    HWND hideOnFocusLost_{};
    HWND showTrayIcon_{};
    HWND searchBehaviorTitle_{};
    HWND wildcardMatching_{};
    HWND numericQuickLaunch_{};
    HWND executeSingleResult_{};
    HWND numericQuickLaunchOrderLabel_{};
    HWND numericQuickLaunchOrder_{};

    HWND hotkeySectionTitle_{};
    HWND primaryHotkeyLabel_{};
    HWND hotkeyCtrl_{};
    HWND hotkeyAlt_{};
    HWND hotkeyShift_{};
    HWND hotkeyWin_{};
    HWND hotkeyKey_{};
    HWND hotkeyApply_{};
    HWND hotkeyStatus_{};

    HWND auxiliaryHotkeyEnabled_{};
    HWND auxiliaryHotkeyCtrl_{};
    HWND auxiliaryHotkeyAlt_{};
    HWND auxiliaryHotkeyShift_{};
    HWND auxiliaryHotkeyWin_{};
    HWND auxiliaryHotkeyKey_{};
    HWND auxiliaryHotkeyApply_{};
    HWND auxiliaryHotkeyStatus_{};

    HWND popupSectionTitle_{};
    HWND popupMonitorLabel_{};
    HWND popupMonitorDescription_{};
    HWND popupMonitor_{};
    HWND generalNote_{};

    HWND uiStyleLabel_{};
    HWND uiStyle_{};
    HWND languageLabel_{};
    HWND language_{};
    HWND appearanceNote_{};

    HWND providerSectionTitle_{};
    HWND providerStartMenu_{};
    HWND providerPackaged_{};
    HWND providerAppPaths_{};
    HWND providerPath_{};
    HWND providerEverything_{};
    HWND providerStatus_{};
    HWND providerGetEverything_{};
    HWND providerRecheckEverything_{};
    HWND providerNote_{};

    HWND dataOpenLabel_{};
    HWND dataOpenFolder_{};
    HWND dataTransferLabel_{};
    HWND dataImportTsv_{};
    HWND dataImportLegacy_{};
    HWND dataExport_{};
    HWND dataMaintenanceLabel_{};
    HWND dataClearUsage_{};
    HWND dataRebuildIndex_{};
    HWND dataResetSettings_{};
    HWND dataStatus_{};

    HWND aboutName_{};
    HWND aboutVersion_{};
    HWND aboutDescription_{};
    HWND dataPathLabel_{};
    HWND dataPath_{};
    HWND openDataFolder_{};
    HWND openGitHub_{};

    HFONT normalFont_{};
    HFONT titleFont_{};
    HFONT appNameFont_{};
    HFONT sectionFont_{};
    HBRUSH backgroundBrush_{};
    HBRUSH sidebarBrush_{};
    HBRUSH cardBrush_{};

    UINT dpi_{96};
    Page page_{Page::Commands};
    bool syncing_{false};
    bool editingNew_{false};
    bool editorDirty_{false};
    int generalScrollOffset_{0};
    std::wstring editingCommandId_;
    std::vector<std::wstring>
        filteredCommandIds_;

    std::vector<HWND>
        commandControls_;
    std::vector<HWND>
        generalControls_;
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
