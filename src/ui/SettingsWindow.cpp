#include "SettingsWindow.hpp"

#include "UiTheme.hpp"
#include "UiTypography.hpp"

#include "../app/App.hpp"
#include "../core/HotkeyRegistry.hpp"
#include "../core/LauncherActionPolicy.hpp"
#include "../platform/Hotkey.hpp"
#include "Version.hpp"

#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <shlobj.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <iterator>
#include <sstream>
#include <string>

namespace altrun {

namespace {

constexpr wchar_t kSettingsClass[] = L"ALTRunNext.Settings";
constexpr wchar_t kSettingsTitle[] = L"ALTRun Next Settings";

constexpr const auto& kPalette =
    ui::kApplicationPalette;
constexpr COLORREF kWindowBackground =
    kPalette.windowBackground;
constexpr COLORREF kSidebarBackground =
    kPalette.sidebarBackground;
constexpr COLORREF kCardBackground =
    kPalette.cardBackground;
constexpr COLORREF kCardPressed =
    kPalette.pressedBackground;
constexpr COLORREF kBorder =
    kPalette.frame;
constexpr COLORREF kText =
    kPalette.text;
constexpr COLORREF kMuted =
    kPalette.mutedText;
constexpr COLORREF kAccent =
    kPalette.accent;

std::wstring FormatBytes(
    std::uint64_t bytes) {
    constexpr double kKiB = 1024.0;
    constexpr double kMiB =
        1024.0 * 1024.0;

    std::wostringstream out;
    out << std::fixed;

    if (bytes >=
        static_cast<std::uint64_t>(
            kMiB)) {
        out << std::setprecision(1)
            << (static_cast<double>(
                    bytes) /
                kMiB)
            << L" MB";
    } else {
        out << std::setprecision(0)
            << (static_cast<double>(
                    bytes) /
                kKiB)
            << L" KB";
    }

    return out.str();
}

} // namespace

SettingsWindow::SettingsWindow(App& app, HINSTANCE instance)
    : app_(app), instance_(instance) {}

SettingsWindow::~SettingsWindow() {
    if (normalFont_) DeleteObject(normalFont_);
    if (titleFont_) DeleteObject(titleFont_);
    if (appNameFont_) DeleteObject(appNameFont_);
    if (sectionFont_) DeleteObject(sectionFont_);
    if (backgroundBrush_) DeleteObject(backgroundBrush_);
    if (sidebarBrush_) DeleteObject(sidebarBrush_);
    if (cardBrush_) DeleteObject(cardBrush_);
}

const wchar_t* SettingsWindow::T(
    const wchar_t* zh,
    const wchar_t* en) const {

    return app_.SettingsData().language == Language::ZhCN ? zh : en;
}

int SettingsWindow::Scale(int value) const {
    return ui::Scale(
        value,
        dpi_);
}

bool SettingsWindow::Create() {
    INITCOMMONCONTROLSEX controls{
        sizeof(controls),
        ICC_STANDARD_CLASSES | ICC_WIN95_CLASSES
    };
    InitCommonControlsEx(&controls);

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.hInstance = instance_;
    wc.lpfnWndProc = WindowProc;
    wc.lpszClassName = kSettingsClass;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    wc.hbrBackground = nullptr;

    if (!RegisterClassExW(&wc) &&
        GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return false;
    }

    hwnd_ = CreateWindowExW(
        WS_EX_APPWINDOW,
        kSettingsClass,
        kSettingsTitle,
        WS_OVERLAPPEDWINDOW |
            WS_CLIPCHILDREN |
            WS_VSCROLL,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        1080,
        800,
        nullptr,
        nullptr,
        instance_,
        this);

    if (!hwnd_) return false;

    dpi_ = GetDpiForWindow(hwnd_);

    SetWindowPos(
        hwnd_,
        nullptr,
        0,
        0,
        Scale(1080),
        Scale(800),
        SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);

    ShowScrollBar(
        hwnd_,
        SB_VERT,
        FALSE);

    backgroundBrush_ = CreateSolidBrush(kWindowBackground);
    sidebarBrush_ = CreateSolidBrush(kSidebarBackground);
    cardBrush_ = CreateSolidBrush(kCardBackground);

    CreateControls();
    ApplyFonts();
    ApplyLanguage();
    RefreshFromSettings();
    ShowPage(Page::General);
    Layout();
    ShowWindow(hwnd_, SW_HIDE);

    return true;
}

HWND SettingsWindow::CreateStatic(
    const wchar_t* text,
    DWORD style,
    DWORD exStyle) {

    return CreateWindowExW(
        exStyle,
        L"STATIC",
        text,
        WS_CHILD | WS_VISIBLE | style,
        0, 0, 0, 0,
        hwnd_,
        nullptr,
        instance_,
        nullptr);
}

HWND SettingsWindow::CreateButton(
    const wchar_t* text,
    UINT id,
    DWORD style) {

    return CreateWindowExW(
        0,
        L"BUTTON",
        text,
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | style,
        0, 0, 0, 0,
        hwnd_,
        reinterpret_cast<HMENU>(
            static_cast<UINT_PTR>(id)),
        instance_,
        nullptr);
}

HWND SettingsWindow::CreateCheckboxRow(
    const wchar_t* text,
    UINT id) {

    return CreateButton(
        text,
        id,
        BS_OWNERDRAW);
}

HWND SettingsWindow::CreateCheckbox(
    const wchar_t* text,
    UINT id) {

    return CreateButton(
        text,
        id,
        BS_AUTOCHECKBOX | BS_FLAT);
}

void SettingsWindow::CreateControls() {
    brandName_ =
        CreateStatic(L"ALTRun Next");
    brandSubtitle_ =
        CreateStatic(L"Settings");

    navGeneral_ =
        CreateButton(
            L"",
            kIdNavGeneral,
            BS_OWNERDRAW);
    navHotkeys_ =
        CreateButton(
            L"",
            kIdNavHotkeys,
            BS_OWNERDRAW);
    navProviders_ =
        CreateButton(
            L"",
            kIdNavProviders,
            BS_OWNERDRAW);
    navAppearance_ =
        CreateButton(
            L"",
            kIdNavAppearance,
            BS_OWNERDRAW);
    navData_ =
        CreateButton(
            L"",
            kIdNavData,
            BS_OWNERDRAW);
    navDiagnostics_ =
        CreateButton(
            L"",
            kIdNavDiagnostics,
            BS_OWNERDRAW);
    navAbout_ =
        CreateButton(
            L"",
            kIdNavAbout,
            BS_OWNERDRAW);

    pageTitle_ =
        CreateStatic(
            L"",
            SS_LEFT | SS_NOPREFIX);
    pageDescription_ =
        CreateStatic(
            L"",
            SS_LEFT | SS_NOPREFIX);

    CreateGeneralPage();
    CreateHotkeyPage();
    CreateProviderPage();
    CreateAppearancePage();
    CreateDataPage();
    CreateDiagnosticsPage();
    CreateAboutPage();
}

void SettingsWindow::CreateGeneralPage() {
    generalBehaviorTitle_ =
        CreateStatic(L"");

    startWithWindows_ =
        CreateCheckboxRow(
            L"",
            kIdStartWithWindows);
    showOnStartup_ =
        CreateCheckboxRow(
            L"",
            kIdShowOnStartup);
    hideAfterLaunch_ =
        CreateCheckboxRow(
            L"",
            kIdHideAfterLaunch);
    clearQueryOnShow_ =
        CreateCheckboxRow(
            L"",
            kIdClearQueryOnShow);
    hideOnFocusLost_ =
        CreateCheckboxRow(
            L"",
            kIdHideOnFocusLost);
    showTrayIcon_ =
        CreateCheckboxRow(
            L"",
            kIdShowTrayIcon);
    showResultIcons_ =
        CreateCheckboxRow(
            L"",
            kIdShowResultIcons);

    searchBehaviorTitle_ =
        CreateStatic(L"");

    pinyinSearch_ =
        CreateCheckboxRow(
            L"",
            kIdPinyinSearch);
    wildcardMatching_ =
        CreateCheckboxRow(
            L"",
            kIdWildcardMatching);
    numericQuickLaunch_ =
        CreateCheckboxRow(
            L"",
            kIdNumericQuickLaunch);
    executeSingleResult_ =
        CreateCheckboxRow(
            L"",
            kIdExecuteSingleResult);

    numericQuickLaunchOrderLabel_ =
        CreateStatic(L"");

    numericQuickLaunchOrder_ =
        CreateWindowExW(
            0,
            L"COMBOBOX",
            L"",
            WS_CHILD | WS_VISIBLE |
                WS_TABSTOP |
                CBS_DROPDOWNLIST |
                WS_VSCROLL,
            0, 0, 0, 0,
            hwnd_,
            reinterpret_cast<HMENU>(
                static_cast<UINT_PTR>(
                    kIdNumericQuickLaunchOrder)),
            instance_,
            nullptr);

    placementSectionTitle_ =
        CreateStatic(L"");

    popupMonitorLabel_ =
        CreateStatic(L"");
    popupMonitorDescription_ =
        CreateStatic(
            L"",
            SS_LEFT | SS_NOPREFIX);
    popupMonitor_ =
        CreateWindowExW(
            0,
            L"COMBOBOX",
            L"",
            WS_CHILD | WS_VISIBLE |
                WS_TABSTOP |
                CBS_DROPDOWNLIST |
                WS_VSCROLL,
            0, 0, 0, 0,
            hwnd_,
            reinterpret_cast<HMENU>(
                static_cast<UINT_PTR>(
                    kIdPopupMonitor)),
            instance_,
            nullptr);

    launcherPlacementLabel_ =
        CreateStatic(L"");
    launcherPlacementDescription_ =
        CreateStatic(
            L"",
            SS_LEFT | SS_NOPREFIX);
    launcherPlacement_ =
        CreateWindowExW(
            0,
            L"COMBOBOX",
            L"",
            WS_CHILD | WS_VISIBLE |
                WS_TABSTOP |
                CBS_DROPDOWNLIST |
                WS_VSCROLL,
            0, 0, 0, 0,
            hwnd_,
            reinterpret_cast<HMENU>(
                static_cast<UINT_PTR>(
                    kIdLauncherPlacement)),
            instance_,
            nullptr);

    settingsPlacementLabel_ =
        CreateStatic(L"");
    settingsPlacementDescription_ =
        CreateStatic(
            L"",
            SS_LEFT | SS_NOPREFIX);
    settingsPlacement_ =
        CreateWindowExW(
            0,
            L"COMBOBOX",
            L"",
            WS_CHILD | WS_VISIBLE |
                WS_TABSTOP |
                CBS_DROPDOWNLIST |
                WS_VSCROLL,
            0, 0, 0, 0,
            hwnd_,
            reinterpret_cast<HMENU>(
                static_cast<UINT_PTR>(
                    kIdSettingsPlacement)),
            instance_,
            nullptr);

    generalNote_ =
        CreateStatic(
            L"",
            SS_LEFT | SS_NOPREFIX);

    generalControls_ = {
        generalBehaviorTitle_,
        startWithWindows_,
        showOnStartup_,
        hideAfterLaunch_,
        clearQueryOnShow_,
        hideOnFocusLost_,
        showTrayIcon_,
        showResultIcons_,
        searchBehaviorTitle_,
        pinyinSearch_,
        wildcardMatching_,
        numericQuickLaunch_,
        executeSingleResult_,
        numericQuickLaunchOrderLabel_,
        numericQuickLaunchOrder_,
        placementSectionTitle_,
        popupMonitorLabel_,
        popupMonitorDescription_,
        popupMonitor_,
        launcherPlacementLabel_,
        launcherPlacementDescription_,
        launcherPlacement_,
        settingsPlacementLabel_,
        settingsPlacementDescription_,
        settingsPlacement_,
        generalNote_,
    };
}

void SettingsWindow::CreateHotkeyPage() {
    hotkeyActionList_ =
        CreateWindowExW(
            0,
            L"LISTBOX",
            L"",
            WS_CHILD | WS_VISIBLE |
                WS_TABSTOP | WS_VSCROLL |
                LBS_NOTIFY |
                LBS_OWNERDRAWFIXED |
                LBS_HASSTRINGS |
                LBS_NOINTEGRALHEIGHT,
            0, 0, 0, 0,
            hwnd_,
            reinterpret_cast<HMENU>(
                static_cast<UINT_PTR>(
                    kIdHotkeyActionList)),
            instance_,
            nullptr);

    hotkeyEditorTitle_ =
        CreateStatic(
            L"",
            SS_LEFT | SS_NOPREFIX);
    hotkeyEditorDescription_ =
        CreateStatic(
            L"",
            SS_LEFT | SS_NOPREFIX);
    hotkeyScope_ =
        CreateStatic(
            L"",
            SS_LEFT | SS_NOPREFIX);

    hotkeyEnabled_ =
        CreateCheckbox(
            L"",
            kIdHotkeyEnabled);

    hotkeyCapture_ =
        CreateButton(
            L"",
            kIdHotkeyCapture);

    hotkeyResetCurrent_ =
        CreateButton(
            L"",
            kIdHotkeyResetCurrent);

    hotkeyResetAll_ =
        CreateButton(
            L"",
            kIdHotkeyResetAll);

    hotkeyPageStatus_ =
        CreateStatic(
            L"",
            SS_LEFT | SS_NOPREFIX);

    hotkeyPageNote_ =
        CreateStatic(
            L"",
            SS_LEFT | SS_NOPREFIX);

    hotkeyControls_ = {
        hotkeyActionList_,
        hotkeyEditorTitle_,
        hotkeyEditorDescription_,
        hotkeyScope_,
        hotkeyEnabled_,
        hotkeyCapture_,
        hotkeyResetCurrent_,
        hotkeyResetAll_,
        hotkeyPageStatus_,
        hotkeyPageNote_,
    };
}

void SettingsWindow::CreateDiagnosticsPage() {
    diagnosticsMemoryTitle_ =
        CreateStatic(L"");
    diagnosticsMemoryStatus_ =
        CreateStatic(
            L"",
            SS_LEFT | SS_NOPREFIX);

    diagnosticsSearchTitle_ =
        CreateStatic(L"");
    diagnosticsSearchStatus_ =
        CreateStatic(
            L"",
            SS_LEFT | SS_NOPREFIX);

    actionsWindowsTitle_ = CreateStatic(L"");
    actionsWindowsStatus_ =
        CreateStatic(L"", SS_LEFT | SS_NOPREFIX);
    actionsClipboardTitle_ = CreateStatic(L"");
    actionsClipboardStatus_ =
        CreateStatic(L"", SS_LEFT | SS_NOPREFIX);
    actionsWebTitle_ = CreateStatic(L"");
    actionsWebStatus_ =
        CreateStatic(L"", SS_LEFT | SS_NOPREFIX);
    actionsNote_ =
        CreateStatic(L"", SS_LEFT | SS_NOPREFIX);

    diagnosticsControls_ = {
        diagnosticsMemoryTitle_,
        diagnosticsMemoryStatus_,
        diagnosticsSearchTitle_,
        diagnosticsSearchStatus_,
        actionsWindowsTitle_,
        actionsWindowsStatus_,
        actionsClipboardTitle_,
        actionsClipboardStatus_,
        actionsWebTitle_,
        actionsWebStatus_,
        actionsNote_,
    };
}

void SettingsWindow::CreateAppearancePage() {
    appearanceLauncherTitle_ =
        CreateStatic(L"");

    uiStyleLabel_ =
        CreateStatic(L"");

    uiStyle_ =
        CreateWindowExW(
            0,
            L"COMBOBOX",
            L"",
            WS_CHILD | WS_VISIBLE |
                WS_TABSTOP |
                CBS_DROPDOWNLIST |
                WS_VSCROLL,
            0, 0, 0, 0,
            hwnd_,
            reinterpret_cast<HMENU>(
                static_cast<UINT_PTR>(
                    kIdUiStyle)),
            instance_,
            nullptr);

    appearanceAppTitle_ =
        CreateStatic(L"");

    languageLabel_ =
        CreateStatic(L"");

    language_ =
        CreateWindowExW(
            0,
            L"COMBOBOX",
            L"",
            WS_CHILD | WS_VISIBLE |
                WS_TABSTOP |
                CBS_DROPDOWNLIST |
                WS_VSCROLL,
            0, 0, 0, 0,
            hwnd_,
            reinterpret_cast<HMENU>(
                static_cast<UINT_PTR>(
                    kIdLanguage)),
            instance_,
            nullptr);

    appearanceNote_ =
        CreateStatic(
            L"",
            SS_LEFT | SS_NOPREFIX);

    appearanceControls_ = {
        appearanceLauncherTitle_,
        uiStyleLabel_,
        uiStyle_,
        appearanceAppTitle_,
        languageLabel_,
        language_,
        appearanceNote_,
    };
}

void SettingsWindow::CreateProviderPage() {
    providerSectionTitle_ =
        CreateStatic(L"");
    providerFilesTitle_ =
        CreateStatic(L"");

    providerStartMenu_ =
        CreateCheckboxRow(
            L"",
            kIdProviderStartMenu);
    providerPackaged_ =
        CreateCheckboxRow(
            L"",
            kIdProviderPackaged);
    providerAppPaths_ =
        CreateCheckboxRow(
            L"",
            kIdProviderAppPaths);
    providerPath_ =
        CreateCheckboxRow(
            L"",
            kIdProviderPath);
    providerEverything_ =
        CreateCheckboxRow(
            L"",
            kIdProviderEverything);

    providerStatus_ =
        CreateStatic(
            L"",
            SS_LEFT | SS_NOPREFIX);

    providerGetEverything_ =
        CreateButton(
            L"",
            kIdProviderGetEverything);
    providerRecheckEverything_ =
        CreateButton(
            L"",
            kIdProviderRecheckEverything);

    providerNote_ =
        CreateStatic(
            L"",
            SS_LEFT | SS_NOPREFIX);

    providerControls_ = {
        providerSectionTitle_,
        providerFilesTitle_,
        providerStartMenu_,
        providerPackaged_,
        providerAppPaths_,
        providerPath_,
        providerEverything_,
        providerStatus_,
        providerGetEverything_,
        providerRecheckEverything_,
        providerNote_,
    };
}

void SettingsWindow::CreateDataPage() {
    dataPathLabel_ =
        CreateStatic(L"");
    dataPath_ =
        CreateStatic(
            L"",
            SS_LEFT |
                SS_PATHELLIPSIS |
                SS_NOPREFIX);
    openDataFolder_ =
        CreateButton(
            L"",
            kIdOpenDataFolder);

    dataTransferLabel_ =
        CreateStatic(L"");
    dataImportTsv_ =
        CreateButton(
            L"",
            kIdDataImportTsv);
    dataImportLegacy_ =
        CreateButton(
            L"",
            kIdDataImportLegacy);
    dataExport_ =
        CreateButton(
            L"",
            kIdDataExport);

    dataMaintenanceLabel_ =
        CreateStatic(L"");
    dataClearUsage_ =
        CreateButton(
            L"",
            kIdDataClearUsage);
    dataRebuildIndex_ =
        CreateButton(
            L"",
            kIdDataRebuildIndex);
    dataResetSettings_ =
        CreateButton(
            L"",
            kIdDataResetSettings);

    dataStatus_ =
        CreateStatic(
            L"",
            SS_LEFT | SS_NOPREFIX);

    dataControls_ = {
        dataPathLabel_,
        dataPath_,
        openDataFolder_,
        dataTransferLabel_,
        dataImportTsv_,
        dataImportLegacy_,
        dataExport_,
        dataMaintenanceLabel_,
        dataClearUsage_,
        dataRebuildIndex_,
        dataResetSettings_,
        dataStatus_,
    };
}

void SettingsWindow::CreateAboutPage() {
    aboutName_ =
        CreateStatic(
            L"ALTRun Next");
    aboutVersion_ =
        CreateStatic(L"");
    aboutDescription_ =
        CreateStatic(
            L"",
            SS_LEFT | SS_NOPREFIX);

    updateSectionTitle_ =
        CreateStatic(L"");
    aboutProjectTitle_ =
        CreateStatic(L"");
    updateChannelLabel_ =
        CreateStatic(L"");

    updateChannel_ =
        CreateWindowExW(
            0,
            L"COMBOBOX",
            L"",
            WS_CHILD | WS_VISIBLE |
                WS_TABSTOP |
                CBS_DROPDOWNLIST |
                WS_VSCROLL,
            0, 0, 0, 0,
            hwnd_,
            reinterpret_cast<HMENU>(
                static_cast<UINT_PTR>(
                    kIdUpdateChannel)),
            instance_,
            nullptr);

    updateAutoCheck_ =
        CreateCheckboxRow(
            L"",
            kIdUpdateAutoCheck);

    updateStatus_ =
        CreateStatic(
            L"",
            SS_LEFT | SS_NOPREFIX);

    updateCheck_ =
        CreateButton(
            L"",
            kIdUpdateCheck);
    updateInstall_ =
        CreateButton(
            L"",
            kIdUpdateInstall);

    openGitHub_ =
        CreateButton(
            L"",
            kIdOpenGitHub);

    aboutControls_ = {
        aboutName_,
        aboutVersion_,
        aboutDescription_,
        updateSectionTitle_,
        aboutProjectTitle_,
        updateChannelLabel_,
        updateChannel_,
        updateAutoCheck_,
        updateStatus_,
        updateCheck_,
        updateInstall_,
        openGitHub_,
    };
}

void SettingsWindow::ApplyFonts() {
    if (normalFont_) {
        DeleteObject(normalFont_);
        normalFont_ = nullptr;
    }
    if (titleFont_) {
        DeleteObject(titleFont_);
        titleFont_ = nullptr;
    }
    if (appNameFont_) {
        DeleteObject(appNameFont_);
        appNameFont_ = nullptr;
    }
    if (sectionFont_) {
        DeleteObject(sectionFont_);
        sectionFont_ = nullptr;
    }

    const auto language =
        app_.SettingsData().language;

    normalFont_ =
        ui::CreateFontHandle(
            ui::ApplicationFontSpec(
                language,
                ui::UiFontRole::Body),
            dpi_);
    sectionFont_ =
        ui::CreateFontHandle(
            ui::ApplicationFontSpec(
                language,
                ui::UiFontRole::SectionTitle),
            dpi_);
    titleFont_ =
        ui::CreateFontHandle(
            ui::ApplicationFontSpec(
                language,
                ui::UiFontRole::PageTitle),
            dpi_);
    appNameFont_ =
        ui::CreateFontHandle(
            ui::ApplicationFontSpec(
                language,
                ui::UiFontRole::AppTitle),
            dpi_);

    const std::vector<HWND> normalControls{
        navGeneral_,
        navHotkeys_,
        navProviders_,
        navAppearance_,
        navData_,
        navDiagnostics_,
        navAbout_,
        brandSubtitle_,
        pageDescription_,
        startWithWindows_,
        showOnStartup_,
        hideAfterLaunch_,
        clearQueryOnShow_,
        hideOnFocusLost_,
        showTrayIcon_,
        showResultIcons_,
        pinyinSearch_,
        wildcardMatching_,
        numericQuickLaunch_,
        executeSingleResult_,
        numericQuickLaunchOrderLabel_,
        numericQuickLaunchOrder_,
        popupMonitorLabel_,
        popupMonitorDescription_,
        popupMonitor_,
        launcherPlacementLabel_,
        launcherPlacementDescription_,
        launcherPlacement_,
        settingsPlacementLabel_,
        settingsPlacementDescription_,
        settingsPlacement_,
        generalNote_,
        hotkeyActionList_,
        hotkeyEditorDescription_,
        hotkeyScope_,
        hotkeyEnabled_,
        hotkeyCapture_,
        hotkeyResetCurrent_,
        hotkeyResetAll_,
        hotkeyPageStatus_,
        hotkeyPageNote_,
        providerStartMenu_,
        providerPackaged_,
        providerAppPaths_,
        providerPath_,
        providerEverything_,
        providerStatus_,
        providerGetEverything_,
        providerRecheckEverything_,
        providerNote_,
        uiStyleLabel_,
        uiStyle_,
        languageLabel_,
        language_,
        appearanceNote_,
        dataPath_,
        openDataFolder_,
        dataImportTsv_,
        dataImportLegacy_,
        dataExport_,
        dataClearUsage_,
        dataRebuildIndex_,
        dataResetSettings_,
        dataStatus_,
        diagnosticsMemoryStatus_,
        diagnosticsSearchStatus_,
        actionsWindowsStatus_,
        actionsClipboardStatus_,
        actionsWebStatus_,
        actionsNote_,
        aboutVersion_,
        aboutDescription_,
        updateChannelLabel_,
        updateChannel_,
        updateAutoCheck_,
        updateStatus_,
        updateCheck_,
        updateInstall_,
        openGitHub_,
    };

    if (hotkeyActionList_) {
        SendMessageW(
            hotkeyActionList_,
            LB_SETITEMHEIGHT,
            0,
            Scale(52));
    }

    for (HWND control :
         normalControls) {
        if (control) {
            SendMessageW(
                control,
                WM_SETFONT,
                reinterpret_cast<WPARAM>(
                    normalFont_),
                TRUE);
        }
    }

    for (HWND control :
         std::array<HWND, 18>{
             generalBehaviorTitle_,
             searchBehaviorTitle_,
             placementSectionTitle_,
             hotkeyEditorTitle_,
             providerSectionTitle_,
             providerFilesTitle_,
             appearanceLauncherTitle_,
             appearanceAppTitle_,
             dataPathLabel_,
             dataTransferLabel_,
             dataMaintenanceLabel_,
             diagnosticsMemoryTitle_,
             diagnosticsSearchTitle_,
             actionsWindowsTitle_,
             actionsClipboardTitle_,
             actionsWebTitle_,
             updateSectionTitle_,
             aboutProjectTitle_}) {
        if (control) {
            SendMessageW(
                control,
                WM_SETFONT,
                reinterpret_cast<WPARAM>(
                    sectionFont_),
                TRUE);
        }
    }

    if (pageTitle_) {
        SendMessageW(
            pageTitle_,
            WM_SETFONT,
            reinterpret_cast<WPARAM>(
                titleFont_),
            TRUE);
    }

    for (HWND control :
         std::array<HWND, 2>{
             brandName_,
             aboutName_}) {
        if (control) {
            SendMessageW(
                control,
                WM_SETFONT,
                reinterpret_cast<WPARAM>(
                    appNameFont_),
                TRUE);
        }
    }
}

void SettingsWindow::ApplyLanguage() {
    if (!hwnd_) return;

    const bool oldSyncing =
        syncing_;
    syncing_ = true;

    SetWindowTextW(
        hwnd_,
        T(L"ALTRun Next 设置",
          L"ALTRun Next Settings"));
    SetWindowTextW(
        brandName_,
        L"ALTRun Next");
    SetWindowTextW(
        brandSubtitle_,
        T(L"设置", L"Settings"));

    SetWindowTextW(
        generalBehaviorTitle_,
        T(L"启动器行为",
          L"Launcher behavior"));
    SetWindowTextW(
        startWithWindows_,
        T(L"开机启动",
          L"Start with Windows"));
    SetWindowTextW(
        showOnStartup_,
        T(L"启动时显示启动器",
          L"Show launcher on startup"));
    SetWindowTextW(
        hideAfterLaunch_,
        T(L"执行后自动隐藏",
          L"Hide after launch"));
    SetWindowTextW(
        clearQueryOnShow_,
        T(L"呼出时清空搜索",
          L"Clear query on open"));
    SetWindowTextW(
        hideOnFocusLost_,
        T(L"失去焦点时隐藏",
          L"Hide when focus is lost"));
    SetWindowTextW(
        showTrayIcon_,
        T(L"显示系统托盘图标",
          L"Show system tray icon"));
    SetWindowTextW(
        showResultIcons_,
        T(L"显示搜索结果图标",
          L"Show search result icons"));

    SetWindowTextW(
        searchBehaviorTitle_,
        T(L"搜索行为",
          L"Search behavior"));
    SetWindowTextW(
        pinyinSearch_,
        T(L"启用拼音搜索",
          L"Enable Pinyin search"));
    SetWindowTextW(
        wildcardMatching_,
        T(L"允许 * / ? 通配符",
          L"Enable * / ? wildcards"));
    SetWindowTextW(
        numericQuickLaunch_,
        T(L"数字键快速执行结果",
          L"Quick launch with number keys"));
    SetWindowTextW(
        executeSingleResult_,
        T(L"仅剩一个结果时立即执行",
          L"Execute immediately when one result remains"));
    SetWindowTextW(
        numericQuickLaunchOrderLabel_,
        T(L"数字顺序",
          L"Number order"));

    SendMessageW(
        numericQuickLaunchOrder_,
        CB_RESETCONTENT, 0, 0);
    SendMessageW(
        numericQuickLaunchOrder_,
        CB_ADDSTRING, 0,
        reinterpret_cast<LPARAM>(
            L"1–9, 0"));
    SendMessageW(
        numericQuickLaunchOrder_,
        CB_ADDSTRING, 0,
        reinterpret_cast<LPARAM>(
            L"0–9"));

    SetWindowTextW(
        placementSectionTitle_,
        T(L"窗口位置",
          L"Window placement"));
    SetWindowTextW(
        popupMonitorLabel_,
        T(L"Launcher 目标显示器",
          L"Launcher monitor"));
    SetWindowTextW(
        popupMonitorDescription_,
        T(L"决定 Launcher 呼出时使用哪一块屏幕。",
          L"Choose which display the launcher uses when it opens."));

    SendMessageW(
        popupMonitor_,
        CB_RESETCONTENT, 0, 0);
    SendMessageW(
        popupMonitor_,
        CB_ADDSTRING, 0,
        reinterpret_cast<LPARAM>(
            T(L"当前鼠标所在显示器",
              L"Monitor containing the mouse")));
    SendMessageW(
        popupMonitor_,
        CB_ADDSTRING, 0,
        reinterpret_cast<LPARAM>(
            T(L"当前活动窗口所在显示器",
              L"Monitor containing the active window")));
    SendMessageW(
        popupMonitor_,
        CB_ADDSTRING, 0,
        reinterpret_cast<LPARAM>(
            T(L"主显示器",
              L"Primary monitor")));

    SetWindowTextW(
        launcherPlacementLabel_,
        T(L"Launcher 出现位置",
          L"Launcher position"));
    SetWindowTextW(
        launcherPlacementDescription_,
        T(L"选择靠上、屏幕居中或恢复上次拖动后的坐标。",
          L"Open near the top, centered, or at the last manually moved position."));

    SendMessageW(
        launcherPlacement_,
        CB_RESETCONTENT, 0, 0);
    SendMessageW(
        launcherPlacement_,
        CB_ADDSTRING, 0,
        reinterpret_cast<LPARAM>(
            T(L"靠近屏幕上方",
              L"Near top of screen")));
    SendMessageW(
        launcherPlacement_,
        CB_ADDSTRING, 0,
        reinterpret_cast<LPARAM>(
            T(L"屏幕居中",
              L"Center on screen")));
    SendMessageW(
        launcherPlacement_,
        CB_ADDSTRING, 0,
        reinterpret_cast<LPARAM>(
            T(L"上次位置",
              L"Last position")));

    SetWindowTextW(
        settingsPlacementLabel_,
        T(L"设置窗口出现位置",
          L"Settings window position"));
    SetWindowTextW(
        settingsPlacementDescription_,
        T(L"每次重新打开设置时居中，或恢复上次拖动后的坐标。",
          L"Center the Settings window when reopened, or restore its last moved position."));

    SendMessageW(
        settingsPlacement_,
        CB_RESETCONTENT, 0, 0);
    SendMessageW(
        settingsPlacement_,
        CB_ADDSTRING, 0,
        reinterpret_cast<LPARAM>(
            T(L"屏幕居中",
              L"Center on screen")));
    SendMessageW(
        settingsPlacement_,
        CB_ADDSTRING, 0,
        reinterpret_cast<LPARAM>(
            T(L"上次位置",
              L"Last position")));

    SetWindowTextW(
        generalNote_,
        T(L"窗口位置与搜索行为会立即保存；“上次位置”会自动限制在当前可用屏幕范围内。",
          L"Placement and search behavior are saved immediately. Last positions are clamped to the currently available displays."));

    SetWindowTextW(
        hotkeyEnabled_,
        T(L"启用此快捷键",
          L"Enable this hotkey"));
    SetWindowTextW(
        hotkeyResetCurrent_,
        T(L"恢复此项默认值",
          L"Reset this binding"));
    SetWindowTextW(
        hotkeyResetAll_,
        T(L"恢复全部默认快捷键",
          L"Reset all hotkeys"));
    SetWindowTextW(
        hotkeyPageNote_,
        T(L"选择左侧动作后，可直接更改快捷键；Esc 取消捕获。Windows 全局热键只有注册成功后才会保存。",
          L"Select an action on the left, then change its binding directly; Esc cancels capture. Windows-global bindings are saved only after registration succeeds."));

    SetWindowTextW(
        providerSectionTitle_,
        T(L"应用来源",
          L"Application sources"));
    SetWindowTextW(
        providerFilesTitle_,
        T(L"文件与文件夹",
          L"Files & folders"));
    SetWindowTextW(
        providerStartMenu_,
        T(L"开始菜单",
          L"Start Menu"));
    SetWindowTextW(
        providerPackaged_,
        L"Windows Apps");
    SetWindowTextW(
        providerAppPaths_,
        L"App Paths");
    SetWindowTextW(
        providerPath_,
        L"PATH");
    SetWindowTextW(
        providerEverything_,
        T(L"Everything 文件与文件夹",
          L"Everything files & folders"));
    SetWindowTextW(
        providerGetEverything_,
        T(L"获取并启动 Everything",
          L"Get and start Everything"));
    SetWindowTextW(
        providerRecheckEverything_,
        T(L"重新检测",
          L"Recheck"));
    SetWindowTextW(
        providerNote_,
        T(L"Everything 使用本机 IPC 实时查询。ALTRun Next 会优先复用已有标准版；只有自己管理的便携版才会管理其 Service 生命周期。",
          L"Everything is queried live over local IPC. Existing standard copies are preferred; ALTRun Next manages service lifecycle only for its own portable copy."));

    SetWindowTextW(
        appearanceLauncherTitle_,
        T(L"启动器",
          L"Launcher"));
    SetWindowTextW(
        uiStyleLabel_,
        T(L"启动器样式",
          L"Launcher style"));
    SendMessageW(
        uiStyle_,
        CB_RESETCONTENT, 0, 0);
    SendMessageW(
        uiStyle_,
        CB_ADDSTRING, 0,
        reinterpret_cast<LPARAM>(
            L"Classic ALTRun"));
    SendMessageW(
        uiStyle_,
        CB_ADDSTRING, 0,
        reinterpret_cast<LPARAM>(
            L"Modern Compact"));

    SetWindowTextW(
        appearanceAppTitle_,
        T(L"应用",
          L"Application"));
    SetWindowTextW(
        languageLabel_,
        T(L"界面语言",
          L"Interface language"));
    SendMessageW(
        language_,
        CB_RESETCONTENT, 0, 0);
    SendMessageW(
        language_,
        CB_ADDSTRING, 0,
        reinterpret_cast<LPARAM>(
            L"简体中文"));
    SendMessageW(
        language_,
        CB_ADDSTRING, 0,
        reinterpret_cast<LPARAM>(
            L"English"));
    SetWindowTextW(
        appearanceNote_,
        T(L"启动器样式和界面语言会立即应用。结果图标已移到“常规 → 启动器行为”。",
          L"Launcher style and interface language apply immediately. Result icons are now under General → Launcher behavior."));

    SetWindowTextW(
        dataPathLabel_,
        T(L"数据目录",
          L"Data directory"));
    SetWindowTextW(
        dataPath_,
        app_.DataDirectory().c_str());
    SetWindowTextW(
        openDataFolder_,
        T(L"打开目录",
          L"Open folder"));

    SetWindowTextW(
        dataTransferLabel_,
        T(L"导入与导出",
          L"Import & export"));
    SetWindowTextW(
        dataImportTsv_,
        T(L"导入 TSV",
          L"Import TSV"));
    SetWindowTextW(
        dataImportLegacy_,
        T(L"导入旧版 AltRun",
          L"Import legacy AltRun"));
    SetWindowTextW(
        dataExport_,
        T(L"导出快捷项",
          L"Export shortcuts"));

    SetWindowTextW(
        dataMaintenanceLabel_,
        T(L"维护",
          L"Maintenance"));
    SetWindowTextW(
        dataClearUsage_,
        T(L"清除使用历史",
          L"Clear usage history"));
    SetWindowTextW(
        dataRebuildIndex_,
        T(L"重新建立程序索引",
          L"Rebuild program index"));
    SetWindowTextW(
        dataResetSettings_,
        T(L"恢复默认设置",
          L"Reset settings"));

    SetWindowTextW(
        diagnosticsMemoryTitle_,
        T(L"进程内存",
          L"Process memory"));
    SetWindowTextW(
        diagnosticsSearchTitle_,
        T(L"搜索数据与后台",
          L"Search data & background"));
    SetWindowTextW(
        actionsWindowsTitle_,
        T(L"Windows 导航与上下文",
          L"Windows navigation & context"));
    SetWindowTextW(
        actionsClipboardTitle_,
        T(L"剪贴板与文本",
          L"Clipboard & text"));
    SetWindowTextW(
        actionsWebTitle_,
        T(L"网页与 URL",
          L"Web & URL"));
    SetWindowTextW(
        actionsNote_,
        T(L"诊断数据每秒刷新一次，仅用于观察运行状态，不会主动修改或裁剪进程。",
          L"Diagnostics refresh once per second for observation only and never modify or trim the process."));

    SetWindowTextW(
        aboutName_,
        L"ALTRun Next");

    std::wstring version =
        L"v";
    version.append(
        kVersion.begin(),
        kVersion.end());
    SetWindowTextW(
        aboutVersion_,
        version.c_str());

    SetWindowTextW(
        aboutDescription_,
        T(L"轻量、快速、键盘优先的 Windows 启动器。",
          L"A lightweight, fast, keyboard-first Windows launcher."));

    SetWindowTextW(
        updateSectionTitle_,
        T(L"更新",
          L"Updates"));
    SetWindowTextW(
        aboutProjectTitle_,
        T(L"项目",
          L"Project"));
    SetWindowTextW(
        updateChannelLabel_,
        T(L"更新通道",
          L"Update channel"));

    const int oldUpdateChannel =
        static_cast<int>(
            SendMessageW(
                updateChannel_,
                CB_GETCURSEL,
                0,
                0));

    SendMessageW(
        updateChannel_,
        CB_RESETCONTENT, 0, 0);
    SendMessageW(
        updateChannel_,
        CB_ADDSTRING, 0,
        reinterpret_cast<LPARAM>(
            T(L"稳定版", L"Stable")));
    SendMessageW(
        updateChannel_,
        CB_ADDSTRING, 0,
        reinterpret_cast<LPARAM>(
            T(L"开发版", L"Development")));
    SendMessageW(
        updateChannel_,
        CB_SETCURSEL,
        oldUpdateChannel >= 0
            ? oldUpdateChannel
            : 0,
        0);

    SetWindowTextW(
        updateAutoCheck_,
        T(L"自动检查更新",
          L"Automatically check for updates"));
    SetWindowTextW(
        updateCheck_,
        T(L"检查更新",
          L"Check for updates"));
    SetWindowTextW(
        updateInstall_,
        T(L"下载并安装",
          L"Download and install"));
    SetWindowTextW(
        openGitHub_,
        L"GitHub");

    ApplyFonts();
    UpdateNavLabels();
    UpdatePageHeader();
    RefreshFromSettings();
    RefreshDataCompatibilityStatus();

    syncing_ = oldSyncing;

    RedrawWindow(
        hwnd_,
        nullptr,
        nullptr,
        RDW_INVALIDATE |
            RDW_ERASE |
            RDW_ALLCHILDREN |
            RDW_UPDATENOW);
}

void SettingsWindow::RefreshFromSettings() {
    if (!hwnd_) return;

    const bool oldSyncing =
        syncing_;
    syncing_ = true;

    const auto& settings =
        app_.SettingsData();

    int monitorIndex = 0;
    if (settings.popupMonitor == "active") {
        monitorIndex = 1;
    } else if (
        settings.popupMonitor == "primary") {
        monitorIndex = 2;
    }

    SendMessageW(
        popupMonitor_,
        CB_SETCURSEL,
        monitorIndex,
        0);

    int launcherPlacementIndex = 0;
    if (settings.launcherPlacement ==
        "center") {
        launcherPlacementIndex = 1;
    } else if (
        settings.launcherPlacement ==
        "last") {
        launcherPlacementIndex = 2;
    }

    SendMessageW(
        launcherPlacement_,
        CB_SETCURSEL,
        launcherPlacementIndex,
        0);

    SendMessageW(
        settingsPlacement_,
        CB_SETCURSEL,
        settings.settingsPlacement ==
                "last"
            ? 1
            : 0,
        0);

    SendMessageW(
        uiStyle_,
        CB_SETCURSEL,
        settings.uiStyle ==
                UiStyle::ModernCompact
            ? 1
            : 0,
        0);

    SendMessageW(
        language_,
        CB_SETCURSEL,
        settings.language ==
                Language::EnUS
            ? 1
            : 0,
        0);

    SendMessageW(
        updateChannel_,
        CB_SETCURSEL,
        settings.updateChannel ==
                UpdateChannel::Development
            ? 1
            : 0,
        0);

    SendMessageW(
        updateAutoCheck_,
        BM_SETCHECK,
        settings.autoCheckUpdates
            ? BST_CHECKED
            : BST_UNCHECKED,
        0);

    SendMessageW(
        showResultIcons_,
        BM_SETCHECK,
        settings.showResultIcons
            ? BST_CHECKED
            : BST_UNCHECKED,
        0);

    SendMessageW(
        numericQuickLaunchOrder_,
        CB_SETCURSEL,
        settings.numericQuickLaunchOrder ==
                "zero-to-nine"
            ? 1
            : 0,
        0);

    EnableWindow(
        numericQuickLaunchOrder_,
        settings.numericQuickLaunch
            ? TRUE
            : FALSE);

    RefreshHotkeyPage();
    RefreshActionDiagnostics();
    RefreshUpdateStatus();

    for (HWND control :
         std::array<HWND, 17>{
             startWithWindows_,
             showOnStartup_,
             hideAfterLaunch_,
             clearQueryOnShow_,
             hideOnFocusLost_,
             showTrayIcon_,
             showResultIcons_,
             pinyinSearch_,
             wildcardMatching_,
             numericQuickLaunch_,
             executeSingleResult_,
             providerStartMenu_,
             providerPackaged_,
             providerAppPaths_,
             providerPath_,
             providerEverything_,
             updateAutoCheck_}) {
        if (control) {
            InvalidateRect(
                control,
                nullptr,
                TRUE);
        }
    }

    RefreshProviderStatus();

    syncing_ = oldSyncing;
}

std::wstring SettingsWindow::HotkeyActionLabel(
    std::string_view actionId) const {
    if (actionId ==
        hotkey_actions::kActivate) {
        return T(
            L"唤起 ALTRun Next",
            L"Show ALTRun Next");
    }
    if (actionId ==
        hotkey_actions::
            kActivateSecondary) {
        return T(
            L"辅助唤起",
            L"Secondary activation");
    }
    if (actionId ==
        hotkey_actions::kOpenSettings) {
        return T(
            L"打开设置",
            L"Open Settings");
    }
    if (actionId ==
        hotkey_actions::
            kNavigateCurrentFileManager) {
        return T(
            L"导航当前文件管理器",
            L"Navigate current file manager");
    }
    if (actionId ==
        hotkey_actions::
            kCopySelectedTarget) {
        return T(
            L"复制选中结果",
            L"Copy selected result");
    }
    return std::wstring(
        actionId.begin(),
        actionId.end());
}

std::wstring SettingsWindow::HotkeyActionDescription(
    std::string_view actionId) const {
    if (actionId ==
        hotkey_actions::kActivate) {
        return T(
            L"在任何程序中显示或隐藏启动器。主热键始终保持启用，并要求至少一个修饰键。",
            L"Show or hide the launcher from any application. The primary binding is always enabled and requires a modifier.");
    }
    if (actionId ==
        hotkey_actions::
            kActivateSecondary) {
        return T(
            L"可选的第二组 Windows 全局呼出热键，默认关闭。",
            L"Optional second Windows-global activation binding; disabled by default.");
    }
    if (actionId ==
        hotkey_actions::kOpenSettings) {
        return T(
            L"仅在启动器窗口打开时进入设置页面。",
            L"Open Settings while the launcher is visible.");
    }
    if (actionId ==
        hotkey_actions::
            kNavigateCurrentFileManager) {
        return T(
            L"对 Folder 结果执行上下文导航：Explorer 或 Total Commander 当前面板。",
            L"Contextually navigate a Folder result in Explorer or the active Total Commander panel.");
    }
    if (actionId ==
        hotkey_actions::
            kCopySelectedTarget) {
        return T(
            L"复制当前结果的路径、URL、Target 或 Smart Action payload。",
            L"Copy the selected result path, URL, target or Smart Action payload.");
    }
    return L"";
}

std::wstring SettingsWindow::FormatHotkeyBinding(
    std::string_view actionId) const {
    const auto binding =
        EffectiveHotkeyBinding(
            app_.SettingsData()
                .hotkeyBindings,
            actionId);

    std::wstring result;

    const auto append =
        [&](std::wstring_view text) {
            if (!result.empty()) {
                result += L" + ";
            }
            result += text;
        };

    for (const auto& modifier :
         binding.modifiers) {
        if (modifier == "ctrl") {
            append(L"Ctrl");
        } else if (modifier == "alt") {
            append(L"Alt");
        } else if (modifier == "shift") {
            append(L"Shift");
        } else if (modifier == "win") {
            append(L"Win");
        }
    }

    const UINT key =
        hotkey::KeyFromName(
            binding.key);

    append(
        key != 0
            ? hotkey::KeyDisplayName(key)
            : std::wstring(
                  binding.key.begin(),
                  binding.key.end()));

    return result;
}

void SettingsWindow::RefreshHotkeyPage() {
    if (!hotkeyActionList_) {
        return;
    }

    const bool oldSyncing =
        syncing_;
    syncing_ = true;

    const std::string preferred =
        selectedHotkeyActionId_;

    SendMessageW(
        hotkeyActionList_,
        LB_RESETCONTENT,
        0,
        0);

    hotkeyActionIds_.clear();

    int selectedIndex = -1;

    for (const auto& action :
         HotkeyActionRegistry()) {
        const std::wstring label =
            HotkeyActionLabel(
                action.id);

        const LRESULT index =
            SendMessageW(
                hotkeyActionList_,
                LB_ADDSTRING,
                0,
                reinterpret_cast<LPARAM>(
                    label.c_str()));

        if (index >= 0) {
            hotkeyActionIds_
                .push_back(
                    action.id);

            if (action.id ==
                preferred) {
                selectedIndex =
                    static_cast<int>(
                        index);
            }
        }
    }

    if (selectedIndex < 0 &&
        !hotkeyActionIds_.empty()) {
        selectedIndex = 0;
    }

    if (selectedIndex >= 0) {
        SendMessageW(
            hotkeyActionList_,
            LB_SETCURSEL,
            selectedIndex,
            0);

        selectedHotkeyActionId_ =
            hotkeyActionIds_[
                static_cast<std::size_t>(
                    selectedIndex)];

        LoadHotkeyEditor(
            selectedHotkeyActionId_);
    } else {
        selectedHotkeyActionId_
            .clear();
    }

    syncing_ = oldSyncing;
}

void SettingsWindow::LoadHotkeyEditor(
    std::string_view actionId) {
    const auto* action =
        FindHotkeyAction(actionId);

    if (!action) {
        return;
    }

    selectedHotkeyActionId_ =
        std::string(actionId);

    const auto binding =
        EffectiveHotkeyBinding(
            app_.SettingsData()
                .hotkeyBindings,
            actionId);

    const auto title =
        HotkeyActionLabel(
            actionId);

    SetWindowTextW(
        hotkeyEditorTitle_,
        title.c_str());

    const auto description =
        HotkeyActionDescription(
            actionId);

    SetWindowTextW(
        hotkeyEditorDescription_,
        description.c_str());

    std::wstring scope =
        action->scope ==
                HotkeyScope::Global
            ? T(L"作用域：Windows 全局",
                L"Scope: Windows global")
            : T(L"作用域：启动器内部",
                L"Scope: Launcher");

    SetWindowTextW(
        hotkeyScope_,
        scope.c_str());

    SendMessageW(
        hotkeyEnabled_,
        BM_SETCHECK,
        binding.enabled
            ? BST_CHECKED
            : BST_UNCHECKED,
        0);

    EnableWindow(
        hotkeyEnabled_,
        action->required
            ? FALSE
            : TRUE);

    const auto chord =
        FormatHotkeyBinding(
            actionId);

    SetWindowTextW(
        hotkeyCapture_,
        capturingHotkeyActionId_ ==
                actionId
            ? T(L"请按新的快捷键…",
                L"Press the new shortcut...")
            : chord.c_str());

    std::wstring status;

    if (capturingHotkeyActionId_ ==
        actionId) {
        status =
            T(L"正在捕获：按下组合键；Esc 取消。",
              L"Capturing: press a key combination; Esc cancels.");
    } else if (!binding.enabled) {
        status =
            T(L"运行状态：已禁用",
              L"Runtime status: Disabled");
    } else if (
        action->scope ==
        HotkeyScope::Global) {
        if (app_.IsHotkeyActionRegistered(
                actionId)) {
            status =
                T(L"运行状态：● 已向 Windows 注册",
                  L"Runtime status: ● Registered with Windows");
        } else {
            status =
                T(L"运行状态：⚠ Windows 注册失败，旧绑定仍保持有效。错误码：",
                  L"Runtime status: ⚠ Windows registration failed; the previous binding remains active. Error: ");
            status +=
                std::to_wstring(
                    app_.HotkeyActionLastError(
                        actionId));
        }
    } else {
        status =
            T(L"运行状态：● 就绪（仅启动器内）",
              L"Runtime status: ● Ready (launcher only)");
    }

    SetWindowTextW(
        hotkeyPageStatus_,
        status.c_str());
}

void SettingsWindow::BeginHotkeyCapture() {
    if (selectedHotkeyActionId_
            .empty()) {
        return;
    }

    capturingHotkeyActionId_ =
        selectedHotkeyActionId_;

    LoadHotkeyEditor(
        selectedHotkeyActionId_);

    SetFocus(hwnd_);
}

void SettingsWindow::ApplyCapturedHotkey(
    UINT virtualKey) {
    if (capturingHotkeyActionId_
            .empty()) {
        return;
    }

    if (virtualKey == VK_ESCAPE) {
        capturingHotkeyActionId_
            .clear();
        LoadHotkeyEditor(
            selectedHotkeyActionId_);
        return;
    }

    if (virtualKey == VK_CONTROL ||
        virtualKey == VK_LCONTROL ||
        virtualKey == VK_RCONTROL ||
        virtualKey == VK_MENU ||
        virtualKey == VK_LMENU ||
        virtualKey == VK_RMENU ||
        virtualKey == VK_SHIFT ||
        virtualKey == VK_LSHIFT ||
        virtualKey == VK_RSHIFT ||
        virtualKey == VK_LWIN ||
        virtualKey == VK_RWIN) {
        return;
    }

    const std::string key =
        hotkey::KeyName(
            virtualKey);

    if (key.empty()) {
        SetWindowTextW(
            hotkeyPageStatus_,
            T(L"这个按键目前不受支持，请换一个组合键。",
              L"This key is not currently supported; choose another combination."));
        return;
    }

    HotkeyBinding candidate =
        EffectiveHotkeyBinding(
            app_.SettingsData()
                .hotkeyBindings,
            capturingHotkeyActionId_);

    candidate.modifiers.clear();
    candidate.key = key;

    if ((GetKeyState(VK_CONTROL) &
         0x8000) != 0) {
        candidate.modifiers
            .push_back("ctrl");
    }
    if ((GetKeyState(VK_MENU) &
         0x8000) != 0) {
        candidate.modifiers
            .push_back("alt");
    }
    if ((GetKeyState(VK_SHIFT) &
         0x8000) != 0) {
        candidate.modifiers
            .push_back("shift");
    }
    if ((GetKeyState(VK_LWIN) &
         0x8000) != 0 ||
        (GetKeyState(VK_RWIN) &
         0x8000) != 0) {
        candidate.modifiers
            .push_back("win");
    }

    CanonicalizeHotkeyBinding(
        candidate);

    if (!ValidateHotkeyBinding(
            capturingHotkeyActionId_,
            candidate)) {
        SetWindowTextW(
            hotkeyPageStatus_,
            T(L"该组合键无效或会抢占搜索输入。裸字符、空格和编辑/导航键保留给输入框；无修饰键时请使用 F1–F24 或 Pause。",
              L"This binding is invalid or would steal query input. Bare characters, Space and editing/navigation keys stay with the edit control; use F1-F24 or Pause when no modifier is present."));
        return;
    }

    if (const auto conflict =
            FindHotkeyConflict(
                app_.SettingsData()
                    .hotkeyBindings,
                capturingHotkeyActionId_,
                candidate)) {
        std::wstring message =
            T(L"与“", L"Conflicts with “");
        message +=
            HotkeyActionLabel(
                *conflict);
        message +=
            T(L"”冲突，请换一个组合键。",
              L"”; choose another binding.");

        SetWindowTextW(
            hotkeyPageStatus_,
            message.c_str());
        return;
    }

    if (!app_.SetHotkeyBinding(
            capturingHotkeyActionId_,
            candidate)) {
        SetWindowTextW(
            hotkeyPageStatus_,
            T(L"无法应用这个快捷键。若它是全局热键，可能已被其他程序占用；旧绑定保持不变。",
              L"Could not apply this binding. A global shortcut may already be owned by another app; the previous binding remains active."));
        return;
    }

    capturingHotkeyActionId_
        .clear();

    RefreshHotkeyPage();
}

void SettingsWindow::ToggleSelectedHotkeyEnabled() {
    if (syncing_ ||
        selectedHotkeyActionId_
            .empty()) {
        return;
    }

    const auto* action =
        FindHotkeyAction(
            selectedHotkeyActionId_);

    if (!action ||
        action->required) {
        LoadHotkeyEditor(
            selectedHotkeyActionId_);
        return;
    }

    auto binding =
        EffectiveHotkeyBinding(
            app_.SettingsData()
                .hotkeyBindings,
            selectedHotkeyActionId_);

    binding.enabled =
        !binding.enabled;

    if (binding.enabled) {
        if (const auto conflict =
                FindHotkeyConflict(
                    app_.SettingsData()
                        .hotkeyBindings,
                    selectedHotkeyActionId_,
                    binding)) {
            std::wstring message =
                T(L"无法启用：与“",
                  L"Cannot enable: conflicts with “");
            message +=
                HotkeyActionLabel(
                    *conflict);
            message += L"”.";

            SetWindowTextW(
                hotkeyPageStatus_,
                message.c_str());
            LoadHotkeyEditor(
                selectedHotkeyActionId_);
            return;
        }
    }

    if (!app_.SetHotkeyBinding(
            selectedHotkeyActionId_,
            binding)) {
        LoadHotkeyEditor(
            selectedHotkeyActionId_);
        return;
    }

    RefreshHotkeyPage();
}

void SettingsWindow::ResetSelectedHotkey() {
    const auto* action =
        FindHotkeyAction(
            selectedHotkeyActionId_);

    if (!action) {
        return;
    }

    if (const auto conflict =
            FindHotkeyConflict(
                app_.SettingsData()
                    .hotkeyBindings,
                selectedHotkeyActionId_,
                action->defaultBinding)) {
        std::wstring message =
            T(L"默认组合键当前与“",
              L"The default binding currently conflicts with “");
        message +=
            HotkeyActionLabel(
                *conflict);
        message += L"”.";

        SetWindowTextW(
            hotkeyPageStatus_,
            message.c_str());
        return;
    }

    if (!app_.SetHotkeyBinding(
            selectedHotkeyActionId_,
            action->defaultBinding)) {
        SetWindowTextW(
            hotkeyPageStatus_,
            T(L"恢复失败；如果这是全局快捷键，默认组合可能已被其他程序占用。",
              L"Reset failed; if this is a global shortcut, another app may own the default binding."));
        return;
    }

    RefreshHotkeyPage();
}

void SettingsWindow::ResetAllHotkeys() {
    const int answer =
        MessageBoxW(
            hwnd_,
            T(L"恢复全部默认快捷键？\n\n主热键将恢复为 Alt + Space，辅助热键关闭，内部动作恢复默认组合。",
              L"Reset every hotkey to defaults?\n\nPrimary activation returns to Alt + Space, secondary activation is disabled and launcher actions regain their defaults."),
            T(L"恢复默认快捷键",
              L"Reset hotkeys"),
            MB_YESNO |
                MB_ICONQUESTION);

    if (answer != IDYES) {
        return;
    }

    if (!app_.ResetHotkeyBindings()) {
        MessageBoxW(
            hwnd_,
            T(L"恢复失败。默认全局热键可能已被其他程序占用，原有可用绑定已恢复。",
              L"Reset failed. Another app may own a default global shortcut; the previous working bindings were restored."),
            T(L"恢复默认快捷键",
              L"Reset hotkeys"),
            MB_OK |
                MB_ICONWARNING);
        return;
    }

    capturingHotkeyActionId_
        .clear();
    RefreshHotkeyPage();
}

void SettingsWindow::RefreshActionDiagnostics() {
    if (!actionsWindowsStatus_) return;

    const auto runtime =
        app_.RuntimeDiagnostics();

    std::wstring memoryText;
    if (runtime.processMemory.available) {
        memoryText +=
            T(L"Working Set：",
              L"Working Set: ");
        memoryText +=
            FormatBytes(
                runtime.processMemory
                    .workingSetBytes);

        memoryText += L"\r\n";
        memoryText +=
            T(L"Peak Working Set：",
              L"Peak Working Set: ");
        memoryText +=
            FormatBytes(
                runtime.processMemory
                    .peakWorkingSetBytes);

        memoryText += L"\r\n";
        memoryText +=
            T(L"Private Bytes：",
              L"Private Bytes: ");
        memoryText +=
            FormatBytes(
                runtime.processMemory
                    .privateBytes);
    } else {
        memoryText =
            T(L"无法读取当前进程内存计数器。",
              L"Unable to read process memory counters.");
    }

    SetWindowTextW(
        diagnosticsMemoryStatus_,
        memoryText.c_str());

    std::wstring searchText =
        T(L"用户快捷项：",
          L"User commands: ");
    searchText +=
        std::to_wstring(
            runtime.userCommandCount);

    searchText +=
        T(L"  ·  Provider 原始命令：",
          L"  ·  Provider commands: ");
    searchText +=
        std::to_wstring(
            runtime.providerCommandCount);

    searchText += L"\r\n";
    searchText +=
        T(L"合并可搜索命令：",
          L"Merged searchable commands: ");
    searchText +=
        std::to_wstring(
            runtime.mergedCommandCount);

    searchText += L"\r\n";
    searchText += L"Pinyin: ";
    if (!runtime.pinyinLoaded) {
        searchText +=
            T(L"未加载", L"Not loaded");
    } else if (
        runtime.pinyinAvailable) {
        searchText +=
            T(L"已加载 / 可用",
              L"Loaded / Ready");
    } else {
        searchText +=
            T(L"已加载 / 不可用",
              L"Loaded / Unavailable");
    }

    searchText +=
        T(L"  ·  Cache：",
          L"  ·  Cache: ");
    searchText +=
        std::to_wstring(
            runtime.pinyinCacheEntryCount);

    searchText += L"\r\n";
    searchText +=
        T(L"Provider Refresh：",
          L"Provider Refresh: ");
    searchText +=
        runtime.providerRefreshRunning
        ? T(L"运行中", L"Running")
        : T(L"空闲", L"Idle");

    searchText +=
        T(L"  ·  Monitor：",
          L"  ·  Monitor: ");
    searchText +=
        runtime.providerMonitorRunning
        ? T(L"运行中", L"Running")
        : T(L"停止", L"Stopped");

    SetWindowTextW(
        diagnosticsSearchStatus_,
        searchText.c_str());

    const auto& context =
        app_.LastActivationContext();

    const auto reasonText =
        [&](ActionUnavailableReason reason)
            -> const wchar_t* {
        switch (reason) {
        case ActionUnavailableReason::None:
            return T(L"可用", L"Available");
        case ActionUnavailableReason::ResultNotFolder:
            return T(L"当前结果不是文件夹",
                     L"The selected result is not a folder");
        case ActionUnavailableReason::NoSupportedFileManager:
            return T(L"最近一次呼出未捕获 Explorer 或 Total Commander",
                     L"The last activation did not capture Explorer or Total Commander");
        case ActionUnavailableReason::NoCopyableTarget:
            return T(L"当前结果没有可复制的目标",
                     L"The selected result has no copyable target");
        case ActionUnavailableReason::InvalidActionTarget:
            return T(L"动作目标无效或为空",
                     L"The action target is invalid or empty");
        }
        return T(L"不可用", L"Unavailable");
    };

    std::wstring windows =
        T(L"最近一次呼出上下文：",
          L"Last activation context: ");

    switch (context.kind) {
    case win::WindowsContextKind::Explorer:
        windows += L"Explorer";
        break;
    case win::WindowsContextKind::FileDialog:
        windows +=
            T(L"文件打开 / 保存对话框",
              L"Open / Save dialog");
        break;
    case win::WindowsContextKind::TotalCommander:
        windows += L"Total Commander";
        break;
    case win::WindowsContextKind::None:
        windows += T(L"无", L"None");
        break;
    }

    if (context.HasExplorer() &&
        !context.explorerFolder.empty()) {
        windows += L"\r\n";
        windows += T(L"来源目录：", L"Source folder: ");
        windows += context.explorerFolder;
    }

    if (context.HasTotalCommander()) {
        windows += L"\r\n";
        windows += T(L"活动面板：", L"Active panel: ");
        windows +=
            context.totalCommanderActivePanel == 1
                ? T(L"左", L"Left")
                : T(L"右", L"Right");

        if (!context.totalCommanderFolder.empty()) {
            windows += L"  ·  ";
            windows += context.totalCommanderFolder;
        }
    }

    if (context.HasFileDialog()) {
        windows += L"\r\n";
        windows += T(L"对话框进程 ID：",
                     L"Dialog process ID: ");
        windows +=
            std::to_wstring(
                context.fileDialogProcessId);
    }

    LauncherResult folderProbe;
    folderProbe.kind = ResultKind::Folder;
    folderProbe.target =
        L"C:\\ALTRunNext\\Diagnostics";
    folderProbe.action.kind =
        LauncherActionKind::OpenFolder;
    folderProbe.action.payload =
        folderProbe.target;

    const auto navigate =
        EvaluateLauncherAction(
            folderProbe,
            LauncherExecutionIntent::
                NavigateCurrentFileManager,
            context.HasExplorer(),
            context.HasFileDialog(),
            context.HasTotalCommander());

    windows += L"\r\n";
    windows += T(L"导航当前文件管理器：",
                 L"Navigate current file manager: ");
    windows += navigate.available
        ? T(L"可用", L"Available")
        : T(L"不可用", L"Unavailable");

    if (!navigate.available) {
        windows += L"  ·  ";
        windows += reasonText(navigate.reason);
    }

    windows += L"\r\n";
    windows += T(L"文件对话框 Folder Enter：",
                 L"File-dialog Folder Enter: ");
    windows += context.HasFileDialog()
        ? T(L"将导航当前对话框",
            L"Navigates the captured dialog")
        : T(L"当前未激活",
            L"Not active");

    const auto folderContext =
        context.CurrentFilesystemFolder();
    windows += L"\r\n{folder}: ";
    if (folderContext.empty()) {
        windows += T(L"不可用", L"Unavailable");
    } else {
        windows += T(L"可用  ·  ", L"Available  ·  ");
        windows += folderContext;
    }

    const bool everythingEnabled =
        providers::IsEnabled(
            app_.SettingsData().providerEnabled,
            providers::kEverythingFilesystem,
            false);

    windows += L"\r\nEverything IPC: ";
    if (!everythingEnabled) {
        windows += T(L"已禁用", L"Disabled");
    } else {
        const auto everything =
            app_.EverythingStatus();
        if (everything.availability ==
            EverythingAvailability::Available) {
            windows += T(L"可用", L"Available");
        } else if (
            everything.availability ==
            EverythingAvailability::Unknown) {
            windows += T(L"正在检测", L"Detecting");
        } else {
            windows +=
                T(L"不可用  ·  应用搜索回退仍有效",
                  L"Unavailable  ·  application-search fallback remains active");
        }
    }

    SetWindowTextW(
        actionsWindowsStatus_,
        windows.c_str());

    LauncherResult copyProbe;
    copyProbe.kind = ResultKind::File;
    copyProbe.target =
        L"C:\\ALTRunNext\\Diagnostics.txt";
    copyProbe.action.kind =
        LauncherActionKind::OpenFile;
    copyProbe.action.payload =
        copyProbe.target;

    const auto copy =
        EvaluateLauncherAction(
            copyProbe,
            LauncherExecutionIntent::
                CopySelectedText,
            false,
            false,
            false);

    std::wstring clipboard =
        T(L"复制选中结果：",
          L"Copy selected result: ");
    clipboard += copy.available
        ? T(L"就绪", L"Ready")
        : reasonText(copy.reason);
    clipboard += L"\r\n";
    clipboard +=
        T(L"Copy / clip / 复制 文本动作：就绪  ·  Unicode CF_UNICODETEXT  ·  不读取或持久化剪贴板历史",
          L"Copy / clip text action: Ready  ·  Unicode CF_UNICODETEXT  ·  clipboard history is neither read nor persisted");

    SetWindowTextW(
        actionsClipboardStatus_,
        clipboard.c_str());

    SetWindowTextW(
        actionsWebStatus_,
        T(L"直接 HTTP / HTTPS / www URL：就绪\r\n{query} URL 模板：就绪  ·  builtin.web 仅运行时存在，不写入 provider-cache",
          L"Direct HTTP / HTTPS / www URLs: Ready\r\n{query} URL templates: Ready  ·  builtin.web is runtime-only and is not written to provider-cache"));
}

void SettingsWindow::RefreshProviderStatus() {
    if (!providerStatus_) {
        return;
    }

    const bool enabled =
        providers::IsEnabled(
            app_.SettingsData()
                .providerEnabled,
            providers::
                kEverythingFilesystem,
            false);

    const auto ipc =
        app_.EverythingStatus();
    const auto bootstrap =
        app_.EverythingBootstrapStatus();

    bool showGetEverything = false;
    bool showRecheck = false;
    std::wstring text;

    if (!enabled) {
        text =
            T(L"○ Everything 文件与文件夹已禁用",
              L"○ Everything files & folders are disabled");
        text += L"\r\n";
        text +=
            T(L"启用后通过本机 IPC 实时查询，不会影响上方应用来源。",
              L"Enable it for live local IPC queries; application sources above remain independent.");
    } else if (
        ipc.availability ==
        EverythingAvailability::
            Available) {

        text =
            T(L"● Everything 正在运行",
              L"● Everything is running");

        text += L"\r\n";
        text += T(
            L"IPC 已连接",
            L"IPC connected");

        if (bootstrap.source ==
                win::EverythingBootstrapSource::
                    Managed ||
            bootstrap.source ==
                win::EverythingBootstrapSource::
                    Downloaded ||
            bootstrap.downloaded) {
            text += T(
                L" · ALTRun Next 托管",
                L" · Managed by ALTRun Next");
        } else {
            text += T(
                L" · 外部安装",
                L" · External installation");
        }

        if (ipc.hasQuery) {
            text += L"\r\n";
            text +=
                T(L"最近查询：",
                  L"Last query: ");

            switch (ipc.lastStatus) {
            case EverythingQueryStatus::Success:
                text += T(L"成功", L"Success");
                break;
            case EverythingQueryStatus::Unavailable:
                text += T(L"不可用", L"Unavailable");
                break;
            case EverythingQueryStatus::SendTimeout:
                text += T(L"发送超时", L"Send timeout");
                break;
            case EverythingQueryStatus::ReplyTimeout:
                text += T(L"响应超时", L"Reply timeout");
                break;
            case EverythingQueryStatus::ProtocolError:
                text += T(L"协议错误", L"Protocol error");
                break;
            case EverythingQueryStatus::Cancelled:
                text += T(L"已取消", L"Cancelled");
                break;
            }

            text += T(
                L" · 返回 ",
                L" · returned ");
            text += std::to_wstring(
                ipc.lastResultCount);

            text += T(
                L" · ",
                L" · ");
            text += std::to_wstring(
                std::max<std::int64_t>(
                    0,
                    (ipc.lastLatency.count() +
                     500) /
                        1000));
            text += L" ms";
        }
    } else if (bootstrap.running) {
        text =
            T(L"◌ 正在准备 Everything",
              L"◌ Preparing Everything");
        text += L"\r\n";

        switch (bootstrap.stage) {
        case win::EverythingBootstrapStage::Discovering:
            text += T(L"正在检测本机已有版本", L"Looking for an existing copy");
            break;
        case win::EverythingBootstrapStage::StartingExisting:
            text += T(L"正在启动已有版本", L"Starting existing copy");
            break;
        case win::EverythingBootstrapStage::DownloadingManifest:
            text += T(L"正在获取官方校验清单", L"Fetching official checksum manifest");
            break;
        case win::EverythingBootstrapStage::DownloadingPackage:
            text += T(L"正在下载官方标准便携版", L"Downloading official standard portable build");
            if (bootstrap.downloadedBytes > 0) {
                text += L" · ";
                text += FormatBytes(
                    bootstrap.downloadedBytes);
                if (bootstrap.totalBytes > 0) {
                    text += L" / ";
                    text += FormatBytes(
                        bootstrap.totalBytes);
                }
            }
            break;
        case win::EverythingBootstrapStage::VerifyingPackage:
            text += T(L"正在校验 SHA-256", L"Verifying SHA-256");
            break;
        case win::EverythingBootstrapStage::ExtractingPackage:
            text += T(L"正在解压便携版", L"Extracting portable build");
            break;
        case win::EverythingBootstrapStage::InstallingService:
        case win::EverythingBootstrapStage::RepairingService:
            text += T(L"正在配置 Everything Service，请确认 UAC", L"Configuring Everything Service; confirm UAC");
            break;
        case win::EverythingBootstrapStage::WaitingForService:
            text += T(L"正在等待 Everything Service", L"Waiting for Everything Service");
            break;
        case win::EverythingBootstrapStage::StartingManaged:
        case win::EverythingBootstrapStage::WaitingForIpc:
            text += T(L"正在启动托管实例并等待 IPC", L"Starting the managed instance and waiting for IPC");
            break;
        default:
            text += T(L"正在应用托管配置", L"Applying managed configuration");
            break;
        }
    } else {
        showGetEverything = true;
        showRecheck = true;

        if (bootstrap.failure ==
                win::EverythingBootstrapFailure::
                    ServiceRepairRequired) {
            text =
                T(L"⚠ Everything Service 路径需要修复",
                  L"⚠ Everything Service path needs repair");
            text += L"\r\n";
            text +=
                T(L"选择“获取并启动 Everything”后会在一次 UAC 授权中修复 ALTRun Next 自己管理的 Service。",
                  L"Choose Get and start Everything to repair only the ALTRun-managed service with one UAC confirmation.");
        } else if (
            bootstrap.failure ==
                win::EverythingBootstrapFailure::
                    ServiceRequired) {
            text =
                T(L"⚠ Everything 缺少 NTFS 索引服务",
                  L"⚠ Everything is missing its NTFS indexing service");
            text += L"\r\n";
            text +=
                T(L"选择“获取并启动 Everything”安装 Everything Service。",
                  L"Choose Get and start Everything to install the Everything Service.");
        } else if (
            bootstrap.stage ==
                win::EverythingBootstrapStage::
                    Failed) {
            text =
                T(L"⚠ Everything 自动准备失败",
                  L"⚠ Automatic Everything setup failed");

            if (bootstrap.nativeError != 0) {
                text += L" · ";
                text +=
                    T(L"系统错误 ",
                      L"Native error ");
                text += std::to_wstring(
                    bootstrap.nativeError);
            }

            text += L"\r\n";
            text +=
                T(L"可重新检测已有标准版，或再次获取托管便携版。",
                  L"Recheck an existing standard copy or try the managed portable setup again.");
        } else if (
            ipc.ambiguousNamedInstances) {
            text =
                T(L"⚠ 检测到多个 Everything 命名实例",
                  L"⚠ Multiple named Everything instances detected");
            text += L"\r\n";
            text +=
                T(L"无法安全自动选择，请保留一个可用标准实例后重新检测。",
                  L"Automatic selection is ambiguous; keep one usable standard instance and recheck.");
        } else {
            text =
                T(L"○ 未检测到可用的 Everything",
                  L"○ No usable Everything instance detected");
            text += L"\r\n";
            text +=
                T(L"可重新检测已有标准版，或让 ALTRun Next 获取官方标准便携版。",
                  L"Recheck an existing standard copy or let ALTRun Next fetch the official standard portable build.");
        }
    }

    const bool visible =
        page_ == Page::Providers;

    ShowWindow(
        providerGetEverything_,
        visible &&
                showGetEverything
            ? SW_SHOW
            : SW_HIDE);

    ShowWindow(
        providerRecheckEverything_,
        visible &&
                showRecheck
            ? SW_SHOW
            : SW_HIDE);

    SetWindowTextW(
        providerStatus_,
        text.c_str());
}

void SettingsWindow::AcquireEverything() {
    if (app_.EverythingBootstrapStatus()
            .running) {
        return;
    }

    const int answer =
        MessageBoxW(
            hwnd_,
            T(L"ALTRun Next 会先尝试复用本机已有的 Everything。\n\n如果需要自己的托管便携版，会从 voidtools 官方获取 Everything 1.4.1.1032 标准版（不是 Lite）并校验 SHA-256。托管版会安装 / 启动 Everything Service 来完成 NTFS 索引，以普通用户后台运行，并隐藏 Everything 托盘图标。\n\n首次安装服务时 Windows 会弹出一次 UAC，请确认后继续。\n\n继续吗？",
              L"ALTRun Next will first try to reuse an existing Everything copy.\n\nIf its managed portable copy is needed, it will fetch the official Everything 1.4.1.1032 standard build (not Lite) from voidtools and verify SHA-256. The managed copy installs / starts the Everything Service for NTFS indexing, runs in the background as a standard user, and hides the Everything tray icon.\n\nWindows will show one UAC prompt when the service is first installed.\n\nContinue?"),
            T(L"获取并启动 Everything",
              L"Get and start Everything"),
            MB_YESNO |
                MB_ICONINFORMATION |
                MB_DEFBUTTON2);

    if (answer != IDYES) {
        return;
    }

    if (!app_.StartEverythingBootstrap(
            true)) {
        RefreshProviderStatus();
        return;
    }

    RefreshProviderStatus();
}

void SettingsWindow::RecheckEverything() {
    app_.StartEverythingBootstrap(false);
    RefreshProviderStatus();
}

void SettingsWindow::RefreshDataCompatibilityStatus() {
    if (!dataStatus_) {
        return;
    }

    const std::wstring warning =
        app_.DataCompatibilityWarning();

    if (!warning.empty()) {
        SetWindowTextW(
            dataStatus_,
            warning.c_str());
    } else if (page_ == Page::Data) {
        SetWindowTextW(
            dataStatus_,
            T(L"数据健康检查正常：目录可写，且本次启动未发生备份恢复或兼容保护。",
              L"Data health check passed: the directory is writable and no backup recovery or compatibility protection was needed this startup."));
    }
}

void SettingsWindow::OnDynamicProviderStatusChanged() {
    if (page_ == Page::Providers) {
        RefreshProviderStatus();
    }
}

void SettingsWindow::OnProgramIndexRefreshCompleted(
    int outcome) {

    if (!dataStatus_) {
        return;
    }

    if (!app_.DataCompatibilityWarning()
             .empty()) {
        RefreshDataCompatibilityStatus();
        RefreshProviderStatus();
        return;
    }

    const wchar_t* status = nullptr;

    if (outcome ==
        static_cast<int>(
            ProviderRefreshOutcome::Success)) {
        status =
            T(L"程序索引已在后台刷新完成。",
              L"Program index refreshed in the background.");
    } else if (
        outcome ==
        static_cast<int>(
            ProviderRefreshOutcome::Partial)) {
        status =
            T(L"程序索引已部分刷新；失败来源继续使用各自的旧缓存。",
              L"Program index partially refreshed. Failed sources keep their previous cache.");
    } else {
        status =
            T(L"程序索引后台刷新失败，继续使用现有缓存。",
              L"Background index refresh failed. The existing cache is still in use.");
    }

    SetWindowTextW(
        dataStatus_,
        status);

    RefreshProviderStatus();
}

void SettingsWindow::UpdateNavLabels() {
    const bool zh =
        app_.SettingsData().language == Language::ZhCN;

    const auto label = [&](Page,
                           const wchar_t* zhText,
                           const wchar_t* enText) {
        return std::wstring(
            zh ? zhText : enText);
    };

    SetWindowTextW(
        navGeneral_,
        label(Page::General, L"常规", L"General").c_str());
    SetWindowTextW(
        navHotkeys_,
        label(Page::Hotkeys, L"快捷键", L"Hotkeys").c_str());
    SetWindowTextW(
        navProviders_,
        label(Page::Providers, L"搜索来源", L"Search sources").c_str());
    SetWindowTextW(
        navAppearance_,
        label(Page::Appearance, L"外观", L"Appearance").c_str());
    SetWindowTextW(
        navData_,
        label(Page::Data, L"数据", L"Data").c_str());
    SetWindowTextW(
        navDiagnostics_,
        label(Page::Diagnostics, L"诊断", L"Diagnostics").c_str());
    SetWindowTextW(
        navAbout_,
        label(Page::About, L"关于", L"About").c_str());
}

void SettingsWindow::UpdatePageHeader() {
    switch (page_) {
    case Page::General:
        SetWindowTextW(
            pageTitle_,
            T(L"常规", L"General"));
        SetWindowTextW(
            pageDescription_,
            T(L"控制启动器行为、搜索方式和呼出位置。",
              L"Control launcher behavior, search interaction and placement."));
        break;

    case Page::Hotkeys:
        SetWindowTextW(
            pageTitle_,
            T(L"快捷键", L"Hotkeys"));
        SetWindowTextW(
            pageDescription_,
            T(L"集中管理全局呼出和启动器内部动作热键；新增动作会统一注册到这里。",
              L"Manage global activation and launcher action bindings in one place; future hotkey actions register here."));
        break;

    case Page::Diagnostics:
        SetWindowTextW(
            pageTitle_,
            T(L"诊断", L"Diagnostics"));
        SetWindowTextW(
            pageDescription_,
            T(L"查看进程内存、搜索数据、后台任务、Smart Actions 与 Windows 呼出上下文，建立性能与故障诊断基线。",
              L"Inspect process memory, search data, background work, Smart Actions and Windows activation context for performance and troubleshooting baselines."));
        break;

    case Page::Appearance:
        SetWindowTextW(
            pageTitle_,
            T(L"外观", L"Appearance"));
        SetWindowTextW(
            pageDescription_,
            T(L"选择启动器样式和 ALTRun Next 的界面语言。",
              L"Choose launcher style and the ALTRun Next interface language."));
        break;

    case Page::Providers:
        SetWindowTextW(
            pageTitle_,
            T(L"搜索来源", L"Search sources"));
        SetWindowTextW(
            pageDescription_,
            T(L"控制 Windows 应用来源和 Everything 文件 / 文件夹搜索，并查看运行状态。",
              L"Choose Windows application sources and Everything file/folder search, and inspect runtime status."));
        break;

    case Page::Data:
        SetWindowTextW(
            pageTitle_,
            T(L"数据", L"Data"));
        SetWindowTextW(
            pageDescription_,
            T(L"导入、导出和维护 ALTRun Next 的本地数据。",
              L"Import, export and maintain ALTRun Next local data."));
        break;

    case Page::About:
        SetWindowTextW(
            pageTitle_,
            T(L"关于", L"About"));
        SetWindowTextW(
            pageDescription_,
            T(L"查看版本、更新状态和项目入口。",
              L"View version information, update status and project links."));
        break;
    }
}

void SettingsWindow::ShowPage(Page page) {
    if (page_ == Page::Providers &&
        page != Page::Providers) {
        KillTimer(
            hwnd_,
            kProviderStatusTimerId);
    }

    if (page_ == Page::Diagnostics &&
        page != Page::Diagnostics) {
        KillTimer(
            hwnd_,
            kDiagnosticsStatusTimerId);
    }

    if (page != page_ &&
        page == Page::General) {
        generalScrollOffset_ = 0;
    }

    page_ = page;

    const auto setVisible = [](const std::vector<HWND>& controls, bool visible) {
        for (HWND control : controls) {
            ShowWindow(
                control,
                visible ? SW_SHOW : SW_HIDE);
        }
    };

    setVisible(generalControls_, page == Page::General);
    setVisible(hotkeyControls_, page == Page::Hotkeys);
    setVisible(diagnosticsControls_, page == Page::Diagnostics);
    setVisible(appearanceControls_, page == Page::Appearance);
    setVisible(providerControls_, page == Page::Providers);
    setVisible(dataControls_, page == Page::Data);
    setVisible(aboutControls_, page == Page::About);

    if (
        page == Page::Hotkeys) {
        RefreshHotkeyPage();
    } else if (
        page == Page::Diagnostics) {
        SetTimer(
            hwnd_,
            kDiagnosticsStatusTimerId,
            1000,
            nullptr);
        RefreshActionDiagnostics();
    } else if (
        page == Page::Providers) {
        SetTimer(
            hwnd_,
            kProviderStatusTimerId,
            1000,
            nullptr);
        RefreshProviderStatus();
    } else if (
        page == Page::Data) {
        RefreshDataCompatibilityStatus();
    }

    UpdateNavLabels();
    UpdatePageHeader();
    Layout();

    RedrawWindow(
        hwnd_,
        nullptr,
        nullptr,
        RDW_INVALIDATE | RDW_ERASE |
            RDW_ALLCHILDREN | RDW_UPDATENOW);
}

void SettingsWindow::ApplyClassicBehaviorControl(
    UINT id) {

    if (syncing_) return;

    const auto settings =
        app_.SettingsData();

    bool pinyinSearch =
        settings.pinyinSearch;
    bool wildcardMatching =
        settings.wildcardMatching;
    bool numericQuickLaunch =
        settings.numericQuickLaunch;
    bool executeSingleResult =
        settings
            .executeSingleResultImmediately;

    switch (id) {
    case kIdPinyinSearch:
        pinyinSearch =
            !pinyinSearch;
        break;
    case kIdWildcardMatching:
        wildcardMatching =
            !wildcardMatching;
        break;
    case kIdNumericQuickLaunch:
        numericQuickLaunch =
            !numericQuickLaunch;
        break;
    case kIdExecuteSingleResult:
        executeSingleResult =
            !executeSingleResult;
        break;
    case kIdNumericQuickLaunchOrder:
    case 0:
        break;
    default:
        return;
    }

    const int orderIndex =
        static_cast<int>(
            SendMessageW(
                numericQuickLaunchOrder_,
                CB_GETCURSEL,
                0,
                0));

    const std::string order =
        orderIndex == 1
            ? "zero-to-nine"
            : "one-to-zero";

    if (!app_.SetClassicBehavior(
            wildcardMatching,
            numericQuickLaunch,
            order,
            executeSingleResult,
            pinyinSearch)) {

        MessageBoxW(
            hwnd_,
            T(L"无法保存搜索行为设置。",
              L"Unable to save search-behavior settings."),
            L"ALTRun Next",
            MB_OK | MB_ICONERROR);

        RefreshFromSettings();
        return;
    }

    EnableWindow(
        numericQuickLaunchOrder_,
        numericQuickLaunch
            ? TRUE
            : FALSE);
}

void SettingsWindow::ImportCommands(bool legacyMode) {
    std::array<wchar_t, 32768> file{};

    const wchar_t nextFilter[] =
        L"ALTRun Next TSV\0*.tsv;*.txt\0"
        L"All files\0*.*\0\0";

    const wchar_t legacyFilter[] =
        L"Legacy ALTRun files\0*.ini;*.txt;*.tsv\0"
        L"All files\0*.*\0\0";

    OPENFILENAMEW open{};
    open.lStructSize = sizeof(open);
    open.hwndOwner = hwnd_;
    open.lpstrFile = file.data();
    open.nMaxFile =
        static_cast<DWORD>(file.size());
    open.lpstrFilter =
        legacyMode ? legacyFilter : nextFilter;
    open.nFilterIndex = 1;
    open.Flags =
        OFN_FILEMUSTEXIST |
        OFN_PATHMUSTEXIST |
        OFN_EXPLORER |
        OFN_NOCHANGEDIR;

    if (!GetOpenFileNameW(&open)) {
        return;
    }

    std::size_t imported = 0;
    std::size_t skipped = 0;

    if (!app_.ImportUserCommands(
            std::filesystem::path(file.data()),
            legacyMode,
            &imported,
            &skipped)) {

        MessageBoxW(
            hwnd_,
            T(L"导入失败，原数据未被替换。",
              L"Import failed. Existing data was not replaced."),
            T(L"导入快捷项", L"Import shortcuts"),
            MB_OK | MB_ICONERROR);
        return;
    }

    std::wstring status =
        T(L"导入完成：新增 ", L"Import complete: added ");
    status += std::to_wstring(imported);
    status += T(L" 项，跳过 ", L", skipped ");
    status += std::to_wstring(skipped);
    status += T(L" 项。", L".");

    SetWindowTextW(
        dataStatus_,
        status.c_str());
}

void SettingsWindow::ExportCommands() {
    std::array<wchar_t, 32768> file{};
    const std::wstring defaultName =
        L"ALTRunNext-commands.tsv";

    std::copy(
        defaultName.begin(),
        defaultName.end(),
        file.begin());

    const wchar_t filter[] =
        L"ALTRun Next TSV\0*.tsv\0"
        L"All files\0*.*\0\0";

    OPENFILENAMEW save{};
    save.lStructSize = sizeof(save);
    save.hwndOwner = hwnd_;
    save.lpstrFile = file.data();
    save.nMaxFile =
        static_cast<DWORD>(file.size());
    save.lpstrFilter = filter;
    save.nFilterIndex = 1;
    save.lpstrDefExt = L"tsv";
    save.Flags =
        OFN_OVERWRITEPROMPT |
        OFN_PATHMUSTEXIST |
        OFN_EXPLORER |
        OFN_NOCHANGEDIR;

    if (!GetSaveFileNameW(&save)) {
        return;
    }

    if (!app_.ExportUserCommands(
            std::filesystem::path(file.data()))) {

        MessageBoxW(
            hwnd_,
            T(L"导出失败。",
              L"Export failed."),
            T(L"导出快捷项", L"Export shortcuts"),
            MB_OK | MB_ICONERROR);
        return;
    }

    SetWindowTextW(
        dataStatus_,
        T(L"快捷项已导出。",
          L"Shortcuts exported."));
}

void SettingsWindow::ClearUsageHistory() {
    const int answer =
        MessageBoxW(
            hwnd_,
            T(L"确定清空全部使用次数和最近使用时间吗？\n\n快捷项本身不会被删除。",
              L"Clear all launch counts and recent-use timestamps?\n\nShortcuts themselves will not be deleted."),
            T(L"清空使用历史", L"Clear usage history"),
            MB_YESNO | MB_ICONWARNING);

    if (answer != IDYES) return;

    if (!app_.ClearUsageHistory()) {
        MessageBoxW(
            hwnd_,
            T(L"清空使用历史失败。",
              L"Failed to clear usage history."),
            L"ALTRun Next",
            MB_OK | MB_ICONERROR);
        return;
    }

    SetWindowTextW(
        dataStatus_,
        T(L"使用历史已清空，排序已立即刷新。",
          L"Usage history cleared. Ranking has been refreshed."));
}

void SettingsWindow::RebuildProgramIndex() {
    app_.RebuildProgramIndex();

    SetWindowTextW(
        dataStatus_,
        T(L"程序索引正在后台重建，当前搜索结果仍可使用。",
          L"Program index is rebuilding in the background. Current search results remain available."));
}

void SettingsWindow::RestoreDefaultSettings() {
    const int answer =
        MessageBoxW(
            hwnd_,
            T(L"确定恢复默认设置吗？\n\n不会删除你的快捷项和使用历史。",
              L"Restore default settings?\n\nYour shortcuts and usage history will not be deleted."),
            T(L"恢复默认设置", L"Restore default settings"),
            MB_YESNO | MB_ICONWARNING);

    if (answer != IDYES) return;

    if (!app_.RestoreDefaultSettings()) {
        MessageBoxW(
            hwnd_,
            T(L"恢复失败。默认热键 Alt + Space 可能发生冲突，或系统设置无法写入。",
              L"Restore failed. The default Alt + Space hotkey may be unavailable, or a system setting could not be written."),
            T(L"恢复默认设置", L"Restore default settings"),
            MB_OK | MB_ICONERROR);
        return;
    }

    RefreshFromSettings();

    SetWindowTextW(
        dataStatus_,
        T(L"设置已恢复为默认值。",
          L"Settings restored to defaults."));
}

void SettingsWindow::ToggleGeneralSetting(UINT id) {
    if (syncing_) return;

    const auto settings = app_.SettingsData();

    bool hideAfterLaunch = settings.hideAfterLaunch;
    bool clearQueryOnShow = settings.clearQueryOnShow;
    bool hideOnFocusLost = settings.hideOnFocusLost;
    bool showTrayIcon = settings.showTrayIcon;

    switch (id) {
    case kIdStartWithWindows:
        if (!app_.SetStartWithWindows(
                !settings.startWithWindows)) {
            MessageBoxW(
                hwnd_,
                T(L"无法更新 Windows 开机启动项。",
                  L"Unable to update the Windows startup entry."),
                L"ALTRun Next",
                MB_OK | MB_ICONERROR);
            RefreshFromSettings();
        }
        return;

    case kIdShowOnStartup:
        if (!app_.SetShowOnStartup(
                !settings.showOnStartup)) {
            MessageBoxW(
                hwnd_,
                T(L"无法保存启动时显示设置。",
                  L"Unable to save the show-on-startup setting."),
                L"ALTRun Next",
                MB_OK | MB_ICONERROR);
            RefreshFromSettings();
        }
        return;

    case kIdShowResultIcons:
        if (!app_.SetShowResultIcons(
                !settings.showResultIcons)) {
            MessageBoxW(
                hwnd_,
                T(L"无法保存搜索结果图标设置。",
                  L"Unable to save the result-icon setting."),
                L"ALTRun Next",
                MB_OK | MB_ICONERROR);
            RefreshFromSettings();
        }
        return;

    case kIdHideAfterLaunch:
        hideAfterLaunch = !hideAfterLaunch;
        break;
    case kIdClearQueryOnShow:
        clearQueryOnShow = !clearQueryOnShow;
        break;
    case kIdHideOnFocusLost:
        hideOnFocusLost = !hideOnFocusLost;
        break;
    case kIdShowTrayIcon:
        showTrayIcon = !showTrayIcon;
        break;
    default:
        return;
    }

    app_.SetGeneralSettings(
        hideAfterLaunch,
        clearQueryOnShow,
        hideOnFocusLost,
        showTrayIcon,
        settings.popupMonitor);
}

void SettingsWindow::ToggleProviderSetting(
    UINT id) {

    if (syncing_) {
        return;
    }

    std::string providerId;

    switch (id) {
    case kIdProviderStartMenu:
        providerId =
            std::string(
                providers::kStartMenu);
        break;
    case kIdProviderPackaged:
        providerId =
            std::string(
                providers::kPackaged);
        break;
    case kIdProviderAppPaths:
        providerId =
            std::string(
                providers::kAppPaths);
        break;
    case kIdProviderPath:
        providerId =
            std::string(
                providers::kPath);
        break;
    case kIdProviderEverything:
        providerId =
            std::string(
                providers::
                    kEverythingFilesystem);
        break;
    default:
        return;
    }

    const bool enabled =
        !ToggleChecked(id);

    const bool everythingProvider =
        providerId ==
        providers::
            kEverythingFilesystem;

    if (!app_.SetProviderEnabled(
            std::move(providerId),
            enabled)) {
        MessageBoxW(
            hwnd_,
            everythingProvider
                ? T(L"无法更改 Everything 搜索源状态。\n\n如果使用的是 ALTRun Next 托管版，请确认 Windows 管理员权限请求；取消 UAC 后开关会恢复原状态。",
                    L"Unable to change the Everything search-source state.\n\nIf ALTRun Next manages this Everything copy, approve the Windows administrator request. Cancelling UAC restores the previous setting.")
                : T(L"无法保存搜索来源设置。",
                    L"Unable to save search-source settings."),
            L"ALTRun Next",
            MB_OK | MB_ICONERROR);

        RefreshFromSettings();
    }
}

void SettingsWindow::ApplyMonitorControl() {
    if (syncing_) return;

    const auto settings =
        app_.SettingsData();

    const int monitorIndex =
        static_cast<int>(
            SendMessageW(
                popupMonitor_,
                CB_GETCURSEL,
                0,
                0));

    std::string popupMonitor =
        "cursor";

    if (monitorIndex == 1) {
        popupMonitor = "active";
    } else if (monitorIndex == 2) {
        popupMonitor = "primary";
    }

    app_.SetGeneralSettings(
        settings.hideAfterLaunch,
        settings.clearQueryOnShow,
        settings.hideOnFocusLost,
        settings.showTrayIcon,
        std::move(
            popupMonitor));
}

void SettingsWindow::ApplyWindowPlacementControls() {
    if (syncing_) return;

    const int launcherIndex =
        static_cast<int>(
            SendMessageW(
                launcherPlacement_,
                CB_GETCURSEL,
                0,
                0));

    const int settingsIndex =
        static_cast<int>(
            SendMessageW(
                settingsPlacement_,
                CB_GETCURSEL,
                0,
                0));

    std::string launcherMode =
        "top";

    if (launcherIndex == 1) {
        launcherMode = "center";
    } else if (launcherIndex == 2) {
        launcherMode = "last";
    }

    const std::string settingsMode =
        settingsIndex == 1
            ? "last"
            : "center";

    if (!app_.SetWindowPlacementSettings(
            std::move(
                launcherMode),
            settingsMode)) {
        MessageBoxW(
            hwnd_,
            T(L"无法保存窗口位置设置。",
              L"Unable to save window placement settings."),
            L"ALTRun Next",
            MB_OK | MB_ICONERROR);
        RefreshFromSettings();
    }
}

void SettingsWindow::ApplyAppearanceControls() {
    if (syncing_) return;

    const int styleIndex =
        static_cast<int>(
            SendMessageW(
                uiStyle_,
                CB_GETCURSEL,
                0,
                0));

    const int languageIndex =
        static_cast<int>(
            SendMessageW(
                language_,
                CB_GETCURSEL,
                0,
                0));

    const UiStyle style =
        styleIndex == 1
            ? UiStyle::ModernCompact
            : UiStyle::Classic;

    const Language language =
        languageIndex == 1
            ? Language::EnUS
            : Language::ZhCN;

    if (style !=
        app_.SettingsData().uiStyle) {
        app_.SetUiStyle(style);
    }

    if (language !=
        app_.SettingsData().language) {
        app_.SetLanguage(language);
    }
}

bool SettingsWindow::ToggleChecked(
    UINT id) const {

    const auto& settings =
        app_.SettingsData();

    const auto providerEnabled =
        [&](std::string_view providerId) {
            return providers::IsEnabled(
                settings.providerEnabled,
                providerId,
                true);
        };

    switch (id) {
    case kIdStartWithWindows:
        return settings.startWithWindows;
    case kIdShowOnStartup:
        return settings.showOnStartup;
    case kIdHideAfterLaunch:
        return settings.hideAfterLaunch;
    case kIdClearQueryOnShow:
        return settings.clearQueryOnShow;
    case kIdHideOnFocusLost:
        return settings.hideOnFocusLost;
    case kIdShowTrayIcon:
        return settings.showTrayIcon;
    case kIdShowResultIcons:
        return settings.showResultIcons;
    case kIdUpdateAutoCheck:
        return settings.autoCheckUpdates;
    case kIdPinyinSearch:
        return settings.pinyinSearch;
    case kIdWildcardMatching:
        return settings.wildcardMatching;
    case kIdNumericQuickLaunch:
        return settings.numericQuickLaunch;
    case kIdExecuteSingleResult:
        return settings
            .executeSingleResultImmediately;
    case kIdProviderStartMenu:
        return providerEnabled(
            providers::kStartMenu);
    case kIdProviderPackaged:
        return providerEnabled(
            providers::kPackaged);
    case kIdProviderAppPaths:
        return providerEnabled(
            providers::kAppPaths);
    case kIdProviderPath:
        return providerEnabled(
            providers::kPath);
    case kIdProviderEverything:
        return providers::IsEnabled(
            settings.providerEnabled,
            providers::
                kEverythingFilesystem,
            false);
    default:
        return false;
    }
}

settings_layout::GeneralLayoutMetrics
SettingsWindow::BuildGeneralLayout(
    int scrollOffset) const {

    RECT client{};
    GetClientRect(
        hwnd_,
        &client);

    return settings_layout::
        BuildGeneralLayout(
            static_cast<int>(
                client.right),
            dpi_,
            scrollOffset,
            kSidebarWidthLogical);
}

void SettingsWindow::UpdateGeneralScrollBar() {
    if (!hwnd_) return;

    if (page_ != Page::General) {
        generalScrollOffset_ = 0;
        ShowScrollBar(
            hwnd_,
            SB_VERT,
            FALSE);
        return;
    }

    RECT client{};
    GetClientRect(
        hwnd_,
        &client);

    auto full =
        BuildGeneralLayout(0);

    int maximum =
        settings_layout::
            MaxScrollOffset(
                full,
                static_cast<int>(
                    client.bottom),
                dpi_);

    ShowScrollBar(
        hwnd_,
        SB_VERT,
        maximum > 0);

    // Showing the scrollbar changes the client width and can switch the
    // General page into its narrow stacked layout. Recalculate once using
    // the final client area before clamping the scroll position.
    GetClientRect(
        hwnd_,
        &client);

    full =
        BuildGeneralLayout(0);

    maximum =
        settings_layout::
            MaxScrollOffset(
                full,
                static_cast<int>(
                    client.bottom),
                dpi_);

    generalScrollOffset_ =
        std::clamp(
            generalScrollOffset_,
            0,
            maximum);

    SCROLLINFO info{};
    info.cbSize =
        sizeof(info);
    info.fMask =
        SIF_RANGE |
        SIF_PAGE |
        SIF_POS;
    info.nMin = 0;
    info.nMax =
        std::max(
            0,
            full.contentBottom +
                Scale(10) - 1);
    info.nPage =
        static_cast<UINT>(
            std::max(
                1,
                static_cast<int>(
                    client.bottom)));
    info.nPos =
        generalScrollOffset_;

    SetScrollInfo(
        hwnd_,
        SB_VERT,
        &info,
        TRUE);
}

void SettingsWindow::ScrollGeneral(
    int delta) {

    if (page_ != Page::General ||
        delta == 0) {
        return;
    }

    UpdateGeneralScrollBar();

    SCROLLINFO info{};
    info.cbSize =
        sizeof(info);
    info.fMask =
        SIF_RANGE |
        SIF_PAGE;

    GetScrollInfo(
        hwnd_,
        SB_VERT,
        &info);

    const int maximum =
        std::max(
            0,
            info.nMax -
                static_cast<int>(
                    info.nPage) +
                1);

    const int next =
        std::clamp(
            generalScrollOffset_ +
                delta,
            0,
            maximum);

    if (next ==
        generalScrollOffset_) {
        return;
    }

    generalScrollOffset_ =
        next;

    Layout();

    RedrawWindow(
        hwnd_,
        nullptr,
        nullptr,
        RDW_INVALIDATE |
            RDW_ERASE |
            RDW_ALLCHILDREN |
            RDW_UPDATENOW);
}

RECT SettingsWindow::BehaviorCardRect() const {
    const auto metrics =
        BuildGeneralLayout(
            generalScrollOffset_);

    return {
        metrics.behavior.left,
        metrics.behavior.top,
        metrics.behavior.right,
        metrics.behavior.bottom,
    };
}

RECT SettingsWindow::SearchBehaviorCardRect() const {
    const auto metrics =
        BuildGeneralLayout(
            generalScrollOffset_);

    return {
        metrics.search.left,
        metrics.search.top,
        metrics.search.right,
        metrics.search.bottom,
    };
}

RECT SettingsWindow::PlacementCardRect() const {
    const auto metrics =
        BuildGeneralLayout(
            generalScrollOffset_);

    return {
        metrics.placement.left,
        metrics.placement.top,
        metrics.placement.right,
        metrics.placement.bottom,
    };
}

void SettingsWindow::Layout() {
    if (!hwnd_) return;

    UpdateGeneralScrollBar();

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
        Scale(18),
        navWidth,
        Scale(32),
        TRUE);
    MoveWindow(
        brandSubtitle_,
        sidebarMargin,
        Scale(49),
        navWidth,
        Scale(22),
        TRUE);

