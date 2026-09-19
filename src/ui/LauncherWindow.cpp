#include "LauncherWindow.hpp"

#include "../app/App.hpp"
#include "../core/ClassicBehavior.hpp"
#include "../core/ResultMerger.hpp"

#include <windowsx.h>
#include <commctrl.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <uxtheme.h>

#include <algorithm>
#include <array>
#include <iterator>
#include <string>
#include <utility>

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

[[nodiscard]] std::wstring
PrimaryResultText(
    const LauncherResult& result) {
    if (result.kind !=
        ResultKind::Folder) {
        return result.title;
    }

    std::wstring text =
        result.title;

    if (!text.empty() &&
        text.back() != L'\\' &&
        text.back() != L'/') {
        text.push_back(L'\\');
    }

    return text;
}

[[nodiscard]] bool IsFileSystemResult(
    const LauncherResult& result) {
    return result.kind ==
            ResultKind::File ||
        result.kind ==
            ResultKind::Folder;
}

} // namespace

LauncherWindow::LauncherWindow(App& app, HINSTANCE instance)
    : app_(app), instance_(instance) {}

LauncherWindow::~LauncherWindow() {
    RemoveTrayIcon();

    if (normalFont_) DeleteObject(normalFont_);
    if (boldFont_) DeleteObject(boldFont_);
    if (titleFont_) DeleteObject(titleFont_);
    if (windowBrush_) DeleteObject(windowBrush_);
    if (controlBrush_) DeleteObject(controlBrush_);
    if (accentBrush_) DeleteObject(accentBrush_);
    if (bottomBrush_) DeleteObject(bottomBrush_);
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
        RGB(244, 245, 247),
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
    if (bottomBrush_) {
        DeleteObject(bottomBrush_);
        bottomBrush_ = nullptr;
    }

    const auto palette = CurrentPalette();
    windowBrush_ = CreateSolidBrush(palette.windowBackground);
    controlBrush_ = CreateSolidBrush(palette.controlBackground);
    accentBrush_ = CreateSolidBrush(palette.accentBackground);
    bottomBrush_ = CreateSolidBrush(
        IsModern() ? palette.windowBackground : RGB(181, 208, 184));
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
    constexpr int leftSideLogical = 7;
    constexpr int rightSideLogical = 7;
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

    const int leftSide = DpiScale(leftSideLogical);
    const int rightSide = DpiScale(rightSideLogical);
    const int titleHeight = DpiScale(titleHeightLogical);
    const int inputHeight = DpiScale(inputHeightLogical);
    const int inputWidth = DpiScale(inputWidthLogical);
    const int contentWidth = width - leftSide - rightSide;
    const int listY = titleHeight + inputHeight + DpiScale(listTopGapLogical);
    const int listHeight = DpiScale(listHeightLogical);
    const int previewY = listY + listHeight + DpiScale(bottomGapLogical);

    MoveWindow(
        edit_,
        leftSide,
        titleHeight,
        inputWidth,
        inputHeight,
        TRUE);

    MoveWindow(
        hint_,
        leftSide + inputWidth,
        titleHeight,
        contentWidth - inputWidth,
        inputHeight,
        TRUE);

    MoveWindow(
        list_,
        leftSide,
        listY,
        contentWidth,
        listHeight,
        TRUE);

    MoveWindow(
        preview_,
        leftSide,
        previewY,
        contentWidth,
        DpiScale(previewHeightLogical),
        TRUE);
}

void LauncherWindow::Reposition() {
    HMONITOR monitor = nullptr;
    const auto& popupMonitor = app_.SettingsData().popupMonitor;

    if (popupMonitor == "primary") {
        POINT origin{0, 0};
        monitor = MonitorFromPoint(origin, MONITOR_DEFAULTTOPRIMARY);
    } else if (popupMonitor == "active") {
        HWND foreground = GetForegroundWindow();
        monitor = MonitorFromWindow(
            foreground,
            MONITOR_DEFAULTTONEAREST);
    } else {
        POINT cursor{};
        GetCursorPos(&cursor);
        monitor = MonitorFromPoint(
            cursor,
            MONITOR_DEFAULTTONEAREST);
    }

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

    const int size = DpiScale(25);
    const int inset = DpiScale(3);

    return {
        client.right - inset - size,
        inset,
        client.right - inset,
        inset + size,
    };
}

