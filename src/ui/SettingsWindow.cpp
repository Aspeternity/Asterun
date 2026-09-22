#include "SettingsWindow.hpp"

#include "UiTheme.hpp"
#include "UiTypography.hpp"

#include "../app/App.hpp"
#include "../core/HotkeyRegistry.hpp"
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
#include <optional>
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
        WS_CAPTION |
            WS_SYSMENU |
            WS_MINIMIZEBOX |
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

    // Size from the desired client viewport rather than treating an
    // outer-window size as if it were client geometry.
    RECT desiredWindow{
        0,
        0,
        Scale(
            ui::kSettingsClientWidthLogical),
        Scale(
            ui::kSettingsClientHeightLogical),
    };

    const DWORD windowStyle =
        static_cast<DWORD>(
            GetWindowLongPtrW(
                hwnd_,
                GWL_STYLE));
    const DWORD windowExStyle =
        static_cast<DWORD>(
            GetWindowLongPtrW(
                hwnd_,
                GWL_EXSTYLE));

    AdjustWindowRectExForDpi(
        &desiredWindow,
        windowStyle,
        FALSE,
        windowExStyle,
        dpi_);

    SetWindowPos(
        hwnd_,
        nullptr,
        0,
        0,
        desiredWindow.right -
            desiredWindow.left,
        desiredWindow.bottom -
            desiredWindow.top,
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
        CreateStatic(
            L"ALTRun",
            SS_CENTER | SS_NOPREFIX);
    brandSubtitle_ =
        CreateStatic(
            L"Next",
            SS_CENTER | SS_NOPREFIX);

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
    hotkeyGlobalTitle_ =
        CreateStatic(L"");
    hotkeyLauncherTitle_ =
        CreateStatic(L"");

    hotkeyRows_.clear();
    hotkeyRows_.reserve(
        HotkeyActionRegistry().size());

    std::size_t index = 0;

    for (const auto& action :
         HotkeyActionRegistry()) {
        HotkeyRowControls row;
        row.actionId = action.id;

        row.title =
            CreateStatic(
                L"",
                SS_LEFT |
                    SS_NOPREFIX);

        row.capture =
            CreateButton(
                L"",
                kIdHotkeyCaptureBase +
                    static_cast<UINT>(
                        index));

        if (!action.required) {
            row.enabled =
                CreateButton(
                    L"",
                    kIdHotkeyEnabledBase +
                        static_cast<UINT>(
                            index),
                    BS_OWNERDRAW);
        }

        row.reset =
            CreateButton(
                L"",
                kIdHotkeyResetBase +
                    static_cast<UINT>(
                        index));

        row.status =
            CreateStatic(
                L"",
                SS_LEFT |
                    SS_NOPREFIX);

        hotkeyRows_.push_back(
            std::move(row));

        ++index;
    }

    hotkeyResetAll_ =
        CreateButton(
            L"",
            kIdHotkeyResetAll);

    hotkeyControls_ = {
        hotkeyGlobalTitle_,
        hotkeyLauncherTitle_,
        hotkeyResetAll_,
    };

    for (const auto& row :
         hotkeyRows_) {
        hotkeyControls_.push_back(
            row.title);
        hotkeyControls_.push_back(
            row.capture);
        if (row.enabled) {
            hotkeyControls_.push_back(
                row.enabled);
        }
        hotkeyControls_.push_back(
            row.reset);
        hotkeyControls_.push_back(
            row.status);
    }
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
        CreateStatic(
            L"",
            SS_LEFT |
                SS_CENTERIMAGE |
                SS_NOPREFIX);
    aboutDescription_ =
        CreateStatic(
            L"",
            SS_LEFT | SS_NOPREFIX);

    updateSectionTitle_ =
        CreateStatic(L"");

    updateAutoCheck_ =
        CreateCheckboxRow(
            L"",
            kIdUpdateAutoCheck);
    updatePrerelease_ =
        CreateCheckboxRow(
            L"",
            kIdUpdatePrerelease);

    updateStatus_ =
        CreateStatic(
            L"",
            SS_OWNERDRAW |
                SS_NOPREFIX);

    updateAction_ =
        CreateButton(
            L"",
            kIdUpdateAction);

    openGitHub_ =
        CreateButton(
            L"",
            kIdOpenGitHub);

    aboutControls_ = {
        aboutName_,
        aboutVersion_,
        aboutDescription_,
        updateSectionTitle_,
        updateAutoCheck_,
        updatePrerelease_,
        updateStatus_,
        updateAction_,
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
        navAbout_,
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
        hotkeyResetAll_,
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
        dataExport_,
        dataClearUsage_,
        dataRebuildIndex_,
        dataResetSettings_,
        dataStatus_,
        aboutVersion_,
        aboutDescription_,
        updateAutoCheck_,
        updatePrerelease_,
        updateStatus_,
        updateAction_,
        openGitHub_,
    };

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

    for (const auto& row :
         hotkeyRows_) {
        for (HWND control :
             std::array<HWND, 4>{
                 row.capture,
                 row.enabled,
                 row.reset,
                 row.status}) {
            if (control) {
                SendMessageW(
                    control,
                    WM_SETFONT,
                    reinterpret_cast<WPARAM>(
                        normalFont_),
                    TRUE);
            }
        }

        if (row.title) {
            SendMessageW(
                row.title,
                WM_SETFONT,
                reinterpret_cast<WPARAM>(
                    normalFont_),
                TRUE);
        }
    }

    for (HWND control :
         std::array<HWND, 13>{
             generalBehaviorTitle_,
             searchBehaviorTitle_,
             placementSectionTitle_,
             hotkeyGlobalTitle_,
             hotkeyLauncherTitle_,
             providerSectionTitle_,
             providerFilesTitle_,
             appearanceLauncherTitle_,
             appearanceAppTitle_,
             dataPathLabel_,
             dataTransferLabel_,
             dataMaintenanceLabel_,
             updateSectionTitle_}) {
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

    if (brandSubtitle_) {
        SendMessageW(
            brandSubtitle_,
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
        L"ALTRun");
    SetWindowTextW(
        brandSubtitle_,
        L"Next");
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
        L"");

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
        L"");

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
        L"");

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
        L"");
    SetWindowTextW(
        hotkeyGlobalTitle_,
        T(L"全局快捷键",
          L"Global hotkeys"));
    SetWindowTextW(
        hotkeyLauncherTitle_,
        T(L"启动器内快捷键",
          L"Launcher hotkeys"));
    SetWindowTextW(
        hotkeyResetAll_,
        T(L"恢复全部默认快捷键",
          L"Reset all hotkeys"));
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
        L"");
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
        L"");
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
        T(L"导入快捷项…",
          L"Import shortcuts…"));
    SetWindowTextW(
        dataExport_,
        T(L"导出快捷项…",
          L"Export shortcuts…"));

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
        T(L"Windows 原生、轻量、高响应的键盘启动器",
          L"Native, lightweight, responsive keyboard launcher for Windows"));

    SetWindowTextW(
        updateSectionTitle_,
        T(L"更新",
          L"Updates"));

    SetWindowTextW(
        updateAutoCheck_,
        T(L"自动检查更新",
          L"Automatically check for updates"));
    SetWindowTextW(
        updatePrerelease_,
        T(L"接收预发布版本更新",
          L"Get prerelease updates"));
    SetWindowTextW(
        updateAction_,
        T(L"检查更新",
          L"Check for updates"));
    SetWindowTextW(
        openGitHub_,
        L"GitHub ↗");

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
        updateAutoCheck_,
        BM_SETCHECK,
        settings.autoCheckUpdates
            ? BST_CHECKED
            : BST_UNCHECKED,
        0);

    SendMessageW(
        updatePrerelease_,
        BM_SETCHECK,
        settings.updateChannel ==
                UpdateChannel::Development
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
    RefreshUpdateStatus();

    for (HWND control :
         std::array<HWND, 18>{
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
             updateAutoCheck_,
             updatePrerelease_}) {
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

SettingsWindow::HotkeyRowControls*
SettingsWindow::FindHotkeyRow(
    std::string_view actionId) {
    const auto it =
        std::find_if(
            hotkeyRows_.begin(),
            hotkeyRows_.end(),
            [&](const HotkeyRowControls& row) {
                return row.actionId ==
                    actionId;
            });

    return it == hotkeyRows_.end()
        ? nullptr
        : &*it;
}

const SettingsWindow::HotkeyRowControls*
SettingsWindow::FindHotkeyRow(
    std::string_view actionId) const {
    const auto it =
        std::find_if(
            hotkeyRows_.begin(),
            hotkeyRows_.end(),
            [&](const HotkeyRowControls& row) {
                return row.actionId ==
                    actionId;
            });

    return it == hotkeyRows_.end()
        ? nullptr
        : &*it;
}

SettingsWindow::HotkeyRowControls*
SettingsWindow::HotkeyRowFromControlId(
    UINT id,
    UINT baseId) {
    if (id < baseId) {
        return nullptr;
    }

    const std::size_t index =
        static_cast<std::size_t>(
            id - baseId);

    if (index >= hotkeyRows_.size()) {
        return nullptr;
    }

    return &hotkeyRows_[index];
}

void SettingsWindow::SetHotkeyRowStatus(
    std::string_view actionId,
    std::wstring_view status) {
    auto* row =
        FindHotkeyRow(actionId);

    if (!row || !row->status) {
        return;
    }

    const bool atomicUpdate =
        hwnd_ &&
        page_ == Page::Hotkeys;

    if (atomicUpdate) {
        SendMessageW(
            hwnd_,
            WM_SETREDRAW,
            FALSE,
            0);
    }

    SetWindowTextW(
        row->status,
        std::wstring(status).c_str());

    row->statusVisible =
        !status.empty();

    ShowWindow(
        row->status,
        row->statusVisible
            ? SW_SHOW
            : SW_HIDE);

    if (!atomicUpdate) {
        return;
    }

    Layout();

    SendMessageW(
        hwnd_,
        WM_SETREDRAW,
        TRUE,
        0);

    RedrawWindow(
        hwnd_,
        nullptr,
        nullptr,
        RDW_INVALIDATE |
            RDW_ERASE |
            RDW_ALLCHILDREN |
            RDW_FRAME |
            RDW_UPDATENOW);
}


bool SettingsWindow::
HotkeyRowHasAuxiliaryContent(
    const HotkeyRowControls& row) const {

    return
        row.statusVisible;
}

int SettingsWindow::HotkeyAuxiliaryHeight(
    const HotkeyRowControls& row) const {

    if (!HotkeyRowHasAuxiliaryContent(
            row)) {
        return 0;
    }

    wchar_t buffer[512]{};
    GetWindowTextW(
        row.status,
        buffer,
        static_cast<int>(
            std::size(buffer)));

    int textHeight =
        Scale(20);

    if (HDC dc = GetDC(hwnd_)) {
        HGDIOBJ oldFont =
            SelectObject(
                dc,
                normalFont_);

        RECT measured{
            0,
            0,
            Scale(224),
            0,
        };

        DrawTextW(
            dc,
            buffer,
            -1,
            &measured,
            DT_LEFT |
                DT_WORDBREAK |
                DT_CALCRECT |
                DT_NOPREFIX);

        textHeight =
            std::max(
                textHeight,
                static_cast<int>(
                    measured.bottom -
                    measured.top));

        SelectObject(
            dc,
            oldFont);
        ReleaseDC(
            hwnd_,
            dc);
    }

    return std::min(
        Scale(72),
        std::max(
            Scale(30),
            textHeight +
                Scale(10)));
}

int SettingsWindow::HotkeyRowHeight(
    const HotkeyRowControls& row) const {

    return
        Scale(54) +
        HotkeyAuxiliaryHeight(row);
}

int SettingsWindow::HotkeyGroupHeight(
    bool global) const {

    int height = 0;

    for (const auto& row :
         hotkeyRows_) {
        const auto* action =
            FindHotkeyAction(
                row.actionId);

        if (!action) {
            continue;
        }

        const bool rowGlobal =
            action->scope ==
                HotkeyScope::Global;

        if (rowGlobal == global) {
            height +=
                HotkeyRowHeight(row);
        }
    }

    return height;
}

void SettingsWindow::RefreshHotkeyPage(
    bool relayout) {
    if (hotkeyRows_.empty()) {
        return;
    }

    const bool atomicUpdate =
        relayout &&
        hwnd_ &&
        page_ == Page::Hotkeys;

    if (atomicUpdate) {
        SendMessageW(
            hwnd_,
            WM_SETREDRAW,
            FALSE,
            0);
    }

    const bool oldSyncing =
        syncing_;
    syncing_ = true;

    const auto bindings =
        app_.SettingsData()
            .hotkeyBindings;

    for (auto& row :
         hotkeyRows_) {
        const auto* action =
            FindHotkeyAction(
                row.actionId);

        if (!action) {
            continue;
        }

        const auto binding =
            EffectiveHotkeyBinding(
                bindings,
                row.actionId);

        SetWindowTextW(
            row.title,
            HotkeyActionLabel(
                row.actionId).c_str());

        const bool capturing =
            capturingHotkeyActionId_ ==
                row.actionId;

        SetWindowTextW(
            row.capture,
            capturing
                ? T(L"请按新的快捷键…",
                    L"Press a new shortcut…")
                : FormatHotkeyBinding(
                      row.actionId).c_str());

        if (row.enabled) {
            InvalidateRect(
                row.enabled,
                nullptr,
                TRUE);
        }

        auto current =
            binding;
        auto defaults =
            action->defaultBinding;

        CanonicalizeHotkeyBinding(
            current);
        CanonicalizeHotkeyBinding(
            defaults);

        const bool modified =
            current.enabled !=
                defaults.enabled ||
            current.key !=
                defaults.key ||
            current.modifiers !=
                defaults.modifiers;

        SetWindowTextW(
            row.reset,
            T(L"恢复默认",
              L"Reset"));

        std::wstring status;

        if (capturing) {
            status =
                T(L"按下新组合键，Esc 取消",
                  L"Press a new shortcut; Esc cancels");
        } else if (
            binding.enabled &&
            action->scope ==
                HotkeyScope::Global &&
            !app_.IsHotkeyActionRegistered(
                row.actionId)) {
            status =
                T(L"Windows 注册失败，旧绑定仍有效。错误码：",
                  L"Windows registration failed; the previous binding remains active. Error: ");
            status +=
                std::to_wstring(
                    app_.HotkeyActionLastError(
                        row.actionId));
        }

        const bool showStatus =
            page_ == Page::Hotkeys &&
            !status.empty();

        // Per-item Reset is an inline main-row action. Only status/capture
        // copy expands the row below the shortcut control.
        const bool showReset =
            page_ == Page::Hotkeys &&
            modified;

        SetWindowTextW(
            row.status,
            status.c_str());

        row.statusVisible =
            showStatus;
        row.resetVisible =
            showReset;

        ShowWindow(
            row.status,
            row.statusVisible
                ? SW_SHOW
                : SW_HIDE);

        ShowWindow(
            row.reset,
            row.resetVisible
                ? SW_SHOW
                : SW_HIDE);
    }

    syncing_ = oldSyncing;

    if (!atomicUpdate) {
        return;
    }

    Layout();

    SendMessageW(
        hwnd_,
        WM_SETREDRAW,
        TRUE,
        0);

    RedrawWindow(
        hwnd_,
        nullptr,
        nullptr,
        RDW_INVALIDATE |
            RDW_ERASE |
            RDW_ALLCHILDREN |
            RDW_FRAME |
            RDW_UPDATENOW);
}

void SettingsWindow::BeginHotkeyCapture(
    std::string_view actionId) {
    if (!FindHotkeyAction(actionId)) {
        return;
    }

    if (capturingHotkeyActionId_ ==
        actionId) {
        CancelHotkeyCapture();
        return;
    }

    capturingHotkeyActionId_ =
        std::string(actionId);

    RefreshHotkeyPage();
    SetFocus(hwnd_);
}

void SettingsWindow::CancelHotkeyCapture(
    bool refresh) {
    if (capturingHotkeyActionId_
            .empty()) {
        return;
    }

    capturingHotkeyActionId_
        .clear();

    if (refresh) {
        RefreshHotkeyPage();
    }
}

void SettingsWindow::ApplyCapturedHotkey(
    UINT virtualKey) {
    if (capturingHotkeyActionId_
            .empty()) {
        return;
    }

    const std::string actionId =
        capturingHotkeyActionId_;

    if (virtualKey == VK_ESCAPE) {
        CancelHotkeyCapture();
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
        SetHotkeyRowStatus(
            actionId,
            T(L"这个按键目前不受支持，请换一个组合键。",
              L"This key is not currently supported; choose another combination."));
        return;
    }

    HotkeyBinding candidate =
        EffectiveHotkeyBinding(
            app_.SettingsData()
                .hotkeyBindings,
            actionId);

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
            actionId,
            candidate)) {
        SetHotkeyRowStatus(
            actionId,
            T(L"该组合键无效或会抢占搜索输入；无修饰键时请使用 F1–F24 或 Pause。",
              L"This binding is invalid or would steal query input; use F1-F24 or Pause when no modifier is present."));
        return;
    }

    if (const auto conflict =
            FindHotkeyConflict(
                app_.SettingsData()
                    .hotkeyBindings,
                actionId,
                candidate)) {
        std::wstring message =
            T(L"与“", L"Conflicts with “");
        message +=
            HotkeyActionLabel(
                *conflict);
        message +=
            T(L"”冲突，请换一个组合键。",
              L"”; choose another binding.");

        SetHotkeyRowStatus(
            actionId,
            message);
        return;
    }

    if (!app_.SetHotkeyBinding(
            actionId,
            candidate)) {
        SetHotkeyRowStatus(
            actionId,
            T(L"无法应用；全局快捷键可能已被其他程序占用，旧绑定保持不变。",
              L"Could not apply; a global shortcut may already be owned by another app. The previous binding remains active."));
        return;
    }

    CancelHotkeyCapture();
}