    std::array<HWND, 6> primaryNav{
        navGeneral_,
        navHotkeys_,
        navProviders_,
        navAppearance_,
        navData_,
        navDiagnostics_,
    };

    const int navTop =
        Scale(88);

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
        Scale(26) -
            pageScroll,
        contentWidth,
        Scale(42),
        TRUE);

    MoveWindow(
        pageDescription_,
        contentLeft,
        Scale(70) -
            pageScroll,
        contentWidth,
        Scale(38),
        TRUE);

    if (page_ == Page::General) {
        const auto metrics =
            BuildGeneralLayout(
                generalScrollOffset_);

        MoveWindow(
            generalBehaviorTitle_,
            metrics.behavior.left,
            metrics.behaviorTitleTop,
            metrics.behavior.right -
                metrics.behavior.left,
            Scale(28),
            TRUE);

        MoveWindow(
            searchBehaviorTitle_,
            metrics.search.left,
            metrics.searchTitleTop,
            metrics.search.right -
                metrics.search.left,
            Scale(28),
            TRUE);

        const int toggleHeight =
            Scale(
                settings_layout::
                    kToggleRowLogical);

        const int behaviorX =
            metrics.behavior.left +
            Scale(1);
        const int behaviorWidth =
            metrics.behavior.right -
            metrics.behavior.left -
            Scale(2);

        std::array<HWND, 7> behaviorRows{
            startWithWindows_,
            showOnStartup_,
            hideAfterLaunch_,
            clearQueryOnShow_,
            hideOnFocusLost_,
            showTrayIcon_,
            showResultIcons_,
        };

        for (std::size_t i = 0;
             i < behaviorRows.size();
             ++i) {
            MoveWindow(
                behaviorRows[i],
                behaviorX,
                metrics.behavior.top +
                    Scale(1) +
                    static_cast<int>(i) *
                        toggleHeight,
                behaviorWidth,
                toggleHeight,
                TRUE);
        }

        const int searchX =
            metrics.search.left +
            Scale(1);
        const int searchWidth =
            metrics.search.right -
            metrics.search.left -
            Scale(2);

        std::array<HWND, 4> searchRows{
            pinyinSearch_,
            wildcardMatching_,
            numericQuickLaunch_,
            executeSingleResult_,
        };

        for (std::size_t i = 0;
             i < searchRows.size();
             ++i) {
            MoveWindow(
                searchRows[i],
                searchX,
                metrics.search.top +
                    Scale(1) +
                    static_cast<int>(i) *
                        toggleHeight,
                searchWidth,
                toggleHeight,
                TRUE);
        }

        const int orderTop =
            metrics.search.top +
            toggleHeight * 4;

        MoveWindow(
            numericQuickLaunchOrderLabel_,
            metrics.search.left +
                Scale(18),
            orderTop +
                Scale(19),
            Scale(150),
            Scale(24),
            TRUE);

        MoveWindow(
            numericQuickLaunchOrder_,
            metrics.search.right -
                Scale(168),
            orderTop +
                Scale(14),
            Scale(150),
            Scale(180),
            TRUE);

        MoveWindow(
            placementSectionTitle_,
            metrics.placement.left,
            metrics.placementTitleTop,
            metrics.placement.right -
                metrics.placement.left,
            Scale(28),
            TRUE);

        const int rowHeight =
            Scale(
                ui::kSettingsComboRowLogical);
        const int labelX =
            metrics.placement.left +
            Scale(18);
        const int placementWidth =
            metrics.placement.right -
            metrics.placement.left;
        const int comboWidth =
            std::clamp(
                placementWidth * 46 / 100,
                Scale(180),
                Scale(250));
        const int comboX =
            metrics.placement.right -
            comboWidth -
            Scale(18);

        struct PlacementRow {
            HWND label;
            HWND description;
            HWND combo;
        };

        const std::array<PlacementRow, 3>
            placementRows{{
                {
                    popupMonitorLabel_,
                    popupMonitorDescription_,
                    popupMonitor_,
                },
                {
                    launcherPlacementLabel_,
                    launcherPlacementDescription_,
                    launcherPlacement_,
                },
                {
                    settingsPlacementLabel_,
                    settingsPlacementDescription_,
                    settingsPlacement_,
                },
            }};

        for (std::size_t i = 0;
             i < placementRows.size();
             ++i) {
            const int top =
                metrics.placement.top +
                static_cast<int>(i) *
                    rowHeight;

            MoveWindow(
                placementRows[i].label,
                labelX,
                top + Scale(10),
                std::max(
                    Scale(180),
                    comboX -
                        labelX -
                        Scale(16)),
                Scale(24),
                TRUE);

            MoveWindow(
                placementRows[i].description,
                labelX,
                top + Scale(34),
                std::max(
                    Scale(180),
                    comboX -
                        labelX -
                        Scale(16)),
                Scale(24),
                TRUE);

            MoveWindow(
                placementRows[i].combo,
                comboX,
                top + Scale(18),
                comboWidth,
                Scale(220),
                TRUE);
        }

        MoveWindow(
            generalNote_,
            metrics.placement.left,
            metrics.noteTop,
            metrics.placement.right -
                metrics.placement.left,
            Scale(36),
            TRUE);
    }

    if (page_ == Page::Hotkeys) {
        const int top =
            Scale(150);
        const int gap =
            Scale(18);
        const int listWidth =
            std::min(
                Scale(285),
                std::max(
                    Scale(230),
                    contentWidth * 38 / 100));
        const int editorX =
            contentLeft +
            listWidth +
            gap;
        const int editorWidth =
            std::max(
                Scale(300),
                contentRight -
                    editorX);

        MoveWindow(
            hotkeyActionList_,
            contentLeft + Scale(12),
            top + Scale(12),
            listWidth - Scale(24),
            Scale(390),
            TRUE);

        MoveWindow(
            hotkeyEditorTitle_,
            editorX + Scale(18),
            top + Scale(18),
            editorWidth - Scale(36),
            Scale(30),
            TRUE);

        MoveWindow(
            hotkeyEditorDescription_,
            editorX + Scale(18),
            top + Scale(54),
            editorWidth - Scale(36),
            Scale(54),
            TRUE);

        MoveWindow(
            hotkeyScope_,
            editorX + Scale(18),
            top + Scale(112),
            editorWidth - Scale(36),
            Scale(26),
            TRUE);

        MoveWindow(
            hotkeyEnabled_,
            editorX + Scale(18),
            top + Scale(150),
            editorWidth - Scale(36),
            Scale(28),
            TRUE);

        MoveWindow(
            hotkeyCapture_,
            editorX + Scale(18),
            top + Scale(194),
            std::min(
                Scale(220),
                editorWidth - Scale(36)),
            Scale(36),
            TRUE);

        MoveWindow(
            hotkeyResetCurrent_,
            editorX + Scale(18),
            top + Scale(244),
            std::min(
                Scale(220),
                editorWidth - Scale(36)),
            Scale(36),
            TRUE);

        MoveWindow(
            hotkeyPageStatus_,
            editorX + Scale(18),
            top + Scale(294),
            editorWidth - Scale(36),
            Scale(64),
            TRUE);

        MoveWindow(
            hotkeyResetAll_,
            editorX + Scale(18),
            top + Scale(372),
            std::min(
                Scale(240),
                editorWidth - Scale(36)),
            Scale(36),
            TRUE);

        MoveWindow(
            hotkeyPageNote_,
            contentLeft,
            top + Scale(430),
            contentWidth,
            Scale(58),
            TRUE);
    }

    if (page_ == Page::Providers) {
        const int width =
            std::min(
                contentWidth,
                Scale(720));

        MoveWindow(
            providerSectionTitle_,
            contentLeft,
            Scale(138),
            width,
            Scale(28),
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

        const int filesTitleTop =
            170 +
            settings_layout::
                kToggleRowLogical * 4 +
            22;

        MoveWindow(
            providerFilesTitle_,
            contentLeft,
            Scale(filesTitleTop),
            width,
            Scale(28),
            TRUE);

        const int filesTop =
            filesTitleTop + 32;

        MoveWindow(
            providerEverything_,
            contentLeft + Scale(1),
            Scale(filesTop + 1),
            width - Scale(2),
            rowHeight,
            TRUE);

        MoveWindow(
            providerStatus_,
            contentLeft + Scale(18),
            Scale(
                filesTop +
                settings_layout::
                    kToggleRowLogical +
                12),
            width - Scale(36),
            Scale(96),
            TRUE);

        MoveWindow(
            providerGetEverything_,
            contentLeft + Scale(18),
            Scale(filesTop + 174),
            Scale(210),
            Scale(34),
            TRUE);

        MoveWindow(
            providerRecheckEverything_,
            contentLeft + Scale(240),
            Scale(filesTop + 174),
            Scale(128),
            Scale(34),
            TRUE);

        MoveWindow(
            providerNote_,
            contentLeft + Scale(18),
            Scale(filesTop + 220),
            width - Scale(36),
            Scale(60),
            TRUE);
    }

    if (page_ == Page::Appearance) {
        const int width =
            std::min(
                contentWidth,
                Scale(680));

        MoveWindow(
            appearanceLauncherTitle_,
            contentLeft,
            Scale(138),
            width,
            Scale(28),
            TRUE);

        MoveWindow(
            uiStyleLabel_,
            contentLeft + Scale(18),
            Scale(192),
            Scale(220),
            Scale(26),
            TRUE);
        MoveWindow(
            uiStyle_,
            contentLeft + width -
                Scale(308),
            Scale(183),
            Scale(290),
            Scale(220),
            TRUE);

        MoveWindow(
            appearanceAppTitle_,
            contentLeft,
            Scale(270),
            width,
            Scale(28),
            TRUE);

        MoveWindow(
            languageLabel_,
            contentLeft + Scale(18),
            Scale(324),
            Scale(220),
            Scale(26),
            TRUE);
        MoveWindow(
            language_,
            contentLeft + width -
                Scale(308),
            Scale(315),
            Scale(290),
            Scale(220),
            TRUE);

        MoveWindow(
            appearanceNote_,
            contentLeft,
            Scale(398),
            width,
            Scale(54),
            TRUE);
    }

    if (page_ == Page::Data) {
        const int width =
            std::min(
                contentWidth,
                Scale(720));

        MoveWindow(
            dataPathLabel_,
            contentLeft,
            Scale(138),
            width,
            Scale(28),
            TRUE);
        MoveWindow(
            dataPath_,
            contentLeft + Scale(18),
            Scale(190),
            width - Scale(220),
            Scale(28),
            TRUE);
        MoveWindow(
            openDataFolder_,
            contentLeft + width -
                Scale(174),
            Scale(181),
            Scale(156),
            Scale(36),
            TRUE);

        MoveWindow(
            dataTransferLabel_,
            contentLeft,
            Scale(280),
            width,
            Scale(28),
            TRUE);
        MoveWindow(
            dataImportTsv_,
            contentLeft + Scale(18),
            Scale(330),
            Scale(170),
            Scale(36),
            TRUE);
        MoveWindow(
            dataImportLegacy_,
            contentLeft + Scale(200),
            Scale(330),
            Scale(190),
            Scale(36),
            TRUE);
        MoveWindow(
            dataExport_,
            contentLeft + Scale(402),
            Scale(330),
            Scale(170),
            Scale(36),
            TRUE);

        MoveWindow(
            dataMaintenanceLabel_,
            contentLeft,
            Scale(420),
            width,
            Scale(28),
            TRUE);
        MoveWindow(
            dataClearUsage_,
            contentLeft + Scale(18),
            Scale(470),
            Scale(170),
            Scale(36),
            TRUE);
        MoveWindow(
            dataRebuildIndex_,
            contentLeft + Scale(200),
            Scale(470),
            Scale(190),
            Scale(36),
            TRUE);
        MoveWindow(
            dataResetSettings_,
            contentLeft + Scale(402),
            Scale(470),
            Scale(170),
            Scale(36),
            TRUE);

        MoveWindow(
            dataStatus_,
            contentLeft,
            Scale(552),
            width,
            Scale(74),
            TRUE);
    }

    if (page_ == Page::Diagnostics) {
        const int width =
            std::min(
                contentWidth,
                Scale(760));
        const int gap =
            Scale(18);
        const int column =
            (width - gap) / 2;

        MoveWindow(
            diagnosticsMemoryTitle_,
            contentLeft + Scale(18),
            Scale(164),
            column - Scale(36),
            Scale(24),
            TRUE);
        MoveWindow(
            diagnosticsMemoryStatus_,
            contentLeft + Scale(18),
            Scale(194),
            column - Scale(36),
            Scale(70),
            TRUE);

        MoveWindow(
            diagnosticsSearchTitle_,
            contentLeft + column +
                gap + Scale(18),
            Scale(164),
            column - Scale(36),
            Scale(24),
            TRUE);
        MoveWindow(
            diagnosticsSearchStatus_,
            contentLeft + column +
                gap + Scale(18),
            Scale(194),
            column - Scale(36),
            Scale(78),
            TRUE);

        const int actionTop =
            310;

        MoveWindow(
            actionsWindowsTitle_,
            contentLeft + Scale(18),
            Scale(actionTop + 14),
            width - Scale(36),
            Scale(24),
            TRUE);
        MoveWindow(
            actionsWindowsStatus_,
            contentLeft + Scale(18),
            Scale(actionTop + 44),
            width - Scale(36),
            Scale(105),
            TRUE);

        MoveWindow(
            actionsClipboardTitle_,
            contentLeft + Scale(18),
            Scale(actionTop + 180),
            width - Scale(36),
            Scale(24),
            TRUE);
        MoveWindow(
            actionsClipboardStatus_,
            contentLeft + Scale(18),
            Scale(actionTop + 210),
            width - Scale(36),
            Scale(48),
            TRUE);

        MoveWindow(
            actionsWebTitle_,
            contentLeft + Scale(18),
            Scale(actionTop + 288),
            width - Scale(36),
            Scale(24),
            TRUE);
        MoveWindow(
            actionsWebStatus_,
            contentLeft + Scale(18),
            Scale(actionTop + 318),
            width - Scale(36),
            Scale(48),
            TRUE);

        MoveWindow(
            actionsNote_,
            contentLeft,
            Scale(actionTop + 390),
            width,
            Scale(48),
            TRUE);
    }

    if (page_ == Page::About) {
        const int width =
            std::min(
                contentWidth,
                Scale(680));

        MoveWindow(
            aboutName_,
            contentLeft,
            Scale(136),
            width,
            Scale(42),
            TRUE);
        MoveWindow(
            aboutVersion_,
            contentLeft,
            Scale(182),
            width,
            Scale(26),
            TRUE);
        MoveWindow(
            aboutDescription_,
            contentLeft,
            Scale(214),
            width,
            Scale(40),
            TRUE);

        MoveWindow(
            updateSectionTitle_,
            contentLeft,
            Scale(282),
            width,
            Scale(28),
            TRUE);

        MoveWindow(
            updateChannelLabel_,
            contentLeft + Scale(18),
            Scale(334),
            Scale(160),
            Scale(26),
            TRUE);
        MoveWindow(
            updateChannel_,
            contentLeft + width -
                Scale(236),
            Scale(325),
            Scale(218),
            Scale(220),
            TRUE);

        MoveWindow(
            updateAutoCheck_,
            contentLeft + Scale(1),
            Scale(376),
            width - Scale(2),
            Scale(
                settings_layout::
                    kToggleRowLogical),
            TRUE);

        MoveWindow(
            updateStatus_,
            contentLeft + Scale(18),
            Scale(448),
            width - Scale(36),
            Scale(50),
            TRUE);

        MoveWindow(
            updateCheck_,
            contentLeft + Scale(18),
            Scale(506),
            Scale(150),
            Scale(36),
            TRUE);
        MoveWindow(
            updateInstall_,
            contentLeft + Scale(180),
            Scale(506),
            Scale(180),
            Scale(36),
            TRUE);

        MoveWindow(
            aboutProjectTitle_,
            contentLeft,
            Scale(576),
            width,
            Scale(28),
            TRUE);
        MoveWindow(
            openGitHub_,
            contentLeft + Scale(18),
            Scale(622),
            Scale(150),
            Scale(36),
            TRUE);
    }
}