void LauncherWindow::PaintClassicLogo(HDC dc, int x, int y) {
    // Clean-room vector recreation of the old skin's visual language:
    // a folded blue paper/arrow form with a large orange star in front.
    POINT backShadow[7]{
        {x + DpiScale(9),  y + DpiScale(1)},
        {x + DpiScale(19), y + DpiScale(6)},
        {x + DpiScale(25), y + DpiScale(11)},
        {x + DpiScale(21), y + DpiScale(20)},
        {x + DpiScale(14), y + DpiScale(23)},
        {x + DpiScale(5),  y + DpiScale(17)},
        {x + DpiScale(6),  y + DpiScale(7)},
    };

    HBRUSH shadowBrush = CreateSolidBrush(RGB(44, 57, 70));
    HPEN shadowPen = CreatePen(PS_SOLID, DpiScale(1), RGB(33, 42, 52));
    HGDIOBJ oldBrush = SelectObject(dc, shadowBrush);
    HGDIOBJ oldPen = SelectObject(dc, shadowPen);
    Polygon(dc, backShadow, 7);
    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
    DeleteObject(shadowBrush);
    DeleteObject(shadowPen);

    POINT blueFold[6]{
        {x + DpiScale(8),  y + DpiScale(1)},
        {x + DpiScale(18), y + DpiScale(6)},
        {x + DpiScale(23), y + DpiScale(11)},
        {x + DpiScale(18), y + DpiScale(20)},
        {x + DpiScale(11), y + DpiScale(22)},
        {x + DpiScale(4),  y + DpiScale(14)},
    };

    HBRUSH blueBrush = CreateSolidBrush(RGB(72, 137, 204));
    HPEN bluePen = CreatePen(PS_SOLID, DpiScale(1), RGB(35, 83, 130));
    oldBrush = SelectObject(dc, blueBrush);
    oldPen = SelectObject(dc, bluePen);
    Polygon(dc, blueFold, 6);
    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
    DeleteObject(blueBrush);
    DeleteObject(bluePen);

    // Pale folded tip on the right, clearly separated from the blue body.
    POINT tip[4]{
        {x + DpiScale(18), y + DpiScale(6)},
        {x + DpiScale(25), y + DpiScale(9)},
        {x + DpiScale(23), y + DpiScale(15)},
        {x + DpiScale(18), y + DpiScale(12)},
    };

    HBRUSH tipBrush = CreateSolidBrush(RGB(244, 222, 132));
    HPEN tipPen = CreatePen(PS_SOLID, DpiScale(1), RGB(116, 104, 63));
    oldBrush = SelectObject(dc, tipBrush);
    oldPen = SelectObject(dc, tipPen);
    Polygon(dc, tip, 4);
    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
    DeleteObject(tipBrush);
    DeleteObject(tipPen);

    // Large foreground five-point star.
    POINT star[10]{
        {x + DpiScale(8),  y + DpiScale(3)},
        {x + DpiScale(11), y + DpiScale(9)},
        {x + DpiScale(17), y + DpiScale(9)},
        {x + DpiScale(12), y + DpiScale(13)},
        {x + DpiScale(14), y + DpiScale(20)},
        {x + DpiScale(8),  y + DpiScale(16)},
        {x + DpiScale(2),  y + DpiScale(20)},
        {x + DpiScale(4),  y + DpiScale(13)},
        {x - DpiScale(1),  y + DpiScale(9)},
        {x + DpiScale(5),  y + DpiScale(9)},
    };

    HBRUSH starBrush = CreateSolidBrush(RGB(249, 171, 63));
    HPEN starPen = CreatePen(PS_SOLID, DpiScale(1), RGB(119, 78, 30));
    oldBrush = SelectObject(dc, starBrush);
    oldPen = SelectObject(dc, starPen);
    Polygon(dc, star, 10);
    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
    DeleteObject(starBrush);
    DeleteObject(starPen);

    HPEN highlight = CreatePen(PS_SOLID, DpiScale(1), RGB(255, 225, 155));
    oldPen = SelectObject(dc, highlight);
    MoveToEx(dc, x + DpiScale(3), y + DpiScale(10), nullptr);
    LineTo(dc, x + DpiScale(8), y + DpiScale(5));
    MoveToEx(dc, x + DpiScale(4), y + DpiScale(15), nullptr);
    LineTo(dc, x + DpiScale(8), y + DpiScale(14));
    SelectObject(dc, oldPen);
    DeleteObject(highlight);
}

