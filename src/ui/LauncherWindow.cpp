#include "Feedback.hpp"
#include "AppIcon.hpp"
#include "LauncherWindow.hpp"
#include "LauncherInteraction.hpp"
#include "../core/RelevancePolicy.hpp"
#include "../app/App.hpp"
#include "../core/ClassicBehavior.hpp"
#include "../core/ContextActions.hpp"
#include "../core/HotkeyRegistry.hpp"
#include "../core/ResultMerger.hpp"
#include "../platform/AppIdentity.hpp"
#include "../platform/Hotkey.hpp"
#include "../platform/InstanceIpc.hpp"
#include "../platform/ShellActions.hpp"
#include "../platform/WinUtil.hpp"
#include "ShortcutEditorDialog.hpp"
#include "TopLevelWindowPresentation.hpp"
#include "UiTypography.hpp"

#include <windowsx.h>
#include <commctrl.h>
#include <dwmapi.h>
#include <imm.h>
#include <shellapi.h>

#include <algorithm>
#include <array>
#include <cwctype>
#include <filesystem>
#include <iterator>
#include <limits>
#include <string>
#include <utility>

namespace altrun {

namespace {

constexpr const wchar_t* kWindowClass =
    instance_ipc::kLauncherWindowClass;
constexpr wchar_t kWindowTitle[] = L"Asterun";

[[nodiscard]] bool BeginEnglishInputOverride(
    HWND edit,
    bool& originalOpen) noexcept {

    originalOpen = false;

    if (!edit) {
        return false;
    }

    HIMC inputContext =
        ImmGetContext(
            edit);

    if (!inputContext) {
        return false;
    }

    originalOpen =
        ImmGetOpenStatus(
            inputContext) != FALSE;

    // If this EDIT already starts in direct input mode, there is nothing for
    // Asterun to override and therefore nothing to restore when it hides.
    if (!originalOpen) {
        ImmReleaseContext(
            edit,
            inputContext);
        return false;
    }

    // Keep the user's current keyboard layout/input method selected. Only the
    // launcher's EDIT IME context is closed for this launcher session.
    ImmNotifyIME(
        inputContext,
        NI_COMPOSITIONSTR,
        CPS_CANCEL,
        0);

    const bool changed =
        ImmSetOpenStatus(
            inputContext,
            FALSE) != FALSE;

    ImmReleaseContext(
        edit,
        inputContext);

    return changed;
}

void RestoreEnglishInputOverride(
    HWND edit,
    bool originalOpen) noexcept {

    if (!edit ||
        !originalOpen) {
        return;
    }

    HIMC inputContext =
        ImmGetContext(
            edit);

    if (!inputContext) {
        return;
    }

    const bool currentOpen =
        ImmGetOpenStatus(
            inputContext) != FALSE;

    // Restore only when the EDIT is still in the exact direct-input state
    // Asterun imposed. If the user manually switched back to Chinese during
    // the session, currentOpen is already true and their choice wins.
    if (!currentOpen) {
        ImmSetOpenStatus(
            inputContext,
            TRUE);
    }

    ImmReleaseContext(
        edit,
        inputContext);
}

void InitializeTrayIconIdentity(
    NOTIFYICONDATAW& data,
    HWND window) {
    data.cbSize = sizeof(data);
    data.hWnd = window;
    data.uID = 1;
    data.guidItem =
        app_identity::kTrayIconGuid;
}

[[nodiscard]] std::wstring
FormatTrayHotkey(
    const HotkeyBinding& binding) {

    if (!binding.enabled ||
        binding.key.empty()) {
        return {};
    }

    std::wstring result;

    const auto append =
        [&](std::wstring_view text) {
            if (!result.empty()) {
                result += L"+";
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

constexpr DWORD kDwmWindowCornerPreference = 33;
constexpr DWORD kDwmSystemBackdropType = 38;
constexpr int kDwmDoNotRound = 1;
constexpr int kDwmRound = 2;
constexpr int kDwmBackdropNone = 1;
constexpr int kDwmBackdropMainWindow = 2;

int CALLBACK MarkFontFamilyAvailable(
    const LOGFONTW*,
    const TEXTMETRICW*,
    DWORD,
    LPARAM data) {

    *reinterpret_cast<bool*>(data) =
        true;
    return 0;
}

[[nodiscard]] bool
FontFamilyAvailable(
    const wchar_t* face) {

    if (!face || !*face) {
        return false;
    }

    HDC dc =
        GetDC(nullptr);
    if (!dc) {
        return false;
    }

    LOGFONTW query{};
    query.lfCharSet =
        DEFAULT_CHARSET;
    wcsncpy_s(
        query.lfFaceName,
        face,
        _TRUNCATE);

    bool available =
        false;

    EnumFontFamiliesExW(
        dc,
        &query,
        MarkFontFamilyAvailable,
        reinterpret_cast<LPARAM>(
            &available),
        0);

    ReleaseDC(
        nullptr,
        dc);

    return available;
}

[[nodiscard]] const wchar_t*
ModernSearchGlyphFace() {

    return FontFamilyAvailable(
               L"Segoe Fluent Icons")
        ? L"Segoe Fluent Icons"
        : L"Segoe MDL2 Assets";
}

enum ResultContextMenuId : UINT {
    kResultContextPrimary = 41001,
    kResultContextNavigate = 41002,
    kResultContextAddShortcut = 41003,
    kResultContextEditShortcut = 41004,
    kResultContextLocate = 41005,
    kResultContextCopy = 41006,
    kResultContextDeleteShortcut = 41007,
    kResultContextRunAsAdministrator = 41008,
};

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

[[nodiscard]] std::wstring
ModernPrimaryResultText(
    const LauncherResult& result) {
    const std::wstring fallback =
        PrimaryResultText(result);

    if (IsFileSystemResult(result) ||
        result.subtitle.empty()) {
        return fallback;
    }

    return result.subtitle;
}

[[nodiscard]] std::wstring
ModernSecondaryResultText(
    const LauncherResult& result) {
    const std::wstring fallback =
        PrimaryResultText(result);

    if (IsFileSystemResult(result)) {
        return result.subtitle;
    }

    if (result.subtitle.empty() ||
        result.subtitle == fallback) {
        return {};
    }

    return fallback;
}

constexpr std::array<
    int,
    ui::kClassicGlyphAssetPixelSizes.size()>
    kClassicShortcutResourceIds{
        IDB_CLASSIC_SHORTCUT,
        IDB_CLASSIC_SHORTCUT_125,
        IDB_CLASSIC_SHORTCUT_150,
        IDB_CLASSIC_SHORTCUT_175,
        IDB_CLASSIC_SHORTCUT_200,
    };

constexpr std::array<
    int,
    ui::kClassicGlyphAssetPixelSizes.size()>
    kClassicCloseResourceIds{
        IDB_CLASSIC_CLOSE,
        IDB_CLASSIC_CLOSE_125,
        IDB_CLASSIC_CLOSE_150,
        IDB_CLASSIC_CLOSE_175,
        IDB_CLASSIC_CLOSE_200,
    };

template <std::size_t N>
bool LoadClassicBitmapResources(
    HINSTANCE instance,
    const std::array<int, N>& resourceIds,
    std::array<HBITMAP, N>& bitmaps) {

    for (std::size_t index = 0;
         index < N;
         ++index) {
        bitmaps[index] =
            static_cast<HBITMAP>(
                LoadImageW(
                    instance,
                    MAKEINTRESOURCEW(
                        resourceIds[index]),
                    IMAGE_BITMAP,
                    0,
                    0,
                    LR_CREATEDIBSECTION));

        if (!bitmaps[index]) {
            return false;
        }
    }

    return true;
}

void PaintClassicBitmapGlyph(
    HDC dc,
    HDC source,
    HBITMAP bitmap,
    int sourceSize,
    bool hasPerPixelAlpha,
    const RECT& clip,
    int x,
    int y,
    int size) {
    if (!source ||
        !bitmap ||
        sourceSize <= 0 ||
        size <= 0) {
        return;
    }

    HGDIOBJ oldBitmap =
        SelectObject(source, bitmap);

    const int saved =
        SaveDC(dc);

    IntersectClipRect(
        dc,
        clip.left,
        clip.top,
        clip.right,
        clip.bottom);

    if (hasPerPixelAlpha) {
        const BLENDFUNCTION blend{
            AC_SRC_OVER,
            0,
            255,
            AC_SRC_ALPHA,
        };

        if (!AlphaBlend(
                dc,
                x,
                y,
                size,
                size,
                source,
                0,
                0,
                sourceSize,
                sourceSize,
                blend)) {
            SetStretchBltMode(
                dc,
                COLORONCOLOR);
            TransparentBlt(
                dc,
                x,
                y,
                size,
                size,
                source,
                0,
                0,
                sourceSize,
                sourceSize,
                RGB(0, 0, 0));
        }
    } else {
        SetStretchBltMode(
            dc,
            COLORONCOLOR);
        TransparentBlt(
            dc,
            x,
            y,
            size,
            size,
            source,
            0,
            0,
            sourceSize,
            sourceSize,
            RGB(0, 0, 0));
    }

    RestoreDC(
        dc,
        saved);
    SelectObject(
        source,
        oldBitmap);
}



} // namespace

LauncherWindow::LauncherWindow(App& app, HINSTANCE instance)
    : app_(app), instance_(instance) {}

LauncherWindow::~LauncherWindow() {
    RemoveTrayIcon();

    if (normalFont_) DeleteObject(normalFont_);
    if (searchFont_) DeleteObject(searchFont_);
    if (auxiliaryFont_) DeleteObject(auxiliaryFont_);
    if (boldFont_) DeleteObject(boldFont_);
    if (titleFont_) DeleteObject(titleFont_);
    if (searchGlyphFont_) DeleteObject(searchGlyphFont_);
    if (shortcutHintFont_) DeleteObject(shortcutHintFont_);
    if (shortcutArrowFont_) DeleteObject(shortcutArrowFont_);
    if (windowBrush_) DeleteObject(windowBrush_);
    if (controlBrush_) DeleteObject(controlBrush_);
    if (accentBrush_) DeleteObject(accentBrush_);
    if (bottomBrush_) DeleteObject(bottomBrush_);
    if (selectionBrush_) DeleteObject(selectionBrush_);
    if (focusAccentBrush_) DeleteObject(focusAccentBrush_);
    if (framePen_) DeleteObject(framePen_);
    ReleaseClassicResources();
}

void LauncherWindow::ReleaseClassicResources() {
    if (classicBitmapDc_) DeleteDC(classicBitmapDc_);
    classicBitmapDc_ = nullptr;
    for (auto& bitmap : classicShortcutBitmaps_) {
        if (bitmap) DeleteObject(bitmap);
        bitmap = nullptr;
    }
    for (auto& bitmap : classicCloseBitmaps_) {
        if (bitmap) DeleteObject(bitmap);
        bitmap = nullptr;
    }
    if (classicBackgroundBitmap_) DeleteObject(classicBackgroundBitmap_);
    classicBackgroundBitmap_ = nullptr;
    classicBackgroundSize_ = {};
}

bool LauncherWindow::IsModern() const {
    return app_.SettingsData().uiStyle == UiStyle::ModernCompact;
}

bool LauncherWindow::EnsureClassicResources() {
    if (classicBitmapDc_) return true;
    if (!LoadClassicBitmapResources(
            instance_,
            kClassicShortcutResourceIds,
            classicShortcutBitmaps_) ||
        !LoadClassicBitmapResources(
            instance_,
            kClassicCloseResourceIds,
            classicCloseBitmaps_)) {
        ReleaseClassicResources();
        return false;
    }

    classicBackgroundBitmap_ =
        static_cast<HBITMAP>(
            LoadImageW(
                instance_,
                MAKEINTRESOURCEW(
                    IDB_CLASSIC_BACKGROUND),
                IMAGE_BITMAP,
                0,
                0,
                LR_CREATEDIBSECTION));

    BITMAP classicBackgroundInfo{};

    if (!classicBackgroundBitmap_ ||
        GetObjectW(
            classicBackgroundBitmap_,
            sizeof(classicBackgroundInfo),
            &classicBackgroundInfo) !=
            sizeof(classicBackgroundInfo)) {
        ReleaseClassicResources();
        return false;
    }

    classicBackgroundSize_ = {
        classicBackgroundInfo.bmWidth,
        classicBackgroundInfo.bmHeight,
    };

    classicBitmapDc_ =
        CreateCompatibleDC(nullptr);

    if (!classicBitmapDc_) {
        ReleaseClassicResources();
        return false;
    }

    return true;
}

bool LauncherWindow::Create() {
    INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_STANDARD_CLASSES};
    InitCommonControlsEx(&controls);

    taskbarCreatedMessage_ =
        RegisterWindowMessageW(
            L"TaskbarCreated");

    if (!IsModern() && !EnsureClassicResources()) return false;

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.hInstance = instance_;
    wc.lpfnWndProc = WindowProc;
    wc.lpszClassName = kWindowClass;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hIcon =
        ui::LoadApplicationIcon(
            instance_);
    wc.hIconSm =
        ui::LoadApplicationIcon(
            instance_,
            true);
    wc.hbrBackground = nullptr;

    if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return false;
    }

    const auto creation =
        window_presentation::
            ResolveOwnedPopupGeometry(
                nullptr,
                instance_,
                widthLogical_,
                250);

    DWORD extendedStyle =
        WS_EX_TOOLWINDOW |
        WS_EX_TOPMOST;

    if (!IsModern()) {
        extendedStyle |=
            WS_EX_LAYERED;
    }

    const DWORD windowStyle =
        WS_POPUP |
        // Parent background painting must never run underneath the native
        // EDIT/LISTBOX/preview children. This is especially important for
        // Classic's layered/color-key surface while live search is repainting.
        WS_CLIPCHILDREN;

    hwnd_ = CreateWindowExW(
        extendedStyle,
        kWindowClass,
        kWindowTitle,
        windowStyle,
        creation.outer.left,
        creation.outer.top,
        creation.outer.right -
            creation.outer.left,
        creation.outer.bottom -
            creation.outer.top,
        nullptr,
        nullptr,
        instance_,
        this);

    if (!hwnd_) return false;

    window_presentation::Configure(
        hwnd_);

    dpi_ = GetDpiForWindow(hwnd_);
    classicDpiMetrics_ =
        ui::ClassicLauncherMetricsForDpi(
            dpi_);
    modernDpiMetrics_ =
        ui::ModernCompactLauncherMetricsForDpi(
            dpi_);
    CreateChildren();
    ApplyAppearance();
    ApplyLanguage();
    AddTrayIcon();

    RefreshResults();
    Reposition();
    firstRevealPending_ = true;
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

    list_ = CreateWindowExW(
        0,
        L"LISTBOX",
        L"",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL |
            LBS_NOTIFY |
            LBS_OWNERDRAWFIXED |
            LBS_NOINTEGRALHEIGHT,
        0, 0, 0, 0,
        hwnd_,
        reinterpret_cast<HMENU>(1002),
        instance_,
        nullptr);

    preview_ = CreateWindowExW(
        0,
        L"STATIC",
        L"",
        WS_CHILD | WS_VISIBLE |
            SS_LEFT |
            SS_CENTERIMAGE |
            SS_PATHELLIPSIS |
            SS_NOPREFIX,
        0, 0, 0, 0,
        hwnd_,
        reinterpret_cast<HMENU>(1003),
        instance_,
        nullptr);

    classicPreview_ = CreateWindowExW(
        0,
        L"STATIC",
        L"",
        WS_CHILD | WS_VISIBLE |
            SS_OWNERDRAW |
            SS_NOPREFIX,
        0, 0, 0, 0,
        hwnd_,
        reinterpret_cast<HMENU>(1005),
        instance_,
        nullptr);

    SetWindowLongPtrW(edit_, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    oldEditProc_ = reinterpret_cast<WNDPROC>(
        SetWindowLongPtrW(edit_, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(EditProc)));
}

const ui::UiPalette&
LauncherWindow::CurrentPalette() const {
    return ui::LauncherPalette(
        app_.SettingsData().uiStyle);
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
    if (selectionBrush_) {
        DeleteObject(selectionBrush_);
        selectionBrush_ = nullptr;
    }
    if (focusAccentBrush_) {
        DeleteObject(focusAccentBrush_);
        focusAccentBrush_ = nullptr;
    }
    if (framePen_) {
        DeleteObject(framePen_);
        framePen_ = nullptr;
    }

    const auto palette = CurrentPalette();
    windowBrush_ = CreateSolidBrush(palette.windowBackground);
    controlBrush_ = CreateSolidBrush(palette.controlBackground);
    accentBrush_ = CreateSolidBrush(palette.accentBackground);
    bottomBrush_ = CreateSolidBrush(
        palette.bottomBackground);
    selectionBrush_ = CreateSolidBrush(
        palette.selectionBackground);
    focusAccentBrush_ = CreateSolidBrush(
        palette.accent);
    framePen_ = CreatePen(
        PS_SOLID,
        1,
        palette.frame);
}

void LauncherWindow::ApplyFonts() {
    if (normalFont_) {
        DeleteObject(normalFont_);
        normalFont_ = nullptr;
    }
    if (searchFont_) {
        DeleteObject(searchFont_);
        searchFont_ = nullptr;
    }
    if (auxiliaryFont_) {
        DeleteObject(auxiliaryFont_);
        auxiliaryFont_ = nullptr;
    }
    if (boldFont_) {
        DeleteObject(boldFont_);
        boldFont_ = nullptr;
    }
    if (titleFont_) {
        DeleteObject(titleFont_);
        titleFont_ = nullptr;
    }
    if (searchGlyphFont_) {
        DeleteObject(searchGlyphFont_);
        searchGlyphFont_ = nullptr;
    }
    if (shortcutHintFont_) {
        DeleteObject(shortcutHintFont_);
        shortcutHintFont_ = nullptr;
    }
    if (shortcutArrowFont_) {
        DeleteObject(shortcutArrowFont_);
        shortcutArrowFont_ = nullptr;
    }

    const auto style =
        app_.SettingsData().uiStyle;
    const auto language =
        app_.SettingsData().language;

    normalFont_ =
        ui::CreateFontHandle(
            ui::LauncherFontSpec(
                style,
                language,
                ui::UiFontRole::Body),
            dpi_);

    searchFont_ =
        ui::CreateFontHandle(
            ui::LauncherFontSpec(
                style,
                language,
                ui::UiFontRole::LauncherSearch),
            dpi_);

    auxiliaryFont_ =
        ui::CreateFontHandle(
            ui::LauncherFontSpec(
                style,
                language,
                ui::UiFontRole::LauncherAuxiliary),
            dpi_);

    boldFont_ =
        ui::CreateFontHandle(
            ui::LauncherFontSpec(
                style,
                language,
                ui::UiFontRole::BodySemibold),
            dpi_);

    titleFont_ =
        ui::CreateFontHandle(
            ui::LauncherFontSpec(
                style,
                language,
                ui::UiFontRole::LauncherTitle),
            dpi_);

    // Match the icon font's em height to the DPI-scaled 16-logical-pixel
    // glyph box instead of tuning a point size for one display scale.
    // Grayscale antialiasing avoids ClearType subpixel fringing on the icon
    // while preserving the Fluent -> MDL2 platform fallback.
    searchGlyphFont_ =
        ui::CreateFontHandle(
            {
                ModernSearchGlyphFace(),
                0,
                -16,
                FW_NORMAL,
                DEFAULT_CHARSET,
                ANTIALIASED_QUALITY,
            },
            dpi_);

    shortcutHintFont_ =
        ui::CreateFontHandle(
            {
                L"Segoe UI",
                12,
                0,
                FW_NORMAL,
                DEFAULT_CHARSET,
                CLEARTYPE_NATURAL_QUALITY,
            },
            dpi_);

    shortcutArrowFont_ =
        ui::CreateFontHandle(
            {
                L"Segoe UI Symbol",
                13,
                0,
                FW_NORMAL,
                DEFAULT_CHARSET,
                ANTIALIASED_QUALITY,
            },
            dpi_);

    SendMessageW(edit_, WM_SETFONT, reinterpret_cast<WPARAM>(searchFont_), TRUE);
    SendMessageW(list_, WM_SETFONT, reinterpret_cast<WPARAM>(normalFont_), TRUE);
    SendMessageW(preview_, WM_SETFONT, reinterpret_cast<WPARAM>(auxiliaryFont_), TRUE);
    SendMessageW(classicPreview_, WM_SETFONT, reinterpret_cast<WPARAM>(auxiliaryFont_), TRUE);
    const int itemHeight =
        IsModern()
            ? modernDpiMetrics_.rowHeight
            : classicDpiMetrics_.rowHeight;

    SendMessageW(
        list_,
        LB_SETITEMHEIGHT,
        0,
        itemHeight);
}



void LauncherWindow::UpdateWindowChrome() {
    if (!hwnd_) return;

    LONG_PTR style =
        GetWindowLongPtrW(
            hwnd_,
            GWL_STYLE);
    // Both launcher modes are borderless top-level popups. Classic owns its
    // bitmap/region chrome; Modern Compact draws a flat one-pixel client frame
    // and lets DWM provide the outer rounded-corner composition.
    const LONG_PTR wantedStyle =
        style & ~WS_BORDER;

    if (wantedStyle != style) {
        SetWindowLongPtrW(
            hwnd_,
            GWL_STYLE,
            wantedStyle);
        SetWindowPos(
            hwnd_,
            nullptr,
            0,
            0,
            0,
            0,
            SWP_NOMOVE |
                SWP_NOSIZE |
                SWP_NOZORDER |
                SWP_NOACTIVATE |
                SWP_FRAMECHANGED);
    }

    LONG_PTR extendedStyle =
        GetWindowLongPtrW(
            hwnd_,
            GWL_EXSTYLE);

    if (IsModern()) {
        extendedStyle &=
            ~WS_EX_LAYERED;
    } else {
        extendedStyle |=
            WS_EX_LAYERED;
    }

    SetWindowLongPtrW(
        hwnd_,
        GWL_EXSTYLE,
        extendedStyle);

    const int preference =
        IsModern()
            ? kDwmRound
            : kDwmDoNotRound;

    DwmSetWindowAttribute(
        hwnd_,
        kDwmWindowCornerPreference,
        &preference,
        sizeof(preference));

    const int backdrop =
        IsModern()
            ? kDwmBackdropMainWindow
            : kDwmBackdropNone;

    modernBackdropAvailable_ =
        IsModern() &&
        SUCCEEDED(
            DwmSetWindowAttribute(
                hwnd_,
                kDwmSystemBackdropType,
                &backdrop,
                sizeof(backdrop)));

    if (!IsModern()) {
        DwmSetWindowAttribute(
            hwnd_,
            kDwmSystemBackdropType,
            &backdrop,
            sizeof(backdrop));
    }

    if (IsModern()) {
        // The Windows 11 backdrop request is optional. Windows 10 simply
        // rejects the attribute and continues on the solid surface palette;
        // no launcher behavior depends on the backdrop being available.
        SetWindowRgn(
            hwnd_,
            nullptr,
            TRUE);
        return;
    }

    SetLayeredWindowAttributes(
        hwnd_,
        RGB(0, 0, 0),
        255,
        LWA_ALPHA |
            LWA_COLORKEY);

    RECT rect{};
    GetWindowRect(
        hwnd_,
        &rect);

    const int width =
        rect.right - rect.left;
    const int height =
        rect.bottom - rect.top;

    HRGN region =
        CreateRoundRectRgn(
            0,
            0,
            width,
            height,
            classicDpiMetrics_.cornerDiameter,
            classicDpiMetrics_.cornerDiameter);

    if (SetWindowRgn(
            hwnd_,
            region,
            TRUE) == 0) {
        DeleteObject(region);
    }
}

void LauncherWindow::ApplyAppearance() {
    // First Classic use loads the original DPI variants once. Keep them when
    // returning to Modern; a failed load leaves no partial resources to retain.
    if (!IsModern()) (void)EnsureClassicResources();
    const auto metrics =
        IsModern()
            ? ui::kModernCompactLauncherMetrics
            : ui::kClassicLauncherMetrics;

    widthLogical_ = metrics.widthLogical;
    rowHeightLogical_ = metrics.rowHeightLogical;
    maxResults_ = metrics.maxResults;

    if (IsModern()) {
        modernLayoutRows_ =
            std::min<std::size_t>(
                results_.size(),
                maxResults_);
        modernDpiMetrics_ =
            ui::ModernCompactLauncherMetricsForDpi(
                dpi_,
                modernLayoutRows_);
    }

    RecreateBrushes();
    UpdateControlFrames();
    ApplyFonts();

    ShowWindow(
        preview_,
        IsModern()
            ? SW_SHOWNA
            : SW_HIDE);
    ShowWindow(
        classicPreview_,
        IsModern()
            ? SW_HIDE
            : SW_SHOWNA);

    Layout();
    UpdateWindowChrome();
    RefreshResults();

    InvalidateRect(hwnd_, nullptr, TRUE);
    InvalidateRect(edit_, nullptr, TRUE);
    InvalidateRect(list_, nullptr, TRUE);
    InvalidateRect(preview_, nullptr, TRUE);
    InvalidateRect(
        classicPreview_,
        nullptr,
        TRUE);

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
    UpdatePreview();
    Layout();

    InvalidateRect(hwnd_, nullptr, TRUE);
    InvalidateRect(edit_, nullptr, TRUE);
    InvalidateRect(list_, nullptr, TRUE);
    InvalidateRect(preview_, nullptr, TRUE);
    InvalidateRect(
        classicPreview_,
        nullptr,
        TRUE);
}







RECT LauncherWindow::ClassicCloseRect() const {
    RECT client{};
    GetClientRect(hwnd_, &client);

    return {
        client.right -
            classicDpiMetrics_.closeRightInset -
            classicDpiMetrics_.closeSize,
        classicDpiMetrics_.closeTop,
        client.right -
            classicDpiMetrics_.closeRightInset,
        classicDpiMetrics_.closeTop +
            classicDpiMetrics_.closeSize,
    };
}

void LauncherWindow::PaintClassicLogo(
    HDC dc,
    int x,
    int y) {
    const int size =
        classicDpiMetrics_.glyphSize;
    RECT clip{
        x,
        y,
        x + size,
        y + size,
    };

    const std::size_t assetIndex =
        ui::ClassicGlyphAssetIndexForTarget(
            size);
    const int sourceSize =
        ui::kClassicGlyphAssetPixelSizes[
            assetIndex];

    PaintClassicBitmapGlyph(
        dc,
        classicBitmapDc_,
        classicShortcutBitmaps_[
            assetIndex],
        sourceSize,
        assetIndex != 0,
        clip,
        x,
        y,
        size);
}

void LauncherWindow::PaintClassicClose(
    HDC dc,
    const RECT& rect) {
    const int glyphSize =
        classicDpiMetrics_.glyphSize;
    const int width =
        rect.right - rect.left;
    const int height =
        rect.bottom - rect.top;
    const int x =
        rect.left +
        (width - glyphSize) / 2;
    const int y =
        rect.top +
        (height - glyphSize) / 2;

    const std::size_t assetIndex =
        ui::ClassicGlyphAssetIndexForTarget(
            glyphSize);
    const int sourceSize =
        ui::kClassicGlyphAssetPixelSizes[
            assetIndex];

    PaintClassicBitmapGlyph(
        dc,
        classicBitmapDc_,
        classicCloseBitmaps_[
            assetIndex],
        sourceSize,
        assetIndex != 0,
        rect,
        x,
        y,
        glyphSize);
}

void LauncherWindow::PaintClassicBackground(
    HDC dc,
    const RECT& client) {
    if (!classicBackgroundBitmap_ ||
        !classicBitmapDc_ ||
        classicBackgroundSize_.cx <= 0 ||
        classicBackgroundSize_.cy <= 0) {
        FillRect(
            dc,
            &client,
            windowBrush_);
        return;
    }

    HGDIOBJ oldBitmap =
        SelectObject(
            classicBitmapDc_,
            classicBackgroundBitmap_);

    SetStretchBltMode(
        dc,
        COLORONCOLOR);

    StretchBlt(
        dc,
        0,
        0,
        DpiScale(
            classicBackgroundSize_.cx),
        DpiScale(
            classicBackgroundSize_.cy),
        classicBitmapDc_,
        0,
        0,
        classicBackgroundSize_.cx,
        classicBackgroundSize_.cy,
        SRCCOPY);

    SelectObject(
        classicBitmapDc_,
        oldBitmap);
}

void LauncherWindow::PaintClassicTitleBar(
    HDC dc,
    const RECT& client) {
    const RECT close =
        ClassicCloseRect();

    RECT textRect{
        classicDpiMetrics_.titleTextLeft,
        0,
        close.left,
        std::min<LONG>(
            client.bottom,
            static_cast<LONG>(
                classicDpiMetrics_.titleHeight)),
    };

    HGDIOBJ oldFont =
        SelectObject(
            dc,
            titleFont_);

    SetBkMode(
        dc,
        TRANSPARENT);
    SetTextColor(
        dc,
        RGB(255, 255, 0));

    DrawTextW(
        dc,
        titleText_.c_str(),
        -1,
        &textRect,
        DT_SINGLELINE |
            DT_CENTER |
            DT_VCENTER |
            DT_END_ELLIPSIS);

    SelectObject(
        dc,
        oldFont);
}

void LauncherWindow::PaintWindowBackground(
    HDC dc) {
    RECT client{};
    GetClientRect(
        hwnd_,
        &client);

    if (IsModern()) {
        // The solid base remains the deterministic Windows 10 fallback even
        // when Windows 11 accepts the system-backdrop hint.
        FillRect(
            dc,
            &client,
            windowBrush_);

        const auto toRect =
            [](const ui::UiRectMetrics& metrics) {
                return RECT{
                    metrics.left,
                    metrics.top,
                    metrics.left +
                        metrics.width,
                    metrics.top +
                        metrics.height,
                };
            };

        RECT searchSurface =
            toRect(
                modernDpiMetrics_
                    .searchSurface);

        HGDIOBJ oldPen =
            SelectObject(
                dc,
                framePen_);
        HGDIOBJ oldBrush =
            SelectObject(
                dc,
                controlBrush_);

        RoundRect(
            dc,
            searchSurface.left,
            searchSurface.top,
            searchSurface.right,
            searchSurface.bottom,
            modernDpiMetrics_
                .searchCornerDiameter,
            modernDpiMetrics_
                .searchCornerDiameter);

        SelectObject(
            dc,
            oldBrush);
        SelectObject(
            dc,
            oldPen);

        const auto palette =
            CurrentPalette();

        // The PUA Search glyph is rasterized by the Windows icon font rather
        // than by aliased GDI primitives, so it remains crisp at fractional
        // DPI. It is paint-only; the real input remains the native EDIT.
        if (searchGlyphFont_) {
            const auto& glyph =
                modernDpiMetrics_
                    .searchGlyph;
            RECT glyphRect{
                glyph.left,
                glyph.top,
                glyph.left +
                    glyph.width,
                glyph.top +
                    glyph.height,
            };

            HGDIOBJ oldGlyphFont =
                SelectObject(
                    dc,
                    searchGlyphFont_);
            SetBkMode(
                dc,
                TRANSPARENT);
            SetTextColor(
                dc,
                palette.text);
            DrawTextW(
                dc,
                L"\xE721",
                1,
                &glyphRect,
                DT_SINGLELINE |
                    DT_CENTER |
                    DT_VCENTER |
                    DT_NOPREFIX);
            SelectObject(
                dc,
                oldGlyphFont);
        }

        if (modernLayoutRows_ > 0) {
            const RECT resultsSurface =
                toRect(
                    modernDpiMetrics_
                        .resultsSurface);

            HGDIOBJ oldSurfacePen =
                SelectObject(
                    dc,
                    GetStockObject(NULL_PEN));
            HGDIOBJ oldSurfaceBrush =
                SelectObject(
                    dc,
                    accentBrush_);

            RoundRect(
                dc,
                resultsSurface.left,
                resultsSurface.top,
                resultsSurface.right,
                resultsSurface.bottom,
                modernDpiMetrics_
                    .resultsCornerDiameter,
                modernDpiMetrics_
                    .resultsCornerDiameter);

            const RECT footerSurface =
                toRect(
                    modernDpiMetrics_
                        .footerSurface);

            SelectObject(
                dc,
                bottomBrush_);
            RoundRect(
                dc,
                footerSurface.left,
                footerSurface.top,
                footerSurface.right,
                footerSurface.bottom,
                modernDpiMetrics_
                    .footerCornerDiameter,
                modernDpiMetrics_
                    .footerCornerDiameter);

            SelectObject(
                dc,
                oldSurfaceBrush);
            SelectObject(
                dc,
                oldSurfacePen);

            const RECT action =
                toRect(
                    modernDpiMetrics_
                        .footerAction);
            HGDIOBJ oldActionPen =
                SelectObject(
                    dc,
                    framePen_);
            HGDIOBJ oldActionBrush =
                SelectObject(
                    dc,
                    GetStockObject(NULL_BRUSH));

            RoundRect(
                dc,
                action.left,
                action.top,
                action.right,
                action.bottom,
                DpiScale(8),
                DpiScale(8));

            SelectObject(
                dc,
                oldActionBrush);
            SelectObject(
                dc,
                oldActionPen);

            HGDIOBJ oldFont =
                SelectObject(
                    dc,
                    auxiliaryFont_);
            SetBkMode(
                dc,
                TRANSPARENT);
            SetTextColor(
                dc,
                palette.mutedText);
            RECT actionText =
                action;
            DrawTextW(
                dc,
                L"Enter",
                -1,
                &actionText,
                DT_SINGLELINE |
                    DT_CENTER |
                    DT_VCENTER |
                    DT_NOPREFIX);
            SelectObject(
                dc,
                oldFont);
        }

        return;
    }

    PaintClassicBackground(
        dc,
        client);
    PaintClassicTitleBar(
        dc,
        client);
    PaintClassicLogo(
        dc,
        classicDpiMetrics_.logoLeft,
        classicDpiMetrics_.logoTop);
    PaintClassicClose(
        dc,
        ClassicCloseRect());
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

    if (!app_.CanRevealLauncher()) {
        app_.DeferLauncherReveal();
        return;
    }

    const bool wasVisible = IsWindowVisible(hwnd_) != FALSE;
    CancelPendingNumericIntent();
    lastTextInputTick_ = 0;
    consumedNumericVirtualKey_ = 0;
    consumedNumericChar_ = 0;

    // Do not carry an interrupted IME composition across launcher hides.
    imeComposing_ = false;

    // Every reveal starts a fresh launcher session. Clear selection before
    // SetWindowTextW because EN_CHANGE rebuilds synchronously and should
    // deterministically select the new query's best result.
    if (list_) {
        SendMessageW(
            list_,
            LB_SETCURSEL,
            static_cast<WPARAM>(-1),
            0);
    }

    const auto generationBeforeReset = searchGeneration_;
    SetWindowTextW(edit_, L"");
    const bool refreshedByReset = searchGeneration_ != generationBeforeReset;

    Reposition();

    if (firstRevealPending_) {
        window_presentation::
            RevealFullyPainted(
                hwnd_);
        firstRevealPending_ = false;
    } else {
        ShowWindow(
            hwnd_,
            SW_SHOWNORMAL);
    }

    SetForegroundWindow(hwnd_);
    SetFocus(edit_);
    SendMessageW(edit_, EM_SETSEL, 0, -1);

    PrepareInputForReveal(
        wasVisible);

    // EN_CHANGE normally already refreshed the empty query before reveal.
    // Retain the explicit refresh only when resetting EDIT did not do so.
    if (!refreshedByReset) RefreshResults();
    if (!wasVisible && IsWindowVisible(hwnd_)) ui::PlayFeedback(FeedbackCue::Reveal);
}

void LauncherWindow::PrepareInputForReveal(
    bool wasVisible) noexcept {

    if (wasVisible ||
        imeRevealOverrideActive_ ||
        !app_.SettingsData()
             .defaultEnglishInputOnReveal) {
        return;
    }

    bool originalOpen = false;

    if (BeginEnglishInputOverride(
            edit_,
            originalOpen)) {
        imeRevealOverrideActive_ = true;
        imeRevealOriginalOpen_ =
            originalOpen;
    }
}

void LauncherWindow::RestoreInputOverride() noexcept {
    if (!imeRevealOverrideActive_) {
        return;
    }

    RestoreEnglishInputOverride(
        edit_,
        imeRevealOriginalOpen_);

    imeRevealOverrideActive_ = false;
    imeRevealOriginalOpen_ = false;
}

void LauncherWindow::Hide() {
    app_.ClearActivationContext();

    CancelPendingNumericIntent();
    immediateExecutionPending_ = false;
    dynamicQueryPending_ = false;
    ++searchGeneration_;

    RestoreInputOverride();

    if (hwnd_) {
        ShowWindow(hwnd_, SW_HIDE);
    }
}

std::wstring LauncherWindow::CurrentQuery() const {
    const int length = GetWindowTextLengthW(edit_);
    std::wstring text(static_cast<std::size_t>(length) + 1, L'\0');
    GetWindowTextW(edit_, text.data(), length + 1);
    text.resize(static_cast<std::size_t>(length));
    return text;
}

void LauncherWindow::RefreshResults(
    bool allowImmediateExecution,
    bool preserveSelection) {
    if (!list_) return;

    const std::wstring query =
        CurrentQuery();

    CancelPendingNumericIntent();
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
        relevance::ShouldRunDynamicFilesystemQuery(query) &&
        app_.DynamicSearchEnabled();

    immediateExecutionPending_ =
        allowImmediateExecution;

    RebuildVisibleResults(
        immediateExecutionPending_,
        preserveSelection,
        query.empty());

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
        immediateExecutionPending_,
        true,
        false);

    immediateExecutionPending_ =
        false;
}

void LauncherWindow::RebuildVisibleResults(
    bool allowImmediateExecution,
    bool preserveSelection,
    bool queryEmpty) {
    std::wstring selectedId;
    std::string selectedProvider;

    const LRESULT previous =
        SendMessageW(
            list_,
            LB_GETCURSEL,
            0,
            0);

    if (preserveSelection &&
        previous != LB_ERR &&
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

    auto nextResults =
        MergeLauncherResultsRanked(
            staticResults_,
            dynamicResults_,
            maxResults_);

    const auto sameRenderedRow =
        [](const LauncherResult& left,
           const LauncherResult& right) {
            return left.kind == right.kind &&
                left.title == right.title &&
                left.subtitle == right.subtitle;
        };

    const std::size_t oldCount =
        results_.size();
    const std::size_t newCount =
        nextResults.size();
    const std::size_t overlap =
        std::min(
            oldCount,
            newCount);

    std::size_t dirtyFirst =
        std::max(
            oldCount,
            newCount);
    std::size_t dirtyLastExclusive = 0;

    const auto markDirty =
        [&](std::size_t index) {
            dirtyFirst =
                std::min(
                    dirtyFirst,
                    index);
            dirtyLastExclusive =
                std::max(
                    dirtyLastExclusive,
                    index + 1);
        };

    for (std::size_t index = 0;
         index < overlap;
         ++index) {
        if (!sameRenderedRow(
                results_[index],
                nextResults[index])) {
            markDirty(index);
        }
    }

    if (oldCount != newCount) {
        dirtyFirst =
            std::min(
                dirtyFirst,
                overlap);
        dirtyLastExclusive =
            std::max(
                dirtyLastExclusive,
                std::max(
                    oldCount,
                    newCount));
    }

    int matchedSelection = -1;

    if (!nextResults.empty() &&
        preserveSelection &&
        !selectedId.empty()) {
        const auto it =
            std::find_if(
                nextResults.begin(),
                nextResults.end(),
                [&](const LauncherResult&
                        result) {
                    return result.id ==
                               selectedId &&
                        result.providerId ==
                               selectedProvider;
                });

        if (it !=
            nextResults.end()) {
            matchedSelection =
                static_cast<int>(
                    std::distance(
                        nextResults.begin(),
                        it));
        }
    }

    const int stableSelection =
        preserveSelection
            ? ui::launcher_interaction::
                  StableSelectionIndex(
                      previous ==
                              LB_ERR
                          ? -1
                          : static_cast<int>(
                                previous),
                      matchedSelection,
                      nextResults.size())
            : nextResults.empty()
                ? -1
                : 0;

    const LRESULT nextSelection =
        stableSelection < 0
            ? LB_ERR
            : static_cast<LRESULT>(
                  stableSelection);

    if (previous != nextSelection) {
        if (previous != LB_ERR &&
            static_cast<std::size_t>(
                previous) < oldCount) {
            markDirty(
                static_cast<std::size_t>(
                    previous));
        }

        if (nextSelection != LB_ERR) {
            markDirty(
                static_cast<std::size_t>(
                    nextSelection));
        }
    }

    // Row-count mutations are the only operations that need redraw
    // suspension. For the common live-typing case where the visible count is
    // unchanged, never toggle WM_SETREDRAW at all.
    const bool countChanged =
        oldCount != newCount;

    if (countChanged) {
        SendMessageW(
            list_,
            WM_SETREDRAW,
            FALSE,
            0);

        LRESULT rowCount =
            SendMessageW(
                list_,
                LB_GETCOUNT,
                0,
                0);

        if (rowCount == LB_ERR) {
            rowCount = 0;
        }

        while (static_cast<std::size_t>(
                   rowCount) >
               newCount) {
            SendMessageW(
                list_,
                LB_DELETESTRING,
                static_cast<WPARAM>(
                    rowCount - 1),
                0);
            --rowCount;
        }

        while (static_cast<std::size_t>(
                   rowCount) <
               newCount) {
            if (SendMessageW(
                    list_,
                    LB_ADDSTRING,
                    0,
                    reinterpret_cast<LPARAM>(
                        L"")) == LB_ERR) {
                break;
            }
            ++rowCount;
        }
    }

    results_ =
        std::move(nextResults);

    if (previous != nextSelection) {
        SendMessageW(
            list_,
            LB_SETCURSEL,
            nextSelection == LB_ERR
                ? static_cast<WPARAM>(-1)
                : static_cast<WPARAM>(
                      nextSelection),
            0);
    }

    if (countChanged) {
        SendMessageW(
            list_,
            WM_SETREDRAW,
            TRUE,
            0);
    }

    if (IsModern()) {
        const std::size_t desiredRows =
            ui::launcher_interaction::
                StableModernVisibleRows(
                    modernLayoutRows_,
                    results_.size(),
                    dynamicQueryPending_);

        // Settle top-level geometry only after the LISTBOX redraw guard is
        // released. While Everything is pending, the shell may expand for
        // synchronous results but never transiently collapses and re-expands.
        if (desiredRows !=
            modernLayoutRows_) {
            modernLayoutRows_ =
                desiredRows;
            Layout();
            // The parent owns only the Modern surfaces around native child
            // controls. Do not request a background erase on every row-count
            // transition; WS_CLIPCHILDREN keeps the native EDIT/LISTBOX out
            // of this paint domain.
            InvalidateRect(
                hwnd_,
                nullptr,
                FALSE);
        }
    }

    // If the rendered rows and selection did not change, do not repaint the
    // LISTBOX at all. This makes repeated no-result typing/deletion a true
    // no-op on the visual surface.
    if (dirtyLastExclusive >
        dirtyFirst) {
        RECT dirty{};
        GetClientRect(
            list_,
            &dirty);

        const LRESULT itemHeight =
            SendMessageW(
                list_,
                LB_GETITEMHEIGHT,
                0,
                0);

        if (itemHeight != LB_ERR &&
            itemHeight > 0) {
            dirty.top =
                std::min<LONG>(
                    dirty.bottom,
                    static_cast<LONG>(
                        dirtyFirst *
                        static_cast<std::size_t>(
                            itemHeight)));
            dirty.bottom =
                std::min<LONG>(
                    dirty.bottom,
                    static_cast<LONG>(
                        dirtyLastExclusive *
                        static_cast<std::size_t>(
                            itemHeight)));
        }

        RedrawWindow(
            list_,
            &dirty,
            nullptr,
            RDW_INVALIDATE |
                RDW_NOERASE);
    }

    UpdatePreview();

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
    if (!preview_ ||
        !classicPreview_) {
        return;
    }

    std::wstring nextTitle;
    std::wstring nextPreview;

    const LRESULT selected =
        SendMessageW(
            list_,
            LB_GETCURSEL,
            0,
            0);

    if (selected != LB_ERR &&
        static_cast<std::size_t>(
            selected) <
            results_.size()) {
        const auto& result =
            results_[
                static_cast<std::size_t>(
                    selected)];

        const std::wstring primary =
            PrimaryResultText(result);

        const std::wstring title =
            result.subtitle.empty()
                ? primary
                : result.subtitle;

        bool bracketTitle =
            IsFileSystemResult(
                result);

        if (!bracketTitle &&
            !result.target.empty()) {
            bracketTitle =
                std::filesystem::path(
                    result.target)
                    .has_parent_path();
        }

        if (bracketTitle) {
            nextTitle.push_back(L'[');
        }

        nextTitle += title;

        if (bracketTitle) {
            nextTitle.push_back(L']');
        }

        if (IsModern()) {
            const bool zh =
                app_.SettingsData()
                    .language ==
                Language::ZhCN;

            switch (result.kind) {
            case ResultKind::File:
            case ResultKind::Folder:
                nextPreview =
                    zh
                        ? L"路径  ·  "
                        : L"Path  ·  ";
                break;
            case ResultKind::UserCommand:
                nextPreview =
                    zh
                        ? L"命令  ·  "
                        : L"Command  ·  ";
                break;
            case ResultKind::Action:
                nextPreview =
                    zh
                        ? L"操作  ·  "
                        : L"Action  ·  ";
                break;
            case ResultKind::Application:
            default:
                nextPreview =
                    zh
                        ? L"应用  ·  "
                        : L"App  ·  ";
                break;
            }
        } else if (!IsFileSystemResult(
                       result)) {
            nextPreview =
                app_.SettingsData()
                        .language ==
                    Language::ZhCN
                    ? L"命令="
                    : L"CMD=";
        }

        const std::wstring_view previewValue =
            result.detail.empty()
                ? std::wstring_view(
                      result.target)
                : std::wstring_view(
                      result.detail);

        if (previewValue.empty()) {
            nextPreview.clear();
        } else {
            nextPreview += previewValue;
        }
    }

    const bool previewChanged =
        nextPreview !=
        previewText_;
    const bool titleChanged =
        nextTitle !=
        titleText_;

    if (previewChanged) {
        previewText_ =
            std::move(
                nextPreview);

        // WM_SETTEXT invalidates native STATIC controls. Do not send it for
        // identical text; in particular, repeated empty-result queries must
        // not keep repainting the command area.
        SetWindowTextW(
            preview_,
            previewText_.c_str());
        SetWindowTextW(
            classicPreview_,
            previewText_.c_str());
    }

    if (!titleChanged) {
        return;
    }

    titleText_ =
        std::move(
            nextTitle);

    if (!IsModern()) {
        const RECT close =
            ClassicCloseRect();

        RECT titleRect{
            classicDpiMetrics_
                .titleTextLeft,
            0,
            close.left,
            classicDpiMetrics_
                .titleHeight,
        };

        // Repaint only the title surface. WS_CLIPCHILDREN plus
        // RDW_NOCHILDREN prevents the title refresh from touching the EDIT
        // that begins at y=30 even though the historical title band is 33px.
        RedrawWindow(
            hwnd_,
            &titleRect,
            nullptr,
            RDW_INVALIDATE |
                RDW_NOERASE |
                RDW_NOCHILDREN);
    }
}

void LauncherWindow::MoveSelection(int delta) {
    if (results_.empty()) {
        return;
    }

    int current =
        static_cast<int>(
            SendMessageW(
                list_,
                LB_GETCURSEL,
                0,
                0));

    if (current == LB_ERR) {
        current =
            delta < 0
                ? static_cast<int>(
                      results_.size()) - 1
                : 0;
    } else {
        current =
            classic_behavior::
                WrappedSelectionIndex(
                    current,
                    delta,
                    results_.size(),
                    true);
    }

    SendMessageW(
        list_,
        LB_SETCURSEL,
        current,
        0);
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

    ExecuteResultSnapshot(
        results_[resultIndex],
        intent);
}

void LauncherWindow::ExecuteResultSnapshot(
    const LauncherResult& result,
    LauncherExecutionIntent intent,
    const std::wstring* snapshotQuery) {

    immediateExecutionPending_ =
        false;

    if (app_.ExecuteResult(
            result,
            intent,
            snapshotQuery != nullptr
                ? std::wstring_view(*snapshotQuery)
                : std::wstring_view(CurrentQuery()))) {
        Hide();
    }
}

int LauncherWindow::NumericDigitForKey(
    WPARAM key) const noexcept {

    if (key >= L'0' &&
        key <= L'9') {
        return static_cast<int>(
            key - L'0');
    }

    if (key >= VK_NUMPAD0 &&
        key <= VK_NUMPAD9) {
        return static_cast<int>(
            key - VK_NUMPAD0);
    }

    return -1;
}

int LauncherWindow::QuickLaunchIndexForKey(
    WPARAM key) const {

    if (!app_.SettingsData()
             .numericQuickLaunch) {
        return -1;
    }

    return classic_behavior::
        QuickLaunchIndexForDigit(
            NumericDigitForKey(key),
            "one-to-zero");
}

bool LauncherWindow::
HasRecentTextInput() const noexcept {

    if (lastTextInputTick_ == 0) {
        return false;
    }

    const std::uint64_t now =
        GetTickCount64();

    return now -
        lastTextInputTick_ <=
        classic_behavior::
            kNumericTypingWindowMs;
}

bool LauncherWindow::
HasStrongNumericContinuation(
    wchar_t digit) const {

    std::wstring candidate =
        CurrentQuery();
    candidate.push_back(digit);

    if (app_.HasStaticQueryContinuation(
            candidate)) {
        return true;
    }

    return classic_behavior::
            HasStrongResultContinuation(
                dynamicResults_,
                candidate) ||
        classic_behavior::
            HasStrongResultContinuation(
                staticResults_,
                candidate);
}

void LauncherWindow::ConsumeNumericKey(
    UINT virtualKey,
    wchar_t digit) noexcept {

    consumedNumericVirtualKey_ =
        virtualKey;
    consumedNumericChar_ =
        digit;
}

void LauncherWindow::
QueuePendingNumericIntent(
    UINT virtualKey,
    wchar_t digit,
    const LauncherResult& result) {

    CancelPendingNumericIntent();

    pendingNumericIntent_.active =
        true;
    pendingNumericIntent_.virtualKey =
        virtualKey;
    pendingNumericIntent_.digit =
        digit;
    pendingNumericIntent_.result =
        result;
    pendingNumericIntent_.query = CurrentQuery();
    pendingNumericIntent_.token = ++numericIntentToken_;
    pendingNumericIntent_.started = GetTickCount64();
    pendingNumericIntent_.selection = static_cast<DWORD>(SendMessageW(edit_, EM_GETSEL, 0, 0));
    pendingNumericIntent_.probeRequired = app_.DynamicSearchEnabled();

    ConsumeNumericKey(
        virtualKey,
        digit);

    if (!SetTimer(hwnd_, kNumericIntentTimerId, 15, nullptr)) {
        CommitPendingNumericIntentAsText();
        return;
    }
    if (pendingNumericIntent_.probeRequired) {
        auto candidate = pendingNumericIntent_.query;
        candidate.push_back(digit);
        app_.BeginNumericContinuationProbe(pendingNumericIntent_.token, std::move(candidate));
    }
}

void LauncherWindow::
CommitPendingNumericIntentAsText() {

    if (!pendingNumericIntent_.active ||
        !edit_) {
        return;
    }

    const wchar_t digit =
        pendingNumericIntent_.digit;

    CancelPendingNumericIntent();

    const wchar_t text[2]{
        digit,
        L'\0',
    };

    numericTextCommitInProgress_ =
        true;

    SendMessageW(
        edit_,
        EM_REPLACESEL,
        TRUE,
        reinterpret_cast<LPARAM>(
            text));

    numericTextCommitInProgress_ =
        false;
    lastTextInputTick_ =
        GetTickCount64();
}

void LauncherWindow::ApplyNumericContinuation(
    std::uint64_t token, classic_behavior::ContinuationEvidence evidence) {
    if (!pendingNumericIntent_.active || token != pendingNumericIntent_.token) return;
    pendingNumericIntent_.evidence = evidence;
    ExecutePendingNumericIntent();
}

void LauncherWindow::
ExecutePendingNumericIntent() {
    if (!pendingNumericIntent_.active) return;
    if (!IsVisible() || GetFocus() != edit_ || imeComposing_ ||
        CurrentQuery() != pendingNumericIntent_.query ||
        static_cast<DWORD>(SendMessageW(edit_, EM_GETSEL, 0, 0)) != pendingNumericIntent_.selection) {
        CancelPendingNumericIntent();
        return;
    }
    const auto decision = classic_behavior::ResolvePendingNumericIntent(
        pendingNumericIntent_.evidence, pendingNumericIntent_.probeRequired,
        GetTickCount64() - pendingNumericIntent_.started);
    if (decision == classic_behavior::PendingNumericDecision::Wait) return;
    if (decision == classic_behavior::PendingNumericDecision::Text) {
        CommitPendingNumericIntentAsText();
        return;
    }

    LauncherResult result =
        pendingNumericIntent_.result;
    std::wstring query =
        pendingNumericIntent_.query;

    CancelPendingNumericIntent();

    ExecuteResultSnapshot(
        result,
        LauncherExecutionIntent::Default,
        &query);
}

void LauncherWindow::
CancelPendingNumericIntent() {

    if (hwnd_) {
        KillTimer(
            hwnd_,
            kNumericIntentTimerId);
    }

    pendingNumericIntent_ =
        PendingNumericIntent{};
}

std::wstring
LauncherWindow::ResultNumberLabel(
    std::size_t resultIndex) const {

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

void LauncherWindow::AddTrayIcon(
    bool force) {
    if (trayIconAdded_ ||
        (!force &&
         !app_.SettingsData()
              .showTrayIcon)) {
        return;
    }

    NOTIFYICONDATAW data{};
    InitializeTrayIconIdentity(
        data,
        hwnd_);
    data.uFlags =
        NIF_MESSAGE |
        NIF_ICON |
        NIF_TIP |
        NIF_SHOWTIP |
        NIF_GUID;
    data.uCallbackMessage =
        kTrayMessage;
    data.hIcon =
        ui::LoadTrayIcon(
            instance_);
    wcscpy_s(
        data.szTip,
        L"Asterun");

    if (!Shell_NotifyIconW(
            NIM_ADD,
            &data)) {
        NOTIFYICONDATAW stale{};
        InitializeTrayIconIdentity(
            stale,
            hwnd_);
        stale.uFlags = NIF_GUID;
        Shell_NotifyIconW(
            NIM_DELETE,
            &stale);

        if (!Shell_NotifyIconW(
                NIM_ADD,
                &data)) {
            return;
        }
    }

    data.uVersion =
        NOTIFYICON_VERSION_4;
    Shell_NotifyIconW(
        NIM_SETVERSION,
        &data);

    trayIconAdded_ = true;
    notificationOnlyTrayIcon_ =
        force &&
        !app_.SettingsData()
             .showTrayIcon;
}

void LauncherWindow::RemoveTrayIcon() {
    if (!hwnd_ || !trayIconAdded_) return;

    NOTIFYICONDATAW data{};
    InitializeTrayIconIdentity(
        data,
        hwnd_);
    data.uFlags = NIF_GUID;
    Shell_NotifyIconW(NIM_DELETE, &data);
    trayIconAdded_ = false;
    notificationOnlyTrayIcon_ = false;
}

void LauncherWindow::ShowStartupNotification(
    std::wstring_view activationHotkey) {
    if (!hwnd_) {
        return;
    }

    AddTrayIcon(true);

    if (!trayIconAdded_) {
        return;
    }

    const bool zh =
        app_.SettingsData().language ==
        Language::ZhCN;

    std::wstring message =
        zh
            ? L"已在后台启动"
            : L"Running in the background";

    if (!activationHotkey.empty()) {
        message +=
            zh
                ? L"\n按 "
                : L"\nPress ";
        message += activationHotkey;
        message +=
            zh
                ? L" 呼出"
                : L" to show";
    }

    NOTIFYICONDATAW data{};
    InitializeTrayIconIdentity(
        data,
        hwnd_);
    data.uFlags =
        NIF_INFO |
        NIF_GUID;
    data.dwInfoFlags =
        NIIF_INFO |
        NIIF_NOSOUND;

    wcsncpy_s(
        data.szInfoTitle,
        L"Asterun",
        _TRUNCATE);
    wcsncpy_s(
        data.szInfo,
        message.c_str(),
        _TRUNCATE);

    if (Shell_NotifyIconW(NIM_MODIFY, &data)) {
        ui::PlayFeedback(FeedbackCue::Startup);
    }
}

void LauncherWindow::QueueNewShortcutForPath(
    std::wstring path) {
    if (path.empty()) {
        return;
    }

    pendingShortcutPaths_.push_back(
        std::move(path));

    if (hwnd_ &&
        !contextActionModalActive_) {
        PostMessageW(
            hwnd_,
            kShortcutIpcMessage,
            0,
            0);
    }
}

void LauncherWindow::ShowNewShortcutForPath(
    std::wstring_view path) {
    const Command seed =
        ShortcutSeedFromFileSystemPath(
            path);

    contextActionModalActive_ = true;
    const bool changed =
        ShortcutEditorDialog::ShowNew(
            app_,
            instance_,
            hwnd_,
            seed);
    contextActionModalActive_ = false;

    if (changed) {
        RefreshResults();
    }
}

void LauncherWindow::
ProcessPendingShortcutPaths() {
    if (contextActionModalActive_ ||
        pendingShortcutPaths_.empty()) {
        return;
    }

    std::wstring path =
        std::move(
            pendingShortcutPaths_
                .front());
    pendingShortcutPaths_.pop_front();

    Hide();
    ShowNewShortcutForPath(
        path);

    if (!pendingShortcutPaths_.empty()) {
        PostMessageW(
            hwnd_,
            kShortcutIpcMessage,
            0,
            0);
    }
}

void LauncherWindow::ApplyGeneralSettings() {
    CancelPendingNumericIntent();
    if (app_.SettingsData().showTrayIcon) {
        AddTrayIcon();
    } else {
        RemoveTrayIcon();
    }

    if (IsWindowVisible(hwnd_)) {
        Reposition();
    }
}

void LauncherWindow::ShowResultContextMenu(
    POINT point) {
    if (!list_ ||
        results_.empty()) {
        return;
    }

    const bool keyboardInvocation =
        point.x == -1 &&
        point.y == -1;

    int selected =
        static_cast<int>(
            SendMessageW(
                list_,
                LB_GETCURSEL,
                0,
                0));

    if (!keyboardInvocation) {
        POINT clientPoint = point;
        ScreenToClient(
            list_,
            &clientPoint);

        const LRESULT hit =
            SendMessageW(
                list_,
                LB_ITEMFROMPOINT,
                0,
                MAKELPARAM(
                    clientPoint.x,
                    clientPoint.y));

        if (HIWORD(
                static_cast<DWORD_PTR>(
                    hit)) != 0) {
            return;
        }

        selected =
            static_cast<int>(
                LOWORD(
                    static_cast<DWORD_PTR>(
                        hit)));

        if (selected < 0 ||
            static_cast<std::size_t>(
                selected) >=
                results_.size()) {
            return;
        }

        SendMessageW(
            list_,
            LB_SETCURSEL,
            selected,
            0);
        UpdatePreview();
    } else {
        if (selected == LB_ERR ||
            selected < 0 ||
            static_cast<std::size_t>(
                selected) >=
                results_.size()) {
            return;
        }

        RECT row{};
        if (SendMessageW(
                list_,
                LB_GETITEMRECT,
                selected,
                reinterpret_cast<LPARAM>(
                    &row)) != LB_ERR) {
            point.x =
                row.left +
                (row.right - row.left) / 2;
            point.y =
                row.top +
                (row.bottom - row.top) / 2;
            ClientToScreen(
                list_,
                &point);
        } else {
            GetCursorPos(&point);
        }
    }

    const LauncherResult result =
        results_[
            static_cast<std::size_t>(
                selected)];

    const auto& context =
        app_.LastActivationContext();

    const auto actions =
        EvaluateLauncherContextActions(
            result,
            context.HasExplorer() ||
                context.HasTotalCommander());

    HMENU menu =
        CreatePopupMenu();

    if (!menu) {
        return;
    }

    const bool zh =
        app_.SettingsData().language ==
        Language::ZhCN;

    const auto appendSeparator =
        [&]() {
            const int count =
                GetMenuItemCount(menu);

            if (count <= 0) {
                return;
            }

            MENUITEMINFOW info{};
            info.cbSize = sizeof(info);
            info.fMask = MIIM_FTYPE;

            if (GetMenuItemInfoW(
                    menu,
                    static_cast<UINT>(
                        count - 1),
                    TRUE,
                    &info) &&
                (info.fType &
                 MFT_SEPARATOR) != 0) {
                return;
            }

            AppendMenuW(
                menu,
                MF_SEPARATOR,
                0,
                nullptr);
        };

    if (actions.primary) {
        const wchar_t* label =
            (result.kind ==
                 ResultKind::File ||
             result.kind ==
                 ResultKind::Folder ||
             result.action.kind ==
                 LauncherActionKind::
                     OpenUrl)
                ? (zh ? L"打开" : L"Open")
                : (result.kind ==
                       ResultKind::Action
                       ? (zh
                              ? L"执行"
                              : L"Execute")
                       : (zh
                              ? L"运行"
                              : L"Run"));

        AppendMenuW(
            menu,
            MF_STRING,
            kResultContextPrimary,
            label);

        SetMenuDefaultItem(
            menu,
            kResultContextPrimary,
            FALSE);
    }

    if (actions.runAsAdministrator) {
        AppendMenuW(
            menu,
            MF_STRING,
            kResultContextRunAsAdministrator,
            zh
                ? L"以管理员身份运行"
                : L"Run as administrator");
    }

    if (actions
            .navigateCurrentFileManager) {
        AppendMenuW(
            menu,
            MF_STRING,
            kResultContextNavigate,
            zh
                ? L"在当前文件管理器中打开"
                : L"Open in current file manager");
    }

    if (actions.editShortcut) {
        appendSeparator();

        AppendMenuW(
            menu,
            MF_STRING,
            kResultContextEditShortcut,
            zh
                ? L"编辑快捷项..."
                : L"Edit shortcut...");
    }

    if (actions.locateInExplorer ||
        actions.copyTarget) {
        appendSeparator();

        if (actions.locateInExplorer) {
            AppendMenuW(
                menu,
                MF_STRING,
                kResultContextLocate,
                zh
                    ? L"打开所在目录"
                    : L"Open containing folder");
        }

        if (actions.copyTarget) {
            const bool pathLike =
                CanRevealTargetInExplorer(
                    result.target);

            const wchar_t* copyLabel =
                result.action.kind ==
                        LauncherActionKind::
                            OpenUrl
                    ? (zh
                           ? L"复制链接"
                           : L"Copy link")
                    : (pathLike
                           ? (zh
                                  ? L"复制路径"
                                  : L"Copy path")
                           : (zh
                                  ? L"复制目标"
                                  : L"Copy target"));

            AppendMenuW(
                menu,
                MF_STRING,
                kResultContextCopy,
                copyLabel);
        }
    }

    if (actions.addAsShortcut) {
        appendSeparator();

        AppendMenuW(
            menu,
            MF_STRING,
            kResultContextAddShortcut,
            zh
                ? L"添加到快捷项..."
                : L"Add to shortcuts...");
    }

    if (actions.deleteShortcut) {
        appendSeparator();

        AppendMenuW(
            menu,
            MF_STRING,
            kResultContextDeleteShortcut,
            zh
                ? L"删除快捷项"
                : L"Delete shortcut");
    }

    SetForegroundWindow(hwnd_);

    const UINT command =
        TrackPopupMenuEx(
            menu,
            TPM_RIGHTBUTTON |
                TPM_LEFTALIGN |
                TPM_TOPALIGN |
                TPM_RETURNCMD |
                TPM_NONOTIFY,
            point.x,
            point.y,
            hwnd_,
            nullptr);

    DestroyMenu(menu);

    const auto execute =
        [&](LauncherExecutionIntent intent) {
            if (app_.ExecuteResult(
                    result,
                    intent,
                    CurrentQuery())) {
                Hide();
            }
        };

    switch (command) {
    case kResultContextPrimary:
        execute(
            LauncherExecutionIntent::
                Default);
        return;

    case kResultContextRunAsAdministrator:
        execute(
            LauncherExecutionIntent::
                RunAsAdministrator);
        return;

    case kResultContextNavigate:
        execute(
            LauncherExecutionIntent::
                NavigateCurrentFileManager);
        return;

    case kResultContextAddShortcut: {
        const Command seed =
            ShortcutSeedFromLauncherResult(
                result);

        contextActionModalActive_ = true;
        const bool changed =
            ShortcutEditorDialog::ShowNew(
                app_,
                instance_,
                hwnd_,
                seed);
        contextActionModalActive_ = false;

        if (changed) {
            RefreshResults();
        }
        return;
    }

    case kResultContextEditShortcut:
        contextActionModalActive_ = true;
        {
            const bool changed =
                ShortcutEditorDialog::Show(
                    app_,
                    instance_,
                    hwnd_,
                    result.id);
            contextActionModalActive_ = false;

            if (changed) {
                RefreshResults();
            }
        }
        return;

    case kResultContextLocate:
        if (!win::RevealInExplorer(
                result.target,
                app_.BaseDirectory(),
                result.kind ==
                    ResultKind::Folder)) {
            altrun::ui::ShowMessage(
                hwnd_,
                zh
                    ? L"无法打开目标所在目录。目标可能已移动、删除，或不是文件系统路径。"
                    : L"Could not open the target's containing folder. It may have moved, been deleted, or may not be a filesystem path.",
                L"Asterun",
                MB_OK |
                    MB_ICONINFORMATION);
        }
        return;

    case kResultContextCopy:
        app_.ExecuteResult(
            result,
            LauncherExecutionIntent::
                CopySelectedText);
        return;

    case kResultContextDeleteShortcut: {
        contextActionModalActive_ = true;
        app_.ConfirmDeleteUserCommand(hwnd_, result.id);
        contextActionModalActive_ = false;
        return;
    }

    default:
        return;
    }
}

void LauncherWindow::PrepareTopLevelForegroundHandoff() {
    if (!hwnd_ ||
        GetForegroundWindow() == hwnd_) {
        return;
    }

    // Tray commands are intentionally deferred until TrackPopupMenu has fully
    // unwound. By then Windows may already have restored the previously active
    // application (for example a full-screen browser). Reclaim the foreground
    // on the launcher host while this process still owns the user-initiated
    // tray interaction, then let the destination window take foreground before
    // the launcher is hidden. This avoids a browser -> Settings activation
    // bounce on the first click/wheel input.
    (void)SetForegroundWindow(
        hwnd_);
}

void LauncherWindow::ShowTrayMenu(POINT point) {
    HMENU menu = CreatePopupMenu();
    if (!menu) {
        return;
    }

    const bool zh =
        app_.SettingsData().language ==
        Language::ZhCN;

    const auto itemText =
        [&](std::wstring_view zhText,
            std::wstring_view enText,
            std::string_view actionId) {

            std::wstring text(
                zh ? zhText : enText);

            const auto binding =
                EffectiveHotkeyBinding(
                    app_.SettingsData()
                        .hotkeyBindings,
                    actionId);

            if (binding.enabled &&
                app_.IsHotkeyActionRegistered(
                    actionId)) {
                const auto hotkey =
                    FormatTrayHotkey(
                        binding);

                if (!hotkey.empty()) {
                    text += L"\t";
                    text += hotkey;
                }
            }

            return text;
        };

    const auto showText =
        itemText(
            L"显示主界面",
            L"Show launcher",
            hotkey_actions::kActivate);

    AppendMenuW(
        menu,
        MF_STRING,
        kMenuShow,
        showText.c_str());

    SetMenuDefaultItem(
        menu,
        kMenuShow,
        FALSE);

    AppendMenuW(
        menu,
        MF_SEPARATOR,
        0,
        nullptr);

    const auto shortcutsText =
        itemText(
            L"快捷项管理…",
            L"Shortcut Manager…",
            hotkey_actions::
                kOpenShortcutManager);

    AppendMenuW(
        menu,
        MF_STRING,
        kMenuShortcuts,
        shortcutsText.c_str());

    const auto settingsText =
        itemText(
            L"设置…",
            L"Settings…",
            hotkey_actions::kOpenSettings);

    AppendMenuW(
        menu,
        MF_STRING,
        kMenuSettings,
        settingsText.c_str());

    AppendMenuW(
        menu,
        MF_SEPARATOR,
        0,
        nullptr);

    AppendMenuW(
        menu,
        MF_STRING,
        kMenuAbout,
        zh
            ? L"关于"
            : L"About");

    AppendMenuW(
        menu,
        MF_STRING,
        kMenuExit,
        zh
            ? L"退出"
            : L"Exit");

    // A tray popup needs a foreground owner so USER32 can dismiss it
    // correctly, but it must not synchronously send WM_COMMAND while the
    // TrackPopupMenu modal loop is still active. Opening another top-level
    // window from inside that nested menu loop caused foreground ownership to
    // bounce once more when the menu unwound, which appeared as a location/
    // activation flash.
    SetForegroundWindow(hwnd_);

    const UINT selected =
        static_cast<UINT>(
            TrackPopupMenu(
                menu,
                TPM_RIGHTBUTTON |
                    TPM_BOTTOMALIGN |
                    TPM_LEFTALIGN |
                    TPM_RETURNCMD |
                    TPM_NONOTIFY,
                point.x,
                point.y,
                0,
                hwnd_,
                nullptr));

    DestroyMenu(menu);

    if (selected != 0) {
        // Fully unwind both the menu modal loop and the tray callback before
        // creating/activating Settings, About or Shortcut Manager. WM_NULL is
        // the documented tray-menu dismissal nudge; FIFO ordering guarantees
        // it is processed before our deferred command. The deferred command
        // then reclaims the launcher as the foreground handoff source and
        // activates the destination before hiding the launcher.
        PostMessageW(
            hwnd_,
            WM_NULL,
            0,
            0);

        PostMessageW(
            hwnd_,
            WM_COMMAND,
            MAKEWPARAM(
                selected,
                0),
            0);
    }
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
    HWND hwnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam) {

    if (message ==
            WM_IME_STARTCOMPOSITION) {
        CommitPendingNumericIntentAsText();
        imeComposing_ = true;
    } else if (
        message ==
            WM_IME_ENDCOMPOSITION) {
        imeComposing_ = false;
    }

    if (message == WM_KILLFOCUS) CancelPendingNumericIntent();
    if (message == WM_LBUTTONDOWN || message == WM_RBUTTONDOWN ||
        message == WM_CUT || message == WM_CLEAR)
        CommitPendingNumericIntentAsText();

    if (message == WM_CHAR && !imeComposing_ &&
        (wParam == L'\r' || wParam == L'\t' || wParam == 27)) {
        return 0;
    }

    if (message == WM_CHAR ||
        message == WM_SYSCHAR) {
        if (consumedNumericChar_ != 0 &&
            static_cast<wchar_t>(
                wParam) ==
                consumedNumericChar_) {
            return 0;
        }

        if (message == WM_CHAR && wParam >= L' ') {
            CommitPendingNumericIntentAsText();
            lastTextInputTick_ =
                GetTickCount64();
        }
    }

    if (message == WM_PASTE &&
        pendingNumericIntent_.active) {
        // A second input action inside the grace window is definitive typing.
        CommitPendingNumericIntentAsText();
    }

    if (message == WM_KEYUP ||
        message == WM_SYSKEYUP) {
        if (consumedNumericVirtualKey_ ==
            static_cast<UINT>(
                wParam)) {
            consumedNumericVirtualKey_ = 0;
            consumedNumericChar_ = 0;
        }

        return CallWindowProcW(
            oldEditProc_,
            hwnd,
            message,
            wParam,
            lParam);
    }

    if (message == WM_KEYDOWN ||
        message == WM_SYSKEYDOWN) {

        const bool firstPress =
            (lParam &
             (static_cast<LPARAM>(1)
              << 30)) == 0;

        if (pendingNumericIntent_.active) {
            if (static_cast<UINT>(
                    wParam) ==
                    pendingNumericIntent_
                        .virtualKey &&
                !firstPress) {
                // Holding the candidate digit must never turn one pending
                // numbered launch into repeated text or repeated launches.
                return 0;
            }

            switch (wParam) {
            case VK_SHIFT: case VK_CONTROL: case VK_MENU: case VK_LWIN: case VK_RWIN:
                // A modifier alone gives no evidence of typing or launch intent.
                return CallWindowProcW(oldEditProc_, hwnd, message, wParam, lParam);
            case VK_ESCAPE: case VK_RETURN: case VK_TAB:
            case VK_UP: case VK_DOWN: case VK_PRIOR: case VK_NEXT:
                CancelPendingNumericIntent();
                break;
            default:
                // Text/editing keys retain the digit before applying the new action.
                CommitPendingNumericIntentAsText();
                break;
            }
        }

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

        const std::string
            keyName =
                hotkey::KeyName(
                    static_cast<UINT>(
                        wParam));

        if (!keyName.empty()) {
            const auto actionId =
                MatchHotkeyAction(
                    app_.SettingsData()
                        .hotkeyBindings,
                    HotkeyScope::Launcher,
                    keyName,
                    controlDown,
                    altDown,
                    shiftDown,
                    winDown);

            if (actionId) {
                if (*actionId ==
                    hotkey_actions::
                        kOpenSettings) {
                    PrepareTopLevelForegroundHandoff();
                    app_.ShowSettings();
                    Hide();
                } else if (
                    *actionId ==
                    hotkey_actions::
                        kExitApplication) {
                    DestroyWindow(hwnd_);
                } else if (
                    *actionId ==
                    hotkey_actions::
                        kNavigateCurrentFileManager) {
                    ExecuteSelection(
                        LauncherExecutionIntent::
                            NavigateCurrentFileManager);
                } else if (
                    *actionId ==
                    hotkey_actions::
                        kCopySelectedTarget) {
                    ExecuteSelection(
                        LauncherExecutionIntent::
                            CopySelectedText);
                }

                return 0;
            }
        }

        const int quickLaunchIndex =
            QuickLaunchIndexForKey(
                wParam);
        const int digit =
            NumericDigitForKey(
                wParam);

        if (quickLaunchIndex >= 0 &&
            digit >= 0) {

            if (!firstPress &&
                consumedNumericVirtualKey_ ==
                    static_cast<UINT>(
                        wParam)) {
                return 0;
            }

            const bool resultAvailable =
                static_cast<std::size_t>(
                    quickLaunchIndex) <
                results_.size();

            const std::wstring query =
                CurrentQuery();

            bool strongContinuation =
                false;

            if (!query.empty() &&
                !controlDown &&
                !altDown &&
                !shiftDown &&
                !winDown &&
                !imeComposing_) {
                strongContinuation =
                    HasStrongNumericContinuation(
                        static_cast<wchar_t>(
                            L'0' + digit));
            }

            classic_behavior::
                NumericQuickLaunchContext
                    context{};
            context.enabled =
                app_.SettingsData()
                    .numericQuickLaunch;
            context.imeComposing =
                imeComposing_;
            context.controlDown =
                controlDown;
            context.altDown =
                altDown;
            context.shiftDown =
                shiftDown;
            context.winDown =
                winDown;
            context.queryEmpty =
                query.empty();
            context.recentTextInput =
                HasRecentTextInput();
            context.strongContinuation =
                strongContinuation;
            context.resultAvailable = resultAvailable;
            DWORD selectionStart = 0, selectionEnd = 0;
            SendMessageW(edit_, EM_GETSEL, reinterpret_cast<WPARAM>(&selectionStart),
                reinterpret_cast<LPARAM>(&selectionEnd));
            context.editingText = selectionStart != selectionEnd || selectionEnd != query.size() ||
                relevance::HasExplicitSyntax(query) || query.find_first_of(L"|!<>\"") != std::wstring::npos;

            const auto decision =
                classic_behavior::
                    DecideNumericQuickLaunch(
                        context);

            if (decision ==
                    classic_behavior::
                        NumericQuickLaunchDecision::
                            ExecuteNow) {
                if (firstPress &&
                    resultAvailable) {
                    ConsumeNumericKey(
                        static_cast<UINT>(
                            wParam),
                        static_cast<wchar_t>(
                            L'0' + digit));

                    const LauncherResult
                        snapshot =
                            results_[
                                static_cast<
                                    std::size_t>(
                                        quickLaunchIndex)];

                    ExecuteResultSnapshot(
                        snapshot);
                }
                return 0;
            }

            if (decision ==
                    classic_behavior::
                        NumericQuickLaunchDecision::
                            DeferExecute) {
                if (firstPress &&
                    resultAvailable) {
                    QueuePendingNumericIntent(
                        static_cast<UINT>(
                            wParam),
                        static_cast<wchar_t>(
                            L'0' + digit),
                        results_[
                            static_cast<
                                std::size_t>(
                                    quickLaunchIndex)]);
                }
                return 0;
            }
        }

        switch (wParam) {
        case VK_DOWN:
            MoveSelection(1);
            return 0;
        case VK_UP:
            MoveSelection(-1);
            return 0;
        case VK_RETURN:
            if (!controlDown &&
                !altDown &&
                !winDown) {
                ExecuteSelection(
                    LauncherExecutionIntent::
                        Default);
            }
            return 0;
        case VK_TAB:
            MoveSelection(
                shiftDown
                    ? -1
                    : 1);
            return 0;
        case VK_ESCAPE:
            Hide();
            return 0;
        default:
            break;
        }
    }

    return CallWindowProcW(
        oldEditProc_,
        hwnd,
        message,
        wParam,
        lParam);
}

LRESULT LauncherWindow::HandleMessage(
    UINT message, WPARAM wParam, LPARAM lParam) {

    if (taskbarCreatedMessage_ != 0 &&
        message == taskbarCreatedMessage_) {
        const bool restorePersistentIcon =
            app_.SettingsData().showTrayIcon;

        trayIconAdded_ = false;
        notificationOnlyTrayIcon_ = false;

        if (restorePersistentIcon) {
            AddTrayIcon();
        }
        return 0;
    }

    switch (message) {
    case WM_TIMER:
        if (wParam ==
            kNumericIntentTimerId) {
            ExecutePendingNumericIntent();
            return 0;
        }
        break;

    case WM_NCHITTEST: {
        POINT point{
            GET_X_LPARAM(lParam),
            GET_Y_LPARAM(lParam),
        };
        ScreenToClient(
            hwnd_,
            &point);

        if (!IsModern()) {
            const RECT close =
                ClassicCloseRect();

            if (PtInRect(
                    &close,
                    point)) {
                return HTCLIENT;
            }

            if (point.y >= 0 &&
                point.y <
                    classicDpiMetrics_
                        .dragHeight) {
                return HTCAPTION;
            }
        } else if (
            point.y >= 0 &&
            point.y < DpiScale(10)) {
            // Modern Compact intentionally keeps only a very small drag
            // strip so the input remains the primary interaction target.
            return HTCAPTION;
        }
        break;
    }

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
            // A query edit starts a new ranking decision. Rebuild the rows
            // atomically and select the new best match only after redraw is
            // suspended; visibly clearing the old selection first caused a
            // transient Classic repaint on every keystroke.
            //
            // IME composition can emit intermediate EN_CHANGE events. Search
            // may update live, but single-result auto execution must wait
            // until composition is committed.
            RefreshResults(
                !imeComposing_ &&
                    !numericTextCommitInProgress_,
                false);
            return 0;
        }
        if (LOWORD(wParam) == 1002 && HIWORD(wParam) == LBN_DBLCLK) {
            ExecuteSelection();
            return 0;
        }
        if (LOWORD(wParam) == 1002 && HIWORD(wParam) == LBN_SELCHANGE) {
            UpdatePreview();

            if (IsModern() &&
                edit_ &&
                GetFocus() != edit_) {
                // Result clicks change selection, not the launcher's typing
                // destination. Return focus to the native EDIT so the next
                // keystroke immediately refines the query instead of invoking
                // LISTBOX type-to-select behavior.
                SetFocus(edit_);
            }
            return 0;
        }

        switch (LOWORD(wParam)) {
        case kMenuShow:
            Show();
            return 0;
        case kMenuShortcuts:
            PrepareTopLevelForegroundHandoff();
            app_.ShowShortcutManager();
            Hide();
            return 0;
        case kMenuSettings:
            PrepareTopLevelForegroundHandoff();
            app_.ShowSettings();
            Hide();
            return 0;
        case kMenuAbout:
            PrepareTopLevelForegroundHandoff();
            app_.ShowAbout();
            Hide();
            return 0;
        case kMenuExit:
            DestroyWindow(hwnd_);
            return 0;
        default:
            break;
        }
        break;

    case WM_COPYDATA: {
        const auto* copyData =
            reinterpret_cast<
                const COPYDATASTRUCT*>(
                    lParam);

        if (!copyData ||
            copyData->dwData !=
                instance_ipc::
                    kAddShortcutCopyData ||
            !copyData->lpData ||
            copyData->cbData <
                sizeof(wchar_t) ||
            copyData->cbData %
                sizeof(wchar_t) != 0 ||
            copyData->cbData >
                32768 *
                    sizeof(wchar_t)) {
            return FALSE;
        }

        const auto* text =
            static_cast<
                const wchar_t*>(
                    copyData->lpData);
        const std::size_t count =
            copyData->cbData /
            sizeof(wchar_t);

        std::size_t length = 0;
        while (length < count &&
               text[length] != L'\0') {
            ++length;
        }

        if (length == 0 ||
            length == count) {
            return FALSE;
        }

        QueueNewShortcutForPath(
            std::wstring(
                text,
                length));
        return TRUE;
    }

    case WM_CLOSE:
        DestroyWindow(hwnd_);
        return 0;

    case WM_CONTEXTMENU: {
        const HWND target =
            reinterpret_cast<HWND>(
                wParam);

        const bool keyboardInvocation =
            GET_X_LPARAM(lParam) == -1 &&
            GET_Y_LPARAM(lParam) == -1;

        if (target == list_ ||
            (target == edit_ &&
             keyboardInvocation)) {
            POINT point{
                GET_X_LPARAM(lParam),
                GET_Y_LPARAM(lParam),
            };
            ShowResultContextMenu(
                point);
            return 0;
        }
        break;
    }

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
        const auto palette =
            CurrentPalette();
        const HWND control =
            reinterpret_cast<HWND>(
                lParam);
        HDC dc =
            reinterpret_cast<HDC>(
                wParam);

        if (!IsModern()) {
            SetBkColor(
                dc,
                palette.accentBackground);

            SetTextColor(
                dc,
                control ==
                        classicPreview_
                    ? RGB(128, 128, 128)
                    : RGB(255, 0, 0));

            return reinterpret_cast<LRESULT>(
                accentBrush_);
        }

        SetTextColor(
            dc,
            palette.text);
        SetBkColor(
            dc,
            palette.controlBackground);

        return reinterpret_cast<LRESULT>(
            controlBrush_);
    }

    case WM_CTLCOLORLISTBOX: {
        const auto palette =
            CurrentPalette();
        HDC dc =
            reinterpret_cast<HDC>(
                wParam);

        SetTextColor(
            dc,
            IsModern()
                ? palette.text
                : RGB(0, 0, 128));
        SetBkColor(
            dc,
            IsModern()
                ? palette.accentBackground
                : GetSysColor(
                      COLOR_WINDOW));

        return reinterpret_cast<LRESULT>(
            IsModern()
                ? accentBrush_
                : controlBrush_);
    }

    case WM_CTLCOLORSTATIC: {
        const auto palette =
            CurrentPalette();
        const HWND control =
            reinterpret_cast<HWND>(
                lParam);
        HDC dc =
            reinterpret_cast<HDC>(
                wParam);

        if (control ==
            preview_) {
            SetTextColor(
                dc,
                palette.mutedText);
            SetBkColor(
                dc,
                IsModern()
                    ? palette.bottomBackground
                    : palette.windowBackground);
            return reinterpret_cast<LRESULT>(
                IsModern()
                    ? bottomBrush_
                    : windowBrush_);
        }

        break;
    }

    case WM_DRAWITEM: {
        const auto* item =
            reinterpret_cast<DRAWITEMSTRUCT*>(
                lParam);

        if (item->CtlID == 1005 &&
            !IsModern()) {
            FillRect(
                item->hDC,
                &item->rcItem,
                bottomBrush_);

            const int length =
                GetWindowTextLengthW(
                    classicPreview_);
            std::wstring commandText(
                static_cast<std::size_t>(
                    length) + 1,
                L'\0');

            if (length > 0) {
                GetWindowTextW(
                    classicPreview_,
                    commandText.data(),
                    length + 1);
            }

            commandText.resize(
                static_cast<std::size_t>(
                    length));

            RECT textRect =
                item->rcItem;
            textRect.left +=
                DpiScale(2);
            textRect.right -=
                DpiScale(2);

            HGDIOBJ oldFont =
                SelectObject(
                    item->hDC,
                    auxiliaryFont_);

            SetBkMode(
                item->hDC,
                TRANSPARENT);
            SetTextColor(
                item->hDC,
                RGB(128, 128, 128));

            DrawTextW(
                item->hDC,
                commandText.c_str(),
                -1,
                &textRect,
                DT_SINGLELINE |
                    DT_VCENTER |
                    DT_PATH_ELLIPSIS |
                    DT_NOPREFIX);

            SelectObject(
                item->hDC,
                oldFont);

            return TRUE;
        }

        if (item->CtlID != 1002 ||
            item->itemID == static_cast<UINT>(-1) ||
            item->itemID >= results_.size()) {
            break;
        }

        const auto palette = CurrentPalette();
        const bool selected = (item->itemState & ODS_SELECTED) != 0;

        if (IsModern()) {
            FillRect(
                item->hDC,
                &item->rcItem,
                accentBrush_);

            if (selected) {
                const auto& modern =
                    modernDpiMetrics_;

                RECT selectionRect =
                    item->rcItem;
                selectionRect.left +=
                    modern.rowSelectionInsetX;
                selectionRect.right -=
                    modern.rowSelectionInsetX;
                selectionRect.top +=
                    modern.rowSelectionInsetY;
                selectionRect.bottom -=
                    modern.rowSelectionInsetY;

                HGDIOBJ oldPen =
                    SelectObject(
                        item->hDC,
                        GetStockObject(NULL_PEN));
                HGDIOBJ oldBrush =
                    SelectObject(
                        item->hDC,
                        selectionBrush_);

                RoundRect(
                    item->hDC,
                    selectionRect.left,
                    selectionRect.top,
                    selectionRect.right,
                    selectionRect.bottom,
                    modern.rowCornerDiameter,
                    modern.rowCornerDiameter);

                SelectObject(
                    item->hDC,
                    oldBrush);
                SelectObject(
                    item->hDC,
                    oldPen);

                RECT accentRect{
                    selectionRect.left +
                        modern.selectionAccentInset,
                    selectionRect.top +
                        modern.selectionAccentInset,
                    selectionRect.left +
                        modern.selectionAccentInset +
                        modern.selectionAccentWidth,
                    selectionRect.bottom -
                        modern.selectionAccentInset,
                };

                if (accentRect.bottom >
                    accentRect.top) {
                    HGDIOBJ oldAccentPen =
                        SelectObject(
                            item->hDC,
                            GetStockObject(NULL_PEN));
                    HGDIOBJ oldAccentBrush =
                        SelectObject(
                            item->hDC,
                            focusAccentBrush_);
                    RoundRect(
                        item->hDC,
                        accentRect.left,
                        accentRect.top,
                        accentRect.right,
                        accentRect.bottom,
                        modern.selectionAccentWidth,
                        modern.selectionAccentWidth);
                    SelectObject(
                        item->hDC,
                        oldAccentBrush);
                    SelectObject(
                        item->hDC,
                        oldAccentPen);
                }
            }
        } else {
            FillRect(
                item->hDC,
                &item->rcItem,
                GetSysColorBrush(
                    selected
                        ? COLOR_HIGHLIGHT
                        : COLOR_WINDOW));
        }

        SetBkMode(
            item->hDC,
            TRANSPARENT);

        const auto& result =
            results_[item->itemID];

        const std::wstring primary =
            PrimaryResultText(result);

        if (IsModern()) {
            const auto& modern =
                modernDpiMetrics_;
            const std::wstring modernPrimary =
                ModernPrimaryResultText(
                    result);
            const std::wstring modernSecondary =
                ModernSecondaryResultText(
                    result);

            const int contentLeft =
                item->rcItem.left +
                modern.rowSelectionInsetX +
                modern.rowTextInset;
            const int rowRight =
                item->rcItem.right -
                modern.rowSelectionInsetX -
                modern.rowTextInset;
            const bool showNumericShortcut =
                app_.SettingsData()
                    .numericQuickLaunch &&
                item->itemID < 10;

            // Keep one stable right-hand action gutter for every row. The
            // selected row uses it for the Enter-style return arrow; the
            // remaining rows use a compact middle-dot + digit shortcut hint.
            RECT shortcutHintRect =
                item->rcItem;
            shortcutHintRect.right =
                rowRight;
            shortcutHintRect.left =
                std::max(
                    contentLeft,
                    rowRight -
                        modern.shortcutHintWidth);

            const int contentRight =
                std::max(
                    contentLeft,
                    static_cast<int>(
                        shortcutHintRect.left) -
                        modern.shortcutHintGap);

            RECT primaryRect =
                item->rcItem;
            primaryRect.left =
                contentLeft;
            primaryRect.right =
                contentRight;

            RECT secondaryRect =
                item->rcItem;
            secondaryRect.left =
                contentRight;
            secondaryRect.right =
                contentRight;

            const auto oldFont =
                SelectObject(
                    item->hDC,
                    boldFont_);

            if (!modernSecondary.empty()) {
                SIZE primarySize{};
                GetTextExtentPoint32W(
                    item->hDC,
                    modernPrimary.c_str(),
                    static_cast<int>(
                        modernPrimary.size()),
                    &primarySize);

                const int available =
                    std::max(
                        0,
                        contentRight -
                            contentLeft);
                const int primaryMax =
                    std::max(
                        DpiScale(96),
                        available -
                            modern.secondaryMinWidth -
                            modern.rowColumnGap);
                const int measuredPrimaryWidth =
                    static_cast<int>(
                        primarySize.cx);
                const int primaryLeft =
                    static_cast<int>(
                        primaryRect.left);
                const int primaryWidth =
                    std::min(
                        measuredPrimaryWidth +
                            DpiScale(2),
                        primaryMax);
                const int primaryRight =
                    std::min(
                        contentRight,
                        primaryLeft +
                            primaryWidth);
                const int secondaryLeft =
                    std::min(
                        contentRight,
                        primaryRight +
                            modern.rowColumnGap);

                primaryRect.right =
                    static_cast<LONG>(
                        primaryRight);
                secondaryRect.left =
                    static_cast<LONG>(
                        secondaryLeft);
            }
            SetTextColor(
                item->hDC,
                selected
                    ? palette.selectionText
                    : palette.text);

            DrawTextW(
                item->hDC,
                modernPrimary.c_str(),
                -1,
                &primaryRect,
                DT_SINGLELINE |
                    DT_VCENTER |
                    DT_END_ELLIPSIS |
                    DT_NOPREFIX);

            if (!modernSecondary.empty()) {
                SelectObject(
                    item->hDC,
                    auxiliaryFont_);
                SetTextColor(
                    item->hDC,
                    palette.mutedText);

                DrawTextW(
                    item->hDC,
                    modernSecondary.c_str(),
                    -1,
                    &secondaryRect,
                    DT_SINGLELINE |
                        DT_LEFT |
                        DT_VCENTER |
                        DT_END_ELLIPSIS |
                        DT_NOPREFIX);
            }

            if (selected) {
                SelectObject(
                    item->hDC,
                    shortcutArrowFont_);
                SetTextColor(
                    item->hDC,
                    palette.accent);

                DrawTextW(
                    item->hDC,
                    L"↩",
                    1,
                    &shortcutHintRect,
                    DT_SINGLELINE |
                        DT_RIGHT |
                        DT_VCENTER |
                        DT_NOPREFIX);
            } else if (
                showNumericShortcut) {
                const std::wstring number =
                    ResultNumberLabel(
                        item->itemID);

                SelectObject(
                    item->hDC,
                    shortcutHintFont_);
                SetTextColor(
                    item->hDC,
                    palette.mutedText);

                SIZE numberSize{};
                GetTextExtentPoint32W(
                    item->hDC,
                    number.c_str(),
                    static_cast<int>(
                        number.size()),
                    &numberSize);

                RECT numberRect =
                    shortcutHintRect;

                DrawTextW(
                    item->hDC,
                    number.c_str(),
                    -1,
                    &numberRect,
                    DT_SINGLELINE |
                        DT_RIGHT |
                        DT_VCENTER |
                        DT_NOPREFIX);

                RECT chevronRect =
                    shortcutHintRect;
                chevronRect.right =
                    std::max(
                        chevronRect.left,
                        shortcutHintRect.right -
                            static_cast<int>(
                                numberSize.cx) -
                            DpiScale(3));

                DrawTextW(
                    item->hDC,
                    L"›",
                    1,
                    &chevronRect,
                    DT_SINGLELINE |
                        DT_RIGHT |
                        DT_VCENTER |
                        DT_NOPREFIX);
            }

            SelectObject(
                item->hDC,
                oldFont);
            return TRUE;
        }

        constexpr int
            textInsetLogical = 4;

        const auto& classicMetrics =
            classicDpiMetrics_;

        const int firstX =
            item->rcItem.left +
            classicMetrics.numberDividerX;
        const int secondX =
            item->rcItem.left +
            classicMetrics.shortcutDividerX;

        RECT numberRect =
            item->rcItem;
        numberRect.right =
            firstX;

        RECT keywordRect =
            item->rcItem;
        keywordRect.left =
            firstX +
            DpiScale(
                textInsetLogical);
        keywordRect.right =
            secondX -
            DpiScale(
                textInsetLogical);

        RECT titleRect =
            item->rcItem;
        titleRect.left =
            secondX +
            DpiScale(
                textInsetLogical);
        titleRect.right -=
            DpiScale(
                textInsetLogical);

        const std::wstring number =
            ResultNumberLabel(
                item->itemID);

        HGDIOBJ oldFont =
            SelectObject(
                item->hDC,
                normalFont_);

        const COLORREF foreground =
            selected
                ? GetSysColor(
                      COLOR_HIGHLIGHTTEXT)
                : RGB(0, 0, 128);

        SetTextColor(
            item->hDC,
            foreground);

        constexpr UINT
            classicTextFlags =
                DT_SINGLELINE |
                DT_VCENTER |
                DT_END_ELLIPSIS |
                DT_NOPREFIX;

        DrawTextW(
            item->hDC,
            number.c_str(),
            -1,
            &numberRect,
            DT_SINGLELINE |
                DT_CENTER |
                DT_VCENTER |
                DT_NOPREFIX);

        DrawTextW(
            item->hDC,
            primary.c_str(),
            -1,
            &keywordRect,
            classicTextFlags);

        DrawTextW(
            item->hDC,
            result.subtitle.c_str(),
            -1,
            &titleRect,
            classicTextFlags);

        const COLORREF
            separatorColor =
                selected
                    ? GetSysColor(
                          COLOR_HIGHLIGHTTEXT)
                    : RGB(0, 0, 128);

        HBRUSH separatorBrush =
            static_cast<HBRUSH>(
                GetStockObject(
                    DC_BRUSH));
        const COLORREF oldDcBrushColor =
            SetDCBrushColor(
                item->hDC,
                separatorColor);

        RECT firstSeparator{
            firstX,
            item->rcItem.top,
            firstX +
                ui::kClassicSeparatorPhysicalThickness,
            item->rcItem.bottom,
        };
        RECT secondSeparator{
            secondX,
            item->rcItem.top,
            secondX +
                ui::kClassicSeparatorPhysicalThickness,
            item->rcItem.bottom,
        };

        FillRect(
            item->hDC,
            &firstSeparator,
            separatorBrush);
        FillRect(
            item->hDC,
            &secondSeparator,
            separatorBrush);

        if (oldDcBrushColor !=
            CLR_INVALID) {
            SetDCBrushColor(
                item->hDC,
                oldDcBrushColor);
        }

        SelectObject(
            item->hDC,
            oldFont);

        return TRUE;
    }

    case WM_EXITSIZEMOVE: {
        RECT rect{};
        if (GetWindowRect(
                hwnd_,
                &rect)) {
            app_.RememberLauncherPosition(
                rect.left,
                rect.top);
        }
        return 0;
    }

    case WM_DPICHANGED: {
        dpi_ = HIWORD(wParam);
        classicDpiMetrics_ =
            ui::ClassicLauncherMetricsForDpi(
                dpi_);
        modernDpiMetrics_ =
            ui::ModernCompactLauncherMetricsForDpi(
                dpi_,
                modernLayoutRows_);
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
            !contextActionModalActive_) {
            Hide();
        }
        break;

    case kShortcutIpcMessage:
        ProcessPendingShortcutPaths();
        return 0;

    case kTrayMessage: {
        const UINT event =
            LOWORD(lParam);

        if (event ==
            NIN_BALLOONUSERCLICK) {
            const bool temporary =
                notificationOnlyTrayIcon_;
            Show();
            if (temporary) {
                RemoveTrayIcon();
            }
            return 0;
        }

        if ((event ==
                 NIN_BALLOONHIDE ||
             event ==
                 NIN_BALLOONTIMEOUT) &&
            notificationOnlyTrayIcon_) {
            RemoveTrayIcon();
            return 0;
        }

        if (event ==
                WM_LBUTTONDBLCLK ||
            event ==
                NIN_KEYSELECT) {
            Show();
            return 0;
        }

        if (event == WM_RBUTTONUP ||
            event == WM_CONTEXTMENU) {
            POINT point{};
            GetCursorPos(&point);
            ShowTrayMenu(point);
            return 0;
        }
        break;
    }

    case WM_DESTROY:
        CancelPendingNumericIntent();
        RemoveTrayIcon();
        hwnd_ = nullptr;
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hwnd_, message, wParam, lParam);
}

} // namespace altrun