RECT SettingsWindow::ProviderCardRect() const {
    return PageCardRect(
        170,
        settings_layout::
            kToggleRowLogical * 4,
        720);
}

RECT SettingsWindow::PageCardRect(
    int topLogical,
    int heightLogical,
    int maxWidthLogical) const {

    RECT client{};
    GetClientRect(
        hwnd_,
        &client);

    const int contentLeft =
        Scale(kSidebarWidthLogical) +
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
            Scale(320),
            contentRight -
                contentLeft);

    const int width =
        std::min(
            contentWidth,
            Scale(maxWidthLogical));

    return {
        contentLeft,
        Scale(topLogical),
        contentLeft + width,
        Scale(
            topLogical +
            heightLogical),
    };
}

void SettingsWindow::DrawNavigationButton(
    const DRAWITEMSTRUCT& item) {

    RECT rect =
        item.rcItem;

    bool selected = false;

    switch (item.CtlID) {
    case kIdNavGeneral:
        selected =
            page_ == Page::General;
        break;
    case kIdNavHotkeys:
        selected =
            page_ == Page::Hotkeys;
        break;
    case kIdNavProviders:
        selected =
            page_ == Page::Providers;
        break;
    case kIdNavAppearance:
        selected =
            page_ == Page::Appearance;
        break;
    case kIdNavData:
        selected =
            page_ == Page::Data;
        break;
    case kIdNavDiagnostics:
        selected =
            page_ == Page::Diagnostics;
        break;
    case kIdNavAbout:
        selected =
            page_ == Page::About;
        break;
    default:
        break;
    }

    const bool pressed =
        (item.itemState &
         ODS_SELECTED) != 0;

    RECT surface = rect;
    InflateRect(
        &surface,
        -Scale(2),
        -Scale(1));

    const COLORREF background =
        pressed
            ? kCardPressed
            : selected
                ? kPalette
                      .selectionBackground
                : kSidebarBackground;

    HBRUSH fill =
        CreateSolidBrush(
            background);
    HPEN pen =
        CreatePen(
            PS_SOLID,
            1,
            background);

    HGDIOBJ oldBrush =
        SelectObject(
            item.hDC,
            fill);
    HGDIOBJ oldPen =
        SelectObject(
            item.hDC,
            pen);

    RoundRect(
        item.hDC,
        surface.left,
        surface.top,
        surface.right,
        surface.bottom,
        Scale(7),
        Scale(7));

    SelectObject(
        item.hDC,
        oldBrush);
    SelectObject(
        item.hDC,
        oldPen);
    DeleteObject(fill);
    DeleteObject(pen);

    if (selected) {
        RECT accent{
            surface.left,
            surface.top + Scale(7),
            surface.left + Scale(3),
            surface.bottom - Scale(7),
        };

        HBRUSH accentBrush =
            CreateSolidBrush(
                kAccent);
        FillRect(
            item.hDC,
            &accent,
            accentBrush);
        DeleteObject(
            accentBrush);
    }

    wchar_t textBuffer[96]{};
    GetWindowTextW(
        item.hwndItem,
        textBuffer,
        static_cast<int>(
            std::size(
                textBuffer)));

    SetBkMode(
        item.hDC,
        TRANSPARENT);
    SetTextColor(
        item.hDC,
        kText);

    HGDIOBJ oldFont =
        SelectObject(
            item.hDC,
            selected
                ? sectionFont_
                : normalFont_);

    RECT textRect{
        surface.left + Scale(16),
        surface.top,
        surface.right - Scale(12),
        surface.bottom,
    };

    DrawTextW(
        item.hDC,
        textBuffer,
        -1,
        &textRect,
        DT_LEFT |
            DT_SINGLELINE |
            DT_VCENTER |
            DT_END_ELLIPSIS |
            DT_NOPREFIX);

    SelectObject(
        item.hDC,
        oldFont);

    if (item.itemState &
        ODS_FOCUS) {
        RECT focus =
            surface;
        InflateRect(
            &focus,
            -Scale(7),
            -Scale(5));
        DrawFocusRect(
            item.hDC,
            &focus);
    }
}