void LauncherWindow::PaintClassicClose(HDC dc, const RECT& rect) {
    // Filled beveled X rather than two crossing strokes. This more closely
    // matches the chunky early-Windows skin in the reference screenshot.
    const int l = rect.left + DpiScale(2);
    const int t = rect.top + DpiScale(2);
    const int r = rect.right - DpiScale(2);
    const int b = rect.bottom - DpiScale(2);
    const int arm = DpiScale(5);

    POINT shadow[12]{
        {l + arm, t + DpiScale(2)},
        {(l + r) / 2, (t + b) / 2 - arm / 2 + DpiScale(2)},
        {r - arm, t + DpiScale(2)},
        {r, t + arm + DpiScale(2)},
        {(l + r) / 2 + arm / 2, (t + b) / 2 + DpiScale(2)},
        {r, b - arm + DpiScale(2)},
        {r - arm, b + DpiScale(2)},
        {(l + r) / 2, (t + b) / 2 + arm / 2 + DpiScale(2)},
        {l + arm, b + DpiScale(2)},
        {l, b - arm + DpiScale(2)},
        {(l + r) / 2 - arm / 2, (t + b) / 2 + DpiScale(2)},
        {l, t + arm + DpiScale(2)},
    };

    HBRUSH shadowBrush = CreateSolidBrush(RGB(111, 48, 48));
    HPEN shadowPen = CreatePen(PS_SOLID, DpiScale(1), RGB(83, 41, 41));
    HGDIOBJ oldBrush = SelectObject(dc, shadowBrush);
    HGDIOBJ oldPen = SelectObject(dc, shadowPen);
    Polygon(dc, shadow, 12);
    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
    DeleteObject(shadowBrush);
    DeleteObject(shadowPen);

    POINT body[12]{
        {l + arm, t},
        {(l + r) / 2, (t + b) / 2 - arm / 2},
        {r - arm, t},
        {r, t + arm},
        {(l + r) / 2 + arm / 2, (t + b) / 2},
        {r, b - arm},
        {r - arm, b},
        {(l + r) / 2, (t + b) / 2 + arm / 2},
        {l + arm, b},
        {l, b - arm},
        {(l + r) / 2 - arm / 2, (t + b) / 2},
        {l, t + arm},
    };

    HBRUSH bodyBrush = CreateSolidBrush(RGB(226, 91, 86));
    HPEN bodyPen = CreatePen(PS_SOLID, DpiScale(1), RGB(158, 62, 60));
    oldBrush = SelectObject(dc, bodyBrush);
    oldPen = SelectObject(dc, bodyPen);
    Polygon(dc, body, 12);
    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
    DeleteObject(bodyBrush);
    DeleteObject(bodyPen);

    // Top-left bevel highlight.
    HPEN light = CreatePen(PS_SOLID, DpiScale(2), RGB(255, 174, 160));
    oldPen = SelectObject(dc, light);
    MoveToEx(dc, l + arm, t + DpiScale(1), nullptr);
    LineTo(dc, (l + r) / 2, (t + b) / 2 - arm / 2 + DpiScale(1));
    MoveToEx(dc, r - arm, t + DpiScale(1), nullptr);
    LineTo(dc, (l + r) / 2 + DpiScale(1), (t + b) / 2 - arm / 2 + DpiScale(1));
    SelectObject(dc, oldPen);
    DeleteObject(light);
}

