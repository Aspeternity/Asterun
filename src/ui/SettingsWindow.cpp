#include "SettingsWindow.hpp"

#include "../app/App.hpp"

#include <commctrl.h>

#include <algorithm>
#include <string>

namespace altrun {

namespace {

constexpr wchar_t kSettingsClass[] = L"ALTRunNext.Settings";
constexpr wchar_t kSettingsTitle[] = L"ALTRun Next Settings";

void SetCheck(HWND control, bool checked) {
    SendMessageW(
        control,
        BM_SETCHECK,
        checked ? BST_CHECKED : BST_UNCHECKED,
        0);
}

bool IsChecked(HWND control) {
    return SendMessageW(control, BM_GETCHECK, 0, 0) == BST_CHECKED;
}

} // namespace

SettingsWindow::SettingsWindow(App& app, HINSTANCE instance)
    : app_(app), instance_(instance) {}

SettingsWindow::~SettingsWindow() {
    if (normalFont_) DeleteObject(normalFont_);
    if (titleFont_) DeleteObject(titleFont_);
    if (appNameFont_) DeleteObject(appNameFont_);
    if (backgroundBrush_) DeleteObject(backgroundBrush_);
    if (sidebarBrush_) DeleteObject(sidebarBrush_);
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
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        900,
        620,
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
        Scale(620),
        SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);

    backgroundBrush_ = CreateSolidBrush(RGB(255, 255, 255));
    sidebarBrush_ = CreateSolidBrush(RGB(246, 247, 249));

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

HWND SettingsWindow::CreateCheckbox(
    const wchar_t* text,
    UINT id) {

    return CreateButton(
        text,
        id,
        BS_AUTOCHECKBOX | BS_FLAT);
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
    hideAfterLaunch_ =
        CreateCheckbox(L"", kIdHideAfterLaunch);
    clearQueryOnShow_ =
        CreateCheckbox(L"", kIdClearQueryOnShow);
    hideOnFocusLost_ =
        CreateCheckbox(L"", kIdHideOnFocusLost);
    showTrayIcon_ =
        CreateCheckbox(L"", kIdShowTrayIcon);

    popupMonitorLabel_ = CreateStatic(L"");

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
        hideAfterLaunch_,
        clearQueryOnShow_,
        hideOnFocusLost_,
        showTrayIcon_,
        popupMonitorLabel_,
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
        hideAfterLaunch_,
        clearQueryOnShow_,
        hideOnFocusLost_,
        showTrayIcon_,
        popupMonitorLabel_,
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
}

void SettingsWindow::ApplyLanguage() {
    if (!hwnd_) return;

    syncing_ = true;

    SetWindowTextW(
        hwnd_,
        T(L"ALTRun Next 设置", L"ALTRun Next Settings"));

    SetWindowTextW(
        hideAfterLaunch_,
        T(L"执行快捷项后自动隐藏启动器",
          L"Hide launcher after executing a command"));

    SetWindowTextW(
        clearQueryOnShow_,
        T(L"每次呼出时清空搜索内容",
          L"Clear search query whenever the launcher opens"));

    SetWindowTextW(
        hideOnFocusLost_,
        T(L"启动器失去焦点时自动隐藏",
          L"Hide launcher when it loses focus"));

    SetWindowTextW(
        showTrayIcon_,
        T(L"显示系统托盘图标",
          L"Show system tray icon"));

    SetWindowTextW(
        popupMonitorLabel_,
        T(L"启动器呼出位置", L"Launcher monitor"));

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
        T(L"版本 0.2.0-alpha.2", L"Version 0.2.0-alpha.2"));

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
    InvalidateRect(hwnd_, nullptr, TRUE);
}

void SettingsWindow::RefreshFromSettings() {
    if (!hwnd_) return;

    syncing_ = true;

    const auto& settings = app_.SettingsData();

    SetCheck(hideAfterLaunch_, settings.hideAfterLaunch);
    SetCheck(clearQueryOnShow_, settings.clearQueryOnShow);
    SetCheck(hideOnFocusLost_, settings.hideOnFocusLost);
    SetCheck(showTrayIcon_, settings.showTrayIcon);

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
}

