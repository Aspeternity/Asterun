#include "Feedback.hpp"
#include "AppIcon.hpp"
#include "ShortcutPathConverterDialog.hpp"

#include "TopLevelWindowPresentation.hpp"
#include "UiFocusCues.hpp"
#include "UiListView.hpp"
#include "UiMetrics.hpp"
#include "UiRoundedFrame.hpp"
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
    L"Asterun.ShortcutPathConverter";

constexpr UINT kIdPortable = 54101;
constexpr UINT kIdAbsolute = 54102;
constexpr UINT kIdRescan = 54103;
constexpr UINT kIdList = 54104;
constexpr UINT kIdApply = 54105;

constexpr int kDefaultWidthLogical = 960;
constexpr int kDefaultHeightLogical = 560;
constexpr int kMinimumWidthLogical = 820;
constexpr int kMinimumHeightLogical = 480;

constexpr int kFieldColumnDefaultLogical = 110;
constexpr int kCurrentColumnDefaultLogical = 360;
constexpr int kConvertedColumnDefaultLogical = 380;
constexpr int kFieldColumnMinimumLogical = 100;
constexpr int kCurrentColumnMinimumLogical = 180;
constexpr int kConvertedColumnMinimumLogical = 180;
constexpr int kStatusColumnMinimumLogical = 100;

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

// Off-screen buffer for one ListView row. Drawing code keeps using list
// client coordinates (the viewport origin is shifted), and the finished row
// reaches the target DC in a single BitBlt when the buffer goes out of scope.
// Falls back to drawing directly if the bitmap cannot be created.
class RowPaintBuffer {
public:
    RowPaintBuffer(HDC target, const RECT& row) noexcept
        : target_(target), row_(row) {
        const int width = row.right - row.left;
        const int height = row.bottom - row.top;
        if (!target || width <= 0 || height <= 0) return;
        memory_ = CreateCompatibleDC(target);
        bitmap_ = memory_
            ? CreateCompatibleBitmap(target, width, height) : nullptr;
        if (!bitmap_) return;
        oldBitmap_ = SelectObject(memory_, bitmap_);
        SetViewportOrgEx(memory_, -row.left, -row.top, nullptr);
    }

    ~RowPaintBuffer() {
        if (memory_ && bitmap_) {
            BitBlt(target_, row_.left, row_.top,
                row_.right - row_.left, row_.bottom - row_.top,
                memory_, row_.left, row_.top, SRCCOPY);
            SelectObject(memory_, oldBitmap_);
        }
        if (bitmap_) DeleteObject(bitmap_);
        if (memory_) DeleteDC(memory_);
    }

    RowPaintBuffer(const RowPaintBuffer&) = delete;
    RowPaintBuffer& operator=(const RowPaintBuffer&) = delete;

    [[nodiscard]] HDC Dc() const noexcept {
        return memory_ && bitmap_ ? memory_ : target_;
    }

private:
    HDC target_{};
    RECT row_{};
    HDC memory_{};
    HBITMAP bitmap_{};
    HGDIOBJ oldBitmap_{};
};

struct CheckboxRasterTemplate {
    int size{};
    int textGap{};
};

[[nodiscard]] CheckboxRasterTemplate
CheckboxTemplateForDpi(
    UINT dpi) noexcept {
    // Match the selector's physical-pixel scale so the square and round
    // selection controls belong to one visual system. The glyph itself is
    // the shared ui::PaintSmoothCheckbox used by every custom checkbox.
    if (dpi <= 108) {
        return {15, 4};
    }

    if (dpi <= 132) {
        return {17, 4};
    }

    if (dpi <= 156) {
        return {19, 5};
    }

    if (dpi <= 180) {
        return {21, 5};
    }

    return {23, 6};
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
    if (hwnd_ && IsWindow(hwnd_)) {
        window_presentation::
            HideForDestroy(
                hwnd_);
        DestroyWindow(hwnd_);
    }

    if (font_) {
        DeleteObject(font_);
        font_ = nullptr;
    }

    if (groupFont_) {
        DeleteObject(groupFont_);
        groupFont_ = nullptr;
    }
}