void LauncherWindow::PaintClassicTitleBar(HDC dc, const RECT& client) {
    // Keep the same gray side rail from the title bar all the way down the
    // launcher. Previously the gradient reached the outer edge while the
    // content area was inset, producing a visible color break on both sides.
    const int leftRail = DpiScale(7);
    const int rightRail = DpiScale(7);
    RECT title{
        client.left + leftRail,
        client.top,
        client.right - rightRail,
        DpiScale(30)
    };

    constexpr int bands = 40;
    const COLORREF left = RGB(86, 91, 96);
    const COLORREF right = RGB(181, 183, 186);

    for (int i = 0; i < bands; ++i) {
        RECT band = title;
        band.left = title.left + (title.right - title.left) * i / bands;
        band.right = title.left + (title.right - title.left) * (i + 1) / bands;

        HBRUSH brush = CreateSolidBrush(MixColor(left, right, i, bands - 1));
        FillRect(dc, &band, brush);
        DeleteObject(brush);
    }

    for (int y = DpiScale(2); y < title.bottom; y += std::max(2, DpiScale(2))) {
        const int logicalY = MulDiv(y, 96, static_cast<int>(dpi_));
        const COLORREF lineColor =
            (logicalY % 4 == 0) ? RGB(128, 132, 135) : RGB(119, 123, 127);
        HPEN line = CreatePen(PS_SOLID, 1, lineColor);
        HGDIOBJ oldPen = SelectObject(dc, line);
        MoveToEx(dc, title.left, y, nullptr);
        LineTo(dc, title.right, y);
        SelectObject(dc, oldPen);
        DeleteObject(line);
    }

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

    // Continuous Classic side rails. These intentionally use the same color
    // above and below the title/content boundary to avoid the visible break
    // that appeared on the right edge in v0.1.5.
    const int leftRailWidth = DpiScale(7);
    const int rightRailWidth = DpiScale(7);
    RECT leftRail{
        client.left,
        client.top,
        client.left + leftRailWidth,
        client.bottom
    };
    RECT rightRail{
        client.right - rightRailWidth,
        client.top,
        client.right,
        client.bottom
    };
    FillRect(dc, &leftRail, windowBrush_);
    FillRect(dc, &rightRail, windowBrush_);

    // The two base frame strokes keep the left/top/bottom appearance that
    // already matched the reference well.
    HBRUSH border = CreateSolidBrush(RGB(75, 80, 86));
    FrameRect(dc, &client, border);
    DeleteObject(border);

    RECT inner = client;
    InflateRect(&inner, -DpiScale(2), -DpiScale(2));
    HBRUSH innerBorder = CreateSolidBrush(palette.frame);
    FrameRect(dc, &inner, innerBorder);
    DeleteObject(innerBorder);

    // v0.1.9 restores the original full Classic frame width on the right,
    // but avoids the "solid gray column" look. The old skin behaves like a
    // beveled frame: its rail gradually darkens toward the bottom, while the
    // innermost pixels softly inherit the color of the adjacent UI section.
    const LONG railLeft = client.right - static_cast<LONG>(rightRailWidth);
    const LONG railRight = client.right;
    const LONG outerEdge = railRight - 1;

    // Base rail: vertical gray gradient, brighter near the title and darker
    // near the command strip. This matches the visual weight of the original
    // skin much better than a single flat gray fill.
    constexpr int railBands = 32;
    const COLORREF railTop = RGB(154, 157, 162);
    const COLORREF railBottom = RGB(106, 109, 113);

    for (int i = 0; i < railBands; ++i) {
        RECT band{
            railLeft,
            client.top + (client.bottom - client.top) * i / railBands,
            outerEdge,
            client.top + (client.bottom - client.top) * (i + 1) / railBands
        };
        HBRUSH bandBrush = CreateSolidBrush(
            MixColor(railTop, railBottom, i, railBands - 1));
        FillRect(dc, &band, bandBrush);
        DeleteObject(bandBrush);
    }

    // Blend the innermost part of the rail toward the adjacent section color.
    // The transition remains full-height and full-width, but because it follows
    // the title/green/list/command colors it reads as a frame rather than an
    // unrelated vertical bar.
    const int innerBlendWidth = std::max(2, DpiScale(2));
    const int secondBlendWidth = std::max(1, DpiScale(1));

    const int titleBottom = DpiScale(30);
    const int hintBottom = titleBottom + DpiScale(22);
    const int listTop = hintBottom + DpiScale(4);
    const int listBottom = listTop + DpiScale(162);
    const int commandTop = listBottom + DpiScale(6);
    const int commandBottom = commandTop + DpiScale(18);

    const int clientTop = static_cast<int>(client.top);
    const int clientBottom = static_cast<int>(client.bottom);

    auto railColorAtY = [&](int y) -> COLORREF {
        const int height = std::max<int>(1, clientBottom - clientTop - 1);
        const int pos = std::clamp<int>(y - clientTop, 0, height);
        return MixColor(railTop, railBottom, pos, height);
    };

    auto paintSectionBlend = [&](int top, int bottom, COLORREF adjacent) {
        top = std::clamp<int>(top, clientTop, clientBottom);
        bottom = std::clamp<int>(bottom, clientTop, clientBottom);
        if (bottom <= top) return;

        const int midY = top + (bottom - top) / 2;
        const COLORREF rail = railColorAtY(midY);

        RECT soft{
            railLeft,
            top,
            std::min<LONG>(railLeft + innerBlendWidth, outerEdge),
            bottom
        };
        HBRUSH softBrush = CreateSolidBrush(MixColor(adjacent, rail, 1, 4));
        FillRect(dc, &soft, softBrush);
        DeleteObject(softBrush);

        RECT middle{
            soft.right,
            top,
            std::min<LONG>(soft.right + secondBlendWidth, outerEdge),
            bottom
        };
        if (middle.right > middle.left) {
            HBRUSH middleBrush = CreateSolidBrush(MixColor(adjacent, rail, 2, 3));
            FillRect(dc, &middle, middleBrush);
            DeleteObject(middleBrush);
        }
    };

    // Title: use the bright end of the horizontal title gradient as the
    // neighboring color.
    paintSectionBlend(client.top, titleBottom, RGB(181, 183, 186));

    // Top input / hint strip.
    paintSectionBlend(titleBottom, hintBottom, palette.accentBackground);

    // Small separator gap before the list.
    paintSectionBlend(hintBottom, listTop, RGB(126, 131, 136));

    // Main result surface.
    paintSectionBlend(listTop, listBottom, palette.controlBackground);

    // Separator gap before the command strip.
    paintSectionBlend(listBottom, commandTop, RGB(114, 119, 123));

    // Bottom command strip.
    paintSectionBlend(commandTop, commandBottom, RGB(181, 208, 184));

    // Remaining bottom frame area continues the darker frame tone.
    paintSectionBlend(commandBottom, client.bottom, RGB(106, 109, 113));

    // One dark outer stroke ties the right side back into the top/bottom frame.
    RECT outerLine{
        outerEdge,
        client.top,
        railRight,
        client.bottom
    };
    HBRUSH outerBrush = CreateSolidBrush(RGB(75, 80, 86));
    FillRect(dc, &outerLine, outerBrush);
    DeleteObject(outerBrush);

    // Corner controls are painted last so the right Classic frame never clips the
    // close button.
    PaintClassicLogo(dc, DpiScale(9), DpiScale(3));
    PaintClassicClose(dc, ClassicCloseRect());
}

