#include "SettingsWindow.hpp"

#include "../app/App.hpp"

#include <commctrl.h>

#include <algorithm>
#include <array>
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
    return MulDiv(value, static_cast<int>(dpi_), 96);
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
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        900,
        640,
        nullptr,
        nullptr,
        instance_,
        this);

    if (!hwnd_) return false;

    dpi_ = GetDpiForWindow(hwnd_);

    SetWindowPos(
        hwnd_,
        nullptr,
        0, 0,
        Scale(900),
        Scale(640),
        SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);

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
        reinterpret_cast<HMENU>(static_cast<UINT_PTR>(id)),
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

void SettingsWindow::CreateControls() {
    navGeneral_ = CreateButton(L"", kIdNavGeneral);
    navAppearance_ = CreateButton(L"", kIdNavAppearance);
    navAbout_ = CreateButton(L"", kIdNavAbout);

    pageTitle_ = CreateStatic(L"", SS_LEFT);
    pageDescription_ = CreateStatic(
        L"",
        SS_LEFT | SS_NOPREFIX);

    CreateGeneralPage();
    CreateAppearancePage();
    CreateAboutPage();
}

void SettingsWindow::CreateGeneralPage() {
    generalBehaviorTitle_ = CreateStatic(L"");

    hideAfterLaunch_ =
        CreateCheckboxRow(L"", kIdHideAfterLaunch);
    clearQueryOnShow_ =
        CreateCheckboxRow(L"", kIdClearQueryOnShow);
    hideOnFocusLost_ =
        CreateCheckboxRow(L"", kIdHideOnFocusLost);
    showTrayIcon_ =
        CreateCheckboxRow(L"", kIdShowTrayIcon);

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
        hideAfterLaunch_,
        clearQueryOnShow_,
        hideOnFocusLost_,
        showTrayIcon_,
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

    std::vector<HWND> controls{
        navGeneral_,
        navAppearance_,
        navAbout_,
        pageDescription_,
        popupMonitorLabel_,
        popupMonitorDescription_,
        popupMonitor_,
        generalNote_,
        uiStyleLabel_,
        uiStyle_,
        languageLabel_,
        language_,
        appearanceNote_,
        aboutVersion_,
        aboutDescription_,
        dataPathLabel_,
        dataPath_,
        openDataFolder_,
        openGitHub_,
    };

    for (HWND control : controls) {
        if (control) {
            SendMessageW(
                control,
                WM_SETFONT,
                reinterpret_cast<WPARAM>(normalFont_),
                TRUE);
        }
    }

    for (HWND control : std::array<HWND, 2>{
             generalBehaviorTitle_,
             popupSectionTitle_}) {
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

    for (HWND control : std::array<HWND, 4>{
             hideAfterLaunch_,
             clearQueryOnShow_,
             hideOnFocusLost_,
             showTrayIcon_}) {
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

    syncing_ = true;

    SetWindowTextW(
        hwnd_,
        T(L"ALTRun Next 设置", L"ALTRun Next Settings"));

    SetWindowTextW(
        generalBehaviorTitle_,
        T(L"启动器行为", L"Launcher behavior"));

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
        T(L"开机启动和自定义全局热键将在后续 alpha 中接入。",
          L"Start-with-Windows and custom global hotkeys will be wired in a later alpha."));

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
        aboutVersion_,
        T(L"版本 0.2.0-alpha.2.1", L"Version 0.2.0-alpha.2.1"));

    SetWindowTextW(
        aboutDescription_,
        T(L"轻量级、键盘优先的 Windows 快捷启动器。\nClassic 主界面保持老 ALTRun 的操作感，设置中心使用独立的现代界面。",
          L"A lightweight, keyboard-first Windows launcher.\nThe Classic launcher preserves the old ALTRun feel while Settings uses a separate modern shell."));

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

    syncing_ = false;

    RedrawWindow(
        hwnd_,
        nullptr,
        nullptr,
        RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW);
}

