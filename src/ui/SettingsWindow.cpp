#include "SettingsWindow.hpp"

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
#include <ctime>
#include <cwctype>
#include <filesystem>
#include <iomanip>
#include <iterator>
#include <sstream>
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
    navDiagnostics_ =
        CreateButton(
            L"",
            kIdNavDiagnostics,
            BS_OWNERDRAW);
    navAppearance_ =
        CreateButton(
            L"",
            kIdNavAppearance,
            BS_OWNERDRAW);
    navProviders_ =
        CreateButton(
            L"",
            kIdNavProviders,
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

    pageTitle_ = CreateStatic(L"", SS_LEFT);
    pageDescription_ = CreateStatic(
        L"",
        SS_LEFT | SS_NOPREFIX);

    CreateGeneralPage();
    CreateHotkeyPage();
    CreateAppearancePage();
    CreateProviderPage();
    CreateDataPage();
    CreateDiagnosticsPage();
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

    commandNameLabel_ = CreateStatic(
        L"",
        SS_LEFTNOWORDWRAP | SS_NOPREFIX);
    commandName_ = CreateEdit(kIdCommandName);

    commandKeywordLabel_ = CreateStatic(
        L"",
        SS_LEFTNOWORDWRAP | SS_NOPREFIX);
    commandKeyword_ = CreateEdit(kIdCommandKeyword);

    commandAliasesLabel_ = CreateStatic(
        L"",
        SS_LEFTNOWORDWRAP | SS_NOPREFIX);
    commandAliases_ = CreateEdit(kIdCommandAliases);

    commandTypeLabel_ = CreateStatic(
        L"",
        SS_LEFTNOWORDWRAP | SS_NOPREFIX);
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

    commandTargetLabel_ = CreateStatic(
        L"",
        SS_LEFTNOWORDWRAP | SS_NOPREFIX);
    commandTarget_ = CreateEdit(kIdCommandTarget);
    commandBrowseTarget_ =
        CreateButton(L"...", kIdCommandBrowseTarget);

    commandArgumentsLabel_ = CreateStatic(
        L"",
        SS_LEFTNOWORDWRAP | SS_NOPREFIX);
    commandArguments_ = CreateEdit(kIdCommandArguments);

    commandWorkdirLabel_ = CreateStatic(
        L"",
        SS_LEFTNOWORDWRAP | SS_NOPREFIX);
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
        pinyinSearch_,
        wildcardMatching_,
        numericQuickLaunch_,
        executeSingleResult_,
        numericQuickLaunchOrderLabel_,
        numericQuickLaunchOrder_,
        popupSectionTitle_,
        popupMonitorLabel_,
        popupMonitorDescription_,
        popupMonitor_,
        generalNote_,
    };

    legacyHotkeyControls_ = {
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
    };

    for (HWND control :
         legacyHotkeyControls_) {
        ShowWindow(
            control,
            SW_HIDE);
    }
}