void SettingsWindow::DrawHotkeyActionItem(
    const DRAWITEMSTRUCT& item) {

    if (item.itemID ==
            static_cast<UINT>(-1) ||
        item.itemID >=
            hotkeyActionIds_.size()) {
        return;
    }

    RECT rect =
        item.rcItem;

    const bool selected =
        (item.itemState &
         ODS_SELECTED) != 0;

    const COLORREF background =
        selected
            ? kPalette.selectionBackground
            : kCardBackground;

    HBRUSH fill =
        CreateSolidBrush(
            background);
    FillRect(
        item.hDC,
        &rect,
        fill);
    DeleteObject(fill);

    const std::string& actionId =
        hotkeyActionIds_[
            item.itemID];

    const std::wstring title =
        HotkeyActionLabel(
            actionId);

    const auto binding =
        EffectiveHotkeyBinding(
            app_.SettingsData()
                .hotkeyBindings,
            actionId);

    const std::wstring detail =
        binding.enabled
            ? FormatHotkeyBinding(
                  actionId)
            : T(L"已禁用",
                L"Disabled");

    SetBkMode(
        item.hDC,
        TRANSPARENT);

    HGDIOBJ oldFont =
        SelectObject(
            item.hDC,
            sectionFont_);

    RECT titleRect{
        rect.left + Scale(12),
        rect.top + Scale(6),
        rect.right - Scale(10),
        rect.top + Scale(28),
    };

    SetTextColor(
        item.hDC,
        kText);

    DrawTextW(
        item.hDC,
        title.c_str(),
        -1,
        &titleRect,
        DT_LEFT |
            DT_SINGLELINE |
            DT_VCENTER |
            DT_END_ELLIPSIS |
            DT_NOPREFIX);

    SelectObject(
        item.hDC,
        normalFont_);

    RECT detailRect{
        titleRect.left,
        rect.top + Scale(27),
        titleRect.right,
        rect.bottom - Scale(5),
    };

    SetTextColor(
        item.hDC,
        kMuted);

    DrawTextW(
        item.hDC,
        detail.c_str(),
        -1,
        &detailRect,
        DT_LEFT |
            DT_SINGLELINE |
            DT_VCENTER |
            DT_END_ELLIPSIS |
            DT_NOPREFIX);

    SelectObject(
        item.hDC,
        oldFont);

    if (item.itemID + 1 <
        hotkeyActionIds_.size()) {
        HPEN separator =
            CreatePen(
                PS_SOLID,
                1,
                kBorder);

        HGDIOBJ oldPen =
            SelectObject(
                item.hDC,
                separator);

        MoveToEx(
            item.hDC,
            rect.left + Scale(12),
            rect.bottom - 1,
            nullptr);
        LineTo(
            item.hDC,
            rect.right - Scale(10),
            rect.bottom - 1);

        SelectObject(
            item.hDC,
            oldPen);
        DeleteObject(
            separator);
    }

    if (item.itemState &
        ODS_FOCUS) {
        RECT focus =
            rect;
        InflateRect(
            &focus,
            -Scale(5),
            -Scale(4));
        DrawFocusRect(
            item.hDC,
            &focus);
    }
}

