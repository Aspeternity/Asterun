#include "LauncherWindow.hpp"

#include "../app/App.hpp"

#include <commctrl.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <uxtheme.h>

#include <algorithm>
#include <string>

namespace altrun {

namespace {

constexpr wchar_t kWindowClass[] = L"ALTRunNext.Launcher";
constexpr wchar_t kWindowTitle[] = L"ALTRun Next";

constexpr DWORD kDwmWindowCornerPreference = 33;
constexpr int kDwmDoNotRound = 1;
constexpr int kDwmRound = 2;

COLORREF MixColor(COLORREF a, COLORREF b, int numerator, int denominator) {
    const int r = GetRValue(a) + (GetRValue(b) - GetRValue(a)) * numerator / denominator;
    const int g = GetGValue(a) + (GetGValue(b) - GetGValue(a)) * numerator / denominator;
    const int bl = GetBValue(a) + (GetBValue(b) - GetBValue(a)) * numerator / denominator;
    return RGB(r, g, bl);
}

} // namespace

LauncherWindow::LauncherWindow(App& app, HINSTANCE instance)
    : app_(app), instance_(instance) {}

LauncherWindow::~LauncherWindow() {
    RemoveTrayIcon();
    if (hwnd_) UnregisterHotKey(hwnd_, kHotkeyId);
    if (normalFont_) DeleteObject(normalFont_);
    if (boldFont_) DeleteObject(boldFont_);
    if (windowBrush_) DeleteObject(windowBrush_);
    if (controlBrush_) DeleteObject(controlBrush_);
}

bool LauncherWindow::Create() {
    INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_STANDARD_CLASSES};
    InitCommonControlsEx(&controls);

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.hInstance = instance_;
    wc.lpfnWndProc = WindowProc;
    wc.lpszClassName = kWindowClass;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    wc.hbrBackground = nullptr;

    if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return false;
    }

    hwnd_ = CreateWindowExW(
        WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
        kWindowClass,
        kWindowTitle,
        WS_POPUP | WS_BORDER,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        widthLogical_,
        320,
        nullptr,
        nullptr,
        instance_,
        this);

    if (!hwnd_) return false;

    dpi_ = GetDpiForWindow(hwnd_);
    CreateChildren();
    ApplyAppearance();
    ApplyLanguage();
    AddTrayIcon();

    if (!RegisterHotKey(hwnd_, kHotkeyId, MOD_ALT | MOD_NOREPEAT, VK_SPACE)) {
        MessageBoxW(
            nullptr,
            app_.Text(TextId::HotkeyBusy).data(),
            L"ALTRun Next",
            MB_ICONWARNING | MB_OK);
    }

    RefreshResults();
    ShowWindow(hwnd_, SW_HIDE);
    return true;
}