void SettingsWindow::CreateHotkeyPage() {
    hotkeyActionList_ =
        CreateWindowExW(
            WS_EX_CLIENTEDGE,
            L"LISTBOX",
            L"",
            WS_CHILD | WS_VISIBLE |
                WS_TABSTOP | WS_VSCROLL |
                LBS_NOTIFY |
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

    showResultIcons_ =
        CreateCheckbox(
            L"",
            kIdShowResultIcons);

    resultIconsNote_ = CreateStatic(
        L"",
        SS_LEFT | SS_NOPREFIX);

    appearanceNote_ = CreateStatic(
        L"",
        SS_LEFT | SS_NOPREFIX);

    appearanceControls_ = {
        uiStyleLabel_,
        uiStyle_,
        languageLabel_,
        language_,
        showResultIcons_,
        resultIconsNote_,
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
        navHotkeys_,
        navDiagnostics_,
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
        hotkeyActionList_,
        hotkeyEditorDescription_,
        hotkeyScope_,
        hotkeyEnabled_,
        hotkeyCapture_,
        hotkeyResetCurrent_,
        hotkeyResetAll_,
        hotkeyPageStatus_,
        hotkeyPageNote_,
        diagnosticsMemoryStatus_,
        diagnosticsSearchStatus_,
        actionsWindowsStatus_,
        actionsClipboardStatus_,
        actionsWebStatus_,
        actionsNote_,
        popupMonitorLabel_,
        popupMonitorDescription_,
        popupMonitor_,
        generalNote_,
        uiStyleLabel_,
        uiStyle_,
        languageLabel_,
        language_,
        showResultIcons_,
        resultIconsNote_,
        appearanceNote_,
        providerStartMenu_,
        providerPackaged_,
        providerAppPaths_,
        providerPath_,
        providerEverything_,
        providerStatus_,
        providerGetEverything_,
        providerRecheckEverything_,
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

    for (HWND control : std::array<HWND, 15>{
             commandEditorTitle_,
             generalBehaviorTitle_,
             searchBehaviorTitle_,
             hotkeySectionTitle_,
             hotkeyEditorTitle_,
             diagnosticsMemoryTitle_,
             diagnosticsSearchTitle_,
             actionsWindowsTitle_,
             actionsClipboardTitle_,
             actionsWebTitle_,
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

    for (HWND control : std::array<HWND, 14>{
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
             providerPath_,
             providerEverything_}) {
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
        T(L"热键已集中到“快捷键”页面管理；这里仅保留启动器行为、搜索方式和呼出位置。",
          L"Hotkeys are managed centrally on the Hotkeys page; this page now focuses on launcher behavior, search and placement."));

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
        T(L"点击快捷键按钮后直接按下新的组合键；Esc 取消。全局热键只有在 Windows 注册成功后才会保存，内部热键会检查与其他动作以及基础导航键的冲突。",
          L"Click the binding button, then press the new key combination; Esc cancels. Global bindings are saved only after Windows registers them, while launcher bindings are checked against actions and reserved navigation keys."));

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
        showResultIcons_,
        T(L"显示搜索结果图标",
          L"Show search result icons"));

    SetWindowTextW(
        resultIconsNote_,
        T(L"关闭时不会解析或缓存 Windows Shell 图标，可减少连续搜索时的前端开销。",
          L"When disabled, Windows Shell icons are not resolved or cached, reducing front-end work while typing."));

    SetWindowTextW(
        appearanceNote_,
        T(L"外观、语言和结果图标设置会立即应用，并写入 data/settings.json。",
          L"Appearance, language and result-icon changes apply immediately and are saved to data/settings.json."));

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
        T(L"此页每秒刷新一次运行时快照，不提供行为开关，也不会主动裁剪工作集。Working Set / Private Bytes 与任务管理器“内存”列的统计口径可能不同。",
          L"This page refreshes runtime snapshots once per second, provides no behavior toggles and never trims the working set. Working Set / Private Bytes can differ from Task Manager's Memory column."));

    SetWindowTextW(
        providerSectionTitle_,
        T(L"搜索来源", L"Search sources"));
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
        T(L"Everything 通过本机 IPC 实时查询。ALTRun Next 不预捆绑 Everything：优先复用本机已有标准版；自己管理的便携版会使用 Everything Service 完成 NTFS 索引，并在后台运行且隐藏托盘图标。首次安装服务会出现一次 Windows UAC。",
          L"Everything is queried live over local IPC. ALTRun Next does not bundle Everything: existing standard copies are preferred; its managed portable copy uses the Everything Service for NTFS indexing and runs in the background with the tray icon hidden. Installing the service requires one Windows UAC confirmation."));

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
        T(L"轻量级、键盘优先的 Windows 快捷启动器。\nv0.7 将快捷项提升为独立核心管理功能。",
          L"A lightweight, keyboard-first Windows launcher.\nv0.7 promotes shortcuts into a first-class management workflow."));

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

    RefreshHotkeyControls();
    RefreshHotkeyPage();
    RefreshActionDiagnostics();

    for (HWND control : std::array<HWND, 15>{
             startWithWindows_,
             showOnStartup_,
             hideAfterLaunch_,
             clearQueryOnShow_,
             hideOnFocusLost_,
             showTrayIcon_,
             pinyinSearch_,
             wildcardMatching_,
             numericQuickLaunch_,
             executeSingleResult_,
             providerStartMenu_,
             providerPackaged_,
             providerAppPaths_,
             providerPath_,
             providerEverything_}) {
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
        std::wstring label =
            HotkeyActionLabel(
                action.id);

        const auto binding =
            EffectiveHotkeyBinding(
                app_.SettingsData()
                    .hotkeyBindings,
                action.id);

        label += L"    ";
        label += binding.enabled
            ? FormatHotkeyBinding(
                  action.id)
            : T(L"（已禁用）",
                L"(disabled)");

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

        text += L"\r\n";
    }

    const bool everythingEnabled =
        providers::IsEnabled(
            app_.SettingsData()
                .providerEnabled,
            providers::
                kEverythingFilesystem,
            false);

    bool showGetEverything = false;
    bool showRecheck = false;

    text += T(
        L"Everything 文件与文件夹",
        L"Everything files & folders");
    text += L"  ·  ";

    if (!everythingEnabled) {
        text += T(
            L"已禁用",
            L"Disabled");
    } else {
        const auto ipc =
            app_.EverythingStatus();
        const auto bootstrap =
            app_.EverythingBootstrapStatus();

        if (ipc.availability ==
            EverythingAvailability::
                Available) {
            text += T(
                L"IPC 可用",
                L"IPC available");

            if (!ipc.ipcWindowClass.empty()) {
                text += L"\r\n    ↳ ";
                text += T(
                    L"IPC 端点：",
                    L"IPC endpoint: ");
                text += ipc.ipcWindowClass;

                if (ipc.namedInstanceFallback) {
                    text += T(
                        L"  ·  命名实例",
                        L"  ·  named instance");
                }
            }

            if (bootstrap.downloaded) {
                text += L"\r\n    ↳ ";
                text += T(
                    L"已由 ALTRun Next 获取并启动官方标准便携版",
                    L"Official standard portable build was fetched and started by ALTRun Next");
            }

            if (ipc.hasQuery) {
                text += L"\r\n    ↳ ";
                text += T(
                    L"最近查询：",
                    L"Last query: ");

                switch (ipc.lastStatus) {
                case EverythingQueryStatus::Success:
                    text += T(
                        L"成功",
                        L"Success");
                    break;
                case EverythingQueryStatus::Unavailable:
                    text += T(
                        L"不可用",
                        L"Unavailable");
                    break;
                case EverythingQueryStatus::SendTimeout:
                    text += T(
                        L"发送超时",
                        L"Send timeout");
                    break;
                case EverythingQueryStatus::ReplyTimeout:
                    text += T(
                        L"响应超时",
                        L"Reply timeout");
                    break;
                case EverythingQueryStatus::ProtocolError:
                    text += T(
                        L"协议错误",
                        L"Protocol error");
                    break;
                case EverythingQueryStatus::Cancelled:
                    text += T(
                        L"已取消",
                        L"Cancelled");
                    break;
                }

                text += T(
                    L"  ·  显示 ",
                    L"  ·  returned ");
                text += std::to_wstring(
                    ipc.lastResultCount);

                if (ipc.lastTotalMatches > 0) {
                    text += L" / ";
                    text += std::to_wstring(
                        ipc.lastTotalMatches);
                }

                text += T(
                    L"  ·  耗时 ",
                    L"  ·  latency ");
                text += std::to_wstring(
                    std::max<std::int64_t>(
                        0,
                        (ipc.lastLatency.count() +
                         500) /
                            1000));
                text += L" ms";
            } else {
                text += T(
                    L"  ·  等待首次查询",
                    L"  ·  Waiting for first query");
            }
        } else if (bootstrap.running) {
            text += T(
                L"正在准备 Everything",
                L"Preparing Everything");
            text += L"  ·  ";

            switch (bootstrap.stage) {
            case win::EverythingBootstrapStage::
                Discovering:
                text += T(
                    L"检测本机已有版本",
                    L"Looking for an existing copy");
                break;
            case win::EverythingBootstrapStage::
                StartingExisting:
                text += T(
                    L"正在启动已有版本",
                    L"Starting existing copy");
                break;
            case win::EverythingBootstrapStage::
                DownloadingManifest:
                text += T(
                    L"获取官方 SHA-256 清单",
                    L"Fetching official SHA-256 manifest");
                break;
            case win::EverythingBootstrapStage::
                DownloadingPackage:
                text += T(
                    L"下载官方标准便携版",
                    L"Downloading official standard portable build");
                if (bootstrap.downloadedBytes > 0) {
                    text += L"  ·  ";
                    text += FormatBytes(
                        bootstrap.downloadedBytes);
                    if (bootstrap.totalBytes > 0) {
                        text += L" / ";
                        text += FormatBytes(
                            bootstrap.totalBytes);
                    }
                }
                break;
            case win::EverythingBootstrapStage::
                VerifyingPackage:
                text += T(
                    L"校验 SHA-256",
                    L"Verifying SHA-256");
                break;
            case win::EverythingBootstrapStage::
                ExtractingPackage:
                text += T(
                    L"解压便携版",
                    L"Extracting portable build");
                break;
            case win::EverythingBootstrapStage::
                ConfiguringManaged:
                text += T(
                    L"配置后台运行并隐藏托盘图标",
                    L"Configuring background mode and hidden tray icon");
                break;
            case win::EverythingBootstrapStage::
                StoppingManaged:
                text += T(
                    L"正在重启托管 Everything 以应用配置",
                    L"Restarting managed Everything to apply configuration");
                break;
            case win::EverythingBootstrapStage::
                InstallingService:
                text += T(
                    L"安装 / 启动 Everything Service（请确认 UAC）",
                    L"Installing / starting Everything Service (confirm UAC)");
                break;
            case win::EverythingBootstrapStage::
                WaitingForService:
                text += T(
                    L"等待 Everything Service 就绪",
                    L"Waiting for Everything Service");
                break;
            case win::EverythingBootstrapStage::
                StartingManaged:
                text += T(
                    L"启动托管实例",
                    L"Starting managed instance");
                break;
            case win::EverythingBootstrapStage::
                WaitingForIpc:
                text += T(
                    L"等待 IPC 就绪",
                    L"Waiting for IPC");
                break;
            default:
                text += T(
                    L"处理中",
                    L"Working");
                break;
            }
        } else {
            showGetEverything = true;
            showRecheck = true;

            if (bootstrap.stage ==
                    win::EverythingBootstrapStage::
                        NeedsInstall &&
                bootstrap.failure ==
                    win::EverythingBootstrapFailure::
                        ServiceRequired) {
                text += T(
                    L"托管 Everything 已就绪，但缺少 NTFS 索引服务",
                    L"Managed Everything is present, but the NTFS indexing service is missing");
                text += L"\r\n    ↳ ";
                text += T(
                    L"点击“获取并启动 Everything”安装 Everything Service；Windows 只会在首次安装服务时请求 UAC。托管版将继续以普通用户后台运行且不显示托盘图标。",
                    L"Choose Get and start Everything to install the Everything Service. Windows asks for UAC only when the service is first installed. The managed client will continue as a standard-user background process with no tray icon.");
            } else if (
                bootstrap.stage ==
                    win::EverythingBootstrapStage::
                        NeedsInstall &&
                bootstrap.failure ==
                    win::EverythingBootstrapFailure::
                        IpcUnavailable) {
                text += T(
                    L"检测到 Everything，但 IPC 不可用",
                    L"Everything was found, but IPC is unavailable");
                text += L"  ·  ";
                text += T(
                    L"可能是 Lite 版或当前实例配置不兼容",
                    L"It may be Lite or an incompatible instance configuration");
            } else if (
                bootstrap.stage ==
                win::EverythingBootstrapStage::
                    Failed) {
                text += T(
                    L"自动准备失败",
                    L"Automatic preparation failed");

                text += L"  ·  ";
                switch (bootstrap.failure) {
                case win::EverythingBootstrapFailure::
                    ManifestDownloadFailed:
                    text += T(
                        L"无法获取官方校验清单",
                        L"Could not fetch the official checksum manifest");
                    break;
                case win::EverythingBootstrapFailure::
                    PackageChecksumMissing:
                    text += T(
                        L"官方清单中缺少当前安装包校验值",
                        L"The official manifest does not contain this package");
                    break;
                case win::EverythingBootstrapFailure::
                    PackageDownloadFailed:
                    text += T(
                        L"下载安装包失败",
                        L"Package download failed");
                    break;
                case win::EverythingBootstrapFailure::
                    PackageHashFailed:
                    text += T(
                        L"无法计算安装包 SHA-256",
                        L"Could not calculate package SHA-256");
                    break;
                case win::EverythingBootstrapFailure::
                    PackageHashMismatch:
                    text += T(
                        L"SHA-256 校验不一致，安装包已拒绝",
                        L"SHA-256 mismatch; the package was rejected");
                    break;
                case win::EverythingBootstrapFailure::
                    PackageStagingFailed:
                    text += T(
                        L"已校验安装包转入 ZIP 解压阶段失败",
                        L"Could not stage the verified package as a ZIP for extraction");
                    break;
                case win::EverythingBootstrapFailure::
                    ExtractionFailed:
                    text += T(
                        L"解压失败",
                        L"Extraction failed");
                    break;
                case win::EverythingBootstrapFailure::
                    ManagedStopFailed:
                    text += T(
                        L"无法关闭旧的托管 Everything 实例",
                        L"Could not stop the previous managed Everything instance");
                    break;
                case win::EverythingBootstrapFailure::
                    ManagedConfigFailed:
                    text += T(
                        L"无法写入托管 Everything 配置",
                        L"Could not write the managed Everything configuration");
                    break;
                case win::EverythingBootstrapFailure::
                    ServiceElevationCancelled:
                    text += T(
                        L"已取消 UAC，Everything Service 未安装",
                        L"UAC was cancelled; the Everything Service was not installed");
                    break;
                case win::EverythingBootstrapFailure::
                    ServiceInstallFailed:
                    text += T(
                        L"Everything Service 安装 / 启动失败",
                        L"Everything Service installation / startup failed");
                    break;
                case win::EverythingBootstrapFailure::
                    ServiceUnavailable:
                    text += T(
                        L"Everything Service 未能进入运行状态",
                        L"Everything Service did not reach the running state");
                    break;
                case win::EverythingBootstrapFailure::
                    ManagedLaunchFailed:
                case win::EverythingBootstrapFailure::
                    ExistingLaunchFailed:
                    text += T(
                        L"启动 Everything 失败",
                        L"Could not start Everything");
                    break;
                case win::EverythingBootstrapFailure::
                    IpcUnavailable:
                    text += T(
                        L"启动后 IPC 仍不可用",
                        L"IPC remained unavailable after startup");
                    break;
                default:
                    text += T(
                        L"请重新检测或再次获取",
                        L"Recheck or try fetching again");
                    break;
                }

                if (bootstrap.nativeError != 0) {
                    text += T(
                        L"  ·  系统错误 ",
                        L"  ·  native error ");
                    text += std::to_wstring(
                        bootstrap.nativeError);
                }
            } else if (ipc.ambiguousNamedInstances) {
                text += T(
                    L"检测到多个 Everything 命名实例，无法安全自动选择",
                    L"Multiple named Everything instances were found; automatic selection is ambiguous");
            } else {
                text += T(
                    L"未检测到可用的 Everything IPC",
                    L"No usable Everything IPC was detected");
            }

            text += L"\r\n    ↳ ";
            text += T(
                L"可重新检测已有标准版，或由 ALTRun Next 获取并启动官方标准便携版；应用搜索回退仍有效。",
                L"Recheck an existing standard copy, or let ALTRun Next fetch and start the official standard portable build. Application-search fallback remains active.");

            if (!bootstrap.executablePath.empty()) {
                text += L"\r\n    ↳ ";
                text += T(
                    L"检测到：",
                    L"Detected: ");
                text +=
                    bootstrap.executablePath
                        .wstring();
            }
        }
    }

    const bool providerPageVisible =
        page_ == Page::Providers;

    ShowWindow(
        providerGetEverything_,
        providerPageVisible &&
                showGetEverything
            ? SW_SHOW
            : SW_HIDE);
    ShowWindow(
        providerRecheckEverything_,
        providerPageVisible &&
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

void SettingsWindow::RefreshCommands() {
    RefreshCommandList(editingCommandId_);
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
        navAppearance_,
        label(Page::Appearance, L"外观", L"Appearance").c_str());
    SetWindowTextW(
        navProviders_,
        label(Page::Providers, L"搜索来源", L"Search sources").c_str());
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
            T(L"选择启动器样式、界面语言以及是否显示搜索结果图标。",
              L"Choose launcher style, interface language and whether result icons are shown."));
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
    setVisible(hotkeyControls_, page == Page::Hotkeys);
    setVisible(diagnosticsControls_, page == Page::Diagnostics);
    setVisible(legacyHotkeyControls_, false);
    setVisible(appearanceControls_, page == Page::Appearance);
    setVisible(providerControls_, page == Page::Providers);
    setVisible(dataControls_, page == Page::Data);
    setVisible(aboutControls_, page == Page::About);

    if (page == Page::Commands) {
        RefreshCommandList(editingCommandId_);
    } else if (
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

    const bool showResultIcons =
        SendMessageW(
            showResultIcons_,
            BM_GETCHECK,
            0,
            0) == BST_CHECKED;

    if (style != app_.SettingsData().uiStyle) {
        app_.SetUiStyle(style);
    }

    if (language != app_.SettingsData().language) {
        app_.SetLanguage(language);
    }

    if (showResultIcons !=
        app_.SettingsData()
            .showResultIcons) {
        if (!app_.SetShowResultIcons(
                showResultIcons)) {
            MessageBoxW(
                hwnd_,
                T(L"无法保存搜索结果图标设置。",
                  L"Unable to save the result-icon setting."),
                L"ALTRun Next",
                MB_OK | MB_ICONERROR);
            RefreshFromSettings();
        }
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

    std::array<HWND, 7> nav{
        navGeneral_,
        navHotkeys_,
        navAppearance_,
        navProviders_,
        navData_,
        navDiagnostics_,
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

        const int labelWidth = Scale(132);
        const int fieldX =
            editorX + labelWidth;
        const int fieldWidth =
            std::max(
                Scale(210),
                editorWidth - labelWidth);
        const int browseWidth = Scale(44);
        const int rowHeight = Scale(44);
        int y = top + Scale(42);

        auto placeField =
            [&](HWND label,
                HWND control,
                HWND browse = nullptr) {
                MoveWindow(
                    label,
                    editorX,
                    y + Scale(6),
                    labelWidth - Scale(12),
                    Scale(24),
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
                    Scale(34),
                    TRUE);

                if (browse) {
                    MoveWindow(
                        browse,
                        fieldX + width + Scale(8),
                        y,
                        browseWidth,
                        Scale(34),
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

        const int optionGap = Scale(8);
        const int enabledWidth =
            std::clamp(
                fieldWidth * 24 / 100,
                Scale(66),
                Scale(96));
        const int pinnedWidth =
            std::clamp(
                fieldWidth * 22 / 100,
                Scale(62),
                Scale(88));
        const int adminWidth =
            std::max(
                Scale(72),
                fieldWidth -
                    enabledWidth -
                    pinnedWidth -
                    optionGap * 2);

        MoveWindow(
            commandEnabled_,
            fieldX,
            y + Scale(3),
            enabledWidth,
            Scale(30),
            TRUE);

        MoveWindow(
            commandAdmin_,
            fieldX +
                enabledWidth +
                optionGap,
            y + Scale(3),
            adminWidth,
            Scale(30),
            TRUE);

        MoveWindow(
            commandPinned_,
            fieldX +
                enabledWidth +
                optionGap +
                adminWidth +
                optionGap,
            y + Scale(3),
            pinnedWidth,
            Scale(30),
            TRUE);

        y += Scale(44);

        const int actionGap = Scale(8);
        const int actionWidth =
            std::max(
                1,
                (fieldWidth -
                 actionGap * 3) / 4);

        std::array<HWND, 4> actions{
            commandTest_,
            commandDelete_,
            commandCancel_,
            commandSave_,
        };

        for (std::size_t i = 0;
             i < actions.size();
             ++i) {
            MoveWindow(
                actions[i],
                fieldX +
                    static_cast<int>(i) *
                        (actionWidth +
                         actionGap),
                y,
                actionWidth,
                Scale(34),
                TRUE);
        }

        MoveWindow(
            commandStatus_,
            fieldX,
            y + Scale(44),
            fieldWidth,
            Scale(30),
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
            Scale(
                settings_layout::
                    kToggleRowLogical);

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
            Scale(
                settings_layout::
                    kToggleRowLogical);

        const int searchRowX =
            metrics.search.left +
            Scale(1);

        const int searchRowWidth =
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
            Scale(
                settings_layout::
                    kToggleRowLogical *
                    4);

        MoveWindow(
            numericQuickLaunchOrderLabel_,
            metrics.search.left +
                Scale(18),
            orderTop +
                Scale(15),
            Scale(105),
            Scale(24),
            TRUE);

        MoveWindow(
            numericQuickLaunchOrder_,
            metrics.search.right -
                Scale(150),
            orderTop +
                Scale(10),
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

    if (page_ == Page::Hotkeys) {
        const int top =
            Scale(138);
        const int listWidth =
            std::clamp(
                Scale(270),
                Scale(220),
                std::max(
                    Scale(220),
                    contentWidth / 3));
        const int gap =
            Scale(28);
        const int editorX =
            contentLeft +
            listWidth +
            gap;
        const int editorWidth =
            std::max(
                Scale(320),
                contentRight -
                    editorX);

        MoveWindow(
            hotkeyActionList_,
            contentLeft,
            top,
            listWidth,
            Scale(360),
            TRUE);

        MoveWindow(
            hotkeyEditorTitle_,
            editorX,
            top,
            editorWidth,
            Scale(32),
            TRUE);

        MoveWindow(
            hotkeyEditorDescription_,
            editorX,
            top + Scale(42),
            editorWidth,
            Scale(56),
            TRUE);

        MoveWindow(
            hotkeyScope_,
            editorX,
            top + Scale(108),
            editorWidth,
            Scale(26),
            TRUE);

        MoveWindow(
            hotkeyEnabled_,
            editorX,
            top + Scale(148),
            Scale(210),
            Scale(28),
            TRUE);

        MoveWindow(
            hotkeyCapture_,
            editorX,
            top + Scale(192),
            std::min(
                editorWidth,
                Scale(300)),
            Scale(38),
            TRUE);

        MoveWindow(
            hotkeyResetCurrent_,
            editorX +
                std::min(
                    editorWidth,
                    Scale(300)) +
                Scale(10),
            top + Scale(192),
            std::max(
                Scale(120),
                editorWidth -
                    std::min(
                        editorWidth,
                        Scale(300)) -
                    Scale(10)),
            Scale(38),
            TRUE);

        MoveWindow(
            hotkeyPageStatus_,
            editorX,
            top + Scale(244),
            editorWidth,
            Scale(70),
            TRUE);

        MoveWindow(
            hotkeyResetAll_,
            editorX,
            top + Scale(330),
            std::min(
                editorWidth,
                Scale(240)),
            Scale(38),
            TRUE);

        MoveWindow(
            hotkeyPageNote_,
            contentLeft,
            top + Scale(390),
            contentWidth,
            Scale(70),
            TRUE);
    }

    if (page_ == Page::Diagnostics) {
        const int x = contentLeft;
        const int width =
            std::min(contentWidth, Scale(760));
        const int y = Scale(138);
        const int gap = Scale(20);
        const int columnWidth =
            std::max(
                1,
                (width - gap) / 2);

        MoveWindow(
            diagnosticsMemoryTitle_,
            x, y,
            columnWidth, Scale(24), TRUE);
        MoveWindow(
            diagnosticsMemoryStatus_,
            x, y + Scale(28),
            columnWidth, Scale(78), TRUE);

        MoveWindow(
            diagnosticsSearchTitle_,
            x + columnWidth + gap, y,
            columnWidth, Scale(24), TRUE);
        MoveWindow(
            diagnosticsSearchStatus_,
            x + columnWidth + gap,
            y + Scale(28),
            columnWidth, Scale(92), TRUE);

        MoveWindow(
            actionsWindowsTitle_,
            x, y + Scale(126),
            width, Scale(24), TRUE);
        MoveWindow(
            actionsWindowsStatus_,
            x, y + Scale(154),
            width, Scale(142), TRUE);

        MoveWindow(
            actionsClipboardTitle_,
            x, y + Scale(306),
            width, Scale(24), TRUE);
        MoveWindow(
            actionsClipboardStatus_,
            x, y + Scale(334),
            width, Scale(58), TRUE);

        MoveWindow(
            actionsWebTitle_,
            x, y + Scale(402),
            width, Scale(24), TRUE);
        MoveWindow(
            actionsWebStatus_,
            x, y + Scale(430),
            width, Scale(58), TRUE);

        MoveWindow(
            actionsNote_,
            x, y + Scale(500),
            width, Scale(60), TRUE);
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
            showResultIcons_,
            x, y + Scale(198),
            controlWidth, Scale(30), TRUE);

        MoveWindow(
            resultIconsNote_,
            x + Scale(22),
            y + Scale(232),
            controlWidth - Scale(22),
            Scale(42), TRUE);

        MoveWindow(
            appearanceNote_,
            x, y + Scale(292),
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
            Scale(
                settings_layout::
                    kToggleRowLogical);
        const int rowX =
            providerCard.left +
            Scale(1);
        const int rowWidth =
            providerCard.right -
            providerCard.left -
            Scale(2);

        std::array<HWND, 5> rows{
            providerStartMenu_,
            providerPackaged_,
            providerAppPaths_,
            providerPath_,
            providerEverything_,
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
                Scale(22),
            controlWidth,
            Scale(126),
            TRUE);

        MoveWindow(
            providerGetEverything_,
            x,
            providerCard.bottom +
                Scale(156),
            Scale(202),
            Scale(34),
            TRUE);

        MoveWindow(
            providerRecheckEverything_,
            x + Scale(214),
            providerCard.bottom +
                Scale(156),
            Scale(120),
            Scale(34),
            TRUE);

        MoveWindow(
            providerNote_,
            x,
            providerCard.bottom +
                Scale(202),
            controlWidth,
            Scale(66),
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
    const int cardWidth =
        std::min(
            contentWidth,
            Scale(590));

    return {
        contentLeft,
        Scale(170),
        contentLeft + cardWidth,
        Scale(
            170 +
            settings_layout::
                kToggleRowLogical * 5),
    };
}


void SettingsWindow::DrawNavigationButton(
    const DRAWITEMSTRUCT& item) {

    RECT rect = item.rcItem;

    bool selected = false;

    switch (item.CtlID) {
    case kIdNavCommands:
        selected =
            page_ == Page::Commands;
        break;
    case kIdNavGeneral:
        selected =
            page_ == Page::General;
        break;
    case kIdNavHotkeys:
        selected =
            page_ == Page::Hotkeys;
        break;
    case kIdNavDiagnostics:
        selected =
            page_ == Page::Diagnostics;
        break;
    case kIdNavAppearance:
        selected =
            page_ == Page::Appearance;
        break;
    case kIdNavProviders:
        selected =
            page_ == Page::Providers;
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

    const COLORREF background =
        pressed
            ? kCardPressed
            : selected
                ? RGB(232, 241, 250)
                : kSidebarBackground;

    HBRUSH fill =
        CreateSolidBrush(background);
    FillRect(
        item.hDC,
        &rect,
        fill);
    DeleteObject(fill);

    if (selected) {
        RECT accent{
            rect.left,
            rect.top + Scale(5),
            rect.left + Scale(4),
            rect.bottom - Scale(5),
        };

        HBRUSH accentBrush =
            CreateSolidBrush(kAccent);
        FillRect(
            item.hDC,
            &accent,
            accentBrush);
        DeleteObject(accentBrush);
    }

    wchar_t textBuffer[96]{};
    GetWindowTextW(
        item.hwndItem,
        textBuffer,
        static_cast<int>(
            std::size(textBuffer)));

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
        rect.left + Scale(18),
        rect.top,
        rect.right - Scale(12),
        rect.bottom,
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

    case kIdPinyinSearch:
        title = T(
            L"启用拼音搜索",
            L"Enable Pinyin search");
        description = T(
            L"使用全拼、首字母和混合拼音匹配中文；关闭后不会加载 cpp-pinyin，可减少不需要的内存占用。",
            L"Match Chinese with full, initial and hybrid Pinyin; when disabled, cpp-pinyin stays unloaded to avoid unnecessary memory use.");
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

    case kIdProviderEverything:
        title = T(
            L"Everything 文件与文件夹",
            L"Everything files & folders");
        description = T(
            L"通过正在运行的标准版 Everything IPC 实时搜索文件和文件夹；Lite 版没有 IPC。",
            L"Search files and folders live through a running standard Everything IPC instance; Everything Lite has no IPC.");
        break;

    default:
        break;
    }

    SetBkMode(item.hDC, TRANSPARENT);

    RECT titleRect{
        box.right + Scale(14),
        rect.top + Scale(6),
        rect.right - Scale(16),
        rect.top + Scale(27),
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
        rect.top + Scale(29),
        titleRect.right,
        rect.bottom - Scale(6),
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
        id == kIdProviderEverything;

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

        case kIdShowResultIcons:
            if (notify == BN_CLICKED) {
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
            (item->CtlID == kIdNavGeneral ||
             item->CtlID == kIdNavHotkeys ||
             item->CtlID == kIdNavDiagnostics ||
             item->CtlID == kIdNavAppearance ||
             item->CtlID == kIdNavProviders ||
             item->CtlID == kIdNavData ||
             item->CtlID == kIdNavAbout)) {
            DrawNavigationButton(*item);
            return TRUE;
        }

        if (item &&
            (item->CtlID == kIdStartWithWindows ||
             item->CtlID == kIdShowOnStartup ||
             item->CtlID == kIdHideAfterLaunch ||
             item->CtlID == kIdClearQueryOnShow ||
             item->CtlID == kIdHideOnFocusLost ||
             item->CtlID == kIdShowTrayIcon ||
             item->CtlID == kIdPinyinSearch ||
             item->CtlID == kIdWildcardMatching ||
             item->CtlID == kIdNumericQuickLaunch ||
             item->CtlID == kIdExecuteSingleResult ||
             item->CtlID == kIdProviderStartMenu ||
             item->CtlID == kIdProviderPackaged ||
             item->CtlID == kIdProviderAppPaths ||
             item->CtlID == kIdProviderPath ||
             item->CtlID == kIdProviderEverything)) {
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
            control == resultIconsNote_ ||
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