void ShortcutPathConverterDialog::
CloseWindow() {
    if (!hwnd_ ||
        !IsWindow(hwnd_)) {
        return;
    }

    // End the nested message loop first. RunModal restores an enabled/active
    // owner while this popup still exists, then destroys the popup. Destroying
    // an active modal window while its owner is disabled lets USER32 activate
    // another window and then bounce back to the owner, which is visible as a
    // one-frame Shortcut Manager flash.
    closed_ = true;
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
        altrun::ui::ShowMessage(
            owner,
            app.SettingsData().language ==
                    Language::ZhCN
                ? L"无法创建路径转换窗口。"
                : L"Could not create the path conversion window.",
            L"Asterun",
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
        ui::LoadApplicationIcon(
            instance_);
    wc.hIconSm =
        ui::LoadApplicationIcon(
            instance_,
            true);
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
    const bool ownerWasEnabled = owner_ && IsWindowEnabled(owner_);
    if (ownerWasEnabled) {
        EnableWindow(owner_, FALSE);
    }

    window_presentation::
        RevealFullyPainted(
            hwnd_);
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

        ui::HideFocusCuesOnMouseDown(msg);

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
            msg.wParam == VK_SPACE &&
            msg.hwnd == list_) {
            const int focusedItem =
                ListView_GetNextItem(
                    list_,
                    -1,
                    LVNI_FOCUSED);

            if (ToggleResultRowSelection(
                    focusedItem)) {
                continue;
            }
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

    if (ownerWasEnabled && IsWindow(owner_)) {
        EnableWindow(owner_, TRUE);
        // Restore the same-thread owner before the active popup disappears.
        // SetActiveWindow is sufficient here and avoids a second global
        // foreground handoff. Hidden Launcher owners stay non-activated.
        if (IsWindowVisible(owner_) && !IsIconic(owner_)) {
            SetActiveWindow(owner_);
        }
    }

    if (hwnd_ && IsWindow(hwnd_)) {
        window_presentation::
            HideForDestroy(
                hwnd_);
        DestroyWindow(hwnd_);
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

    makeStatic(status_);

    makeButton(
        apply_,
        L"",
        kIdApply,
        BS_OWNERDRAW);

    ApplyFonts();

    ui::NextListColumnResizePolicy
        resizePolicy{};
    resizePolicy.resizableColumnCount =
        3;
    resizePolicy.elasticColumn =
        3;
    resizePolicy.minimumLogicalWidths =
        {
            kFieldColumnMinimumLogical,
            kCurrentColumnMinimumLogical,
            kConvertedColumnMinimumLogical,
            kStatusColumnMinimumLogical,
        };
    ui::ConfigureNextListColumnResize(
        list_,
        resizePolicy);

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

}

void ShortcutPathConverterDialog::ApplyFonts(BOOL repaint) {
    const auto language = app_.SettingsData().language;
    HFONT body = ui::CreateFontHandle(
        ui::ApplicationFontSpec(language, ui::UiFontRole::Body), dpi_);
    if (!body) return;

    HFONT heading = ui::CreateFontHandle(
        ui::ApplicationFontSpec(language, ui::UiFontRole::BodySemibold),
        dpi_);
    if (!heading) {
        DeleteObject(body);
        return;
    }

    const HFONT previousBody = font_;
    const HFONT previousHeading = groupFont_;
    font_ = body;
    groupFont_ = heading;

    for (HWND control : std::array<HWND, 8>{
        modeTitle_, portable_, absolute_, rescan_,
        rule_, list_, status_, apply_}) {
        if (control) {
            SendMessageW(control, WM_SETFONT,
                reinterpret_cast<WPARAM>(font_), repaint);
        }
    }
    if (modeTitle_) {
        SendMessageW(modeTitle_, WM_SETFONT,
            reinterpret_cast<WPARAM>(groupFont_), repaint);
    }
    if (list_) {
        ui::InitializeNextListView(list_, dpi_, font_, groupFont_);
    }

    if (previousBody) DeleteObject(previousBody);
    if (previousHeading) DeleteObject(previousHeading);
}

void ShortcutPathConverterDialog::ApplyLanguage() {
    SetWindowTextW(
        hwnd_,
        T(L"Asterun 路径转换",
          L"Asterun Path Conversion"));

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
        T(L"仅转换目标和工作目录；参数、URL、UNC 路径和裸命令保持不变。",
          L"Only Target and Working Directory are converted; arguments, URLs, UNC paths and bare commands stay unchanged."));

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

    // GDI RoundRect left stepped, uneven corners on these large cards. Use
    // the shared anti-aliased outline at the Settings card radius.
    ui::PaintSmoothRoundedFrame(
        draw.hDC,
        rect,
        surface,
        Scale(ui::kSettingsCardRadiusLogical),
        palette.windowBackground,
        borderColor,
        fillColor);

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

    RECT surface =
        rect;
    InflateRect(
        &surface,
        -1,
        -1);

    // Same smooth outline and radius as the editor and manager buttons.
    ui::PaintSmoothRoundedFrame(
        draw.hDC,
        rect,
        surface,
        Scale(ui::kControlCornerRadiusLogical),
        palette.windowBackground,
        borderColor,
        fillColor);

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

void ShortcutPathConverterDialog::Layout(bool repaint) {
    if (!hwnd_) {
        return;
    }

    const auto moveControl = [repaint](HWND control, int x, int y,
                                       int width, int height) {
        if (repaint) {
            MoveWindow(control, x, y, width, height, TRUE);
        } else {
            SetWindowPos(control, nullptr, x, y, width, height,
                SWP_NOZORDER | SWP_NOACTIVATE |
                SWP_NOREDRAW | SWP_NOCOPYBITS);
        }
    };

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

    moveControl(
        modeTitle_,
        margin,
        y,
        std::max(
            1,
            static_cast<int>(
                client.right) -
                margin * 2),
        titleHeight);

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

    moveControl(
        portable_,
        margin,
        y,
        cardWidth,
        cardHeight);

    moveControl(
        absolute_,
        margin +
            cardWidth +
            gap,
        y,
        cardWidth,
        cardHeight);

    moveControl(
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
        buttonHeight);

    y += cardHeight +
        Scale(10);

    moveControl(
        rule_,
        margin,
        y,
        std::max(
            1,
            static_cast<int>(
                client.right) -
                margin * 2),
        ruleHeight);

    y += ruleHeight +
        Scale(10);

    moveControl(
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
                footerHeight));

    const int footerY =
        static_cast<int>(
            client.bottom) -
        margin -
        buttonHeight;

    moveControl(
        apply_,
        static_cast<int>(
            client.right) -
            margin -
            buttonWidth,
        footerY,
        buttonWidth,
        buttonHeight);

    moveControl(
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
        Scale(20));

    UpdateColumnWidths();
}

std::size_t
ShortcutPathConverterDialog::
SelectedFieldCount() const {
    return static_cast<std::size_t>(
        std::count_if(
            rows_.begin(),
            rows_.end(),
            [](const Row& row) {
                return row.selected;
            }));
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
UpdateColumnWidths() {
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
            Scale(kFieldColumnMinimumLogical),
            Scale(kCurrentColumnMinimumLogical),
            Scale(kConvertedColumnMinimumLogical),
            Scale(kStatusColumnMinimumLogical),
        };
    std::array<int, 3> widths{};

    if (!ui::
            NextListHasUserAdjustedColumns(
                list_)) {
        widths[0] =
            std::max(
                minimums[0],
                Scale(kFieldColumnDefaultLogical));
        widths[1] =
            std::max(
                minimums[1],
                Scale(kCurrentColumnDefaultLogical));
        widths[2] =
            std::max(
                minimums[2],
                Scale(kConvertedColumnDefaultLogical));
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

    const int firstThreeLimit =
        std::max(
            minimums[0] +
                minimums[1] +
                minimums[2],
            contentWidth -
                minimums[3]);
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

    const int statusWidth =
        std::max(
            1,
            contentWidth -
                widths[0] -
                widths[1] -
                widths[2]);
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
                index) != width) {
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
}

void ShortcutPathConverterDialog::InsertGroupHeader(
    std::wstring_view title) {
    const int itemIndex =
        ListView_GetItemCount(list_);

    std::wstring text(title);

    LVITEMW item{};
    item.mask =
        LVIF_TEXT |
        LVIF_PARAM;
    item.iItem = itemIndex;
    item.iSubItem = 0;
    item.pszText = text.data();
    item.lParam = kGroupHeaderItemParam;

    const int inserted =
        ListView_InsertItem(
            list_,
            &item);

    if (inserted >= 0) {
        ListView_SetItemState(
            list_,
            inserted,
            0,
            LVIS_SELECTED);
    }
}

void ShortcutPathConverterDialog::InsertPreviewRow(
    Row row) {
    const std::size_t rowIndex =
        rows_.size();

    row.selected = row.exists;

    rows_.push_back(
        std::move(row));

    const int itemIndex =
        ListView_GetItemCount(list_);

    wchar_t emptyText[] = L"";

    LVITEMW item{};
    item.mask =
        LVIF_TEXT |
        LVIF_PARAM;
    item.iItem = itemIndex;
    item.iSubItem = 0;
    item.pszText = emptyText;
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

const wchar_t*
ShortcutPathConverterDialog::FieldLabel(
    Field field) const {
    switch (field) {
    case Field::Target:
        return T(L"目标", L"Target");
    case Field::WorkingDirectory:
        return T(
            L"工作目录",
            L"Working directory");
    }

    return L"";
}

bool ShortcutPathConverterDialog::GetResultFieldInteractionRect(
    int itemIndex,
    RECT& interactionRect) const {
    if (!list_ ||
        itemIndex < 0 ||
        !RowIndexForListItem(
            itemIndex)) {
        return false;
    }

    RECT rowRect{};
    if (!ListView_GetItemRect(
            list_,
            itemIndex,
            &rowRect,
            LVIR_BOUNDS)) {
        return false;
    }

    HWND header =
        ListView_GetHeader(
            list_);

    if (!header ||
        !Header_GetItemRect(
            header,
            0,
            &interactionRect)) {
        return false;
    }

    interactionRect.top =
        rowRect.top;
    interactionRect.bottom =
        rowRect.bottom;

    return interactionRect.right >
            interactionRect.left &&
        interactionRect.bottom >
            interactionRect.top;
}

bool ShortcutPathConverterDialog::GetResultFieldLayout(
    int itemIndex,
    RECT& checkboxRect,
    RECT& textRect) const {
    RECT fieldRect{};

    if (!GetResultFieldInteractionRect(
            itemIndex,
            fieldRect)) {
        return false;
    }

    const CheckboxRasterTemplate raster =
        CheckboxTemplateForDpi(
            dpi_);

    const double rowCenter =
        (static_cast<double>(
             fieldRect.top) +
         static_cast<double>(
             fieldRect.bottom)) /
        2.0;

    const int checkboxTop =
        static_cast<int>(
            std::lround(
                rowCenter -
                static_cast<double>(
                    raster.size) /
                    2.0));
    // Indent like every other cell's text (and the 字段 header), so the
    // glyph does not crowd the list's left border.
    const int checkboxLeft =
        fieldRect.left +
        ui::NextListCellPadding(
            dpi_);

    checkboxRect = RECT{
        checkboxLeft,
        checkboxTop,
        checkboxLeft +
            raster.size,
        checkboxTop +
            raster.size,
    };

    textRect = fieldRect;
    textRect.left =
        checkboxRect.right +
        raster.textGap;
    textRect.right -=
        raster.textGap;

    return checkboxRect.left >=
            fieldRect.left &&
        checkboxRect.top >=
            fieldRect.top &&
        checkboxRect.right <=
            fieldRect.right &&
        checkboxRect.bottom <=
            fieldRect.bottom &&
        textRect.right >
            textRect.left;
}

void ShortcutPathConverterDialog::DrawResultFieldCell(
    HDC dc,
    int itemIndex) const {
    if (!dc) {
        return;
    }

    const auto rowIndex =
        RowIndexForListItem(
            itemIndex);
    if (!rowIndex) {
        return;
    }

    RECT checkboxRect{};
    RECT textRect{};
    if (!GetResultFieldLayout(
            itemIndex,
            checkboxRect,
            textRect)) {
        return;
    }

    const Row& row =
        rows_[*rowIndex];
    const auto& palette =
        ui::kApplicationPalette;
    const CheckboxRasterTemplate raster =
        CheckboxTemplateForDpi(
            dpi_);

    // Use the row's own background state rather than sampling the DC:
    // GetPixel is CLR_INVALID outside a partial repaint's clip region.
    const bool rowSelected =
        (ListView_GetItemState(
             list_,
             itemIndex,
             LVIS_SELECTED) &
         LVIS_SELECTED) != 0;
    const COLORREF background =
        ui::NextListRowBackground(
            list_,
            itemIndex,
            rowSelected);

    ui::PaintSmoothCheckbox(
        dc,
        checkboxRect.left,
        checkboxRect.top,
        raster.size,
        Scale(ui::kControlCornerRadiusLogical),
        row.selected,
        true,
        false,
        background,
        palette.accent,
        palette.controlBackground);

    const bool listSelected =
        (ListView_GetItemState(
             list_,
             itemIndex,
             LVIS_SELECTED) &
         LVIS_SELECTED) != 0;

    const COLORREF fieldTextColor =
        ui::NextListRowText(
            listSelected);

    SetBkMode(
        dc,
        TRANSPARENT);
    SetTextColor(
        dc,
        fieldTextColor);

    HGDIOBJ previousFont =
        SelectObject(
            dc,
            font_);

    DrawTextW(
        dc,
        FieldLabel(
            row.field),
        -1,
        &textRect,
        DT_LEFT |
            DT_VCENTER |
            DT_SINGLELINE |
            DT_END_ELLIPSIS |
            DT_NOPREFIX);

    SelectObject(
        dc,
        previousFont);
}

bool ShortcutPathConverterDialog::ToggleResultRowSelection(
    int itemIndex) {
    const auto rowIndex =
        RowIndexForListItem(
            itemIndex);
    if (!rowIndex) {
        return false;
    }

    rows_[*rowIndex].selected =
        !rows_[*rowIndex].selected;

    RECT rowRect{};
    if (ListView_GetItemRect(
            list_,
            itemIndex,
            &rowRect,
            LVIR_BOUNDS)) {
        InvalidateRect(
            list_,
            &rowRect,
            FALSE);
    } else {
        InvalidateRect(
            list_,
            nullptr,
            FALSE);
    }

    UpdateSelectionState();
    return true;
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

        RECT row{};
        if (!ListView_GetItemRect(
                list_,
                itemIndex,
                &row,
                LVIR_BOUNDS)) {
            return CDRF_DODEFAULT;
        }

        RECT client{};
        GetClientRect(
            list_,
            &client);
        row.left =
            client.left;
        row.right =
            client.right;

        // Paint the whole row off-screen and copy it once. ListView can repaint
        // its item (column 0) area outside its own double buffer on hover; a
        // direct fill -> text -> checkbox sequence then flickers on screen.
        RowPaintBuffer buffer(draw->nmcd.hdc, row);
        const HDC rowDc = buffer.Dc();

        if (IsGroupHeaderItem(
                itemIndex)) {
            HBRUSH background =
                CreateSolidBrush(
                    RGB(251, 252, 253));
            FillRect(
                rowDc,
                &row,
                background);
            DeleteObject(
                background);

            std::array<wchar_t, 512>
                title{};

            ListView_GetItemText(
                list_,
                itemIndex,
                0,
                title.data(),
                static_cast<int>(
                    title.size()));

            RECT textRect = row;
            const int padding =
                ui::NextListCellPadding(
                    dpi_);
            textRect.left +=
                padding + Scale(2);
            textRect.right -=
                padding;

            SetBkMode(
                rowDc,
                TRANSPARENT);
            SetTextColor(
                rowDc,
                palette.text);

            HGDIOBJ previousFont =
                SelectObject(
                    rowDc,
                    groupFont_
                        ? groupFont_
                        : font_);

            DrawTextW(
                rowDc,
                title.data(),
                -1,
                &textRect,
                DT_LEFT |
                    DT_VCENTER |
                    DT_SINGLELINE |
                    DT_END_ELLIPSIS |
                    DT_NOPREFIX);

            SelectObject(
                rowDc,
                previousFont);

            ui::DrawNextListRowSeparator(
                rowDc,
                row);

            return CDRF_SKIPDEFAULT;
        }

        const bool selected =
            (ListView_GetItemState(
                 list_,
                 itemIndex,
                 LVIS_SELECTED) &
             LVIS_SELECTED) != 0;

        HBRUSH background =
            CreateSolidBrush(
                ui::NextListRowBackground(
                    list_,
                    itemIndex,
                    selected));
        FillRect(
            rowDc,
            &row,
            background);
        DeleteObject(
            background);

        SetBkMode(
            rowDc,
            TRANSPARENT);
        SetTextColor(
            rowDc,
            ui::NextListRowText(
                selected));

        HGDIOBJ oldFont =
            SelectObject(
                rowDc,
                font_);

        const int padding =
            ui::NextListCellPadding(
                dpi_);
        int x = row.left;

        for (int column = 0;
             column < 4;
             ++column) {
            const int width =
                ListView_GetColumnWidth(
                    list_,
                    column);

            if (column > 0) {
                RECT cell{
                    x + padding,
                    row.top,
                    x +
                        width -
                        padding,
                    row.bottom,
                };

                wchar_t text[2048]{};
                ListView_GetItemText(
                    list_,
                    itemIndex,
                    column,
                    text,
                    static_cast<int>(
                        std::size(text)));

                DrawTextW(
                    rowDc,
                    text,
                    -1,
                    &cell,
                    DT_LEFT |
                        DT_VCENTER |
                        DT_SINGLELINE |
                        DT_END_ELLIPSIS |
                        DT_NOPREFIX);
            }

            x += width;
        }

        SelectObject(
            rowDc,
            oldFont);

        DrawResultFieldCell(
            rowDc,
            itemIndex);

        ui::DrawNextListRowSeparator(
            rowDc,
            row);

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

    for (const Row& row :
         rows_) {
        if (!row.selected) {
            continue;
        }

        ++selectedFieldCount;

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
        }
    }

    if (updates.empty()) {
        UpdateSelectionState();
        return;
    }

    if (!app_.ApplyUserCommandPathUpdates(
            updates)) {
        altrun::ui::ShowMessage(
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
    if (ui::HandleFocusCueMessage(hwnd_, message, wParam)) {
        return 0;
    }

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

    case WM_DPICHANGED: {
        const UINT nextDpi = HIWORD(wParam);
        if (nextDpi == 0) return 0;
        dpi_ = nextDpi;
        const auto* suggested = reinterpret_cast<const RECT*>(lParam);
        if (suggested) {
            dpiLayoutInProgress_ = true;
            SetWindowPos(hwnd_, nullptr, suggested->left, suggested->top,
                suggested->right - suggested->left,
                suggested->bottom - suggested->top,
                SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOREDRAW);
            dpiLayoutInProgress_ = false;
        }
        ApplyFonts(FALSE);
        Layout(false);
        RedrawWindow(hwnd_, nullptr, nullptr,
            RDW_INVALIDATE | RDW_NOERASE |
            RDW_ALLCHILDREN | RDW_UPDATENOW);
        return 0;
    }

    case WM_SIZE:
        if (!dpiLayoutInProgress_) Layout();
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
        // The shared Header subclass owns visible Header painting and all
        // resize gestures. Header notifications are not a resize control flow.
        // Swallow them so they never fall through into ListView handlers.
        const auto* headerNotification =
            reinterpret_cast<NMHDR*>(
                lParam);
        if (headerNotification &&
            headerNotification->hwndFrom ==
                ListView_GetHeader(
                    list_)) {
            return 0;
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
                NM_CLICK ||
            header->code ==
                NM_DBLCLK) {
            const auto* click =
                reinterpret_cast<
                    NMITEMACTIVATE*>(
                        lParam);

            if (click) {
                const int itemCount =
                    ListView_GetItemCount(
                        list_);

                for (int itemIndex = 0;
                     itemIndex < itemCount;
                     ++itemIndex) {
                    RECT interactionRect{};

                    if (GetResultFieldInteractionRect(
                            itemIndex,
                            interactionRect) &&
                        PtInRect(
                            &interactionRect,
                            click->ptAction)) {
                        ToggleResultRowSelection(
                            itemIndex);
                        break;
                    }
                }
            }

            return 0;
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
                     LVIS_SELECTED) !=
                    0) {
                    return TRUE;
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

    case WM_NCDESTROY: {
        // Let USER32 finish its own per-window teardown before the object
        // forgets the handle, matching Settings and the Shortcut Manager.
        const HWND destroyedWindow = hwnd_;
        hwnd_ = nullptr;
        closed_ = true;
        return DefWindowProcW(
            destroyedWindow, message, wParam, lParam);
    }

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