void SettingsWindow::DrawActionButton(
    const DRAWITEMSTRUCT& item) {

    RECT rect =
        item.rcItem;

    const bool disabled =
        (item.itemState &
         ODS_DISABLED) != 0;
    const bool pressed =
        (item.itemState &
         ODS_SELECTED) != 0;

    const bool primary =
        item.CtlID ==
            kIdUpdateInstall;
    const bool danger =
        item.CtlID ==
            kIdDataResetSettings;

    COLORREF fillColor =
        pressed
            ? kCardPressed
            : RGB(255, 255, 255);
    COLORREF borderColor =
        kBorder;
    COLORREF textColor =
        disabled
            ? RGB(155, 162, 171)
            : kText;

    if (primary && !disabled) {
        fillColor =
            pressed
                ? RGB(0, 99, 177)
                : kAccent;
        borderColor =
            fillColor;
        textColor =
            RGB(255, 255, 255);
    } else if (
        danger &&
        !disabled) {
        textColor =
            RGB(190, 45, 45);
        borderColor =
            RGB(226, 185, 185);
    }

    RECT surface =
        rect;
    InflateRect(
        &surface,
        -1,
        -1);

    HBRUSH fill =
        CreateSolidBrush(
            fillColor);
    HPEN pen =
        CreatePen(
            PS_SOLID,
            1,
            borderColor);

    HGDIOBJ oldBrush =
        SelectObject(
            item.hDC,
            fill);
    HGDIOBJ oldPen =
        SelectObject(
            item.hDC,
            pen);

    RoundRect(
        item.hDC,
        surface.left,
        surface.top,
        surface.right,
        surface.bottom,
        Scale(6),
        Scale(6));

    SelectObject(
        item.hDC,
        oldBrush);
    SelectObject(
        item.hDC,
        oldPen);
    DeleteObject(fill);
    DeleteObject(pen);

    wchar_t buffer[160]{};
    GetWindowTextW(
        item.hwndItem,
        buffer,
        static_cast<int>(
            std::size(buffer)));

    SetBkMode(
        item.hDC,
        TRANSPARENT);
    SetTextColor(
        item.hDC,
        textColor);

    HGDIOBJ oldFont =
        SelectObject(
            item.hDC,
            normalFont_);

    RECT textRect =
        surface;
    InflateRect(
        &textRect,
        -Scale(10),
        0);

    DrawTextW(
        item.hDC,
        buffer,
        -1,
        &textRect,
        DT_CENTER |
            DT_VCENTER |
            DT_SINGLELINE |
            DT_END_ELLIPSIS |
            DT_NOPREFIX);

    SelectObject(
        item.hDC,
        oldFont);

    if (item.itemState &
        ODS_FOCUS) {
        RECT focus =
            surface;
        InflateRect(
            &focus,
            -Scale(5),
            -Scale(4));
        DrawFocusRect(
            item.hDC,
            &focus);
    }
}