void LauncherWindow::CreateChildren() {
    edit_ = CreateWindowExW(
        WS_EX_CLIENTEDGE,
        L"EDIT",
        L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
        0, 0, 0, 0,
        hwnd_,
        reinterpret_cast<HMENU>(1001),
        instance_,
        nullptr);

    hint_ = CreateWindowExW(
        0,
        L"STATIC",
        L"",
        WS_CHILD | WS_VISIBLE | SS_RIGHT | SS_CENTERIMAGE | SS_NOPREFIX,
        0, 0, 0, 0,
        hwnd_,
        reinterpret_cast<HMENU>(1004),
        instance_,
        nullptr);

    list_ = CreateWindowExW(
        WS_EX_CLIENTEDGE,
        L"LISTBOX",
        L"",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOTIFY | LBS_OWNERDRAWFIXED | LBS_NOINTEGRALHEIGHT,
        0, 0, 0, 0,
        hwnd_,
        reinterpret_cast<HMENU>(1002),
        instance_,
        nullptr);

    preview_ = CreateWindowExW(
        WS_EX_CLIENTEDGE,
        L"STATIC",
        L"",
        WS_CHILD | WS_VISIBLE | SS_LEFT | SS_CENTERIMAGE | SS_PATHELLIPSIS | SS_NOPREFIX,
        0, 0, 0, 0,
        hwnd_,
        reinterpret_cast<HMENU>(1003),
        instance_,
        nullptr);

    SetWindowLongPtrW(edit_, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    oldEditProc_ = reinterpret_cast<WNDPROC>(
        SetWindowLongPtrW(edit_, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(EditProc)));
}

LauncherWindow::ThemePalette LauncherWindow::CurrentPalette() const {
    if (app_.SettingsData().uiStyle == UiStyle::ModernCompact) {
        return {
            RGB(246, 247, 249),
            RGB(255, 255, 255),
            RGB(31, 41, 55),
            RGB(107, 114, 128),
            RGB(37, 99, 235),
            RGB(229, 239, 255),
            RGB(15, 23, 42),
            RGB(229, 231, 235),
            RGB(205, 210, 218),
        };
    }

    return {
        RGB(226, 231, 236),
        RGB(255, 255, 255),
        RGB(0, 0, 0),
        RGB(128, 128, 128),
        RGB(0, 0, 0),
        RGB(49, 106, 197),
        RGB(255, 255, 255),
        RGB(214, 214, 214),
        RGB(128, 136, 145),
    };
}

void LauncherWindow::RecreateBrushes() {
    if (windowBrush_) {
        DeleteObject(windowBrush_);
        windowBrush_ = nullptr;
    }
    if (controlBrush_) {
        DeleteObject(controlBrush_);
        controlBrush_ = nullptr;
    }

    const auto palette = CurrentPalette();
    windowBrush_ = CreateSolidBrush(palette.windowBackground);
    controlBrush_ = CreateSolidBrush(palette.controlBackground);
}

void LauncherWindow::ApplyFonts() {
    if (normalFont_) {
        DeleteObject(normalFont_);
        normalFont_ = nullptr;
    }
    if (boldFont_) {
        DeleteObject(boldFont_);
        boldFont_ = nullptr;
    }

    const bool modern = app_.SettingsData().uiStyle == UiStyle::ModernCompact;
    const bool zh = app_.SettingsData().language == Language::ZhCN;
    const int pointSize = modern ? 10 : 9;
    const int normalHeight = -MulDiv(pointSize, static_cast<int>(dpi_), 72);

    const wchar_t* face = nullptr;
    if (modern) {
        face = zh ? L"Microsoft YaHei UI" : L"Segoe UI";
    } else {
        face = zh ? L"SimSun" : L"Tahoma";
    }

    normalFont_ = CreateFontW(
        normalHeight, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE, face);

    boldFont_ = CreateFontW(
        normalHeight, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE, face);

    SendMessageW(edit_, WM_SETFONT, reinterpret_cast<WPARAM>(normalFont_), TRUE);
    SendMessageW(hint_, WM_SETFONT, reinterpret_cast<WPARAM>(normalFont_), TRUE);
    SendMessageW(list_, WM_SETFONT, reinterpret_cast<WPARAM>(normalFont_), TRUE);
    SendMessageW(preview_, WM_SETFONT, reinterpret_cast<WPARAM>(normalFont_), TRUE);
    SendMessageW(list_, LB_SETITEMHEIGHT, 0, DpiScale(rowHeightLogical_));
}

void LauncherWindow::UpdateControlFrames() {
    if (!edit_ || !list_ || !preview_) return;

    const bool modern = app_.SettingsData().uiStyle == UiStyle::ModernCompact;

    auto applyFrame = [modern](HWND control, bool preview) {
        LONG_PTR style = GetWindowLongPtrW(control, GWL_EXSTYLE);
        style &= ~(WS_EX_CLIENTEDGE | WS_EX_STATICEDGE);

        if (!modern) {
            style |= WS_EX_CLIENTEDGE;
        } else if (!preview) {
            style |= WS_EX_STATICEDGE;
        }

        SetWindowLongPtrW(control, GWL_EXSTYLE, style);
        SetWindowPos(
            control,
            nullptr,
            0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    };

    applyFrame(edit_, false);
    applyFrame(list_, false);
    applyFrame(preview_, true);

    if (modern) {
        SetWindowTheme(edit_, L"Explorer", nullptr);
        SetWindowTheme(list_, L"Explorer", nullptr);
        SetWindowTheme(preview_, L"Explorer", nullptr);
    } else {
        SetWindowTheme(edit_, L"", L"");
        SetWindowTheme(list_, L"", L"");
        SetWindowTheme(preview_, L"", L"");
    }
}

void LauncherWindow::UpdateWindowChrome() {
    if (!hwnd_) return;

    const bool modern = app_.SettingsData().uiStyle == UiStyle::ModernCompact;
    const int preference = modern ? kDwmRound : kDwmDoNotRound;
    DwmSetWindowAttribute(
        hwnd_,
        kDwmWindowCornerPreference,
        &preference,
        sizeof(preference));
}

void LauncherWindow::ApplyAppearance() {
    const bool modern = app_.SettingsData().uiStyle == UiStyle::ModernCompact;

    widthLogical_ = modern ? 620 : 500;
    rowHeightLogical_ = modern ? 32 : 22;
    maxResults_ = modern ? 9 : 10;

    RecreateBrushes();
    UpdateControlFrames();
    UpdateWindowChrome();
    ApplyFonts();

    ShowWindow(hint_, modern ? SW_HIDE : SW_SHOWNA);

    Layout();
    UpdateHint();
    RefreshResults();

    InvalidateRect(hwnd_, nullptr, TRUE);
    InvalidateRect(edit_, nullptr, TRUE);
    InvalidateRect(hint_, nullptr, TRUE);
    InvalidateRect(list_, nullptr, TRUE);
    InvalidateRect(preview_, nullptr, TRUE);

    if (IsWindowVisible(hwnd_)) {
        Reposition();
    }
}

void LauncherWindow::ApplyLanguage() {
    if (!edit_) return;

    const bool modern = app_.SettingsData().uiStyle == UiStyle::ModernCompact;

    SendMessageW(
        edit_,
        EM_SETCUEBANNER,
        TRUE,
        reinterpret_cast<LPARAM>(
            modern ? app_.Text(TextId::SearchPlaceholder).data() : L""));

    ApplyFonts();
    UpdateHint();
    Layout();

    InvalidateRect(hwnd_, nullptr, TRUE);
    InvalidateRect(edit_, nullptr, TRUE);
    InvalidateRect(hint_, nullptr, TRUE);
    InvalidateRect(list_, nullptr, TRUE);
    InvalidateRect(preview_, nullptr, TRUE);
}

int LauncherWindow::DpiScale(int value) const {
    return MulDiv(value, static_cast<int>(dpi_), 96);
}

void LauncherWindow::Layout() {
    if (!hwnd_) return;

    const bool modern = app_.SettingsData().uiStyle == UiStyle::ModernCompact;
    const int margin = DpiScale(modern ? 12 : 6);
    const int inputHeight = DpiScale(modern ? 36 : 23);
    const int gap = DpiScale(modern ? 8 : 4);
    const int rowHeight = DpiScale(rowHeightLogical_);
    const int listHeight = rowHeight * static_cast<int>(maxResults_) + DpiScale(modern ? 2 : 4);
    const int previewHeight = DpiScale(modern ? 25 : 23);
    const int width = DpiScale(widthLogical_);
    const int height = margin + inputHeight + gap + listHeight + gap + previewHeight + margin;

    SetWindowPos(
        hwnd_,
        nullptr,
        0, 0,
        width,
        height,
        SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);

    if (modern) {
        MoveWindow(
            edit_,
            margin,
            margin,
            width - margin * 2,
            inputHeight,
            TRUE);

        MoveWindow(hint_, 0, 0, 0, 0, FALSE);
    } else {
        const int editWidth = DpiScale(184);
        const int hintGap = DpiScale(8);

        MoveWindow(
            edit_,
            margin,
            margin,
            editWidth,
            inputHeight,
            TRUE);

        MoveWindow(
            hint_,
            margin + editWidth + hintGap,
            margin,
            width - margin * 2 - editWidth - hintGap,
            inputHeight,
            TRUE);
    }

    MoveWindow(
        list_,
        margin,
        margin + inputHeight + gap,
        width - margin * 2,
        listHeight,
        TRUE);

    MoveWindow(
        preview_,
        margin,
        margin + inputHeight + gap + listHeight + gap,
        width - margin * 2,
        previewHeight,
        TRUE);
}

void LauncherWindow::Reposition() {
    POINT cursor{};
    GetCursorPos(&cursor);
    const HMONITOR monitor = MonitorFromPoint(cursor, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info{sizeof(info)};
    GetMonitorInfoW(monitor, &info);

    RECT rect{};
    GetWindowRect(hwnd_, &rect);
    const int width = rect.right - rect.left;
    const int height = rect.bottom - rect.top;
    const int workWidth = info.rcWork.right - info.rcWork.left;
    const int workHeight = info.rcWork.bottom - info.rcWork.top;

    const int x = info.rcWork.left + (workWidth - width) / 2;
    const int y = info.rcWork.top + std::max(DpiScale(70), (workHeight - height) / 5);
    SetWindowPos(hwnd_, HWND_TOPMOST, x, y, width, height, SWP_NOACTIVATE);
}

void LauncherWindow::PaintWindowBackground(HDC dc) {
    RECT rect{};
    GetClientRect(hwnd_, &rect);

    if (app_.SettingsData().uiStyle == UiStyle::ModernCompact) {
        FillRect(dc, &rect, windowBrush_);
        return;
    }

    const COLORREF top = RGB(248, 250, 252);
    const COLORREF bottom = RGB(207, 214, 221);
    constexpr int bands = 24;

    for (int i = 0; i < bands; ++i) {
        RECT band = rect;
        band.top = rect.top + (rect.bottom - rect.top) * i / bands;
        band.bottom = rect.top + (rect.bottom - rect.top) * (i + 1) / bands;

        HBRUSH brush = CreateSolidBrush(MixColor(top, bottom, i, bands - 1));
        FillRect(dc, &band, brush);
        DeleteObject(brush);
    }

    const auto palette = CurrentPalette();
    HBRUSH frame = CreateSolidBrush(palette.frame);
    FrameRect(dc, &rect, frame);
    DeleteObject(frame);

    RECT inner = rect;
    InflateRect(&inner, -1, -1);
    HBRUSH highlight = CreateSolidBrush(RGB(255, 255, 255));
    FrameRect(dc, &inner, highlight);
    DeleteObject(highlight);
}

void LauncherWindow::Show() {
    if (!hwnd_) return;

    ++hintCycle_;
    UpdateHint();
    Reposition();
    ShowWindow(hwnd_, SW_SHOWNORMAL);
    SetForegroundWindow(hwnd_);
    SetFocus(edit_);
    SendMessageW(edit_, EM_SETSEL, 0, -1);
    RefreshResults();
}

void LauncherWindow::Hide() {
    if (hwnd_) ShowWindow(hwnd_, SW_HIDE);
}

void LauncherWindow::UpdateHint() {
    if (!hint_) return;

    if (app_.SettingsData().uiStyle == UiStyle::ModernCompact) {
        SetWindowTextW(hint_, L"");
        return;
    }

    TextId id = TextId::ClassicHintKeyboard;
    switch (hintCycle_ % 3) {
    case 1:
        id = TextId::ClassicHintMouse;
        break;
    case 2:
        id = TextId::ClassicHintTray;
        break;
    default:
        break;
    }

    SetWindowTextW(hint_, app_.Text(id).data());
}

std::wstring LauncherWindow::CurrentQuery() const {
    const int length = GetWindowTextLengthW(edit_);
    std::wstring text(static_cast<std::size_t>(length) + 1, L'\0');
    GetWindowTextW(edit_, text.data(), length + 1);
    text.resize(static_cast<std::size_t>(length));
    return text;
}

void LauncherWindow::RefreshResults() {
    if (!list_) return;

    results_ = app_.Search(CurrentQuery(), maxResults_);
    SendMessageW(list_, WM_SETREDRAW, FALSE, 0);
    SendMessageW(list_, LB_RESETCONTENT, 0, 0);

    for (std::size_t i = 0; i < results_.size(); ++i) {
        SendMessageW(list_, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L""));
    }

    if (!results_.empty()) {
        SendMessageW(list_, LB_SETCURSEL, 0, 0);
    }

    SendMessageW(list_, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(list_, nullptr, TRUE);
    UpdatePreview();
}

void LauncherWindow::UpdatePreview() {
    if (!preview_) return;

    const LRESULT selected = SendMessageW(list_, LB_GETCURSEL, 0, 0);
    if (selected == LB_ERR || static_cast<std::size_t>(selected) >= results_.size()) {
        SetWindowTextW(preview_, L"");
        return;
    }

    const auto& command =
        app_.GetCommand(results_[static_cast<std::size_t>(selected)].commandIndex);

    std::wstring preview = command.target;
    if (!command.arguments.empty()) {
        preview += L"  ";
        preview += command.arguments;
    }

    SetWindowTextW(preview_, preview.c_str());
}

void LauncherWindow::MoveSelection(int delta) {
    if (results_.empty()) return;

    int current = static_cast<int>(SendMessageW(list_, LB_GETCURSEL, 0, 0));
    if (current == LB_ERR) current = 0;

    current = std::clamp(current + delta, 0, static_cast<int>(results_.size()) - 1);
    SendMessageW(list_, LB_SETCURSEL, current, 0);
    UpdatePreview();
}

void LauncherWindow::ExecuteSelection() {
    const LRESULT selected = SendMessageW(list_, LB_GETCURSEL, 0, 0);
    if (selected == LB_ERR || static_cast<std::size_t>(selected) >= results_.size()) return;

    if (app_.ExecuteCommand(results_[static_cast<std::size_t>(selected)].commandIndex)) {
        Hide();
        SetWindowTextW(edit_, L"");
    }
}

void LauncherWindow::AddTrayIcon() {
    NOTIFYICONDATAW data{};
    data.cbSize = sizeof(data);
    data.hWnd = hwnd_;
    data.uID = 1;
    data.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    data.uCallbackMessage = kTrayMessage;
    data.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    wcscpy_s(data.szTip, L"ALTRun Next");
    Shell_NotifyIconW(NIM_ADD, &data);

    data.uVersion = NOTIFYICON_VERSION_4;
    Shell_NotifyIconW(NIM_SETVERSION, &data);
}

void LauncherWindow::RemoveTrayIcon() {
    if (!hwnd_) return;

    NOTIFYICONDATAW data{};
    data.cbSize = sizeof(data);
    data.hWnd = hwnd_;
    data.uID = 1;
    Shell_NotifyIconW(NIM_DELETE, &data);
}

void LauncherWindow::ShowTrayMenu(POINT point) {
    HMENU menu = CreatePopupMenu();
    HMENU appearanceMenu = CreatePopupMenu();
    HMENU languageMenu = CreatePopupMenu();

    const auto& settings = app_.SettingsData();

    AppendMenuW(menu, MF_STRING, kMenuShow, app_.Text(TextId::TrayShow).data());
    AppendMenuW(menu, MF_STRING, kMenuReload, app_.Text(TextId::TrayReload).data());
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);

    AppendMenuW(
        appearanceMenu,
        MF_STRING | (settings.uiStyle == UiStyle::Classic ? MF_CHECKED : MF_UNCHECKED),
        kMenuThemeClassic,
        app_.Text(TextId::TrayClassic).data());

    AppendMenuW(
        appearanceMenu,
        MF_STRING | (settings.uiStyle == UiStyle::ModernCompact ? MF_CHECKED : MF_UNCHECKED),
        kMenuThemeModern,
        app_.Text(TextId::TrayModern).data());

    AppendMenuW(
        menu,
        MF_POPUP,
        reinterpret_cast<UINT_PTR>(appearanceMenu),
        app_.Text(TextId::TrayAppearance).data());

    AppendMenuW(
        languageMenu,
        MF_STRING | (settings.language == Language::ZhCN ? MF_CHECKED : MF_UNCHECKED),
        kMenuLangZh,
        app_.Text(TextId::TrayChinese).data());

    AppendMenuW(
        languageMenu,
        MF_STRING | (settings.language == Language::EnUS ? MF_CHECKED : MF_UNCHECKED),
        kMenuLangEn,
        app_.Text(TextId::TrayEnglish).data());

    AppendMenuW(
        menu,
        MF_POPUP,
        reinterpret_cast<UINT_PTR>(languageMenu),
        app_.Text(TextId::TrayLanguage).data());

    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, kMenuExit, app_.Text(TextId::TrayExit).data());

    SetForegroundWindow(hwnd_);
    TrackPopupMenu(
        menu,
        TPM_RIGHTBUTTON | TPM_BOTTOMALIGN | TPM_LEFTALIGN,
        point.x,
        point.y,
        0,
        hwnd_,
        nullptr);

    DestroyMenu(menu);
}

LRESULT CALLBACK LauncherWindow::WindowProc(
    HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {

    LauncherWindow* self = nullptr;

    if (message == WM_NCCREATE) {
        const auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = static_cast<LauncherWindow*>(create->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        self->hwnd_ = hwnd;
    } else {
        self = reinterpret_cast<LauncherWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (self) return self->HandleMessage(message, wParam, lParam);
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

LRESULT CALLBACK LauncherWindow::EditProc(
    HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {

    auto* self = reinterpret_cast<LauncherWindow*>(
        GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    if (self) return self->HandleEditMessage(hwnd, message, wParam, lParam);
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

LRESULT LauncherWindow::HandleEditMessage(
    HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {

    if (message == WM_KEYDOWN) {
        switch (wParam) {
        case VK_DOWN:
            MoveSelection(1);
            return 0;
        case VK_UP:
            MoveSelection(-1);
            return 0;
        case VK_RETURN:
            ExecuteSelection();
            return 0;
        case VK_ESCAPE:
            Hide();
            return 0;
        default:
            break;
        }
    }

    return CallWindowProcW(oldEditProc_, hwnd, message, wParam, lParam);
}

LRESULT LauncherWindow::HandleMessage(
    UINT message, WPARAM wParam, LPARAM lParam) {

    switch (message) {
    case WM_HOTKEY:
        if (wParam == kHotkeyId) {
            if (IsWindowVisible(hwnd_)) Hide();
            else Show();
            return 0;
        }
        break;

    case WM_COMMAND:
        if (LOWORD(wParam) == 1001 && HIWORD(wParam) == EN_CHANGE) {
            RefreshResults();
            return 0;
        }
        if (LOWORD(wParam) == 1002 && HIWORD(wParam) == LBN_DBLCLK) {
            ExecuteSelection();
            return 0;
        }
        if (LOWORD(wParam) == 1002 && HIWORD(wParam) == LBN_SELCHANGE) {
            UpdatePreview();
            return 0;
        }

        switch (LOWORD(wParam)) {
        case kMenuShow:
            Show();
            return 0;
        case kMenuReload:
            app_.ReloadCommands();
            return 0;
        case kMenuThemeClassic:
            app_.SetUiStyle(UiStyle::Classic);
            return 0;
        case kMenuThemeModern:
            app_.SetUiStyle(UiStyle::ModernCompact);
            return 0;
        case kMenuLangZh:
            app_.SetLanguage(Language::ZhCN);
            return 0;
        case kMenuLangEn:
            app_.SetLanguage(Language::EnUS);
            return 0;
        case kMenuExit:
            DestroyWindow(hwnd_);
            return 0;
        default:
            break;
        }
        break;

    case WM_PAINT: {
        PAINTSTRUCT paint{};
        HDC dc = BeginPaint(hwnd_, &paint);
        PaintWindowBackground(dc);
        EndPaint(hwnd_, &paint);
        return 0;
    }

    case WM_ERASEBKGND:
        return 1;

    case WM_CTLCOLOREDIT: {
        const auto palette = CurrentPalette();
        HDC dc = reinterpret_cast<HDC>(wParam);
        SetTextColor(dc, palette.text);
        SetBkColor(dc, palette.controlBackground);
        return reinterpret_cast<LRESULT>(controlBrush_);
    }

    case WM_CTLCOLORLISTBOX: {
        const auto palette = CurrentPalette();
        HDC dc = reinterpret_cast<HDC>(wParam);
        SetTextColor(dc, palette.text);
        SetBkColor(dc, palette.controlBackground);
        return reinterpret_cast<LRESULT>(controlBrush_);
    }

    case WM_CTLCOLORSTATIC: {
        const auto palette = CurrentPalette();
        const HWND control = reinterpret_cast<HWND>(lParam);
        HDC dc = reinterpret_cast<HDC>(wParam);

        if (control == hint_) {
            SetTextColor(dc, palette.mutedText);
            SetBkMode(dc, TRANSPARENT);
            return reinterpret_cast<LRESULT>(GetStockObject(HOLLOW_BRUSH));
        }

        if (control == preview_) {
            SetTextColor(dc, palette.mutedText);

            if (app_.SettingsData().uiStyle == UiStyle::Classic) {
                SetBkColor(dc, palette.controlBackground);
                return reinterpret_cast<LRESULT>(controlBrush_);
            }

            SetBkColor(dc, palette.windowBackground);
            return reinterpret_cast<LRESULT>(windowBrush_);
        }
        break;
    }

    case WM_DRAWITEM: {
        const auto* item = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
        if (item->CtlID != 1002 ||
            item->itemID == static_cast<UINT>(-1) ||
            item->itemID >= results_.size()) {
            break;
        }

        const auto palette = CurrentPalette();
        const bool modern = app_.SettingsData().uiStyle == UiStyle::ModernCompact;
        const bool selected = (item->itemState & ODS_SELECTED) != 0;

        const COLORREF background =
            selected ? palette.selectionBackground : palette.controlBackground;

        HBRUSH brush = CreateSolidBrush(background);
        FillRect(item->hDC, &item->rcItem, brush);
        DeleteObject(brush);

        SetBkMode(item->hDC, TRANSPARENT);

        const auto& command =
            app_.GetCommand(results_[item->itemID].commandIndex);

        RECT keywordRect = item->rcItem;
        RECT titleRect = item->rcItem;

        if (modern) {
            keywordRect.left += DpiScale(12);
            keywordRect.right = keywordRect.left + DpiScale(165);
            titleRect.left = keywordRect.right + DpiScale(10);
            titleRect.right -= DpiScale(12);

            const auto oldFont = SelectObject(item->hDC, boldFont_);
            SetTextColor(
                item->hDC,
                selected ? palette.selectionText : palette.keyword);

            DrawTextW(
                item->hDC,
                command.keyword.c_str(),
                -1,
                &keywordRect,
                DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);

            SelectObject(item->hDC, normalFont_);
            SetTextColor(
                item->hDC,
                selected ? palette.selectionText : palette.text);

            DrawTextW(
                item->hDC,
                command.title.c_str(),
                -1,
                &titleRect,
                DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);

            SelectObject(item->hDC, oldFont);

            if (!selected) {
                HPEN pen = CreatePen(PS_SOLID, 1, palette.separator);
                HGDIOBJ oldPen = SelectObject(item->hDC, pen);
                MoveToEx(
                    item->hDC,
                    item->rcItem.left + DpiScale(10),
                    item->rcItem.bottom - 1,
                    nullptr);
                LineTo(
                    item->hDC,
                    item->rcItem.right - DpiScale(10),
                    item->rcItem.bottom - 1);
                SelectObject(item->hDC, oldPen);
                DeleteObject(pen);
            }
        } else {
            constexpr int classicKeywordColumn = 202;
            keywordRect.left += DpiScale(6);
            keywordRect.right = item->rcItem.left + DpiScale(classicKeywordColumn);
            titleRect.left = keywordRect.right + DpiScale(7);
            titleRect.right -= DpiScale(6);

            const std::wstring shortcut =
                std::to_wstring(static_cast<unsigned long long>(item->itemID + 1)) +
                L"  " + command.keyword;

            const auto oldFont = SelectObject(item->hDC, normalFont_);
            SetTextColor(
                item->hDC,
                selected ? palette.selectionText : palette.keyword);

            DrawTextW(
                item->hDC,
                shortcut.c_str(),
                -1,
                &keywordRect,
                DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);

            SetTextColor(
                item->hDC,
                selected ? palette.selectionText : palette.text);

            DrawTextW(
                item->hDC,
                command.title.c_str(),
                -1,
                &titleRect,
                DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);

            const COLORREF lineColor =
                selected ? RGB(117, 156, 212) : palette.separator;

            HPEN separator = CreatePen(PS_SOLID, 1, lineColor);
            HGDIOBJ oldPen = SelectObject(item->hDC, separator);
            const int x = item->rcItem.left + DpiScale(classicKeywordColumn);
            MoveToEx(item->hDC, x, item->rcItem.top, nullptr);
            LineTo(item->hDC, x, item->rcItem.bottom);
            SelectObject(item->hDC, oldPen);
            DeleteObject(separator);

            SelectObject(item->hDC, oldFont);
        }

        return TRUE;
    }

    case WM_DPICHANGED: {
        dpi_ = HIWORD(wParam);
        const auto* suggested = reinterpret_cast<RECT*>(lParam);

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

    case WM_ACTIVATE:
        if (LOWORD(wParam) == WA_INACTIVE && IsWindowVisible(hwnd_)) {
            Hide();
        }
        break;

    case kTrayMessage:
        if (LOWORD(lParam) == WM_LBUTTONDBLCLK) {
            Show();
            return 0;
        }
        if (LOWORD(lParam) == WM_RBUTTONUP ||
            LOWORD(lParam) == WM_CONTEXTMENU) {
            POINT point{};
            GetCursorPos(&point);
            ShowTrayMenu(point);
            return 0;
        }
        break;

    case WM_DESTROY:
        RemoveTrayIcon();
        UnregisterHotKey(hwnd_, kHotkeyId);
        hwnd_ = nullptr;
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hwnd_, message, wParam, lParam);
}

} // namespace altrun
