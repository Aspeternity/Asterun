#include "Feedback.hpp"
#include "AppIcon.hpp"
#include "SettingsWindow.hpp"

#include "TopLevelWindowPresentation.hpp"
#include "UiComboBox.hpp"
#include "UiTheme.hpp"
#include "UiTypography.hpp"

#include "../app/App.hpp"
#include "../core/HotkeyRegistry.hpp"
#include "../platform/Hotkey.hpp"
#include "Version.hpp"

#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <shlobj.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <iterator>
#include <optional>
#include <sstream>
#include <string>

namespace altrun {

namespace {

constexpr wchar_t kSettingsClass[] = L"Asterun.Settings";
constexpr wchar_t kSettingsTitle[] = L"Asterun Settings";

constexpr DWORD kSettingsWindowExStyle =
    WS_EX_APPWINDOW;
constexpr DWORD kSettingsWindowStyle =
    WS_CAPTION |
    WS_SYSMENU |
    WS_MINIMIZEBOX |
    WS_CLIPCHILDREN;

struct SettingsCreationGeometry {
    RECT outer{};
    UINT dpi{96};
};

[[nodiscard]] UINT ProbeMonitorDpi(
    HINSTANCE instance,
    const MONITORINFO& info) {

    const int monitorWidth =
        info.rcMonitor.right -
        info.rcMonitor.left;
    const int monitorHeight =
        info.rcMonitor.bottom -
        info.rcMonitor.top;

    const int x =
        info.rcMonitor.left +
        std::max(
            0,
            monitorWidth / 2);
    const int y =
        info.rcMonitor.top +
        std::max(
            0,
            monitorHeight / 2);

    // GetDpiForWindow is the DPI-aware API Microsoft recommends for a
    // PerMonitorV2 thread, but the real Settings HWND must not be created at
    // a temporary monitor-origin position just to discover its DPI. Use a
    // never-visible tool-window probe on the target monitor, read its DPI,
    // then destroy it before the real Settings HWND exists.
    HWND probe =
        CreateWindowExW(
            WS_EX_TOOLWINDOW |
                WS_EX_NOACTIVATE,
            L"STATIC",
            L"",
            WS_POPUP,
            x,
            y,
            1,
            1,
            nullptr,
            nullptr,
            instance,
            nullptr);

    if (!probe) {
        return 96;
    }

    const UINT dpi =
        GetDpiForWindow(probe);

    DestroyWindow(probe);

    return dpi != 0
        ? dpi
        : 96;
}

[[nodiscard]]
SettingsCreationGeometry
ResolveSettingsCreationGeometry(
    const Settings& settings,
    HINSTANCE instance) {

    const bool useLast =
        settings.settingsPlacement ==
            "last" &&
        settings.settingsLastPositionValid;

    POINT anchor{};

    if (useLast) {
        anchor.x =
            settings.settingsLastX;
        anchor.y =
            settings.settingsLastY;
    } else if (!GetCursorPos(
                   &anchor)) {
        anchor = {0, 0};
    }

    const HMONITOR monitor =
        MonitorFromPoint(
            anchor,
            MONITOR_DEFAULTTONEAREST);

    MONITORINFO info{
        sizeof(info)};

    if (!monitor ||
        !GetMonitorInfoW(
            monitor,
            &info)) {
        SettingsCreationGeometry fallback;

        RECT outer{
            0,
            0,
            ui::Scale(
                ui::kSettingsClientWidthLogical,
                fallback.dpi),
            ui::Scale(
                ui::kSettingsClientHeightLogical,
                fallback.dpi),
        };

        AdjustWindowRectExForDpi(
            &outer,
            kSettingsWindowStyle,
            FALSE,
            kSettingsWindowExStyle,
            fallback.dpi);

        fallback.outer = {
            anchor.x,
            anchor.y,
            anchor.x +
                (outer.right -
                 outer.left),
            anchor.y +
                (outer.bottom -
                 outer.top),
        };
        return fallback;
    }

    SettingsCreationGeometry geometry;
    geometry.dpi =
        ProbeMonitorDpi(
            instance,
            info);

    RECT outer{
        0,
        0,
        ui::Scale(
            ui::kSettingsClientWidthLogical,
            geometry.dpi),
        ui::Scale(
            ui::kSettingsClientHeightLogical,
            geometry.dpi),
    };

    AdjustWindowRectExForDpi(
        &outer,
        kSettingsWindowStyle,
        FALSE,
        kSettingsWindowExStyle,
        geometry.dpi);

    const int width =
        std::min(
            outer.right -
                outer.left,
            info.rcWork.right -
                info.rcWork.left);
    const int height =
        std::min(
            outer.bottom -
                outer.top,
            info.rcWork.bottom -
                info.rcWork.top);

    if (useLast) {
        const auto clamped =
            settings_layout::
                ClampRectToWorkArea(
                    {
                        settings.settingsLastX,
                        settings.settingsLastY,
                        settings.settingsLastX +
                            width,
                        settings.settingsLastY +
                            height,
                    },
                    {
                        info.rcWork.left,
                        info.rcWork.top,
                        info.rcWork.right,
                        info.rcWork.bottom,
                    });

        geometry.outer = {
            clamped.left,
            clamped.top,
            clamped.right,
            clamped.bottom,
        };
    } else {
        const auto origin =
            settings_layout::
                ResolveWindowOrigin(
                    {
                        static_cast<int>(
                            info.rcWork.left),
                        static_cast<int>(
                            info.rcWork.top),
                        static_cast<int>(
                            info.rcWork.right),
                        static_cast<int>(
                            info.rcWork.bottom),
                    },
                    width,
                    height,
                    settings.settingsPlacement ==
                        "top",
                    ui::Scale(
                        45,
                        geometry.dpi));

        geometry.outer = {
            origin.x,
            origin.y,
            origin.x + width,
            origin.y + height,
        };
    }

    return geometry;
}

void HideSettingsVerticalScrollBar(
    HWND hwnd) noexcept {

    if (!hwnd) {
        return;
    }

    // Clear the range first so USER32 has no scroll state that can revive the
    // non-client bar on a later FRAMECHANGED / DPI / resize pass.
    SCROLLINFO cleared{};
    cleared.cbSize =
        sizeof(cleared);
    cleared.fMask =
        SIF_RANGE |
        SIF_PAGE |
        SIF_POS;
    cleared.nMin = 0;
    cleared.nMax = 0;
    cleared.nPage = 1;
    cleared.nPos = 0;

    SetScrollInfo(
        hwnd,
        SB_VERT,
        &cleared,
        FALSE);

    LONG_PTR style =
        GetWindowLongPtrW(
            hwnd,
            GWL_STYLE);

    if ((style & WS_VSCROLL) != 0) {
        SetWindowLongPtrW(
            hwnd,
            GWL_STYLE,
            style & ~WS_VSCROLL);
    }

    ShowScrollBar(
        hwnd,
        SB_VERT,
        FALSE);
}

constexpr const auto& kPalette =
    ui::kApplicationPalette;
constexpr COLORREF kWindowBackground =
    kPalette.windowBackground;
constexpr COLORREF kSidebarBackground =
    kPalette.sidebarBackground;
constexpr COLORREF kCardBackground =
    kPalette.cardBackground;
constexpr COLORREF kCardPressed =
    kPalette.pressedBackground;
constexpr COLORREF kBorder =
    kPalette.frame;
constexpr COLORREF kText =
    kPalette.text;
constexpr COLORREF kMuted =
    kPalette.mutedText;
constexpr COLORREF kAccent =
    kPalette.accent;

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

} // namespace

SettingsWindow::SettingsWindow(App& app, HINSTANCE instance)
    : app_(app), instance_(instance) {}

SettingsWindow::~SettingsWindow() {
    if (hwnd_ &&
        IsWindow(hwnd_)) {
        DestroyWindow(hwnd_);
    }

    ReleaseWindowResources();
}

const wchar_t* SettingsWindow::T(
    const wchar_t* zh,
    const wchar_t* en) const {

    return app_.SettingsData().language == Language::ZhCN ? zh : en;
}

int SettingsWindow::Scale(int value) const {
    return ui::Scale(
        value,
        dpi_);
}

bool SettingsWindow::EnsureCreated() {
    if (hwnd_ &&
        IsWindow(hwnd_)) {
        return true;
    }

    if (Create()) {
        return true;
    }

    altrun::ui::ShowMessage(
        nullptr,
        T(L"无法创建设置窗口。",
          L"Could not create the Settings window."),
        L"Asterun",
        MB_OK | MB_ICONERROR);
    return false;
}

void SettingsWindow::
ResetWindowInstanceState() {
    page_ = Page::General;
    syncing_ = false;
    generalScrollOffset_ = 0;
    hotkeyScrollOffset_ = 0;
    capturingHotkeyActionId_.clear();
    pendingProviderStates_.clear();
    providerCommitInProgress_ = false;
    providerTrayVisible_ = false;
    providerActionsVisible_ = false;

    hotkeyRows_.clear();
    generalControls_.clear();
    hotkeyControls_.clear();
    appearanceControls_.clear();
    providerControls_.clear();
    dataControls_.clear();
    aboutControls_.clear();

    // These controls are touched by asynchronous App callbacks. Clear them
    // as soon as the HWND is gone so a recycled native handle is never used.
    providerStatus_ = nullptr;
    managedEverythingTrayIcon_ = nullptr;
    providerGetEverything_ = nullptr;
    providerUpdateEverything_ = nullptr;
    providerRecheckEverything_ = nullptr;
    dataStatus_ = nullptr;
    updateStatus_ = nullptr;
    updateAction_ = nullptr;
}

void SettingsWindow::
ReleaseWindowResources() {
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
    if (backgroundBrush_) {
        DeleteObject(backgroundBrush_);
        backgroundBrush_ = nullptr;
    }
    if (sidebarBrush_) {
        DeleteObject(sidebarBrush_);
        sidebarBrush_ = nullptr;
    }
    if (cardBrush_) {
        DeleteObject(cardBrush_);
        cardBrush_ = nullptr;
    }
}

bool SettingsWindow::Create() {
    if (hwnd_ &&
        IsWindow(hwnd_)) {
        return true;
    }

    ResetWindowInstanceState();
    ReleaseWindowResources();
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
    wc.hIcon =
        ui::LoadApplicationIcon(
            instance_);
    wc.hIconSm =
        ui::LoadApplicationIcon(
            instance_,
            true);
    wc.hbrBackground = nullptr;

    if (!RegisterClassExW(&wc) &&
        GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return false;
    }

    // Resolve the target monitor, target DPI, exact fixed-client outer size
    // and final Center/Last rectangle *before* the real Settings HWND exists.
    // This is deliberate: older revisions created the real HWND at the
    // monitor work-area origin and moved it while hidden. USER32/DWM could
    // still retain that birth rectangle for a first/last transition frame.
    // The real Settings HWND now has no upper-left intermediate placement.
    const auto creation =
        ResolveSettingsCreationGeometry(
            app_.SettingsData(),
            instance_);

    hwnd_ = CreateWindowExW(
        kSettingsWindowExStyle,
        kSettingsClass,
        kSettingsTitle,
        kSettingsWindowStyle,
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

    // Size from the desired client viewport rather than treating an
    // outer-window size as if it were client geometry.
    RECT desiredWindow{
        0,
        0,
        Scale(
            ui::kSettingsClientWidthLogical),
        Scale(
            ui::kSettingsClientHeightLogical),
    };

    AdjustWindowRectExForDpi(
        &desiredWindow,
        kSettingsWindowStyle,
        FALSE,
        kSettingsWindowExStyle,
        dpi_);

    const int desiredOuterWidth =
        desiredWindow.right -
        desiredWindow.left;
    const int desiredOuterHeight =
        desiredWindow.bottom -
        desiredWindow.top;

    RECT createdRect{};
    GetWindowRect(
        hwnd_,
        &createdRect);

    if ((createdRect.right -
         createdRect.left) !=
            desiredOuterWidth ||
        (createdRect.bottom -
         createdRect.top) !=
            desiredOuterHeight) {
        // Probe DPI and actual HWND DPI should normally match. If Windows
        // resolves a different DPI context, correct only the hidden size;
        // PositionForShow below will recenter/clamp with that settled size.
        SetWindowPos(
            hwnd_,
            nullptr,
            0,
            0,
            desiredOuterWidth,
            desiredOuterHeight,
            SWP_NOMOVE |
                SWP_NOZORDER |
                SWP_NOACTIVATE);
    }

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

    // The real HWND was already born at the intended rectangle. Re-resolve
    // once after actual GetDpiForWindow/outer-size settlement in case Windows
    // adjusted the DPI context, but keep the window hidden until Show().
    PositionForShow();

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

void SettingsWindow::CreateControls() {
    brandName_ =
        CreateStatic(
            L"Asterun",
            SS_CENTER | SS_NOPREFIX);
    brandSubtitle_ =
        CreateStatic(
            L"Launch Faster, Go Further",
            SS_CENTER | SS_NOPREFIX);

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
    navProviders_ =
        CreateButton(
            L"",
            kIdNavProviders,
            BS_OWNERDRAW);
    navAppearance_ =
        CreateButton(
            L"",
            kIdNavAppearance,
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

    pageTitle_ =
        CreateStatic(
            L"",
            SS_LEFT | SS_NOPREFIX);
    pageDescription_ =
        CreateStatic(
            L"",
            SS_LEFT | SS_NOPREFIX);

    CreateGeneralPage();
    CreateHotkeyPage();
    CreateProviderPage();
    CreateAppearancePage();
    CreateDataPage();
    CreateAboutPage();
}

void SettingsWindow::CreateGeneralPage() {
    generalBehaviorTitle_ = CreateStatic(L"");
    startWithWindows_ = CreateCheckboxRow(L"", kIdStartWithWindows);

    startupBehaviorLabel_ = CreateStatic(L"");
    startupBehavior_ =
        ui::CreateNextComboBox(
            hwnd_,
            instance_,
            kIdStartupBehavior,
            kCardBackground);

    showTrayIcon_ = CreateCheckboxRow(L"", kIdShowTrayIcon);
    soundEnabled_ = CreateCheckboxRow(L"", kIdSoundEnabled);
    addToSendToMenu_ = CreateCheckboxRow(L"", kIdAddToSendToMenu);

    searchBehaviorTitle_ = CreateStatic(L"");
    pinyinSearch_ = CreateCheckboxRow(L"", kIdPinyinSearch);
    numericQuickLaunch_ = CreateCheckboxRow(L"", kIdNumericQuickLaunch);
    executeSingleResult_ = CreateCheckboxRow(L"", kIdExecuteSingleResult);

    placementSectionTitle_ = CreateStatic(L"");
    popupMonitorLabel_ = CreateStatic(L"");
    popupMonitorDescription_ = CreateStatic(L"", SS_LEFT | SS_NOPREFIX);
    popupMonitor_ =
        ui::CreateNextComboBox(
            hwnd_,
            instance_,
            kIdPopupMonitor,
            kCardBackground);

    launcherPlacementLabel_ = CreateStatic(L"");
    launcherPlacementDescription_ = CreateStatic(L"", SS_LEFT | SS_NOPREFIX);
    launcherPlacement_ =
        ui::CreateNextComboBox(
            hwnd_,
            instance_,
            kIdLauncherPlacement,
            kCardBackground);

    settingsPlacementLabel_ = CreateStatic(L"");
    settingsPlacementDescription_ = CreateStatic(L"", SS_LEFT | SS_NOPREFIX);
    settingsPlacement_ =
        ui::CreateNextComboBox(
            hwnd_,
            instance_,
            kIdSettingsPlacement,
            kCardBackground);

    shortcutManagerPlacementLabel_ = CreateStatic(L"");
    shortcutManagerPlacementDescription_ = CreateStatic(L"", SS_LEFT | SS_NOPREFIX);
    shortcutManagerPlacement_ =
        ui::CreateNextComboBox(
            hwnd_,
            instance_,
            kIdShortcutManagerPlacement,
            kCardBackground);

    generalNote_ = CreateStatic(L"", SS_LEFT | SS_NOPREFIX);

    generalControls_ = {
        generalBehaviorTitle_, startWithWindows_,
        startupBehaviorLabel_, startupBehavior_,
        showTrayIcon_, soundEnabled_, addToSendToMenu_,
        searchBehaviorTitle_, pinyinSearch_,
        numericQuickLaunch_, executeSingleResult_,
        placementSectionTitle_,
        popupMonitorLabel_, popupMonitorDescription_, popupMonitor_,
        launcherPlacementLabel_, launcherPlacementDescription_, launcherPlacement_,
        settingsPlacementLabel_, settingsPlacementDescription_, settingsPlacement_,
        shortcutManagerPlacementLabel_, shortcutManagerPlacementDescription_,
        shortcutManagerPlacement_, generalNote_,
    };
}


void SettingsWindow::CreateHotkeyPage() {
    hotkeyGlobalTitle_ =
        CreateStatic(L"");
    hotkeyLauncherTitle_ =
        CreateStatic(L"");

    hotkeyRows_.clear();
    hotkeyRows_.reserve(
        HotkeyActionRegistry().size());

    std::size_t index = 0;

    for (const auto& action :
         HotkeyActionRegistry()) {
        HotkeyRowControls row;
        row.actionId = action.id;

        row.title =
            CreateStatic(
                L"",
                SS_LEFT |
                    SS_NOPREFIX);

        row.capture =
            CreateButton(
                L"",
                kIdHotkeyCaptureBase +
                    static_cast<UINT>(
                        index));

        if (!action.required) {
            row.enabled =
                CreateButton(
                    L"",
                    kIdHotkeyEnabledBase +
                        static_cast<UINT>(
                            index),
                    BS_OWNERDRAW);
        }

        row.reset =
            CreateButton(
                L"",
                kIdHotkeyResetBase +
                    static_cast<UINT>(
                        index));

        row.status =
            CreateStatic(
                L"",
                SS_LEFT |
                    SS_NOPREFIX);

        hotkeyRows_.push_back(
            std::move(row));

        ++index;
    }

    hotkeyResetAll_ =
        CreateButton(
            L"",
            kIdHotkeyResetAll);

    hotkeyControls_ = {
        hotkeyGlobalTitle_,
        hotkeyLauncherTitle_,
        hotkeyResetAll_,
    };

    for (const auto& row :
         hotkeyRows_) {
        hotkeyControls_.push_back(
            row.title);
        hotkeyControls_.push_back(
            row.capture);
        if (row.enabled) {
            hotkeyControls_.push_back(
                row.enabled);
        }
        hotkeyControls_.push_back(
            row.reset);
        hotkeyControls_.push_back(
            row.status);
    }
}

void SettingsWindow::CreateAppearancePage() {
    appearanceLauncherTitle_ =
        CreateStatic(L"");

    uiStyleLabel_ =
        CreateStatic(L"");

    uiStyle_ =
        ui::CreateNextComboBox(
            hwnd_,
            instance_,
            kIdUiStyle,
            kCardBackground);

    appearanceAppTitle_ =
        CreateStatic(L"");

    languageLabel_ =
        CreateStatic(L"");

    language_ =
        ui::CreateNextComboBox(
            hwnd_,
            instance_,
            kIdLanguage,
            kCardBackground);

    appearanceNote_ =
        CreateStatic(
            L"",
            SS_LEFT | SS_NOPREFIX);

    appearanceControls_ = {
        appearanceLauncherTitle_,
        uiStyleLabel_,
        uiStyle_,
        appearanceAppTitle_,
        languageLabel_,
        language_,
        appearanceNote_,
    };
}

void SettingsWindow::CreateProviderPage() {
    providerSectionTitle_ =
        CreateStatic(L"");
    providerFilesTitle_ =
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
    managedEverythingTrayIcon_ =
        CreateCheckboxRow(
            L"",
            kIdManagedEverythingTrayIcon);

    providerStatus_ =
        CreateStatic(
            L"",
            SS_LEFT | SS_NOPREFIX);

    providerGetEverything_ =
        CreateButton(
            L"",
            kIdProviderGetEverything);
    providerUpdateEverything_ =
        CreateButton(
            L"",
            kIdProviderUpdateEverything);
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
        providerFilesTitle_,
        providerStartMenu_,
        providerPackaged_,
        providerAppPaths_,
        providerPath_,
        providerEverything_,
        managedEverythingTrayIcon_,
        providerStatus_,
        providerGetEverything_,
        providerUpdateEverything_,
        providerRecheckEverything_,
        providerNote_,
    };
}

void SettingsWindow::CreateDataPage() {
    dataPathLabel_ =
        CreateStatic(L"");
    dataPath_ =
        CreateStatic(
            L"",
            SS_LEFT |
                SS_PATHELLIPSIS |
                SS_NOPREFIX);
    openDataFolder_ =
        CreateButton(
            L"",
            kIdOpenDataFolder);

    dataTransferLabel_ =
        CreateStatic(L"");
    dataImportTsv_ =
        CreateButton(
            L"",
            kIdDataImportTsv);
    dataExport_ =
        CreateButton(
            L"",
            kIdDataExport);

    dataMaintenanceLabel_ =
        CreateStatic(L"");
    dataClearUsage_ =
        CreateButton(
            L"",
            kIdDataClearUsage);
    dataRebuildIndex_ =
        CreateButton(
            L"",
            kIdDataRebuildIndex);
    dataResetSettings_ =
        CreateButton(
            L"",
            kIdDataResetSettings);

    dataStatus_ =
        CreateStatic(
            L"",
            SS_LEFT | SS_NOPREFIX);

    dataControls_ = {
        dataPathLabel_,
        dataPath_,
        openDataFolder_,
        dataTransferLabel_,
        dataImportTsv_,
        dataExport_,
        dataMaintenanceLabel_,
        dataClearUsage_,
        dataRebuildIndex_,
        dataResetSettings_,
        dataStatus_,
    };
}

void SettingsWindow::CreateAboutPage() {
    aboutName_ =
        CreateStatic(
            L"Asterun");
    aboutVersion_ =
        CreateStatic(
            L"",
            SS_LEFT |
                SS_CENTERIMAGE |
                SS_NOPREFIX);
    aboutDescription_ =
        CreateStatic(
            L"",
            SS_LEFT | SS_NOPREFIX);

    updateSectionTitle_ =
        CreateStatic(L"");

    updateAutoCheck_ =
        CreateCheckboxRow(
            L"",
            kIdUpdateAutoCheck);
    updatePrerelease_ =
        CreateCheckboxRow(
            L"",
            kIdUpdatePrerelease);

    updateStatus_ =
        CreateStatic(
            L"",
            SS_OWNERDRAW |
                SS_NOPREFIX);

    updateAction_ =
        CreateButton(
            L"",
            kIdUpdateAction);

    openGitHub_ =
        CreateButton(
            L"",
            kIdOpenGitHub);

    aboutControls_ = {
        aboutName_,
        aboutVersion_,
        aboutDescription_,
        updateSectionTitle_,
        updateAutoCheck_,
        updatePrerelease_,
        updateStatus_,
        updateAction_,
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

    const auto language =
        app_.SettingsData().language;

    normalFont_ =
        ui::CreateFontHandle(
            ui::ApplicationFontSpec(
                language,
                ui::UiFontRole::Body),
            dpi_);
    sectionFont_ =
        ui::CreateFontHandle(
            ui::ApplicationFontSpec(
                language,
                ui::UiFontRole::SectionTitle),
            dpi_);
    titleFont_ =
        ui::CreateFontHandle(
            ui::ApplicationFontSpec(
                language,
                ui::UiFontRole::PageTitle),
            dpi_);
    appNameFont_ =
        ui::CreateFontHandle(
            ui::ApplicationFontSpec(
                language,
                ui::UiFontRole::AppTitle),
            dpi_);

    const std::vector<HWND> normalControls{
        navGeneral_,
        navHotkeys_,
        navProviders_,
        navAppearance_,
        navData_,
        navAbout_,
        pageDescription_,
        startWithWindows_,
        startupBehaviorLabel_,
        startupBehavior_,
        showTrayIcon_,
        soundEnabled_,
        addToSendToMenu_,
        pinyinSearch_,
        numericQuickLaunch_,
        executeSingleResult_,
        popupMonitorLabel_,
        popupMonitorDescription_,
        popupMonitor_,
        launcherPlacementLabel_,
        launcherPlacementDescription_,
        launcherPlacement_,
        settingsPlacementLabel_,
        settingsPlacementDescription_,
        settingsPlacement_,
        shortcutManagerPlacementLabel_,
        shortcutManagerPlacementDescription_,
        shortcutManagerPlacement_,
        generalNote_,
        hotkeyResetAll_,
        providerStartMenu_,
        providerPackaged_,
        providerAppPaths_,
        providerPath_,
        providerEverything_,
        providerStatus_,
        providerGetEverything_,
        providerRecheckEverything_,
        providerNote_,
        uiStyleLabel_,
        uiStyle_,
        languageLabel_,
        language_,
        appearanceNote_,
        dataPath_,
        openDataFolder_,
        dataImportTsv_,
        dataExport_,
        dataClearUsage_,
        dataRebuildIndex_,
        dataResetSettings_,
        dataStatus_,
        aboutVersion_,
        aboutDescription_,
        updateAutoCheck_,
        updatePrerelease_,
        updateStatus_,
        updateAction_,
        openGitHub_,
    };

    for (HWND control :
         normalControls) {
        if (control) {
            SendMessageW(
                control,
                WM_SETFONT,
                reinterpret_cast<WPARAM>(
                    normalFont_),
                TRUE);
        }
    }

    for (const auto& row :
         hotkeyRows_) {
        for (HWND control :
             std::array<HWND, 4>{
                 row.capture,
                 row.enabled,
                 row.reset,
                 row.status}) {
            if (control) {
                SendMessageW(
                    control,
                    WM_SETFONT,
                    reinterpret_cast<WPARAM>(
                        normalFont_),
                    TRUE);
            }
        }

        if (row.title) {
            SendMessageW(
                row.title,
                WM_SETFONT,
                reinterpret_cast<WPARAM>(
                    normalFont_),
                TRUE);
        }
    }

    for (HWND control :
         std::array<HWND, 13>{
             generalBehaviorTitle_,
             searchBehaviorTitle_,
             placementSectionTitle_,
             hotkeyGlobalTitle_,
             hotkeyLauncherTitle_,
             providerSectionTitle_,
             providerFilesTitle_,
             appearanceLauncherTitle_,
             appearanceAppTitle_,
             dataPathLabel_,
             dataTransferLabel_,
             dataMaintenanceLabel_,
             updateSectionTitle_}) {
        if (control) {
            SendMessageW(
                control,
                WM_SETFONT,
                reinterpret_cast<WPARAM>(
                    sectionFont_),
                TRUE);
        }
    }

    if (pageTitle_) {
        SendMessageW(
            pageTitle_,
            WM_SETFONT,
            reinterpret_cast<WPARAM>(
                titleFont_),
            TRUE);
    }

    if (brandSubtitle_) {
        SendMessageW(
            brandSubtitle_,
            WM_SETFONT,
            reinterpret_cast<WPARAM>(
                titleFont_),
            TRUE);
    }

    for (HWND control :
         std::array<HWND, 2>{
             brandName_,
             aboutName_}) {
        if (control) {
            SendMessageW(
                control,
                WM_SETFONT,
                reinterpret_cast<WPARAM>(
                    appNameFont_),
                TRUE);
        }
    }

    for (HWND combo :
         std::array<HWND, 7>{
             startupBehavior_,
             popupMonitor_,
             launcherPlacement_,
             settingsPlacement_,
             shortcutManagerPlacement_,
             uiStyle_,
             language_}) {
        ui::ApplyNextComboBoxMetrics(
            combo,
            dpi_);
    }
}

void SettingsWindow::ApplyLanguage() {
    if (!hwnd_) return;

    const bool oldSyncing =
        syncing_;
    syncing_ = true;

    SetWindowTextW(
        hwnd_,
        T(L"Asterun 设置",
          L"Asterun Settings"));
    SetWindowTextW(
        brandName_,
        L"Asterun");
    SetWindowTextW(
        brandSubtitle_,
        L"Launch Faster, Go Further");
    SetWindowTextW(
        generalBehaviorTitle_,
        T(L"Windows 与启动", L"Windows & startup"));
    SetWindowTextW(startWithWindows_, T(L"开机启动", L"Start with Windows"));
    SetWindowTextW(startupBehaviorLabel_, T(L"启动行为", L"Startup behavior"));
    SendMessageW(startupBehavior_, CB_RESETCONTENT, 0, 0);
    SendMessageW(startupBehavior_, CB_ADDSTRING, 0,
        reinterpret_cast<LPARAM>(T(L"静默启动", L"Start silently")));
    SendMessageW(startupBehavior_, CB_ADDSTRING, 0,
        reinterpret_cast<LPARAM>(T(L"显示启动通知", L"Show startup notification")));
    SendMessageW(startupBehavior_, CB_ADDSTRING, 0,
        reinterpret_cast<LPARAM>(T(L"显示启动器", L"Show launcher")));
    SetWindowTextW(soundEnabled_, T(L"提示音", L"Sound effects"));
    SetWindowTextW(showTrayIcon_, T(L"显示系统托盘图标", L"Show system tray icon"));
    SetWindowTextW(addToSendToMenu_, T(L"添加到“发送到”菜单", L"Add to “Send to” menu"));

    SetWindowTextW(searchBehaviorTitle_, T(L"搜索与执行", L"Search & execution"));
    SetWindowTextW(pinyinSearch_, T(L"启用拼音搜索", L"Enable Pinyin search"));
    SetWindowTextW(numericQuickLaunch_, T(L"数字键快速执行结果", L"Quick launch with number keys"));
    SetWindowTextW(executeSingleResult_,
        T(L"仅剩一个结果时立即执行", L"Execute immediately when one result remains"));

    SetWindowTextW(
        placementSectionTitle_,
        T(L"窗口位置",
          L"Window placement"));
    SetWindowTextW(
        popupMonitorLabel_,
        T(L"启动器显示器",
          L"Launcher monitor"));
    SetWindowTextW(
        popupMonitorDescription_,
        L"");

    SendMessageW(
        popupMonitor_,
        CB_RESETCONTENT, 0, 0);
    SendMessageW(
        popupMonitor_,
        CB_ADDSTRING, 0,
        reinterpret_cast<LPARAM>(
            T(L"鼠标所在显示器",
              L"Monitor containing the mouse")));
    SendMessageW(
        popupMonitor_,
        CB_ADDSTRING, 0,
        reinterpret_cast<LPARAM>(
            T(L"活动窗口所在显示器",
              L"Monitor containing the active window")));
    SendMessageW(
        popupMonitor_,
        CB_ADDSTRING, 0,
        reinterpret_cast<LPARAM>(
            T(L"主显示器",
              L"Primary monitor")));

    SetWindowTextW(
        launcherPlacementLabel_,
        T(L"启动器窗口位置",
          L"Launcher window position"));
    SetWindowTextW(
        launcherPlacementDescription_,
        L"");

    SendMessageW(
        launcherPlacement_,
        CB_RESETCONTENT, 0, 0);
    SendMessageW(
        launcherPlacement_,
        CB_ADDSTRING, 0,
        reinterpret_cast<LPARAM>(
            T(L"靠近屏幕顶部",
              L"Near top of screen")));
    SendMessageW(
        launcherPlacement_,
        CB_ADDSTRING, 0,
        reinterpret_cast<LPARAM>(
            T(L"屏幕居中",
              L"Center on screen")));
    SendMessageW(
        launcherPlacement_,
        CB_ADDSTRING, 0,
        reinterpret_cast<LPARAM>(
            T(L"上次位置",
              L"Last position")));

    SetWindowTextW(
        settingsPlacementLabel_,
        T(L"设置窗口位置",
          L"Settings window position"));
    SetWindowTextW(
        settingsPlacementDescription_,
        L"");

    SendMessageW(
        settingsPlacement_,
        CB_RESETCONTENT, 0, 0);
    SendMessageW(
        settingsPlacement_,
        CB_ADDSTRING, 0,
        reinterpret_cast<LPARAM>(
            T(L"靠近屏幕顶部",
              L"Near top of screen")));
    SendMessageW(
        settingsPlacement_,
        CB_ADDSTRING, 0,
        reinterpret_cast<LPARAM>(
            T(L"屏幕居中",
              L"Center on screen")));
    SendMessageW(
        settingsPlacement_,
        CB_ADDSTRING, 0,
        reinterpret_cast<LPARAM>(
            T(L"上次位置",
              L"Last position")));

    SetWindowTextW(
        shortcutManagerPlacementLabel_,
        T(L"快捷项管理窗口位置",
          L"Shortcut Manager window position"));
    SetWindowTextW(
        shortcutManagerPlacementDescription_,
        L"");

    SendMessageW(
        shortcutManagerPlacement_,
        CB_RESETCONTENT, 0, 0);
    SendMessageW(
        shortcutManagerPlacement_,
        CB_ADDSTRING, 0,
        reinterpret_cast<LPARAM>(
            T(L"靠近屏幕顶部",
              L"Near top of screen")));
    SendMessageW(
        shortcutManagerPlacement_,
        CB_ADDSTRING, 0,
        reinterpret_cast<LPARAM>(
            T(L"屏幕居中",
              L"Center on screen")));
    SendMessageW(
        shortcutManagerPlacement_,
        CB_ADDSTRING, 0,
        reinterpret_cast<LPARAM>(
            T(L"上次位置",
              L"Last position")));

    SetWindowTextW(
        generalNote_,
        L"");
    SetWindowTextW(
        hotkeyGlobalTitle_,
        T(L"全局快捷键",
          L"Global hotkeys"));
    SetWindowTextW(
        hotkeyLauncherTitle_,
        T(L"启动器内快捷键",
          L"Launcher hotkeys"));
    SetWindowTextW(
        hotkeyResetAll_,
        T(L"恢复全部默认快捷键",
          L"Reset all hotkeys"));
    SetWindowTextW(
        providerSectionTitle_,
        T(L"应用来源",
          L"Application sources"));
    SetWindowTextW(
        providerFilesTitle_,
        T(L"文件与文件夹",
          L"Files & folders"));
    SetWindowTextW(
        providerStartMenu_,
        T(L"开始菜单",
          L"Start Menu"));
    SetWindowTextW(
        providerPackaged_,
        L"Windows Apps");
    SetWindowTextW(
        providerAppPaths_,
        L"App Paths");
    SetWindowTextW(
        providerPath_,
        L"PATH");
    SetWindowTextW(
        providerEverything_,
        T(L"Everything 文件与文件夹",
          L"Everything files & folders"));
    SetWindowTextW(
        managedEverythingTrayIcon_,
        T(L"显示 Everything 托盘图标",
          L"Show Everything tray icon"));
    SetWindowTextW(
        providerGetEverything_,
        T(L"获取并启动 Everything",
          L"Get and start Everything"));
    SetWindowTextW(
        providerUpdateEverything_,
        T(L"检查更新",
          L"Check for updates"));
    SetWindowTextW(
        providerRecheckEverything_,
        T(L"重新检测",
          L"Recheck"));
    SetWindowTextW(
        providerNote_,
        L"");
    SetWindowTextW(
        appearanceLauncherTitle_,
        T(L"启动器",
          L"Launcher"));
    SetWindowTextW(
        uiStyleLabel_,
        T(L"启动器样式",
          L"Launcher style"));
    SendMessageW(
        uiStyle_,
        CB_RESETCONTENT, 0, 0);
    SendMessageW(
        uiStyle_,
        CB_ADDSTRING, 0,
        reinterpret_cast<LPARAM>(
            T(L"经典模式（ALTRun 风格）",
              L"Classic (ALTRun-inspired)")));
    SendMessageW(
        uiStyle_,
        CB_ADDSTRING, 0,
        reinterpret_cast<LPARAM>(
            T(L"现代紧凑",
              L"Modern Compact")));

    SetWindowTextW(
        appearanceAppTitle_,
        T(L"应用",
          L"Application"));
    SetWindowTextW(
        languageLabel_,
        T(L"界面语言",
          L"Interface language"));
    SendMessageW(
        language_,
        CB_RESETCONTENT, 0, 0);
    SendMessageW(
        language_,
        CB_ADDSTRING, 0,
        reinterpret_cast<LPARAM>(
            L"简体中文"));
    SendMessageW(
        language_,
        CB_ADDSTRING, 0,
        reinterpret_cast<LPARAM>(
            L"English"));
    SetWindowTextW(
        appearanceNote_,
        L"");
    SetWindowTextW(
        dataPathLabel_,
        T(L"数据目录",
          L"Data directory"));
    SetWindowTextW(
        dataPath_,
        app_.DataDirectory().c_str());
    SetWindowTextW(
        openDataFolder_,
        T(L"打开目录",
          L"Open folder"));

    SetWindowTextW(
        dataTransferLabel_,
        T(L"导入与导出",
          L"Import & export"));
    SetWindowTextW(
        dataImportTsv_,
        T(L"导入快捷项…",
          L"Import shortcuts…"));
    SetWindowTextW(
        dataExport_,
        T(L"导出快捷项…",
          L"Export shortcuts…"));

    SetWindowTextW(
        dataMaintenanceLabel_,
        T(L"维护",
          L"Maintenance"));
    SetWindowTextW(
        dataClearUsage_,
        T(L"清除使用历史",
          L"Clear usage history"));
    SetWindowTextW(
        dataRebuildIndex_,
        T(L"重新建立程序索引",
          L"Rebuild program index"));
    SetWindowTextW(
        dataResetSettings_,
        T(L"恢复默认设置",
          L"Reset settings"));

    SetWindowTextW(
        aboutName_,
        L"Asterun");

    std::wstring version =
        L"v";
    version.append(
        kVersion.begin(),
        kVersion.end());
    SetWindowTextW(
        aboutVersion_,
        version.c_str());

    SetWindowTextW(
        aboutDescription_,
        T(L"Windows 原生、轻量、高响应的键盘启动器",
          L"Native, lightweight, responsive keyboard launcher for Windows"));

    SetWindowTextW(
        updateSectionTitle_,
        T(L"更新",
          L"Updates"));

    SetWindowTextW(
        updateAutoCheck_,
        T(L"自动检查更新",
          L"Automatically check for updates"));
    SetWindowTextW(
        updatePrerelease_,
        T(L"接收预发布版本更新",
          L"Get prerelease updates"));
    SetWindowTextW(
        updateAction_,
        T(L"检查更新",
          L"Check for updates"));
    SetWindowTextW(
        openGitHub_,
        L"GitHub ↗");

    ApplyFonts();
    UpdateNavLabels();
    UpdatePageHeader();
    RefreshFromSettings();
    RefreshDataCompatibilityStatus();

    syncing_ = oldSyncing;

    RedrawWindow(
        hwnd_,
        nullptr,
        nullptr,
        RDW_INVALIDATE |
            RDW_ERASE |
            RDW_ALLCHILDREN |
            RDW_UPDATENOW);
}

void SettingsWindow::RefreshFromSettings() {
    if (!hwnd_) return;

    const bool oldSyncing =
        syncing_;
    syncing_ = true;

    const auto& settings =
        app_.SettingsData();

    int startupBehaviorIndex = 1;
    if (settings.startupBehavior == StartupBehavior::Silent) startupBehaviorIndex = 0;
    else if (settings.startupBehavior == StartupBehavior::ShowLauncher) startupBehaviorIndex = 2;
    SendMessageW(startupBehavior_, CB_SETCURSEL, startupBehaviorIndex, 0);

    int monitorIndex = 0;
    if (settings.popupMonitor == "active") {
        monitorIndex = 1;
    } else if (
        settings.popupMonitor == "primary") {
        monitorIndex = 2;
    }

    SendMessageW(
        popupMonitor_,
        CB_SETCURSEL,
        monitorIndex,
        0);

    int launcherPlacementIndex = 0;
    if (settings.launcherPlacement ==
        "center") {
        launcherPlacementIndex = 1;
    } else if (
        settings.launcherPlacement ==
        "last") {
        launcherPlacementIndex = 2;
    }

    SendMessageW(
        launcherPlacement_,
        CB_SETCURSEL,
        launcherPlacementIndex,
        0);

    int settingsPlacementIndex = 0;
    if (settings.settingsPlacement ==
        "center") {
        settingsPlacementIndex = 1;
    } else if (
        settings.settingsPlacement ==
        "last") {
        settingsPlacementIndex = 2;
    }

    SendMessageW(
        settingsPlacement_,
        CB_SETCURSEL,
        settingsPlacementIndex,
        0);

    int shortcutManagerPlacementIndex = 0;
    if (settings.shortcutManagerPlacement ==
        "center") {
        shortcutManagerPlacementIndex = 1;
    } else if (
        settings.shortcutManagerPlacement ==
        "last") {
        shortcutManagerPlacementIndex = 2;
    }

    SendMessageW(
        shortcutManagerPlacement_,
        CB_SETCURSEL,
        shortcutManagerPlacementIndex,
        0);

    SendMessageW(
        uiStyle_,
        CB_SETCURSEL,
        settings.uiStyle ==
                UiStyle::ModernCompact
            ? 1
            : 0,
        0);

    SendMessageW(
        language_,
        CB_SETCURSEL,
        settings.language ==
                Language::EnUS
            ? 1
            : 0,
        0);

    SendMessageW(
        updateAutoCheck_,
        BM_SETCHECK,
        settings.autoCheckUpdates
            ? BST_CHECKED
            : BST_UNCHECKED,
        0);

    SendMessageW(
        updatePrerelease_,
        BM_SETCHECK,
        settings.updateChannel ==
                UpdateChannel::Development
            ? BST_CHECKED
            : BST_UNCHECKED,
        0);

    RefreshHotkeyPage();
    RefreshUpdateStatus();
    SyncUpdateStatusTimer();

    for (HWND control :
         std::array<HWND, 15>{
             startWithWindows_, showTrayIcon_, soundEnabled_, addToSendToMenu_,
             pinyinSearch_, numericQuickLaunch_,
             executeSingleResult_, providerStartMenu_, providerPackaged_,
             providerAppPaths_, providerPath_, providerEverything_,
             managedEverythingTrayIcon_,
             updateAutoCheck_, updatePrerelease_}) {
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
            L"唤起 Asterun",
            L"Show Asterun");
    }
    if (actionId ==
        hotkey_actions::
            kActivateSecondary) {
        return T(
            L"辅助唤起",
            L"Secondary activation");
    }
    if (actionId == hotkey_actions::kOpenSettings) {
        return T(L"打开设置", L"Open Settings");
    }
    if (actionId == hotkey_actions::kOpenShortcutManager) {
        return T(L"打开快捷项管理", L"Open Shortcut Manager");
    }
    if (actionId == hotkey_actions::kExitApplication) {
        return T(L"退出 Asterun", L"Exit Asterun");
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

SettingsWindow::HotkeyRowControls*
SettingsWindow::FindHotkeyRow(
    std::string_view actionId) {
    const auto it =
        std::find_if(
            hotkeyRows_.begin(),
            hotkeyRows_.end(),
            [&](const HotkeyRowControls& row) {
                return row.actionId ==
                    actionId;
            });

    return it == hotkeyRows_.end()
        ? nullptr
        : &*it;
}

const SettingsWindow::HotkeyRowControls*
SettingsWindow::FindHotkeyRow(
    std::string_view actionId) const {
    const auto it =
        std::find_if(
            hotkeyRows_.begin(),
            hotkeyRows_.end(),
            [&](const HotkeyRowControls& row) {
                return row.actionId ==
                    actionId;
            });

    return it == hotkeyRows_.end()
        ? nullptr
        : &*it;
}

SettingsWindow::HotkeyRowControls*
SettingsWindow::HotkeyRowFromControlId(
    UINT id,
    UINT baseId) {
    if (id < baseId) {
        return nullptr;
    }

    const std::size_t index =
        static_cast<std::size_t>(
            id - baseId);

    if (index >= hotkeyRows_.size()) {
        return nullptr;
    }

    return &hotkeyRows_[index];
}

void SettingsWindow::SetHotkeyRowStatus(
    std::string_view actionId,
    std::wstring_view status) {
    auto* row =
        FindHotkeyRow(actionId);

    if (!row || !row->status) {
        return;
    }

    const bool atomicUpdate =
        hwnd_ &&
        page_ == Page::Hotkeys;

    window_presentation::ScopedRedrawSuspend redrawGuard(
        atomicUpdate ? hwnd_ : nullptr);

    SetWindowTextW(
        row->status,
        std::wstring(status).c_str());

    row->statusVisible =
        !status.empty();

    ShowWindow(
        row->status,
        row->statusVisible
            ? SW_SHOW
            : SW_HIDE);

    if (!atomicUpdate) {
        return;
    }

    Layout();

    redrawGuard.Resume();

    RedrawWindow(
        hwnd_,
        nullptr,
        nullptr,
        RDW_INVALIDATE |
            RDW_ERASE |
            RDW_ALLCHILDREN |
            RDW_FRAME |
            RDW_UPDATENOW);
}


bool SettingsWindow::
HotkeyRowHasAuxiliaryContent(
    const HotkeyRowControls& row) const {

    return
        row.statusVisible;
}

int SettingsWindow::HotkeyAuxiliaryHeight(
    const HotkeyRowControls& row) const {

    if (!HotkeyRowHasAuxiliaryContent(
            row)) {
        return 0;
    }

    wchar_t buffer[512]{};
    GetWindowTextW(
        row.status,
        buffer,
        static_cast<int>(
            std::size(buffer)));

    int textHeight =
        Scale(20);

    if (HDC dc = GetDC(hwnd_)) {
        HGDIOBJ oldFont =
            SelectObject(
                dc,
                normalFont_);

        RECT measured{
            0,
            0,
            Scale(224),
            0,
        };

        DrawTextW(
            dc,
            buffer,
            -1,
            &measured,
            DT_LEFT |
                DT_WORDBREAK |
                DT_CALCRECT |
                DT_NOPREFIX);

        textHeight =
            std::max(
                textHeight,
                static_cast<int>(
                    measured.bottom -
                    measured.top));

        SelectObject(
            dc,
            oldFont);
        ReleaseDC(
            hwnd_,
            dc);
    }

    return std::min(
        Scale(72),
        std::max(
            Scale(30),
            textHeight +
                Scale(10)));
}

int SettingsWindow::HotkeyRowHeight(
    const HotkeyRowControls& row) const {

    return
        Scale(54) +
        HotkeyAuxiliaryHeight(row);
}

int SettingsWindow::HotkeyGroupHeight(
    bool global) const {

    int height = 0;

    for (const auto& row :
         hotkeyRows_) {
        const auto* action =
            FindHotkeyAction(
                row.actionId);

        if (!action) {
            continue;
        }

        const bool rowGlobal =
            action->scope ==
                HotkeyScope::Global;

        if (rowGlobal == global) {
            height +=
                HotkeyRowHeight(row);
        }
    }

    return height;
}

void SettingsWindow::RefreshHotkeyPage(
    bool relayout) {
    if (hotkeyRows_.empty()) {
        return;
    }

    const bool atomicUpdate =
        relayout &&
        hwnd_ &&
        page_ == Page::Hotkeys;

    window_presentation::ScopedRedrawSuspend redrawGuard(
        atomicUpdate ? hwnd_ : nullptr);

    const bool oldSyncing =
        syncing_;
    syncing_ = true;

    const auto bindings =
        app_.SettingsData()
            .hotkeyBindings;

    for (auto& row :
         hotkeyRows_) {
        const auto* action =
            FindHotkeyAction(
                row.actionId);

        if (!action) {
            continue;
        }

        const auto binding =
            EffectiveHotkeyBinding(
                bindings,
                row.actionId);

        SetWindowTextW(
            row.title,
            HotkeyActionLabel(
                row.actionId).c_str());

        const bool capturing =
            capturingHotkeyActionId_ ==
                row.actionId;

        SetWindowTextW(
            row.capture,
            capturing
                ? T(L"请按新的快捷键…",
                    L"Press a new shortcut…")
                : FormatHotkeyBinding(
                      row.actionId).c_str());

        if (row.enabled) {
            InvalidateRect(
                row.enabled,
                nullptr,
                TRUE);
        }

        auto current =
            binding;
        auto defaults =
            action->defaultBinding;

        CanonicalizeHotkeyBinding(
            current);
        CanonicalizeHotkeyBinding(
            defaults);

        const bool modified =
            current.enabled !=
                defaults.enabled ||
            current.key !=
                defaults.key ||
            current.modifiers !=
                defaults.modifiers;

        SetWindowTextW(
            row.reset,
            T(L"恢复默认",
              L"Reset"));

        std::wstring status;

        if (capturing) {
            status =
                T(L"按下新组合键，Esc 取消",
                  L"Press a new shortcut; Esc cancels");
        } else if (
            binding.enabled &&
            action->scope ==
                HotkeyScope::Global &&
            !app_.IsHotkeyActionRegistered(
                row.actionId)) {
            status =
                T(L"Windows 注册失败，旧绑定仍有效。错误码：",
                  L"Windows registration failed; the previous binding remains active. Error: ");
            status +=
                std::to_wstring(
                    app_.HotkeyActionLastError(
                        row.actionId));
        }

        const bool showStatus =
            page_ == Page::Hotkeys &&
            !status.empty();

        // Per-item Reset is an inline main-row action. Only status/capture
        // copy expands the row below the shortcut control.
        const bool showReset =
            page_ == Page::Hotkeys &&
            modified;

        SetWindowTextW(
            row.status,
            status.c_str());

        row.statusVisible =
            showStatus;
        row.resetVisible =
            showReset;

        ShowWindow(
            row.status,
            row.statusVisible
                ? SW_SHOW
                : SW_HIDE);

        ShowWindow(
            row.reset,
            row.resetVisible
                ? SW_SHOW
                : SW_HIDE);
    }

    syncing_ = oldSyncing;

    if (!atomicUpdate) {
        return;
    }

    Layout();

    redrawGuard.Resume();

    RedrawWindow(
        hwnd_,
        nullptr,
        nullptr,
        RDW_INVALIDATE |
            RDW_ERASE |
            RDW_ALLCHILDREN |
            RDW_FRAME |
            RDW_UPDATENOW);
}

void SettingsWindow::BeginHotkeyCapture(
    std::string_view actionId) {
    if (!FindHotkeyAction(actionId)) {
        return;
    }

    if (capturingHotkeyActionId_ ==
        actionId) {
        CancelHotkeyCapture();
        return;
    }

    capturingHotkeyActionId_ =
        std::string(actionId);

    RefreshHotkeyPage();
    SetFocus(hwnd_);
}

void SettingsWindow::CancelHotkeyCapture(
    bool refresh) {
    if (capturingHotkeyActionId_
            .empty()) {
        return;
    }

    capturingHotkeyActionId_
        .clear();

    if (refresh) {
        RefreshHotkeyPage();
    }
}

void SettingsWindow::ApplyCapturedHotkey(
    UINT virtualKey) {
    if (capturingHotkeyActionId_
            .empty()) {
        return;
    }

    const std::string actionId =
        capturingHotkeyActionId_;

    if (virtualKey == VK_ESCAPE) {
        CancelHotkeyCapture();
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
        SetHotkeyRowStatus(
            actionId,
            T(L"这个按键目前不受支持，请换一个组合键。",
              L"This key is not currently supported; choose another combination."));
        return;
    }

    HotkeyBinding candidate =
        EffectiveHotkeyBinding(
            app_.SettingsData()
                .hotkeyBindings,
            actionId);

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
            actionId,
            candidate)) {
        SetHotkeyRowStatus(
            actionId,
            T(L"该组合键无效或会抢占搜索输入；无修饰键时请使用 F1–F24 或 Pause。",
              L"This binding is invalid or would steal query input; use F1-F24 or Pause when no modifier is present."));
        return;
    }

    if (const auto conflict =
            FindHotkeyConflict(
                app_.SettingsData()
                    .hotkeyBindings,
                actionId,
                candidate)) {
        std::wstring message =
            T(L"与“", L"Conflicts with “");
        message +=
            HotkeyActionLabel(
                *conflict);
        message +=
            T(L"”冲突，请换一个组合键。",
              L"”; choose another binding.");

        SetHotkeyRowStatus(
            actionId,
            message);
        return;
    }

    if (!app_.SetHotkeyBinding(
            actionId,
            candidate)) {
        SetHotkeyRowStatus(
            actionId,
            T(L"无法应用；全局快捷键可能已被其他程序占用，旧绑定保持不变。",
              L"Could not apply; a global shortcut may already be owned by another app. The previous binding remains active."));
        return;
    }

    CancelHotkeyCapture();
}

void SettingsWindow::ToggleHotkeyActionEnabled(
    std::string_view actionId) {
    if (syncing_) {
        return;
    }

    const auto* action =
        FindHotkeyAction(actionId);

    if (!action ||
        action->required) {
        return;
    }

    auto binding =
        EffectiveHotkeyBinding(
            app_.SettingsData()
                .hotkeyBindings,
            actionId);

    binding.enabled =
        !binding.enabled;

    if (binding.enabled) {
        if (const auto conflict =
                FindHotkeyConflict(
                    app_.SettingsData()
                        .hotkeyBindings,
                    actionId,
                    binding)) {
            std::wstring message =
                T(L"无法启用：与“",
                  L"Cannot enable: conflicts with “");
            message +=
                HotkeyActionLabel(
                    *conflict);
            message += L"”.";

            SetHotkeyRowStatus(
                actionId,
                message);
            return;
        }
    }

    if (!app_.SetHotkeyBinding(
            std::string(actionId),
            binding)) {
        SetHotkeyRowStatus(
            actionId,
            T(L"无法更新此快捷键状态。",
              L"Could not update this hotkey state."));
        return;
    }

    RefreshHotkeyPage();
}

void SettingsWindow::ResetHotkeyAction(
    std::string_view actionId) {
    const auto* action =
        FindHotkeyAction(actionId);

    if (!action) {
        return;
    }

    if (const auto conflict =
            FindHotkeyConflict(
                app_.SettingsData()
                    .hotkeyBindings,
                actionId,
                action->defaultBinding)) {
        std::wstring message =
            T(L"默认组合键当前与“",
              L"The default binding currently conflicts with “");
        message +=
            HotkeyActionLabel(
                *conflict);
        message += L"”.";

        SetHotkeyRowStatus(
            actionId,
            message);
        return;
    }

    if (!app_.SetHotkeyBinding(
            std::string(actionId),
            action->defaultBinding)) {
        SetHotkeyRowStatus(
            actionId,
            T(L"恢复失败；默认全局快捷键可能已被其他程序占用。",
              L"Reset failed; another app may own the default global shortcut."));
        return;
    }

    if (capturingHotkeyActionId_ ==
        actionId) {
        CancelHotkeyCapture(false);
    }

    RefreshHotkeyPage();
}

void SettingsWindow::ResetAllHotkeys() {
    const int answer =
        altrun::ui::ShowMessage(
            hwnd_,
            T(L"恢复全部默认快捷键？\n\n主热键将恢复为 Alt + Space，辅助热键关闭，启动器内动作恢复默认组合。",
              L"Reset every hotkey to defaults?\n\nPrimary activation returns to Alt + Space, secondary activation is disabled and launcher actions regain their defaults."),
            T(L"恢复默认快捷键",
              L"Reset hotkeys"),
            MB_YESNO |
                MB_ICONQUESTION);

    if (answer != IDYES) {
        return;
    }

    if (!app_.ResetHotkeyBindings()) {
        altrun::ui::ShowMessage(
            hwnd_,
            T(L"恢复失败。默认全局热键可能已被其他程序占用，原有可用绑定已恢复。",
              L"Reset failed. Another app may own a default global shortcut; the previous working bindings were restored."),
            T(L"恢复默认快捷键",
              L"Reset hotkeys"),
            MB_OK |
                MB_ICONWARNING);
        return;
    }

    CancelHotkeyCapture(false);
    RefreshHotkeyPage();
}

void SettingsWindow::RefreshProviderStatus() {
    if (!providerStatus_) {
        return;
    }

    const bool enabled =
        providers::IsEnabled(
            app_.SettingsData()
                .providerEnabled,
            providers::
                kEverythingFilesystem,
            false);

    const auto ipc =
        app_.EverythingStatus();
    const auto bootstrap =
        app_.EverythingBootstrapStatus();

    bool showGetEverything = false;
    bool showUpdateEverything = false;
    bool showRecheck = false;
    std::wstring text;

    // Treat the tray preference as a runtime control, not as an
    // installation-path preference. ManagedEverythingExecutable() always
    // returns a candidate path (including the pinned fallback path), so path
    // non-emptiness cannot prove that Everything was installed or started.
    // Ownership of the live default IPC window is the authoritative signal.
    const bool managedActive =
        enabled &&
        win::IsManagedEverythingRunning(
            app_.DataDirectory());

    const bool externalActive =
        ipc.availability ==
            EverythingAvailability::
                Available &&
        !managedActive;

    if (!enabled) {
        text =
            T(L"○ Everything 已禁用",
              L"○ Everything is disabled");
    } else if (bootstrap.running) {
        text =
            T(L"◌ 正在准备 Everything",
              L"◌ Preparing Everything");

        text += L" · ";

        switch (bootstrap.stage) {
        case win::EverythingBootstrapStage::Discovering:
            text +=
                T(L"检测本机版本",
                  L"Detecting local copies");
            break;
        case win::EverythingBootstrapStage::StartingExisting:
            text +=
                T(L"启动已有版本",
                  L"Starting existing copy");
            break;
        case win::EverythingBootstrapStage::ResolvingStableVersion:
            text +=
                T(L"检查官方稳定版",
                  L"Checking latest stable release");
            break;
        case win::EverythingBootstrapStage::DownloadingManifest:
            text +=
                T(L"准备安全下载",
                  L"Preparing secure download");
            break;
        case win::EverythingBootstrapStage::DownloadingPackage:
            text +=
                T(L"下载 Everything",
                  L"Downloading Everything");

            if (bootstrap.downloadedBytes > 0) {
                text += L" ";
                text += FormatBytes(
                    bootstrap.downloadedBytes);

                if (bootstrap.totalBytes > 0) {
                    text += L" / ";
                    text += FormatBytes(
                        bootstrap.totalBytes);
                }
            }
            break;
        case win::EverythingBootstrapStage::VerifyingPackage:
            text +=
                T(L"验证下载文件",
                  L"Verifying download");
            break;
        case win::EverythingBootstrapStage::ExtractingPackage:
            text +=
                T(L"准备文件",
                  L"Preparing files");
            break;
        case win::EverythingBootstrapStage::StoppingManaged:
            text +=
                T(L"切换版本",
                  L"Switching versions");
            break;
        case win::EverythingBootstrapStage::InstallingService:
        case win::EverythingBootstrapStage::RepairingService:
            text +=
                T(L"启用文件索引，请确认 Windows 提示",
                  L"Enabling file indexing; confirm the Windows prompt");
            break;
        case win::EverythingBootstrapStage::WaitingForService:
            text +=
                T(L"启动文件索引",
                  L"Starting file index");
            break;
        case win::EverythingBootstrapStage::StartingManaged:
        case win::EverythingBootstrapStage::WaitingForIpc:
            text +=
                T(L"连接 Everything",
                  L"Connecting to Everything");
            break;
        default:
            text +=
                T(L"应用设置",
                  L"Applying settings");
            break;
        }
    } else if (
        ipc.availability ==
        EverythingAvailability::
            Available) {
        text =
            T(L"● Everything 正在运行",
              L"● Everything is running");
        showRecheck = true;

        if (managedActive) {
            showUpdateEverything = true;

            text +=
                T(L" · Asterun 托管",
                  L" · Managed by Asterun");

            if (!bootstrap.installedVersion.empty()) {
                text += L" · v";
                text +=
                    bootstrap.installedVersion;
            }

            if (bootstrap.updateAvailable &&
                !bootstrap.availableVersion.empty()) {
                text +=
                    T(L" · 可更新到 v",
                      L" · update available: v");
                text +=
                    bootstrap.availableVersion;
            } else if (
                bootstrap.usedPinnedVersionFallback) {
                text +=
                    T(L" · 当前无法检查最新版本",
                      L" · latest version could not be checked");
            } else if (
                !bootstrap.availableVersion.empty() &&
                bootstrap.stage ==
                    win::EverythingBootstrapStage::
                        Ready) {
                text +=
                    T(L" · 已是最新稳定版",
                      L" · latest stable");
            } else if (
                bootstrap.stage ==
                    win::EverythingBootstrapStage::
                        Failed) {
                text +=
                    T(L" · 更新检查失败",
                      L" · update check failed");
            }
        } else {
            text +=
                T(L" · 外部安装",
                  L" · External installation");
            text +=
                T(L" · 托盘图标由 Everything 控制",
                  L" · tray icon is controlled by Everything");
        }
    } else {
        showGetEverything = true;
        showRecheck = true;

        if (bootstrap.failure ==
                win::EverythingBootstrapFailure::
                    ServiceRepairRequired) {
            text =
                T(L"⚠ 需要管理员权限修复文件索引组件",
                  L"⚠ Administrator approval is needed to repair file indexing");
        } else if (
            bootstrap.failure ==
                win::EverythingBootstrapFailure::
                    ServiceRequired) {
            text =
                T(L"⚠ 需要管理员权限启用文件索引",
                  L"⚠ Administrator approval is needed to enable file indexing");
        } else if (
            bootstrap.stage ==
                win::EverythingBootstrapStage::
                    Failed) {
            text =
                T(L"⚠ Everything 准备失败",
                  L"⚠ Everything setup failed");

            if (bootstrap.nativeError != 0) {
                text +=
                    T(L" · 错误 ",
                      L" · error ");
                text +=
                    std::to_wstring(
                        bootstrap.nativeError);
            }
        } else if (
            ipc.ambiguousNamedInstances) {
            text =
                T(L"⚠ 检测到多个 Everything 实例",
                  L"⚠ Multiple Everything instances detected");
        } else {
            text =
                T(L"○ 未检测到可用的 Everything",
                  L"○ No usable Everything detected");
        }
    }

    const bool visible =
        page_ == Page::Providers;
    // Only expose this after the Asterun-managed Everything process has
    // successfully started and owns the default IPC endpoint. Merely having
    // an install candidate/path is intentionally insufficient.
    const bool showTray =
        enabled &&
        managedActive;
    const bool showActions = showGetEverything || showUpdateEverything || showRecheck;
    const bool layoutChanged =
        providerTrayVisible_ != showTray ||
        providerActionsVisible_ != showActions;
    const bool atomicProviderUpdate =
        visible &&
        layoutChanged &&
        hwnd_ &&
        IsWindowVisible(hwnd_);

    RECT oldFilesCard{};
    if (atomicProviderUpdate) {
        oldFilesCard =
            PageCardRect(
                394,
                ProviderFilesHeightLogical(),
                720);
    }

    window_presentation::ScopedRedrawSuspend
        redrawGuard(
            atomicProviderUpdate
                ? hwnd_
                : nullptr);

    providerTrayVisible_ = showTray;
    providerActionsVisible_ = showActions;

    if (managedEverythingTrayIcon_) {
        ShowWindow(managedEverythingTrayIcon_, visible && showTray ? SW_SHOW : SW_HIDE);
        EnableWindow(
            managedEverythingTrayIcon_,
            enabled &&
                !bootstrap.running &&
                !externalActive);
        InvalidateRect(
            managedEverythingTrayIcon_,
            nullptr,
            TRUE);
    }

    if (providerUpdateEverything_) {
        SetWindowTextW(
            providerUpdateEverything_,
            bootstrap.updateAvailable
                ? T(L"更新 Everything",
                    L"Update Everything")
                : T(L"检查更新",
                    L"Check for updates"));
    }

    ShowWindow(
        providerGetEverything_,
        visible &&
                showGetEverything
            ? SW_SHOW
            : SW_HIDE);

    ShowWindow(
        providerUpdateEverything_,
        visible &&
                showUpdateEverything
            ? SW_SHOW
            : SW_HIDE);

    ShowWindow(
        providerRecheckEverything_,
        visible &&
                showRecheck
            ? SW_SHOW
            : SW_HIDE);

    SetWindowTextW(
        providerStatus_,
        text.c_str());

    if (visible && layoutChanged) {
        Layout();
    }

    if (atomicProviderUpdate) {
        RECT newFilesCard =
            PageCardRect(
                394,
                ProviderFilesHeightLogical(),
                720);
        RECT providerDirtyRect{};
        UnionRect(
            &providerDirtyRect,
            &oldFilesCard,
            &newFilesCard);
        InflateRect(
            &providerDirtyRect,
            Scale(2),
            Scale(2));

        redrawGuard.Resume();

        // Repaint only the Everything card. Repainting the entire Settings
        // window made unrelated provider labels visibly blink even though
        // their content and geometry never changed.
        RedrawWindow(
            hwnd_,
            &providerDirtyRect,
            nullptr,
            RDW_INVALIDATE |
                RDW_ALLCHILDREN |
                RDW_UPDATENOW);
    } else if (
        visible &&
        layoutChanged) {
        const RECT filesCard =
            PageCardRect(
                394,
                ProviderFilesHeightLogical(),
                720);
        InvalidateRect(
            hwnd_,
            &filesCard,
            FALSE);
    }
}

void SettingsWindow::AcquireEverything() {
    if (app_.EverythingBootstrapStatus()
            .running) {
        return;
    }

    if (!altrun::ui::
             ConfirmEverythingSetup(
                 hwnd_,
                 app_.SettingsData()
                         .language ==
                     Language::ZhCN)) {
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

void SettingsWindow::
CheckOrUpdateEverything() {
    const auto bootstrap =
        app_.EverythingBootstrapStatus();

    if (bootstrap.running) {
        return;
    }

    if (bootstrap.updateAvailable) {
        app_.StartEverythingBootstrap(
            true,
            true);
    } else {
        app_.StartEverythingUpdateCheck();
    }

    RefreshProviderStatus();
}

void SettingsWindow::
ToggleManagedEverythingTrayIcon() {
    if (syncing_) {
        return;
    }

    const auto ipc =
        app_.EverythingStatus();
    const auto bootstrap =
        app_.EverythingBootstrapStatus();

    const bool externalActive =
        ipc.availability ==
            EverythingAvailability::
                Available &&
        bootstrap.source !=
            win::EverythingBootstrapSource::
                Managed &&
        bootstrap.source !=
            win::EverythingBootstrapSource::
                Downloaded;

    if (externalActive) {
        return;
    }

    const auto settings =
        app_.SettingsData();

    if (!app_
             .SetManagedEverythingShowTrayIcon(
                 !settings
                      .managedEverythingShowTrayIcon,
                 false)) {
        altrun::ui::ShowMessage(
            hwnd_,
            T(L"无法保存 Everything 托盘图标设置。",
              L"Could not save the Everything tray icon setting."),
            L"Asterun",
            MB_OK |
                MB_ICONERROR);
    }

    if (managedEverythingTrayIcon_) {
        InvalidateRect(
            managedEverythingTrayIcon_,
            nullptr,
            FALSE);
    }
    RefreshProviderStatus();
}

void SettingsWindow::RefreshDataCompatibilityStatus() {
    if (!dataStatus_) {
        return;
    }

    const std::wstring warning =
        app_.DataCompatibilityWarning();

    SetWindowTextW(
        dataStatus_,
        warning.c_str());
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
        navProviders_,
        label(Page::Providers, L"搜索来源", L"Search sources").c_str());
    SetWindowTextW(
        navAppearance_,
        label(Page::Appearance, L"外观", L"Appearance").c_str());
    SetWindowTextW(
        navData_,
        label(Page::Data, L"数据", L"Data").c_str());
    SetWindowTextW(
        navAbout_,
        label(Page::About, L"关于", L"About").c_str());
}


void SettingsWindow::UpdatePageHeader() {
    const wchar_t* title = L"";

    switch (page_) {
    case Page::General:
        title = T(L"常规", L"General");
        break;
    case Page::Hotkeys:
        title = T(L"快捷键", L"Hotkeys");
        break;
    case Page::Appearance:
        title = T(L"外观", L"Appearance");
        break;
    case Page::Providers:
        title = T(L"搜索来源", L"Search sources");
        break;
    case Page::Data:
        title = T(L"数据", L"Data");
        break;
    case Page::About:
        title = T(L"关于", L"About");
        break;
    }

    SetWindowTextW(
        pageTitle_,
        title);

    SetWindowTextW(
        pageDescription_,
        L"");
    ShowWindow(
        pageDescription_,
        SW_HIDE);
}


void SettingsWindow::ShowPage(Page page) {
    // Clicking the already selected navigation item must be a true no-op.
    // Rebuilding the same page hides/shows every child, forces a frame change,
    // and was visible on real desktops as a flash in the content pane.
    // Keep the hidden-window path active so Show()/first creation can still
    // refresh the page before its first visible frame.
    if (page == page_ &&
        hwnd_ &&
        IsWindowVisible(hwnd_)) {
        return;
    }

    if (page_ == Page::Hotkeys &&
        page != Page::Hotkeys) {
        CancelHotkeyCapture(false);
    }

    if (page_ == Page::Providers &&
        page != Page::Providers) {
        KillTimer(
            hwnd_,
            kProviderStatusTimerId);
    }

    if (page_ == Page::About &&
        page != Page::About) {
        KillTimer(
            hwnd_,
            kUpdateStatusTimerId);
    }

    if (page != page_) {
        if (page == Page::General) {
            generalScrollOffset_ = 0;
        } else if (
            page == Page::Hotkeys) {
            hotkeyScrollOffset_ = 0;
        }
    }

    window_presentation::ScopedRedrawSuspend redrawGuard(hwnd_);

    page_ = page;

    const auto setVisible =
        [](const std::vector<HWND>& controls,
           bool visible) {
            for (HWND control : controls) {
                ShowWindow(
                    control,
                    visible
                        ? SW_SHOW
                        : SW_HIDE);
            }
        };

    setVisible(
        generalControls_,
        page == Page::General);
    setVisible(
        hotkeyControls_,
        page == Page::Hotkeys);
    setVisible(
        appearanceControls_,
        page == Page::Appearance);
    setVisible(
        providerControls_,
        page == Page::Providers);
    setVisible(
        dataControls_,
        page == Page::Data);
    setVisible(
        aboutControls_,
        page == Page::About);

    for (HWND control :
         std::array<HWND, 7>{
             pageDescription_,
             popupMonitorDescription_,
             launcherPlacementDescription_,
             settingsPlacementDescription_,
             generalNote_,
             providerNote_,
             appearanceNote_}) {
        if (control) {
            ShowWindow(
                control,
                SW_HIDE);
        }
    }

    if (page == Page::Hotkeys) {
        RefreshHotkeyPage(false);
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
    } else if (
        page == Page::About) {
        RefreshUpdateStatus();
        SyncUpdateStatusTimer();
    }

    UpdateNavLabels();
    UpdatePageHeader();
    Layout();

    redrawGuard.Resume();

    UpdatePageScrollBar();

    // Let USER32 coalesce the final parent/child repaint instead of erasing
    // and synchronously repainting the whole window on every navigation.
    // Only the content pane and navigation buttons changed.
    RECT client{};
    GetClientRect(
        hwnd_,
        &client);

    RECT content{
        Scale(
            kSidebarWidthLogical),
        client.top,
        client.right,
        client.bottom,
    };

    RedrawWindow(
        hwnd_,
        &content,
        nullptr,
        RDW_INVALIDATE |
            RDW_NOERASE |
            RDW_ALLCHILDREN);

    for (HWND navigation :
         std::array<HWND, 6>{
             navGeneral_,
             navHotkeys_,
             navProviders_,
             navAppearance_,
             navData_,
             navAbout_}) {
        if (navigation) {
            InvalidateRect(
                navigation,
                nullptr,
                FALSE);
        }
    }
}

void SettingsWindow::ApplyClassicBehaviorControl(UINT id) {
    if (syncing_) return;
    bool pinyinSearch = app_.SettingsData().pinyinSearch;
    bool numericQuickLaunch = app_.SettingsData().numericQuickLaunch;
    bool executeSingleResult = app_.SettingsData().executeSingleResultImmediately;

    switch (id) {
    case kIdPinyinSearch: pinyinSearch = !pinyinSearch; break;
    case kIdNumericQuickLaunch: numericQuickLaunch = !numericQuickLaunch; break;
    case kIdExecuteSingleResult: executeSingleResult = !executeSingleResult; break;
    case 0: break;
    default: return;
    }

    if (!app_.SetClassicBehavior(numericQuickLaunch, executeSingleResult, pinyinSearch)) {
        altrun::ui::ShowMessage(hwnd_,
            T(L"无法保存搜索与执行设置。", L"Unable to save search and execution settings."),
            L"Asterun", MB_OK | MB_ICONERROR);
        RefreshFromSettings();
    }
}


void SettingsWindow::ImportCommands() {
    std::array<wchar_t, 32768> file{};

    const wchar_t filter[] =
        L"Asterun shortcuts\0*.tsv;*.txt\0"
        L"All files\0*.*\0\0";

    OPENFILENAMEW open{};
    open.lStructSize = sizeof(open);
    open.hwndOwner = hwnd_;
    open.lpstrFile = file.data();
    open.nMaxFile =
        static_cast<DWORD>(
            file.size());
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

    std::size_t imported = 0;
    std::size_t skipped = 0;

    if (!app_.ImportUserCommands(
            std::filesystem::path(
                file.data()),
            &imported,
            &skipped)) {

        altrun::ui::ShowMessage(
            hwnd_,
            T(L"导入失败，原数据未被替换。",
              L"Import failed. Existing data was not replaced."),
            T(L"导入快捷项",
              L"Import shortcuts"),
            MB_OK |
                MB_ICONERROR);
        return;
    }

    std::wstring status =
        T(L"导入完成：新增 ",
          L"Import complete: added ");
    status +=
        std::to_wstring(imported);
    status +=
        T(L" 项，跳过 ",
          L", skipped ");
    status +=
        std::to_wstring(skipped);
    status +=
        T(L" 项。",
          L".");

    SetWindowTextW(
        dataStatus_,
        status.c_str());
}

void SettingsWindow::ExportCommands() {
    std::array<wchar_t, 32768> file{};
    const std::wstring defaultName =
        L"Asterun-commands.tsv";

    std::copy(
        defaultName.begin(),
        defaultName.end(),
        file.begin());

    const wchar_t filter[] =
        L"Asterun TSV\0*.tsv\0"
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

        altrun::ui::ShowMessage(
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
        altrun::ui::ShowMessage(
            hwnd_,
            T(L"确定清空全部使用次数和最近使用时间吗？\n\n快捷项本身不会被删除。",
              L"Clear all launch counts and recent-use timestamps?\n\nShortcuts themselves will not be deleted."),
            T(L"清空使用历史", L"Clear usage history"),
            MB_YESNO | MB_ICONWARNING);

    if (answer != IDYES) return;

    if (!app_.ClearUsageHistory()) {
        altrun::ui::ShowMessage(
            hwnd_,
            T(L"清空使用历史失败。",
              L"Failed to clear usage history."),
            L"Asterun",
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
        altrun::ui::ShowMessage(
            hwnd_,
            T(L"确定恢复默认设置吗？\n\n不会删除你的快捷项和使用历史。",
              L"Restore default settings?\n\nYour shortcuts and usage history will not be deleted."),
            T(L"恢复默认设置", L"Restore default settings"),
            MB_YESNO | MB_ICONWARNING);

    if (answer != IDYES) return;

    if (!app_.RestoreDefaultSettings()) {
        altrun::ui::ShowMessage(
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
    bool success = true;
    switch (id) {
    case kIdStartWithWindows: success = app_.SetStartWithWindows(!settings.startWithWindows); break;
    case kIdSoundEnabled: success = app_.SetSoundEnabled(!settings.soundEnabled); break;
    case kIdShowTrayIcon: success = app_.SetShowTrayIcon(!settings.showTrayIcon); break;
    case kIdAddToSendToMenu: success = app_.SetAddToSendToMenu(!settings.addToSendToMenu); break;
    default: return;
    }
    if (!success) {
        altrun::ui::ShowMessage(hwnd_, T(L"无法保存此设置。", L"Unable to save this setting."),
            L"Asterun", MB_OK | MB_ICONERROR);
        RefreshFromSettings();
    }
}


void SettingsWindow::ToggleProviderSetting(
    UINT id) {

    if (syncing_ ||
        providerCommitInProgress_) {
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

    // Keep the visual state local and immediate. The expensive provider
    // cache/lifecycle work is committed once after a short quiet period,
    // so repeated clicks collapse to the user's final intent instead of
    // queueing synchronous refreshes on the UI thread.
    pendingProviderStates_[
        providerId] =
        !ToggleChecked(id);

    InvalidateRect(
        reinterpret_cast<HWND>(
            GetDlgItem(
                hwnd_,
                static_cast<int>(id))),
        nullptr,
        TRUE);

    KillTimer(
        hwnd_,
        kProviderCommitTimerId);

    SetTimer(
        hwnd_,
        kProviderCommitTimerId,
        180,
        nullptr);
}


void SettingsWindow::CommitPendingProviderChanges() {
    KillTimer(
        hwnd_,
        kProviderCommitTimerId);

    if (pendingProviderStates_.empty() ||
        providerCommitInProgress_) {
        return;
    }

    const auto pending =
        pendingProviderStates_;

    providerCommitInProgress_ = true;

    ProviderEnableMap ordinaryChanges;
    std::optional<bool>
        everythingChange;

    for (const auto& [providerId, enabled] :
         pending) {
        const bool everything =
            providerId ==
            providers::
                kEverythingFilesystem;

        const bool current =
            providers::IsEnabled(
                app_.SettingsData()
                    .providerEnabled,
                providerId,
                !everything);

        if (current == enabled) {
            continue;
        }

        if (everything) {
            everythingChange =
                enabled;
        } else {
            ordinaryChanges[
                providerId] =
                enabled;
        }
    }

    bool failed = false;
    bool everythingFailed = false;
    ProviderChangeDiagnostic
        everythingDiagnostic;

    // Apply all ordinary discovery providers in one Settings save / cache
    // merge / Launcher refresh. This is the expensive work that used to run
    // once per click.
    if (!ordinaryChanges.empty() &&
        !app_.SetProviderEnabledBatch(
            ordinaryChanges,
            false)) {
        failed = true;
    }

    // Everything remains a separate lifecycle operation because it may own
    // a managed Service and require UAC. Run it after the cheap batch so a
    // permission prompt cannot delay the ordinary-provider final state.
    if (everythingChange &&
        !app_.SetProviderEnabled(
            std::string(
                providers::
                    kEverythingFilesystem),
            *everythingChange,
            false,
            &everythingDiagnostic)) {
        failed = true;
        everythingFailed = true;
    }

    pendingProviderStates_.clear();
    providerCommitInProgress_ = false;

    // The clicked switches have already painted the pending state. Avoid the
    // broad RefreshFromSettings() invalidation here; it repaints unchanged
    // rows such as Start Menu / Windows Apps / App Paths / PATH and was the
    // remaining source of the text blink reported on real hardware.
    if (failed) {
        for (HWND control :
             std::array<HWND, 5>{
                 providerStartMenu_,
                 providerPackaged_,
                 providerAppPaths_,
                 providerPath_,
                 providerEverything_}) {
            if (control) {
                InvalidateRect(
                    control,
                    nullptr,
                    FALSE);
            }
        }
    }

    RefreshProviderStatus();

    if (failed) {
        std::wstring message;

        if (everythingFailed &&
            everythingDiagnostic.failure ==
                ProviderChangeFailure::
                    EverythingServicePolicy) {
            message =
                T(L"无法读取或应用 Everything 服务状态。",
                  L"Unable to query or apply the Everything service state.");

            if (everythingDiagnostic.nativeError != 0) {
                message +=
                    T(L"\n\nWindows 错误代码：",
                      L"\n\nWindows error code: ");
                message +=
                    std::to_wstring(
                        everythingDiagnostic.nativeError);
            }

            message +=
                T(L"\n\nEverything 开关已恢复原状态。",
                  L"\n\nThe Everything switch was restored to its previous state.");
        } else if (
            everythingFailed &&
            everythingDiagnostic.failure ==
                ProviderChangeFailure::
                    SettingsPersistence) {
            message =
                T(L"无法保存 Asterun 的 settings.json，因此 Everything 开关没有生效。"
                  L"\n\n这不是 Everything 服务安装状态错误；请检查当前数据目录的配置写入或只读保护状态。",
                  L"Asterun could not save settings.json, so the Everything switch was not applied."
                  L"\n\nThis is not an Everything-service installation error; check configuration write access or read-only recovery protection for the current data directory.");
        } else {
            message =
                everythingFailed
                    ? T(L"Everything 搜索来源未能应用，开关已恢复实际状态。",
                        L"The Everything search source could not be applied. The switch was restored to its actual state.")
                    : T(L"部分搜索来源设置无法保存，未成功的开关已恢复实际状态。",
                        L"Some search-source settings could not be saved. Failed switches were restored to their actual state.");
        }

        altrun::ui::ShowMessage(
            hwnd_,
            message.c_str(),
            L"Asterun",
            MB_OK |
                MB_ICONERROR);
    }
}

void SettingsWindow::ApplyStartupBehaviorControl() {
    if (syncing_) return;
    const int index = static_cast<int>(SendMessageW(startupBehavior_, CB_GETCURSEL, 0, 0));
    StartupBehavior behavior = StartupBehavior::Notification;
    if (index == 0) behavior = StartupBehavior::Silent;
    else if (index == 2) behavior = StartupBehavior::ShowLauncher;
    if (!app_.SetStartupBehavior(behavior)) {
        altrun::ui::ShowMessage(hwnd_, T(L"无法保存启动行为设置。", L"Unable to save startup behavior."),
            L"Asterun", MB_OK | MB_ICONERROR);
        RefreshFromSettings();
    }
}


void SettingsWindow::ApplyMonitorControl() {
    if (syncing_) return;
    const int monitorIndex = static_cast<int>(SendMessageW(popupMonitor_, CB_GETCURSEL, 0, 0));
    std::string popupMonitor = "cursor";
    if (monitorIndex == 1) popupMonitor = "active";
    else if (monitorIndex == 2) popupMonitor = "primary";
    if (!app_.SetPopupMonitor(std::move(popupMonitor))) {
        altrun::ui::ShowMessage(hwnd_, T(L"无法保存启动器显示器设置。", L"Unable to save the launcher monitor setting."),
            L"Asterun", MB_OK | MB_ICONERROR);
        RefreshFromSettings();
    }
}


void SettingsWindow::ApplyWindowPlacementControls() {
    if (syncing_) return;

    const int launcherIndex =
        static_cast<int>(
            SendMessageW(
                launcherPlacement_,
                CB_GETCURSEL,
                0,
                0));

    const int settingsIndex =
        static_cast<int>(
            SendMessageW(
                settingsPlacement_,
                CB_GETCURSEL,
                0,
                0));

    const int shortcutManagerIndex =
        static_cast<int>(
            SendMessageW(
                shortcutManagerPlacement_,
                CB_GETCURSEL,
                0,
                0));

    const auto placementMode =
        [](int index) {
            if (index == 1) {
                return std::string("center");
            }
            if (index == 2) {
                return std::string("last");
            }
            return std::string("top");
        };

    if (!app_.SetWindowPlacementSettings(
            placementMode(
                launcherIndex),
            placementMode(
                settingsIndex),
            placementMode(
                shortcutManagerIndex))) {
        altrun::ui::ShowMessage(
            hwnd_,
            T(L"无法保存窗口位置设置。",
              L"Unable to save window placement settings."),
            L"Asterun",
            MB_OK | MB_ICONERROR);
        RefreshFromSettings();
    }
}

void SettingsWindow::ApplyAppearanceControls() {
    if (syncing_) return;

    const int styleIndex =
        static_cast<int>(
            SendMessageW(
                uiStyle_,
                CB_GETCURSEL,
                0,
                0));

    const int languageIndex =
        static_cast<int>(
            SendMessageW(
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

    if (style !=
        app_.SettingsData().uiStyle) {
        app_.SetUiStyle(style);
    }

    if (language !=
        app_.SettingsData().language) {
        app_.SetLanguage(language);
    }
}

bool SettingsWindow::ToggleChecked(
    UINT id) const {

    const auto& settings =
        app_.SettingsData();

    const auto providerEnabled =
        [&](std::string_view providerId,
            bool defaultEnabled = true) {
            const auto pending =
                pendingProviderStates_.find(
                    std::string(providerId));

            if (pending !=
                pendingProviderStates_.end()) {
                return pending->second;
            }

            return providers::IsEnabled(
                settings.providerEnabled,
                providerId,
                defaultEnabled);
        };

    switch (id) {
    case kIdStartWithWindows: return settings.startWithWindows;
    case kIdSoundEnabled: return settings.soundEnabled;
    case kIdShowTrayIcon: return settings.showTrayIcon;
    case kIdAddToSendToMenu: return settings.addToSendToMenu;
    case kIdUpdateAutoCheck:
        return settings.autoCheckUpdates;
    case kIdUpdatePrerelease:
        return settings.updateChannel ==
            UpdateChannel::Development;
    case kIdPinyinSearch:
        return settings.pinyinSearch;
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
        return providerEnabled(
            providers::
                kEverythingFilesystem,
            false);
    case kIdManagedEverythingTrayIcon:
        return settings
            .managedEverythingShowTrayIcon;
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

RECT SettingsWindow::HotkeyScrollViewport() const {
    RECT client{};
    GetClientRect(
        hwnd_,
        &client);

    const int top =
        Scale(104);
    const int bottom =
        std::max(
            top + Scale(80),
            static_cast<int>(
                client.bottom) -
                Scale(64));

    return {
        Scale(
            kSidebarWidthLogical),
        top,
        static_cast<int>(
            client.right),
        bottom,
    };
}

int SettingsWindow::HotkeyContentBottom() const {
    const int globalTop =
        Scale(140);
    const int globalBottom =
        globalTop +
        HotkeyGroupHeight(true);
    const int launcherTop =
        globalBottom +
        Scale(54);

    return launcherTop +
        HotkeyGroupHeight(false) +
        Scale(8);
}

void SettingsWindow::UpdatePageScrollBar() {
    if (!hwnd_) {
        return;
    }

    // Settings uses an in-client overlay indicator. Keep USER32's non-client
    // scrollbar disabled so it cannot change client width or fight the app's
    // visual language.
    HideSettingsVerticalScrollBar(
        hwnd_);

    if (page_ == Page::General) {
        generalScrollOffset_ =
            std::clamp(
                generalScrollOffset_,
                0,
                PageScrollMaximum());
    } else if (page_ == Page::Hotkeys) {
        hotkeyScrollOffset_ =
            std::clamp(
                hotkeyScrollOffset_,
                0,
                PageScrollMaximum());
    }
}

int SettingsWindow::PageScrollMaximum() const {
    if (!hwnd_) {
        return 0;
    }

    if (page_ == Page::General) {
        RECT client{};
        GetClientRect(
            hwnd_,
            &client);

        const auto full =
            BuildGeneralLayout(0);

        return settings_layout::
            MaxScrollOffset(
                full,
                static_cast<int>(
                    client.bottom),
                dpi_);
    }

    if (page_ == Page::Hotkeys) {
        const RECT viewport =
            HotkeyScrollViewport();
        const int pageHeight =
            std::max(
                1,
                static_cast<int>(
                    viewport.bottom -
                    viewport.top));
        const int contentHeight =
            std::max(
                1,
                HotkeyContentBottom() -
                    static_cast<int>(
                        viewport.top));

        return std::max(
            0,
            contentHeight -
                pageHeight);
    }

    return 0;
}

RECT SettingsWindow::PageScrollTrackRect() const {
    RECT client{};
    if (!hwnd_) {
        return client;
    }

    GetClientRect(
        hwnd_,
        &client);

    const int hitWidth =
        Scale(14);
    const int rightInset =
        Scale(3);
    const int verticalInset =
        Scale(14);

    return {
        std::max(
            client.left,
            client.right -
                rightInset -
                hitWidth),
        client.top +
            verticalInset,
        client.right -
            rightInset,
        std::max(
            client.top +
                verticalInset,
            client.bottom -
                verticalInset),
    };
}

RECT SettingsWindow::PageScrollThumbRect() const {
    RECT track =
        PageScrollTrackRect();
    const int maximum =
        PageScrollMaximum();

    if (maximum <= 0 ||
        track.bottom <= track.top) {
        return {};
    }

    const int trackHeight =
        track.bottom -
        track.top;
    int viewportHeight = 1;

    if (page_ == Page::Hotkeys) {
        const RECT viewport =
            HotkeyScrollViewport();
        viewportHeight =
            std::max(
                1,
                static_cast<int>(
                    viewport.bottom -
                    viewport.top));
    } else {
        RECT client{};
        GetClientRect(
            hwnd_,
            &client);
        viewportHeight =
            std::max(
                1,
                static_cast<int>(
                    client.bottom));
    }

    const int contentHeight =
        viewportHeight +
        maximum;
    const int minimumThumb =
        Scale(48);
    const int thumbHeight =
        std::clamp(
            static_cast<int>(
                (static_cast<long long>(
                     trackHeight) *
                 viewportHeight) /
                std::max(
                    1,
                    contentHeight)),
            std::min(
                minimumThumb,
                trackHeight),
            trackHeight);
    const int travel =
        std::max(
            0,
            trackHeight -
                thumbHeight);
    const int offset =
        page_ == Page::General
            ? generalScrollOffset_
            : hotkeyScrollOffset_;
    const int thumbTop =
        track.top +
        (maximum > 0
             ? static_cast<int>(
                   (static_cast<long long>(
                        travel) *
                    offset) /
                   maximum)
             : 0);

    const int visualWidth =
        Scale(
            pageScrollHovered_ ||
                    pageScrollDragging_
                ? 6
                : 4);
    const int centerX =
        track.left +
        (track.right -
         track.left) / 2;

    return {
        centerX -
            visualWidth / 2,
        thumbTop,
        centerX -
            visualWidth / 2 +
            visualWidth,
        thumbTop +
            thumbHeight,
    };
}

void SettingsWindow::DrawPageScrollBar(
    HDC dc) {

    if (!dc ||
        PageScrollMaximum() <= 0) {
        return;
    }

    RECT thumb =
        PageScrollThumbRect();

    if (thumb.right <= thumb.left ||
        thumb.bottom <= thumb.top) {
        return;
    }

    const COLORREF color =
        pageScrollDragging_
            ? RGB(105, 115, 124)
            : pageScrollHovered_
                ? RGB(126, 135, 144)
                : RGB(166, 173, 181);

    HBRUSH brush =
        CreateSolidBrush(
            color);
    HGDIOBJ oldBrush =
        SelectObject(
            dc,
            brush);
    HGDIOBJ oldPen =
        SelectObject(
            dc,
            GetStockObject(
                NULL_PEN));

    const int radius =
        std::max(
            1,
            static_cast<int>(
                thumb.right -
                thumb.left) / 2);

    RoundRect(
        dc,
        thumb.left,
        thumb.top,
        thumb.right,
        thumb.bottom,
        radius,
        radius);

    SelectObject(
        dc,
        oldPen);
    SelectObject(
        dc,
        oldBrush);
    DeleteObject(
        brush);
}

void SettingsWindow::ScrollCurrentPage(
    int delta) {

    if ((page_ != Page::General &&
         page_ != Page::Hotkeys) ||
        delta == 0) {
        return;
    }

    UpdatePageScrollBar();

    const int maximum =
        PageScrollMaximum();

    int& offset =
        page_ == Page::General
            ? generalScrollOffset_
            : hotkeyScrollOffset_;

    const int next =
        std::clamp(
            offset + delta,
            0,
            maximum);

    if (next == offset) {
        return;
    }

    offset = next;

    window_presentation::ScopedRedrawSuspend
        redrawGuard(hwnd_);
    LayoutCurrentPage();
    redrawGuard.Resume();
    RedrawCurrentPage();
}

bool SettingsWindow::HotkeyControlDesiredVisible(
    HWND control) const {

    if (!control) {
        return false;
    }

    if (control == hotkeyResetAll_ ||
        control == hotkeyGlobalTitle_ ||
        control == hotkeyLauncherTitle_) {
        return true;
    }

    for (const auto& row :
         hotkeyRows_) {
        if (control == row.status) {
            return row.statusVisible;
        }

        if (control == row.reset) {
            return row.resetVisible;
        }

        if (control == row.title ||
            control == row.capture ||
            control == row.enabled) {
            return true;
        }
    }

    return true;
}

void SettingsWindow::
ClipHotkeyControlsToViewport() {
    if (page_ != Page::Hotkeys) {
        return;
    }

    const RECT viewport =
        HotkeyScrollViewport();

    for (HWND control :
         hotkeyControls_) {
        if (!control ||
            control ==
                hotkeyResetAll_) {
            continue;
        }

        const bool desiredVisible =
            HotkeyControlDesiredVisible(
                control);

        if (!desiredVisible) {
            SetWindowRgn(
                control,
                nullptr,
                FALSE);
            ShowWindow(
                control,
                SW_HIDE);
            continue;
        }

        RECT rect{};
        GetWindowRect(
            control,
            &rect);

        MapWindowPoints(
            HWND_DESKTOP,
            hwnd_,
            reinterpret_cast<POINT*>(
                &rect),
            2);

        RECT visible{};

        if (!IntersectRect(
                &visible,
                &rect,
                &viewport)) {
            SetWindowRgn(
                control,
                nullptr,
                FALSE);
            ShowWindow(
                control,
                SW_HIDE);
            continue;
        }

        HRGN region =
            CreateRectRgn(
                visible.left -
                    rect.left,
                visible.top -
                    rect.top,
                visible.right -
                    rect.left,
                visible.bottom -
                    rect.top);

        if (region &&
            !SetWindowRgn(
                control,
                region,
                TRUE)) {
            DeleteObject(
                region);
        }

        ShowWindow(
            control,
            SW_SHOW);
    }

    if (hotkeyResetAll_) {
        SetWindowRgn(
            hotkeyResetAll_,
            nullptr,
            FALSE);
        ShowWindow(
            hotkeyResetAll_,
            SW_SHOW);
    }
}


RECT SettingsWindow::BehaviorCardRect() const {
    const auto metrics =
        BuildGeneralLayout(
            generalScrollOffset_);

    return {
        metrics.behavior.left,
        metrics.behavior.top,
        metrics.behavior.right,
        metrics.behavior.bottom,
    };
}

RECT SettingsWindow::SearchBehaviorCardRect() const {
    const auto metrics =
        BuildGeneralLayout(
            generalScrollOffset_);

    return {
        metrics.search.left,
        metrics.search.top,
        metrics.search.right,
        metrics.search.bottom,
    };
}

RECT SettingsWindow::PlacementCardRect() const {
    const auto metrics =
        BuildGeneralLayout(
            generalScrollOffset_);

    return {
        metrics.placement.left,
        metrics.placement.top,
        metrics.placement.right,
        metrics.placement.bottom,
    };
}


int SettingsWindow::ProviderFilesHeightLogical() const {
    return settings_layout::kToggleRowLogical * (providerTrayVisible_ ? 2 : 1) +
        10 + 42 + (providerActionsVisible_ ? 8 + 34 : 0) + 16;
}

RECT SettingsWindow::ProviderCardRect() const {
    return PageCardRect(
        140,
        settings_layout::
            kToggleRowLogical * 4,
        720);
}

RECT SettingsWindow::PageCardRect(
    int topLogical,
    int heightLogical,
    int maxWidthLogical) const {

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

    const int width =
        std::min(
            contentWidth,
            Scale(maxWidthLogical));

    return {
        contentLeft,
        Scale(topLogical),
        contentLeft + width,
        Scale(
            topLogical +
            heightLogical),
    };
}

void SettingsWindow::DrawNavigationButton(
    const DRAWITEMSTRUCT& item) {

    RECT rect =
        item.rcItem;

    bool selected = false;

    switch (item.CtlID) {
    case kIdNavGeneral:
        selected =
            page_ == Page::General;
        break;
    case kIdNavHotkeys:
        selected =
            page_ == Page::Hotkeys;
        break;
    case kIdNavProviders:
        selected =
            page_ == Page::Providers;
        break;
    case kIdNavAppearance:
        selected =
            page_ == Page::Appearance;
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
    const bool focused =
        (item.itemState &
         ODS_FOCUS) != 0;

    RECT surface = rect;
    InflateRect(
        &surface,
        -Scale(2),
        -Scale(1));

    const COLORREF background =
        pressed
            ? kCardPressed
            : selected
                ? kPalette
                      .selectionBackground
                : focused
                    ? kCardPressed
                    : kSidebarBackground;

    HBRUSH fill =
        CreateSolidBrush(
            background);
    HPEN pen =
        CreatePen(
            PS_SOLID,
            1,
            background);

    HGDIOBJ oldBrush =
        SelectObject(
            item.hDC,
            fill);
    HGDIOBJ oldPen =
        SelectObject(
            item.hDC,
            pen);

    RoundRect(
        item.hDC,
        surface.left,
        surface.top,
        surface.right,
        surface.bottom,
        Scale(7),
        Scale(7));

    SelectObject(
        item.hDC,
        oldBrush);
    SelectObject(
        item.hDC,
        oldPen);
    DeleteObject(fill);
    DeleteObject(pen);

    if (selected) {
        RECT accent{
            surface.left,
            surface.top + Scale(7),
            surface.left + Scale(3),
            surface.bottom - Scale(7),
        };

        HBRUSH accentBrush =
            CreateSolidBrush(
                kAccent);
        FillRect(
            item.hDC,
            &accent,
            accentBrush);
        DeleteObject(
            accentBrush);
    }

    wchar_t textBuffer[96]{};
    GetWindowTextW(
        item.hwndItem,
        textBuffer,
        static_cast<int>(
            std::size(
                textBuffer)));

    SetBkMode(
        item.hDC,
        TRANSPARENT);
    const bool disabled =
        (item.itemState &
         ODS_DISABLED) != 0;
    SetTextColor(
        item.hDC,
        disabled
            ? kMuted
            : kText);

    HGDIOBJ oldFont =
        SelectObject(
            item.hDC,
            selected
                ? sectionFont_
                : normalFont_);

    RECT textRect{
        surface.left + Scale(16),
        surface.top,
        surface.right - Scale(12),
        surface.bottom,
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
}


void SettingsWindow::DrawSwitchGlyph(
    HDC dc,
    const RECT& rect,
    bool checked,
    bool pressed) {

    const int switchWidth =
        rect.right -
        rect.left;
    const int switchHeight =
        rect.bottom -
        rect.top;

    constexpr int kSupersample = 3;
    const int margin =
        std::max(1, Scale(2));
    const int targetWidth =
        switchWidth +
        margin * 2;
    const int targetHeight =
        switchHeight +
        margin * 2;
    const int sourceWidth =
        targetWidth *
        kSupersample;
    const int sourceHeight =
        targetHeight *
        kSupersample;

    HDC switchDc =
        CreateCompatibleDC(dc);

    HBITMAP switchBitmap =
        switchDc
            ? CreateCompatibleBitmap(
                  dc,
                  sourceWidth,
                  sourceHeight)
            : nullptr;

    const COLORREF background =
        pressed
            ? kCardPressed
            : kCardBackground;

    if (switchDc &&
        switchBitmap) {
        HGDIOBJ oldBitmap =
            SelectObject(
                switchDc,
                switchBitmap);

        RECT sourceRect{
            0,
            0,
            sourceWidth,
            sourceHeight,
        };

        HBRUSH sourceBackground =
            CreateSolidBrush(
                background);
        FillRect(
            switchDc,
            &sourceRect,
            sourceBackground);
        DeleteObject(
            sourceBackground);

        const int sourceMargin =
            margin *
            kSupersample;
        const int trackWidth =
            switchWidth *
            kSupersample;
        const int trackHeight =
            switchHeight *
            kSupersample;

        HBRUSH trackBrush =
            CreateSolidBrush(
                checked
                    ? kAccent
                    : RGB(
                          210,
                          216,
                          224));

        HGDIOBJ oldBrush =
            SelectObject(
                switchDc,
                trackBrush);
        HGDIOBJ oldPen =
            SelectObject(
                switchDc,
                GetStockObject(
                    NULL_PEN));

        RoundRect(
            switchDc,
            sourceMargin,
            sourceMargin,
            sourceMargin +
                trackWidth,
            sourceMargin +
                trackHeight,
            trackHeight,
            trackHeight);

        const int knobSize =
            Scale(16) *
            kSupersample;
        const int knobInset =
            Scale(3) *
            kSupersample;
        const int knobLeft =
            checked
                ? sourceMargin +
                    trackWidth -
                    knobInset -
                    knobSize
                : sourceMargin +
                    knobInset;

        HBRUSH knobBrush =
            CreateSolidBrush(
                RGB(
                    255,
                    255,
                    255));

        SelectObject(
            switchDc,
            knobBrush);

        Ellipse(
            switchDc,
            knobLeft,
            sourceMargin +
                knobInset,
            knobLeft +
                knobSize,
            sourceMargin +
                knobInset +
                knobSize);

        SelectObject(
            switchDc,
            oldBrush);
        SelectObject(
            switchDc,
            oldPen);
        DeleteObject(
            trackBrush);
        DeleteObject(
            knobBrush);

        const int oldStretchMode =
            SetStretchBltMode(
                dc,
                HALFTONE);

        POINT oldBrushOrigin{};
        SetBrushOrgEx(
            dc,
            0,
            0,
            &oldBrushOrigin);

        StretchBlt(
            dc,
            rect.left - margin,
            rect.top - margin,
            targetWidth,
            targetHeight,
            switchDc,
            0,
            0,
            sourceWidth,
            sourceHeight,
            SRCCOPY);

        SetBrushOrgEx(
            dc,
            oldBrushOrigin.x,
            oldBrushOrigin.y,
            nullptr);
        SetStretchBltMode(
            dc,
            oldStretchMode);

        SelectObject(
            switchDc,
            oldBitmap);
    } else {
        HBRUSH trackBrush =
            CreateSolidBrush(
                checked
                    ? kAccent
                    : RGB(
                          210,
                          216,
                          224));

        HGDIOBJ oldBrush =
            SelectObject(
                dc,
                trackBrush);
        HGDIOBJ oldPen =
            SelectObject(
                dc,
                GetStockObject(
                    NULL_PEN));

        RoundRect(
            dc,
            rect.left,
            rect.top,
            rect.right,
            rect.bottom,
            switchHeight,
            switchHeight);

        const int knobSize =
            Scale(16);
        const int knobInset =
            Scale(3);
        const int knobLeft =
            checked
                ? rect.right -
                    knobInset -
                    knobSize
                : rect.left +
                    knobInset;

        HBRUSH knobBrush =
            CreateSolidBrush(
                RGB(
                    255,
                    255,
                    255));

        SelectObject(
            dc,
            knobBrush);

        Ellipse(
            dc,
            knobLeft,
            rect.top +
                knobInset,
            knobLeft +
                knobSize,
            rect.top +
                knobInset +
                knobSize);

        SelectObject(
            dc,
            oldBrush);
        SelectObject(
            dc,
            oldPen);
        DeleteObject(
            trackBrush);
        DeleteObject(
            knobBrush);
    }

    if (switchBitmap) {
        DeleteObject(
            switchBitmap);
    }
    if (switchDc) {
        DeleteDC(
            switchDc);
    }
}

void SettingsWindow::DrawHotkeyToggle(
    const DRAWITEMSTRUCT& item,
    std::size_t rowIndex) {

    if (rowIndex >=
        hotkeyRows_.size()) {
        return;
    }

    const auto& row =
        hotkeyRows_[rowIndex];

    const auto binding =
        EffectiveHotkeyBinding(
            app_.SettingsData()
                .hotkeyBindings,
            row.actionId);

    const bool pressed =
        (item.itemState &
         ODS_SELECTED) != 0;
    const bool focused =
        (item.itemState &
         ODS_FOCUS) != 0;

    const COLORREF background =
        pressed || focused
            ? kCardPressed
            : kCardBackground;

    RECT itemRect =
        item.rcItem;

    HBRUSH fill =
        CreateSolidBrush(
            background);
    FillRect(
        item.hDC,
        &itemRect,
        fill);
    DeleteObject(fill);

    const int switchWidth =
        Scale(40);
    const int switchHeight =
        Scale(22);

    RECT switchRect{
        itemRect.left +
            (itemRect.right -
             itemRect.left -
             switchWidth) / 2,
        itemRect.top +
            (itemRect.bottom -
             itemRect.top -
             switchHeight) / 2,
        0,
        0,
    };

    switchRect.right =
        switchRect.left +
        switchWidth;
    switchRect.bottom =
        switchRect.top +
        switchHeight;

    DrawSwitchGlyph(
        item.hDC,
        switchRect,
        binding.enabled,
        pressed || focused);
}

void SettingsWindow::DrawHotkeyResetLink(
    const DRAWITEMSTRUCT& item) {

    RECT rect =
        item.rcItem;

    HBRUSH background =
        CreateSolidBrush(
            kCardBackground);
    FillRect(
        item.hDC,
        &rect,
        background);
    DeleteObject(
        background);

    const bool disabled =
        (item.itemState &
         ODS_DISABLED) != 0;
    const bool pressed =
        (item.itemState &
         ODS_SELECTED) != 0;
    const bool focused =
        (item.itemState &
         ODS_FOCUS) != 0;

    const COLORREF textColor =
        disabled
            ? RGB(155, 162, 171)
            : pressed
                ? RGB(0, 99, 177)
                : kAccent;

    wchar_t buffer[64]{};
    GetWindowTextW(
        item.hwndItem,
        buffer,
        static_cast<int>(
            std::size(buffer)));

    SetBkMode(
        item.hDC,
        TRANSPARENT);
    SetTextColor(
        item.hDC,
        textColor);

    HGDIOBJ oldFont =
        SelectObject(
            item.hDC,
            normalFont_);

    RECT textRect =
        rect;

    DrawTextW(
        item.hDC,
        buffer,
        -1,
        &textRect,
        DT_LEFT |
            DT_VCENTER |
            DT_SINGLELINE |
            DT_NOPREFIX);

    if (focused) {
        SIZE size{};
        if (GetTextExtentPoint32W(
                item.hDC,
                buffer,
                lstrlenW(buffer),
                &size)) {
            HPEN underline =
                CreatePen(
                    PS_SOLID,
                    1,
                    kAccent);
            HGDIOBJ oldPen =
                SelectObject(
                    item.hDC,
                    underline);

            const int y =
                rect.top +
                (rect.bottom -
                 rect.top +
                 size.cy) / 2;

            MoveToEx(
                item.hDC,
                rect.left,
                y,
                nullptr);
            LineTo(
                item.hDC,
                rect.left +
                    size.cx,
                y);

            SelectObject(
                item.hDC,
                oldPen);
            DeleteObject(
                underline);
        }
    }

    SelectObject(
        item.hDC,
        oldFont);
}


void SettingsWindow::DrawActionButton(
    const DRAWITEMSTRUCT& item) {

    RECT rect =
        item.rcItem;

    const bool disabled =
        (item.itemState &
         ODS_DISABLED) != 0;
    const bool pressed =
        (item.itemState &
         ODS_SELECTED) != 0;

    const bool primary =
        item.CtlID ==
                kIdUpdateAction &&
            app_.UpdateStatus().stage ==
                win::UpdateStage::Available;
    const bool danger =
        item.CtlID ==
            kIdDataResetSettings;

    COLORREF fillColor =
        pressed
            ? kCardPressed
            : RGB(255, 255, 255);
    COLORREF borderColor =
        kBorder;
    COLORREF textColor =
        disabled
            ? RGB(155, 162, 171)
            : kText;

    if (primary && !disabled) {
        fillColor =
            pressed
                ? RGB(0, 99, 177)
                : kAccent;
        borderColor =
            fillColor;
        textColor =
            RGB(255, 255, 255);
    } else if (
        danger &&
        !disabled) {
        textColor =
            RGB(190, 45, 45);
        borderColor =
            RGB(226, 185, 185);
    }

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
            item.hDC,
            fill);
    HGDIOBJ oldPen =
        SelectObject(
            item.hDC,
            pen);

    RoundRect(
        item.hDC,
        surface.left,
        surface.top,
        surface.right,
        surface.bottom,
        Scale(6),
        Scale(6));

    SelectObject(
        item.hDC,
        oldBrush);
    SelectObject(
        item.hDC,
        oldPen);
    DeleteObject(fill);
    DeleteObject(pen);

    wchar_t buffer[160]{};
    GetWindowTextW(
        item.hwndItem,
        buffer,
        static_cast<int>(
            std::size(buffer)));

    SetBkMode(
        item.hDC,
        TRANSPARENT);
    SetTextColor(
        item.hDC,
        textColor);

    HGDIOBJ oldFont =
        SelectObject(
            item.hDC,
            normalFont_);

    RECT textRect =
        surface;
    InflateRect(
        &textRect,
        -Scale(10),
        0);

    DrawTextW(
        item.hDC,
        buffer,
        -1,
        &textRect,
        DT_CENTER |
            DT_VCENTER |
            DT_SINGLELINE |
            DT_END_ELLIPSIS |
            DT_NOPREFIX);

    SelectObject(
        item.hDC,
        oldFont);

    if (item.itemState &
        ODS_FOCUS) {
        RECT focus =
            surface;
        InflateRect(
            &focus,
            -Scale(5),
            -Scale(4));
        DrawFocusRect(
            item.hDC,
            &focus);
    }
}




void SettingsWindow::DrawGitHubLink(
    const DRAWITEMSTRUCT& item) {

    RECT rect =
        item.rcItem;

    HBRUSH background =
        CreateSolidBrush(
            kWindowBackground);
    FillRect(
        item.hDC,
        &rect,
        background);
    DeleteObject(
        background);

    const bool disabled =
        (item.itemState &
         ODS_DISABLED) != 0;
    const bool pressed =
        (item.itemState &
         ODS_SELECTED) != 0;
    const bool focused =
        (item.itemState &
         ODS_FOCUS) != 0;

    const COLORREF textColor =
        disabled
            ? kMuted
            : pressed
                ? RGB(0, 99, 177)
                : kAccent;

    wchar_t buffer[64]{};
    GetWindowTextW(
        item.hwndItem,
        buffer,
        static_cast<int>(
            std::size(buffer)));

    SetBkMode(
        item.hDC,
        TRANSPARENT);
    SetTextColor(
        item.hDC,
        textColor);

    HGDIOBJ oldFont =
        SelectObject(
            item.hDC,
            normalFont_);

    RECT textRect =
        rect;

    DrawTextW(
        item.hDC,
        buffer,
        -1,
        &textRect,
        DT_LEFT |
            DT_VCENTER |
            DT_SINGLELINE |
            DT_NOPREFIX);

    if (focused) {
        SIZE size{};
        if (GetTextExtentPoint32W(
                item.hDC,
                buffer,
                lstrlenW(buffer),
                &size)) {
            HPEN underline =
                CreatePen(
                    PS_SOLID,
                    1,
                    kAccent);
            HGDIOBJ oldPen =
                SelectObject(
                    item.hDC,
                    underline);

            const int y =
                rect.top +
                (rect.bottom -
                 rect.top +
                 size.cy) / 2;

            MoveToEx(
                item.hDC,
                rect.left,
                y,
                nullptr);
            LineTo(
                item.hDC,
                rect.left +
                    size.cx,
                y);

            SelectObject(
                item.hDC,
                oldPen);
            DeleteObject(
                underline);
        }
    }

    SelectObject(
        item.hDC,
        oldFont);
}


void SettingsWindow::DrawUpdateStatus(
    const DRAWITEMSTRUCT& item) {

    RECT rect =
        item.rcItem;

    HBRUSH background =
        CreateSolidBrush(
            kCardBackground);
    FillRect(
        item.hDC,
        &rect,
        background);
    DeleteObject(
        background);

    wchar_t buffer[512]{};
    GetWindowTextW(
        item.hwndItem,
        buffer,
        static_cast<int>(
            std::size(buffer)));

    SetBkMode(
        item.hDC,
        TRANSPARENT);
    SetTextColor(
        item.hDC,
        kMuted);

    HGDIOBJ oldFont =
        SelectObject(
            item.hDC,
            normalFont_);

    RECT measured{
        0,
        0,
        rect.right -
            rect.left,
        0,
    };

    DrawTextW(
        item.hDC,
        buffer,
        -1,
        &measured,
        DT_LEFT |
            DT_WORDBREAK |
            DT_CALCRECT |
            DT_NOPREFIX);

    const int textHeight =
        measured.bottom -
        measured.top;
    const int availableHeight =
        rect.bottom -
        rect.top;
    const int top =
        rect.top +
        std::max(
            0,
            (availableHeight -
             textHeight) / 2);

    RECT textRect{
        rect.left,
        top,
        rect.right,
        rect.bottom,
    };

    DrawTextW(
        item.hDC,
        buffer,
        -1,
        &textRect,
        DT_LEFT |
            DT_WORDBREAK |
            DT_NOPREFIX);

    SelectObject(
        item.hDC,
        oldFont);
}


void SettingsWindow::DrawGeneralToggle(
    const DRAWITEMSTRUCT& item) {

    RECT rect =
        item.rcItem;

    const bool pressed =
        (item.itemState &
         ODS_SELECTED) != 0;

    const COLORREF rowColor =
        pressed
            ? kCardPressed
            : kCardBackground;

    HBRUSH rowBrush =
        CreateSolidBrush(
            rowColor);

    FillRect(
        item.hDC,
        &rect,
        rowBrush);
    DeleteObject(
        rowBrush);

    const UINT id =
        static_cast<UINT>(
            item.CtlID);

    const bool checked =
        ToggleChecked(id);

    const wchar_t* title = L"";

    switch (id) {
    case kIdStartWithWindows: title = T(L"开机启动", L"Start with Windows"); break;
    case kIdSoundEnabled: title = T(L"提示音", L"Sound effects"); break;
    case kIdShowTrayIcon: title = T(L"显示系统托盘图标", L"Show system tray icon"); break;
    case kIdAddToSendToMenu: title = T(L"添加到“发送到”菜单", L"Add to “Send to” menu"); break;
    case kIdPinyinSearch:
        title =
            T(L"启用拼音搜索",
              L"Enable Pinyin search");
        break;
    case kIdNumericQuickLaunch:
        title =
            T(L"数字键快速执行结果",
              L"Quick launch with number keys");
        break;
    case kIdExecuteSingleResult:
        title =
            T(L"仅剩一个结果时立即执行",
              L"Execute when one result remains");
        break;
    case kIdProviderStartMenu:
        title =
            T(L"开始菜单",
              L"Start Menu");
        break;
    case kIdProviderPackaged:
        title = L"Windows Apps";
        break;
    case kIdProviderAppPaths:
        title = L"App Paths";
        break;
    case kIdProviderPath:
        title = L"PATH";
        break;
    case kIdProviderEverything:
        title =
            T(L"Everything 文件与文件夹",
              L"Everything files & folders");
        break;
    case kIdManagedEverythingTrayIcon:
        title =
            T(L"显示 Everything 托盘图标",
              L"Show Everything tray icon");
        break;
    case kIdUpdateAutoCheck:
        title =
            T(L"自动检查更新",
              L"Automatically check for updates");
        break;
    case kIdUpdatePrerelease:
        title =
            T(L"接收预发布版本更新",
              L"Get prerelease updates");
        break;
    default:
        break;
    }

    const int switchWidth =
        Scale(40);
    const int switchHeight =
        Scale(22);

    RECT switchRect{
        rect.right -
            Scale(18) -
            switchWidth,
        rect.top +
            (rect.bottom -
             rect.top -
             switchHeight) / 2,
        rect.right -
            Scale(18),
        0,
    };

    switchRect.bottom =
        switchRect.top +
        switchHeight;

    DrawSwitchGlyph(
        item.hDC,
        switchRect,
        checked,
        pressed);

    SetBkMode(
        item.hDC,
        TRANSPARENT);
    const bool disabled =
        (item.itemState &
         ODS_DISABLED) != 0;
    SetTextColor(
        item.hDC,
        disabled
            ? kMuted
            : kText);

    HGDIOBJ oldFont =
        SelectObject(
            item.hDC,
            normalFont_);

    RECT titleRect{
        rect.left +
            Scale(18),
        rect.top,
        switchRect.left -
            Scale(16),
        rect.bottom,
    };

    DrawTextW(
        item.hDC,
        title,
        -1,
        &titleRect,
        DT_LEFT |
            DT_SINGLELINE |
            DT_VCENTER |
            DT_END_ELLIPSIS |
            DT_NOPREFIX);

    SelectObject(
        item.hDC,
        oldFont);

    const bool lastRow =
        id ==
            kIdExecuteSingleResult ||
        id ==
            kIdProviderPath ||
        id ==
            kIdManagedEverythingTrayIcon ||
        (id == kIdProviderEverything && !providerTrayVisible_);

    if (!lastRow) {
        HPEN separator =
            CreatePen(
                PS_SOLID,
                1,
                kBorder);

        HGDIOBJ oldPen =
            SelectObject(
                item.hDC,
                separator);

        MoveToEx(
            item.hDC,
            rect.left +
                Scale(18),
            rect.bottom - 1,
            nullptr);
        LineTo(
            item.hDC,
            rect.right -
                Scale(18),
            rect.bottom - 1);

        SelectObject(
            item.hDC,
            oldPen);
        DeleteObject(
            separator);
    }

    if (item.itemState &
        ODS_FOCUS) {
        RECT focusBar{
            rect.left +
                Scale(5),
            rect.top +
                Scale(12),
            rect.left +
                Scale(7),
            rect.bottom -
                Scale(12),
        };

        HBRUSH focusBrush =
            CreateSolidBrush(
                kAccent);
        FillRect(
            item.hDC,
            &focusBar,
            focusBrush);
        DeleteObject(
            focusBrush);
    }
}

void SettingsWindow::PositionForShow() {
    RECT rect{};
    GetWindowRect(
        hwnd_,
        &rect);

    const int requestedWidth =
        rect.right - rect.left;
    const int requestedHeight =
        rect.bottom - rect.top;

    const auto& settings =
        app_.SettingsData();

    if (settings.settingsPlacement ==
            "last" &&
        settings.settingsLastPositionValid) {

        RECT requested{
            settings.settingsLastX,
            settings.settingsLastY,
            settings.settingsLastX +
                requestedWidth,
            settings.settingsLastY +
                requestedHeight,
        };

        HMONITOR monitor =
            MonitorFromRect(
                &requested,
                MONITOR_DEFAULTTONEAREST);

        MONITORINFO info{
            sizeof(info)};

        if (GetMonitorInfoW(
                monitor,
                &info)) {

            const auto clamped =
                settings_layout::
                    ClampRectToWorkArea(
                        {
                            requested.left,
                            requested.top,
                            requested.right,
                            requested.bottom,
                        },
                        {
                            info.rcWork.left,
                            info.rcWork.top,
                            info.rcWork.right,
                            info.rcWork.bottom,
                        });

            SetWindowPos(
                hwnd_,
                nullptr,
                clamped.left,
                clamped.top,
                clamped.right -
                    clamped.left,
                clamped.bottom -
                    clamped.top,
                SWP_NOZORDER |
                    SWP_NOACTIVATE);
            return;
        }
    }

    POINT cursor{};
    GetCursorPos(
        &cursor);

    HMONITOR monitor =
        MonitorFromPoint(
            cursor,
            MONITOR_DEFAULTTONEAREST);

    MONITORINFO info{
        sizeof(info)};
    GetMonitorInfoW(
        monitor,
        &info);

    const int workWidth =
        info.rcWork.right -
        info.rcWork.left;
    const int workHeight =
        info.rcWork.bottom -
        info.rcWork.top;

    const int width =
        std::min(
            requestedWidth,
            workWidth);
    const int height =
        std::min(
            requestedHeight,
            workHeight);

    const auto origin =
        settings_layout::
            ResolveWindowOrigin(
                {
                    static_cast<int>(
                        info.rcWork.left),
                    static_cast<int>(
                        info.rcWork.top),
                    static_cast<int>(
                        info.rcWork.right),
                    static_cast<int>(
                        info.rcWork.bottom),
                },
                width,
                height,
                settings.settingsPlacement ==
                    "top",
                Scale(45));

    SetWindowPos(
        hwnd_,
        nullptr,
        origin.x,
        origin.y,
        width,
        height,
        SWP_NOZORDER |
            SWP_NOACTIVATE);
}

void SettingsWindow::OnUpdateStatusChanged() {
    RefreshUpdateStatus();
    SyncUpdateStatusTimer();
}

void SettingsWindow::SyncUpdateStatusTimer() {
    if (!hwnd_ ||
        !IsWindow(hwnd_)) {
        return;
    }

    const auto status =
        app_.UpdateStatus();
    const bool active =
        status.running ||
        app_.UpdateWorkerRunning();

    if (page_ == Page::About &&
        active) {
        SetTimer(
            hwnd_,
            kUpdateStatusTimerId,
            250,
            nullptr);
    } else {
        KillTimer(
            hwnd_,
            kUpdateStatusTimerId);
    }
}

void SettingsWindow::TogglePrereleaseUpdates() {
    if (syncing_) {
        return;
    }

    const auto settings =
        app_.SettingsData();

    const UpdateChannel nextChannel =
        settings.updateChannel ==
                UpdateChannel::Development
            ? UpdateChannel::Stable
            : UpdateChannel::Development;

    if (!app_.SetUpdateSettings(
            settings.autoCheckUpdates,
            nextChannel)) {
        altrun::ui::ShowMessage(
            hwnd_,
            T(L"无法保存更新设置。",
              L"Could not save update settings."),
            L"Asterun",
            MB_OK | MB_ICONERROR);
    }

    RefreshFromSettings();
}

void SettingsWindow::RefreshUpdateStatus() {
    if (!updateStatus_ ||
        !updateAction_) {
        return;
    }

    const auto status =
        app_.UpdateStatus();
    const bool settingsChanged =
        app_.UpdateSettingsChangedSinceCheck();
    const bool workerRunning =
        app_.UpdateWorkerRunning();

    std::wstring text;
    const wchar_t* actionText =
        T(L"检查更新",
          L"Check for updates");
    bool actionEnabled = false;

    switch (status.stage) {
    case win::UpdateStage::Idle:
        text =
            settingsChanged
                ? T(L"尚未按当前设置检查更新。",
                    L"Updates have not been checked with the current settings.")
                : T(L"尚未检查更新。",
                    L"Updates have not been checked yet.");
        actionEnabled = true;
        break;

    case win::UpdateStage::Checking:
        text =
            T(L"正在检查更新...",
              L"Checking for updates...");
        actionText =
            T(L"正在检查...",
              L"Checking...");
        break;

    case win::UpdateStage::UpToDate:
        text =
            T(L"已是最新版本。",
              L"You're up to date.");
        actionEnabled = true;
        break;

    case win::UpdateStage::ChannelNotNewer:
        text =
            T(L"正式版最新为 v",
              L"The latest stable release is v");
        text += std::wstring(
            status.availableVersion.begin(),
            status.availableVersion.end());
        text +=
            T(L"；当前版本较新，不会降级。",
              L"; this build is newer, so no downgrade will be offered.");
        actionEnabled = true;
        break;

    case win::UpdateStage::Available:
        text =
            T(L"发现新版本：",
              L"New version available: ");
        text += std::wstring(
            status.availableVersion.begin(),
            status.availableVersion.end());
        actionText =
            T(L"下载并安装",
              L"Download and install");
        actionEnabled = true;
        break;

    case win::UpdateStage::Downloading:
        text =
            T(L"正在下载更新",
              L"Downloading update");

        if (status.downloadedBytes > 0) {
            text += L"  ·  ";
            text += FormatBytes(
                status.downloadedBytes);

            if (status.totalBytes > 0) {
                text += L" / ";
                text += FormatBytes(
                    status.totalBytes);
            }
        }

        actionText =
            T(L"正在下载...",
              L"Downloading...");
        break;

    case win::UpdateStage::Verifying:
        text =
            T(L"正在校验更新包 SHA-256...",
              L"Verifying update package SHA-256...");
        actionText =
            T(L"正在校验...",
              L"Verifying...");
        break;

    case win::UpdateStage::Extracting:
        text =
            T(L"正在准备更新文件...",
              L"Preparing update files...");
        actionText =
            T(L"正在准备...",
              L"Preparing...");
        break;

    case win::UpdateStage::ReadyToInstall:
        text =
            T(L"更新已下载并校验，正在准备安装...",
              L"Update downloaded and verified; preparing installation...");
        actionText =
            T(L"正在安装...",
              L"Installing...");
        break;

    case win::UpdateStage::Applying:
        text =
            T(L"正在启动安全更新程序，Asterun 将退出并自动重新启动。",
              L"Starting the safe updater. Asterun will exit and restart automatically.");
        actionText =
            T(L"正在更新...",
              L"Updating...");
        break;

    case win::UpdateStage::Failed:
        text =
            T(L"更新失败：",
              L"Update failed: ");

        switch (status.failure) {
        case win::UpdateFailure::ManifestDownloadFailed:
            text +=
                IsTransientUpdateHttpStatus(
                    status.nativeError)
                    ? T(L"暂时无法检查更新，请稍后重试",
                        L"temporarily unable to check for updates; try again later")
                    : T(L"无法获取更新清单",
                        L"could not fetch the update manifest");
            break;
        case win::UpdateFailure::CheckTimedOut:
            text +=
                T(L"检查更新超时，请重试",
                  L"update check timed out; try again");
            break;
        case win::UpdateFailure::StableManifestUnavailable:
            text +=
                T(L"最新稳定版未提供应用内更新清单，请从 GitHub 手动更新",
                  L"the latest stable release does not provide an in-app update manifest; update manually from GitHub");
            break;
        case win::UpdateFailure::ManifestInvalid:
            text +=
                T(L"更新清单无效",
                  L"invalid update manifest");
            break;
        case win::UpdateFailure::UnsupportedArchitecture:
            text +=
                T(L"当前架构没有可用更新包",
                  L"no package is available for this architecture");
            break;
        case win::UpdateFailure::AssetDownloadFailed:
            text +=
                T(L"下载更新包失败",
                  L"package download failed");
            break;
        case win::UpdateFailure::AssetHashFailed:
            text +=
                T(L"无法计算更新包 SHA-256",
                  L"could not calculate package SHA-256");
            break;
        case win::UpdateFailure::AssetHashMismatch:
            text +=
                T(L"SHA-256 校验不一致，已拒绝更新",
                  L"SHA-256 mismatch; update rejected");
            break;
        case win::UpdateFailure::ExtractionFailed:
            text +=
                T(L"解压更新包失败",
                  L"could not extract the update package");
            break;
        case win::UpdateFailure::StagedPackageInvalid:
            text +=
                T(L"更新包内容不完整或版本不匹配",
                  L"staged package is incomplete or has the wrong version");
            break;
        case win::UpdateFailure::UpdaterMissing:
            text +=
                T(L"缺少 Update.exe",
                  L"Update.exe is missing");
            break;
        case win::UpdateFailure::LaunchUpdaterFailed:
            text +=
                T(L"无法启动更新程序",
                  L"could not start the updater");
            break;
        case win::UpdateFailure::Cancelled:
            text +=
                T(L"操作已取消",
                  L"operation cancelled");
            break;
        case win::UpdateFailure::UnexpectedFailure:
            text +=
                T(L"后台任务遇到意外错误，请重试",
                  L"the background task encountered an unexpected error; try again");
            break;
        default:
            text +=
                T(L"未知错误",
                  L"unknown error");
            break;
        }

        if (status.nativeError != 0 &&
            status.failure !=
                win::UpdateFailure::
                    CheckTimedOut &&
            !IsTransientUpdateHttpStatus(
                status.nativeError)) {
            text +=
                T(L"  ·  系统错误 ",
                  L"  ·  native error ");
            text += std::to_wstring(
                status.nativeError);
        }

        actionText =
            T(L"重试",
              L"Retry");
        actionEnabled = true;
        break;
    }

    SetWindowTextW(
        updateStatus_,
        text.c_str());
    SetWindowTextW(
        updateAction_,
        actionText);

    EnableWindow(
        updateAction_,
        actionEnabled &&
                !status.running &&
                !workerRunning
            ? TRUE
            : FALSE);

    EnableWindow(
        updateAutoCheck_,
        TRUE);

    EnableWindow(
        updatePrerelease_,
        TRUE);

    // updateStatus_ is SS_OWNERDRAW. WM_SETTEXT updates its backing text
    // but does not reliably repaint the pixels that DrawUpdateStatus owns.
    // Force the status surface to paint synchronously so completion from the
    // worker/watchdog cannot leave the visible page stuck on "Checking...".
    RedrawWindow(
        updateStatus_,
        nullptr,
        nullptr,
        RDW_INVALIDATE |
            RDW_ERASE |
            RDW_UPDATENOW);

    RedrawWindow(
        updateAction_,
        nullptr,
        nullptr,
        RDW_INVALIDATE |
            RDW_ERASE |
            RDW_UPDATENOW);
}

void SettingsWindow::ShowAbout() {
    Present(true);
}

void SettingsWindow::Show() {
    Present(false);
}

void SettingsWindow::Present(
    bool selectAbout) {

    if (!EnsureCreated()) {
        return;
    }

    CancelHotkeyCapture(false);

    // Retry a binding that previously failed, but never tear down a
    // working hotkey merely because the Settings window was opened.
    app_.RepairGlobalHotkey(false);
    RefreshFromSettings();

    // Select the requested page before the first visible frame. The old About
    // path called Show() first and only then switched General -> About, so the
    // user could see a fully opened Settings window repaint into About.
    if (selectAbout) {
        ShowPage(Page::About);
    }

    if (page_ == Page::Providers) {
        SetTimer(
            hwnd_,
            kProviderStatusTimerId,
            1000,
            nullptr);
        RefreshProviderStatus();
    }

    if (!IsWindowVisible(hwnd_)) {
        // Finish placement/layout while hidden, expose one fully-painted
        // frame without activation, then perform exactly one foreground
        // transition below.
        PositionForShow();

        window_presentation::
            RevealFullyPainted(
                hwnd_);
    } else if (IsIconic(hwnd_)) {
        window_presentation::
            RestoreFullyPainted(
                hwnd_);
    }

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

    const auto isHotkeyCaptureWindow =
        [&](HWND control) {
            if (!control) {
                return false;
            }

            const UINT id =
                static_cast<UINT>(
                    GetDlgCtrlID(
                        control));

            return
                id >= kIdHotkeyCaptureBase &&
                id <
                    kIdHotkeyCaptureBase +
                        hotkeyRows_.size();
        };

    const auto dismissComboFocus =
        [&]() {
            const HWND focused =
                GetFocus();

            if (!focused) {
                return;
            }

            for (HWND combo :
                 std::array<HWND, 7>{
                     startupBehavior_,
                     popupMonitor_,
                     launcherPlacement_,
                     settingsPlacement_,
                     shortcutManagerPlacement_,
                     uiStyle_,
                     language_}) {
                if (focused == combo) {
                    SetFocus(hwnd_);
                    return;
                }
            }
        };

    switch (message) {
    case WM_SYSCOMMAND:
        if ((wParam & 0xFFF0u) ==
                SC_RESTORE &&
            IsIconic(hwnd_)) {
            window_presentation::
                RestoreFullyPainted(
                    hwnd_);
            SetForegroundWindow(
                hwnd_);
            return 0;
        }
        break;

    case WM_SETCURSOR: {
        const HWND cursorWindow =
            reinterpret_cast<HWND>(
                wParam);
        const UINT cursorId =
            cursorWindow
                ? static_cast<UINT>(
                      GetDlgCtrlID(
                          cursorWindow))
                : 0;

        if (cursorWindow ==
                openGitHub_ ||
            (cursorId >=
                 kIdHotkeyResetBase &&
             cursorId <
                 kIdHotkeyResetBase +
                     hotkeyRows_.size())) {
            SetCursor(
                LoadCursorW(
                    nullptr,
                    IDC_HAND));
            return TRUE;
        }
        break;
    }

    case WM_MOUSEMOVE: {
        POINTS raw =
            MAKEPOINTS(lParam);
        POINT point{
            raw.x,
            raw.y,
        };

        if (pageScrollDragging_) {
            const RECT track =
                PageScrollTrackRect();
            const RECT thumb =
                PageScrollThumbRect();
            const int maximum =
                PageScrollMaximum();
            const int travel =
                std::max(
                    1,
                    static_cast<int>(
                        track.bottom -
                        track.top -
                        (thumb.bottom -
                         thumb.top)));
            int& offset =
                page_ == Page::General
                    ? generalScrollOffset_
                    : hotkeyScrollOffset_;
            const int next =
                std::clamp(
                    pageScrollDragStartOffset_ +
                        static_cast<int>(
                            (static_cast<long long>(
                                 point.y -
                                 pageScrollDragAnchorY_) *
                             maximum) /
                            travel),
                    0,
                    maximum);

            if (next != offset) {
                offset = next;

                window_presentation::ScopedRedrawSuspend
                    redrawGuard(hwnd_);
                LayoutCurrentPage();
                redrawGuard.Resume();
                RedrawCurrentPage();
            }
            return 0;
        }

        const RECT track =
            PageScrollTrackRect();
        const bool hovered =
            PageScrollMaximum() > 0 &&
            PtInRect(
                &track,
                point) != FALSE;

        if (hovered !=
            pageScrollHovered_) {
            pageScrollHovered_ =
                hovered;
            InvalidateRect(
                hwnd_,
                &track,
                FALSE);
        }

        if (hovered) {
            TRACKMOUSEEVENT trackMouse{
                sizeof(trackMouse),
                TME_LEAVE,
                hwnd_,
                0,
            };
            TrackMouseEvent(
                &trackMouse);
        }
        break;
    }

    case WM_MOUSELEAVE:
        if (!pageScrollDragging_ &&
            pageScrollHovered_) {
            const RECT track =
                PageScrollTrackRect();
            pageScrollHovered_ = false;
            InvalidateRect(
                hwnd_,
                &track,
                FALSE);
        }
        return 0;

    case WM_LBUTTONDOWN: {
        dismissComboFocus();

        if (!capturingHotkeyActionId_
                 .empty()) {
            CancelHotkeyCapture();
        }

        POINTS raw =
            MAKEPOINTS(lParam);
        POINT point{
            raw.x,
            raw.y,
        };
        const RECT track =
            PageScrollTrackRect();

        if (PageScrollMaximum() > 0 &&
            PtInRect(
                &track,
                point)) {
            const RECT thumb =
                PageScrollThumbRect();

            pageScrollHovered_ = true;

            if (PtInRect(
                    &thumb,
                    point)) {
                pageScrollDragging_ = true;
                pageScrollDragAnchorY_ =
                    point.y;
                pageScrollDragStartOffset_ =
                    page_ == Page::General
                        ? generalScrollOffset_
                        : hotkeyScrollOffset_;
                SetCapture(
                    hwnd_);
            } else {
                RECT client{};
                GetClientRect(
                    hwnd_,
                    &client);
                const int pageStep =
                    page_ == Page::Hotkeys
                        ? std::max(
                              Scale(80),
                              static_cast<int>(
                                  HotkeyScrollViewport()
                                      .bottom -
                                  HotkeyScrollViewport()
                                      .top) -
                                  Scale(40))
                        : std::max(
                              Scale(80),
                              static_cast<int>(
                                  client.bottom) -
                                  Scale(80));

                ScrollCurrentPage(
                    point.y <
                            thumb.top
                        ? -pageStep
                        : pageStep);
            }

            InvalidateRect(
                hwnd_,
                &track,
                FALSE);
            return 0;
        }
        break;
    }

    case WM_LBUTTONUP:
        if (pageScrollDragging_) {
            pageScrollDragging_ = false;
            if (GetCapture() ==
                hwnd_) {
                ReleaseCapture();
            }
            const RECT track =
                PageScrollTrackRect();
            InvalidateRect(
                hwnd_,
                &track,
                FALSE);
            return 0;
        }
        break;

    case WM_CAPTURECHANGED:
        if (pageScrollDragging_) {
            pageScrollDragging_ = false;
            const RECT track =
                PageScrollTrackRect();
            InvalidateRect(
                hwnd_,
                &track,
                FALSE);
        }
        break;

    case WM_RBUTTONDOWN:
    case WM_MBUTTONDOWN:
        dismissComboFocus();

        if (!capturingHotkeyActionId_
                 .empty()) {
            CancelHotkeyCapture();
        }
        break;

    case WM_PARENTNOTIFY:
        if (LOWORD(wParam) ==
                WM_LBUTTONDOWN ||
            LOWORD(wParam) ==
                WM_RBUTTONDOWN ||
            LOWORD(wParam) ==
                WM_MBUTTONDOWN) {
            // Let interactive child controls transfer focus directly. The
            // previous unconditional SetFocus(hwnd_) inserted an unnecessary
            // Combo A -> Settings -> Combo B transition and made the old
            // ComboBox visibly repaint before the new one received focus.
            POINT screenPoint{};
            GetCursorPos(
                &screenPoint);
            POINT point =
                screenPoint;
            ScreenToClient(
                hwnd_,
                &point);

            HWND clickedChild =
                ChildWindowFromPointEx(
                    hwnd_,
                    point,
                    CWP_SKIPINVISIBLE |
                        CWP_SKIPDISABLED);

            bool passiveSurface = false;

            if (clickedChild &&
                clickedChild != hwnd_) {
                wchar_t className[32]{};

                if (GetClassNameW(
                        clickedChild,
                        className,
                        static_cast<int>(
                            _countof(
                                className))) > 0) {
                    passiveSurface =
                        lstrcmpiW(
                            className,
                            L"Static") == 0;
                }
            }

            if (passiveSurface) {
                dismissComboFocus();
            }

            if (!capturingHotkeyActionId_
                     .empty()) {
                const HWND clicked =
                    WindowFromPoint(
                        screenPoint);

                if (LOWORD(wParam) !=
                        WM_LBUTTONDOWN ||
                    !isHotkeyCaptureWindow(
                        clicked)) {
                    CancelHotkeyCapture();
                }
            }
        }
        break;

    case WM_NCLBUTTONDOWN:
    case WM_NCRBUTTONDOWN:
    case WM_NCMBUTTONDOWN:
        if (!capturingHotkeyActionId_
                 .empty()) {
            CancelHotkeyCapture();
        }
        break;

    case WM_ACTIVATE:
        if (LOWORD(wParam) ==
                WA_INACTIVE &&
            !capturingHotkeyActionId_
                 .empty()) {
            CancelHotkeyCapture();
        }
        break;

    case WM_TIMER:
        if (wParam ==
            kUpdateStatusTimerId) {
            if (page_ == Page::About) {
                RefreshUpdateStatus();
                SyncUpdateStatusTimer();
            } else {
                KillTimer(
                    hwnd_,
                    kUpdateStatusTimerId);
            }
            return 0;
        }

        if (wParam ==
            kProviderCommitTimerId) {
            CommitPendingProviderChanges();
            return 0;
        }

        if (wParam ==
            kProviderStatusTimerId &&
            page_ == Page::Providers) {
            RefreshProviderStatus();
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
        HWND commandControl =
            reinterpret_cast<HWND>(
                lParam);

        switch (id) {
        case kIdStartupBehavior:
        case kIdPopupMonitor:
        case kIdLauncherPlacement:
        case kIdSettingsPlacement:
        case kIdShortcutManagerPlacement:
        case kIdUiStyle:
        case kIdLanguage:
            ui::RefreshNextComboBoxState(
                commandControl,
                notify);
            break;

        default:
            break;
        }

        const auto redrawClickedToggle =
            [&]() {
                HWND control =
                    commandControl;
                if (!control) {
                    return;
                }

                InvalidateRect(
                    control,
                    nullptr,
                    TRUE);
                UpdateWindow(
                    control);
            };

        // BS_OWNERDRAW buttons report the second press of a rapid
        // double-click as BN_DOUBLECLICKED rather than BN_CLICKED.
        // Treat both notifications as one physical toggle activation so
        // every quick click flips the setting exactly once.
        const bool toggleActivated =
            notify == BN_CLICKED ||
            notify == BN_DOUBLECLICKED;

        if (id >= kIdHotkeyCaptureBase &&
            id < kIdHotkeyCaptureBase +
                hotkeyRows_.size()) {
            if (notify == BN_CLICKED) {
                if (auto* row =
                        HotkeyRowFromControlId(
                            id,
                            kIdHotkeyCaptureBase)) {
                    BeginHotkeyCapture(
                        row->actionId);
                }
            }
            return 0;
        }

        if (id >= kIdHotkeyEnabledBase &&
            id < kIdHotkeyEnabledBase +
                hotkeyRows_.size()) {
            if (toggleActivated) {
                if (auto* row =
                        HotkeyRowFromControlId(
                            id,
                            kIdHotkeyEnabledBase)) {
                    ToggleHotkeyActionEnabled(
                        row->actionId);
                    redrawClickedToggle();
                }
            }
            return 0;
        }

        if (id >= kIdHotkeyResetBase &&
            id < kIdHotkeyResetBase +
                hotkeyRows_.size()) {
            if (notify == BN_CLICKED) {
                if (auto* row =
                        HotkeyRowFromControlId(
                            id,
                            kIdHotkeyResetBase)) {
                    ResetHotkeyAction(
                        row->actionId);
                }
            }
            return 0;
        }

        switch (id) {
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

        case kIdStartWithWindows:
        case kIdSoundEnabled:
        case kIdShowTrayIcon:
        case kIdAddToSendToMenu:
            if (toggleActivated) {
                ToggleGeneralSetting(id);
                redrawClickedToggle();
            }
            return 0;

        case kIdPinyinSearch:
        case kIdNumericQuickLaunch:
        case kIdExecuteSingleResult:
            if (toggleActivated) {
                ApplyClassicBehaviorControl(id);
                redrawClickedToggle();
            }
            return 0;

        case kIdStartupBehavior:
            if (notify == CBN_SELCHANGE) {
                ApplyStartupBehaviorControl();
            }
            return 0;

        case kIdHotkeyResetAll:
            if (notify == BN_CLICKED) {
                ResetAllHotkeys();
            }
            return 0;

        case kIdProviderStartMenu:
        case kIdProviderPackaged:
        case kIdProviderAppPaths:
        case kIdProviderPath:
        case kIdProviderEverything:
            if (toggleActivated) {
                ToggleProviderSetting(id);
                redrawClickedToggle();
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

        case kIdProviderUpdateEverything:
            if (notify == BN_CLICKED) {
                CheckOrUpdateEverything();
            }
            return 0;

        case kIdManagedEverythingTrayIcon:
            if (toggleActivated) {
                ToggleManagedEverythingTrayIcon();
                redrawClickedToggle();
            }
            return 0;

        case kIdPopupMonitor:
            if (notify == CBN_SELCHANGE) {
                ApplyMonitorControl();
            }
            return 0;

        case kIdLauncherPlacement:
        case kIdSettingsPlacement:
        case kIdShortcutManagerPlacement:
            if (notify == CBN_SELCHANGE) {
                ApplyWindowPlacementControls();
            }
            return 0;

        case kIdDataImportTsv:
            if (notify == BN_CLICKED) {
                ImportCommands();
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

        case kIdUpdatePrerelease:
            if (toggleActivated &&
                !syncing_) {
                TogglePrereleaseUpdates();
                redrawClickedToggle();
            }
            return 0;

        case kIdUpdateAutoCheck:
            if (toggleActivated &&
                !syncing_) {
                const auto settings =
                    app_.SettingsData();

                if (!app_.SetUpdateSettings(
                        !settings.autoCheckUpdates,
                        settings.updateChannel)) {
                    altrun::ui::ShowMessage(
                        hwnd_,
                        T(L"无法保存更新设置。",
                          L"Could not save update settings."),
                        L"Asterun",
                        MB_OK | MB_ICONERROR);
                }

                RefreshFromSettings();
                redrawClickedToggle();
            }
            return 0;

        case kIdUpdateAction:
            if (notify == BN_CLICKED) {
                const auto status =
                    app_.UpdateStatus();

                if (status.stage ==
                        win::UpdateStage::Available &&
                    !status.running) {
                    app_.StartUpdateDownloadAndInstall();
                } else if (!status.running) {
                    app_.StartUpdateCheck(true);
                }

                RefreshUpdateStatus();
            }
            return 0;

        default:
            break;
        }
        break;
    }

    case WM_MEASUREITEM: {
        auto* measure =
            reinterpret_cast<
                MEASUREITEMSTRUCT*>(
                    lParam);

        if (measure &&
            measure->CtlType ==
                ODT_COMBOBOX) {
            measure->itemHeight =
                ui::NextComboBoxItemHeight(
                    dpi_);
            return TRUE;
        }

        break;
    }

    case WM_DRAWITEM: {
        const auto* item =
            reinterpret_cast<
                DRAWITEMSTRUCT*>(
                    lParam);

        if (!item) {
            break;
        }

        if (item->CtlType ==
                ODT_COMBOBOX) {
            ui::DrawNextComboBoxItem(
                *item,
                dpi_);
            return TRUE;
        }

        if (item->CtlType ==
                ODT_BUTTON &&
            item->CtlID >=
                kIdHotkeyEnabledBase &&
            item->CtlID <
                kIdHotkeyEnabledBase +
                    hotkeyRows_.size()) {
            DrawHotkeyToggle(
                *item,
                static_cast<std::size_t>(
                    item->CtlID -
                    kIdHotkeyEnabledBase));
            return TRUE;
        }

        if (item->CtlID == kIdNavGeneral ||
            item->CtlID == kIdNavHotkeys ||
            item->CtlID == kIdNavProviders ||
            item->CtlID == kIdNavAppearance ||
            item->CtlID == kIdNavData ||
            item->CtlID == kIdNavAbout) {
            DrawNavigationButton(
                *item);
            return TRUE;
        }

        if (item->CtlID == kIdStartWithWindows ||
            item->CtlID == kIdSoundEnabled ||
            item->CtlID == kIdShowTrayIcon ||
            item->CtlID == kIdAddToSendToMenu ||
            item->CtlID == kIdPinyinSearch ||
            item->CtlID == kIdNumericQuickLaunch ||
            item->CtlID == kIdExecuteSingleResult ||
            item->CtlID == kIdProviderStartMenu ||
            item->CtlID == kIdProviderPackaged ||
            item->CtlID == kIdProviderAppPaths ||
            item->CtlID == kIdProviderPath ||
            item->CtlID == kIdProviderEverything ||
            item->CtlID == kIdManagedEverythingTrayIcon ||
            item->CtlID == kIdUpdateAutoCheck ||
            item->CtlID == kIdUpdatePrerelease) {
            DrawGeneralToggle(
                *item);
            return TRUE;
        }

        if (item->CtlID >=
                kIdHotkeyResetBase &&
            item->CtlID <
                kIdHotkeyResetBase +
                    hotkeyRows_.size()) {
            DrawHotkeyResetLink(
                *item);
            return TRUE;
        }

        if (item->CtlID ==
                kIdOpenGitHub) {
            DrawGitHubLink(
                *item);
            return TRUE;
        }

        if (item->hwndItem ==
                updateStatus_) {
            DrawUpdateStatus(
                *item);
            return TRUE;
        }

        if (item->CtlType == ODT_BUTTON) {
            DrawActionButton(
                *item);
            return TRUE;
        }

        break;
    }

    case WM_VSCROLL:
        // Native non-client scrollbars are intentionally disabled.
        return 0;

    case WM_MOUSEWHEEL:
        if (page_ == Page::General ||
            page_ == Page::Hotkeys) {
            const int wheel =
                GET_WHEEL_DELTA_WPARAM(
                    wParam);

            if (wheel != 0) {
                ScrollCurrentPage(
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
        GetClientRect(
            hwnd_,
            &client);

        FillRect(
            dc,
            &client,
            backgroundBrush_);

        const int sidebarX =
            Scale(
                kSidebarWidthLogical);

        RECT sidebar{
            client.left,
            client.top,
            sidebarX,
            client.bottom,
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

        MoveToEx(
            dc,
            sidebarX,
            client.top,
            nullptr);
        LineTo(
            dc,
            sidebarX,
            client.bottom);

        const int contentLeft =
            sidebarX +
            Scale(
                settings_layout::
                    kContentLeftInsetLogical);

        MoveToEx(
            dc,
            contentLeft,
            Scale(
                settings_layout::
                    kPageDividerTopLogical) -
                (page_ ==
                         Page::General
                     ? generalScrollOffset_
                     : 0),
            nullptr);
        LineTo(
            dc,
            client.right -
                Scale(
                    settings_layout::
                        kContentRightInsetLogical),
            Scale(
                settings_layout::
                    kPageDividerTopLogical) -
                (page_ ==
                         Page::General
                     ? generalScrollOffset_
                     : 0));

        const int aboutSeparatorY =
            std::max(
                Scale(360),
                static_cast<int>(
                    client.bottom) -
                    Scale(
                        ui::
                            kSettingsNavHeightLogical) -
                    Scale(34));

        MoveToEx(
            dc,
            Scale(16),
            aboutSeparatorY,
            nullptr);
        LineTo(
            dc,
            sidebarX - Scale(16),
            aboutSeparatorY);

        SelectObject(
            dc,
            oldPen);
        DeleteObject(
            separator);

        const auto drawCard =
            [&](RECT card) {
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
                    Scale(
                        ui::
                            kSettingsCardRadiusLogical),
                    Scale(
                        ui::
                            kSettingsCardRadiusLogical));

                SelectObject(
                    dc,
                    previousBrush);
                SelectObject(
                    dc,
                    previousPen);
                DeleteObject(fill);
                DeleteObject(border);
            };

        if (page_ == Page::General) {
            drawCard(
                BehaviorCardRect());
            drawCard(
                SearchBehaviorCardRect());
            drawCard(
                PlacementCardRect());
        } else if (
            page_ == Page::Hotkeys) {
            const int contentRight =
                client.right -
                Scale(
                    settings_layout::
                        kContentRightInsetLogical);
            const int cardRight =
                contentLeft +
                std::min(
                    contentRight -
                        contentLeft,
                    Scale(560));
            const RECT viewport =
                HotkeyScrollViewport();
            const int saved =
                SaveDC(dc);

            IntersectClipRect(
                dc,
                viewport.left,
                viewport.top,
                viewport.right,
                viewport.bottom);

            const int globalTop =
                Scale(140) -
                hotkeyScrollOffset_;
            const int globalHeight =
                HotkeyGroupHeight(true);
            const int launcherTop =
                globalTop +
                globalHeight +
                Scale(54);
            const int launcherHeight =
                HotkeyGroupHeight(false);

            drawCard({
                contentLeft,
                globalTop,
                cardRight,
                globalTop +
                    globalHeight,
            });

            drawCard({
                contentLeft,
                launcherTop,
                cardRight,
                launcherTop +
                    launcherHeight,
            });

            const auto drawGroupSeparators =
                [&](bool global,
                    int groupTop) {
                    int rowTop =
                        groupTop;
                    int remaining = 0;

                    for (const auto& row :
                         hotkeyRows_) {
                        const auto* action =
                            FindHotkeyAction(
                                row.actionId);

                        if (!action) {
                            continue;
                        }

                        const bool rowGlobal =
                            action->scope ==
                                HotkeyScope::Global;

                        if (rowGlobal == global) {
                            ++remaining;
                        }
                    }

                    HPEN separator =
                        CreatePen(
                            PS_SOLID,
                            1,
                            kBorder);
                    HGDIOBJ oldPen =
                        SelectObject(
                            dc,
                            separator);

                    for (const auto& row :
                         hotkeyRows_) {
                        const auto* action =
                            FindHotkeyAction(
                                row.actionId);

                        if (!action) {
                            continue;
                        }

                        const bool rowGlobal =
                            action->scope ==
                                HotkeyScope::Global;

                        if (rowGlobal != global) {
                            continue;
                        }

                        rowTop +=
                            HotkeyRowHeight(row);
                        --remaining;

                        if (remaining > 0) {
                            MoveToEx(
                                dc,
                                contentLeft +
                                    Scale(18),
                                rowTop,
                                nullptr);
                            LineTo(
                                dc,
                                cardRight -
                                    Scale(18),
                                rowTop);
                        }
                    }

                    SelectObject(
                        dc,
                        oldPen);
                    DeleteObject(
                        separator);
                };

            drawGroupSeparators(
                true,
                globalTop);
            drawGroupSeparators(
                false,
                launcherTop);

            RestoreDC(
                dc,
                saved);

            HPEN footerLine =
                CreatePen(
                    PS_SOLID,
                    1,
                    kBorder);
            HGDIOBJ oldFooterPen =
                SelectObject(
                    dc,
                    footerLine);

            MoveToEx(
                dc,
                contentLeft,
                viewport.bottom +
                    Scale(5),
                nullptr);
            LineTo(
                dc,
                cardRight,
                viewport.bottom +
                    Scale(5));

            SelectObject(
                dc,
                oldFooterPen);
            DeleteObject(
                footerLine);
        } else if (
            page_ == Page::Providers) {
            drawCard(
                ProviderCardRect());
            drawCard(
                PageCardRect(
                    394,
                    ProviderFilesHeightLogical(),
                    720));
        } else if (
            page_ == Page::Appearance) {
            drawCard(
                PageCardRect(
                    140,
                    58,
                    560));
            drawCard(
                PageCardRect(
                    264,
                    58,
                    560));
        } else if (
            page_ == Page::Data) {
            drawCard(
                PageCardRect(
                    140,
                    60,
                    560));
            drawCard(
                PageCardRect(
                    264,
                    64,
                    560));
            drawCard(
                PageCardRect(
                    392,
                    64,
                    560));
        } else if (
            page_ == Page::About) {
            drawCard(
                PageCardRect(
                    246,
                    174,
                    560));
        }

        DrawPageScrollBar(
            dc);

        EndPaint(
            hwnd_,
            &paint);

        return 0;
    }

    case WM_ERASEBKGND:
        return 1;

    case WM_CTLCOLORLISTBOX:
        return ui::ColorNextComboBoxList(
            reinterpret_cast<HDC>(
                wParam));


    case WM_CTLCOLORSTATIC: {
        HDC dc =
            reinterpret_cast<HDC>(
                wParam);

        HWND control =
            reinterpret_cast<HWND>(
                lParam);

        const bool sidebarStatic =
            control == brandName_ ||
            control == brandSubtitle_;

        const bool hotkeyCardStatic =
            std::any_of(
                hotkeyRows_.begin(),
                hotkeyRows_.end(),
                [&](const HotkeyRowControls& row) {
                    return control ==
                            row.title ||
                        control ==
                            row.status;
                });

        const bool hotkeyMutedStatic =
            std::any_of(
                hotkeyRows_.begin(),
                hotkeyRows_.end(),
                [&](const HotkeyRowControls& row) {
                    return control ==
                        row.status;
                });

        const bool cardStatic =
            control ==
                startupBehaviorLabel_ ||
            control == popupMonitorLabel_ ||
            control ==
                launcherPlacementLabel_ ||
            control ==
                settingsPlacementLabel_ ||
            control ==
                shortcutManagerPlacementLabel_ ||
            hotkeyCardStatic ||
            control == providerStatus_ ||
            control == uiStyleLabel_ ||
            control == languageLabel_ ||
            control == dataPath_ ||
            control == updateStatus_;

        const COLORREF background =
            sidebarStatic
                ? kSidebarBackground
                : cardStatic
                    ? kCardBackground
                    : kWindowBackground;

        SetBkMode(
            dc,
            OPAQUE);
        SetBkColor(
            dc,
            background);

        const bool muted =
            control == pageDescription_ ||
            control ==
                popupMonitorDescription_ ||
            control ==
                launcherPlacementDescription_ ||
            control ==
                settingsPlacementDescription_ ||
            control ==
                shortcutManagerPlacementDescription_ ||
            control == generalNote_ ||
            hotkeyMutedStatic ||
            control == providerStatus_ ||
            control == providerNote_ ||
            control == appearanceNote_ ||
            control == dataStatus_ ||
            control == dataPath_ ||
            control == aboutVersion_ ||
            control == aboutDescription_ ||
            control == updateStatus_;

        SetTextColor(
            dc,
            muted
                ? kMuted
                : kText);

        return reinterpret_cast<
            LRESULT>(
                sidebarStatic
                    ? sidebarBrush_
                    : cardStatic
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

    case WM_EXITSIZEMOVE: {
        if (!IsIconic(hwnd_) &&
            !IsZoomed(hwnd_)) {
            RECT moved{};
            if (GetWindowRect(
                    hwnd_,
                    &moved)) {
                app_.RememberSettingsPosition(
                    moved.left,
                    moved.top);
            }
        }
        return 0;
    }

    case WM_DPICHANGED: {
        dpi_ = HIWORD(wParam);
        generalScrollOffset_ = 0;
        hotkeyScrollOffset_ = 0;

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

    case WM_CLOSE: {
        // Capture the real on-screen rectangle before changing visibility.
        // WM_EXITSIZEMOVE normally persisted the latest drag already, but the
        // close snapshot also covers programmatic moves and a close that
        // follows immediately after movement.
        RECT closingRect{};
        const bool rememberPosition =
            !IsIconic(hwnd_) &&
            !IsZoomed(hwnd_) &&
            GetWindowRect(
                hwnd_,
                &closingRect);

        // Remove the window from composition before teardown through the
        // shared top-level presentation policy used by every custom window.
        window_presentation::
            HideForDestroy(
                hwnd_);

        CancelHotkeyCapture(false);
        CommitPendingProviderChanges();

        if (rememberPosition) {
            app_.RememberSettingsPosition(
                closingRect.left,
                closingRect.top);
        }

        KillTimer(
            hwnd_,
            kProviderStatusTimerId);
        KillTimer(
            hwnd_,
            kProviderCommitTimerId);
        KillTimer(
            hwnd_,
            kUpdateStatusTimerId);

        DestroyWindow(hwnd_);
        return 0;
    }

    case WM_DESTROY:
        KillTimer(
            hwnd_,
            kProviderStatusTimerId);
        KillTimer(
            hwnd_,
            kProviderCommitTimerId);
        KillTimer(
            hwnd_,
            kUpdateStatusTimerId);
        return 0;

    case WM_NCDESTROY: {
        const HWND destroyedWindow =
            hwnd_;
        const LRESULT result =
            DefWindowProcW(
                destroyedWindow,
                message,
                wParam,
                lParam);

        hwnd_ = nullptr;
        ResetWindowInstanceState();
        ReleaseWindowResources();
        return result;
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
