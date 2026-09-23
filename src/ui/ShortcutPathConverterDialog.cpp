#include "ShortcutPathConverterDialog.hpp"

#include "TopLevelWindowPresentation.hpp"
#include "UiMetrics.hpp"
#include "UiTheme.hpp"
#include "UiTypography.hpp"

#include "../app/App.hpp"
#include "../core/Command.hpp"
#include "../core/UserCommandStore.hpp"
#include "../platform/WinUtil.hpp"

#include <commctrl.h>
#include <uxtheme.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <iterator>
#include <optional>

namespace altrun {

namespace {

constexpr wchar_t kPathConverterClass[] =
    L"ALTRunNext.ShortcutPathConverter";

constexpr UINT kIdPortable = 54101;
constexpr UINT kIdAbsolute = 54102;
constexpr UINT kIdRescan = 54103;
constexpr UINT kIdList = 54104;
constexpr UINT kIdApply = 54105;

constexpr int kDefaultWidthLogical = 960;
constexpr int kDefaultHeightLogical = 560;
constexpr int kMinimumWidthLogical = 820;
constexpr int kMinimumHeightLogical = 480;

constexpr int kFieldColumnPercent = 13;
constexpr int kCurrentColumnPercent = 36;
constexpr int kConvertedColumnPercent = 39;
constexpr int kFieldColumnMinimumLogical = 100;
constexpr int kCurrentColumnMinimumLogical = 180;
constexpr int kConvertedColumnMinimumLogical = 180;
constexpr int kStatusColumnMinimumLogical = 100;
constexpr int kResultRowHeightLogical = 24;

constexpr LPARAM kGroupHeaderItemParam =
    static_cast<LPARAM>(-1);

struct SelectorRasterTemplate {
    int diameter{};
    double ringThickness{};
    int dotDiameter{};
};

[[nodiscard]] SelectorRasterTemplate
SelectorTemplateForDpi(
    UINT dpi) noexcept {
    // Hand-tuned physical-pixel templates. The selected template is rendered
    // directly into the destination HDC; no theme glyph or bitmap scaling is
    // involved after this point.
    if (dpi <= 108) {
        return {15, 1.35, 5};
    }

    if (dpi <= 132) {
        return {17, 1.45, 6};
    }

    if (dpi <= 156) {
        return {19, 1.60, 7};
    }

    if (dpi <= 180) {
        return {21, 1.75, 8};
    }

    return {23, 1.90, 9};
}

[[nodiscard]] COLORREF BlendSelectorPixel(
    COLORREF background,
    COLORREF foreground,
    double coverage) noexcept {
    coverage =
        std::clamp(
            coverage,
            0.0,
            1.0);

    const auto blendChannel =
        [coverage](
            int backgroundValue,
            int foregroundValue) {
            return static_cast<int>(
                std::lround(
                    static_cast<double>(
                        backgroundValue) *
                        (1.0 - coverage) +
                    static_cast<double>(
                        foregroundValue) *
                        coverage));
        };

    return RGB(
        blendChannel(
            GetRValue(background),
            GetRValue(foreground)),
        blendChannel(
            GetGValue(background),
            GetGValue(foreground)),
        blendChannel(
            GetBValue(background),
            GetBValue(foreground)));
}

[[nodiscard]] double CircleCoverage(
    int pixelX,
    int pixelY,
    double center,
    double radius) noexcept {
    constexpr int kSamplesPerAxis = 4;
    constexpr int kSampleCount =
        kSamplesPerAxis *
        kSamplesPerAxis;

    int inside = 0;

    for (int sampleY = 0;
         sampleY < kSamplesPerAxis;
         ++sampleY) {
        for (int sampleX = 0;
             sampleX < kSamplesPerAxis;
             ++sampleX) {
            const double x =
                static_cast<double>(
                    pixelX) +
                (static_cast<double>(
                     sampleX) +
                 0.5) /
                    kSamplesPerAxis;
            const double y =
                static_cast<double>(
                    pixelY) +
                (static_cast<double>(
                     sampleY) +
                 0.5) /
                    kSamplesPerAxis;

            const double dx =
                x - center;
            const double dy =
                y - center;

            if (dx * dx +
                    dy * dy <=
                radius * radius) {
                ++inside;
            }
        }
    }

    return static_cast<double>(
               inside) /
        static_cast<double>(
            kSampleCount);
}

void DrawSelectorRaster(
    HDC dc,
    int left,
    int top,
    const SelectorRasterTemplate& raster,
    bool selected,
    COLORREF background,
    COLORREF ringColor,
    COLORREF dotColor) {
    if (!dc ||
        raster.diameter <= 0) {
        return;
    }

    const double center =
        static_cast<double>(
            raster.diameter) /
        2.0;
    const double outerRadius =
        static_cast<double>(
            raster.diameter) /
            2.0 -
        0.5;
    const double innerRadius =
        std::max(
            0.0,
            outerRadius -
                raster.ringThickness);
    const double dotRadius =
        static_cast<double>(
            raster.dotDiameter) /
            2.0;

    for (int y = 0;
         y < raster.diameter;
         ++y) {
        for (int x = 0;
             x < raster.diameter;
             ++x) {
            const double outerCoverage =
                CircleCoverage(
                    x,
                    y,
                    center,
                    outerRadius);
            const double innerCoverage =
                CircleCoverage(
                    x,
                    y,
                    center,
                    innerRadius);
            const double ringCoverage =
                std::clamp(
                    outerCoverage -
                        innerCoverage,
                    0.0,
                    1.0);

            COLORREF color =
                BlendSelectorPixel(
                    background,
                    ringColor,
                    ringCoverage);

            if (selected) {
                const double dotCoverage =
                    CircleCoverage(
                        x,
                        y,
                        center,
                        dotRadius);

                color =
                    BlendSelectorPixel(
                        color,
                        dotColor,
                        dotCoverage);
            }

            if (color != background) {
                SetPixelV(
                    dc,
                    left + x,
                    top + y,
                    color);
            }
        }
    }
}

} // namespace

ShortcutPathConverterDialog::
ShortcutPathConverterDialog(
    App& app,
    HINSTANCE instance,
    HWND owner)
    : app_(app),
      instance_(instance),
      owner_(owner) {}

ShortcutPathConverterDialog::
~ShortcutPathConverterDialog() {
    CloseWindow();

    if (font_) {
        DeleteObject(font_);
        font_ = nullptr;
    }

    if (groupFont_) {
        DeleteObject(groupFont_);
        groupFont_ = nullptr;
    }

    if (rowHeightImageList_) {
        ImageList_Destroy(
            rowHeightImageList_);
        rowHeightImageList_ = nullptr;
    }
}

void ShortcutPathConverterDialog::
CloseWindow() {
    if (!hwnd_ ||
        !IsWindow(hwnd_)) {
        return;
    }

    window_presentation::
        HideForDestroy(
            hwnd_);
    DestroyWindow(hwnd_);
}

bool ShortcutPathConverterDialog::Show(
    App& app,
    HINSTANCE instance,
    HWND owner) {
    ShortcutPathConverterDialog dialog(
        app,
        instance,
        owner);

    if (!dialog.Create()) {
        MessageBoxW(
            owner,
            app.SettingsData().language ==
                    Language::ZhCN
                ? L"无法创建路径转换窗口。"
                : L"Could not create the path conversion window.",
            L"ALTRun Next",
            MB_OK | MB_ICONERROR);
        return false;
    }

    return dialog.RunModal();
}

const wchar_t*
ShortcutPathConverterDialog::T(
    const wchar_t* zh,
    const wchar_t* en) const {
    return app_.SettingsData().language ==
            Language::ZhCN
        ? zh
        : en;
}

int ShortcutPathConverterDialog::Scale(
    int value) const {
    return ui::Scale(
        value,
        dpi_);
}

bool ShortcutPathConverterDialog::Create() {
    INITCOMMONCONTROLSEX controls{
        sizeof(controls),
        ICC_STANDARD_CLASSES |
            ICC_LISTVIEW_CLASSES,
    };
    InitCommonControlsEx(&controls);

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.hInstance = instance_;
    wc.lpfnWndProc = WindowProc;
    wc.lpszClassName =
        kPathConverterClass;
    wc.hCursor =
        LoadCursorW(nullptr, IDC_ARROW);
    wc.hIcon =
        LoadIconW(nullptr, IDI_APPLICATION);
    wc.hbrBackground =
        reinterpret_cast<HBRUSH>(
            COLOR_WINDOW + 1);

    if (!RegisterClassExW(&wc) &&
        GetLastError() !=
            ERROR_CLASS_ALREADY_EXISTS) {
        return false;
    }

    const auto creation =
        window_presentation::
            ResolveOwnedPopupGeometry(
                owner_,
                instance_,
                kDefaultWidthLogical,
                kDefaultHeightLogical);

    hwnd_ = CreateWindowExW(
        WS_EX_DLGMODALFRAME |
            WS_EX_CONTROLPARENT,
        kPathConverterClass,
        L"",
        WS_POPUP |
            WS_CAPTION |
            WS_SYSMENU |
            WS_THICKFRAME |
            WS_CLIPCHILDREN,
        creation.outer.left,
        creation.outer.top,
        creation.outer.right -
            creation.outer.left,
        creation.outer.bottom -
            creation.outer.top,
        owner_,
        nullptr,
        instance_,
        this);

    if (!hwnd_) {
        return false;
    }

    window_presentation::Configure(
        hwnd_);

    dpi_ = GetDpiForWindow(hwnd_);

    CreateControls();
    ApplyLanguage();
    Layout();
    Scan();

    window_presentation::
        CenterExistingWindow(
            hwnd_,
            owner_);

    return true;
}

bool ShortcutPathConverterDialog::RunModal() {
    if (owner_) {
        EnableWindow(owner_, FALSE);
    }

    window_presentation::
        RevealFullyPainted(
            hwnd_,
            SW_SHOW);
    SetForegroundWindow(hwnd_);

    MSG msg{};
    bool sawQuit = false;
    int quitCode = 0;

    while (!closed_) {
        const BOOL result =
            GetMessageW(
                &msg,
                nullptr,
                0,
                0);

        if (result == 0) {
            sawQuit = true;
            quitCode =
                static_cast<int>(
                    msg.wParam);
            break;
        }

        if (result < 0) {
            break;
        }

        if (msg.message == WM_KEYDOWN &&
            (msg.wParam == VK_LEFT ||
             msg.wParam == VK_RIGHT) &&
            (msg.hwnd == portable_ ||
             msg.hwnd == absolute_)) {
            const Mode requested =
                msg.wParam == VK_LEFT
                    ? Mode::Portable
                    : Mode::Absolute;

            if (mode_ != requested) {
                mode_ = requested;
                InvalidateRect(
                    portable_,
                    nullptr,
                    TRUE);
                InvalidateRect(
                    absolute_,
                    nullptr,
                    TRUE);
                Scan();
            }

            SetFocus(
                mode_ == Mode::Portable
                    ? portable_
                    : absolute_);
            continue;
        }

        if (msg.message == WM_KEYDOWN &&
            msg.wParam == VK_ESCAPE &&
            (msg.hwnd == hwnd_ ||
             IsChild(
                 hwnd_,
                 msg.hwnd))) {
            CloseWindow();
            continue;
        }

        if (!IsDialogMessageW(
                hwnd_,
                &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    if (owner_) {
        EnableWindow(owner_, TRUE);
        SetForegroundWindow(owner_);
    }

    if (sawQuit) {
        PostQuitMessage(quitCode);
    }

    return changed_;
}

void ShortcutPathConverterDialog::CreateControls() {
    const auto makeButton =
        [&](HWND& target,
            const wchar_t* text,
            UINT id,
            DWORD style) {
            target = CreateWindowExW(
                0,
                L"BUTTON",
                text,
                WS_CHILD |
                    WS_VISIBLE |
                    WS_TABSTOP |
                    style,
                0,
                0,
                0,
                0,
                hwnd_,
                reinterpret_cast<HMENU>(
                    static_cast<UINT_PTR>(
                        id)),
                instance_,
                nullptr);
        };

    const auto makeStatic =
        [&](HWND& target,
            DWORD style = SS_LEFT |
                SS_NOPREFIX) {
            target = CreateWindowExW(
                0,
                L"STATIC",
                L"",
                WS_CHILD |
                    WS_VISIBLE |
                    style,
                0,
                0,
                0,
                0,
                hwnd_,
                nullptr,
                instance_,
                nullptr);
        };

    makeStatic(modeTitle_);

    makeButton(
        portable_,
        L"",
        kIdPortable,
        BS_OWNERDRAW |
            WS_GROUP);

    makeButton(
        absolute_,
        L"",
        kIdAbsolute,
        BS_OWNERDRAW);

    makeButton(
        rescan_,
        L"",
        kIdRescan,
        BS_OWNERDRAW);

    makeStatic(rule_);

    list_ = CreateWindowExW(
        0,
        WC_LISTVIEWW,
        L"",
        WS_CHILD |
            WS_VISIBLE |
            WS_TABSTOP |
            LVS_REPORT |
            LVS_SHOWSELALWAYS,
        0,
        0,
        0,
        0,
        hwnd_,
        reinterpret_cast<HMENU>(
            static_cast<UINT_PTR>(
                kIdList)),
        instance_,
        nullptr);

    ListView_SetExtendedListViewStyle(
        list_,
        LVS_EX_FULLROWSELECT |
            LVS_EX_DOUBLEBUFFER |
            LVS_EX_CHECKBOXES);

    rowHeightImageList_ =
        ImageList_Create(
            1,
            Scale(
                kResultRowHeightLogical),
            ILC_COLOR32,
            1,
            1);

    if (rowHeightImageList_) {
        ListView_SetImageList(
            list_,
            rowHeightImageList_,
            LVSIL_SMALL);
    }

    SetWindowTheme(
        list_,
        L"Explorer",
        nullptr);

    if (HWND header =
            ListView_GetHeader(
                list_)) {
        SetWindowTheme(
            header,
            L"Explorer",
            nullptr);
    }

    ListView_SetBkColor(
        list_,
        ui::kApplicationPalette
            .controlBackground);
    ListView_SetTextBkColor(
        list_,
        ui::kApplicationPalette
            .controlBackground);
    ListView_SetTextColor(
        list_,
        ui::kApplicationPalette.text);

    makeStatic(status_);

    makeButton(
        apply_,
        L"",
        kIdApply,
        BS_OWNERDRAW);

    const auto language =
        app_.SettingsData().language;

    font_ =
        ui::CreateFontHandle(
            ui::ApplicationFontSpec(
                language,
                ui::UiFontRole::Body),
            dpi_);

    groupFont_ =
        ui::CreateFontHandle(
            ui::ApplicationFontSpec(
                language,
                ui::UiFontRole::BodySemibold),
            dpi_);

    for (HWND control :
         std::array<HWND, 8>{
             modeTitle_,
             portable_,
             absolute_,
             rescan_,
             rule_,
             list_,
             status_,
             apply_}) {
        SendMessageW(
            control,
            WM_SETFONT,
            reinterpret_cast<WPARAM>(
                font_),
            TRUE);
    }

    SendMessageW(
        modeTitle_,
        WM_SETFONT,
        reinterpret_cast<WPARAM>(
            groupFont_),
        TRUE);

    EnableWindow(
        apply_,
        FALSE);

    const std::array<int, 4> widths{
        120,
        320,
        330,
        110,
    };

    const std::array<const wchar_t*, 4>
        initialLabels{
            T(L"字段", L"Field"),
            T(L"当前路径", L"Current path"),
            T(L"转换后路径", L"Converted path"),
            T(L"状态", L"Status"),
        };

    adjustingColumnWidths_ =
        true;

    for (int index = 0;
         index <
            static_cast<int>(
                widths.size());
         ++index) {
        LVCOLUMNW column{};
        column.mask =
            LVCF_WIDTH |
            LVCF_SUBITEM |
            LVCF_TEXT;
        column.cx =
            Scale(widths[
                static_cast<
                    std::size_t>(
                        index)]);
        column.iSubItem = index;
        column.pszText =
            const_cast<wchar_t*>(
                initialLabels[
                    static_cast<
                        std::size_t>(
                            index)]);

        ListView_InsertColumn(
            list_,
            index,
            &column);
    }

    adjustingColumnWidths_ =
        false;
}

void ShortcutPathConverterDialog::ApplyLanguage() {
    SetWindowTextW(
        hwnd_,
        T(L"ALTRun Next 路径转换",
          L"ALTRun Next Path Conversion"));

    SetWindowTextW(
        modeTitle_,
        T(L"转换方式",
          L"Conversion mode"));

    SetWindowTextW(
        portable_,
        T(L"便携化\n绝对路径 → 相对路径 / 环境变量",
          L"Portable\nAbsolute → relative / environment variable"));

    SetWindowTextW(
        absolute_,
        T(L"展开\n相对路径 / 环境变量 → 当前机器绝对路径",
          L"Expand\nRelative / environment variable → absolute"));

    SetWindowTextW(
        rescan_,
        T(L"重新扫描", L"Rescan"));

    SetWindowTextW(
        rule_,
        T(L"仅转换目标、工作目录和自定义图标；参数、URL、UNC 路径和裸命令保持不变。",
          L"Only Target, Working Directory and custom icons are converted; arguments, URLs, UNC paths and bare commands stay unchanged."));

    SetWindowTextW(
        apply_,
        T(L"应用所选", L"Apply selected"));

    const std::array<const wchar_t*, 4>
        labels{
            T(L"字段", L"Field"),
            T(L"当前路径", L"Current path"),
            T(L"转换后路径", L"Converted path"),
            T(L"状态", L"Status"),
        };

    for (int index = 0;
         index <
            static_cast<int>(
                labels.size());
         ++index) {
        LVCOLUMNW column{};
        column.mask = LVCF_TEXT;
        column.pszText =
            const_cast<wchar_t*>(
                labels[
                    static_cast<
                        std::size_t>(
                            index)]);
        ListView_SetColumn(
            list_,
            index,
            &column);
    }

    InvalidateRect(
        portable_,
        nullptr,
        TRUE);
    InvalidateRect(
        absolute_,
        nullptr,
        TRUE);
}

void ShortcutPathConverterDialog::DrawModeCard(
    const DRAWITEMSTRUCT& draw) const {
    if (!draw.hwndItem) {
        return;
    }

    const auto& palette =
        ui::kApplicationPalette;

    const bool portable =
        draw.CtlID == kIdPortable;
    const bool selected =
        portable
            ? mode_ == Mode::Portable
            : mode_ == Mode::Absolute;
    const bool pressed =
        (draw.itemState &
         ODS_SELECTED) != 0;
    const bool focused =
        (draw.itemState &
         ODS_FOCUS) != 0;

    RECT rect = draw.rcItem;

    HBRUSH outer =
        CreateSolidBrush(
            palette.windowBackground);
    FillRect(
        draw.hDC,
        &rect,
        outer);
    DeleteObject(outer);

    RECT surface = rect;
    InflateRect(
        &surface,
        -1,
        -1);

    COLORREF fillColor =
        selected
            ? palette.selectionBackground
            : palette.cardBackground;

    if (pressed) {
        fillColor =
            palette.pressedBackground;
    }

    const COLORREF borderColor =
        selected || focused
            ? palette.accent
            : palette.frame;

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
            draw.hDC,
            fill);
    HGDIOBJ oldPen =
        SelectObject(
            draw.hDC,
            pen);

    RoundRect(
        draw.hDC,
        surface.left,
        surface.top,
        surface.right,
        surface.bottom,
        Scale(8),
        Scale(8));

    SelectObject(
        draw.hDC,
        oldPen);
    SelectObject(
        draw.hDC,
        oldBrush);
    DeleteObject(pen);
    DeleteObject(fill);

    // The selector uses discrete DPI templates and a 4x4 coverage raster
    // written directly to final device pixels. This keeps the ring and center
    // dot smooth without any second scaling/interpolation pass.
    const bool disabled =
        (draw.itemState &
         ODS_DISABLED) != 0;

    const SelectorRasterTemplate selector =
        SelectorTemplateForDpi(
            dpi_);
    const int radioLeft =
        surface.left +
        Scale(12);
    const int radioTop =
        surface.top +
        Scale(12);

    const COLORREF ringColor =
        disabled
            ? palette.separator
            : selected || focused
                ? palette.accent
                : palette.mutedText;
    const COLORREF dotColor =
        disabled
            ? palette.mutedText
            : palette.accent;

    DrawSelectorRaster(
        draw.hDC,
        radioLeft,
        radioTop,
        selector,
        selected,
        fillColor,
        ringColor,
        dotColor);

    SetBkMode(
        draw.hDC,
        TRANSPARENT);

    RECT titleRect{
        surface.left + Scale(34),
        surface.top + Scale(8),
        surface.right - Scale(10),
        surface.top + Scale(28),
    };

    HGDIOBJ oldFont =
        SelectObject(
            draw.hDC,
            groupFont_
                ? groupFont_
                : font_);

    SetTextColor(
        draw.hDC,
        palette.text);

    DrawTextW(
        draw.hDC,
        portable
            ? T(L"便携化",
                L"Portable")
            : T(L"展开",
                L"Expand"),
        -1,
        &titleRect,
        DT_LEFT |
            DT_VCENTER |
            DT_SINGLELINE |
            DT_END_ELLIPSIS);

    SelectObject(
        draw.hDC,
        font_
            ? font_
            : oldFont);

    RECT descriptionRect{
        titleRect.left,
        surface.top + Scale(29),
        titleRect.right,
        surface.bottom - Scale(7),
    };

    SetTextColor(
        draw.hDC,
        palette.mutedText);

    DrawTextW(
        draw.hDC,
        portable
            ? T(L"绝对路径 → 相对路径 / 环境变量",
                L"Absolute → relative / environment variable")
            : T(L"相对路径 / 环境变量 → 当前机器绝对路径",
                L"Relative / environment variable → absolute"),
        -1,
        &descriptionRect,
        DT_LEFT |
            DT_VCENTER |
            DT_SINGLELINE |
            DT_END_ELLIPSIS);

    SelectObject(
        draw.hDC,
        oldFont);
}

void ShortcutPathConverterDialog::DrawActionButton(
    const DRAWITEMSTRUCT& draw) const {
    if (!draw.hwndItem) {
        return;
    }

    const auto& palette =
        ui::kApplicationPalette;

    const bool primary =
        draw.CtlID == kIdApply;
    const bool disabled =
        (draw.itemState &
         ODS_DISABLED) != 0;
    const bool pressed =
        (draw.itemState &
         ODS_SELECTED) != 0;
    const bool focused =
        (draw.itemState &
         ODS_FOCUS) != 0;
    const bool hot =
        (draw.itemState &
         ODS_HOTLIGHT) != 0;

    COLORREF fillColor =
        palette.controlBackground;
    COLORREF borderColor =
        palette.frame;
    COLORREF textColor =
        disabled
            ? palette.mutedText
            : palette.text;

    if (primary) {
        if (disabled) {
            fillColor =
                palette.cardBackground;
            borderColor =
                palette.separator;
        } else {
            fillColor =
                pressed
                    ? RGB(0, 96, 170)
                    : palette.accent;
            borderColor =
                fillColor;
            textColor =
                RGB(255, 255, 255);
        }
    } else if (pressed) {
        fillColor =
            palette.pressedBackground;
    } else if (hot) {
        fillColor =
            palette.cardBackground;
    }

    if (focused &&
        !primary) {
        borderColor =
            palette.accent;
    }

    RECT rect =
        draw.rcItem;

    HBRUSH outer =
        CreateSolidBrush(
            palette.windowBackground);
    FillRect(
        draw.hDC,
        &rect,
        outer);
    DeleteObject(
        outer);

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
            draw.hDC,
            fill);
    HGDIOBJ oldPen =
        SelectObject(
            draw.hDC,
            pen);

    RoundRect(
        draw.hDC,
        surface.left,
        surface.top,
        surface.right,
        surface.bottom,
        Scale(6),
        Scale(6));

    SelectObject(
        draw.hDC,
        oldPen);
    SelectObject(
        draw.hDC,
        oldBrush);
    DeleteObject(
        pen);
    DeleteObject(
        fill);

    wchar_t text[128]{};
    GetWindowTextW(
        draw.hwndItem,
        text,
        static_cast<int>(
            std::size(text)));

    SetBkMode(
        draw.hDC,
        TRANSPARENT);
    SetTextColor(
        draw.hDC,
        textColor);

    HGDIOBJ oldFont =
        SelectObject(
            draw.hDC,
            font_);

    RECT textRect =
        surface;
    InflateRect(
        &textRect,
        -Scale(10),
        0);

    DrawTextW(
        draw.hDC,
        text,
        -1,
        &textRect,
        DT_CENTER |
            DT_VCENTER |
            DT_SINGLELINE |
            DT_END_ELLIPSIS |
            DT_NOPREFIX);

    SelectObject(
        draw.hDC,
        oldFont);
}

LRESULT ShortcutPathConverterDialog::
HandleHeaderCustomDraw(
    NMCUSTOMDRAW* draw) {
    if (!draw ||
        !list_ ||
        draw->hdr.hwndFrom !=
            ListView_GetHeader(
                list_)) {
        return CDRF_DODEFAULT;
    }

    const auto& palette =
        ui::kApplicationPalette;
    const COLORREF headerBackground =
        RGB(250, 251, 252);
    const COLORREF headerSeparator =
        RGB(236, 239, 243);

    if (draw->dwDrawStage ==
        CDDS_PREPAINT) {
        return CDRF_NOTIFYITEMDRAW;
    }

    if (draw->dwDrawStage !=
        CDDS_ITEMPREPAINT) {
        return CDRF_DODEFAULT;
    }

    RECT rect =
        draw->rc;

    HBRUSH background =
        CreateSolidBrush(
            (draw->uItemState &
             (CDIS_SELECTED |
              CDIS_HOT)) != 0
                ? palette.cardBackground
                : headerBackground);

    FillRect(
        draw->hdc,
        &rect,
        background);
    DeleteObject(
        background);

    std::array<wchar_t, 128>
        text{};

    HDITEMW item{};
    item.mask =
        HDI_TEXT;
    item.pszText =
        text.data();
    item.cchTextMax =
        static_cast<int>(
            text.size());

    Header_GetItem(
        draw->hdr.hwndFrom,
        static_cast<int>(
            draw->dwItemSpec),
        &item);

    SetBkMode(
        draw->hdc,
        TRANSPARENT);
    SetTextColor(
        draw->hdc,
        palette.text);

    HGDIOBJ oldFont =
        SelectObject(
            draw->hdc,
            font_);

    RECT textRect =
        rect;
    textRect.left +=
        Scale(8);
    textRect.right -=
        Scale(8);

    DrawTextW(
        draw->hdc,
        text.data(),
        -1,
        &textRect,
        DT_LEFT |
            DT_VCENTER |
            DT_SINGLELINE |
            DT_END_ELLIPSIS |
            DT_NOPREFIX);

    SelectObject(
        draw->hdc,
        oldFont);

    HPEN separator =
        CreatePen(
            PS_SOLID,
            1,
            headerSeparator);
    HGDIOBJ oldPen =
        SelectObject(
            draw->hdc,
            separator);

    MoveToEx(
        draw->hdc,
        rect.left,
        rect.bottom - 1,
        nullptr);
    LineTo(
        draw->hdc,
        rect.right,
        rect.bottom - 1);

    MoveToEx(
        draw->hdc,
        rect.right - 1,
        rect.top + Scale(6),
        nullptr);
    LineTo(
        draw->hdc,
        rect.right - 1,
        rect.bottom - Scale(6));

    SelectObject(
        draw->hdc,
        oldPen);
    DeleteObject(
        separator);

    return CDRF_SKIPDEFAULT;
}

void ShortcutPathConverterDialog::Layout() {
    if (!hwnd_) {
        return;
    }

    RECT client{};
    GetClientRect(
        hwnd_,
        &client);

    const int margin = Scale(20);
    const int gap = Scale(12);
    const int titleHeight = Scale(20);
    const int cardHeight = Scale(62);
    const int ruleHeight = Scale(20);
    const int rescanWidth = Scale(104);
    const int buttonWidth = Scale(122);
    const int buttonHeight = Scale(34);
    const int footerHeight = Scale(58);

    int y = Scale(18);

    MoveWindow(
        modeTitle_,
        margin,
        y,
        std::max(
            1,
            static_cast<int>(
                client.right) -
                margin * 2),
        titleHeight,
        TRUE);

    y += Scale(28);

    const int cardsRight =
        static_cast<int>(
            client.right) -
        margin -
        rescanWidth -
        gap;

    const int cardsWidth =
        std::max(
            Scale(440),
            cardsRight -
                margin);

    const int cardWidth =
        std::max(
            Scale(210),
            (cardsWidth - gap) / 2);

    MoveWindow(
        portable_,
        margin,
        y,
        cardWidth,
        cardHeight,
        TRUE);

    MoveWindow(
        absolute_,
        margin +
            cardWidth +
            gap,
        y,
        cardWidth,
        cardHeight,
        TRUE);

    MoveWindow(
        rescan_,
        static_cast<int>(
            client.right) -
            margin -
            rescanWidth,
        y +
            std::max(
                0,
                (cardHeight -
                 buttonHeight) / 2),
        rescanWidth,
        buttonHeight,
        TRUE);

    y += cardHeight +
        Scale(10);

    MoveWindow(
        rule_,
        margin,
        y,
        std::max(
            1,
            static_cast<int>(
                client.right) -
                margin * 2),
        ruleHeight,
        TRUE);

    y += ruleHeight +
        Scale(10);

    MoveWindow(
        list_,
        margin,
        y,
        std::max(
            1,
            static_cast<int>(
                client.right) -
                margin * 2),
        std::max(
            1,
            static_cast<int>(
                client.bottom) -
                y -
                footerHeight),
        TRUE);

    const int footerY =
        static_cast<int>(
            client.bottom) -
        margin -
        buttonHeight;

    MoveWindow(
        apply_,
        static_cast<int>(
            client.right) -
            margin -
            buttonWidth,
        footerY,
        buttonWidth,
        buttonHeight,
        TRUE);

    MoveWindow(
        status_,
        margin,
        footerY +
            std::max(
                0,
                (buttonHeight -
                 Scale(20)) / 2),
        std::max(
            1,
            static_cast<int>(
                client.right) -
                margin * 2 -
                buttonWidth -
                gap),
        Scale(20),
        TRUE);

    UpdateColumnWidths();
}

std::size_t
ShortcutPathConverterDialog::
SelectedFieldCount() const {
    if (!list_) {
        return 0;
    }

    std::size_t count = 0;
    const int itemCount =
        ListView_GetItemCount(
            list_);

    for (int itemIndex = 0;
         itemIndex < itemCount;
         ++itemIndex) {
        if (RowIndexForListItem(
                itemIndex) &&
            ListView_GetCheckState(
                list_,
                itemIndex)) {
            ++count;
        }
    }

    return count;
}

void ShortcutPathConverterDialog::
UpdateSelectionState(
    std::optional<std::size_t>
        appliedFieldCount) {
    if (!status_ ||
        !apply_) {
        return;
    }

    const std::size_t selected =
        SelectedFieldCount();

    EnableWindow(
        apply_,
        selected > 0
            ? TRUE
            : FALSE);

    InvalidateRect(
        apply_,
        nullptr,
        TRUE);

    std::wstring text;

    if (appliedFieldCount) {
        text =
            T(L"已应用 ",
              L"Applied ");
        text +=
            std::to_wstring(
                *appliedFieldCount);
        text +=
            T(L" 个路径转换",
              L" path conversions");
    } else {
        text =
            std::to_wstring(
                convertibleShortcutCount_);
        text +=
            T(L" 个快捷项 · ",
              L" shortcuts · ");
        text +=
            std::to_wstring(
                rows_.size());
        text +=
            T(L" 个可转换字段 · 已选择 ",
              L" convertible fields · ");
        text +=
            std::to_wstring(
                selected);
        text +=
            T(L" 项",
              L" selected");
    }

    SetWindowTextW(
        status_,
        text.c_str());
}

void ShortcutPathConverterDialog::
UpdateColumnWidths(
    int resizedColumn) {
    if (!list_) {
        return;
    }

    HWND header =
        ListView_GetHeader(
            list_);

    RECT headerRect{};
    RECT listRect{};

    int contentWidth = 0;

    if (header &&
        GetClientRect(
            header,
            &headerRect)) {
        contentWidth =
            headerRect.right -
            headerRect.left;
    }

    if (contentWidth <= 0 &&
        GetClientRect(
            list_,
            &listRect)) {
        contentWidth =
            listRect.right -
            listRect.left;
    }

    if (contentWidth <= 0) {
        return;
    }

    const std::array<int, 4>
        minimums{
            Scale(
                kFieldColumnMinimumLogical),
            Scale(
                kCurrentColumnMinimumLogical),
            Scale(
                kConvertedColumnMinimumLogical),
            Scale(
                kStatusColumnMinimumLogical),
        };

    std::array<int, 3> widths{};

    if (!customColumnWidths_) {
        widths[0] =
            std::max(
                minimums[0],
                contentWidth *
                    kFieldColumnPercent /
                    100);
        widths[1] =
            std::max(
                minimums[1],
                contentWidth *
                    kCurrentColumnPercent /
                    100);
        widths[2] =
            std::max(
                minimums[2],
                contentWidth *
                    kConvertedColumnPercent /
                    100);
    } else {
        for (int index = 0;
             index < 3;
             ++index) {
            widths[
                static_cast<
                    std::size_t>(
                        index)] =
                std::max(
                    minimums[
                        static_cast<
                            std::size_t>(
                                index)],
                    ListView_GetColumnWidth(
                        list_,
                        index));
        }
    }

    const int minimumFirstThree =
        minimums[0] +
        minimums[1] +
        minimums[2];

    const int firstThreeLimit =
        std::max(
            minimumFirstThree,
            contentWidth -
                minimums[3]);

    if (resizedColumn >= 0 &&
        resizedColumn < 3) {
        int otherWidth = 0;

        for (int index = 0;
             index < 3;
             ++index) {
            if (index !=
                resizedColumn) {
                otherWidth +=
                    widths[
                        static_cast<
                            std::size_t>(
                                index)];
            }
        }

        const int maximum =
            std::max(
                minimums[
                    static_cast<
                        std::size_t>(
                            resizedColumn)],
                firstThreeLimit -
                    otherWidth);

        widths[
            static_cast<
                std::size_t>(
                    resizedColumn)] =
            std::clamp(
                widths[
                    static_cast<
                        std::size_t>(
                            resizedColumn)],
                minimums[
                    static_cast<
                        std::size_t>(
                            resizedColumn)],
                maximum);
    } else {
        int total =
            widths[0] +
            widths[1] +
            widths[2];

        if (total >
            firstThreeLimit) {
            int excess =
                total -
                firstThreeLimit;

            for (int index :
                 std::array<int, 3>{
                     2,
                     1,
                     0}) {
                if (excess <= 0) {
                    break;
                }

                const auto position =
                    static_cast<
                        std::size_t>(
                            index);

                const int capacity =
                    std::max(
                        0,
                        widths[position] -
                            minimums[position]);

                const int amount =
                    std::min(
                        excess,
                        capacity);

                widths[position] -=
                    amount;
                excess -= amount;
            }
        }
    }

    const int statusWidth =
        std::max(
            1,
            contentWidth -
                widths[0] -
                widths[1] -
                widths[2]);

    adjustingColumnWidths_ =
        true;

    const int currentStatus =
        ListView_GetColumnWidth(
            list_,
            3);

    if (statusWidth <
        currentStatus) {
        ListView_SetColumnWidth(
            list_,
            3,
            statusWidth);
    }

    for (int index = 0;
         index < 3;
         ++index) {
        const int width =
            widths[
                static_cast<
                    std::size_t>(
                        index)];

        if (ListView_GetColumnWidth(
                list_,
                index) !=
            width) {
            ListView_SetColumnWidth(
                list_,
                index,
                width);
        }
    }

    if (statusWidth >=
            currentStatus &&
        currentStatus !=
            statusWidth) {
        ListView_SetColumnWidth(
            list_,
            3,
            statusWidth);
    }

    adjustingColumnWidths_ =
        false;
}

bool ShortcutPathConverterDialog::
HandleHeaderNotification(
    LPARAM lParam,
    LRESULT& result) {
    if (!list_ ||
        adjustingColumnWidths_) {
        return false;
    }

    auto* notification =
        reinterpret_cast<NMHDR*>(
            lParam);

    if (!notification) {
        return false;
    }

    HWND headerWindow =
        ListView_GetHeader(
            list_);

    if (notification->hwndFrom !=
        headerWindow) {
        return false;
    }

    const int code =
        static_cast<int>(
            notification->code);

    const bool beginTrack =
        code == HDN_BEGINTRACKA ||
        code == HDN_BEGINTRACKW;
    const bool itemChanging =
        code == HDN_ITEMCHANGINGA ||
        code == HDN_ITEMCHANGINGW;
    const bool track =
        code == HDN_TRACKA ||
        code == HDN_TRACKW;
    const bool itemChanged =
        code == HDN_ITEMCHANGEDA ||
        code == HDN_ITEMCHANGEDW;
    const bool endTrack =
        code == HDN_ENDTRACKA ||
        code == HDN_ENDTRACKW;
    const bool dividerDoubleClick =
        code == HDN_DIVIDERDBLCLICKA ||
        code == HDN_DIVIDERDBLCLICKW;

    if (!beginTrack &&
        !itemChanging &&
        !track &&
        !itemChanged &&
        !endTrack &&
        !dividerDoubleClick) {
        return false;
    }

    auto* header =
        reinterpret_cast<NMHEADERW*>(
            lParam);

    if (!header ||
        header->iItem < 0 ||
        header->iItem > 3) {
        return false;
    }

    const int column =
        header->iItem;

    const bool widthChange =
        header->pitem &&
        (header->pitem->mask &
         HDI_WIDTH) != 0;

    if (column == 3 &&
        (beginTrack ||
         track ||
         endTrack ||
         dividerDoubleClick ||
         ((itemChanging ||
           itemChanged) &&
          widthChange))) {
        result = TRUE;
        return true;
    }

    if (dividerDoubleClick) {
        result = TRUE;
        return true;
    }

    if (beginTrack) {
        customColumnWidths_ =
            true;
        result = FALSE;
        return true;
    }

    if ((itemChanging ||
         track) &&
        header->pitem &&
        (header->pitem->mask &
         HDI_WIDTH) != 0) {
        customColumnWidths_ =
            true;

        const std::array<int, 4>
            minimums{
                Scale(
                    kFieldColumnMinimumLogical),
                Scale(
                    kCurrentColumnMinimumLogical),
                Scale(
                    kConvertedColumnMinimumLogical),
                Scale(
                    kStatusColumnMinimumLogical),
            };

        HWND listHeader =
            ListView_GetHeader(
                list_);
        RECT client{};
        GetClientRect(
            listHeader,
            &client);

        int otherWidth = 0;
        for (int index = 0;
             index < 3;
             ++index) {
            if (index != column) {
                otherWidth +=
                    ListView_GetColumnWidth(
                        list_,
                        index);
            }
        }

        const int maximum =
            std::max(
                minimums[
                    static_cast<
                        std::size_t>(
                            column)],
                static_cast<int>(
                    client.right -
                    client.left) -
                    minimums[3] -
                    otherWidth);

        header->pitem->cxy =
            std::clamp(
                header->pitem->cxy,
                minimums[
                    static_cast<
                        std::size_t>(
                            column)],
                maximum);

        result = FALSE;
        return true;
    }

    if (itemChanged &&
        header->pitem &&
        (header->pitem->mask &
         HDI_WIDTH) != 0) {
        customColumnWidths_ =
            true;
        UpdateColumnWidths(
            column);
        result = FALSE;
        return true;
    }

    if (endTrack) {
        customColumnWidths_ =
            true;
        UpdateColumnWidths(
            column);
        result = FALSE;
        return true;
    }

    return false;
}

void ShortcutPathConverterDialog::InsertGroupHeader(
    std::wstring_view title) {
    const int itemIndex =
        ListView_GetItemCount(list_);

    std::wstring text(title);

    LVITEMW item{};
    item.mask =
        LVIF_TEXT |
        LVIF_PARAM |
        LVIF_STATE;
    item.iItem = itemIndex;
    item.iSubItem = 0;
    item.pszText = text.data();
    item.lParam = kGroupHeaderItemParam;
    item.stateMask =
        LVIS_STATEIMAGEMASK;
    item.state = 0;

    const int inserted =
        ListView_InsertItem(
            list_,
            &item);

    if (inserted >= 0) {
        ListView_SetItemState(
            list_,
            inserted,
            0,
            LVIS_STATEIMAGEMASK |
                LVIS_SELECTED);
    }
}

void ShortcutPathConverterDialog::InsertPreviewRow(
    Row row) {
    const std::size_t rowIndex =
        rows_.size();

    rows_.push_back(
        std::move(row));

    const int itemIndex =
        ListView_GetItemCount(list_);

    std::wstring fieldText;

    switch (rows_.back().field) {
    case Field::Target:
        fieldText =
            T(L"    目标",
              L"    Target");
        break;
    case Field::WorkingDirectory:
        fieldText =
            T(L"    工作目录",
              L"    Working directory");
        break;
    case Field::Icon:
        fieldText =
            T(L"    自定义图标",
              L"    Custom icon");
        break;
    }

    LVITEMW item{};
    item.mask =
        LVIF_TEXT |
        LVIF_PARAM;
    item.iItem = itemIndex;
    item.iSubItem = 0;
    item.pszText = fieldText.data();
    item.lParam =
        static_cast<LPARAM>(
            rowIndex + 1);

    const int inserted =
        ListView_InsertItem(
            list_,
            &item);

    if (inserted < 0) {
        rows_.pop_back();
        return;
    }

    ListView_SetItemText(
        list_,
        inserted,
        1,
        rows_.back()
            .current
            .data());

    ListView_SetItemText(
        list_,
        inserted,
        2,
        rows_.back()
            .converted
            .data());

    ListView_SetItemText(
        list_,
        inserted,
        3,
        const_cast<wchar_t*>(
            rows_.back().exists
                ? T(L"可访问",
                    L"Accessible")
                : T(L"路径不存在",
                    L"Missing")));

    ListView_SetCheckState(
        list_,
        inserted,
        rows_.back().exists
            ? TRUE
            : FALSE);
}

bool ShortcutPathConverterDialog::IsGroupHeaderItem(
    int itemIndex) const {
    if (!list_ || itemIndex < 0) {
        return false;
    }

    LVITEMW item{};
    item.mask = LVIF_PARAM;
    item.iItem = itemIndex;

    if (!ListView_GetItem(
            list_,
            &item)) {
        return false;
    }

    return item.lParam ==
        kGroupHeaderItemParam;
}

std::optional<std::size_t>
ShortcutPathConverterDialog::RowIndexForListItem(
    int itemIndex) const {
    if (!list_ || itemIndex < 0) {
        return std::nullopt;
    }

    LVITEMW item{};
    item.mask = LVIF_PARAM;
    item.iItem = itemIndex;

    if (!ListView_GetItem(
            list_,
            &item) ||
        item.lParam <= 0) {
        return std::nullopt;
    }

    const auto rowIndex =
        static_cast<std::size_t>(
            item.lParam - 1);

    if (rowIndex >= rows_.size()) {
        return std::nullopt;
    }

    return rowIndex;
}

LRESULT ShortcutPathConverterDialog::HandleListCustomDraw(
    NMLVCUSTOMDRAW* draw) {
    if (!draw) {
        return CDRF_DODEFAULT;
    }

    const auto& palette =
        ui::kApplicationPalette;

    switch (draw->nmcd.dwDrawStage) {
    case CDDS_PREPAINT:
        return CDRF_NOTIFYITEMDRAW |
            CDRF_NOTIFYPOSTPAINT;

    case CDDS_POSTPAINT:
        if (!rows_.empty()) {
            return CDRF_DODEFAULT;
        } else {
            RECT rect{};
            GetClientRect(
                list_,
                &rect);

            rect.left += Scale(24);
            rect.right -= Scale(24);

            const int listHeight =
                static_cast<int>(
                    rect.bottom) -
                static_cast<int>(
                    rect.top);

            const int centerY =
                static_cast<int>(
                    rect.top) +
                std::max(
                    0,
                    listHeight * 44 / 100);

            RECT titleRect{
                rect.left,
                centerY - Scale(28),
                rect.right,
                centerY - Scale(4),
            };

            RECT detailRect{
                rect.left,
                centerY + Scale(2),
                rect.right,
                centerY + Scale(28),
            };

            SetBkMode(
                draw->nmcd.hdc,
                TRANSPARENT);

            HGDIOBJ oldFont =
                SelectObject(
                    draw->nmcd.hdc,
                    groupFont_
                        ? groupFont_
                        : font_);

            SetTextColor(
                draw->nmcd.hdc,
                palette.text);

            DrawTextW(
                draw->nmcd.hdc,
                T(L"当前没有可转换的路径",
                  L"No convertible paths"),
                -1,
                &titleRect,
                DT_CENTER |
                    DT_VCENTER |
                    DT_SINGLELINE |
                    DT_END_ELLIPSIS);

            SelectObject(
                draw->nmcd.hdc,
                font_
                    ? font_
                    : oldFont);

            SetTextColor(
                draw->nmcd.hdc,
                palette.mutedText);

            DrawTextW(
                draw->nmcd.hdc,
                mode_ == Mode::Portable
                    ? T(L"没有发现需要进行便携化转换的快捷项",
                        L"No shortcuts currently need portable-path conversion")
                    : T(L"没有发现需要展开为绝对路径的快捷项",
                        L"No shortcuts currently need expansion to absolute paths"),
                -1,
                &detailRect,
                DT_CENTER |
                    DT_VCENTER |
                    DT_SINGLELINE |
                    DT_END_ELLIPSIS);

            SelectObject(
                draw->nmcd.hdc,
                oldFont);

            return CDRF_DODEFAULT;
        }

    case CDDS_ITEMPREPAINT: {
        const int itemIndex =
            static_cast<int>(
                draw->nmcd.dwItemSpec);

        if (!IsGroupHeaderItem(
                itemIndex)) {
            return CDRF_DODEFAULT;
        }

        RECT rect{};
        if (!ListView_GetItemRect(
                list_,
                itemIndex,
                &rect,
                LVIR_BOUNDS)) {
            return CDRF_DODEFAULT;
        }

        RECT client{};
        GetClientRect(
            list_,
            &client);
        rect.left = client.left;
        rect.right = client.right;

        HBRUSH background =
            CreateSolidBrush(
                palette.cardBackground);
        FillRect(
            draw->nmcd.hdc,
            &rect,
            background);
        DeleteObject(background);

        std::array<wchar_t, 512>
            title{};

        ListView_GetItemText(
            list_,
            itemIndex,
            0,
            title.data(),
            static_cast<int>(
                title.size()));

        RECT textRect = rect;
        textRect.left += Scale(12);
        textRect.right -= Scale(8);

        SetBkMode(
            draw->nmcd.hdc,
            TRANSPARENT);
        SetTextColor(
            draw->nmcd.hdc,
            palette.text);

        HGDIOBJ previousFont =
            SelectObject(
                draw->nmcd.hdc,
                groupFont_
                    ? groupFont_
                    : font_);

        DrawTextW(
            draw->nmcd.hdc,
            title.data(),
            -1,
            &textRect,
            DT_LEFT |
                DT_VCENTER |
                DT_SINGLELINE |
                DT_END_ELLIPSIS);

        SelectObject(
            draw->nmcd.hdc,
            previousFont);

        HPEN separator =
            CreatePen(
                PS_SOLID,
                1,
                palette.separator);

        HGDIOBJ previousPen =
            SelectObject(
                draw->nmcd.hdc,
                separator);

        MoveToEx(
            draw->nmcd.hdc,
            rect.left,
            rect.bottom - 1,
            nullptr);

        LineTo(
            draw->nmcd.hdc,
            rect.right,
            rect.bottom - 1);

        SelectObject(
            draw->nmcd.hdc,
            previousPen);
        DeleteObject(separator);

        return CDRF_SKIPDEFAULT;
    }

    default:
        return CDRF_DODEFAULT;
    }
}

void ShortcutPathConverterDialog::Scan(
    std::optional<std::size_t>
        appliedFieldCount) {
    if (!list_) {
        return;
    }

    rebuildingList_ = true;
    ListView_DeleteAllItems(
        list_);
    rows_.clear();

    std::size_t convertibleShortcutCount = 0;

    const auto makePreview =
        [&](const Command& command,
            Field field,
            std::wstring_view value,
            bool bareRelativeIsPath)
            -> std::optional<Row> {
            if (value.empty()) {
                return std::nullopt;
            }

            std::optional<win::PortablePathPreview>
                preview;

            if (mode_ == Mode::Portable) {
                preview =
                    win::MakePortablePath(
                        value,
                        app_.BaseDirectory(),
                        bareRelativeIsPath);
            } else {
                preview =
                    win::ExpandPortablePath(
                        value,
                        app_.BaseDirectory(),
                        bareRelativeIsPath);
            }

            if (!preview) {
                return std::nullopt;
            }

            Row row;
            row.commandId = command.id;
            row.field = field;
            row.current =
                std::wstring(value);
            row.converted =
                preview->converted;
            row.resolved =
                preview->resolved;
            row.exists =
                preview->exists;
            return row;
        };

    for (const auto& command :
         app_.UserCommands()) {
        std::vector<Row> commandRows;

        if (command.type !=
            CommandType::Url) {
            auto target =
                makePreview(
                    command,
                    Field::Target,
                    command.target,
                    command.type ==
                        CommandType::Folder);

            if (target) {
                commandRows.push_back(
                    std::move(*target));
            }
        }

        auto workingDirectory =
            makePreview(
                command,
                Field::WorkingDirectory,
                command.workingDirectory,
                true);

        if (workingDirectory) {
            commandRows.push_back(
                std::move(
                    *workingDirectory));
        }

        if (!command.icon.empty() &&
            command.icon != L"auto") {
            auto icon =
                makePreview(
                    command,
                    Field::Icon,
                    command.icon,
                    true);

            if (icon) {
                commandRows.push_back(
                    std::move(*icon));
            }
        }

        if (commandRows.empty()) {
            continue;
        }

        ++convertibleShortcutCount;

        std::wstring groupTitle =
            command.keyword.empty()
                ? command.title
                : command.keyword;

        if (!command.title.empty() &&
            !command.keyword.empty() &&
            command.title !=
                command.keyword) {
            groupTitle += L"  —  ";
            groupTitle += command.title;
        }

        if (groupTitle.empty()) {
            groupTitle = command.id;
        }

        InsertGroupHeader(
            groupTitle);

        for (auto& row :
             commandRows) {
            InsertPreviewRow(
                std::move(row));
        }
    }

    convertibleShortcutCount_ =
        convertibleShortcutCount;
    rebuildingList_ = false;

    UpdateSelectionState(
        appliedFieldCount);

    InvalidateRect(
        list_,
        nullptr,
        FALSE);
}

void ShortcutPathConverterDialog::ApplySelected() {
    std::vector<UserCommandPathUpdate>
        updates;
    std::size_t selectedFieldCount = 0;

    const int itemCount =
        ListView_GetItemCount(
            list_);

    for (int itemIndex = 0;
         itemIndex < itemCount;
         ++itemIndex) {
        const auto rowIndex =
            RowIndexForListItem(
                itemIndex);

        if (!rowIndex ||
            !ListView_GetCheckState(
                list_,
                itemIndex)) {
            continue;
        }

        ++selectedFieldCount;

        const Row& row =
            rows_[*rowIndex];

        auto it =
            std::find_if(
                updates.begin(),
                updates.end(),
                [&](const auto& update) {
                    return update.id ==
                        row.commandId;
                });

        if (it ==
            updates.end()) {
            UserCommandPathUpdate update;
            update.id = row.commandId;
            updates.push_back(
                std::move(update));
            it =
                std::prev(
                    updates.end());
        }

        switch (row.field) {
        case Field::Target:
            it->target =
                row.converted;
            break;
        case Field::WorkingDirectory:
            it->workingDirectory =
                row.converted;
            break;
        case Field::Icon:
            it->icon =
                row.converted;
            break;
        }
    }

    if (updates.empty()) {
        UpdateSelectionState();
        return;
    }

    if (!app_.ApplyUserCommandPathUpdates(
            updates)) {
        MessageBoxW(
            hwnd_,
            T(L"写入 commands.json 失败，原数据已保留。",
              L"Failed to write commands.json. Existing data was preserved."),
            T(L"路径转换失败",
              L"Path Conversion Failed"),
            MB_OK |
                MB_ICONERROR);
        return;
    }

    changed_ = true;

    Scan(
        selectedFieldCount);
}

LRESULT CALLBACK
ShortcutPathConverterDialog::WindowProc(
    HWND hwnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam) {
    ShortcutPathConverterDialog* self =
        nullptr;

    if (message == WM_NCCREATE) {
        const auto* create =
            reinterpret_cast<
                CREATESTRUCTW*>(
                    lParam);

        self =
            static_cast<
                ShortcutPathConverterDialog*>(
                    create->lpCreateParams);

        SetWindowLongPtrW(
            hwnd,
            GWLP_USERDATA,
            reinterpret_cast<LONG_PTR>(
                self));

        self->hwnd_ = hwnd;
    } else {
        self =
            reinterpret_cast<
                ShortcutPathConverterDialog*>(
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

LRESULT ShortcutPathConverterDialog::HandleMessage(
    UINT message,
    WPARAM wParam,
    LPARAM lParam) {
    switch (message) {
    case WM_GETMINMAXINFO: {
        auto* minMax =
            reinterpret_cast<MINMAXINFO*>(
                lParam);

        if (minMax) {
            minMax->ptMinTrackSize.x =
                Scale(
                    kMinimumWidthLogical);
            minMax->ptMinTrackSize.y =
                Scale(
                    kMinimumHeightLogical);
        }
        return 0;
    }

    case WM_SIZE:
        Layout();
        return 0;

    case WM_DRAWITEM: {
        const auto* draw =
            reinterpret_cast<
                DRAWITEMSTRUCT*>(
                    lParam);

        if (draw &&
            (draw->CtlID ==
                 kIdPortable ||
             draw->CtlID ==
                 kIdAbsolute)) {
            DrawModeCard(
                *draw);
            return TRUE;
        }

        if (draw &&
            (draw->CtlID ==
                 kIdRescan ||
             draw->CtlID ==
                 kIdApply)) {
            DrawActionButton(
                *draw);
            return TRUE;
        }
        break;
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

        SetTextColor(
            dc,
            control == rule_ ||
                    control == status_
                ? ui::kApplicationPalette
                      .mutedText
                : ui::kApplicationPalette
                      .text);

        return reinterpret_cast<LRESULT>(
            GetSysColorBrush(
                COLOR_WINDOW));
    }

    case WM_NOTIFY: {
        const auto* notification =
            reinterpret_cast<NMHDR*>(
                lParam);

        if (notification &&
            notification->code ==
                NM_CUSTOMDRAW &&
            notification->hwndFrom ==
                ListView_GetHeader(
                    list_)) {
            return HandleHeaderCustomDraw(
                reinterpret_cast<
                    NMCUSTOMDRAW*>(
                        lParam));
        }

        LRESULT headerResult = 0;
        if (HandleHeaderNotification(
                lParam,
                headerResult)) {
            return headerResult;
        }

        const auto* header =
            reinterpret_cast<NMHDR*>(
                lParam);

        if (!header ||
            header->idFrom !=
                kIdList) {
            break;
        }

        if (header->code ==
            NM_CUSTOMDRAW) {
            return HandleListCustomDraw(
                reinterpret_cast<
                    NMLVCUSTOMDRAW*>(
                        lParam));
        }

        if (header->code ==
            LVN_ITEMCHANGING) {
            const auto* change =
                reinterpret_cast<
                    NMLISTVIEW*>(
                        lParam);

            if (IsGroupHeaderItem(
                    change->iItem)) {
                const UINT changedState =
                    change->uNewState ^
                    change->uOldState;

                if ((changedState &
                     (LVIS_SELECTED |
                      LVIS_STATEIMAGEMASK)) !=
                    0) {
                    return TRUE;
                }
            }
        }

        if (header->code ==
                LVN_ITEMCHANGED &&
            !rebuildingList_) {
            const auto* change =
                reinterpret_cast<
                    NMLISTVIEW*>(
                        lParam);

            if (!IsGroupHeaderItem(
                    change->iItem)) {
                const UINT changedState =
                    change->uNewState ^
                    change->uOldState;

                if ((changedState &
                     LVIS_STATEIMAGEMASK) !=
                    0) {
                    UpdateSelectionState();
                }
            }
        }

        return 0;
    }

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case kIdPortable:
            if (HIWORD(wParam) ==
                    BN_CLICKED &&
                mode_ != Mode::Portable) {
                mode_ = Mode::Portable;
                InvalidateRect(
                    portable_,
                    nullptr,
                    TRUE);
                InvalidateRect(
                    absolute_,
                    nullptr,
                    TRUE);
                Scan();
            }
            return 0;

        case kIdAbsolute:
            if (HIWORD(wParam) ==
                    BN_CLICKED &&
                mode_ != Mode::Absolute) {
                mode_ = Mode::Absolute;
                InvalidateRect(
                    portable_,
                    nullptr,
                    TRUE);
                InvalidateRect(
                    absolute_,
                    nullptr,
                    TRUE);
                Scan();
            }
            return 0;

        case kIdRescan:
            if (HIWORD(wParam) ==
                BN_CLICKED) {
                Scan();
            }
            return 0;

        case kIdApply:
            if (HIWORD(wParam) ==
                BN_CLICKED) {
                ApplySelected();
            }
            return 0;

        default:
            break;
        }
        break;

    case WM_CLOSE:
        CloseWindow();
        return 0;

    case WM_NCDESTROY:
        hwnd_ = nullptr;
        closed_ = true;
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