void SettingsWindow::ApplyGeneralControls() {
    if (syncing_) return;

    int monitorIndex =
        static_cast<int>(SendMessageW(
            popupMonitor_,
            CB_GETCURSEL,
            0,
            0));

    std::string popupMonitor = "cursor";
    if (monitorIndex == 1) popupMonitor = "active";
    else if (monitorIndex == 2) popupMonitor = "primary";

    app_.SetGeneralSettings(
        IsChecked(hideAfterLaunch_),
        IsChecked(clearQueryOnShow_),
        IsChecked(hideOnFocusLost_),
        IsChecked(showTrayIcon_),
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
    const int contentWidth = std::max(Scale(320), contentRight - contentLeft);

    MoveWindow(
        pageTitle_,
        contentLeft,
        Scale(36),
        contentWidth,
        Scale(42),
        TRUE);

    MoveWindow(
        pageDescription_,
        contentLeft,
        Scale(82),
        contentWidth,
        Scale(42),
        TRUE);

    const int x = contentLeft;
    const int y = Scale(150);
    const int controlWidth = std::min(contentWidth, Scale(570));
    const int row = Scale(42);

    if (page_ == Page::General) {
        MoveWindow(
            hideAfterLaunch_,
            x, y,
            controlWidth, Scale(28), TRUE);

        MoveWindow(
            clearQueryOnShow_,
            x, y + row,
            controlWidth, Scale(28), TRUE);

        MoveWindow(
            hideOnFocusLost_,
            x, y + row * 2,
            controlWidth, Scale(28), TRUE);

        MoveWindow(
            showTrayIcon_,
            x, y + row * 3,
            controlWidth, Scale(28), TRUE);

        MoveWindow(
            popupMonitorLabel_,
            x, y + row * 4 + Scale(12),
            Scale(190), Scale(28), TRUE);

        MoveWindow(
            popupMonitor_,
            x,
            y + row * 4 + Scale(45),
            Scale(320),
            Scale(220),
            TRUE);

        MoveWindow(
            generalNote_,
            x,
            y + row * 4 + Scale(92),
            controlWidth,
            Scale(48),
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
            if (notify == BN_CLICKED) ApplyGeneralControls();
            return 0;

        case kIdPopupMonitor:
            if (notify == CBN_SELCHANGE) ApplyGeneralControls();
            return 0;

        case kIdUiStyle:
        case kIdLanguage:
            if (notify == CBN_SELCHANGE) ApplyAppearanceControls();
            return 0;

        case kIdOpenDataFolder:
            if (notify == BN_CLICKED) app_.OpenDataFolder();
            return 0;

        case kIdOpenGitHub:
            if (notify == BN_CLICKED) app_.OpenProjectPage();
            return 0;

        default:
            break;
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
                RGB(229, 231, 235));

        HGDIOBJ oldPen =
            SelectObject(dc, separator);

        const int x = Scale(kSidebarWidthLogical);
        MoveToEx(dc, x, client.top, nullptr);
        LineTo(dc, x, client.bottom);

        SelectObject(dc, oldPen);
        DeleteObject(separator);

        EndPaint(hwnd_, &paint);
        return 0;
    }

    case WM_CTLCOLORSTATIC: {
        HDC dc = reinterpret_cast<HDC>(wParam);
        HWND control = reinterpret_cast<HWND>(lParam);

        SetBkMode(dc, TRANSPARENT);

        if (control == pageDescription_ ||
            control == generalNote_ ||
            control == appearanceNote_ ||
            control == aboutVersion_ ||
            control == aboutDescription_ ||
            control == dataPathLabel_ ||
            control == dataPath_) {
            SetTextColor(dc, RGB(100, 107, 116));
        } else {
            SetTextColor(dc, RGB(31, 41, 55));
        }

        return reinterpret_cast<LRESULT>(
            GetStockObject(HOLLOW_BRUSH));
    }

    case WM_SIZE:
        Layout();
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
        return 0;
    }

    case WM_GETMINMAXINFO: {
        auto* info =
            reinterpret_cast<MINMAXINFO*>(lParam);

        info->ptMinTrackSize.x = Scale(760);
        info->ptMinTrackSize.y = Scale(520);
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
