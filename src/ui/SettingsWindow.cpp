#include "SettingsWindow.hpp"

#include "../app/App.hpp"
#include "../platform/Hotkey.hpp"
#include "Version.hpp"

#include <commctrl.h>
#include <commdlg.h>
#include <shlobj.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <ctime>
#include <cwctype>
#include <filesystem>
#include <string>

namespace altrun {

namespace {

constexpr wchar_t kSettingsClass[] = L"ALTRunNext.Settings";
constexpr wchar_t kSettingsTitle[] = L"ALTRun Next Settings";

constexpr COLORREF kWindowBackground = RGB(255, 255, 255);
constexpr COLORREF kSidebarBackground = RGB(246, 247, 249);
constexpr COLORREF kCardBackground = RGB(249, 250, 252);
constexpr COLORREF kCardPressed = RGB(243, 246, 249);
constexpr COLORREF kBorder = RGB(225, 229, 235);
constexpr COLORREF kText = RGB(31, 41, 55);
constexpr COLORREF kMuted = RGB(100, 107, 116);
constexpr COLORREF kAccent = RGB(0, 120, 212);

std::wstring TrimWide(std::wstring_view value) {
    std::size_t first = 0;
    std::size_t last = value.size();

    while (first < last && std::iswspace(value[first])) ++first;
    while (last > first && std::iswspace(value[last - 1])) --last;

    return std::wstring(value.substr(first, last - first));
}

std::wstring LowerWide(std::wstring_view value) {
    std::wstring out(value);
    std::transform(
        out.begin(),
        out.end(),
        out.begin(),
        [](wchar_t c) {
            return static_cast<wchar_t>(std::towlower(c));
        });
    return out;
}

bool ContainsInsensitive(
    std::wstring_view value,
    std::wstring_view needle) {

    if (needle.empty()) return true;
    return LowerWide(value).find(LowerWide(needle)) != std::wstring::npos;
}

std::wstring FormatLocalTime(
    std::int64_t unixTime) {

    if (unixTime <= 0) {
        return L"—";
    }

    const std::time_t value =
        static_cast<std::time_t>(
            unixTime);

    std::tm local{};

    if (localtime_s(
            &local,
            &value) != 0) {
        return L"—";
    }

    wchar_t buffer[32]{};

    if (std::wcsftime(
            buffer,
            sizeof(buffer) /
                sizeof(buffer[0]),
            L"%Y-%m-%d %H:%M:%S",
            &local) == 0) {
        return L"—";
    }

    return buffer;
}

bool IsChecked(HWND control) {
    return SendMessageW(
        control,
        BM_GETCHECK,
        0,
        0) == BST_CHECKED;
}

void SetChecked(HWND control, bool checked) {
    SendMessageW(
        control,
        BM_SETCHECK,
        checked ? BST_CHECKED : BST_UNCHECKED,
        0);
}

int TypeIndex(CommandType type) {
    switch (type) {
    case CommandType::Url:
        return 1;
    case CommandType::Folder:
        return 2;
    case CommandType::CommandLine:
        return 3;
    case CommandType::Application:
    default:
        return 0;
    }
}

CommandType TypeFromIndex(int index) {
    switch (index) {
    case 1:
        return CommandType::Url;
    case 2:
        return CommandType::Folder;
    case 3:
        return CommandType::CommandLine;
    default:
        return CommandType::Application;
    }
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
    return settings_layout::
        Scale(
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
    RefreshCommands();
    ShowPage(Page::Commands);
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

HWND SettingsWindow::CreateEdit(
    UINT id,
    DWORD style) {

    HWND edit = CreateWindowExW(
        WS_EX_CLIENTEDGE,
        L"EDIT",
        L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | style,
        0, 0, 0, 0,
        hwnd_,
        reinterpret_cast<HMENU>(
            static_cast<UINT_PTR>(id)),
        instance_,
        nullptr);

    SendMessageW(
        edit,
        EM_SETMARGINS,
        EC_LEFTMARGIN | EC_RIGHTMARGIN,
        MAKELPARAM(Scale(6), Scale(6)));

    return edit;
}

std::wstring SettingsWindow::ControlText(
    HWND control) const {

    const int length =
        GetWindowTextLengthW(control);

    std::wstring value(
        static_cast<std::size_t>(length + 1),
        L'\0');

    GetWindowTextW(
        control,
        value.data(),
        length + 1);

    value.resize(static_cast<std::size_t>(length));
    return value;
}

void SettingsWindow::CreateControls() {
    navCommands_ = CreateButton(L"", kIdNavCommands);
    navGeneral_ = CreateButton(L"", kIdNavGeneral);
    navAppearance_ = CreateButton(L"", kIdNavAppearance);
    navProviders_ = CreateButton(L"", kIdNavProviders);
    navData_ = CreateButton(L"", kIdNavData);
    navAbout_ = CreateButton(L"", kIdNavAbout);

    pageTitle_ = CreateStatic(L"", SS_LEFT);
    pageDescription_ = CreateStatic(
        L"",
        SS_LEFT | SS_NOPREFIX);

    CreateCommandPage();
    CreateGeneralPage();
    CreateAppearancePage();
    CreateProviderPage();
    CreateDataPage();
    CreateAboutPage();
}

void SettingsWindow::CreateCommandPage() {
    commandSearch_ = CreateEdit(
        kIdCommandSearch,
        ES_AUTOHSCROLL);

    commandNew_ =
        CreateButton(L"", kIdCommandNew);

    commandList_ = CreateWindowExW(
        WS_EX_CLIENTEDGE,
        L"LISTBOX",
        L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP |
            WS_VSCROLL | LBS_NOTIFY |
            LBS_NOINTEGRALHEIGHT,
        0, 0, 0, 0,
        hwnd_,
        reinterpret_cast<HMENU>(
            static_cast<UINT_PTR>(kIdCommandList)),
        instance_,
        nullptr);

    commandMoveUp_ =
        CreateButton(L"", kIdCommandMoveUp);
    commandMoveDown_ =
        CreateButton(L"", kIdCommandMoveDown);

    commandEditorTitle_ = CreateStatic(L"");

    commandNameLabel_ = CreateStatic(L"");
    commandName_ = CreateEdit(kIdCommandName);

    commandKeywordLabel_ = CreateStatic(L"");
    commandKeyword_ = CreateEdit(kIdCommandKeyword);

    commandAliasesLabel_ = CreateStatic(L"");
    commandAliases_ = CreateEdit(kIdCommandAliases);

    commandTypeLabel_ = CreateStatic(L"");
    commandType_ = CreateWindowExW(
        0,
        L"COMBOBOX",
        L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP |
            CBS_DROPDOWNLIST | WS_VSCROLL,
        0, 0, 0, 0,
        hwnd_,
        reinterpret_cast<HMENU>(
            static_cast<UINT_PTR>(kIdCommandType)),
        instance_,
        nullptr);

    commandTargetLabel_ = CreateStatic(L"");
    commandTarget_ = CreateEdit(kIdCommandTarget);
    commandBrowseTarget_ =
        CreateButton(L"...", kIdCommandBrowseTarget);

    commandArgumentsLabel_ = CreateStatic(L"");
    commandArguments_ = CreateEdit(kIdCommandArguments);

    commandWorkdirLabel_ = CreateStatic(L"");
    commandWorkdir_ = CreateEdit(kIdCommandWorkdir);
    commandBrowseWorkdir_ =
        CreateButton(L"...", kIdCommandBrowseWorkdir);

    commandEnabled_ =
        CreateCheckbox(L"", kIdCommandEnabled);
    commandAdmin_ =
        CreateCheckbox(L"", kIdCommandAdmin);
    commandPinned_ =
        CreateCheckbox(L"", kIdCommandPinned);

    commandTest_ =
        CreateButton(L"", kIdCommandTest);
    commandDelete_ =
        CreateButton(L"", kIdCommandDelete);
    commandCancel_ =
        CreateButton(L"", kIdCommandCancel);
    commandSave_ =
        CreateButton(L"", kIdCommandSave);

    commandStatus_ = CreateStatic(
        L"",
        SS_LEFT | SS_NOPREFIX);

    commandControls_ = {
        commandSearch_,
        commandNew_,
        commandList_,
        commandMoveUp_,
        commandMoveDown_,
        commandEditorTitle_,
        commandNameLabel_,
        commandName_,
        commandKeywordLabel_,
        commandKeyword_,
        commandAliasesLabel_,
        commandAliases_,
        commandTypeLabel_,
        commandType_,
        commandTargetLabel_,
        commandTarget_,
        commandBrowseTarget_,
        commandArgumentsLabel_,
        commandArguments_,
        commandWorkdirLabel_,
        commandWorkdir_,
        commandBrowseWorkdir_,
        commandEnabled_,
        commandAdmin_,
        commandPinned_,
        commandTest_,
        commandDelete_,
        commandCancel_,
        commandSave_,
        commandStatus_,
    };
}

void SettingsWindow::CreateGeneralPage() {
    generalBehaviorTitle_ = CreateStatic(L"");

    startWithWindows_ =
        CreateCheckboxRow(L"", kIdStartWithWindows);
    showOnStartup_ =
        CreateCheckboxRow(L"", kIdShowOnStartup);
    hideAfterLaunch_ =
        CreateCheckboxRow(L"", kIdHideAfterLaunch);
    clearQueryOnShow_ =
        CreateCheckboxRow(L"", kIdClearQueryOnShow);
    hideOnFocusLost_ =
        CreateCheckboxRow(L"", kIdHideOnFocusLost);
    showTrayIcon_ =
        CreateCheckboxRow(L"", kIdShowTrayIcon);

    searchBehaviorTitle_ =
        CreateStatic(L"");

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
            WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                CBS_DROPDOWNLIST | WS_VSCROLL,
            0, 0, 0, 0,
            hwnd_,
            reinterpret_cast<HMENU>(
                static_cast<UINT_PTR>(
                    kIdNumericQuickLaunchOrder)),
            instance_,
            nullptr);

    hotkeySectionTitle_ = CreateStatic(L"");
    primaryHotkeyLabel_ = CreateStatic(L"");

    hotkeyCtrl_ =
        CreateCheckbox(L"Ctrl", kIdHotkeyCtrl);
    hotkeyAlt_ =
        CreateCheckbox(L"Alt", kIdHotkeyAlt);
    hotkeyShift_ =
        CreateCheckbox(L"Shift", kIdHotkeyShift);
    hotkeyWin_ =
        CreateCheckbox(L"Win", kIdHotkeyWin);

    hotkeyKey_ = CreateWindowExW(
        0,
        L"COMBOBOX",
        L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP |
            CBS_DROPDOWNLIST | WS_VSCROLL,
        0, 0, 0, 0,
        hwnd_,
        reinterpret_cast<HMENU>(
            static_cast<UINT_PTR>(kIdHotkeyKey)),
        instance_,
        nullptr);

    auxiliaryHotkeyEnabled_ =
        CreateCheckbox(
            L"",
            kIdAuxHotkeyEnabled);
    auxiliaryHotkeyCtrl_ =
        CreateCheckbox(
            L"Ctrl",
            kIdAuxHotkeyCtrl);
    auxiliaryHotkeyAlt_ =
        CreateCheckbox(
            L"Alt",
            kIdAuxHotkeyAlt);
    auxiliaryHotkeyShift_ =
        CreateCheckbox(
            L"Shift",
            kIdAuxHotkeyShift);
    auxiliaryHotkeyWin_ =
        CreateCheckbox(
            L"Win",
            kIdAuxHotkeyWin);

    auxiliaryHotkeyKey_ =
        CreateWindowExW(
            0,
            L"COMBOBOX",
            L"",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                CBS_DROPDOWNLIST | WS_VSCROLL,
            0, 0, 0, 0,
            hwnd_,
            reinterpret_cast<HMENU>(
                static_cast<UINT_PTR>(
                    kIdAuxHotkeyKey)),
            instance_,
            nullptr);

    const auto addHotkeyKey =
        [&](HWND combo,
            UINT virtualKey) {
            const std::wstring display =
                hotkey::KeyDisplayName(
                    virtualKey);

            const LRESULT index =
                SendMessageW(
                    combo,
                    CB_ADDSTRING,
                    0,
                    reinterpret_cast<LPARAM>(
                        display.c_str()));

            if (index >= 0) {
                SendMessageW(
                    combo,
                    CB_SETITEMDATA,
                    static_cast<WPARAM>(
                        index),
                    static_cast<LPARAM>(
                        virtualKey));
            }
        };

    const auto populateHotkeyCombo =
        [&](HWND combo) {
            addHotkeyKey(combo, VK_SPACE);
            addHotkeyKey(combo, VK_PAUSE);

            for (UINT key = 'A';
                 key <= 'Z';
                 ++key) {
                addHotkeyKey(combo, key);
            }

            for (UINT key = '0';
                 key <= '9';
                 ++key) {
                addHotkeyKey(combo, key);
            }

            for (UINT key = VK_F1;
                 key <= VK_F24;
                 ++key) {
                addHotkeyKey(combo, key);
            }

            for (const UINT key :
                 std::array<UINT, 12>{
                     VK_RETURN,
                     VK_TAB,
                     VK_ESCAPE,
                     VK_HOME,
                     VK_END,
                     VK_INSERT,
                     VK_DELETE,
                     VK_PRIOR,
                     VK_NEXT,
                     VK_UP,
                     VK_DOWN,
                     VK_LEFT}) {
                addHotkeyKey(combo, key);
            }

            addHotkeyKey(combo, VK_RIGHT);
        };

    populateHotkeyCombo(hotkeyKey_);
    populateHotkeyCombo(auxiliaryHotkeyKey_);

    hotkeyApply_ =
        CreateButton(L"", kIdHotkeyApply);

    hotkeyStatus_ = CreateStatic(
        L"",
        SS_LEFT | SS_NOPREFIX);

    auxiliaryHotkeyApply_ =
        CreateButton(
            L"",
            kIdAuxHotkeyApply);

    auxiliaryHotkeyStatus_ =
        CreateStatic(
            L"",
            SS_LEFT | SS_NOPREFIX);

    popupSectionTitle_ = CreateStatic(L"");
    popupMonitorLabel_ = CreateStatic(L"");
    popupMonitorDescription_ = CreateStatic(
        L"",
        SS_LEFT | SS_NOPREFIX);

    popupMonitor_ = CreateWindowExW(
        0,
        L"COMBOBOX",
        L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP |
            CBS_DROPDOWNLIST | WS_VSCROLL,
        0, 0, 0, 0,
        hwnd_,
        reinterpret_cast<HMENU>(
            static_cast<UINT_PTR>(kIdPopupMonitor)),
        instance_,
        nullptr);

    generalNote_ = CreateStatic(
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
        searchBehaviorTitle_,
        wildcardMatching_,
        numericQuickLaunch_,
        executeSingleResult_,
        numericQuickLaunchOrderLabel_,
        numericQuickLaunchOrder_,
        hotkeySectionTitle_,
        primaryHotkeyLabel_,
        hotkeyCtrl_,
        hotkeyAlt_,
        hotkeyShift_,
        hotkeyWin_,
        hotkeyKey_,
        hotkeyApply_,
        hotkeyStatus_,
        auxiliaryHotkeyEnabled_,
        auxiliaryHotkeyCtrl_,
        auxiliaryHotkeyAlt_,
        auxiliaryHotkeyShift_,
        auxiliaryHotkeyWin_,
        auxiliaryHotkeyKey_,
        auxiliaryHotkeyApply_,
        auxiliaryHotkeyStatus_,
        popupSectionTitle_,
        popupMonitorLabel_,
        popupMonitorDescription_,
        popupMonitor_,
        generalNote_,
    };
}

void SettingsWindow::CreateAppearancePage() {
    uiStyleLabel_ = CreateStatic(L"");

    uiStyle_ = CreateWindowExW(
        0,
        L"COMBOBOX",
        L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP |
            CBS_DROPDOWNLIST | WS_VSCROLL,
        0, 0, 0, 0,
        hwnd_,
        reinterpret_cast<HMENU>(
            static_cast<UINT_PTR>(kIdUiStyle)),
        instance_,
        nullptr);

    languageLabel_ = CreateStatic(L"");

    language_ = CreateWindowExW(
        0,
        L"COMBOBOX",
        L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP |
            CBS_DROPDOWNLIST | WS_VSCROLL,
        0, 0, 0, 0,
        hwnd_,
        reinterpret_cast<HMENU>(
            static_cast<UINT_PTR>(kIdLanguage)),
        instance_,
        nullptr);

    appearanceNote_ = CreateStatic(
        L"",
        SS_LEFT | SS_NOPREFIX);

    appearanceControls_ = {
        uiStyleLabel_,
        uiStyle_,
        languageLabel_,
        language_,
        appearanceNote_,
    };
}

void SettingsWindow::CreateProviderPage() {
    providerSectionTitle_ =
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

    providerStatus_ =
        CreateStatic(
            L"",
            SS_LEFT | SS_NOPREFIX);

    providerNote_ =
        CreateStatic(
            L"",
            SS_LEFT | SS_NOPREFIX);

    providerControls_ = {
        providerSectionTitle_,
        providerStartMenu_,
        providerPackaged_,
        providerAppPaths_,
        providerPath_,
        providerStatus_,
        providerNote_,
    };
}

void SettingsWindow::CreateDataPage() {
    dataOpenLabel_ = CreateStatic(L"");
    dataOpenFolder_ =
        CreateButton(L"", kIdDataOpenFolder);

    dataTransferLabel_ = CreateStatic(L"");
    dataImportTsv_ =
        CreateButton(L"", kIdDataImportTsv);
    dataImportLegacy_ =
        CreateButton(L"", kIdDataImportLegacy);
    dataExport_ =
        CreateButton(L"", kIdDataExport);

    dataMaintenanceLabel_ = CreateStatic(L"");
    dataClearUsage_ =
        CreateButton(L"", kIdDataClearUsage);
    dataRebuildIndex_ =
        CreateButton(L"", kIdDataRebuildIndex);
    dataResetSettings_ =
        CreateButton(L"", kIdDataResetSettings);

    dataStatus_ = CreateStatic(
        L"",
        SS_LEFT | SS_NOPREFIX);

    dataControls_ = {
        dataOpenLabel_,
        dataOpenFolder_,
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
    aboutName_ = CreateStatic(L"ALTRun Next");
    aboutVersion_ = CreateStatic(L"");
    aboutDescription_ = CreateStatic(
        L"",
        SS_LEFT | SS_NOPREFIX);

    dataPathLabel_ = CreateStatic(L"");
    dataPath_ = CreateStatic(
        L"",
        SS_LEFT | SS_PATHELLIPSIS | SS_NOPREFIX);

    openDataFolder_ =
        CreateButton(L"", kIdOpenDataFolder);
    openGitHub_ =
        CreateButton(L"", kIdOpenGitHub);

    aboutControls_ = {
        aboutName_,
        aboutVersion_,
        aboutDescription_,
        dataPathLabel_,
        dataPath_,
        openDataFolder_,
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

    const wchar_t* face =
        app_.SettingsData().language == Language::ZhCN
            ? L"Microsoft YaHei UI"
            : L"Segoe UI";

    normalFont_ = CreateFontW(
        -MulDiv(10, static_cast<int>(dpi_), 72),
        0, 0, 0,
        FW_NORMAL,
        FALSE, FALSE, FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE,
        face);

    sectionFont_ = CreateFontW(
        -MulDiv(11, static_cast<int>(dpi_), 72),
        0, 0, 0,
        FW_SEMIBOLD,
        FALSE, FALSE, FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE,
        face);

    titleFont_ = CreateFontW(
        -MulDiv(18, static_cast<int>(dpi_), 72),
        0, 0, 0,
        FW_SEMIBOLD,
        FALSE, FALSE, FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE,
        face);

    appNameFont_ = CreateFontW(
        -MulDiv(22, static_cast<int>(dpi_), 72),
        0, 0, 0,
        FW_SEMIBOLD,
        FALSE, FALSE, FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE,
        face);

    std::vector<HWND> normalControls{
        navCommands_,
        navGeneral_,
        navAppearance_,
        navProviders_,
        navData_,
        navAbout_,
        pageDescription_,
        commandSearch_,
        commandNew_,
        commandList_,
        commandMoveUp_,
        commandMoveDown_,
        commandNameLabel_,
        commandName_,
        commandKeywordLabel_,
        commandKeyword_,
        commandAliasesLabel_,
        commandAliases_,
        commandTypeLabel_,
        commandType_,
        commandTargetLabel_,
        commandTarget_,
        commandBrowseTarget_,
        commandArgumentsLabel_,
        commandArguments_,
        commandWorkdirLabel_,
        commandWorkdir_,
        commandBrowseWorkdir_,
        commandEnabled_,
        commandAdmin_,
        commandPinned_,
        commandTest_,
        commandDelete_,
        commandCancel_,
        commandSave_,
        commandStatus_,
        searchBehaviorTitle_,
        numericQuickLaunchOrderLabel_,
        numericQuickLaunchOrder_,
        primaryHotkeyLabel_,
        hotkeyCtrl_,
        hotkeyAlt_,
        hotkeyShift_,
        hotkeyWin_,
        hotkeyKey_,
        hotkeyApply_,
        hotkeyStatus_,
        auxiliaryHotkeyEnabled_,
        auxiliaryHotkeyCtrl_,
        auxiliaryHotkeyAlt_,
        auxiliaryHotkeyShift_,
        auxiliaryHotkeyWin_,
        auxiliaryHotkeyKey_,
        auxiliaryHotkeyApply_,
        auxiliaryHotkeyStatus_,
        popupMonitorLabel_,
        popupMonitorDescription_,
        popupMonitor_,
        generalNote_,
        uiStyleLabel_,
        uiStyle_,
        languageLabel_,
        language_,
        appearanceNote_,
        providerStartMenu_,
        providerPackaged_,
        providerAppPaths_,
        providerPath_,
        providerStatus_,
        providerNote_,
        dataOpenLabel_,
        dataOpenFolder_,
        dataTransferLabel_,
        dataImportTsv_,
        dataImportLegacy_,
        dataExport_,
        dataMaintenanceLabel_,
        dataClearUsage_,
        dataRebuildIndex_,
        dataResetSettings_,
        dataStatus_,
        aboutVersion_,
        aboutDescription_,
        dataPathLabel_,
        dataPath_,
        openDataFolder_,
        openGitHub_,
    };

    for (HWND control : normalControls) {
        if (control) {
            SendMessageW(
                control,
                WM_SETFONT,
                reinterpret_cast<WPARAM>(normalFont_),
                TRUE);
        }
    }

    for (HWND control : std::array<HWND, 9>{
             commandEditorTitle_,
             generalBehaviorTitle_,
             searchBehaviorTitle_,
             hotkeySectionTitle_,
             popupSectionTitle_,
             providerSectionTitle_,
             dataOpenLabel_,
             dataTransferLabel_,
             dataMaintenanceLabel_}) {
        if (control) {
            SendMessageW(
                control,
                WM_SETFONT,
                reinterpret_cast<WPARAM>(sectionFont_),
                TRUE);
        }
    }

    if (pageTitle_) {
        SendMessageW(
            pageTitle_,
            WM_SETFONT,
            reinterpret_cast<WPARAM>(titleFont_),
            TRUE);
    }

    if (aboutName_) {
        SendMessageW(
            aboutName_,
            WM_SETFONT,
            reinterpret_cast<WPARAM>(appNameFont_),
            TRUE);
    }

    for (HWND control : std::array<HWND, 13>{
             startWithWindows_,
             showOnStartup_,
             hideAfterLaunch_,
             clearQueryOnShow_,
             hideOnFocusLost_,
             showTrayIcon_,
             wildcardMatching_,
             numericQuickLaunch_,
             executeSingleResult_,
             providerStartMenu_,
             providerPackaged_,
             providerAppPaths_,
             providerPath_}) {
        if (control) {
            SendMessageW(
                control,
                WM_SETFONT,
                reinterpret_cast<WPARAM>(normalFont_),
                TRUE);
        }
    }
}

void SettingsWindow::ApplyLanguage() {
    if (!hwnd_) return;

    const bool oldSyncing = syncing_;
    syncing_ = true;

    SetWindowTextW(
        hwnd_,
        T(L"ALTRun Next 设置", L"ALTRun Next Settings"));

    SendMessageW(
        commandSearch_,
        EM_SETCUEBANNER,
        TRUE,
        reinterpret_cast<LPARAM>(
            T(L"搜索快捷项...", L"Search shortcuts...")));

    SetWindowTextW(
        commandNew_,
        T(L"+ 新建", L"+ New"));
    SetWindowTextW(
        commandMoveUp_,
        T(L"上移", L"Move up"));
    SetWindowTextW(
        commandMoveDown_,
        T(L"下移", L"Move down"));
    SetWindowTextW(
        commandEditorTitle_,
        T(L"快捷项详情", L"Shortcut details"));
    SetWindowTextW(
        commandNameLabel_,
        T(L"名称 *", L"Name *"));
    SetWindowTextW(
        commandKeywordLabel_,
        T(L"主快捷词 *", L"Primary keyword *"));
    SetWindowTextW(
        commandAliasesLabel_,
        T(L"别名（逗号分隔）", L"Aliases (comma separated)"));
    SetWindowTextW(
        commandTypeLabel_,
        T(L"类型", L"Type"));
    SetWindowTextW(
        commandTargetLabel_,
        T(L"目标 *", L"Target *"));
    SetWindowTextW(
        commandArgumentsLabel_,
        T(L"参数", L"Arguments"));
    SetWindowTextW(
        commandWorkdirLabel_,
        T(L"工作目录", L"Working directory"));
    SetWindowTextW(
        commandEnabled_,
        T(L"启用", L"Enabled"));
    SetWindowTextW(
        commandAdmin_,
        T(L"以管理员身份运行", L"Run as administrator"));
    SetWindowTextW(
        commandPinned_,
        T(L"置顶", L"Pinned"));
    SetWindowTextW(
        commandTest_,
        T(L"测试运行", L"Test"));
    SetWindowTextW(
        commandDelete_,
        T(L"删除", L"Delete"));
    SetWindowTextW(
        commandCancel_,
        T(L"取消更改", L"Discard changes"));
    SetWindowTextW(
        commandSave_,
        T(L"保存", L"Save"));

    const int oldType =
        std::max(
            0,
            static_cast<int>(
                SendMessageW(
                    commandType_,
                    CB_GETCURSEL,
                    0,
                    0)));

    SendMessageW(commandType_, CB_RESETCONTENT, 0, 0);
    SendMessageW(
        commandType_,
        CB_ADDSTRING,
        0,
        reinterpret_cast<LPARAM>(
            T(L"应用程序", L"Application")));
    SendMessageW(
        commandType_,
        CB_ADDSTRING,
        0,
        reinterpret_cast<LPARAM>(
            T(L"网址", L"URL")));
    SendMessageW(
        commandType_,
        CB_ADDSTRING,
        0,
        reinterpret_cast<LPARAM>(
            T(L"文件夹", L"Folder")));
    SendMessageW(
        commandType_,
        CB_ADDSTRING,
        0,
        reinterpret_cast<LPARAM>(
            T(L"命令", L"Command")));
    SendMessageW(
        commandType_,
        CB_SETCURSEL,
        oldType,
        0);

    SetWindowTextW(
        generalBehaviorTitle_,
        T(L"启动器行为", L"Launcher behavior"));
    SetWindowTextW(
        startWithWindows_,
        T(L"开机启动", L"Start with Windows"));
    SetWindowTextW(
        showOnStartup_,
        T(L"启动时显示启动器",
          L"Show launcher on startup"));
    SetWindowTextW(
        hideAfterLaunch_,
        T(L"执行后自动隐藏", L"Hide after launch"));
    SetWindowTextW(
        clearQueryOnShow_,
        T(L"呼出时清空搜索", L"Clear query on open"));
    SetWindowTextW(
        hideOnFocusLost_,
        T(L"失去焦点时隐藏", L"Hide when focus is lost"));
    SetWindowTextW(
        showTrayIcon_,
        T(L"显示系统托盘图标", L"Show system tray icon"));

    SetWindowTextW(
        searchBehaviorTitle_,
        T(L"搜索行为",
          L"Search behavior"));
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
        CB_RESETCONTENT,
        0,
        0);
    SendMessageW(
        numericQuickLaunchOrder_,
        CB_ADDSTRING,
        0,
        reinterpret_cast<LPARAM>(
            L"1–9, 0"));
    SendMessageW(
        numericQuickLaunchOrder_,
        CB_ADDSTRING,
        0,
        reinterpret_cast<LPARAM>(
            L"0–9"));

    SetWindowTextW(
        hotkeySectionTitle_,
        T(L"全局热键",
          L"Global hotkeys"));
    SetWindowTextW(
        primaryHotkeyLabel_,
        T(L"主热键",
          L"Primary"));
    SetWindowTextW(
        hotkeyApply_,
        T(L"应用",
          L"Apply"));
    SetWindowTextW(
        auxiliaryHotkeyEnabled_,
        T(L"启用辅助热键",
          L"Enable auxiliary hotkey"));
    SetWindowTextW(
        auxiliaryHotkeyApply_,
        T(L"应用",
          L"Apply"));

    SetWindowTextW(
        popupSectionTitle_,
        T(L"呼出位置", L"Launcher placement"));
    SetWindowTextW(
        popupMonitorLabel_,
        T(L"显示器", L"Monitor"));
    SetWindowTextW(
        popupMonitorDescription_,
        T(L"选择启动器每次呼出时使用哪一块屏幕。",
          L"Choose which display the launcher uses when it opens."));
    SetWindowTextW(
        generalNote_,
        T(L"主热键和已启用的辅助热键只有在 Windows 注册成功后才会保存；冲突时保留旧绑定。",
          L"Primary and enabled auxiliary hotkeys are saved only after Windows registers them successfully; conflicts keep the previous binding."));

    SendMessageW(popupMonitor_, CB_RESETCONTENT, 0, 0);
    SendMessageW(
        popupMonitor_,
        CB_ADDSTRING,
        0,
        reinterpret_cast<LPARAM>(
            T(L"当前鼠标所在显示器", L"Monitor containing the mouse")));
    SendMessageW(
        popupMonitor_,
        CB_ADDSTRING,
        0,
        reinterpret_cast<LPARAM>(
            T(L"当前活动窗口所在显示器", L"Monitor containing the active window")));
    SendMessageW(
        popupMonitor_,
        CB_ADDSTRING,
        0,
        reinterpret_cast<LPARAM>(
            T(L"主显示器", L"Primary monitor")));

    SetWindowTextW(
        uiStyleLabel_,
        T(L"启动器样式", L"Launcher style"));

    SendMessageW(uiStyle_, CB_RESETCONTENT, 0, 0);
    SendMessageW(
        uiStyle_,
        CB_ADDSTRING,
        0,
        reinterpret_cast<LPARAM>(L"Classic ALTRun"));
    SendMessageW(
        uiStyle_,
        CB_ADDSTRING,
        0,
        reinterpret_cast<LPARAM>(L"Modern Compact"));

    SetWindowTextW(
        languageLabel_,
        T(L"界面语言", L"Interface language"));

    SendMessageW(language_, CB_RESETCONTENT, 0, 0);
    SendMessageW(
        language_,
        CB_ADDSTRING,
        0,
        reinterpret_cast<LPARAM>(L"简体中文"));
    SendMessageW(
        language_,
        CB_ADDSTRING,
        0,
        reinterpret_cast<LPARAM>(L"English"));

    SetWindowTextW(
        appearanceNote_,
        T(L"外观和语言更改会立即应用，并写入 data/settings.json。",
          L"Appearance and language changes apply immediately and are saved to data/settings.json."));

    SetWindowTextW(
        providerSectionTitle_,
        T(L"Windows 应用来源", L"Windows application sources"));
    SetWindowTextW(
        providerStartMenu_,
        T(L"开始菜单", L"Start Menu"));
    SetWindowTextW(
        providerPackaged_,
        T(L"Windows Apps", L"Windows Apps"));
    SetWindowTextW(
        providerAppPaths_,
        T(L"App Paths", L"App Paths"));
    SetWindowTextW(
        providerPath_,
        T(L"PATH", L"PATH"));
    SetWindowTextW(
        providerNote_,
        T(L"ALTRun Next 会低频检测来源变化，只刷新发生变化的来源；短时间内的连续变化会自动合并。",
          L"ALTRun Next watches sources at low frequency and refreshes only changed providers. Rapid changes are debounced automatically."));

    SetWindowTextW(
        dataOpenLabel_,
        T(L"数据目录", L"Data directory"));
    SetWindowTextW(
        dataOpenFolder_,
        T(L"打开数据目录", L"Open data folder"));

    SetWindowTextW(
        dataTransferLabel_,
        T(L"导入 / 导出", L"Import / Export"));
    SetWindowTextW(
        dataImportTsv_,
        T(L"导入 ALTRun Next TSV", L"Import ALTRun Next TSV"));
    SetWindowTextW(
        dataImportLegacy_,
        T(L"导入旧版 ALTRun（Beta）", L"Import legacy ALTRun (Beta)"));
    SetWindowTextW(
        dataExport_,
        T(L"导出快捷项 TSV", L"Export shortcuts TSV"));

    SetWindowTextW(
        dataMaintenanceLabel_,
        T(L"维护", L"Maintenance"));
    SetWindowTextW(
        dataClearUsage_,
        T(L"清空使用历史", L"Clear usage history"));
    SetWindowTextW(
        dataRebuildIndex_,
        T(L"重建程序索引", L"Rebuild program index"));
    SetWindowTextW(
        dataResetSettings_,
        T(L"恢复默认设置", L"Restore default settings"));

    std::wstring versionText =
        T(L"版本 ", L"Version ");
    versionText += kVersionWide;

    SetWindowTextW(
        aboutVersion_,
        versionText.c_str());

    SetWindowTextW(
        aboutDescription_,
        T(L"轻量级、键盘优先的 Windows 快捷启动器。\nv0.4.1 正在补齐经典 ALTRun 设置与交互能力。",
          L"A lightweight, keyboard-first Windows launcher.\nv0.4.1 adds classic ALTRun settings and interaction parity."));

    SetWindowTextW(
        dataPathLabel_,
        T(L"数据目录", L"Data directory"));

    SetWindowTextW(
        dataPath_,
        app_.DataDirectory().c_str());

    SetWindowTextW(
        openDataFolder_,
        T(L"打开数据目录", L"Open data folder"));

    SetWindowTextW(
        openGitHub_,
        L"GitHub");

    ApplyFonts();
    UpdateNavLabels();
    UpdatePageHeader();
    RefreshFromSettings();
    RefreshCommandList(editingCommandId_);
    RefreshDataCompatibilityStatus();

    syncing_ = oldSyncing;

    RedrawWindow(
        hwnd_,
        nullptr,
        nullptr,
        RDW_INVALIDATE | RDW_ERASE |
            RDW_ALLCHILDREN | RDW_UPDATENOW);
}

void SettingsWindow::RefreshFromSettings() {
    if (!hwnd_) return;

    const bool oldSyncing = syncing_;
    syncing_ = true;

    const auto& settings = app_.SettingsData();

    int monitorIndex = 0;
    if (settings.popupMonitor == "active") monitorIndex = 1;
    else if (settings.popupMonitor == "primary") monitorIndex = 2;

    SendMessageW(
        popupMonitor_,
        CB_SETCURSEL,
        monitorIndex,
        0);

    SendMessageW(
        uiStyle_,
        CB_SETCURSEL,
        settings.uiStyle == UiStyle::ModernCompact ? 1 : 0,
        0);

    SendMessageW(
        language_,
        CB_SETCURSEL,
        settings.language == Language::EnUS ? 1 : 0,
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

    RefreshHotkeyControls();

    for (HWND control : std::array<HWND, 13>{
             startWithWindows_,
             showOnStartup_,
             hideAfterLaunch_,
             clearQueryOnShow_,
             hideOnFocusLost_,
             showTrayIcon_,
             wildcardMatching_,
             numericQuickLaunch_,
             executeSingleResult_,
             providerStartMenu_,
             providerPackaged_,
             providerAppPaths_,
             providerPath_}) {
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

void SettingsWindow::RefreshProviderStatus() {
    if (!providerStatus_) {
        return;
    }

    const auto statuses =
        app_.ProviderStatuses();

    std::wstring text;

    for (std::size_t i = 0;
         i < statuses.size();
         ++i) {

        const auto& status =
            statuses[i];

        std::wstring name =
            status.name;

        if (status.id ==
            providers::kStartMenu) {
            name =
                T(L"开始菜单", L"Start Menu");
        } else if (
            status.id ==
            providers::kPackaged) {
            name = L"Windows Apps";
        } else if (
            status.id ==
            providers::kAppPaths) {
            name = L"App Paths";
        } else if (
            status.id ==
            providers::kPath) {
            name = L"PATH";
        }

        text += name;
        text += L"  ·  ";

        if (!status.enabled) {
            text += T(
                L"已禁用",
                L"Disabled");
        } else if (
            status.lastAttemptUnix > 0 &&
            !status.lastAttemptSucceeded) {
            text += T(
                L"刷新失败",
                L"Refresh failed");
        } else {
            text += T(
                L"正常",
                L"Healthy");
        }

        text += L"  ·  ";
        text += T(L"缓存 ", L"Cached ");
        text += std::to_wstring(
            status.commandCount);
        text += T(L" / 搜索 ", L" / active ");
        text += std::to_wstring(
            status.activeCommandCount);

        if (status.suppressedCommandCount >
            0) {
            text += T(L" / 去重 ", L" / dedup ");
            text += std::to_wstring(
                status.suppressedCommandCount);
        }

        text += L"  ·  ";
        text += T(
            L"成功刷新 ",
            L"Last success ");
        text += FormatLocalTime(
            status.lastRefreshUnix);

        if (status.lastAttemptUnix > 0 &&
            !status.lastAttemptSucceeded &&
            !status.lastError.empty()) {

            text += L"\r\n    ↳ ";
            text += T(
                L"错误：",
                L"Error: ");
            text += status.lastError;
        }

        if (i + 1 <
            statuses.size()) {
            text += L"\r\n";
        }
    }

    SetWindowTextW(
        providerStatus_,
        text.c_str());
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

void SettingsWindow::RefreshCommands() {
    RefreshCommandList(editingCommandId_);
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

    const auto label = [&](Page page,
                           const wchar_t* zhText,
                           const wchar_t* enText) {
        std::wstring text =
            page_ == page ? L"●  " : L"   ";
        text += zh ? zhText : enText;
        return text;
    };

    SetWindowTextW(
        navCommands_,
        label(Page::Commands, L"快捷项", L"Shortcuts").c_str());
    SetWindowTextW(
        navGeneral_,
        label(Page::General, L"常规", L"General").c_str());
    SetWindowTextW(
        navAppearance_,
        label(Page::Appearance, L"外观", L"Appearance").c_str());
    SetWindowTextW(
        navProviders_,
        label(Page::Providers, L"搜索来源", L"Search sources").c_str());
    SetWindowTextW(
        navData_,
        label(Page::Data, L"数据", L"Data").c_str());
    SetWindowTextW(
        navAbout_,
        label(Page::About, L"关于", L"About").c_str());
}

void SettingsWindow::UpdatePageHeader() {
    switch (page_) {
    case Page::Commands:
        SetWindowTextW(
            pageTitle_,
            T(L"快捷项", L"Shortcuts"));
        SetWindowTextW(
            pageDescription_,
            T(L"管理用户自定义快捷项。开始菜单自动发现的程序不会写入这里。",
              L"Manage user shortcuts. Automatically discovered Start Menu apps are not stored here."));
        break;

    case Page::General:
        SetWindowTextW(
            pageTitle_,
            T(L"常规", L"General"));
        SetWindowTextW(
            pageDescription_,
            T(L"控制启动器行为、搜索方式、全局热键和呼出位置。",
              L"Control launcher behavior, search interaction, global hotkeys and placement."));
        break;

    case Page::Appearance:
        SetWindowTextW(
            pageTitle_,
            T(L"外观", L"Appearance"));
        SetWindowTextW(
            pageDescription_,
            T(L"选择启动器样式和界面语言。",
              L"Choose the launcher style and interface language."));
        break;

    case Page::Providers:
        SetWindowTextW(
            pageTitle_,
            T(L"搜索来源", L"Search sources"));
        SetWindowTextW(
            pageDescription_,
            T(L"控制哪些 Windows 应用来源参与启动器搜索。",
              L"Choose which Windows application sources participate in launcher search."));
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
            T(L"版本、项目入口和本地数据位置。",
              L"Version information, project links and local data."));
        break;
    }
}

bool SettingsWindow::ConfirmDiscardChanges() {
    if (!editorDirty_) return true;

    const int result = MessageBoxW(
        hwnd_,
        T(L"当前快捷项有尚未保存的修改。\n\n确定放弃这些修改吗？",
          L"The current shortcut has unsaved changes.\n\nDiscard them?"),
        T(L"未保存的修改", L"Unsaved changes"),
        MB_OKCANCEL | MB_ICONWARNING);

    if (result != IDOK) {
        return false;
    }

    editorDirty_ = false;
    return true;
}

void SettingsWindow::ShowPage(Page page) {
    if (page_ == Page::Commands &&
        page != Page::Commands &&
        !ConfirmDiscardChanges()) {
        return;
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

    setVisible(commandControls_, page == Page::Commands);
    setVisible(generalControls_, page == Page::General);
    setVisible(appearanceControls_, page == Page::Appearance);
    setVisible(providerControls_, page == Page::Providers);
    setVisible(dataControls_, page == Page::Data);
    setVisible(aboutControls_, page == Page::About);

    if (page == Page::Commands) {
        RefreshCommandList(editingCommandId_);
    } else if (
        page == Page::Providers) {
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

void SettingsWindow::RefreshCommandList(
    std::wstring_view preferredId) {

    if (!commandList_) return;

    const std::wstring filter =
        TrimWide(ControlText(commandSearch_));

    std::vector<const Command*> commands;
    commands.reserve(app_.UserCommands().size());

    for (const auto& command : app_.UserCommands()) {
        bool matches =
            filter.empty() ||
            ContainsInsensitive(command.title, filter) ||
            ContainsInsensitive(command.keyword, filter) ||
            ContainsInsensitive(command.target, filter);

        if (!matches) {
            for (const auto& alias : command.aliases) {
                if (ContainsInsensitive(alias, filter)) {
                    matches = true;
                    break;
                }
            }
        }

        if (matches) {
            commands.push_back(&command);
        }
    }

    std::stable_sort(
        commands.begin(),
        commands.end(),
        [](const Command* a, const Command* b) {
            if (a->sortOrder != b->sortOrder) {
                return a->sortOrder < b->sortOrder;
            }
            return a->keyword < b->keyword;
        });

    const bool oldSyncing = syncing_;
    syncing_ = true;

    SendMessageW(commandList_, LB_RESETCONTENT, 0, 0);
    filteredCommandIds_.clear();

    int preferredIndex = -1;

    for (const Command* command : commands) {
        std::wstring label =
            command->enabled ? L"✓  " : L"○  ";

        if (command->pinned) {
            label += L"★ ";
        }

        label += command->keyword;
        label += L"    ";
        label += command->title;

        const LRESULT index = SendMessageW(
            commandList_,
            LB_ADDSTRING,
            0,
            reinterpret_cast<LPARAM>(label.c_str()));

        if (index >= 0) {
            filteredCommandIds_.push_back(command->id);

            if (!preferredId.empty() &&
                command->id == preferredId) {
                preferredIndex =
                    static_cast<int>(index);
            }
        }
    }

    if (preferredIndex >= 0) {
        SendMessageW(
            commandList_,
            LB_SETCURSEL,
            preferredIndex,
            0);
    } else if (
        editingCommandId_.empty() &&
        !editingNew_ &&
        !filteredCommandIds_.empty()) {

        SendMessageW(
            commandList_,
            LB_SETCURSEL,
            0,
            0);
    } else {
        SendMessageW(
            commandList_,
            LB_SETCURSEL,
            static_cast<WPARAM>(-1),
            0);
    }

    syncing_ = oldSyncing;

    if (editingCommandId_.empty() &&
        !editingNew_ &&
        !filteredCommandIds_.empty()) {
        LoadCommandEditor(filteredCommandIds_.front());
    }

    EnableWindow(
        commandMoveUp_,
        !editingCommandId_.empty());
    EnableWindow(
        commandMoveDown_,
        !editingCommandId_.empty());
}

void SettingsWindow::LoadCommandEditor(
    std::wstring_view id) {

    const auto it = std::find_if(
        app_.UserCommands().begin(),
        app_.UserCommands().end(),
        [&](const Command& command) {
            return command.id == id;
        });

    if (it == app_.UserCommands().end()) {
        ClearCommandEditor();
        return;
    }

    const bool oldSyncing = syncing_;
    syncing_ = true;

    editingCommandId_ = it->id;
    editingNew_ = false;
    editorDirty_ = false;

    SetWindowTextW(
        commandEditorTitle_,
        T(L"快捷项详情", L"Shortcut details"));

    SetWindowTextW(commandName_, it->title.c_str());
    SetWindowTextW(commandKeyword_, it->keyword.c_str());

    std::wstring aliases;
    for (std::size_t i = 0; i < it->aliases.size(); ++i) {
        if (i > 0) aliases += L", ";
        aliases += it->aliases[i];
    }
    SetWindowTextW(commandAliases_, aliases.c_str());

    SendMessageW(
        commandType_,
        CB_SETCURSEL,
        TypeIndex(it->type),
        0);

    SetWindowTextW(commandTarget_, it->target.c_str());
    SetWindowTextW(commandArguments_, it->arguments.c_str());
    SetWindowTextW(
        commandWorkdir_,
        it->workingDirectory.c_str());

    SetChecked(commandEnabled_, it->enabled);
    SetChecked(commandAdmin_, it->runAsAdmin);
    SetChecked(commandPinned_, it->pinned);

    SetWindowTextW(commandStatus_, L"");
    SetCommandEditorEnabled(true);
    EnableWindow(commandDelete_, TRUE);

    syncing_ = oldSyncing;
}

void SettingsWindow::ClearCommandEditor() {
    const bool oldSyncing = syncing_;
    syncing_ = true;

    editingCommandId_.clear();
    editingNew_ = false;
    editorDirty_ = false;

    SetWindowTextW(
        commandEditorTitle_,
        T(L"快捷项详情", L"Shortcut details"));

    SetWindowTextW(commandName_, L"");
    SetWindowTextW(commandKeyword_, L"");
    SetWindowTextW(commandAliases_, L"");
    SendMessageW(commandType_, CB_SETCURSEL, 0, 0);
    SetWindowTextW(commandTarget_, L"");
    SetWindowTextW(commandArguments_, L"");
    SetWindowTextW(commandWorkdir_, L"");
    SetChecked(commandEnabled_, true);
    SetChecked(commandAdmin_, false);
    SetChecked(commandPinned_, false);
    SetWindowTextW(commandStatus_, L"");

    SetCommandEditorEnabled(false);

    syncing_ = oldSyncing;
}

void SettingsWindow::SetCommandEditorEnabled(
    bool enabled) {

    for (HWND control : std::array<HWND, 16>{
             commandName_,
             commandKeyword_,
             commandAliases_,
             commandType_,
             commandTarget_,
             commandBrowseTarget_,
             commandArguments_,
             commandWorkdir_,
             commandBrowseWorkdir_,
             commandEnabled_,
             commandAdmin_,
             commandPinned_,
             commandTest_,
             commandDelete_,
             commandCancel_,
             commandSave_}) {
        EnableWindow(
            control,
            enabled ? TRUE : FALSE);
    }

    EnableWindow(
        commandDelete_,
        enabled &&
            !editingNew_ &&
            !editingCommandId_.empty());
}

void SettingsWindow::BeginNewCommand() {
    if (!ConfirmDiscardChanges()) return;

    const bool oldSyncing = syncing_;
    syncing_ = true;

    editingCommandId_.clear();
    editingNew_ = true;
    editorDirty_ = false;

    SendMessageW(
        commandList_,
        LB_SETCURSEL,
        static_cast<WPARAM>(-1),
        0);

    SetWindowTextW(
        commandEditorTitle_,
        T(L"新建快捷项", L"New shortcut"));

    SetWindowTextW(commandName_, L"");
    SetWindowTextW(commandKeyword_, L"");
    SetWindowTextW(commandAliases_, L"");
    SendMessageW(commandType_, CB_SETCURSEL, 0, 0);
    SetWindowTextW(commandTarget_, L"");
    SetWindowTextW(commandArguments_, L"");
    SetWindowTextW(commandWorkdir_, L"");
    SetChecked(commandEnabled_, true);
    SetChecked(commandAdmin_, false);
    SetChecked(commandPinned_, false);
    SetWindowTextW(commandStatus_, L"");

    SetCommandEditorEnabled(true);
    EnableWindow(commandDelete_, FALSE);

    syncing_ = oldSyncing;

    SetFocus(commandName_);
}

void SettingsWindow::MarkEditorDirty() {
    if (syncing_) return;
    if (editingNew_ || !editingCommandId_.empty()) {
        editorDirty_ = true;
        SetWindowTextW(
            commandStatus_,
            T(L"有未保存的修改", L"Unsaved changes"));
    }
}

std::vector<std::wstring> SettingsWindow::ParseAliases(
    std::wstring_view text) const {

    std::vector<std::wstring> aliases;
    std::wstring current;

    auto flush = [&]() {
        const std::wstring value = TrimWide(current);
        current.clear();

        if (value.empty()) return;

        const auto duplicate = std::find_if(
            aliases.begin(),
            aliases.end(),
            [&](const std::wstring& existing) {
                return LowerWide(existing) == LowerWide(value);
            });

        if (duplicate == aliases.end()) {
            aliases.push_back(value);
        }
    };

    for (wchar_t c : text) {
        if (c == L',' ||
            c == L';' ||
            c == L'，' ||
            c == L'；') {
            flush();
        } else {
            current.push_back(c);
        }
    }

    flush();
    return aliases;
}

Command SettingsWindow::CollectCommandEditor() const {
    Command command;

    command.title =
        TrimWide(ControlText(commandName_));
    command.keyword =
        TrimWide(ControlText(commandKeyword_));
    command.aliases =
        ParseAliases(ControlText(commandAliases_));
    command.type =
        TypeFromIndex(
            static_cast<int>(
                SendMessageW(
                    commandType_,
                    CB_GETCURSEL,
                    0,
                    0)));
    command.target =
        TrimWide(ControlText(commandTarget_));
    command.arguments =
        TrimWide(ControlText(commandArguments_));
    command.workingDirectory =
        TrimWide(ControlText(commandWorkdir_));
    command.icon = L"auto";
    command.enabled = IsChecked(commandEnabled_);
    command.runAsAdmin = IsChecked(commandAdmin_);
    command.pinned = IsChecked(commandPinned_);
    command.source = CommandSource::User;
    command.basePriority = 120;

    return command;
}

bool SettingsWindow::SaveCommandEditor() {
    if (!editingNew_ && editingCommandId_.empty()) {
        return false;
    }

    Command command = CollectCommandEditor();

    if (command.title.empty() ||
        command.keyword.empty() ||
        command.target.empty()) {

        MessageBoxW(
            hwnd_,
            T(L"名称、主快捷词和目标为必填项。",
              L"Name, primary keyword and target are required."),
            T(L"无法保存快捷项", L"Cannot save shortcut"),
            MB_OK | MB_ICONWARNING);

        return false;
    }

    const std::wstring keyword =
        LowerWide(command.keyword);

    bool duplicateKeyword = false;
    for (const auto& existing : app_.UserCommands()) {
        if (!editingCommandId_.empty() &&
            existing.id == editingCommandId_) {
            continue;
        }

        if (LowerWide(existing.keyword) == keyword) {
            duplicateKeyword = true;
            break;
        }
    }

    if (duplicateKeyword) {
        const int result = MessageBoxW(
            hwnd_,
            T(L"这个主快捷词已被另一个快捷项使用。\n\n仍然保存吗？",
              L"This primary keyword is already used by another shortcut.\n\nSave anyway?"),
            T(L"快捷词冲突", L"Keyword conflict"),
            MB_YESNO | MB_ICONWARNING);

        if (result != IDYES) {
            return false;
        }
    }

    std::wstring savedId = editingCommandId_;
    bool saved = false;

    if (editingNew_) {
        saved = app_.CreateUserCommand(
            std::move(command),
            &savedId);
    } else {
        saved = app_.UpdateUserCommand(
            editingCommandId_,
            std::move(command));
    }

    if (!saved) {
        MessageBoxW(
            hwnd_,
            T(L"写入 commands.json 失败。原数据未被替换。",
              L"Failed to write commands.json. Existing data was not replaced."),
            T(L"保存失败", L"Save failed"),
            MB_OK | MB_ICONERROR);
        return false;
    }

    editingNew_ = false;
    editingCommandId_ = savedId;
    editorDirty_ = false;

    RefreshCommandList(savedId);
    LoadCommandEditor(savedId);

    SetWindowTextW(
        commandStatus_,
        T(L"已保存，Launcher 已刷新。",
          L"Saved. The launcher has been refreshed."));

    return true;
}

void SettingsWindow::DeleteEditingCommand() {
    if (editingCommandId_.empty() || editingNew_) {
        return;
    }

    const int result = MessageBoxW(
        hwnd_,
        T(L"确定删除这个快捷项吗？\n\n此操作会立即写入 commands.json。",
          L"Delete this shortcut?\n\nThe change will be written to commands.json immediately."),
        T(L"删除快捷项", L"Delete shortcut"),
        MB_YESNO | MB_ICONWARNING);

    if (result != IDYES) return;

    const std::wstring id = editingCommandId_;

    if (!app_.DeleteUserCommand(id)) {
        MessageBoxW(
            hwnd_,
            T(L"删除失败。", L"Delete failed."),
            T(L"快捷项", L"Shortcut"),
            MB_OK | MB_ICONERROR);
        return;
    }

    editingCommandId_.clear();
    editingNew_ = false;
    editorDirty_ = false;

    RefreshCommandList();
    if (filteredCommandIds_.empty()) {
        ClearCommandEditor();
    }

    SetWindowTextW(
        commandStatus_,
        T(L"快捷项已删除。", L"Shortcut deleted."));
}

void SettingsWindow::MoveEditingCommand(int direction) {
    if (editingCommandId_.empty() || editingNew_) {
        return;
    }

    if (editorDirty_ && !ConfirmDiscardChanges()) {
        return;
    }

    if (app_.MoveUserCommand(
            editingCommandId_,
            direction)) {
        RefreshCommandList(editingCommandId_);
        LoadCommandEditor(editingCommandId_);
    }
}

void SettingsWindow::TestEditingCommand() {
    Command command = CollectCommandEditor();

    if (command.target.empty()) {
        MessageBoxW(
            hwnd_,
            T(L"请先填写目标。", L"Enter a target first."),
            T(L"测试运行", L"Test"),
            MB_OK | MB_ICONWARNING);
        return;
    }

    if (app_.TestCommand(command)) {
        SetWindowTextW(
            commandStatus_,
            T(L"测试运行已启动。", L"Test launch started."));
    }
}

void SettingsWindow::BrowseCommandTarget() {
    std::wstring current =
        ControlText(commandTarget_);

    std::array<wchar_t, 32768> file{};
    if (!current.empty() &&
        current.size() < file.size()) {
        std::copy(
            current.begin(),
            current.end(),
            file.begin());
    }

    const wchar_t filter[] =
        L"Programs and shortcuts\0*.exe;*.lnk;*.bat;*.cmd;*.com;*.url\0"
        L"All files\0*.*\0\0";

    OPENFILENAMEW open{};
    open.lStructSize = sizeof(open);
    open.hwndOwner = hwnd_;
    open.lpstrFile = file.data();
    open.nMaxFile =
        static_cast<DWORD>(file.size());
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

    const std::filesystem::path path(file.data());

    SetWindowTextW(
        commandTarget_,
        path.wstring().c_str());

    if (TrimWide(ControlText(commandName_)).empty()) {
        SetWindowTextW(
            commandName_,
            path.stem().wstring().c_str());
    }

    if (TrimWide(ControlText(commandWorkdir_)).empty()) {
        SetWindowTextW(
            commandWorkdir_,
            path.parent_path().wstring().c_str());
    }

    if (SendMessageW(
            commandType_,
            CB_GETCURSEL,
            0,
            0) < 0) {
        SendMessageW(
            commandType_,
            CB_SETCURSEL,
            0,
            0);
    }

    MarkEditorDirty();
}

void SettingsWindow::BrowseCommandWorkingDirectory() {
    BROWSEINFOW browse{};
    browse.hwndOwner = hwnd_;
    browse.lpszTitle =
        T(L"选择工作目录", L"Choose working directory");
    browse.ulFlags =
        BIF_RETURNONLYFSDIRS |
        BIF_NEWDIALOGSTYLE |
        BIF_EDITBOX;

    PIDLIST_ABSOLUTE item =
        SHBrowseForFolderW(&browse);

    if (!item) return;

    std::array<wchar_t, MAX_PATH> path{};
    if (SHGetPathFromIDListW(
            item,
            path.data())) {
        SetWindowTextW(
            commandWorkdir_,
            path.data());
        MarkEditorDirty();
    }

    CoTaskMemFree(item);
}

void SettingsWindow::RefreshHotkeyControls() {
    const auto& settings = app_.SettingsData();

    const auto hasModifier =
        [&](std::string_view name) {
            return std::find(
                       settings.hotkeyModifiers.begin(),
                       settings.hotkeyModifiers.end(),
                       name) !=
                   settings.hotkeyModifiers.end();
        };

    SetChecked(hotkeyCtrl_, hasModifier("ctrl") || hasModifier("control"));
    SetChecked(hotkeyAlt_, hasModifier("alt"));
    SetChecked(hotkeyShift_, hasModifier("shift"));
    SetChecked(hotkeyWin_, hasModifier("win") || hasModifier("windows"));

    const UINT desired =
        hotkey::KeyFromName(settings.hotkeyKey);

    const int count =
        static_cast<int>(
            SendMessageW(
                hotkeyKey_,
                CB_GETCOUNT,
                0,
                0));

    int selected = -1;

    for (int i = 0; i < count; ++i) {
        const UINT value =
            static_cast<UINT>(
                SendMessageW(
                    hotkeyKey_,
                    CB_GETITEMDATA,
                    static_cast<WPARAM>(i),
                    0));

        if (value == desired) {
            selected = i;
            break;
        }
    }

    if (selected < 0 && count > 0) {
        selected = 0;
    }

    SendMessageW(
        hotkeyKey_,
        CB_SETCURSEL,
        selected,
        0);

    std::wstring display;
    const auto append =
        [&](std::wstring_view part) {
            if (!display.empty()) display += L" + ";
            display += part;
        };

    if (hasModifier("ctrl") || hasModifier("control")) append(L"Ctrl");
    if (hasModifier("alt")) append(L"Alt");
    if (hasModifier("shift")) append(L"Shift");
    if (hasModifier("win") || hasModifier("windows")) append(L"Win");

    if (desired != 0) {
        append(hotkey::KeyDisplayName(desired));
    }

    std::wstring status =
        T(L"当前热键：", L"Current hotkey: ");
    status += display;

    if (app_.IsGlobalHotkeyRegistered()) {
        status += T(
            L"  ·  已注册",
            L"  ·  Registered");
    } else {
        status += T(
            L"  ·  未注册",
            L"  ·  Not registered");

        const DWORD error =
            app_.GlobalHotkeyLastError();

        if (error != ERROR_SUCCESS) {
            status += T(
                L"（错误码 ",
                L" (error ");
            status += std::to_wstring(error);
            status += T(L"）", L")");
        }
    }

    SetWindowTextW(
        hotkeyStatus_,
        status.c_str());

    const auto hasAuxModifier =
        [&](std::string_view name) {
            return std::find(
                       settings
                           .auxiliaryHotkeyModifiers
                           .begin(),
                       settings
                           .auxiliaryHotkeyModifiers
                           .end(),
                       name) !=
                   settings
                       .auxiliaryHotkeyModifiers
                       .end();
        };

    SetChecked(
        auxiliaryHotkeyEnabled_,
        settings.auxiliaryHotkeyEnabled);
    SetChecked(
        auxiliaryHotkeyCtrl_,
        hasAuxModifier("ctrl") ||
            hasAuxModifier("control"));
    SetChecked(
        auxiliaryHotkeyAlt_,
        hasAuxModifier("alt"));
    SetChecked(
        auxiliaryHotkeyShift_,
        hasAuxModifier("shift"));
    SetChecked(
        auxiliaryHotkeyWin_,
        hasAuxModifier("win") ||
            hasAuxModifier("windows"));

    const UINT auxiliaryDesired =
        hotkey::KeyFromName(
            settings.auxiliaryHotkeyKey);

    const int auxiliaryCount =
        static_cast<int>(
            SendMessageW(
                auxiliaryHotkeyKey_,
                CB_GETCOUNT,
                0,
                0));

    int auxiliarySelected = -1;

    for (int i = 0;
         i < auxiliaryCount;
         ++i) {
        const UINT value =
            static_cast<UINT>(
                SendMessageW(
                    auxiliaryHotkeyKey_,
                    CB_GETITEMDATA,
                    static_cast<WPARAM>(i),
                    0));

        if (value ==
            auxiliaryDesired) {
            auxiliarySelected = i;
            break;
        }
    }

    if (auxiliarySelected < 0 &&
        auxiliaryCount > 0) {
        auxiliarySelected = 0;
    }

    SendMessageW(
        auxiliaryHotkeyKey_,
        CB_SETCURSEL,
        auxiliarySelected,
        0);

    std::wstring auxiliaryDisplay;

    const auto appendAuxiliary =
        [&](std::wstring_view part) {
            if (!auxiliaryDisplay.empty()) {
                auxiliaryDisplay += L" + ";
            }
            auxiliaryDisplay += part;
        };

    if (hasAuxModifier("ctrl") ||
        hasAuxModifier("control")) {
        appendAuxiliary(L"Ctrl");
    }
    if (hasAuxModifier("alt")) {
        appendAuxiliary(L"Alt");
    }
    if (hasAuxModifier("shift")) {
        appendAuxiliary(L"Shift");
    }
    if (hasAuxModifier("win") ||
        hasAuxModifier("windows")) {
        appendAuxiliary(L"Win");
    }

    if (auxiliaryDesired != 0) {
        appendAuxiliary(
            hotkey::KeyDisplayName(
                auxiliaryDesired));
    }

    std::wstring auxiliaryStatus =
        T(L"辅助热键：",
          L"Auxiliary hotkey: ");
    auxiliaryStatus +=
        auxiliaryDisplay;

    if (!settings.auxiliaryHotkeyEnabled) {
        auxiliaryStatus +=
            T(L"  ·  已禁用",
              L"  ·  Disabled");
    } else if (
        app_.IsAuxiliaryHotkeyRegistered()) {
        auxiliaryStatus +=
            T(L"  ·  已注册",
              L"  ·  Registered");
    } else {
        auxiliaryStatus +=
            T(L"  ·  未注册",
              L"  ·  Not registered");

        const DWORD error =
            app_.AuxiliaryHotkeyLastError();

        if (error != ERROR_SUCCESS) {
            auxiliaryStatus +=
                T(L"（错误码 ",
                  L" (error ");
            auxiliaryStatus +=
                std::to_wstring(error);
            auxiliaryStatus +=
                T(L"）", L")");
        }
    }

    SetWindowTextW(
        auxiliaryHotkeyStatus_,
        auxiliaryStatus.c_str());
}

void SettingsWindow::ApplyHotkeyControl() {
    if (syncing_) return;

    std::vector<std::string> modifiers;

    if (IsChecked(hotkeyCtrl_)) modifiers.push_back("ctrl");
    if (IsChecked(hotkeyAlt_)) modifiers.push_back("alt");
    if (IsChecked(hotkeyShift_)) modifiers.push_back("shift");
    if (IsChecked(hotkeyWin_)) modifiers.push_back("win");

    if (modifiers.empty()) {
        MessageBoxW(
            hwnd_,
            T(L"请至少选择一个修饰键（Ctrl / Alt / Shift / Win）。",
              L"Choose at least one modifier (Ctrl / Alt / Shift / Win)."),
            T(L"全局热键", L"Global hotkey"),
            MB_OK | MB_ICONWARNING);
        RefreshHotkeyControls();
        return;
    }

    const int index =
        static_cast<int>(
            SendMessageW(
                hotkeyKey_,
                CB_GETCURSEL,
                0,
                0));

    if (index < 0) {
        return;
    }

    const UINT virtualKey =
        static_cast<UINT>(
            SendMessageW(
                hotkeyKey_,
                CB_GETITEMDATA,
                static_cast<WPARAM>(index),
                0));

    const std::string key =
        hotkey::KeyName(virtualKey);

    if (key.empty() ||
        !app_.SetHotkeySettings(
            std::move(modifiers),
            key)) {

        MessageBoxW(
            hwnd_,
            T(L"Windows 无法注册这个热键，可能已被其他程序占用。旧热键保持不变。",
              L"Windows could not register this hotkey. It may already be used by another application. The previous hotkey is unchanged."),
            T(L"热键冲突", L"Hotkey conflict"),
            MB_OK | MB_ICONWARNING);

        RefreshHotkeyControls();
        return;
    }

    RefreshHotkeyControls();
}

void SettingsWindow::ApplyAuxiliaryHotkeyControl() {
    if (syncing_) return;

    std::vector<std::string> modifiers;

    if (IsChecked(auxiliaryHotkeyCtrl_)) {
        modifiers.push_back("ctrl");
    }
    if (IsChecked(auxiliaryHotkeyAlt_)) {
        modifiers.push_back("alt");
    }
    if (IsChecked(auxiliaryHotkeyShift_)) {
        modifiers.push_back("shift");
    }
    if (IsChecked(auxiliaryHotkeyWin_)) {
        modifiers.push_back("win");
    }

    const int index =
        static_cast<int>(
            SendMessageW(
                auxiliaryHotkeyKey_,
                CB_GETCURSEL,
                0,
                0));

    if (index < 0) {
        RefreshHotkeyControls();
        return;
    }

    const UINT virtualKey =
        static_cast<UINT>(
            SendMessageW(
                auxiliaryHotkeyKey_,
                CB_GETITEMDATA,
                static_cast<WPARAM>(index),
                0));

    const std::string key =
        hotkey::KeyName(virtualKey);

    const bool enabled =
        IsChecked(
            auxiliaryHotkeyEnabled_);

    if (key.empty() ||
        !app_.SetAuxiliaryHotkeySettings(
            enabled,
            std::move(modifiers),
            key)) {

        MessageBoxW(
            hwnd_,
            T(L"Windows 无法注册这个辅助热键，可能已被其他程序占用。旧辅助热键保持不变。",
              L"Windows could not register this auxiliary hotkey. It may already be used by another application. The previous auxiliary hotkey is unchanged."),
            T(L"辅助热键冲突",
              L"Auxiliary hotkey conflict"),
            MB_OK | MB_ICONWARNING);

        RefreshHotkeyControls();
        return;
    }

    RefreshHotkeyControls();
}

void SettingsWindow::ApplyClassicBehaviorControl(
    UINT id) {

    if (syncing_) return;

    const auto settings =
        app_.SettingsData();

    bool wildcardMatching =
        settings.wildcardMatching;
    bool numericQuickLaunch =
        settings.numericQuickLaunch;
    bool executeSingleResult =
        settings
            .executeSingleResultImmediately;

    switch (id) {
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
            executeSingleResult)) {

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
    default:
        return;
    }

    const bool enabled =
        !ToggleChecked(id);

    if (!app_.SetProviderEnabled(
            std::move(providerId),
            enabled)) {
        MessageBoxW(
            hwnd_,
            T(L"无法保存搜索来源设置。",
              L"Unable to save search-source settings."),
            L"ALTRun Next",
            MB_OK | MB_ICONERROR);

        RefreshFromSettings();
    }
}

void SettingsWindow::ApplyMonitorControl() {
    if (syncing_) return;

    const auto settings = app_.SettingsData();

    const int monitorIndex =
        static_cast<int>(SendMessageW(
            popupMonitor_,
            CB_GETCURSEL,
            0,
            0));

    std::string popupMonitor = "cursor";
    if (monitorIndex == 1) popupMonitor = "active";
    else if (monitorIndex == 2) popupMonitor = "primary";

    app_.SetGeneralSettings(
        settings.hideAfterLaunch,
        settings.clearQueryOnShow,
        settings.hideOnFocusLost,
        settings.showTrayIcon,
        std::move(popupMonitor));
}

void SettingsWindow::ApplyAppearanceControls() {
    if (syncing_) return;

    const int styleIndex =
        static_cast<int>(SendMessageW(
            uiStyle_,
            CB_GETCURSEL,
            0,
            0));

    const int languageIndex =
        static_cast<int>(SendMessageW(
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

    if (style != app_.SettingsData().uiStyle) {
        app_.SetUiStyle(style);
    }

    if (language != app_.SettingsData().language) {
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
    const auto rect =
        BuildGeneralLayout(
            generalScrollOffset_)
            .behavior;

    return {
        rect.left,
        rect.top,
        rect.right,
        rect.bottom,
    };
}

RECT SettingsWindow::SearchBehaviorCardRect() const {
    const auto rect =
        BuildGeneralLayout(
            generalScrollOffset_)
            .search;

    return {
        rect.left,
        rect.top,
        rect.right,
        rect.bottom,
    };
}

RECT SettingsWindow::MonitorCardRect() const {
    const auto rect =
        BuildGeneralLayout(
            generalScrollOffset_)
            .monitor;

    return {
        rect.left,
        rect.top,
        rect.right,
        rect.bottom,
    };
}

void SettingsWindow::Layout() {
    if (!hwnd_) return;

    UpdateGeneralScrollBar();

    RECT client{};
    GetClientRect(hwnd_, &client);

    const int sidebar = Scale(kSidebarWidthLogical);
    const int sidebarMargin = Scale(18);
    const int navWidth = sidebar - sidebarMargin * 2;
    const int navHeight = Scale(42);
    const int navGap = Scale(8);

    std::array<HWND, 6> nav{
        navCommands_,
        navGeneral_,
        navAppearance_,
        navProviders_,
        navData_,
        navAbout_,
    };

    for (std::size_t i = 0; i < nav.size(); ++i) {
        MoveWindow(
            nav[i],
            sidebarMargin,
            Scale(72) +
                static_cast<int>(i) *
                    (navHeight + navGap),
            navWidth,
            navHeight,
            TRUE);
    }

    const int contentLeft =
        sidebar + Scale(38);
    const int contentRight =
        client.right - Scale(34);
    const int contentWidth =
        std::max(
            Scale(360),
            contentRight - contentLeft);

    const int pageScroll =
        page_ == Page::General
            ? generalScrollOffset_
            : 0;

    MoveWindow(
        pageTitle_,
        contentLeft,
        Scale(30) -
            pageScroll,
        contentWidth,
        Scale(42),
        TRUE);

    MoveWindow(
        pageDescription_,
        contentLeft,
        Scale(76) -
            pageScroll,
        contentWidth,
        Scale(42),
        TRUE);

    if (page_ == Page::Commands) {
        const int top = Scale(130);
        const int leftWidth =
            std::clamp(
                Scale(300),
                Scale(250),
                std::max(
                    Scale(250),
                    contentWidth / 3));

        const int gap = Scale(28);
        const int editorX =
            contentLeft + leftWidth + gap;
        const int editorWidth =
            std::max(
                Scale(360),
                contentRight - editorX);

        MoveWindow(
            commandSearch_,
            contentLeft,
            top,
            leftWidth - Scale(90),
            Scale(34),
            TRUE);

        MoveWindow(
            commandNew_,
            contentLeft + leftWidth - Scale(82),
            top,
            Scale(82),
            Scale(34),
            TRUE);

        MoveWindow(
            commandList_,
            contentLeft,
            top + Scale(46),
            leftWidth,
            std::max<int>(
                Scale(250),
                static_cast<int>(client.bottom) - top - Scale(118)),
            TRUE);

        MoveWindow(
            commandMoveUp_,
            contentLeft,
            client.bottom - Scale(56),
            (leftWidth - Scale(8)) / 2,
            Scale(34),
            TRUE);

        MoveWindow(
            commandMoveDown_,
            contentLeft +
                (leftWidth - Scale(8)) / 2 +
                Scale(8),
            client.bottom - Scale(56),
            (leftWidth - Scale(8)) / 2,
            Scale(34),
            TRUE);

        MoveWindow(
            commandEditorTitle_,
            editorX,
            top,
            editorWidth,
            Scale(30),
            TRUE);

        const int labelWidth = Scale(112);
        const int fieldX =
            editorX + labelWidth;
        const int fieldWidth =
            std::max(
                Scale(210),
                editorWidth - labelWidth);
        const int browseWidth = Scale(44);
        const int rowHeight = Scale(42);
        int y = top + Scale(42);

        auto placeField =
            [&](HWND label,
                HWND control,
                HWND browse = nullptr) {
                MoveWindow(
                    label,
                    editorX,
                    y + Scale(5),
                    labelWidth - Scale(10),
                    Scale(26),
                    TRUE);

                const int width =
                    browse
                        ? fieldWidth - browseWidth - Scale(8)
                        : fieldWidth;

                MoveWindow(
                    control,
                    fieldX,
                    y,
                    width,
                    Scale(32),
                    TRUE);

                if (browse) {
                    MoveWindow(
                        browse,
                        fieldX + width + Scale(8),
                        y,
                        browseWidth,
                        Scale(32),
                        TRUE);
                }

                y += rowHeight;
            };

        placeField(
            commandNameLabel_,
            commandName_);
        placeField(
            commandKeywordLabel_,
            commandKeyword_);
        placeField(
            commandAliasesLabel_,
            commandAliases_);
        placeField(
            commandTypeLabel_,
            commandType_);
        placeField(
            commandTargetLabel_,
            commandTarget_,
            commandBrowseTarget_);
        placeField(
            commandArgumentsLabel_,
            commandArguments_);
        placeField(
            commandWorkdirLabel_,
            commandWorkdir_,
            commandBrowseWorkdir_);

        MoveWindow(
            commandEnabled_,
            fieldX,
            y + Scale(4),
            Scale(110),
            Scale(28),
            TRUE);

        MoveWindow(
            commandAdmin_,
            fieldX + Scale(118),
            y + Scale(4),
            Scale(190),
            Scale(28),
            TRUE);

        MoveWindow(
            commandPinned_,
            fieldX + Scale(316),
            y + Scale(4),
            Scale(90),
            Scale(28),
            TRUE);

        y += Scale(48);

        MoveWindow(
            commandTest_,
            fieldX,
            y,
            Scale(110),
            Scale(34),
            TRUE);

        MoveWindow(
            commandDelete_,
            fieldX + Scale(120),
            y,
            Scale(92),
            Scale(34),
            TRUE);

        MoveWindow(
            commandCancel_,
            std::max(
                fieldX + Scale(222),
                contentRight - Scale(190)),
            y,
            Scale(92),
            Scale(34),
            TRUE);

        MoveWindow(
            commandSave_,
            contentRight - Scale(90),
            y,
            Scale(90),
            Scale(34),
            TRUE);

        MoveWindow(
            commandStatus_,
            fieldX,
            y + Scale(46),
            fieldWidth,
            Scale(32),
            TRUE);
    }

    if (page_ == Page::General) {
        const auto metrics =
            BuildGeneralLayout(
                generalScrollOffset_);

        const int x =
            metrics.monitor.left;

        const int controlWidth =
            metrics.monitor.right -
            metrics.monitor.left;

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

        const int behaviorRowHeight =
            Scale(46);

        const int behaviorRowX =
            metrics.behavior.left +
            Scale(1);

        const int behaviorRowWidth =
            metrics.behavior.right -
            metrics.behavior.left -
            Scale(2);

        std::array<HWND, 6> behaviorRows{
            startWithWindows_,
            showOnStartup_,
            hideAfterLaunch_,
            clearQueryOnShow_,
            hideOnFocusLost_,
            showTrayIcon_,
        };

        for (std::size_t i = 0;
             i < behaviorRows.size();
             ++i) {

            MoveWindow(
                behaviorRows[i],
                behaviorRowX,
                metrics.behavior.top +
                    Scale(1) +
                    static_cast<int>(i) *
                        behaviorRowHeight,
                behaviorRowWidth,
                behaviorRowHeight,
                TRUE);
        }

        const int searchRowHeight =
            Scale(46);

        const int searchRowX =
            metrics.search.left +
            Scale(1);

        const int searchRowWidth =
            metrics.search.right -
            metrics.search.left -
            Scale(2);

        std::array<HWND, 3> searchRows{
            wildcardMatching_,
            numericQuickLaunch_,
            executeSingleResult_,
        };

        for (std::size_t i = 0;
             i < searchRows.size();
             ++i) {

            MoveWindow(
                searchRows[i],
                searchRowX,
                metrics.search.top +
                    Scale(1) +
                    static_cast<int>(i) *
                        searchRowHeight,
                searchRowWidth,
                searchRowHeight,
                TRUE);
        }

        const int orderTop =
            metrics.search.top +
            Scale(46 * 3);

        MoveWindow(
            numericQuickLaunchOrderLabel_,
            metrics.search.left +
                Scale(18),
            orderTop +
                Scale(11),
            Scale(105),
            Scale(24),
            TRUE);

        MoveWindow(
            numericQuickLaunchOrder_,
            metrics.search.right -
                Scale(150),
            orderTop +
                Scale(7),
            Scale(132),
            Scale(180),
            TRUE);

        MoveWindow(
            hotkeySectionTitle_,
            x,
            metrics.hotkeySectionTop,
            controlWidth,
            Scale(28),
            TRUE);

        const int modifierWidth =
            metrics.compactHotkeys
                ? std::clamp(
                      (controlWidth -
                       Scale(140)) / 4,
                      Scale(26),
                      Scale(56))
                : Scale(58);

        MoveWindow(
            primaryHotkeyLabel_,
            x,
            metrics.primaryRowTop +
                Scale(2),
            Scale(72),
            Scale(28),
            TRUE);

        int hotkeyX =
            x + Scale(82);

        for (HWND control :
             std::array<HWND, 4>{
                 hotkeyCtrl_,
                 hotkeyAlt_,
                 hotkeyShift_,
                 hotkeyWin_}) {

            MoveWindow(
                control,
                hotkeyX,
                metrics.primaryRowTop,
                modifierWidth,
                Scale(30),
                TRUE);

            hotkeyX +=
                modifierWidth;
        }

        const int keyRowX =
            metrics.compactHotkeys
                ? x + Scale(82)
                : hotkeyX + Scale(10);

        const int applyWidth =
            Scale(80);

        const int keyWidth =
            metrics.compactHotkeys
                ? std::max(
                      Scale(80),
                      std::min(
                          Scale(140),
                          controlWidth -
                              Scale(82) -
                              applyWidth -
                              Scale(12)))
                : std::max(
                      Scale(110),
                      std::min(
                          Scale(138),
                          controlWidth -
                              (keyRowX - x) -
                              applyWidth -
                              Scale(12)));

        MoveWindow(
            hotkeyKey_,
            keyRowX,
            metrics.primaryKeyRowTop -
                Scale(2),
            keyWidth,
            Scale(220),
            TRUE);

        MoveWindow(
            hotkeyApply_,
            keyRowX +
                keyWidth +
                Scale(8),
            metrics.primaryKeyRowTop -
                Scale(2),
            applyWidth,
            Scale(32),
            TRUE);

        MoveWindow(
            hotkeyStatus_,
            x,
            metrics.primaryStatusTop,
            controlWidth,
            Scale(24),
            TRUE);

        MoveWindow(
            auxiliaryHotkeyEnabled_,
            x,
            metrics.auxiliaryRowTop,
            metrics.compactHotkeys
                ? Scale(132)
                : Scale(170),
            Scale(30),
            TRUE);

        int auxiliaryX =
            x +
            (metrics.compactHotkeys
                 ? Scale(140)
                 : Scale(180));

        for (HWND control :
             std::array<HWND, 4>{
                 auxiliaryHotkeyCtrl_,
                 auxiliaryHotkeyAlt_,
                 auxiliaryHotkeyShift_,
                 auxiliaryHotkeyWin_}) {

            MoveWindow(
                control,
                auxiliaryX,
                metrics.auxiliaryRowTop,
                modifierWidth,
                Scale(30),
                TRUE);

            auxiliaryX +=
                modifierWidth;
        }

        const int auxiliaryKeyX =
            metrics.compactHotkeys
                ? x + Scale(82)
                : auxiliaryX +
                    Scale(8);

        const int auxiliaryKeyWidth =
            metrics.compactHotkeys
                ? keyWidth
                : std::max(
                      Scale(100),
                      std::min(
                          Scale(138),
                          controlWidth -
                              (auxiliaryKeyX - x) -
                              applyWidth -
                              Scale(12)));

        MoveWindow(
            auxiliaryHotkeyKey_,
            auxiliaryKeyX,
            metrics.auxiliaryKeyRowTop -
                Scale(2),
            auxiliaryKeyWidth,
            Scale(220),
            TRUE);

        MoveWindow(
            auxiliaryHotkeyApply_,
            auxiliaryKeyX +
                auxiliaryKeyWidth +
                Scale(8),
            metrics.auxiliaryKeyRowTop -
                Scale(2),
            applyWidth,
            Scale(32),
            TRUE);

        MoveWindow(
            auxiliaryHotkeyStatus_,
            x,
            metrics.auxiliaryStatusTop,
            controlWidth,
            Scale(24),
            TRUE);

        MoveWindow(
            popupSectionTitle_,
            x,
            metrics.popupSectionTop,
            controlWidth,
            Scale(28),
            TRUE);

        const int monitorWidth =
            metrics.monitor.right -
            metrics.monitor.left;

        MoveWindow(
            popupMonitorLabel_,
            metrics.monitor.left +
                Scale(18),
            metrics.monitor.top +
                Scale(9),
            Scale(180),
            Scale(22),
            TRUE);

        MoveWindow(
            popupMonitorDescription_,
            metrics.monitor.left +
                Scale(18),
            metrics.monitor.top +
                Scale(30),
            std::max(
                Scale(160),
                monitorWidth -
                    Scale(330)),
            Scale(22),
            TRUE);

        MoveWindow(
            popupMonitor_,
            metrics.monitor.right -
                Scale(278),
            metrics.monitor.top +
                Scale(11),
            Scale(250),
            Scale(220),
            TRUE);

        MoveWindow(
            generalNote_,
            x,
            metrics.noteTop,
            controlWidth,
            Scale(28),
            TRUE);
    }

    if (page_ == Page::Appearance) {
        const int x = contentLeft;
        const int y = Scale(150);
        const int controlWidth =
            std::min(
                contentWidth,
                Scale(570));

        MoveWindow(
            uiStyleLabel_,
            x, y,
            Scale(180), Scale(28), TRUE);

        MoveWindow(
            uiStyle_,
            x, y + Scale(34),
            Scale(320), Scale(220), TRUE);

        MoveWindow(
            languageLabel_,
            x, y + Scale(100),
            Scale(180), Scale(28), TRUE);

        MoveWindow(
            language_,
            x, y + Scale(134),
            Scale(320), Scale(220), TRUE);

        MoveWindow(
            appearanceNote_,
            x, y + Scale(204),
            controlWidth, Scale(52), TRUE);
    }

    if (page_ == Page::Providers) {
        const int x = contentLeft;
        const int controlWidth =
            std::min(
                contentWidth,
                Scale(590));

        MoveWindow(
            providerSectionTitle_,
            x,
            Scale(138),
            controlWidth,
            Scale(28),
            TRUE);

        const RECT providerCard =
            ProviderCardRect();

        const int rowHeight =
            Scale(58);
        const int rowX =
            providerCard.left +
            Scale(1);
        const int rowWidth =
            providerCard.right -
            providerCard.left -
            Scale(2);

        std::array<HWND, 4> rows{
            providerStartMenu_,
            providerPackaged_,
            providerAppPaths_,
            providerPath_,
        };

        for (std::size_t i = 0;
             i < rows.size();
             ++i) {
            MoveWindow(
                rows[i],
                rowX,
                providerCard.top +
                    Scale(1) +
                    static_cast<int>(i) *
                        rowHeight,
                rowWidth,
                rowHeight,
                TRUE);
        }

        MoveWindow(
            providerStatus_,
            x,
            providerCard.bottom +
                Scale(24),
            controlWidth,
            Scale(156),
            TRUE);

        MoveWindow(
            providerNote_,
            x,
            providerCard.bottom +
                Scale(190),
            controlWidth,
            Scale(54),
            TRUE);
    }

    if (page_ == Page::Data) {
        const int x = contentLeft;
        const int width =
            std::min(
                contentWidth,
                Scale(700));

        MoveWindow(
            dataOpenLabel_,
            x, Scale(146),
            width, Scale(28), TRUE);

        MoveWindow(
            dataOpenFolder_,
            x, Scale(182),
            Scale(190), Scale(36), TRUE);

        MoveWindow(
            dataTransferLabel_,
            x, Scale(252),
            width, Scale(28), TRUE);

        MoveWindow(
            dataImportTsv_,
            x, Scale(288),
            Scale(200), Scale(36), TRUE);

        MoveWindow(
            dataImportLegacy_,
            x + Scale(214), Scale(288),
            Scale(220), Scale(36), TRUE);

        MoveWindow(
            dataExport_,
            x + Scale(448), Scale(288),
            Scale(190), Scale(36), TRUE);

        MoveWindow(
            dataMaintenanceLabel_,
            x, Scale(370),
            width, Scale(28), TRUE);

        MoveWindow(
            dataClearUsage_,
            x, Scale(406),
            Scale(190), Scale(36), TRUE);

        MoveWindow(
            dataRebuildIndex_,
            x + Scale(204), Scale(406),
            Scale(190), Scale(36), TRUE);

        MoveWindow(
            dataResetSettings_,
            x + Scale(408), Scale(406),
            Scale(190), Scale(36), TRUE);

        MoveWindow(
            dataStatus_,
            x, Scale(468),
            width, Scale(126), TRUE);
    }

    if (page_ == Page::About) {
        const int x = contentLeft;
        const int y = Scale(150);
        const int controlWidth =
            std::min(
                contentWidth,
                Scale(590));

        MoveWindow(
            aboutName_,
            x, y - Scale(12),
            controlWidth, Scale(46), TRUE);

        MoveWindow(
            aboutVersion_,
            x, y + Scale(42),
            controlWidth, Scale(28), TRUE);

        MoveWindow(
            aboutDescription_,
            x, y + Scale(88),
            controlWidth, Scale(70), TRUE);

        MoveWindow(
            dataPathLabel_,
            x, y + Scale(184),
            Scale(200), Scale(28), TRUE);

        MoveWindow(
            dataPath_,
            x, y + Scale(218),
            controlWidth, Scale(30), TRUE);

        MoveWindow(
            openDataFolder_,
            x, y + Scale(270),
            Scale(180), Scale(38), TRUE);

        MoveWindow(
            openGitHub_,
            x + Scale(196),
            y + Scale(270),
            Scale(120), Scale(38), TRUE);
    }
}

RECT SettingsWindow::ProviderCardRect() const {
    RECT client{};
    GetClientRect(
        hwnd_,
        &client);

    const int contentLeft =
        Scale(kSidebarWidthLogical) +
        Scale(42);
    const int contentRight =
        client.right -
        Scale(42);
    const int contentWidth =
        std::max(
            Scale(320),
            contentRight -
                contentLeft);
    const int cardWidth =
        std::min(
            contentWidth,
            Scale(590));

    return {
        contentLeft,
        Scale(170),
        contentLeft + cardWidth,
        Scale(170 + 58 * 4),
    };
}


void SettingsWindow::DrawGeneralToggle(
    const DRAWITEMSTRUCT& item) {

    RECT rect = item.rcItem;

    const COLORREF rowBackground =
        (item.itemState & ODS_SELECTED)
            ? kCardPressed
            : kCardBackground;

    HBRUSH rowBrush =
        CreateSolidBrush(rowBackground);
    FillRect(item.hDC, &rect, rowBrush);
    DeleteObject(rowBrush);

    const UINT id =
        static_cast<UINT>(item.CtlID);
    const bool checked =
        ToggleChecked(id);

    const int boxSize = Scale(20);
    const int boxLeft =
        rect.left + Scale(18);
    const int boxTop =
        rect.top +
        (rect.bottom - rect.top - boxSize) / 2;

    RECT box{
        boxLeft,
        boxTop,
        boxLeft + boxSize,
        boxTop + boxSize,
    };

    HBRUSH boxBrush =
        CreateSolidBrush(
            checked
                ? kAccent
                : RGB(255, 255, 255));

    HPEN boxPen =
        CreatePen(
            PS_SOLID,
            std::max(1, Scale(1)),
            checked
                ? kAccent
                : RGB(166, 174, 184));

    HGDIOBJ oldBrush =
        SelectObject(item.hDC, boxBrush);
    HGDIOBJ oldPen =
        SelectObject(item.hDC, boxPen);

    RoundRect(
        item.hDC,
        box.left,
        box.top,
        box.right,
        box.bottom,
        Scale(5),
        Scale(5));

    SelectObject(item.hDC, oldBrush);
    SelectObject(item.hDC, oldPen);
    DeleteObject(boxBrush);
    DeleteObject(boxPen);

    if (checked) {
        HPEN checkPen =
            CreatePen(
                PS_SOLID,
                std::max(2, Scale(2)),
                RGB(255, 255, 255));

        oldPen =
            SelectObject(
                item.hDC,
                checkPen);

        MoveToEx(
            item.hDC,
            box.left + Scale(5),
            box.top + Scale(10),
            nullptr);

        LineTo(
            item.hDC,
            box.left + Scale(9),
            box.top + Scale(14));

        LineTo(
            item.hDC,
            box.left + Scale(16),
            box.top + Scale(6));

        SelectObject(
            item.hDC,
            oldPen);

        DeleteObject(checkPen);
    }

    const wchar_t* title = L"";
    const wchar_t* description = L"";

    switch (id) {
    case kIdStartWithWindows:
        title = T(
            L"开机启动",
            L"Start with Windows");
        description = T(
            L"登录 Windows 后自动启动 ALTRun Next。",
            L"Launch ALTRun Next automatically after signing in to Windows.");
        break;

    case kIdShowOnStartup:
        title = T(
            L"启动时显示启动器",
            L"Show launcher on startup");
        description = T(
            L"程序启动后立即显示 Launcher；默认保持后台静默启动。",
            L"Show the launcher when the app starts; the default remains silent background startup.");
        break;

    case kIdHideAfterLaunch:
        title = T(
            L"执行后自动隐藏",
            L"Hide after launch");
        description = T(
            L"成功启动快捷项后自动收起启动器。",
            L"Automatically close the launcher after a command starts.");
        break;

    case kIdClearQueryOnShow:
        title = T(
            L"呼出时清空搜索",
            L"Clear query on open");
        description = T(
            L"每次呼出启动器时从空白搜索开始。",
            L"Start with an empty search every time the launcher opens.");
        break;

    case kIdHideOnFocusLost:
        title = T(
            L"失去焦点时隐藏",
            L"Hide when focus is lost");
        description = T(
            L"切换到其他窗口时自动收起启动器。",
            L"Hide the launcher automatically when another window is focused.");
        break;

    case kIdShowTrayIcon:
        title = T(
            L"显示系统托盘图标",
            L"Show system tray icon");
        description = T(
            L"保留托盘入口，用于打开设置、重新加载或退出。",
            L"Keep the tray entry for Settings, reload and exit actions.");
        break;

    case kIdWildcardMatching:
        title = T(
            L"允许 * / ? 通配符",
            L"Enable * / ? wildcards");
        description = T(
            L"查询包含 * 或 ? 时使用 glob 匹配；普通搜索仍使用模糊和拼音匹配。",
            L"Use glob matching when the query contains * or ?; normal fuzzy and pinyin search stays unchanged.");
        break;

    case kIdNumericQuickLaunch:
        title = T(
            L"数字键快速执行结果",
            L"Quick launch with number keys");
        description = T(
            L"Classic 下数字键直接执行对应结果；启用后数字不会输入搜索框。",
            L"In Classic mode, number keys launch the matching result instead of typing digits into the query.");
        break;

    case kIdExecuteSingleResult:
        title = T(
            L"仅剩一个结果时立即执行",
            L"Execute immediately when one result remains");
        description = T(
            L"非空查询只剩唯一结果时立即启动；默认关闭以避免误触。",
            L"Launch immediately when a non-empty query narrows to one result; off by default to avoid accidents.");
        break;

    case kIdProviderStartMenu:
        title = T(
            L"开始菜单",
            L"Start Menu");
        description = T(
            L"发现当前用户和所有用户开始菜单中的快捷方式与程序。",
            L"Discover shortcuts and programs from the current-user and all-users Start Menu.");
        break;

    case kIdProviderPackaged:
        title = T(
            L"Windows Apps",
            L"Windows Apps");
        description = T(
            L"发现 Microsoft Store、UWP 和 MSIX 应用。",
            L"Discover Microsoft Store, UWP and MSIX applications.");
        break;

    case kIdProviderAppPaths:
        title = L"App Paths";
        description = T(
            L"从 Windows 注册表的 App Paths 中发现传统桌面程序。",
            L"Discover traditional desktop apps from the Windows App Paths registry.");
        break;

    case kIdProviderPath:
        title = L"PATH";
        description = T(
            L"发现 PATH 环境变量目录中的 EXE、COM、BAT 和 CMD。",
            L"Discover EXE, COM, BAT and CMD files exposed through the PATH environment variable.");
        break;

    default:
        break;
    }

    SetBkMode(item.hDC, TRANSPARENT);

    RECT titleRect{
        box.right + Scale(14),
        rect.top + Scale(8),
        rect.right - Scale(16),
        rect.top + Scale(31),
    };

    HGDIOBJ oldFont =
        SelectObject(
            item.hDC,
            sectionFont_);

    SetTextColor(item.hDC, kText);

    DrawTextW(
        item.hDC,
        title,
        -1,
        &titleRect,
        DT_LEFT | DT_SINGLELINE |
            DT_VCENTER | DT_NOPREFIX);

    RECT descriptionRect{
        titleRect.left,
        rect.top + Scale(31),
        titleRect.right,
        rect.bottom - Scale(7),
    };

    SelectObject(
        item.hDC,
        normalFont_);

    SetTextColor(item.hDC, kMuted);

    DrawTextW(
        item.hDC,
        description,
        -1,
        &descriptionRect,
        DT_LEFT | DT_SINGLELINE |
            DT_VCENTER | DT_END_ELLIPSIS |
            DT_NOPREFIX);

    SelectObject(
        item.hDC,
        oldFont);

    const bool lastRow =
        id == kIdShowTrayIcon ||
        id == kIdProviderPath;

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
            rect.left + Scale(52),
            rect.bottom - 1,
            nullptr);

        LineTo(
            item.hDC,
            rect.right - Scale(14),
            rect.bottom - 1);

        SelectObject(
            item.hDC,
            oldPen);

        DeleteObject(separator);
    }

    if (item.itemState & ODS_FOCUS) {
        RECT focus = rect;
        InflateRect(
            &focus,
            -Scale(6),
            -Scale(5));
        DrawFocusRect(
            item.hDC,
            &focus);
    }
}

void SettingsWindow::CenterOnCurrentMonitor() {
    POINT cursor{};
    GetCursorPos(&cursor);

    HMONITOR monitor =
        MonitorFromPoint(
            cursor,
            MONITOR_DEFAULTTONEAREST);

    MONITORINFO info{sizeof(info)};
    GetMonitorInfoW(monitor, &info);

    RECT rect{};
    GetWindowRect(hwnd_, &rect);

    const int requestedWidth =
        rect.right - rect.left;

    const int requestedHeight =
        rect.bottom - rect.top;

    const int workWidth =
        info.rcWork.right -
        info.rcWork.left;

    const int workHeight =
        info.rcWork.bottom -
        info.rcWork.top;

    // Per-monitor DPI can make the logical default larger than the
    // available work area (for example 150% scaling on a 1080p panel).
    // Never center an oversized Settings window partly off-screen.
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
        SWP_NOZORDER | SWP_NOACTIVATE);
}

void SettingsWindow::Show() {
    if (!hwnd_) return;

    // Retry a binding that previously failed, but never tear down a
    // working hotkey merely because the Settings window was opened.
    app_.RepairGlobalHotkey(false);
    RefreshFromSettings();
    RefreshCommandList(editingCommandId_);

    if (!IsWindowVisible(hwnd_)) {
        CenterOnCurrentMonitor();
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
    case WM_COMMAND: {
        const UINT id = LOWORD(wParam);
        const UINT notify = HIWORD(wParam);

        switch (id) {
        case kIdNavCommands:
            if (notify == BN_CLICKED) {
                ShowPage(Page::Commands);
            }
            return 0;

        case kIdNavGeneral:
            if (notify == BN_CLICKED) {
                ShowPage(Page::General);
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

        case kIdCommandSearch:
            if (notify == EN_CHANGE && !syncing_) {
                RefreshCommandList(editingCommandId_);
            }
            return 0;

        case kIdCommandList:
            if (notify == LBN_SELCHANGE && !syncing_) {
                const int index =
                    static_cast<int>(
                        SendMessageW(
                            commandList_,
                            LB_GETCURSEL,
                            0,
                            0));

                if (index >= 0 &&
                    index <
                        static_cast<int>(
                            filteredCommandIds_.size())) {

                    const std::wstring selectedId =
                        filteredCommandIds_[
                            static_cast<std::size_t>(index)];

                    if (selectedId != editingCommandId_) {
                        if (!ConfirmDiscardChanges()) {
                            RefreshCommandList(
                                editingCommandId_);
                            return 0;
                        }

                        LoadCommandEditor(selectedId);
                    }
                }
            }
            return 0;

        case kIdCommandNew:
            if (notify == BN_CLICKED) {
                BeginNewCommand();
            }
            return 0;

        case kIdCommandMoveUp:
            if (notify == BN_CLICKED) {
                MoveEditingCommand(-1);
            }
            return 0;

        case kIdCommandMoveDown:
            if (notify == BN_CLICKED) {
                MoveEditingCommand(1);
            }
            return 0;

        case kIdCommandBrowseTarget:
            if (notify == BN_CLICKED) {
                BrowseCommandTarget();
            }
            return 0;

        case kIdCommandBrowseWorkdir:
            if (notify == BN_CLICKED) {
                BrowseCommandWorkingDirectory();
            }
            return 0;

        case kIdCommandTest:
            if (notify == BN_CLICKED) {
                TestEditingCommand();
            }
            return 0;

        case kIdCommandDelete:
            if (notify == BN_CLICKED) {
                DeleteEditingCommand();
            }
            return 0;

        case kIdCommandCancel:
            if (notify == BN_CLICKED) {
                editorDirty_ = false;

                if (editingNew_) {
                    editingNew_ = false;
                    editingCommandId_.clear();
                    RefreshCommandList();
                    if (filteredCommandIds_.empty()) {
                        ClearCommandEditor();
                    }
                } else if (!editingCommandId_.empty()) {
                    LoadCommandEditor(
                        editingCommandId_);
                }
            }
            return 0;

        case kIdCommandSave:
            if (notify == BN_CLICKED) {
                SaveCommandEditor();
            }
            return 0;

        case kIdCommandName:
        case kIdCommandKeyword:
        case kIdCommandAliases:
        case kIdCommandTarget:
        case kIdCommandArguments:
        case kIdCommandWorkdir:
            if (notify == EN_CHANGE) {
                MarkEditorDirty();
            }
            return 0;

        case kIdCommandType:
            if (notify == CBN_SELCHANGE) {
                MarkEditorDirty();
            }
            return 0;

        case kIdCommandEnabled:
        case kIdCommandAdmin:
        case kIdCommandPinned:
            if (notify == BN_CLICKED) {
                MarkEditorDirty();
            }
            return 0;

        case kIdStartWithWindows:
        case kIdShowOnStartup:
        case kIdHideAfterLaunch:
        case kIdClearQueryOnShow:
        case kIdHideOnFocusLost:
        case kIdShowTrayIcon:
            if (notify == BN_CLICKED) {
                ToggleGeneralSetting(id);
            }
            return 0;

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

        case kIdAuxHotkeyEnabled:
            if (notify == BN_CLICKED) {
                ApplyAuxiliaryHotkeyControl();
            }
            return 0;

        case kIdAuxHotkeyApply:
            if (notify == BN_CLICKED) {
                ApplyAuxiliaryHotkeyControl();
            }
            return 0;

        case kIdProviderStartMenu:
        case kIdProviderPackaged:
        case kIdProviderAppPaths:
        case kIdProviderPath:
            if (notify == BN_CLICKED) {
                ToggleProviderSetting(id);
            }
            return 0;

        case kIdHotkeyApply:
            if (notify == BN_CLICKED) {
                ApplyHotkeyControl();
            }
            return 0;

        case kIdPopupMonitor:
            if (notify == CBN_SELCHANGE) {
                ApplyMonitorControl();
            }
            return 0;

        case kIdDataOpenFolder:
            if (notify == BN_CLICKED) {
                app_.OpenDataFolder();
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

        default:
            break;
        }
        break;
    }

    case WM_DRAWITEM: {
        const auto* item =
            reinterpret_cast<DRAWITEMSTRUCT*>(
                lParam);

        if (item &&
            (item->CtlID == kIdStartWithWindows ||
             item->CtlID == kIdShowOnStartup ||
             item->CtlID == kIdHideAfterLaunch ||
             item->CtlID == kIdClearQueryOnShow ||
             item->CtlID == kIdHideOnFocusLost ||
             item->CtlID == kIdShowTrayIcon ||
             item->CtlID == kIdWildcardMatching ||
             item->CtlID == kIdNumericQuickLaunch ||
             item->CtlID == kIdExecuteSingleResult ||
             item->CtlID == kIdProviderStartMenu ||
             item->CtlID == kIdProviderPackaged ||
             item->CtlID == kIdProviderAppPaths ||
             item->CtlID == kIdProviderPath)) {
            DrawGeneralToggle(*item);
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
        GetClientRect(hwnd_, &client);
        FillRect(
            dc,
            &client,
            backgroundBrush_);

        RECT sidebar{
            client.left,
            client.top,
            Scale(kSidebarWidthLogical),
            client.bottom
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

        const int sidebarX =
            Scale(kSidebarWidthLogical);

        MoveToEx(
            dc,
            sidebarX,
            client.top,
            nullptr);

        LineTo(
            dc,
            sidebarX,
            client.bottom);

        SelectObject(
            dc,
            oldPen);

        DeleteObject(separator);

        if (page_ == Page::General) {
            for (const RECT card :
                 std::array<RECT, 3>{
                     BehaviorCardRect(),
                     SearchBehaviorCardRect(),
                     MonitorCardRect()}) {

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
                    Scale(8),
                    Scale(8));

                SelectObject(
                    dc,
                    previousBrush);

                SelectObject(
                    dc,
                    previousPen);

                DeleteObject(fill);
                DeleteObject(border);
            }
        }

        if (page_ == Page::Providers) {
            const RECT card =
                ProviderCardRect();

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
                Scale(8),
                Scale(8));

            SelectObject(
                dc,
                previousBrush);
            SelectObject(
                dc,
                previousPen);

            DeleteObject(fill);
            DeleteObject(border);
        }

        if (page_ == Page::Commands) {
            RECT clientRect{};
            GetClientRect(hwnd_, &clientRect);

            const int contentLeft =
                Scale(kSidebarWidthLogical) +
                Scale(38);

            const int contentRight =
                clientRect.right - Scale(34);

            const int contentWidth =
                std::max(
                    Scale(360),
                    contentRight - contentLeft);

            const int leftWidth =
                std::clamp(
                    Scale(300),
                    Scale(250),
                    std::max(
                        Scale(250),
                        contentWidth / 3));

            const int dividerX =
                contentLeft +
                leftWidth +
                Scale(14);

            HPEN divider =
                CreatePen(
                    PS_SOLID,
                    1,
                    kBorder);

            oldPen =
                SelectObject(
                    dc,
                    divider);

            MoveToEx(
                dc,
                dividerX,
                Scale(130),
                nullptr);

            LineTo(
                dc,
                dividerX,
                client.bottom - Scale(22));

            SelectObject(
                dc,
                oldPen);

            DeleteObject(divider);
        }

        EndPaint(
            hwnd_,
            &paint);

        return 0;
    }

    case WM_ERASEBKGND:
        return 1;

    case WM_CTLCOLORSTATIC: {
        HDC dc =
            reinterpret_cast<HDC>(wParam);

        HWND control =
            reinterpret_cast<HWND>(lParam);

        const bool cardStatic =
            control ==
                numericQuickLaunchOrderLabel_ ||
            control == popupMonitorLabel_ ||
            control == popupMonitorDescription_;

        const COLORREF background =
            cardStatic
                ? kCardBackground
                : kWindowBackground;

        SetBkMode(dc, OPAQUE);
        SetBkColor(dc, background);

        if (control == pageDescription_ ||
            control == commandStatus_ ||
            control == hotkeyStatus_ ||
            control == auxiliaryHotkeyStatus_ ||
            control == dataStatus_ ||
            control == generalNote_ ||
            control == popupMonitorDescription_ ||
            control == appearanceNote_ ||
            control == providerStatus_ ||
            control == providerNote_ ||
            control == aboutVersion_ ||
            control == aboutDescription_ ||
            control == dataPathLabel_ ||
            control == dataPath_) {
            SetTextColor(dc, kMuted);
        } else {
            SetTextColor(dc, kText);
        }

        return reinterpret_cast<LRESULT>(
            cardStatic
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
            Scale(920);

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
        if (page_ == Page::Commands &&
            !ConfirmDiscardChanges()) {
            return 0;
        }

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