void SettingsWindow::DrawGeneralToggle(
    const DRAWITEMSTRUCT& item) {

    RECT rect =
        item.rcItem;

    const bool pressed =
        (item.itemState &
         ODS_SELECTED) != 0;

    HBRUSH rowBrush =
        CreateSolidBrush(
            pressed
                ? kCardPressed
                : kCardBackground);

    FillRect(
        item.hDC,
        &rect,
        rowBrush);
    DeleteObject(
        rowBrush);

    const UINT id =
        static_cast<UINT>(
            item.CtlID);

    const bool checked =
        ToggleChecked(id);

    const wchar_t* title = L"";
    const wchar_t* description = L"";

    switch (id) {
    case kIdStartWithWindows:
        title = T(
            L"开机启动",
            L"Start with Windows");
        description = T(
            L"登录 Windows 后自动启动 ALTRun Next。",
            L"Launch ALTRun Next automatically after signing in.");
        break;
    case kIdShowOnStartup:
        title = T(
            L"启动时显示启动器",
            L"Show launcher on startup");
        description = T(
            L"程序启动后立即显示 Launcher；默认保持后台静默启动。",
            L"Show the launcher when the app starts; otherwise start silently.");
        break;
    case kIdHideAfterLaunch:
        title = T(
            L"执行后自动隐藏",
            L"Hide after launch");
        description = T(
            L"成功执行结果后自动收起 Launcher。",
            L"Hide the launcher after a result is executed successfully.");
        break;
    case kIdClearQueryOnShow:
        title = T(
            L"呼出时清空搜索",
            L"Clear query on open");
        description = T(
            L"每次呼出 Launcher 时从空白搜索开始。",
            L"Start with an empty query every time the launcher opens.");
        break;
    case kIdHideOnFocusLost:
        title = T(
            L"失去焦点时隐藏",
            L"Hide when focus is lost");
        description = T(
            L"切换到其他窗口时自动收起 Launcher。",
            L"Hide the launcher when another window receives focus.");
        break;
    case kIdShowTrayIcon:
        title = T(
            L"显示系统托盘图标",
            L"Show system tray icon");
        description = T(
            L"保留托盘入口，用于设置、重新加载和退出。",
            L"Keep the tray entry for Settings, reload and exit.");
        break;
    case kIdShowResultIcons:
        title = T(
            L"显示搜索结果图标",
            L"Show search result icons");
        description = T(
            L"关闭时不解析或缓存 Shell 图标，连续搜索会更轻。",
            L"Disable Shell icon resolution and caching for lighter continuous search.");
        break;
    case kIdPinyinSearch:
        title = T(
            L"启用拼音搜索",
            L"Enable Pinyin search");
        description = T(
            L"支持全拼、首字母和混合拼音匹配中文。",
            L"Match Chinese using full, initial and mixed Pinyin.");
        break;
    case kIdWildcardMatching:
        title = T(
            L"允许 * / ? 通配符",
            L"Enable * / ? wildcards");
        description = T(
            L"查询包含通配符时使用 glob 匹配。",
            L"Use glob matching when the query contains wildcard characters.");
        break;
    case kIdNumericQuickLaunch:
        title = T(
            L"数字键快速执行结果",
            L"Quick launch with number keys");
        description = T(
            L"Classic 下数字键直接执行对应结果。",
            L"In Classic mode, number keys execute matching results.");
        break;
    case kIdExecuteSingleResult:
        title = T(
            L"仅剩一个结果时立即执行",
            L"Execute immediately when one result remains");
        description = T(
            L"非空查询只剩唯一结果时立即启动；默认关闭以避免误触。",
            L"Launch when a non-empty query narrows to one result; off by default.");
        break;
    case kIdProviderStartMenu:
        title = T(
            L"开始菜单",
            L"Start Menu");
        description = T(
            L"发现当前用户和所有用户开始菜单中的快捷方式与程序。",
            L"Discover shortcuts and programs from Windows Start Menu locations.");
        break;
    case kIdProviderPackaged:
        title = L"Windows Apps";
        description = T(
            L"发现 Microsoft Store、UWP 和 MSIX 应用。",
            L"Discover Microsoft Store, UWP and MSIX applications.");
        break;
    case kIdProviderAppPaths:
        title = L"App Paths";
        description = T(
            L"从注册表 App Paths 发现传统桌面程序。",
            L"Discover traditional desktop applications from the App Paths registry.");
        break;
    case kIdProviderPath:
        title = L"PATH";
        description = T(
            L"发现 PATH 中的 EXE、COM、BAT 和 CMD。",
            L"Discover EXE, COM, BAT and CMD files exposed through PATH.");
        break;
    case kIdProviderEverything:
        title = T(
            L"Everything 文件与文件夹",
            L"Everything files & folders");
        description = T(
            L"通过标准版 Everything IPC 实时搜索文件和文件夹。",
            L"Search files and folders live through standard Everything IPC.");
        break;
    case kIdUpdateAutoCheck:
        title = T(
            L"自动检查更新",
            L"Automatically check for updates");
        description = T(
            L"后台最多每天检查一次当前更新通道。",
            L"Check the selected update channel in the background at most once per day.");
        break;
    default:
        break;
    }

    const int switchWidth =
        Scale(40);
    const int switchHeight =
        Scale(22);
    const int switchLeft =
        rect.right -
        Scale(18) -
        switchWidth;
    const int switchTop =
        rect.top +
        (rect.bottom -
         rect.top -
         switchHeight) / 2;

    RECT track{
        switchLeft,
        switchTop,
        switchLeft + switchWidth,
        switchTop + switchHeight,
    };

    const COLORREF trackColor =
        checked
            ? kAccent
            : RGB(214, 219, 226);

    HBRUSH trackBrush =
        CreateSolidBrush(
            trackColor);
    HPEN trackPen =
        CreatePen(
            PS_SOLID,
            1,
            checked
                ? kAccent
                : RGB(184, 191, 201));

    HGDIOBJ oldBrush =
        SelectObject(
            item.hDC,
            trackBrush);
    HGDIOBJ oldPen =
        SelectObject(
            item.hDC,
            trackPen);

    RoundRect(
        item.hDC,
        track.left,
        track.top,
        track.right,
        track.bottom,
        switchHeight,
        switchHeight);

    SelectObject(
        item.hDC,
        oldBrush);
    SelectObject(
        item.hDC,
        oldPen);
    DeleteObject(
        trackBrush);
    DeleteObject(
        trackPen);

    const int knobSize =
        Scale(16);
    const int knobInset =
        Scale(3);
    const int knobLeft =
        checked
            ? track.right -
                knobInset -
                knobSize
            : track.left +
                knobInset;

    HBRUSH knobBrush =
        CreateSolidBrush(
            RGB(255, 255, 255));
    HPEN knobPen =
        CreatePen(
            PS_SOLID,
            1,
            RGB(255, 255, 255));

    oldBrush =
        SelectObject(
            item.hDC,
            knobBrush);
    oldPen =
        SelectObject(
            item.hDC,
            knobPen);

    Ellipse(
        item.hDC,
        knobLeft,
        track.top + knobInset,
        knobLeft + knobSize,
        track.top +
            knobInset +
            knobSize);

    SelectObject(
        item.hDC,
        oldBrush);
    SelectObject(
        item.hDC,
        oldPen);
    DeleteObject(
        knobBrush);
    DeleteObject(
        knobPen);

    SetBkMode(
        item.hDC,
        TRANSPARENT);

    HGDIOBJ oldFont =
        SelectObject(
            item.hDC,
            normalFont_);

    RECT titleRect{
        rect.left + Scale(18),
        rect.top + Scale(9),
        switchLeft - Scale(16),
        rect.top + Scale(31),
    };

    SetTextColor(
        item.hDC,
        kText);
    DrawTextW(
        item.hDC,
        title,
        -1,
        &titleRect,
        DT_LEFT |
            DT_SINGLELINE |
            DT_VCENTER |
            DT_END_ELLIPSIS |
            DT_NOPREFIX);

    RECT descriptionRect{
        titleRect.left,
        rect.top + Scale(32),
        titleRect.right,
        rect.bottom - Scale(6),
    };

    SetTextColor(
        item.hDC,
        kMuted);
    DrawTextW(
        item.hDC,
        description,
        -1,
        &descriptionRect,
        DT_LEFT |
            DT_SINGLELINE |
            DT_VCENTER |
            DT_END_ELLIPSIS |
            DT_NOPREFIX);

    SelectObject(
        item.hDC,
        oldFont);

    const bool lastRow =
        id == kIdShowResultIcons ||
        id == kIdProviderPath ||
        id == kIdProviderEverything ||
        id == kIdUpdateAutoCheck;

    if (!lastRow) {
        HPEN separator =
            CreatePen(
                PS_SOLID,
                1,
                kBorder);

        oldPen =
            SelectObject(
                item.hDC,
                separator);

        MoveToEx(
            item.hDC,
            rect.left + Scale(18),
            rect.bottom - 1,
            nullptr);

        LineTo(
            item.hDC,
            rect.right - Scale(18),
            rect.bottom - 1);

        SelectObject(
            item.hDC,
            oldPen);
        DeleteObject(
            separator);
    }

    if (item.itemState &
        ODS_FOCUS) {
        RECT focus =
            rect;
        InflateRect(
            &focus,
            -Scale(7),
            -Scale(5));
        DrawFocusRect(
            item.hDC,
            &focus);
    }
}