void SettingsWindow::RefreshFromSettings() {
    if (!hwnd_) return;

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

    for (HWND control : std::array<HWND, 4>{
             hideAfterLaunch_,
             clearQueryOnShow_,
             hideOnFocusLost_,
             showTrayIcon_}) {
        if (control) InvalidateRect(control, nullptr, TRUE);
    }

    syncing_ = false;
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
        navGeneral_,
        label(Page::General, L"常规", L"General").c_str());

    SetWindowTextW(
        navAppearance_,
        label(Page::Appearance, L"外观", L"Appearance").c_str());

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
            T(L"控制启动器的日常行为和呼出位置。",
              L"Control everyday launcher behavior and placement."));
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

void SettingsWindow::ShowPage(Page page) {
    page_ = page;

    const auto setVisible = [](const std::vector<HWND>& controls, bool visible) {
        for (HWND control : controls) {
            ShowWindow(control, visible ? SW_SHOW : SW_HIDE);
        }
    };

    setVisible(generalControls_, page == Page::General);
    setVisible(appearanceControls_, page == Page::Appearance);
    setVisible(aboutControls_, page == Page::About);

    UpdateNavLabels();
    UpdatePageHeader();
    Layout();

    // Static controls use opaque backgrounds now, and the full redraw below
    // also guarantees that switching pages never leaves stale glyphs behind.
    RedrawWindow(
        hwnd_,
        nullptr,
        nullptr,
        RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW);
}

bool SettingsWindow::ToggleChecked(UINT id) const {
    const auto& settings = app_.SettingsData();

    switch (id) {
    case kIdHideAfterLaunch:
        return settings.hideAfterLaunch;
    case kIdClearQueryOnShow:
        return settings.clearQueryOnShow;
    case kIdHideOnFocusLost:
        return settings.hideOnFocusLost;
    case kIdShowTrayIcon:
        return settings.showTrayIcon;
    default:
        return false;
    }
}