void SettingsWindow::ToggleHotkeyActionEnabled(
    std::string_view actionId) {
    if (syncing_) {
        return;
    }

    const auto* action =
        FindHotkeyAction(actionId);

    if (!action ||
        action->required) {
        return;
    }

    auto binding =
        EffectiveHotkeyBinding(
            app_.SettingsData()
                .hotkeyBindings,
            actionId);

    binding.enabled =
        !binding.enabled;

    if (binding.enabled) {
        if (const auto conflict =
                FindHotkeyConflict(
                    app_.SettingsData()
                        .hotkeyBindings,
                    actionId,
                    binding)) {
            std::wstring message =
                T(L"无法启用：与“",
                  L"Cannot enable: conflicts with “");
            message +=
                HotkeyActionLabel(
                    *conflict);
            message += L"”.";

            SetHotkeyRowStatus(
                actionId,
                message);
            return;
        }
    }

    if (!app_.SetHotkeyBinding(
            std::string(actionId),
            binding)) {
        SetHotkeyRowStatus(
            actionId,
            T(L"无法更新此快捷键状态。",
              L"Could not update this hotkey state."));
        return;
    }

    RefreshHotkeyPage();
}

void SettingsWindow::ResetHotkeyAction(
    std::string_view actionId) {
    const auto* action =
        FindHotkeyAction(actionId);

    if (!action) {
        return;
    }

    if (const auto conflict =
            FindHotkeyConflict(
                app_.SettingsData()
                    .hotkeyBindings,
                actionId,
                action->defaultBinding)) {
        std::wstring message =
            T(L"默认组合键当前与“",
              L"The default binding currently conflicts with “");
        message +=
            HotkeyActionLabel(
                *conflict);
        message += L"”.";

        SetHotkeyRowStatus(
            actionId,
            message);
        return;
    }

    if (!app_.SetHotkeyBinding(
            std::string(actionId),
            action->defaultBinding)) {
        SetHotkeyRowStatus(
            actionId,
            T(L"恢复失败；默认全局快捷键可能已被其他程序占用。",
              L"Reset failed; another app may own the default global shortcut."));
        return;
    }

    if (capturingHotkeyActionId_ ==
        actionId) {
        CancelHotkeyCapture(false);
    }

    RefreshHotkeyPage();
}