void SettingsWindow::PositionForShow() {
    RECT rect{};
    GetWindowRect(
        hwnd_,
        &rect);

    const int requestedWidth =
        rect.right - rect.left;
    const int requestedHeight =
        rect.bottom - rect.top;

    const auto& settings =
        app_.SettingsData();

    if (settings.settingsPlacement ==
            "last" &&
        settings.settingsLastPositionValid) {

        RECT requested{
            settings.settingsLastX,
            settings.settingsLastY,
            settings.settingsLastX +
                requestedWidth,
            settings.settingsLastY +
                requestedHeight,
        };

        HMONITOR monitor =
            MonitorFromRect(
                &requested,
                MONITOR_DEFAULTTONEAREST);

        MONITORINFO info{
            sizeof(info)};

        if (GetMonitorInfoW(
                monitor,
                &info)) {

            const auto clamped =
                settings_layout::
                    ClampRectToWorkArea(
                        {
                            requested.left,
                            requested.top,
                            requested.right,
                            requested.bottom,
                        },
                        {
                            info.rcWork.left,
                            info.rcWork.top,
                            info.rcWork.right,
                            info.rcWork.bottom,
                        });

            SetWindowPos(
                hwnd_,
                nullptr,
                clamped.left,
                clamped.top,
                clamped.right -
                    clamped.left,
                clamped.bottom -
                    clamped.top,
                SWP_NOZORDER |
                    SWP_NOACTIVATE);
            return;
        }
    }

    POINT cursor{};
    GetCursorPos(
        &cursor);

    HMONITOR monitor =
        MonitorFromPoint(
            cursor,
            MONITOR_DEFAULTTONEAREST);

    MONITORINFO info{
        sizeof(info)};
    GetMonitorInfoW(
        monitor,
        &info);

    const int workWidth =
        info.rcWork.right -
        info.rcWork.left;
    const int workHeight =
        info.rcWork.bottom -
        info.rcWork.top;

    const int width =
        std::min(
            requestedWidth,
            workWidth);
    const int height =
        std::min(
            requestedHeight,
            workHeight);

    const int x =
        info.rcWork.left +
        std::max(
            0,
            (workWidth - width) / 2);
    const int y =
        info.rcWork.top +
        std::max(
            0,
            (workHeight - height) / 2);

    SetWindowPos(
        hwnd_,
        nullptr,
        x,
        y,
        width,
        height,
        SWP_NOZORDER |
            SWP_NOACTIVATE);
}

void SettingsWindow::OnUpdateStatusChanged() {
    RefreshUpdateStatus();
}

void SettingsWindow::ApplyUpdateSettings() {
    if (syncing_) {
        return;
    }

    const int channelIndex =
        static_cast<int>(
            SendMessageW(
                updateChannel_,
                CB_GETCURSEL,
                0,
                0));

    const UpdateChannel channel =
        channelIndex == 1
            ? UpdateChannel::Development
            : UpdateChannel::Stable;

    const bool autoCheck =
        app_.SettingsData()
            .autoCheckUpdates;

    if (!app_.SetUpdateSettings(
            autoCheck,
            channel)) {
        MessageBoxW(
            hwnd_,
            T(L"无法保存更新设置。",
              L"Could not save update settings."),
            L"ALTRun Next",
            MB_OK | MB_ICONERROR);
    }

    RefreshFromSettings();
}

void SettingsWindow::RefreshUpdateStatus() {
    if (!updateStatus_ ||
        !updateCheck_ ||
        !updateInstall_) {
        return;
    }

    const auto status =
        app_.UpdateStatus();

    std::wstring text;

    switch (status.stage) {
    case win::UpdateStage::Idle:
        text =
            T(L"尚未检查更新。",
              L"Updates have not been checked yet.");
        break;

    case win::UpdateStage::Checking:
        text =
            T(L"正在检查更新...",
              L"Checking for updates...");
        break;

    case win::UpdateStage::UpToDate:
        text =
            T(L"已是最新版本。",
              L"You're up to date.");
        break;

    case win::UpdateStage::ChannelNotNewer:
        text =
            T(L"稳定版通道最新为 v",
              L"The stable channel currently ends at v");
        text += std::wstring(
            status.availableVersion.begin(),
            status.availableVersion.end());
        text +=
            T(L"；当前版本较新，不会降级。",
              L"; this build is newer, so no downgrade will be offered.");
        break;

    case win::UpdateStage::Available:
        text =
            T(L"发现新版本：",
              L"New version available: ");
        text += std::wstring(
            status.availableVersion.begin(),
            status.availableVersion.end());
        break;

    case win::UpdateStage::Downloading:
        text =
            T(L"正在下载更新",
              L"Downloading update");

        if (status.downloadedBytes > 0) {
            text += L"  ·  ";
            text += FormatBytes(
                status.downloadedBytes);

            if (status.totalBytes > 0) {
                text += L" / ";
                text += FormatBytes(
                    status.totalBytes);
            }
        }
        break;

    case win::UpdateStage::Verifying:
        text =
            T(L"正在校验更新包 SHA-256...",
              L"Verifying update package SHA-256...");
        break;

    case win::UpdateStage::Extracting:
        text =
            T(L"正在准备更新文件...",
              L"Preparing update files...");
        break;

    case win::UpdateStage::ReadyToInstall:
        text =
            T(L"更新已下载并校验，正在准备安装...",
              L"Update downloaded and verified; preparing installation...");
        break;

    case win::UpdateStage::Applying:
        text =
            T(L"正在启动安全更新程序，ALTRun Next 将退出并自动重新启动。",
              L"Starting the safe updater. ALTRun Next will exit and restart automatically.");
        break;

    case win::UpdateStage::Failed:
        text =
            T(L"更新失败：",
              L"Update failed: ");

        switch (status.failure) {
        case win::UpdateFailure::ManifestDownloadFailed:
            text +=
                T(L"无法获取更新清单",
                  L"could not fetch the update manifest");
            break;
        case win::UpdateFailure::StableManifestUnavailable:
            text +=
                T(L"最新稳定版未提供应用内更新清单，请从 GitHub 手动更新",
                  L"the latest stable release does not provide an in-app update manifest; update manually from GitHub");
            break;
        case win::UpdateFailure::ManifestInvalid:
            text +=
                T(L"更新清单无效",
                  L"invalid update manifest");
            break;
        case win::UpdateFailure::UnsupportedArchitecture:
            text +=
                T(L"当前架构没有可用更新包",
                  L"no package is available for this architecture");
            break;
        case win::UpdateFailure::AssetDownloadFailed:
            text +=
                T(L"下载更新包失败",
                  L"package download failed");
            break;
        case win::UpdateFailure::AssetHashFailed:
            text +=
                T(L"无法计算更新包 SHA-256",
                  L"could not calculate package SHA-256");
            break;
        case win::UpdateFailure::AssetHashMismatch:
            text +=
                T(L"SHA-256 校验不一致，已拒绝更新",
                  L"SHA-256 mismatch; update rejected");
            break;
        case win::UpdateFailure::ExtractionFailed:
            text +=
                T(L"解压更新包失败",
                  L"could not extract the update package");
            break;
        case win::UpdateFailure::StagedPackageInvalid:
            text +=
                T(L"更新包内容不完整或版本不匹配",
                  L"staged package is incomplete or has the wrong version");
            break;
        case win::UpdateFailure::UpdaterMissing:
            text +=
                T(L"缺少 ALTRunNext.Updater.exe",
                  L"ALTRunNext.Updater.exe is missing");
            break;
        case win::UpdateFailure::LaunchUpdaterFailed:
            text +=
                T(L"无法启动更新程序",
                  L"could not start the updater");
            break;
        case win::UpdateFailure::Cancelled:
            text +=
                T(L"操作已取消",
                  L"operation cancelled");
            break;
        default:
            text +=
                T(L"未知错误",
                  L"unknown error");
            break;
        }

        if (status.nativeError != 0) {
            text +=
                T(L"  ·  系统错误 ",
                  L"  ·  native error ");
            text += std::to_wstring(
                status.nativeError);
        }
        break;
    }

    SetWindowTextW(
        updateStatus_,
        text.c_str());

    EnableWindow(
        updateCheck_,
        status.running
            ? FALSE
            : TRUE);

    EnableWindow(
        updateInstall_,
        status.stage ==
                win::UpdateStage::Available &&
            !status.running
            ? TRUE
            : FALSE);

    EnableWindow(
        updateChannel_,
        status.running
            ? FALSE
            : TRUE);

    EnableWindow(
        updateAutoCheck_,
        status.running
            ? FALSE
            : TRUE);
}

void SettingsWindow::ShowAbout() {
    if (!hwnd_) return;
    ShowPage(Page::About);
    Show();
}

void SettingsWindow::Show() {
    if (!hwnd_) return;

    // Retry a binding that previously failed, but never tear down a
    // working hotkey merely because the Settings window was opened.
    app_.RepairGlobalHotkey(false);
    RefreshFromSettings();

    if (page_ == Page::Providers) {
        SetTimer(
            hwnd_,
            kProviderStatusTimerId,
            1000,
            nullptr);
        RefreshProviderStatus();
    } else if (page_ == Page::Diagnostics) {
        SetTimer(
            hwnd_,
            kDiagnosticsStatusTimerId,
            1000,
            nullptr);
        RefreshActionDiagnostics();
    }

    if (!IsWindowVisible(hwnd_)) {
        PositionForShow();
    }

    ShowWindow(
        hwnd_,
        SW_SHOWNORMAL);

    SetForegroundWindow(hwnd_);
}

LRESULT CALLBACK SettingsWindow::WindowProc(
    HWND hwnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam) {

    SettingsWindow* self = nullptr;

    if (message == WM_NCCREATE) {
        const auto* create =
            reinterpret_cast<CREATESTRUCTW*>(lParam);

        self =
            static_cast<SettingsWindow*>(
                create->lpCreateParams);

        SetWindowLongPtrW(
            hwnd,
            GWLP_USERDATA,
            reinterpret_cast<LONG_PTR>(self));

        self->hwnd_ = hwnd;
    } else {
        self =
            reinterpret_cast<SettingsWindow*>(
                GetWindowLongPtrW(
                    hwnd,
                    GWLP_USERDATA));
    }

    if (self) {
        return self->HandleMessage(
            message,
            wParam,
            lParam);
    }

    return DefWindowProcW(
        hwnd,
        message,
        wParam,
        lParam);
}