void SettingsWindow::ToggleGeneralSetting(UINT id) {
    if (syncing_) return;

    const auto settings = app_.SettingsData();

    bool hideAfterLaunch = settings.hideAfterLaunch;
    bool clearQueryOnShow = settings.clearQueryOnShow;
    bool hideOnFocusLost = settings.hideOnFocusLost;
    bool showTrayIcon = settings.showTrayIcon;

    switch (id) {
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

RECT SettingsWindow::BehaviorCardRect() const {
    RECT client{};
    GetClientRect(hwnd_, &client);

    const int sidebar = Scale(kSidebarWidthLogical);
    const int contentLeft = sidebar + Scale(42);
    const int contentRight = client.right - Scale(42);
    const int contentWidth =
        std::max(Scale(320), contentRight - contentLeft);
    const int cardWidth =
        std::min(contentWidth, Scale(590));

    return {
        contentLeft,
        Scale(176),
        contentLeft + cardWidth,
        Scale(176 + 58 * 4),
    };
}

RECT SettingsWindow::MonitorCardRect() const {
    RECT client{};
    GetClientRect(hwnd_, &client);

    const int sidebar = Scale(kSidebarWidthLogical);
    const int contentLeft = sidebar + Scale(42);
    const int contentRight = client.right - Scale(42);
    const int contentWidth =
        std::max(Scale(320), contentRight - contentLeft);
    const int cardWidth =
        std::min(contentWidth, Scale(590));

    return {
        contentLeft,
        Scale(468),
        contentLeft + cardWidth,
        Scale(550),
    };
}

void SettingsWindow::Layout() {
    if (!hwnd_) return;

    RECT client{};
    GetClientRect(hwnd_, &client);

    const int sidebar = Scale(kSidebarWidthLogical);
    const int sidebarMargin = Scale(18);
    const int navWidth = sidebar - sidebarMargin * 2;
    const int navHeight = Scale(42);
    const int navGap = Scale(8);

    MoveWindow(
        navGeneral_,
        sidebarMargin,
        Scale(82),
        navWidth,
        navHeight,
        TRUE);

    MoveWindow(
        navAppearance_,
        sidebarMargin,
        Scale(82) + navHeight + navGap,
        navWidth,
        navHeight,
        TRUE);

    MoveWindow(
        navAbout_,
        sidebarMargin,
        Scale(82) + (navHeight + navGap) * 2,
        navWidth,
        navHeight,
        TRUE);

    const int contentLeft = sidebar + Scale(42);
    const int contentRight = client.right - Scale(42);
    const int contentWidth =
        std::max(Scale(320), contentRight - contentLeft);

    MoveWindow(
        pageTitle_,
        contentLeft,
        Scale(34),
        contentWidth,
        Scale(42),
        TRUE);

    MoveWindow(
        pageDescription_,
        contentLeft,
        Scale(82),
        contentWidth,
        Scale(40),
        TRUE);

    const int x = contentLeft;
    const int y = Scale(150);
    const int controlWidth =
        std::min(contentWidth, Scale(590));

    if (page_ == Page::General) {
        MoveWindow(
            generalBehaviorTitle_,
            x,
            Scale(142),
            controlWidth,
            Scale(28),
            TRUE);

        const RECT behavior = BehaviorCardRect();
        const int rowHeight = Scale(58);
        const int rowX = behavior.left + Scale(1);
        const int rowWidth =
            behavior.right - behavior.left - Scale(2);

        std::array<HWND, 4> rows{
            hideAfterLaunch_,
            clearQueryOnShow_,
            hideOnFocusLost_,
            showTrayIcon_,
        };

        for (std::size_t i = 0; i < rows.size(); ++i) {
            MoveWindow(
                rows[i],
                rowX,
                behavior.top + Scale(1) +
                    static_cast<int>(i) * rowHeight,
                rowWidth,
                rowHeight,
                TRUE);
        }

        MoveWindow(
            popupSectionTitle_,
            x,
            Scale(434),
            controlWidth,
            Scale(28),
            TRUE);

        const RECT monitor = MonitorCardRect();
        const int monitorWidth =
            monitor.right - monitor.left;

        MoveWindow(
            popupMonitorLabel_,
            monitor.left + Scale(18),
            monitor.top + Scale(14),
            Scale(220),
            Scale(24),
            TRUE);

        MoveWindow(
            popupMonitorDescription_,
            monitor.left + Scale(18),
            monitor.top + Scale(40),
            std::max(
                Scale(180),
                monitorWidth - Scale(330)),
            Scale(30),
            TRUE);

        MoveWindow(
            popupMonitor_,
            monitor.right - Scale(278),
            monitor.top + Scale(23),
            Scale(250),
            Scale(220),
            TRUE);

        MoveWindow(
            generalNote_,
            x,
            Scale(568),
            controlWidth,
            Scale(36),
            TRUE);
    }

    if (page_ == Page::Appearance) {
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

    if (page_ == Page::About) {
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
            x + Scale(196), y + Scale(270),
            Scale(120), Scale(38), TRUE);
    }
}

void SettingsWindow::DrawGeneralToggle(
    const DRAWITEMSTRUCT& item) {

    RECT rect = item.rcItem;

    const COLORREF rowBackground =
        (item.itemState & ODS_SELECTED)
            ? kCardPressed
            : kCardBackground;

    HBRUSH rowBrush = CreateSolidBrush(rowBackground);
    FillRect(item.hDC, &rect, rowBrush);
    DeleteObject(rowBrush);

    const UINT id = static_cast<UINT>(item.CtlID);
    const bool checked = ToggleChecked(id);

    const int boxSize = Scale(20);
    const int boxLeft = rect.left + Scale(18);
    const int boxTop =
        rect.top + (rect.bottom - rect.top - boxSize) / 2;

    RECT box{
        boxLeft,
        boxTop,
        boxLeft + boxSize,
        boxTop + boxSize,
    };

    HBRUSH boxBrush =
        CreateSolidBrush(
            checked ? kAccent : RGB(255, 255, 255));
    HPEN boxPen =
        CreatePen(
            PS_SOLID,
            std::max(1, Scale(1)),
            checked ? kAccent : RGB(166, 174, 184));

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

        oldPen = SelectObject(item.hDC, checkPen);

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

        SelectObject(item.hDC, oldPen);
        DeleteObject(checkPen);
    }

    const wchar_t* title = L"";
    const wchar_t* description = L"";

    switch (id) {
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
        SelectObject(item.hDC, sectionFont_);

    SetTextColor(item.hDC, kText);
    DrawTextW(
        item.hDC,
        title,
        -1,
        &titleRect,
        DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);

    RECT descriptionRect{
        titleRect.left,
        rect.top + Scale(31),
        titleRect.right,
        rect.bottom - Scale(7),
    };

    SelectObject(item.hDC, normalFont_);
    SetTextColor(item.hDC, kMuted);
    DrawTextW(
        item.hDC,
        description,
        -1,
        &descriptionRect,
        DT_LEFT | DT_SINGLELINE | DT_VCENTER |
            DT_END_ELLIPSIS | DT_NOPREFIX);

    SelectObject(item.hDC, oldFont);

    if (id != kIdShowTrayIcon) {
        HPEN separator =
            CreatePen(
                PS_SOLID,
                1,
                kBorder);

        oldPen = SelectObject(item.hDC, separator);

        MoveToEx(
            item.hDC,
            rect.left + Scale(52),
            rect.bottom - 1,
            nullptr);

        LineTo(
            item.hDC,
            rect.right - Scale(14),
            rect.bottom - 1);

        SelectObject(item.hDC, oldPen);
        DeleteObject(separator);
    }

    if (item.itemState & ODS_FOCUS) {
        RECT focus = rect;
        InflateRect(&focus, -Scale(6), -Scale(5));
        DrawFocusRect(item.hDC, &focus);
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

    const int width = rect.right - rect.left;
    const int height = rect.bottom - rect.top;

    const int workWidth =
        info.rcWork.right - info.rcWork.left;
    const int workHeight =
        info.rcWork.bottom - info.rcWork.top;

    const int x =
        info.rcWork.left + (workWidth - width) / 2;
    const int y =
        info.rcWork.top + (workHeight - height) / 2;

    SetWindowPos(
        hwnd_,
        nullptr,
        x, y,
        width, height,
        SWP_NOZORDER | SWP_NOACTIVATE);
}

void SettingsWindow::Show() {
    if (!hwnd_) return;

    RefreshFromSettings();

    if (!IsWindowVisible(hwnd_)) {
        CenterOnCurrentMonitor();
    }

    ShowWindow(hwnd_, SW_SHOWNORMAL);
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
                GetWindowLongPtrW(hwnd, GWLP_USERDATA));
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
        case kIdNavGeneral:
            if (notify == BN_CLICKED) ShowPage(Page::General);
            return 0;

        case kIdNavAppearance:
            if (notify == BN_CLICKED) ShowPage(Page::Appearance);
            return 0;

        case kIdNavAbout:
            if (notify == BN_CLICKED) ShowPage(Page::About);
            return 0;

        case kIdHideAfterLaunch:
        case kIdClearQueryOnShow:
        case kIdHideOnFocusLost:
        case kIdShowTrayIcon:
            if (notify == BN_CLICKED) {
                ToggleGeneralSetting(id);
            }
            return 0;

        case kIdPopupMonitor:
            if (notify == CBN_SELCHANGE) {
                ApplyMonitorControl();
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
            reinterpret_cast<DRAWITEMSTRUCT*>(lParam);

        if (item &&
            (item->CtlID == kIdHideAfterLaunch ||
             item->CtlID == kIdClearQueryOnShow ||
             item->CtlID == kIdHideOnFocusLost ||
             item->CtlID == kIdShowTrayIcon)) {
            DrawGeneralToggle(*item);
            return TRUE;
        }
        break;
    }

    case WM_PAINT: {
        PAINTSTRUCT paint{};
        HDC dc = BeginPaint(hwnd_, &paint);

        RECT client{};
        GetClientRect(hwnd_, &client);
        FillRect(dc, &client, backgroundBrush_);

        RECT sidebar{
            client.left,
            client.top,
            Scale(kSidebarWidthLogical),
            client.bottom
        };
        FillRect(dc, &sidebar, sidebarBrush_);

        HPEN separator =
            CreatePen(
                PS_SOLID,
                1,
                kBorder);

        HGDIOBJ oldPen =
            SelectObject(dc, separator);

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

        SelectObject(dc, oldPen);
        DeleteObject(separator);

        if (page_ == Page::General) {
            for (const RECT card : std::array<RECT, 2>{
                     BehaviorCardRect(),
                     MonitorCardRect()}) {
                HBRUSH fill =
                    CreateSolidBrush(kCardBackground);
                HPEN border =
                    CreatePen(
                        PS_SOLID,
                        1,
                        kBorder);

                HGDIOBJ previousBrush =
                    SelectObject(dc, fill);
                HGDIOBJ previousPen =
                    SelectObject(dc, border);

                RoundRect(
                    dc,
                    card.left,
                    card.top,
                    card.right,
                    card.bottom,
                    Scale(8),
                    Scale(8));

                SelectObject(dc, previousBrush);
                SelectObject(dc, previousPen);
                DeleteObject(fill);
                DeleteObject(border);
            }
        }

        EndPaint(hwnd_, &paint);
        return 0;
    }

    case WM_ERASEBKGND:
        return 1;

    case WM_CTLCOLORSTATIC: {
        HDC dc = reinterpret_cast<HDC>(wParam);
        HWND control = reinterpret_cast<HWND>(lParam);

        const bool cardStatic =
            control == popupMonitorLabel_ ||
            control == popupMonitorDescription_;

        const COLORREF background =
            cardStatic ? kCardBackground : kWindowBackground;

        SetBkMode(dc, OPAQUE);
        SetBkColor(dc, background);

        if (control == pageDescription_ ||
            control == generalNote_ ||
            control == popupMonitorDescription_ ||
            control == appearanceNote_ ||
            control == aboutVersion_ ||
            control == aboutDescription_ ||
            control == dataPathLabel_ ||
            control == dataPath_) {
            SetTextColor(dc, kMuted);
        } else {
            SetTextColor(dc, kText);
        }

        return reinterpret_cast<LRESULT>(
            cardStatic ? cardBrush_ : backgroundBrush_);
    }

    case WM_SIZE:
        Layout();
        InvalidateRect(hwnd_, nullptr, TRUE);
        return 0;

    case WM_DPICHANGED: {
        dpi_ = HIWORD(wParam);

        const auto* suggested =
            reinterpret_cast<RECT*>(lParam);

        SetWindowPos(
            hwnd_,
            nullptr,
            suggested->left,
            suggested->top,
            suggested->right - suggested->left,
            suggested->bottom - suggested->top,
            SWP_NOZORDER | SWP_NOACTIVATE);

        ApplyFonts();
        Layout();

        RedrawWindow(
            hwnd_,
            nullptr,
            nullptr,
            RDW_INVALIDATE | RDW_ERASE |
                RDW_ALLCHILDREN | RDW_UPDATENOW);
        return 0;
    }

    case WM_GETMINMAXINFO: {
        auto* info =
            reinterpret_cast<MINMAXINFO*>(lParam);

        info->ptMinTrackSize.x = Scale(760);
        info->ptMinTrackSize.y = Scale(540);
        return 0;
    }

    case WM_CLOSE:
        ShowWindow(hwnd_, SW_HIDE);
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