void SettingsWindow::ResetAllHotkeys() {
    const int answer =
        MessageBoxW(
            hwnd_,
            T(L"恢复全部默认快捷键？\n\n主热键将恢复为 Alt + Space，辅助热键关闭，启动器内动作恢复默认组合。",
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

    CancelHotkeyCapture(false);
    RefreshHotkeyPage();
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
            T(L"○ Everything 已禁用",
              L"○ Everything is disabled");
    } else if (
        ipc.availability ==
        EverythingAvailability::
            Available) {

        text =
            T(L"● Everything 正在运行",
              L"● Everything is running");

        if (bootstrap.source ==
                win::EverythingBootstrapSource::
                    Managed ||
            bootstrap.source ==
                win::EverythingBootstrapSource::
                    Downloaded ||
            bootstrap.downloaded) {
            text +=
                T(L" · ALTRun Next 托管",
                  L" · Managed by ALTRun Next");
        } else {
            text +=
                T(L" · 外部安装",
                  L" · External installation");
        }
    } else if (bootstrap.running) {
        text =
            T(L"◌ 正在准备 Everything",
              L"◌ Preparing Everything");

        text += L" · ";

        switch (bootstrap.stage) {
        case win::EverythingBootstrapStage::Discovering:
            text += T(L"检测本机版本", L"Detecting local copies");
            break;
        case win::EverythingBootstrapStage::StartingExisting:
            text += T(L"启动已有版本", L"Starting existing copy");
            break;
        case win::EverythingBootstrapStage::DownloadingManifest:
            text += T(L"获取校验清单", L"Fetching checksums");
            break;
        case win::EverythingBootstrapStage::DownloadingPackage:
            text += T(L"下载便携版", L"Downloading portable build");
            if (bootstrap.downloadedBytes > 0) {
                text += L" ";
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
            text += T(L"校验 SHA-256", L"Verifying SHA-256");
            break;
        case win::EverythingBootstrapStage::ExtractingPackage:
            text += T(L"解压文件", L"Extracting");
            break;
        case win::EverythingBootstrapStage::InstallingService:
        case win::EverythingBootstrapStage::RepairingService:
            text += T(L"配置 Service，请确认 UAC", L"Configuring Service; confirm UAC");
            break;
        case win::EverythingBootstrapStage::WaitingForService:
            text += T(L"等待 Service", L"Waiting for Service");
            break;
        case win::EverythingBootstrapStage::StartingManaged:
        case win::EverythingBootstrapStage::WaitingForIpc:
            text += T(L"等待 IPC", L"Waiting for IPC");
            break;
        default:
            text += T(L"应用配置", L"Applying configuration");
            break;
        }
    } else {
        showGetEverything = true;
        showRecheck = true;

        if (bootstrap.failure ==
                win::EverythingBootstrapFailure::
                    ServiceRepairRequired) {
            text =
                T(L"⚠ Everything Service 需要修复",
                  L"⚠ Everything Service needs repair");
        } else if (
            bootstrap.failure ==
                win::EverythingBootstrapFailure::
                    ServiceRequired) {
            text =
                T(L"⚠ Everything Service 尚未安装",
                  L"⚠ Everything Service is not installed");
        } else if (
            bootstrap.stage ==
                win::EverythingBootstrapStage::
                    Failed) {
            text =
                T(L"⚠ Everything 自动准备失败",
                  L"⚠ Automatic Everything setup failed");

            if (bootstrap.nativeError != 0) {
                text += T(L" · 系统错误 ", L" · Native error ");
                text += std::to_wstring(
                    bootstrap.nativeError);
            }
        } else if (
            ipc.ambiguousNamedInstances) {
            text =
                T(L"⚠ 检测到多个 Everything 命名实例",
                  L"⚠ Multiple named Everything instances detected");
        } else {
            text =
                T(L"○ 未检测到可用的 Everything",
                  L"○ No usable Everything instance detected");
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

    SetWindowTextW(
        dataStatus_,
        warning.c_str());
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
        navAbout_,
        label(Page::About, L"关于", L"About").c_str());
}


void SettingsWindow::UpdatePageHeader() {
    const wchar_t* title = L"";

    switch (page_) {
    case Page::General:
        title = T(L"常规", L"General");
        break;
    case Page::Hotkeys:
        title = T(L"快捷键", L"Hotkeys");
        break;
    case Page::Appearance:
        title = T(L"外观", L"Appearance");
        break;
    case Page::Providers:
        title = T(L"搜索来源", L"Search sources");
        break;
    case Page::Data:
        title = T(L"数据", L"Data");
        break;
    case Page::About:
        title = T(L"关于", L"About");
        break;
    }

    SetWindowTextW(
        pageTitle_,
        title);

    SetWindowTextW(
        pageDescription_,
        L"");
    ShowWindow(
        pageDescription_,
        SW_HIDE);
}


void SettingsWindow::ShowPage(Page page) {
    if (page_ == Page::Hotkeys &&
        page != Page::Hotkeys) {
        CancelHotkeyCapture(false);
    }

    if (page_ == Page::Providers &&
        page != Page::Providers) {
        KillTimer(
            hwnd_,
            kProviderStatusTimerId);
    }

    if (page != page_ &&
        page == Page::General) {
        generalScrollOffset_ = 0;
    }

    SendMessageW(
        hwnd_,
        WM_SETREDRAW,
        FALSE,
        0);

    page_ = page;

    const auto setVisible =
        [](const std::vector<HWND>& controls,
           bool visible) {
            for (HWND control : controls) {
                ShowWindow(
                    control,
                    visible
                        ? SW_SHOW
                        : SW_HIDE);
            }
        };

    setVisible(
        generalControls_,
        page == Page::General);
    setVisible(
        hotkeyControls_,
        page == Page::Hotkeys);
    setVisible(
        appearanceControls_,
        page == Page::Appearance);
    setVisible(
        providerControls_,
        page == Page::Providers);
    setVisible(
        dataControls_,
        page == Page::Data);
    setVisible(
        aboutControls_,
        page == Page::About);

    for (HWND control :
         std::array<HWND, 7>{
             pageDescription_,
             popupMonitorDescription_,
             launcherPlacementDescription_,
             settingsPlacementDescription_,
             generalNote_,
             providerNote_,
             appearanceNote_}) {
        if (control) {
            ShowWindow(
                control,
                SW_HIDE);
        }
    }

    if (page == Page::Hotkeys) {
        RefreshHotkeyPage(false);
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

    SendMessageW(
        hwnd_,
        WM_SETREDRAW,
        TRUE,
        0);

    RedrawWindow(
        hwnd_,
        nullptr,
        nullptr,
        RDW_INVALIDATE |
            RDW_ERASE |
            RDW_ALLCHILDREN |
            RDW_UPDATENOW);
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


void SettingsWindow::ImportCommands() {
    std::array<wchar_t, 32768> file{};

    const wchar_t filter[] =
        L"ALTRun Next shortcuts\0*.tsv;*.txt\0"
        L"All files\0*.*\0\0";

    OPENFILENAMEW open{};
    open.lStructSize = sizeof(open);
    open.hwndOwner = hwnd_;
    open.lpstrFile = file.data();
    open.nMaxFile =
        static_cast<DWORD>(
            file.size());
    open.lpstrFilter = filter;
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
            std::filesystem::path(
                file.data()),
            &imported,
            &skipped)) {

        MessageBoxW(
            hwnd_,
            T(L"导入失败，原数据未被替换。",
              L"Import failed. Existing data was not replaced."),
            T(L"导入快捷项",
              L"Import shortcuts"),
            MB_OK |
                MB_ICONERROR);
        return;
    }

    std::wstring status =
        T(L"导入完成：新增 ",
          L"Import complete: added ");
    status +=
        std::to_wstring(imported);
    status +=
        T(L" 项，跳过 ",
          L", skipped ");
    status +=
        std::to_wstring(skipped);
    status +=
        T(L" 项。",
          L".");

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

    if (syncing_ ||
        providerCommitInProgress_) {
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

    // Keep the visual state local and immediate. The expensive provider
    // cache/lifecycle work is committed once after a short quiet period,
    // so repeated clicks collapse to the user's final intent instead of
    // queueing synchronous refreshes on the UI thread.
    pendingProviderStates_[
        providerId] =
        !ToggleChecked(id);

    InvalidateRect(
        reinterpret_cast<HWND>(
            GetDlgItem(
                hwnd_,
                static_cast<int>(id))),
        nullptr,
        TRUE);

    KillTimer(
        hwnd_,
        kProviderCommitTimerId);

    SetTimer(
        hwnd_,
        kProviderCommitTimerId,
        180,
        nullptr);
}


void SettingsWindow::CommitPendingProviderChanges() {
    KillTimer(
        hwnd_,
        kProviderCommitTimerId);

    if (pendingProviderStates_.empty() ||
        providerCommitInProgress_) {
        return;
    }

    const auto pending =
        pendingProviderStates_;

    providerCommitInProgress_ = true;

    for (HWND control :
         std::array<HWND, 5>{
             providerStartMenu_,
             providerPackaged_,
             providerAppPaths_,
             providerPath_,
             providerEverything_}) {
        EnableWindow(
            control,
            FALSE);
    }

    ProviderEnableMap ordinaryChanges;
    std::optional<bool>
        everythingChange;

    for (const auto& [providerId, enabled] :
         pending) {
        const bool everything =
            providerId ==
            providers::
                kEverythingFilesystem;

        const bool current =
            providers::IsEnabled(
                app_.SettingsData()
                    .providerEnabled,
                providerId,
                !everything);

        if (current == enabled) {
            continue;
        }

        if (everything) {
            everythingChange =
                enabled;
        } else {
            ordinaryChanges[
                providerId] =
                enabled;
        }
    }

    bool failed = false;
    bool everythingFailed = false;

    // Apply all ordinary discovery providers in one Settings save / cache
    // merge / Launcher refresh. This is the expensive work that used to run
    // once per click.
    if (!ordinaryChanges.empty() &&
        !app_.SetProviderEnabledBatch(
            ordinaryChanges)) {
        failed = true;
    }

    // Everything remains a separate lifecycle operation because it may own
    // a managed Service and require UAC. Run it after the cheap batch so a
    // permission prompt cannot delay the ordinary-provider final state.
    if (everythingChange &&
        !app_.SetProviderEnabled(
            std::string(
                providers::
                    kEverythingFilesystem),
            *everythingChange)) {
        failed = true;
        everythingFailed = true;
    }

    pendingProviderStates_.clear();
    providerCommitInProgress_ = false;

    for (HWND control :
         std::array<HWND, 5>{
             providerStartMenu_,
             providerPackaged_,
             providerAppPaths_,
             providerPath_,
             providerEverything_}) {
        EnableWindow(
            control,
            TRUE);
        InvalidateRect(
            control,
            nullptr,
            TRUE);
    }

    RefreshFromSettings();

    if (failed) {
        MessageBoxW(
            hwnd_,
            everythingFailed
                ? T(L"部分搜索来源未能应用；Everything 托管模式可能需要 Windows 管理员权限。未成功的开关已恢复实际状态。",
                    L"Some search-source changes could not be applied. Managed Everything may require Windows administrator approval. Failed switches were restored to their actual state.")
                : T(L"部分搜索来源设置无法保存，未成功的开关已恢复实际状态。",
                    L"Some search-source settings could not be saved. Failed switches were restored to their actual state."),
            L"ALTRun Next",
            MB_OK |
                MB_ICONERROR);
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
        [&](std::string_view providerId,
            bool defaultEnabled = true) {
            const auto pending =
                pendingProviderStates_.find(
                    std::string(providerId));

            if (pending !=
                pendingProviderStates_.end()) {
                return pending->second;
            }

            return providers::IsEnabled(
                settings.providerEnabled,
                providerId,
                defaultEnabled);
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
    case kIdUpdatePrerelease:
        return settings.updateChannel ==
            UpdateChannel::Development;
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
        return providerEnabled(
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
                Scale(13),
            Scale(150),
            Scale(24),
            TRUE);

        MoveWindow(
            numericQuickLaunchOrder_,
            metrics.search.right -
                Scale(118),
            orderTop +
                Scale(9),
            Scale(100),
            Scale(180),
            TRUE);

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
        const int comboWidth =
            Scale(180);
        const int comboX =
            metrics.placement.right -
            comboWidth -
            Scale(18);

        const std::array<
            std::pair<HWND, HWND>,
            3>
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
            }};

        for (std::size_t i = 0;
             i < placementRows.size();
             ++i) {
            const int top =
                metrics.placement.top +
                static_cast<int>(i) *
                    rowHeight;

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

            MoveWindow(
                placementRows[i].second,
                comboX,
                top + Scale(10),
                comboWidth,
                Scale(180),
                TRUE);
        }
    }



    if (page_ == Page::Hotkeys) {
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

        const int globalTitleTop =
            Scale(108);
        const int globalCardTop =
            Scale(140);
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

            // Reserve the switch column even for required actions so every
            // shortcut capture button shares the same left/right baseline.
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

        const int launcherCardBottom =
            launcherCardTop +
            HotkeyGroupHeight(false);

        const int resetAllWidth =
            Scale(184);

        MoveWindow(
            hotkeyResetAll_,
            cardRight -
                inner -
                resetAllWidth,
            launcherCardBottom +
                Scale(14),
            resetAllWidth,
            Scale(34),
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
            providerStatus_,
            contentLeft + Scale(18),
            filesTop + Scale(62),
            width - Scale(36),
            Scale(42),
            TRUE);

        MoveWindow(
            providerGetEverything_,
            contentLeft + Scale(18),
            filesTop + Scale(116),
            Scale(210),
            Scale(34),
            TRUE);

        MoveWindow(
            providerRecheckEverything_,
            contentLeft + Scale(240),
            filesTop + Scale(116),
            Scale(128),
            Scale(34),
            TRUE);
    }


    if (page_ == Page::Appearance) {
        const int width =
            std::min(
                contentWidth,
                Scale(560));
        const int inner =
            Scale(18);
        const int comboWidth =
            Scale(160);
        const int comboX =
            contentLeft +
            width -
            inner -
            comboWidth;

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
                comboX -
                    contentLeft -
                    inner -
                    Scale(16)),
            Scale(24),
            TRUE);
        MoveWindow(
            uiStyle_,
            comboX,
            Scale(158),
            comboWidth,
            Scale(180),
            TRUE);

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
                comboX -
                    contentLeft -
                    inner -
                    Scale(16)),
            Scale(24),
            TRUE);
        MoveWindow(
            language_,
            comboX,
            Scale(282),
            comboWidth,
            Scale(180),
            TRUE);
    }


    if (page_ == Page::Data) {
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

    if (page_ == Page::About) {
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
}

RECT SettingsWindow::ProviderCardRect() const {
    return PageCardRect(
        140,
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
    const bool focused =
        (item.itemState &
         ODS_FOCUS) != 0;

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
                : focused
                    ? kCardPressed
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
}


void SettingsWindow::DrawSwitchGlyph(
    HDC dc,
    const RECT& rect,
    bool checked,
    bool pressed) {

    const int switchWidth =
        rect.right -
        rect.left;
    const int switchHeight =
        rect.bottom -
        rect.top;

    constexpr int kSupersample = 3;
    const int margin =
        std::max(1, Scale(2));
    const int targetWidth =
        switchWidth +
        margin * 2;
    const int targetHeight =
        switchHeight +
        margin * 2;
    const int sourceWidth =
        targetWidth *
        kSupersample;
    const int sourceHeight =
        targetHeight *
        kSupersample;

    HDC switchDc =
        CreateCompatibleDC(dc);

    HBITMAP switchBitmap =
        switchDc
            ? CreateCompatibleBitmap(
                  dc,
                  sourceWidth,
                  sourceHeight)
            : nullptr;

    const COLORREF background =
        pressed
            ? kCardPressed
            : kCardBackground;

    if (switchDc &&
        switchBitmap) {
        HGDIOBJ oldBitmap =
            SelectObject(
                switchDc,
                switchBitmap);

        RECT sourceRect{
            0,
            0,
            sourceWidth,
            sourceHeight,
        };

        HBRUSH sourceBackground =
            CreateSolidBrush(
                background);
        FillRect(
            switchDc,
            &sourceRect,
            sourceBackground);
        DeleteObject(
            sourceBackground);

        const int sourceMargin =
            margin *
            kSupersample;
        const int trackWidth =
            switchWidth *
            kSupersample;
        const int trackHeight =
            switchHeight *
            kSupersample;

        HBRUSH trackBrush =
            CreateSolidBrush(
                checked
                    ? kAccent
                    : RGB(
                          210,
                          216,
                          224));

        HGDIOBJ oldBrush =
            SelectObject(
                switchDc,
                trackBrush);
        HGDIOBJ oldPen =
            SelectObject(
                switchDc,
                GetStockObject(
                    NULL_PEN));

        RoundRect(
            switchDc,
            sourceMargin,
            sourceMargin,
            sourceMargin +
                trackWidth,
            sourceMargin +
                trackHeight,
            trackHeight,
            trackHeight);

        const int knobSize =
            Scale(16) *
            kSupersample;
        const int knobInset =
            Scale(3) *
            kSupersample;
        const int knobLeft =
            checked
                ? sourceMargin +
                    trackWidth -
                    knobInset -
                    knobSize
                : sourceMargin +
                    knobInset;

        HBRUSH knobBrush =
            CreateSolidBrush(
                RGB(
                    255,
                    255,
                    255));

        SelectObject(
            switchDc,
            knobBrush);

        Ellipse(
            switchDc,
            knobLeft,
            sourceMargin +
                knobInset,
            knobLeft +
                knobSize,
            sourceMargin +
                knobInset +
                knobSize);

        SelectObject(
            switchDc,
            oldBrush);
        SelectObject(
            switchDc,
            oldPen);
        DeleteObject(
            trackBrush);
        DeleteObject(
            knobBrush);

        const int oldStretchMode =
            SetStretchBltMode(
                dc,
                HALFTONE);

        POINT oldBrushOrigin{};
        SetBrushOrgEx(
            dc,
            0,
            0,
            &oldBrushOrigin);

        StretchBlt(
            dc,
            rect.left - margin,
            rect.top - margin,
            targetWidth,
            targetHeight,
            switchDc,
            0,
            0,
            sourceWidth,
            sourceHeight,
            SRCCOPY);

        SetBrushOrgEx(
            dc,
            oldBrushOrigin.x,
            oldBrushOrigin.y,
            nullptr);
        SetStretchBltMode(
            dc,
            oldStretchMode);

        SelectObject(
            switchDc,
            oldBitmap);
    } else {
        HBRUSH trackBrush =
            CreateSolidBrush(
                checked
                    ? kAccent
                    : RGB(
                          210,
                          216,
                          224));

        HGDIOBJ oldBrush =
            SelectObject(
                dc,
                trackBrush);
        HGDIOBJ oldPen =
            SelectObject(
                dc,
                GetStockObject(
                    NULL_PEN));

        RoundRect(
            dc,
            rect.left,
            rect.top,
            rect.right,
            rect.bottom,
            switchHeight,
            switchHeight);

        const int knobSize =
            Scale(16);
        const int knobInset =
            Scale(3);
        const int knobLeft =
            checked
                ? rect.right -
                    knobInset -
                    knobSize
                : rect.left +
                    knobInset;

        HBRUSH knobBrush =
            CreateSolidBrush(
                RGB(
                    255,
                    255,
                    255));

        SelectObject(
            dc,
            knobBrush);

        Ellipse(
            dc,
            knobLeft,
            rect.top +
                knobInset,
            knobLeft +
                knobSize,
            rect.top +
                knobInset +
                knobSize);

        SelectObject(
            dc,
            oldBrush);
        SelectObject(
            dc,
            oldPen);
        DeleteObject(
            trackBrush);
        DeleteObject(
            knobBrush);
    }

    if (switchBitmap) {
        DeleteObject(
            switchBitmap);
    }
    if (switchDc) {
        DeleteDC(
            switchDc);
    }
}

void SettingsWindow::DrawHotkeyToggle(
    const DRAWITEMSTRUCT& item,
    std::size_t rowIndex) {

    if (rowIndex >=
        hotkeyRows_.size()) {
        return;
    }

    const auto& row =
        hotkeyRows_[rowIndex];

    const auto binding =
        EffectiveHotkeyBinding(
            app_.SettingsData()
                .hotkeyBindings,
            row.actionId);

    const bool pressed =
        (item.itemState &
         ODS_SELECTED) != 0;
    const bool focused =
        (item.itemState &
         ODS_FOCUS) != 0;

    const COLORREF background =
        pressed || focused
            ? kCardPressed
            : kCardBackground;

    RECT itemRect =
        item.rcItem;

    HBRUSH fill =
        CreateSolidBrush(
            background);
    FillRect(
        item.hDC,
        &itemRect,
        fill);
    DeleteObject(fill);

    const int switchWidth =
        Scale(40);
    const int switchHeight =
        Scale(22);

    RECT switchRect{
        itemRect.left +
            (itemRect.right -
             itemRect.left -
             switchWidth) / 2,
        itemRect.top +
            (itemRect.bottom -
             itemRect.top -
             switchHeight) / 2,
        0,
        0,
    };

    switchRect.right =
        switchRect.left +
        switchWidth;
    switchRect.bottom =
        switchRect.top +
        switchHeight;

    DrawSwitchGlyph(
        item.hDC,
        switchRect,
        binding.enabled,
        pressed || focused);
}

void SettingsWindow::DrawHotkeyResetLink(
    const DRAWITEMSTRUCT& item) {

    RECT rect =
        item.rcItem;

    HBRUSH background =
        CreateSolidBrush(
            kCardBackground);
    FillRect(
        item.hDC,
        &rect,
        background);
    DeleteObject(
        background);

    const bool disabled =
        (item.itemState &
         ODS_DISABLED) != 0;
    const bool pressed =
        (item.itemState &
         ODS_SELECTED) != 0;
    const bool focused =
        (item.itemState &
         ODS_FOCUS) != 0;

    const COLORREF textColor =
        disabled
            ? RGB(155, 162, 171)
            : pressed
                ? RGB(0, 99, 177)
                : kAccent;

    wchar_t buffer[64]{};
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
        rect;

    DrawTextW(
        item.hDC,
        buffer,
        -1,
        &textRect,
        DT_LEFT |
            DT_VCENTER |
            DT_SINGLELINE |
            DT_NOPREFIX);

    if (focused) {
        SIZE size{};
        if (GetTextExtentPoint32W(
                item.hDC,
                buffer,
                lstrlenW(buffer),
                &size)) {
            HPEN underline =
                CreatePen(
                    PS_SOLID,
                    1,
                    kAccent);
            HGDIOBJ oldPen =
                SelectObject(
                    item.hDC,
                    underline);

            const int y =
                rect.top +
                (rect.bottom -
                 rect.top +
                 size.cy) / 2;

            MoveToEx(
                item.hDC,
                rect.left,
                y,
                nullptr);
            LineTo(
                item.hDC,
                rect.left +
                    size.cx,
                y);

            SelectObject(
                item.hDC,
                oldPen);
            DeleteObject(
                underline);
        }
    }

    SelectObject(
        item.hDC,
        oldFont);
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
                kIdUpdateAction &&
            app_.UpdateStatus().stage ==
                win::UpdateStage::Available;
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




void SettingsWindow::DrawGitHubLink(
    const DRAWITEMSTRUCT& item) {

    RECT rect =
        item.rcItem;

    HBRUSH background =
        CreateSolidBrush(
            kWindowBackground);
    FillRect(
        item.hDC,
        &rect,
        background);
    DeleteObject(
        background);

    const bool disabled =
        (item.itemState &
         ODS_DISABLED) != 0;
    const bool pressed =
        (item.itemState &
         ODS_SELECTED) != 0;
    const bool focused =
        (item.itemState &
         ODS_FOCUS) != 0;

    const COLORREF textColor =
        disabled
            ? kMuted
            : pressed
                ? RGB(0, 99, 177)
                : kAccent;

    wchar_t buffer[64]{};
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
        rect;

    DrawTextW(
        item.hDC,
        buffer,
        -1,
        &textRect,
        DT_LEFT |
            DT_VCENTER |
            DT_SINGLELINE |
            DT_NOPREFIX);

    if (focused) {
        SIZE size{};
        if (GetTextExtentPoint32W(
                item.hDC,
                buffer,
                lstrlenW(buffer),
                &size)) {
            HPEN underline =
                CreatePen(
                    PS_SOLID,
                    1,
                    kAccent);
            HGDIOBJ oldPen =
                SelectObject(
                    item.hDC,
                    underline);

            const int y =
                rect.top +
                (rect.bottom -
                 rect.top +
                 size.cy) / 2;

            MoveToEx(
                item.hDC,
                rect.left,
                y,
                nullptr);
            LineTo(
                item.hDC,
                rect.left +
                    size.cx,
                y);

            SelectObject(
                item.hDC,
                oldPen);
            DeleteObject(
                underline);
        }
    }

    SelectObject(
        item.hDC,
        oldFont);
}


void SettingsWindow::DrawUpdateStatus(
    const DRAWITEMSTRUCT& item) {

    RECT rect =
        item.rcItem;

    HBRUSH background =
        CreateSolidBrush(
            kCardBackground);
    FillRect(
        item.hDC,
        &rect,
        background);
    DeleteObject(
        background);

    wchar_t buffer[512]{};
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
        kMuted);

    HGDIOBJ oldFont =
        SelectObject(
            item.hDC,
            normalFont_);

    RECT measured{
        0,
        0,
        rect.right -
            rect.left,
        0,
    };

    DrawTextW(
        item.hDC,
        buffer,
        -1,
        &measured,
        DT_LEFT |
            DT_WORDBREAK |
            DT_CALCRECT |
            DT_NOPREFIX);

    const int textHeight =
        measured.bottom -
        measured.top;
    const int availableHeight =
        rect.bottom -
        rect.top;
    const int top =
        rect.top +
        std::max(
            0,
            (availableHeight -
             textHeight) / 2);

    RECT textRect{
        rect.left,
        top,
        rect.right,
        rect.bottom,
    };

    DrawTextW(
        item.hDC,
        buffer,
        -1,
        &textRect,
        DT_LEFT |
            DT_WORDBREAK |
            DT_NOPREFIX);

    SelectObject(
        item.hDC,
        oldFont);
}


void SettingsWindow::DrawGeneralToggle(
    const DRAWITEMSTRUCT& item) {

    RECT rect =
        item.rcItem;

    const bool pressed =
        (item.itemState &
         ODS_SELECTED) != 0;

    const COLORREF rowColor =
        pressed
            ? kCardPressed
            : kCardBackground;

    HBRUSH rowBrush =
        CreateSolidBrush(
            rowColor);

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

    switch (id) {
    case kIdStartWithWindows:
        title =
            T(L"开机启动",
              L"Start with Windows");
        break;
    case kIdShowOnStartup:
        title =
            T(L"启动时显示启动器",
              L"Show launcher on startup");
        break;
    case kIdHideAfterLaunch:
        title =
            T(L"执行后自动隐藏",
              L"Hide after launch");
        break;
    case kIdClearQueryOnShow:
        title =
            T(L"呼出时清空搜索",
              L"Clear query on open");
        break;
    case kIdHideOnFocusLost:
        title =
            T(L"失去焦点时隐藏",
              L"Hide when focus is lost");
        break;
    case kIdShowTrayIcon:
        title =
            T(L"显示系统托盘图标",
              L"Show system tray icon");
        break;
    case kIdShowResultIcons:
        title =
            T(L"显示搜索结果图标",
              L"Show search result icons");
        break;
    case kIdPinyinSearch:
        title =
            T(L"启用拼音搜索",
              L"Enable Pinyin search");
        break;
    case kIdWildcardMatching:
        title =
            T(L"允许 * / ? 通配符",
              L"Enable * / ? wildcards");
        break;
    case kIdNumericQuickLaunch:
        title =
            T(L"数字键快速执行结果",
              L"Quick launch with number keys");
        break;
    case kIdExecuteSingleResult:
        title =
            T(L"仅剩一个结果时立即执行",
              L"Execute when one result remains");
        break;
    case kIdProviderStartMenu:
        title =
            T(L"开始菜单",
              L"Start Menu");
        break;
    case kIdProviderPackaged:
        title = L"Windows Apps";
        break;
    case kIdProviderAppPaths:
        title = L"App Paths";
        break;
    case kIdProviderPath:
        title = L"PATH";
        break;
    case kIdProviderEverything:
        title =
            T(L"Everything 文件与文件夹",
              L"Everything files & folders");
        break;
    case kIdUpdateAutoCheck:
        title =
            T(L"自动检查更新",
              L"Automatically check for updates");
        break;
    case kIdUpdatePrerelease:
        title =
            T(L"接收预发布版本更新",
              L"Get prerelease updates");
        break;
    default:
        break;
    }

    const int switchWidth =
        Scale(40);
    const int switchHeight =
        Scale(22);

    RECT switchRect{
        rect.right -
            Scale(18) -
            switchWidth,
        rect.top +
            (rect.bottom -
             rect.top -
             switchHeight) / 2,
        rect.right -
            Scale(18),
        0,
    };

    switchRect.bottom =
        switchRect.top +
        switchHeight;

    DrawSwitchGlyph(
        item.hDC,
        switchRect,
        checked,
        pressed);

    SetBkMode(
        item.hDC,
        TRANSPARENT);
    SetTextColor(
        item.hDC,
        kText);

    HGDIOBJ oldFont =
        SelectObject(
            item.hDC,
            normalFont_);

    RECT titleRect{
        rect.left +
            Scale(18),
        rect.top,
        switchRect.left -
            Scale(16),
        rect.bottom,
    };

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

    SelectObject(
        item.hDC,
        oldFont);

    const bool lastRow =
        id ==
            kIdShowResultIcons ||
        id ==
            kIdProviderPath ||
        id ==
            kIdProviderEverything;

    if (!lastRow) {
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
            rect.left +
                Scale(18),
            rect.bottom - 1,
            nullptr);
        LineTo(
            item.hDC,
            rect.right -
                Scale(18),
            rect.bottom - 1);

        SelectObject(
            item.hDC,
            oldPen);
        DeleteObject(
            separator);
    }

    if (item.itemState &
        ODS_FOCUS) {
        RECT focusBar{
            rect.left +
                Scale(5),
            rect.top +
                Scale(12),
            rect.left +
                Scale(7),
            rect.bottom -
                Scale(12),
        };

        HBRUSH focusBrush =
            CreateSolidBrush(
                kAccent);
        FillRect(
            item.hDC,
            &focusBar,
            focusBrush);
        DeleteObject(
            focusBrush);
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

void SettingsWindow::TogglePrereleaseUpdates() {
    if (syncing_) {
        return;
    }

    const auto settings =
        app_.SettingsData();

    const UpdateChannel nextChannel =
        settings.updateChannel ==
                UpdateChannel::Development
            ? UpdateChannel::Stable
            : UpdateChannel::Development;

    if (!app_.SetUpdateSettings(
            settings.autoCheckUpdates,
            nextChannel)) {
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
        !updateAction_) {
        return;
    }

    const auto status =
        app_.UpdateStatus();
    const bool settingsChanged =
        app_.UpdateSettingsChangedSinceCheck();
    const bool workerRunning =
        app_.UpdateWorkerRunning();

    std::wstring text;
    const wchar_t* actionText =
        T(L"检查更新",
          L"Check for updates");
    bool actionEnabled = false;

    switch (status.stage) {
    case win::UpdateStage::Idle:
        text =
            settingsChanged
                ? T(L"尚未按当前设置检查更新。",
                    L"Updates have not been checked with the current settings.")
                : T(L"尚未检查更新。",
                    L"Updates have not been checked yet.");
        actionEnabled = true;
        break;

    case win::UpdateStage::Checking:
        text =
            T(L"正在检查更新...",
              L"Checking for updates...");
        actionText =
            T(L"正在检查...",
              L"Checking...");
        break;

    case win::UpdateStage::UpToDate:
        text =
            T(L"已是最新版本。",
              L"You're up to date.");
        actionEnabled = true;
        break;

    case win::UpdateStage::ChannelNotNewer:
        text =
            T(L"正式版最新为 v",
              L"The latest stable release is v");
        text += std::wstring(
            status.availableVersion.begin(),
            status.availableVersion.end());
        text +=
            T(L"；当前版本较新，不会降级。",
              L"; this build is newer, so no downgrade will be offered.");
        actionEnabled = true;
        break;

    case win::UpdateStage::Available:
        text =
            T(L"发现新版本：",
              L"New version available: ");
        text += std::wstring(
            status.availableVersion.begin(),
            status.availableVersion.end());
        actionText =
            T(L"下载并安装",
              L"Download and install");
        actionEnabled = true;
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

        actionText =
            T(L"正在下载...",
              L"Downloading...");
        break;

    case win::UpdateStage::Verifying:
        text =
            T(L"正在校验更新包 SHA-256...",
              L"Verifying update package SHA-256...");
        actionText =
            T(L"正在校验...",
              L"Verifying...");
        break;

    case win::UpdateStage::Extracting:
        text =
            T(L"正在准备更新文件...",
              L"Preparing update files...");
        actionText =
            T(L"正在准备...",
              L"Preparing...");
        break;

    case win::UpdateStage::ReadyToInstall:
        text =
            T(L"更新已下载并校验，正在准备安装...",
              L"Update downloaded and verified; preparing installation...");
        actionText =
            T(L"正在安装...",
              L"Installing...");
        break;

    case win::UpdateStage::Applying:
        text =
            T(L"正在启动安全更新程序，ALTRun Next 将退出并自动重新启动。",
              L"Starting the safe updater. ALTRun Next will exit and restart automatically.");
        actionText =
            T(L"正在更新...",
              L"Updating...");
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
                T(L"缺少 Update.exe",
                  L"Update.exe is missing");
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

        actionText =
            T(L"重试",
              L"Retry");
        actionEnabled = true;
        break;
    }

    SetWindowTextW(
        updateStatus_,
        text.c_str());
    SetWindowTextW(
        updateAction_,
        actionText);

    EnableWindow(
        updateAction_,
        actionEnabled &&
                !status.running &&
                !workerRunning
            ? TRUE
            : FALSE);

    EnableWindow(
        updateAutoCheck_,
        TRUE);

    EnableWindow(
        updatePrerelease_,
        TRUE);

    InvalidateRect(
        updateAction_,
        nullptr,
        TRUE);
}

void SettingsWindow::ShowAbout() {
    if (!hwnd_) return;
    ShowPage(Page::About);
    Show();
}

void SettingsWindow::Show() {
    if (!hwnd_) return;

    CancelHotkeyCapture(false);

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

    const auto isHotkeyCaptureWindow =
        [&](HWND control) {
            if (!control) {
                return false;
            }

            const UINT id =
                static_cast<UINT>(
                    GetDlgCtrlID(
                        control));

            return
                id >= kIdHotkeyCaptureBase &&
                id <
                    kIdHotkeyCaptureBase +
                        hotkeyRows_.size();
        };

    const auto dismissComboFocus =
        [&]() {
            const HWND focused =
                GetFocus();

            if (!focused) {
                return;
            }

            for (HWND combo :
                 std::array<HWND, 6>{
                     numericQuickLaunchOrder_,
                     popupMonitor_,
                     launcherPlacement_,
                     settingsPlacement_,
                     uiStyle_,
                     language_}) {
                if (focused == combo) {
                    SetFocus(hwnd_);
                    return;
                }
            }
        };

    switch (message) {
    case WM_SETCURSOR: {
        const HWND cursorWindow =
            reinterpret_cast<HWND>(
                wParam);
        const UINT cursorId =
            cursorWindow
                ? static_cast<UINT>(
                      GetDlgCtrlID(
                          cursorWindow))
                : 0;

        if (cursorWindow ==
                openGitHub_ ||
            (cursorId >=
                 kIdHotkeyResetBase &&
             cursorId <
                 kIdHotkeyResetBase +
                     hotkeyRows_.size())) {
            SetCursor(
                LoadCursorW(
                    nullptr,
                    IDC_HAND));
            return TRUE;
        }
        break;
    }

    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_MBUTTONDOWN:
        dismissComboFocus();

        if (!capturingHotkeyActionId_
                 .empty()) {
            CancelHotkeyCapture();
        }
        break;

    case WM_PARENTNOTIFY:
        if (LOWORD(wParam) ==
                WM_LBUTTONDOWN ||
            LOWORD(wParam) ==
                WM_RBUTTONDOWN ||
            LOWORD(wParam) ==
                WM_MBUTTONDOWN) {
            // Native ComboBox controls keep their selection highlight while
            // focused. Any click elsewhere inside Settings should dismiss that
            // focus first; the clicked child can then take focus normally.
            dismissComboFocus();

            if (!capturingHotkeyActionId_
                     .empty()) {
                POINT point{};
                GetCursorPos(
                    &point);

                const HWND clicked =
                    WindowFromPoint(
                        point);

                if (LOWORD(wParam) !=
                        WM_LBUTTONDOWN ||
                    !isHotkeyCaptureWindow(
                        clicked)) {
                    CancelHotkeyCapture();
                }
            }
        }
        break;

    case WM_NCLBUTTONDOWN:
    case WM_NCRBUTTONDOWN:
    case WM_NCMBUTTONDOWN:
        if (!capturingHotkeyActionId_
                 .empty()) {
            CancelHotkeyCapture();
        }
        break;

    case WM_ACTIVATE:
        if (LOWORD(wParam) ==
                WA_INACTIVE &&
            !capturingHotkeyActionId_
                 .empty()) {
            CancelHotkeyCapture();
        }
        break;

    case WM_TIMER:
        if (wParam ==
            kProviderCommitTimerId) {
            CommitPendingProviderChanges();
            return 0;
        }

        if (wParam ==
            kProviderStatusTimerId &&
            page_ == Page::Providers) {
            RefreshProviderStatus();
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

        const auto redrawClickedToggle =
            [&]() {
                HWND control =
                    reinterpret_cast<HWND>(
                        lParam);
                if (!control) {
                    return;
                }

                InvalidateRect(
                    control,
                    nullptr,
                    TRUE);
                UpdateWindow(
                    control);
            };

        // BS_OWNERDRAW buttons report the second press of a rapid
        // double-click as BN_DOUBLECLICKED rather than BN_CLICKED.
        // Treat both notifications as one physical toggle activation so
        // every quick click flips the setting exactly once.
        const bool toggleActivated =
            notify == BN_CLICKED ||
            notify == BN_DOUBLECLICKED;

        if (id >= kIdHotkeyCaptureBase &&
            id < kIdHotkeyCaptureBase +
                hotkeyRows_.size()) {
            if (notify == BN_CLICKED) {
                if (auto* row =
                        HotkeyRowFromControlId(
                            id,
                            kIdHotkeyCaptureBase)) {
                    BeginHotkeyCapture(
                        row->actionId);
                }
            }
            return 0;
        }

        if (id >= kIdHotkeyEnabledBase &&
            id < kIdHotkeyEnabledBase +
                hotkeyRows_.size()) {
            if (toggleActivated) {
                if (auto* row =
                        HotkeyRowFromControlId(
                            id,
                            kIdHotkeyEnabledBase)) {
                    ToggleHotkeyActionEnabled(
                        row->actionId);
                    redrawClickedToggle();
                }
            }
            return 0;
        }

        if (id >= kIdHotkeyResetBase &&
            id < kIdHotkeyResetBase +
                hotkeyRows_.size()) {
            if (notify == BN_CLICKED) {
                if (auto* row =
                        HotkeyRowFromControlId(
                            id,
                            kIdHotkeyResetBase)) {
                    ResetHotkeyAction(
                        row->actionId);
                }
            }
            return 0;
        }

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
            if (toggleActivated) {
                ToggleGeneralSetting(id);
                redrawClickedToggle();
            }
            return 0;

        case kIdPinyinSearch:
        case kIdWildcardMatching:
        case kIdNumericQuickLaunch:
        case kIdExecuteSingleResult:
            if (toggleActivated) {
                ApplyClassicBehaviorControl(id);
                redrawClickedToggle();
            }
            return 0;

        case kIdNumericQuickLaunchOrder:
            if (notify == CBN_SELCHANGE) {
                ApplyClassicBehaviorControl(
                    kIdNumericQuickLaunchOrder);
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
            if (toggleActivated) {
                ToggleProviderSetting(id);
                redrawClickedToggle();
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
                ImportCommands();
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

        case kIdUpdatePrerelease:
            if (toggleActivated &&
                !syncing_) {
                TogglePrereleaseUpdates();
                redrawClickedToggle();
            }
            return 0;

        case kIdUpdateAutoCheck:
            if (toggleActivated &&
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
                redrawClickedToggle();
            }
            return 0;

        case kIdUpdateAction:
            if (notify == BN_CLICKED) {
                const auto status =
                    app_.UpdateStatus();

                if (status.stage ==
                        win::UpdateStage::Available &&
                    !status.running) {
                    app_.StartUpdateDownloadAndInstall();
                } else if (!status.running) {
                    app_.StartUpdateCheck(true);
                }

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

        if (item->CtlType ==
                ODT_BUTTON &&
            item->CtlID >=
                kIdHotkeyEnabledBase &&
            item->CtlID <
                kIdHotkeyEnabledBase +
                    hotkeyRows_.size()) {
            DrawHotkeyToggle(
                *item,
                static_cast<std::size_t>(
                    item->CtlID -
                    kIdHotkeyEnabledBase));
            return TRUE;
        }

        if (item->CtlID == kIdNavGeneral ||
            item->CtlID == kIdNavHotkeys ||
            item->CtlID == kIdNavProviders ||
            item->CtlID == kIdNavAppearance ||
            item->CtlID == kIdNavData ||
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
            item->CtlID == kIdUpdateAutoCheck ||
            item->CtlID == kIdUpdatePrerelease) {
            DrawGeneralToggle(
                *item);
            return TRUE;
        }

        if (item->CtlID >=
                kIdHotkeyResetBase &&
            item->CtlID <
                kIdHotkeyResetBase +
                    hotkeyRows_.size()) {
            DrawHotkeyResetLink(
                *item);
            return TRUE;
        }

        if (item->CtlID ==
                kIdOpenGitHub) {
            DrawGitHubLink(
                *item);
            return TRUE;
        }

        if (item->hwndItem ==
                updateStatus_) {
            DrawUpdateStatus(
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
            Scale(
                settings_layout::
                    kPageDividerTopLogical) -
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
            Scale(
                settings_layout::
                    kPageDividerTopLogical) -
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
            const int cardRight =
                contentLeft +
                std::min(
                    contentRight -
                        contentLeft,
                    Scale(560));
            const int globalTop =
                Scale(140);
            const int globalHeight =
                HotkeyGroupHeight(true);
            const int launcherTop =
                globalTop +
                globalHeight +
                Scale(54);
            const int launcherHeight =
                HotkeyGroupHeight(false);

            drawCard({
                contentLeft,
                globalTop,
                cardRight,
                globalTop +
                    globalHeight,
            });

            drawCard({
                contentLeft,
                launcherTop,
                cardRight,
                launcherTop +
                    launcherHeight,
            });

            const auto drawGroupSeparators =
                [&](bool global,
                    int groupTop) {
                    int rowTop =
                        groupTop;
                    int remaining = 0;

                    for (const auto& row :
                         hotkeyRows_) {
                        const auto* action =
                            FindHotkeyAction(
                                row.actionId);

                        if (!action) {
                            continue;
                        }

                        const bool rowGlobal =
                            action->scope ==
                                HotkeyScope::Global;

                        if (rowGlobal == global) {
                            ++remaining;
                        }
                    }

                    HPEN separator =
                        CreatePen(
                            PS_SOLID,
                            1,
                            kBorder);
                    HGDIOBJ oldPen =
                        SelectObject(
                            dc,
                            separator);

                    for (const auto& row :
                         hotkeyRows_) {
                        const auto* action =
                            FindHotkeyAction(
                                row.actionId);

                        if (!action) {
                            continue;
                        }

                        const bool rowGlobal =
                            action->scope ==
                                HotkeyScope::Global;

                        if (rowGlobal != global) {
                            continue;
                        }

                        rowTop +=
                            HotkeyRowHeight(row);
                        --remaining;

                        if (remaining > 0) {
                            MoveToEx(
                                dc,
                                contentLeft +
                                    Scale(18),
                                rowTop,
                                nullptr);
                            LineTo(
                                dc,
                                cardRight -
                                    Scale(18),
                                rowTop);
                        }
                    }

                    SelectObject(
                        dc,
                        oldPen);
                    DeleteObject(
                        separator);
                };

            drawGroupSeparators(
                true,
                globalTop);
            drawGroupSeparators(
                false,
                launcherTop);
        } else if (
            page_ == Page::Providers) {
            drawCard(
                ProviderCardRect());
            drawCard(
                PageCardRect(
                    394,
                    174,
                    720));
        } else if (
            page_ == Page::Appearance) {
            drawCard(
                PageCardRect(
                    140,
                    58,
                    560));
            drawCard(
                PageCardRect(
                    264,
                    58,
                    560));
        } else if (
            page_ == Page::Data) {
            drawCard(
                PageCardRect(
                    140,
                    60,
                    560));
            drawCard(
                PageCardRect(
                    264,
                    64,
                    560));
            drawCard(
                PageCardRect(
                    392,
                    64,
                    560));
        } else if (
            page_ == Page::About) {
            drawCard(
                PageCardRect(
                    246,
                    174,
                    560));
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

        const bool sidebarStatic =
            control == brandName_ ||
            control == brandSubtitle_;

        const bool hotkeyCardStatic =
            std::any_of(
                hotkeyRows_.begin(),
                hotkeyRows_.end(),
                [&](const HotkeyRowControls& row) {
                    return control ==
                            row.title ||
                        control ==
                            row.status;
                });

        const bool hotkeyMutedStatic =
            std::any_of(
                hotkeyRows_.begin(),
                hotkeyRows_.end(),
                [&](const HotkeyRowControls& row) {
                    return control ==
                        row.status;
                });

        const bool cardStatic =
            control ==
                numericQuickLaunchOrderLabel_ ||
            control == popupMonitorLabel_ ||
            control ==
                launcherPlacementLabel_ ||
            control ==
                settingsPlacementLabel_ ||
            hotkeyCardStatic ||
            control == providerStatus_ ||
            control == uiStyleLabel_ ||
            control == languageLabel_ ||
            control == dataPath_ ||
            control == updateStatus_;

        const COLORREF background =
            sidebarStatic
                ? kSidebarBackground
                : cardStatic
                    ? kCardBackground
                    : kWindowBackground;

        SetBkMode(
            dc,
            OPAQUE);
        SetBkColor(
            dc,
            background);

        const bool muted =
            control == pageDescription_ ||
            control ==
                popupMonitorDescription_ ||
            control ==
                launcherPlacementDescription_ ||
            control ==
                settingsPlacementDescription_ ||
            control == generalNote_ ||
            hotkeyMutedStatic ||
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
                sidebarStatic
                    ? sidebarBrush_
                    : cardStatic
                        ? cardBrush_
                        : backgroundBrush_);
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

    case WM_CLOSE:
        CancelHotkeyCapture(false);
        CommitPendingProviderChanges();

        KillTimer(
            hwnd_,
            kProviderStatusTimerId);
        KillTimer(
            hwnd_,
            kProviderCommitTimerId);

        ShowWindow(
            hwnd_,
            SW_HIDE);

        return 0;

    case WM_DESTROY:
        KillTimer(
            hwnd_,
            kProviderStatusTimerId);
        KillTimer(
            hwnd_,
            kProviderCommitTimerId);
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