LRESULT SettingsWindow::HandleMessage(
    UINT message,
    WPARAM wParam,
    LPARAM lParam) {

    switch (message) {
    case WM_TIMER:
        if (wParam ==
            kProviderStatusTimerId &&
            page_ == Page::Providers) {
            RefreshProviderStatus();
            return 0;
        }
        if (wParam ==
            kDiagnosticsStatusTimerId &&
            page_ == Page::Diagnostics) {
            RefreshActionDiagnostics();
            return 0;
        }
        break;

    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        if (!capturingHotkeyActionId_
                 .empty()) {
            ApplyCapturedHotkey(
                static_cast<UINT>(
                    wParam));
            return 0;
        }
        break;

    case WM_COMMAND: {
        const UINT id = LOWORD(wParam);
        const UINT notify = HIWORD(wParam);

        switch (id) {
        case kIdNavGeneral:
            if (notify == BN_CLICKED) {
                ShowPage(Page::General);
            }
            return 0;

        case kIdNavHotkeys:
            if (notify == BN_CLICKED) {
                ShowPage(Page::Hotkeys);
            }
            return 0;

        case kIdNavDiagnostics:
            if (notify == BN_CLICKED) {
                ShowPage(Page::Diagnostics);
            }
            return 0;

        case kIdNavAppearance:
            if (notify == BN_CLICKED) {
                ShowPage(Page::Appearance);
            }
            return 0;

        case kIdNavProviders:
            if (notify == BN_CLICKED) {
                ShowPage(Page::Providers);
            }
            return 0;

        case kIdNavData:
            if (notify == BN_CLICKED) {
                ShowPage(Page::Data);
            }
            return 0;

        case kIdNavAbout:
            if (notify == BN_CLICKED) {
                ShowPage(Page::About);
            }
            return 0;

        case kIdStartWithWindows:
        case kIdShowOnStartup:
        case kIdHideAfterLaunch:
        case kIdClearQueryOnShow:
        case kIdHideOnFocusLost:
        case kIdShowTrayIcon:
        case kIdShowResultIcons:
            if (notify == BN_CLICKED) {
                ToggleGeneralSetting(id);
            }
            return 0;

        case kIdPinyinSearch:
        case kIdWildcardMatching:
        case kIdNumericQuickLaunch:
        case kIdExecuteSingleResult:
            if (notify == BN_CLICKED) {
                ApplyClassicBehaviorControl(id);
            }
            return 0;

        case kIdNumericQuickLaunchOrder:
            if (notify == CBN_SELCHANGE) {
                ApplyClassicBehaviorControl(
                    kIdNumericQuickLaunchOrder);
            }
            return 0;

        case kIdHotkeyActionList:
            if (notify == LBN_SELCHANGE &&
                !syncing_) {
                const int index =
                    static_cast<int>(
                        SendMessageW(
                            hotkeyActionList_,
                            LB_GETCURSEL,
                            0,
                            0));

                if (index >= 0 &&
                    index <
                        static_cast<int>(
                            hotkeyActionIds_
                                .size())) {
                    capturingHotkeyActionId_
                        .clear();
                    LoadHotkeyEditor(
                        hotkeyActionIds_[
                            static_cast<
                                std::size_t>(
                                index)]);
                }
            }
            return 0;

        case kIdHotkeyEnabled:
            if (notify == BN_CLICKED) {
                ToggleSelectedHotkeyEnabled();
            }
            return 0;

        case kIdHotkeyCapture:
            if (notify == BN_CLICKED) {
                BeginHotkeyCapture();
            }
            return 0;

        case kIdHotkeyResetCurrent:
            if (notify == BN_CLICKED) {
                ResetSelectedHotkey();
            }
            return 0;

        case kIdHotkeyResetAll:
            if (notify == BN_CLICKED) {
                ResetAllHotkeys();
            }
            return 0;

        case kIdProviderStartMenu:
        case kIdProviderPackaged:
        case kIdProviderAppPaths:
        case kIdProviderPath:
        case kIdProviderEverything:
            if (notify == BN_CLICKED) {
                ToggleProviderSetting(id);
            }
            return 0;

        case kIdProviderGetEverything:
            if (notify == BN_CLICKED) {
                AcquireEverything();
            }
            return 0;

        case kIdProviderRecheckEverything:
            if (notify == BN_CLICKED) {
                RecheckEverything();
            }
            return 0;

        case kIdPopupMonitor:
            if (notify == CBN_SELCHANGE) {
                ApplyMonitorControl();
            }
            return 0;

        case kIdLauncherPlacement:
        case kIdSettingsPlacement:
            if (notify == CBN_SELCHANGE) {
                ApplyWindowPlacementControls();
            }
            return 0;

        case kIdDataImportTsv:
            if (notify == BN_CLICKED) {
                ImportCommands(false);
            }
            return 0;

        case kIdDataImportLegacy:
            if (notify == BN_CLICKED) {
                ImportCommands(true);
            }
            return 0;

        case kIdDataExport:
            if (notify == BN_CLICKED) {
                ExportCommands();
            }
            return 0;

        case kIdDataClearUsage:
            if (notify == BN_CLICKED) {
                ClearUsageHistory();
            }
            return 0;

        case kIdDataRebuildIndex:
            if (notify == BN_CLICKED) {
                RebuildProgramIndex();
            }
            return 0;

        case kIdDataResetSettings:
            if (notify == BN_CLICKED) {
                RestoreDefaultSettings();
            }
            return 0;

        case kIdUiStyle:
        case kIdLanguage:
            if (notify == CBN_SELCHANGE) {
                ApplyAppearanceControls();
            }
            return 0;

        case kIdOpenDataFolder:
            if (notify == BN_CLICKED) {
                app_.OpenDataFolder();
            }
            return 0;

        case kIdOpenGitHub:
            if (notify == BN_CLICKED) {
                app_.OpenProjectPage();
            }
            return 0;

        case kIdUpdateChannel:
            if (notify == CBN_SELCHANGE &&
                !syncing_) {
                ApplyUpdateSettings();
            }
            return 0;

        case kIdUpdateAutoCheck:
            if (notify == BN_CLICKED &&
                !syncing_) {
                const auto settings =
                    app_.SettingsData();

                if (!app_.SetUpdateSettings(
                        !settings.autoCheckUpdates,
                        settings.updateChannel)) {
                    MessageBoxW(
                        hwnd_,
                        T(L"无法保存更新设置。",
                          L"Could not save update settings."),
                        L"ALTRun Next",
                        MB_OK | MB_ICONERROR);
                }

                RefreshFromSettings();
            }
            return 0;

        case kIdUpdateCheck:
            if (notify == BN_CLICKED) {
                app_.StartUpdateCheck(true);
                RefreshUpdateStatus();
            }
            return 0;

        case kIdUpdateInstall:
            if (notify == BN_CLICKED) {
                app_.StartUpdateDownloadAndInstall();
                RefreshUpdateStatus();
            }
            return 0;

        default:
            break;
        }
        break;
    }

    case WM_DRAWITEM: {
        const auto* item =
            reinterpret_cast<
                DRAWITEMSTRUCT*>(
                    lParam);

        if (!item) {
            break;
        }

        if (item->CtlID ==
                kIdHotkeyActionList &&
            item->CtlType ==
                ODT_LISTBOX) {
            DrawHotkeyActionItem(
                *item);
            return TRUE;
        }

        if (item->CtlID == kIdNavGeneral ||
            item->CtlID == kIdNavHotkeys ||
            item->CtlID == kIdNavProviders ||
            item->CtlID == kIdNavAppearance ||
            item->CtlID == kIdNavData ||
            item->CtlID == kIdNavDiagnostics ||
            item->CtlID == kIdNavAbout) {
            DrawNavigationButton(
                *item);
            return TRUE;
        }

        if (item->CtlID == kIdStartWithWindows ||
            item->CtlID == kIdShowOnStartup ||
            item->CtlID == kIdHideAfterLaunch ||
            item->CtlID == kIdClearQueryOnShow ||
            item->CtlID == kIdHideOnFocusLost ||
            item->CtlID == kIdShowTrayIcon ||
            item->CtlID == kIdShowResultIcons ||
            item->CtlID == kIdPinyinSearch ||
            item->CtlID == kIdWildcardMatching ||
            item->CtlID == kIdNumericQuickLaunch ||
            item->CtlID == kIdExecuteSingleResult ||
            item->CtlID == kIdProviderStartMenu ||
            item->CtlID == kIdProviderPackaged ||
            item->CtlID == kIdProviderAppPaths ||
            item->CtlID == kIdProviderPath ||
            item->CtlID == kIdProviderEverything ||
            item->CtlID == kIdUpdateAutoCheck) {
            DrawGeneralToggle(
                *item);
            return TRUE;
        }

        if (item->CtlType == ODT_BUTTON) {
            DrawActionButton(
                *item);
            return TRUE;
        }

        break;
    }

    case WM_VSCROLL:
        if (page_ == Page::General) {
            SCROLLINFO info{};
            info.cbSize =
                sizeof(info);
            info.fMask =
                SIF_ALL;

            GetScrollInfo(
                hwnd_,
                SB_VERT,
                &info);

            int next =
                generalScrollOffset_;

            switch (LOWORD(wParam)) {
            case SB_LINEUP:
                next -= Scale(40);
                break;
            case SB_LINEDOWN:
                next += Scale(40);
                break;
            case SB_PAGEUP:
                next -=
                    static_cast<int>(
                        info.nPage);
                break;
            case SB_PAGEDOWN:
                next +=
                    static_cast<int>(
                        info.nPage);
                break;
            case SB_THUMBPOSITION:
            case SB_THUMBTRACK:
                next =
                    info.nTrackPos;
                break;
            case SB_TOP:
                next = 0;
                break;
            case SB_BOTTOM:
                next =
                    std::max(
                        0,
                        info.nMax -
                            static_cast<int>(
                                info.nPage) +
                            1);
                break;
            default:
                return 0;
            }

            ScrollGeneral(
                next -
                generalScrollOffset_);
        }
        return 0;

    case WM_MOUSEWHEEL:
        if (page_ == Page::General) {
            const int wheel =
                GET_WHEEL_DELTA_WPARAM(
                    wParam);

            if (wheel != 0) {
                ScrollGeneral(
                    -(wheel *
                      Scale(72)) /
                    WHEEL_DELTA);
            }

            return 0;
        }
        break;

    case WM_PAINT: {
        PAINTSTRUCT paint{};
        HDC dc =
            BeginPaint(
                hwnd_,
                &paint);

        RECT client{};
        GetClientRect(
            hwnd_,
            &client);

        FillRect(
            dc,
            &client,
            backgroundBrush_);

        const int sidebarX =
            Scale(
                kSidebarWidthLogical);

        RECT sidebar{
            client.left,
            client.top,
            sidebarX,
            client.bottom,
        };

        FillRect(
            dc,
            &sidebar,
            sidebarBrush_);

        HPEN separator =
            CreatePen(
                PS_SOLID,
                1,
                kBorder);

        HGDIOBJ oldPen =
            SelectObject(
                dc,
                separator);

        MoveToEx(
            dc,
            sidebarX,
            client.top,
            nullptr);
        LineTo(
            dc,
            sidebarX,
            client.bottom);

        const int contentLeft =
            sidebarX +
            Scale(
                settings_layout::
                    kContentLeftInsetLogical);

        MoveToEx(
            dc,
            contentLeft,
            Scale(116) -
                (page_ ==
                         Page::General
                     ? generalScrollOffset_
                     : 0),
            nullptr);
        LineTo(
            dc,
            client.right -
                Scale(
                    settings_layout::
                        kContentRightInsetLogical),
            Scale(116) -
                (page_ ==
                         Page::General
                     ? generalScrollOffset_
                     : 0));

        const int aboutSeparatorY =
            std::max(
                Scale(360),
                static_cast<int>(
                    client.bottom) -
                    Scale(
                        ui::
                            kSettingsNavHeightLogical) -
                    Scale(34));

        MoveToEx(
            dc,
            Scale(16),
            aboutSeparatorY,
            nullptr);
        LineTo(
            dc,
            sidebarX - Scale(16),
            aboutSeparatorY);

        SelectObject(
            dc,
            oldPen);
        DeleteObject(
            separator);

        const auto drawCard =
            [&](RECT card) {
                HBRUSH fill =
                    CreateSolidBrush(
                        kCardBackground);
                HPEN border =
                    CreatePen(
                        PS_SOLID,
                        1,
                        kBorder);

                HGDIOBJ previousBrush =
                    SelectObject(
                        dc,
                        fill);
                HGDIOBJ previousPen =
                    SelectObject(
                        dc,
                        border);

                RoundRect(
                    dc,
                    card.left,
                    card.top,
                    card.right,
                    card.bottom,
                    Scale(
                        ui::
                            kSettingsCardRadiusLogical),
                    Scale(
                        ui::
                            kSettingsCardRadiusLogical));

                SelectObject(
                    dc,
                    previousBrush);
                SelectObject(
                    dc,
                    previousPen);
                DeleteObject(fill);
                DeleteObject(border);
            };

        if (page_ == Page::General) {
            drawCard(
                BehaviorCardRect());
            drawCard(
                SearchBehaviorCardRect());
            drawCard(
                PlacementCardRect());
        } else if (
            page_ == Page::Hotkeys) {

            const int contentRight =
                client.right -
                Scale(
                    settings_layout::
                        kContentRightInsetLogical);
            const int contentWidth =
                contentRight -
                contentLeft;
            const int gap =
                Scale(18);
            const int listWidth =
                std::min(
                    Scale(285),
                    std::max(
                        Scale(230),
                        contentWidth *
                            38 / 100));

            drawCard({
                contentLeft,
                Scale(150),
                contentLeft +
                    listWidth,
                Scale(564),
            });

            drawCard({
                contentLeft +
                    listWidth +
                    gap,
                Scale(150),
                contentRight,
                Scale(564),
            });
        } else if (
            page_ == Page::Providers) {
            drawCard(
                ProviderCardRect());

            const int filesTitleTop =
                170 +
                settings_layout::
                    kToggleRowLogical * 4 +
                22;

            drawCard(
                PageCardRect(
                    filesTitleTop + 32,
                    294,
                    720));
        } else if (
            page_ == Page::Appearance) {
            drawCard(
                PageCardRect(
                    170,
                    68,
                    680));
            drawCard(
                PageCardRect(
                    302,
                    68,
                    680));
        } else if (
            page_ == Page::Data) {
            drawCard(
                PageCardRect(
                    170,
                    66,
                    720));
            drawCard(
                PageCardRect(
                    312,
                    72,
                    720));
            drawCard(
                PageCardRect(
                    452,
                    72,
                    720));
        } else if (
            page_ == Page::Diagnostics) {

            const int contentRight =
                client.right -
                Scale(
                    settings_layout::
                        kContentRightInsetLogical);
            const int width =
                std::min(
                    contentRight -
                        contentLeft,
                    Scale(760));
            const int gap =
                Scale(18);
            const int column =
                (width - gap) / 2;

            drawCard({
                contentLeft,
                Scale(150),
                contentLeft +
                    column,
                Scale(286),
            });

            drawCard({
                contentLeft +
                    column +
                    gap,
                Scale(150),
                contentLeft +
                    width,
                Scale(286),
            });

            drawCard(
                PageCardRect(
                    310,
                    160,
                    760));
            drawCard(
                PageCardRect(
                    490,
                    88,
                    760));
            drawCard(
                PageCardRect(
                    598,
                    88,
                    760));
        } else if (
            page_ == Page::About) {
            drawCard(
                PageCardRect(
                    314,
                    238,
                    680));
            drawCard(
                PageCardRect(
                    608,
                    80,
                    680));
        }

        EndPaint(
            hwnd_,
            &paint);

        return 0;
    }

    case WM_ERASEBKGND:
        return 1;

    case WM_CTLCOLORLISTBOX: {
        HDC dc =
            reinterpret_cast<HDC>(
                wParam);
        SetTextColor(
            dc,
            kText);
        SetBkColor(
            dc,
            kCardBackground);
        return reinterpret_cast<LRESULT>(
            cardBrush_);
    }

    case WM_CTLCOLORSTATIC: {
        HDC dc =
            reinterpret_cast<HDC>(
                wParam);

        HWND control =
            reinterpret_cast<HWND>(
                lParam);

        SetBkMode(
            dc,
            TRANSPARENT);

        const bool muted =
            control == brandSubtitle_ ||
            control == pageDescription_ ||
            control ==
                popupMonitorDescription_ ||
            control ==
                launcherPlacementDescription_ ||
            control ==
                settingsPlacementDescription_ ||
            control == generalNote_ ||
            control ==
                hotkeyEditorDescription_ ||
            control == hotkeyScope_ ||
            control == hotkeyPageStatus_ ||
            control == hotkeyPageNote_ ||
            control ==
                diagnosticsMemoryStatus_ ||
            control ==
                diagnosticsSearchStatus_ ||
            control == actionsWindowsStatus_ ||
            control ==
                actionsClipboardStatus_ ||
            control == actionsWebStatus_ ||
            control == actionsNote_ ||
            control == providerStatus_ ||
            control == providerNote_ ||
            control == appearanceNote_ ||
            control == dataStatus_ ||
            control == dataPath_ ||
            control == aboutVersion_ ||
            control == aboutDescription_ ||
            control == updateStatus_;

        SetTextColor(
            dc,
            muted
                ? kMuted
                : kText);

        return reinterpret_cast<
            LRESULT>(
                GetStockObject(
                    HOLLOW_BRUSH));
    }

    case WM_SIZE:
        Layout();
        InvalidateRect(
            hwnd_,
            nullptr,
            TRUE);
        return 0;

    case WM_EXITSIZEMOVE: {
        if (!IsIconic(hwnd_) &&
            !IsZoomed(hwnd_)) {
            RECT moved{};
            if (GetWindowRect(
                    hwnd_,
                    &moved)) {
                app_.RememberSettingsPosition(
                    moved.left,
                    moved.top);
            }
        }
        return 0;
    }

    case WM_DPICHANGED: {
        dpi_ = HIWORD(wParam);
        generalScrollOffset_ = 0;

        const auto* suggested =
            reinterpret_cast<RECT*>(
                lParam);

        RECT target =
            *suggested;

        HMONITOR monitor =
            MonitorFromRect(
                suggested,
                MONITOR_DEFAULTTONEAREST);

        MONITORINFO monitorInfo{
            sizeof(monitorInfo)};

        if (GetMonitorInfoW(
                monitor,
                &monitorInfo)) {

            const auto clamped =
                settings_layout::
                    ClampRectToWorkArea(
                        {
                            static_cast<int>(
                                suggested->left),
                            static_cast<int>(
                                suggested->top),
                            static_cast<int>(
                                suggested->right),
                            static_cast<int>(
                                suggested->bottom),
                        },
                        {
                            static_cast<int>(
                                monitorInfo.rcWork.left),
                            static_cast<int>(
                                monitorInfo.rcWork.top),
                            static_cast<int>(
                                monitorInfo.rcWork.right),
                            static_cast<int>(
                                monitorInfo.rcWork.bottom),
                        });

            target = {
                clamped.left,
                clamped.top,
                clamped.right,
                clamped.bottom,
            };
        }

        SetWindowPos(
            hwnd_,
            nullptr,
            target.left,
            target.top,
            target.right -
                target.left,
            target.bottom -
                target.top,
            SWP_NOZORDER |
                SWP_NOACTIVATE);

        ApplyFonts();
        Layout();

        RedrawWindow(
            hwnd_,
            nullptr,
            nullptr,
            RDW_INVALIDATE |
                RDW_ERASE |
                RDW_ALLCHILDREN |
                RDW_UPDATENOW);

        return 0;
    }

    case WM_GETMINMAXINFO: {
        auto* info =
            reinterpret_cast<MINMAXINFO*>(
                lParam);

        int minimumWidth =
            Scale(960);

        int minimumHeight =
            Scale(680);

        const HMONITOR monitor =
            MonitorFromWindow(
                hwnd_,
                MONITOR_DEFAULTTONEAREST);

        MONITORINFO monitorInfo{
            sizeof(monitorInfo)};

        if (GetMonitorInfoW(
                monitor,
                &monitorInfo)) {

            const int workWidth =
                static_cast<int>(
                    monitorInfo.rcWork.right -
                    monitorInfo.rcWork.left);

            const int workHeight =
                static_cast<int>(
                    monitorInfo.rcWork.bottom -
                    monitorInfo.rcWork.top);

            minimumWidth =
                std::min(
                    minimumWidth,
                    workWidth);

            minimumHeight =
                std::min(
                    minimumHeight,
                    workHeight);
        }

        info->ptMinTrackSize.x =
            minimumWidth;

        info->ptMinTrackSize.y =
            minimumHeight;

        return 0;
    }

    case WM_CLOSE:
        KillTimer(
            hwnd_,
            kProviderStatusTimerId);

        ShowWindow(
            hwnd_,
            SW_HIDE);

        return 0;

    case WM_DESTROY:
        hwnd_ = nullptr;
        return 0;

    default:
        break;
    }

    return DefWindowProcW(
        hwnd_,
        message,
        wParam,
        lParam);
}

} // namespace altrun
