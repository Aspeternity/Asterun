#include "LauncherWindow.hpp"

#include "../app/App.hpp"

#include <windowsx.h>
#include <commctrl.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <uxtheme.h>

#include <algorithm>
#include <array>
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
    if (titleFont_) DeleteObject(titleFont_);
    if (windowBrush_) DeleteObject(windowBrush_);
    if (controlBrush_) DeleteObject(controlBrush_);
    if (accentBrush_) DeleteObject(accentBrush_);
}

bool LauncherWindow::IsModern() const {
    return app_.SettingsData().uiStyle == UiStyle::ModernCompact;
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
        250,
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
        0,
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
        WS_EX_STATICEDGE,
        L"LISTBOX",
        L"",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOTIFY | LBS_OWNERDRAWFIXED | LBS_NOINTEGRALHEIGHT,
        0, 0, 0, 0,
        hwnd_,
        reinterpret_cast<HMENU>(1002),
        instance_,
        nullptr);

    preview_ = CreateWindowExW(
        0,
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
    if (IsModern()) {
        return {
            RGB(246, 247, 249),
            RGB(255, 255, 255),
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
        RGB(103, 109, 115),
        RGB(244, 246, 248),
        RGB(186, 214, 190),
        RGB(38, 41, 145),
        RGB(104, 119, 109),
        RGB(38, 41, 145),
        RGB(4, 119, 210),
        RGB(255, 255, 255),
        RGB(43, 45, 148),
        RGB(91, 97, 104),
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
    if (accentBrush_) {
        DeleteObject(accentBrush_);
        accentBrush_ = nullptr;
    }

    const auto palette = CurrentPalette();
    windowBrush_ = CreateSolidBrush(palette.windowBackground);
    controlBrush_ = CreateSolidBrush(palette.controlBackground);
    accentBrush_ = CreateSolidBrush(palette.accentBackground);
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
    if (titleFont_) {
        DeleteObject(titleFont_);
        titleFont_ = nullptr;
    }

    const bool zh = app_.SettingsData().language == Language::ZhCN;
    const int pointSize = IsModern() ? 10 : 9;
    const int normalHeight = -MulDiv(pointSize, static_cast<int>(dpi_), 72);
    const int titleHeight = -MulDiv(IsModern() ? 10 : 10, static_cast<int>(dpi_), 72);

    const wchar_t* face = nullptr;
    if (IsModern()) {
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

    titleFont_ = CreateFontW(
        titleHeight, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
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

    auto setFrame = [](HWND control, LONG_PTR edge) {
        LONG_PTR style = GetWindowLongPtrW(control, GWL_EXSTYLE);
        style &= ~(WS_EX_CLIENTEDGE | WS_EX_STATICEDGE);
        style |= edge;
        SetWindowLongPtrW(control, GWL_EXSTYLE, style);
        SetWindowPos(
            control,
            nullptr,
            0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    };

    if (IsModern()) {
        setFrame(edit_, WS_EX_STATICEDGE);
        setFrame(list_, WS_EX_STATICEDGE);
        setFrame(preview_, 0);

        SetWindowTheme(edit_, L"Explorer", nullptr);
        SetWindowTheme(list_, L"Explorer", nullptr);
        SetWindowTheme(preview_, L"Explorer", nullptr);
    } else {
        setFrame(edit_, 0);
        setFrame(list_, WS_EX_STATICEDGE);
        setFrame(preview_, 0);

        SetWindowTheme(edit_, L"", L"");
        SetWindowTheme(list_, L"", L"");
        SetWindowTheme(preview_, L"", L"");
    }
}

void LauncherWindow::UpdateWindowChrome() {
    if (!hwnd_) return;

    const int preference = IsModern() ? kDwmRound : kDwmDoNotRound;
    DwmSetWindowAttribute(
        hwnd_,
        kDwmWindowCornerPreference,
        &preference,
        sizeof(preference));

    if (IsModern()) {
        SetWindowRgn(hwnd_, nullptr, TRUE);
        return;
    }

    RECT rect{};
    GetWindowRect(hwnd_, &rect);
    const int width = rect.right - rect.left;
    const int height = rect.bottom - rect.top;
    HRGN region = CreateRoundRectRgn(
        0, 0,
        width + 1,
        height + 1,
        DpiScale(7),
        DpiScale(7));

    if (SetWindowRgn(hwnd_, region, TRUE) == 0) {
        DeleteObject(region);
    }
}

void LauncherWindow::ApplyAppearance() {
    if (IsModern()) {
        widthLogical_ = 620;
        rowHeightLogical_ = 32;
        maxResults_ = 9;
    } else {
        widthLogical_ = 420;
        rowHeightLogical_ = 16;
        maxResults_ = 10;
    }

    RecreateBrushes();
    UpdateControlFrames();
    ApplyFonts();

    ShowWindow(hint_, IsModern() ? SW_HIDE : SW_SHOWNA);

    Layout();
    UpdateWindowChrome();
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

    SendMessageW(
        edit_,
        EM_SETCUEBANNER,
        TRUE,
        reinterpret_cast<LPARAM>(
            IsModern() ? app_.Text(TextId::SearchPlaceholder).data() : L""));

    ApplyFonts();
    UpdateHint();
    UpdatePreview();
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

    int width = 0;
    int height = 0;

    if (IsModern()) {
        const int margin = DpiScale(12);
        const int inputHeight = DpiScale(36);
        const int gap = DpiScale(8);
        const int rowHeight = DpiScale(rowHeightLogical_);
        const int listHeight = rowHeight * static_cast<int>(maxResults_) + DpiScale(2);
        const int previewHeight = DpiScale(25);

        width = DpiScale(widthLogical_);
        height = margin + inputHeight + gap + listHeight + gap + previewHeight + margin;

        SetWindowPos(
            hwnd_, nullptr, 0, 0, width, height,
            SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);

        MoveWindow(edit_, margin, margin, width - margin * 2, inputHeight, TRUE);
        MoveWindow(hint_, 0, 0, 0, 0, FALSE);
        MoveWindow(
            list_,
            margin,
            margin + inputHeight + gap,
            width - margin * 2,
            listHeight,
            TRUE);
        MoveWindow(
            preview_,
            margin + DpiScale(3),
            margin + inputHeight + gap + listHeight + gap,
            width - margin * 2 - DpiScale(6),
            previewHeight,
            TRUE);
        return;
    }

    constexpr int titleHeightLogical = 30;
    constexpr int sideLogical = 7;
    constexpr int inputHeightLogical = 22;
    constexpr int inputWidthLogical = 190;
    constexpr int listTopGapLogical = 4;
    constexpr int listHeightLogical = 162;
    constexpr int bottomGapLogical = 6;
    constexpr int previewHeightLogical = 18;
    constexpr int totalHeightLogical = 250;

    width = DpiScale(widthLogical_);
    height = DpiScale(totalHeightLogical);

    SetWindowPos(
        hwnd_, nullptr, 0, 0, width, height,
        SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);

    const int side = DpiScale(sideLogical);
    const int titleHeight = DpiScale(titleHeightLogical);
    const int inputHeight = DpiScale(inputHeightLogical);
    const int inputWidth = DpiScale(inputWidthLogical);
    const int contentWidth = width - side * 2;
    const int listY = titleHeight + inputHeight + DpiScale(listTopGapLogical);
    const int listHeight = DpiScale(listHeightLogical);
    const int previewY = listY + listHeight + DpiScale(bottomGapLogical);

    MoveWindow(
        edit_,
        side,
        titleHeight,
        inputWidth,
        inputHeight,
        TRUE);

    MoveWindow(
        hint_,
        side + inputWidth,
        titleHeight,
        contentWidth - inputWidth,
        inputHeight,
        TRUE);

    MoveWindow(
        list_,
        side,
        listY,
        contentWidth,
        listHeight,
        TRUE);

    MoveWindow(
        preview_,
        side,
        previewY,
        contentWidth,
        DpiScale(previewHeightLogical),
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
    const int y = info.rcWork.top + std::max(DpiScale(45), (workHeight - height) / 5);

    SetWindowPos(
        hwnd_,
        HWND_TOPMOST,
        x, y,
        width, height,
        SWP_NOACTIVATE);
}

RECT LauncherWindow::ClassicCloseRect() const {
    RECT client{};
    GetClientRect(hwnd_, &client);

    const int size = DpiScale(24);
    const int inset = DpiScale(4);

    return {
        client.right - inset - size,
        inset,
        client.right - inset,
        inset + size,
    };
}

void LauncherWindow::PaintClassicLogo(HDC dc, int x, int y) {
    const int s = DpiScale(18);

    std::array<POINT, 8> star{{
        {x + s / 2, y},
        {x + s * 3 / 5, y + s * 2 / 5},
        {x + s, y + s / 2},
        {x + s * 3 / 5, y + s * 3 / 5},
        {x + s / 2, y + s},
        {x + s * 2 / 5, y + s * 3 / 5},
        {x, y + s / 2},
        {x + s * 2 / 5, y + s * 2 / 5},
    }};

    HBRUSH blue = CreateSolidBrush(RGB(44, 118, 184));
    HPEN outline = CreatePen(PS_SOLID, DpiScale(1), RGB(35, 70, 95));
    HGDIOBJ oldBrush = SelectObject(dc, blue);
    HGDIOBJ oldPen = SelectObject(dc, outline);
    Polygon(dc, star.data(), static_cast<int>(star.size()));
    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
    DeleteObject(blue);
    DeleteObject(outline);

    POINT diamond[4]{
        {x + s / 2, y + DpiScale(3)},
        {x + s - DpiScale(3), y + s / 2},
        {x + s / 2, y + s - DpiScale(3)},
        {x + DpiScale(3), y + s / 2},
    };

    HBRUSH gold = CreateSolidBrush(RGB(247, 193, 70));
    oldBrush = SelectObject(dc, gold);
    Polygon(dc, diamond, 4);
    SelectObject(dc, oldBrush);
    DeleteObject(gold);
}

void LauncherWindow::PaintClassicClose(HDC dc, const RECT& rect) {
    const int width = std::max(2, DpiScale(3));

    HPEN shadow = CreatePen(PS_SOLID, width + 1, RGB(116, 48, 45));
    HGDIOBJ oldPen = SelectObject(dc, shadow);

    MoveToEx(dc, rect.left + DpiScale(4), rect.top + DpiScale(5), nullptr);
    LineTo(dc, rect.right - DpiScale(3), rect.bottom - DpiScale(4));
    MoveToEx(dc, rect.right - DpiScale(3), rect.top + DpiScale(5), nullptr);
    LineTo(dc, rect.left + DpiScale(4), rect.bottom - DpiScale(4));

    SelectObject(dc, oldPen);
    DeleteObject(shadow);

    HPEN red = CreatePen(PS_SOLID, width, RGB(231, 95, 87));
    oldPen = SelectObject(dc, red);

    MoveToEx(dc, rect.left + DpiScale(4), rect.top + DpiScale(4), nullptr);
    LineTo(dc, rect.right - DpiScale(4), rect.bottom - DpiScale(5));
    MoveToEx(dc, rect.right - DpiScale(4), rect.top + DpiScale(4), nullptr);
    LineTo(dc, rect.left + DpiScale(4), rect.bottom - DpiScale(5));

    SelectObject(dc, oldPen);
    DeleteObject(red);
}

void LauncherWindow::PaintClassicTitleBar(HDC dc, const RECT& client) {
    RECT title{client.left, client.top, client.right, DpiScale(30)};

    constexpr int bands = 40;
    const COLORREF left = RGB(96, 99, 102);
    const COLORREF right = RGB(158, 161, 165);

    for (int i = 0; i < bands; ++i) {
        RECT band = title;
        band.left = title.left + (title.right - title.left) * i / bands;
        band.right = title.left + (title.right - title.left) * (i + 1) / bands;

        HBRUSH brush = CreateSolidBrush(MixColor(left, right, i, bands - 1));
        FillRect(dc, &band, brush);
        DeleteObject(brush);
    }

    for (int y = DpiScale(2); y < title.bottom; y += std::max(2, DpiScale(2))) {
        HPEN line = CreatePen(PS_SOLID, 1, RGB(122, 126, 130));
        HGDIOBJ oldPen = SelectObject(dc, line);
        MoveToEx(dc, title.left, y, nullptr);
        LineTo(dc, title.right, y);
        SelectObject(dc, oldPen);
        DeleteObject(line);
    }

    PaintClassicLogo(dc, DpiScale(9), DpiScale(6));

    RECT textRect = title;
    textRect.left += DpiScale(38);
    textRect.right -= DpiScale(38);

    HGDIOBJ oldFont = SelectObject(dc, titleFont_);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(232, 247, 37));

    DrawTextW(
        dc,
        titleText_.c_str(),
        -1,
        &textRect,
        DT_SINGLELINE | DT_CENTER | DT_VCENTER | DT_END_ELLIPSIS);

    SelectObject(dc, oldFont);

    PaintClassicClose(dc, ClassicCloseRect());
}

void LauncherWindow::PaintWindowBackground(HDC dc) {
    RECT client{};
    GetClientRect(hwnd_, &client);

    if (IsModern()) {
        FillRect(dc, &client, windowBrush_);
        return;
    }

    FillRect(dc, &client, windowBrush_);
    PaintClassicTitleBar(dc, client);

    const auto palette = CurrentPalette();
    HBRUSH border = CreateSolidBrush(RGB(75, 80, 86));
    FrameRect(dc, &client, border);
    DeleteObject(border);

    RECT inner = client;
    InflateRect(&inner, -DpiScale(2), -DpiScale(2));
    HBRUSH innerBorder = CreateSolidBrush(palette.frame);
    FrameRect(dc, &inner, innerBorder);
    DeleteObject(innerBorder);
}

void LauncherWindow::Show() {
    if (!hwnd_) return;

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

    SetWindowTextW(
        hint_,
        IsModern() ? L"" : app_.Text(TextId::ClassicHint).data());
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
        titleText_ = L"[ALTRun]";
        InvalidateRect(hwnd_, nullptr, FALSE);
        return;
    }

    const auto& command =
        app_.GetCommand(results_[static_cast<std::size_t>(selected)].commandIndex);

    titleText_ = L"[";
    titleText_ += command.keyword;
    titleText_ += L"]";

    std::wstring preview;
    if (!IsModern()) {
        preview = app_.Text(TextId::CommandPrefix);
    }

    preview += command.target;
    if (!command.arguments.empty()) {
        preview += L"  ";
        preview += command.arguments;
    }

    SetWindowTextW(preview_, preview.c_str());

    if (!IsModern()) {
        RECT title{};
        GetClientRect(hwnd_, &title);
        title.bottom = DpiScale(30);
        InvalidateRect(hwnd_, &title, FALSE);
    }
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
    case WM_NCHITTEST:
        if (!IsModern()) {
            POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            ScreenToClient(hwnd_, &point);

            const RECT close = ClassicCloseRect();
            if (PtInRect(&close, point)) {
                return HTCLIENT;
            }

            if (point.y >= 0 && point.y < DpiScale(30)) {
                return HTCAPTION;
            }
        }
        break;

    case WM_LBUTTONUP:
        if (!IsModern()) {
            POINT point{
                GET_X_LPARAM(lParam),
                GET_Y_LPARAM(lParam),
            };

            const RECT close = ClassicCloseRect();
            if (PtInRect(&close, point)) {
                DestroyWindow(hwnd_);
                return 0;
            }
        }
        break;

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

        SetTextColor(dc, IsModern() ? palette.text : RGB(0, 0, 0));
        SetBkColor(dc, IsModern() ? palette.controlBackground : palette.accentBackground);

        return reinterpret_cast<LRESULT>(
            IsModern() ? controlBrush_ : accentBrush_);
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

        if (control == hint_ || control == preview_) {
            SetTextColor(dc, palette.mutedText);

            if (!IsModern()) {
                SetBkColor(dc, palette.accentBackground);
                return reinterpret_cast<LRESULT>(accentBrush_);
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
        const bool selected = (item->itemState & ODS_SELECTED) != 0;

        const COLORREF background =
            selected ? palette.selectionBackground : palette.controlBackground;

        HBRUSH brush = CreateSolidBrush(background);
        FillRect(item->hDC, &item->rcItem, brush);
        DeleteObject(brush);

        SetBkMode(item->hDC, TRANSPARENT);

        const auto& command =
            app_.GetCommand(results_[item->itemID].commandIndex);

        if (IsModern()) {
            RECT keywordRect = item->rcItem;
            keywordRect.left += DpiScale(12);
            keywordRect.right = keywordRect.left + DpiScale(165);

            RECT titleRect = item->rcItem;
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

            return TRUE;
        }

        constexpr int hotkeyColumnLogical = 23;
        constexpr int shortcutColumnLogical = 230;

        RECT numberRect = item->rcItem;
        numberRect.right = item->rcItem.left + DpiScale(hotkeyColumnLogical);

        RECT keywordRect = item->rcItem;
        keywordRect.left = numberRect.right + DpiScale(3);
        keywordRect.right = item->rcItem.left + DpiScale(shortcutColumnLogical) - DpiScale(4);

        RECT titleRect = item->rcItem;
        titleRect.left = item->rcItem.left + DpiScale(shortcutColumnLogical) + DpiScale(8);
        titleRect.right -= DpiScale(4);

        const std::wstring number =
            item->itemID == 9
                ? L"0"
                : std::to_wstring(static_cast<unsigned long long>(item->itemID + 1));

        const auto oldFont = SelectObject(item->hDC, normalFont_);
        const COLORREF fg =
            selected ? palette.selectionText : palette.text;

        SetTextColor(item->hDC, fg);

        DrawTextW(
            item->hDC,
            number.c_str(),
            -1,
            &numberRect,
            DT_SINGLELINE | DT_CENTER | DT_VCENTER);

        DrawTextW(
            item->hDC,
            command.keyword.c_str(),
            -1,
            &keywordRect,
            DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);

        DrawTextW(
            item->hDC,
            command.title.c_str(),
            -1,
            &titleRect,
            DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);

        const COLORREF separatorColor =
            selected ? RGB(195, 226, 248) : palette.separator;

        HPEN separator = CreatePen(PS_SOLID, 1, separatorColor);
        HGDIOBJ oldPen = SelectObject(item->hDC, separator);

        const int firstX = item->rcItem.left + DpiScale(hotkeyColumnLogical);
        const int secondX = item->rcItem.left + DpiScale(shortcutColumnLogical);

        MoveToEx(item->hDC, firstX, item->rcItem.top, nullptr);
        LineTo(item->hDC, firstX, item->rcItem.bottom);

        MoveToEx(item->hDC, secondX, item->rcItem.top, nullptr);
        LineTo(item->hDC, secondX, item->rcItem.bottom);

        SelectObject(item->hDC, oldPen);
        DeleteObject(separator);
        SelectObject(item->hDC, oldFont);

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
        UpdateWindowChrome();
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