void LauncherWindow::Toggle() {
    if (!hwnd_) return;

    if (IsWindowVisible(hwnd_)) {
        Hide();
    } else {
        Show();
    }
}

void LauncherWindow::Show() {
    if (!hwnd_) return;

    // Do not carry an interrupted IME composition across launcher hides.
    imeComposing_ = false;

    if (app_.SettingsData().clearQueryOnShow) {
        SetWindowTextW(edit_, L"");
    }

    Reposition();
    ShowWindow(hwnd_, SW_SHOWNORMAL);
    SetForegroundWindow(hwnd_);
    SetFocus(edit_);
    SendMessageW(edit_, EM_SETSEL, 0, -1);
    RefreshResults();
}

void LauncherWindow::Hide() {
    app_.ClearActivationContext();

    immediateExecutionPending_ = false;
    dynamicQueryPending_ = false;
    ++searchGeneration_;

    if (hwnd_) {
        ShowWindow(hwnd_, SW_HIDE);
    }
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

void LauncherWindow::RefreshResults(
    bool allowImmediateExecution) {
    if (!list_) return;

    const std::wstring query =
        CurrentQuery();

    ++searchGeneration_;

    const std::size_t
        candidateLimit =
            std::max<std::size_t>(
                maxResults_,
                maxResults_ * 3);

    staticResults_ =
        app_.Search(
            query,
            candidateLimit);
    dynamicResults_.clear();

    dynamicQueryPending_ =
        !query.empty() &&
        app_.DynamicSearchEnabled();

    immediateExecutionPending_ =
        allowImmediateExecution;

    RebuildVisibleResults(
        immediateExecutionPending_);

    if (dynamicQueryPending_) {
        app_.BeginDynamicSearch(
            searchGeneration_,
            query,
            candidateLimit);
    } else {
        immediateExecutionPending_ =
            false;
    }
}

void LauncherWindow::ApplyDynamicResults(
    std::uint64_t generation,
    std::vector<LauncherResult> results) {
    if (generation !=
        searchGeneration_) {
        return;
    }

    dynamicQueryPending_ = false;
    dynamicResults_ =
        std::move(results);

    RebuildVisibleResults(
        immediateExecutionPending_);

    immediateExecutionPending_ =
        false;
}

void LauncherWindow::RebuildVisibleResults(
    bool allowImmediateExecution) {
    std::wstring selectedId;
    std::string selectedProvider;

    const LRESULT previous =
        SendMessageW(
            list_,
            LB_GETCURSEL,
            0,
            0);

    if (previous != LB_ERR &&
        static_cast<std::size_t>(
            previous) <
            results_.size()) {
        selectedId =
            results_[
                static_cast<std::size_t>(
                    previous)]
                .id;
        selectedProvider =
            results_[
                static_cast<std::size_t>(
                    previous)]
                .providerId;
    }

    results_ =
        MergeLauncherResultsRanked(
            staticResults_,
            dynamicResults_,
            maxResults_);

    SendMessageW(
        list_,
        WM_SETREDRAW,
        FALSE,
        0);
    SendMessageW(
        list_,
        LB_RESETCONTENT,
        0,
        0);

    for (std::size_t i = 0;
         i < results_.size();
         ++i) {
        SendMessageW(
            list_,
            LB_ADDSTRING,
            0,
            reinterpret_cast<LPARAM>(
                L""));
    }

    if (!results_.empty()) {
        std::size_t selection = 0;

        if (!selectedId.empty()) {
            const auto it =
                std::find_if(
                    results_.begin(),
                    results_.end(),
                    [&](const LauncherResult&
                            result) {
                        return result.id ==
                                   selectedId &&
                            result.providerId ==
                                   selectedProvider;
                    });

            if (it != results_.end()) {
                selection =
                    static_cast<std::size_t>(
                        std::distance(
                            results_.begin(),
                            it));
            }
        }

        SendMessageW(
            list_,
            LB_SETCURSEL,
            static_cast<WPARAM>(
                selection),
            0);
    }

    SendMessageW(
        list_,
        WM_SETREDRAW,
        TRUE,
        0);
    InvalidateRect(
        list_,
        nullptr,
        TRUE);
    UpdatePreview();

    const bool queryEmpty =
        CurrentQuery().empty();

    if (classic_behavior::
            ShouldExecuteSingleResult(
                allowImmediateExecution &&
                    !app_.
                        DynamicSearchEnabled(),
                app_.SettingsData()
                    .executeSingleResultImmediately,
                imeComposing_,
                queryEmpty,
                dynamicQueryPending_,
                results_.size())) {
        ExecuteResultAt(0);
    }
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

    const auto& result =
        results_[
            static_cast<std::size_t>(
                selected)];

    const std::wstring primary =
        PrimaryResultText(result);

    titleText_ = L"[";
    titleText_ += primary;
    titleText_ += L"]";

    std::wstring preview;
    if (!IsModern() &&
        !IsFileSystemResult(result)) {
        preview =
            app_.Text(
                TextId::CommandPrefix);
    }

    preview +=
        result.detail.empty()
            ? result.target
            : result.detail;

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

void LauncherWindow::ExecuteSelection(
    LauncherExecutionIntent intent) {
    const LRESULT selected =
        SendMessageW(
            list_,
            LB_GETCURSEL,
            0,
            0);

    if (selected == LB_ERR) {
        return;
    }

    ExecuteResultAt(
        static_cast<std::size_t>(
            selected),
        intent);
}

void LauncherWindow::ExecuteResultAt(
    std::size_t resultIndex,
    LauncherExecutionIntent intent) {

    if (resultIndex >=
        results_.size()) {
        return;
    }

    immediateExecutionPending_ =
        false;

    if (app_.ExecuteResult(
            results_[resultIndex],
            intent) &&
        app_.SettingsData()
            .hideAfterLaunch) {
        Hide();
    }
}

int LauncherWindow::QuickLaunchIndexForKey(
    WPARAM key) const {

    if (IsModern() ||
        !app_.SettingsData()
             .numericQuickLaunch) {
        return -1;
    }

    if ((GetKeyState(VK_CONTROL) &
             0x8000) != 0 ||
        (GetKeyState(VK_MENU) &
             0x8000) != 0 ||
        (GetKeyState(VK_SHIFT) &
             0x8000) != 0 ||
        (GetKeyState(VK_LWIN) &
             0x8000) != 0 ||
        (GetKeyState(VK_RWIN) &
             0x8000) != 0) {
        return -1;
    }

    int digit = -1;

    if (key >= L'0' &&
        key <= L'9') {
        digit =
            static_cast<int>(
                key - L'0');
    } else if (
        key >= VK_NUMPAD0 &&
        key <= VK_NUMPAD9) {
        digit =
            static_cast<int>(
                key - VK_NUMPAD0);
    }

    return classic_behavior::
        QuickLaunchIndexForDigit(
            digit,
            app_.SettingsData()
                .numericQuickLaunchOrder);
}

std::wstring
LauncherWindow::ResultNumberLabel(
    std::size_t resultIndex) const {

    if (app_.SettingsData()
            .numericQuickLaunchOrder ==
        "zero-to-nine") {

        return resultIndex < 10
            ? std::to_wstring(
                  static_cast<
                      unsigned long long>(
                      resultIndex))
            : L"";
    }

    if (resultIndex >= 10) {
        return L"";
    }

    return resultIndex == 9
        ? L"0"
        : std::to_wstring(
              static_cast<
                  unsigned long long>(
                  resultIndex + 1));
}

void LauncherWindow::AddTrayIcon() {
    if (!app_.SettingsData().showTrayIcon || trayIconAdded_) return;

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
    trayIconAdded_ = true;
}

void LauncherWindow::RemoveTrayIcon() {
    if (!hwnd_ || !trayIconAdded_) return;

    NOTIFYICONDATAW data{};
    data.cbSize = sizeof(data);
    data.hWnd = hwnd_;
    data.uID = 1;
    Shell_NotifyIconW(NIM_DELETE, &data);
    trayIconAdded_ = false;
}

void LauncherWindow::ApplyGeneralSettings() {
    if (app_.SettingsData().showTrayIcon) {
        AddTrayIcon();
    } else {
        RemoveTrayIcon();
    }
}

void LauncherWindow::ShowTrayMenu(POINT point) {
    HMENU menu = CreatePopupMenu();
    HMENU appearanceMenu = CreatePopupMenu();
    HMENU languageMenu = CreatePopupMenu();

    const auto& settings = app_.SettingsData();

    AppendMenuW(menu, MF_STRING, kMenuShow, app_.Text(TextId::TrayShow).data());
    AppendMenuW(
        menu,
        MF_STRING,
        kMenuSettings,
        app_.SettingsData().language == Language::ZhCN ? L"设置\tF2" : L"Settings\tF2");
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

    if (message == WM_IME_STARTCOMPOSITION) {
        imeComposing_ = true;
    } else if (
        message == WM_IME_ENDCOMPOSITION) {
        imeComposing_ = false;
    }

    if (message == WM_KEYDOWN) {
        const bool controlDown =
            (GetKeyState(VK_CONTROL) &
                0x8000) != 0;
        const bool shiftDown =
            (GetKeyState(VK_SHIFT) &
                0x8000) != 0;
        const bool altDown =
            (GetKeyState(VK_MENU) &
                0x8000) != 0;
        const bool winDown =
            (GetKeyState(VK_LWIN) &
                0x8000) != 0 ||
            (GetKeyState(VK_RWIN) &
                0x8000) != 0;

        if ((wParam == L'C' ||
             wParam == L'c') &&
            controlDown &&
            shiftDown &&
            !altDown &&
            !winDown) {
            const bool firstPress =
                (lParam &
                 (static_cast<LPARAM>(1)
                  << 30)) == 0;

            if (firstPress) {
                ExecuteSelection(
                    LauncherExecutionIntent::
                        CopySelectedText);
            }

            return 0;
        }

        const int quickLaunchIndex =
            QuickLaunchIndexForKey(
                wParam);

        if (quickLaunchIndex >= 0) {
            // Bit 30 is set for key-repeat WM_KEYDOWN messages. Swallow
            // repeats so holding a number cannot launch the same result
            // many times when hide-after-launch is disabled.
            const bool firstPress =
                (lParam &
                 (static_cast<LPARAM>(1)
                  << 30)) == 0;

            if (firstPress &&
                static_cast<std::size_t>(
                    quickLaunchIndex) <
                    results_.size()) {

                ExecuteResultAt(
                    static_cast<
                        std::size_t>(
                        quickLaunchIndex));
            }

            return 0;
        }

        switch (wParam) {
        case VK_DOWN:
            MoveSelection(1);
            return 0;
        case VK_UP:
            MoveSelection(-1);
            return 0;
        case VK_RETURN: {
            const bool navigateExplorer =
                (GetKeyState(VK_CONTROL) &
                    0x8000) != 0;

            ExecuteSelection(
                navigateExplorer
                    ? LauncherExecutionIntent::
                        NavigateCurrentFileManager
                    : LauncherExecutionIntent::
                        Default);
            return 0;
        }
        case VK_TAB:
            MoveSelection((GetKeyState(VK_SHIFT) & 0x8000) != 0 ? -1 : 1);
            return 0;
        case VK_F2:
            Hide();
            app_.ShowSettings();
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
                Hide();
                return 0;
            }
        }
        break;

    case WM_COMMAND:
        if (LOWORD(wParam) == 1001 &&
            HIWORD(wParam) == EN_CHANGE) {
            // IME composition can emit intermediate EN_CHANGE events.
            // Search may update live, but single-result auto execution must
            // wait until composition is committed.
            RefreshResults(
                !imeComposing_);
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
        case kMenuSettings:
            Hide();
            app_.ShowSettings();
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

        if (control == hint_) {
            SetTextColor(dc, palette.mutedText);

            if (!IsModern()) {
                SetBkColor(dc, palette.accentBackground);
                return reinterpret_cast<LRESULT>(accentBrush_);
            }

            SetBkColor(dc, palette.windowBackground);
            return reinterpret_cast<LRESULT>(windowBrush_);
        }

        if (control == preview_) {
            SetTextColor(dc, palette.mutedText);

            if (!IsModern()) {
                SetBkColor(dc, RGB(181, 208, 184));
                return reinterpret_cast<LRESULT>(bottomBrush_);
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

        const auto& result =
            results_[item->itemID];

        const std::wstring primary =
            PrimaryResultText(result);

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
                primary.c_str(),
                -1,
                &keywordRect,
                DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);

            SelectObject(item->hDC, normalFont_);
            SetTextColor(
                item->hDC,
                selected ? palette.selectionText : palette.text);

            DrawTextW(
                item->hDC,
                result.subtitle.c_str(),
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
            ResultNumberLabel(
                item->itemID);

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
            primary.c_str(),
            -1,
            &keywordRect,
            DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);

        DrawTextW(
            item->hDC,
            result.subtitle.c_str(),
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

    case WM_POWERBROADCAST:
        if (wParam == PBT_APMRESUMEAUTOMATIC ||
            wParam == PBT_APMRESUMESUSPEND) {
            app_.RepairGlobalHotkey();
        }
        return TRUE;

    case WM_ACTIVATE:
        if (LOWORD(wParam) == WA_INACTIVE &&
            IsWindowVisible(hwnd_) &&
            app_.SettingsData().hideOnFocusLost) {
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
        hwnd_ = nullptr;
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hwnd_, message, wParam, lParam);
}

} // namespace altrun
