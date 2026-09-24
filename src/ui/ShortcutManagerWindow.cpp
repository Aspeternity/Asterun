#include "ShortcutManagerWindow.hpp"

#include "ShortcutEditorDialog.hpp"
#include "ShortcutPathConverterDialog.hpp"
#include "TopLevelWindowPresentation.hpp"
#include "UiListView.hpp"
#include "UiMetrics.hpp"
#include "UiTheme.hpp"
#include "UiTypography.hpp"
#include "../app/App.hpp"
#include "../core/Command.hpp"
#include "../core/ContextActions.hpp"
#include "../core/ShortcutEditorModel.hpp"
#include "../core/SettingsLayout.hpp"
#include "../platform/ShellActions.hpp"
#include "../platform/WinClipboard.hpp"

#include <commctrl.h>
#include <windowsx.h>

#include <algorithm>
#include <array>
#include <iterator>

#ifndef LVM_SETEMPTYTEXT
#define LVM_SETEMPTYTEXT (LVM_FIRST + 204)
#endif

namespace altrun {

namespace {

constexpr wchar_t kShortcutManagerClass[] =
    L"ALTRunNext.ShortcutManager";

constexpr int kDefaultWidthLogical = 720;
constexpr int kDefaultHeightLogical = 480;
constexpr int kMinimumWidthLogical = 720;
constexpr int kMinimumHeightLogical = 480;

constexpr int kKeywordColumnPercent = 16;
constexpr int kNameColumnPercent = 24;
constexpr int kTypeColumnPercent = 14;
// Match the validated first-open proportions at the Manager's minimum window
// size. Users may widen columns (with Target remaining elastic) but should not
// compress Keywords/Name/Type below the baseline that was already accepted
// visually on real Windows.
constexpr int kKeywordColumnMinimumLogical = 107;
constexpr int kNameColumnMinimumLogical = 161;
constexpr int kTypeColumnMinimumLogical = 94;
constexpr int kTargetColumnMinimumLogical = 180;

constexpr DWORD kShortcutManagerWindowExStyle =
    WS_EX_APPWINDOW;
constexpr DWORD kShortcutManagerWindowStyle =
    WS_OVERLAPPEDWINDOW |
    WS_CLIPCHILDREN;

struct ShortcutManagerCreationGeometry {
    RECT outer{};
    UINT dpi{96};
};

[[nodiscard]] UINT ProbeShortcutManagerMonitorDpi(
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

[[nodiscard]] HMONITOR
ResolveShortcutManagerMonitor(
    const Settings& settings) {

    POINT anchor{};

    if (settings.shortcutManagerPlacement ==
            "last" &&
        settings.shortcutManagerLastPositionValid) {
        anchor.x =
            settings.shortcutManagerLastX;
        anchor.y =
            settings.shortcutManagerLastY;
    } else if (!GetCursorPos(
                   &anchor)) {
        anchor = {0, 0};
    }

    return MonitorFromPoint(
        anchor,
        MONITOR_DEFAULTTONEAREST);
}

[[nodiscard]] settings_layout::Rect
ResolveShortcutManagerRect(
    const Settings& settings,
    const MONITORINFO& info,
    int width,
    int height,
    UINT dpi) {

    const settings_layout::Rect work{
        static_cast<int>(
            info.rcWork.left),
        static_cast<int>(
            info.rcWork.top),
        static_cast<int>(
            info.rcWork.right),
        static_cast<int>(
            info.rcWork.bottom),
    };

    if (settings.shortcutManagerPlacement ==
            "last" &&
        settings.shortcutManagerLastPositionValid) {
        return settings_layout::
            ClampRectToWorkArea(
                {
                    settings.shortcutManagerLastX,
                    settings.shortcutManagerLastY,
                    settings.shortcutManagerLastX +
                        width,
                    settings.shortcutManagerLastY +
                        height,
                },
                work);
    }

    const auto origin =
        settings_layout::
            ResolveWindowOrigin(
                work,
                width,
                height,
                settings.shortcutManagerPlacement ==
                    "top",
                ui::Scale(
                    45,
                    dpi));

    return {
        origin.x,
        origin.y,
        origin.x + width,
        origin.y + height,
    };
}

[[nodiscard]] ShortcutManagerCreationGeometry
ResolveShortcutManagerCreationGeometry(
    const Settings& settings,
    HINSTANCE instance) {

    ShortcutManagerCreationGeometry geometry;

    const HMONITOR monitor =
        ResolveShortcutManagerMonitor(
            settings);

    MONITORINFO info{
        sizeof(info)};

    if (!monitor ||
        !GetMonitorInfoW(
            monitor,
            &info)) {
        geometry.outer = {
            0,
            0,
            kDefaultWidthLogical,
            kDefaultHeightLogical,
        };
        return geometry;
    }

    geometry.dpi =
        ProbeShortcutManagerMonitorDpi(
            instance,
            info);

    const int workWidth =
        info.rcWork.right -
        info.rcWork.left;
    const int workHeight =
        info.rcWork.bottom -
        info.rcWork.top;

    const int width =
        std::min(
            ui::Scale(
                kDefaultWidthLogical,
                geometry.dpi),
            workWidth);
    const int height =
        std::min(
            ui::Scale(
                kDefaultHeightLogical,
                geometry.dpi),
            workHeight);

    const auto resolved =
        ResolveShortcutManagerRect(
            settings,
            info,
            width,
            height,
            geometry.dpi);

    geometry.outer = {
        resolved.left,
        resolved.top,
        resolved.right,
        resolved.bottom,
    };
    return geometry;
}

enum ShortcutContextMenuId : UINT {
    kShortcutContextAdd = 52201,
    kShortcutContextEdit = 52202,
    kShortcutContextTest = 52203,
    kShortcutContextLocate = 52204,
    kShortcutContextCopy = 52205,
    kShortcutContextDelete = 52206,
};

std::wstring TypeText(
    CommandType type,
    bool zh) {
    switch (type) {
    case CommandType::Url:
        return zh ? L"网址" : L"URL";
    case CommandType::Folder:
        return zh ? L"文件夹" : L"Folder";
    case CommandType::CommandLine:
        return zh ? L"命令" : L"Command";
    case CommandType::Application:
    default:
        return zh
            ? L"应用程序"
            : L"Application";
    }
}

[[nodiscard]] std::wstring
WindowText(HWND control) {
    const int length =
        GetWindowTextLengthW(control);
    std::wstring value(
        static_cast<std::size_t>(length + 1),
        L'\0');
    GetWindowTextW(
        control,
        value.data(),
        length + 1);
    value.resize(
        static_cast<std::size_t>(length));
    return value;
}

} // namespace

ShortcutManagerWindow::
ShortcutManagerWindow(
    App& app,
    HINSTANCE instance)
    : app_(app),
      instance_(instance) {}

ShortcutManagerWindow::
~ShortcutManagerWindow() {
    if (hwnd_ &&
        IsWindow(hwnd_)) {
        DestroyWindow(hwnd_);
    }

    ReleaseWindowResources();
}

const wchar_t*
ShortcutManagerWindow::T(
    const wchar_t* zh,
    const wchar_t* en) const {
    return app_.SettingsData().language ==
            Language::ZhCN
        ? zh
        : en;
}

int ShortcutManagerWindow::Scale(
    int value) const {
    return ui::Scale(
        value,
        dpi_);
}

void ShortcutManagerWindow::
ReleaseWindowResources() {
    if (font_) {
        DeleteObject(font_);
        font_ = nullptr;
    }

    if (semiboldFont_) {
        DeleteObject(
            semiboldFont_);
        semiboldFont_ = nullptr;
    }

    add_ = nullptr;
    edit_ = nullptr;
    delete_ = nullptr;
    test_ = nullptr;
    pathConversion_ = nullptr;
    filter_ = nullptr;
    list_ = nullptr;

    customColumnWidths_ = false;
    adjustingColumnWidths_ = false;
    columnTracking_ = false;
    trackedColumn_ = -1;
    trackedColumnWidth_ = -1;
    suppressFilterRefresh_ = false;
    visibleIds_.clear();
}

void ShortcutManagerWindow::
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

bool ShortcutManagerWindow::Create() {
    if (hwnd_ &&
        IsWindow(hwnd_)) {
        return true;
    }

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
        kShortcutManagerClass;
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

    // Resolve monitor, DPI, default size and final position before the real
    // HWND exists. Creating at CW_USEDEFAULT and moving the hidden window later
    // allowed USER32/DWM to expose a one-frame upper-left birth rectangle when
    // Shortcut Manager was opened from the tray.
    const auto creation =
        ResolveShortcutManagerCreationGeometry(
            app_.SettingsData(),
            instance_);

    hwnd_ = CreateWindowExW(
        kShortcutManagerWindowExStyle,
        kShortcutManagerClass,
        L"",
        kShortcutManagerWindowStyle,
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

    if (!hwnd_) {
        return false;
    }

    window_presentation::Configure(
        hwnd_);

    dpi_ = GetDpiForWindow(hwnd_);

    // Keep the logical default size even if USER32 settled the HWND on a
    // different per-monitor DPI than the pre-creation probe predicted.
    const int desiredWidth =
        Scale(kDefaultWidthLogical);
    const int desiredHeight =
        Scale(kDefaultHeightLogical);

    RECT createdRect{};
    GetWindowRect(
        hwnd_,
        &createdRect);

    if ((createdRect.right -
         createdRect.left) !=
            desiredWidth ||
        (createdRect.bottom -
         createdRect.top) !=
            desiredHeight) {
        SetWindowPos(
            hwnd_,
            nullptr,
            0,
            0,
            desiredWidth,
            desiredHeight,
            SWP_NOMOVE |
                SWP_NOZORDER |
                SWP_NOACTIVATE);
    }

    CreateControls();

    // A recreated Manager always starts from the validated default column
    // balance. customColumnWidths_ becomes true only after a real Header width
    // edit while this window instance is open.
    customColumnWidths_ = false;

    ApplyLanguage();
    Layout();
    Refresh();
    ApplyConfiguredPlacement();

    return true;
}

void ShortcutManagerWindow::CreateControls() {
    const auto makeButton =
        [&](HWND& target,
            UINT id) {
            target = CreateWindowExW(
                0,
                L"BUTTON",
                L"",
                WS_CHILD |
                    WS_VISIBLE |
                    WS_TABSTOP |
                    BS_OWNERDRAW,
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

    // Create controls in visual/tab order:
    // search -> new -> list -> global tool -> selected-item actions.
    filter_ = CreateWindowExW(
        0,
        L"EDIT",
        L"",
        WS_CHILD |
            WS_VISIBLE |
            WS_TABSTOP |
            WS_BORDER |
            ES_AUTOHSCROLL,
        0,
        0,
        0,
        0,
        hwnd_,
        reinterpret_cast<HMENU>(
            static_cast<UINT_PTR>(
                kIdFilter)),
        instance_,
        nullptr);

    makeButton(
        add_,
        kIdAdd);

    list_ = CreateWindowExW(
        0,
        WC_LISTVIEWW,
        L"",
        WS_CHILD |
            WS_VISIBLE |
            WS_TABSTOP |
            LVS_REPORT |
            LVS_SINGLESEL |
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

    makeButton(
        pathConversion_,
        kIdPathConversion);
    makeButton(
        test_,
        kIdTest);
    makeButton(
        edit_,
        kIdEdit);
    makeButton(
        delete_,
        kIdDelete);

    SendMessageW(
        filter_,
        EM_SETMARGINS,
        EC_LEFTMARGIN |
            EC_RIGHTMARGIN,
        MAKELPARAM(
            Scale(10),
            Scale(10)));

    RecreateFonts();

    const auto addColumn =
        [&](int index,
            int width,
            const wchar_t* text) {
            LVCOLUMNW column{};
            column.mask =
                LVCF_WIDTH |
                LVCF_SUBITEM |
                LVCF_TEXT;
            column.cx = Scale(width);
            column.iSubItem = index;
            column.pszText =
                const_cast<wchar_t*>(
                    text);

            ListView_InsertColumn(
                list_,
                index,
                &column);
        };

    addColumn(
        0,
        116,
        T(L"快捷词", L"Keywords"));
    addColumn(
        1,
        170,
        T(L"名称", L"Name"));
    addColumn(
        2,
        100,
        T(L"类型", L"Type"));
    addColumn(
        3,
        334,
        T(L"目标", L"Target"));

    for (HWND control :
         std::array<HWND, 7>{
             filter_,
             add_,
             list_,
             pathConversion_,
             test_,
             edit_,
             delete_}) {
        SetWindowSubclass(
            control,
            ChildSubclassProc,
            kChildSubclassId,
            reinterpret_cast<DWORD_PTR>(
                this));
    }
}

void ShortcutManagerWindow::ApplyLanguage() {
    if (!hwnd_) {
        return;
    }

    SetWindowTextW(
        hwnd_,
        T(L"ALTRun Next 快捷项管理",
          L"ALTRun Next Shortcut Manager"));

    SetWindowTextW(
        add_,
        T(L"新建快捷项",
          L"New shortcut"));
    SetWindowTextW(
        edit_,
        T(L"编辑", L"Edit"));
    SetWindowTextW(
        delete_,
        T(L"删除", L"Delete"));
    SetWindowTextW(
        test_,
        T(L"测试", L"Test"));
    SetWindowTextW(
        pathConversion_,
        T(L"路径转换…",
          L"Path conversion…"));

    InvalidateRect(
        filter_,
        nullptr,
        TRUE);

    const std::array<const wchar_t*, 4>
        labels{
            T(L"快捷词", L"Keywords"),
            T(L"名称", L"Name"),
            T(L"类型", L"Type"),
            T(L"目标", L"Target"),
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

    UpdateEmptyText();
    Refresh(SelectedId());
}

void ShortcutManagerWindow::Show(
    std::wstring_view preferredId) {
    if (!Create()) {
        MessageBoxW(
            nullptr,
            T(L"无法创建快捷项管理窗口。",
              L"Could not create the Shortcut Manager."),
            L"ALTRun Next",
            MB_OK | MB_ICONERROR);
        return;
    }

    const bool reopening =
        !IsWindowVisible(hwnd_);

    if (reopening) {
        ResetTransientState(
            preferredId);
    } else {
        Refresh(preferredId);
    }

    if (!IsWindowVisible(hwnd_)) {
        // Re-resolve after all hidden layout/DPI work, then expose only a
        // fully-painted final frame. This mirrors the Settings first-frame
        // compositor barrier and removes tray-open upper-left flashes.
        ApplyConfiguredPlacement();

        window_presentation::
            RevealFullyPainted(
                hwnd_,
                SW_SHOW);
    } else if (IsIconic(hwnd_)) {
        ShowWindow(
            hwnd_,
            SW_RESTORE);
    }

    SetForegroundWindow(hwnd_);
}

void ShortcutManagerWindow::
ResetTransientState(
    std::wstring_view preferredId) {
    suppressFilterRefresh_ = true;

    if (filter_) {
        SetWindowTextW(
            filter_,
            L"");
    }

    suppressFilterRefresh_ = false;

    if (list_) {
        ListView_SetItemState(
            list_,
            -1,
            0,
            LVIS_SELECTED |
                LVIS_FOCUSED);
        ListView_SetSelectionMark(
            list_,
            -1);
    }

    Refresh(preferredId);

    if (list_ &&
        preferredId.empty()) {
        ListView_SetItemState(
            list_,
            -1,
            0,
            LVIS_SELECTED |
                LVIS_FOCUSED);
        ListView_SetSelectionMark(
            list_,
            -1);

        if (ListView_GetItemCount(
                list_) > 0) {
            ListView_EnsureVisible(
                list_,
                0,
                FALSE);
        }

        EnableWindow(
            edit_,
            FALSE);
        EnableWindow(
            delete_,
            FALSE);
        EnableWindow(
            test_,
            FALSE);
    }

    if (filter_) {
        InvalidateRect(
            filter_,
            nullptr,
            TRUE);
    }
}

void ShortcutManagerWindow::Refresh(
    std::wstring_view preferredId) {
    if (!list_) {
        return;
    }

    std::wstring selectedId(
        preferredId);

    if (selectedId.empty()) {
        selectedId = SelectedId();
    }

    ListView_DeleteAllItems(list_);
    visibleIds_.clear();

    const std::wstring filterText =
        filter_
            ? WindowText(filter_)
            : std::wstring{};

    std::vector<const Command*>
        commands;
    commands.reserve(
        app_.UserCommands().size());

    for (const auto& command :
         app_.UserCommands()) {
        if (ShortcutMatchesFilter(
                command,
                filterText)) {
            commands.push_back(&command);
        }
    }

    std::stable_sort(
        commands.begin(),
        commands.end(),
        [](const Command* left,
           const Command* right) {
            if (left->sortOrder !=
                right->sortOrder) {
                return left->sortOrder <
                    right->sortOrder;
            }
            return left->keyword <
                right->keyword;
        });

    const bool zh =
        app_.SettingsData().language ==
        Language::ZhCN;

    int selected = -1;

    for (std::size_t index = 0;
         index < commands.size();
         ++index) {
        const Command& command =
            *commands[index];

        std::wstring keyword =
            FormatShortcutKeywords(
                command.keyword,
                command.aliases);

        LVITEMW item{};
        item.mask = LVIF_TEXT;
        item.iItem =
            static_cast<int>(index);
        item.iSubItem = 0;
        item.pszText =
            keyword.data();

        ListView_InsertItem(
            list_,
            &item);

        ListView_SetItemText(
            list_,
            static_cast<int>(index),
            1,
            const_cast<wchar_t*>(
                command.title.c_str()));

        auto type =
            TypeText(
                command.type,
                zh);

        ListView_SetItemText(
            list_,
            static_cast<int>(index),
            2,
            type.data());

        std::wstring target =
            command.target;

        if (!command.arguments.empty()) {
            target += L" ";
            target += command.arguments;
        }

        ListView_SetItemText(
            list_,
            static_cast<int>(index),
            3,
            target.data());

        visibleIds_.push_back(
            command.id);

        if (!selectedId.empty() &&
            command.id ==
                selectedId) {
            selected =
                static_cast<int>(
                    index);
        }
    }

    if (selected >= 0) {
        ListView_SetItemState(
            list_,
            selected,
            LVIS_SELECTED |
                LVIS_FOCUSED,
            LVIS_SELECTED |
                LVIS_FOCUSED);
        ListView_EnsureVisible(
            list_,
            selected,
            FALSE);
    }

    const BOOL hasSelection =
        selected >= 0
            ? TRUE
            : FALSE;

    EnableWindow(edit_, hasSelection);
    EnableWindow(delete_, hasSelection);
    EnableWindow(test_, hasSelection);
    EnableWindow(
        pathConversion_,
        app_.UserCommands().empty()
            ? FALSE
            : TRUE);

    UpdateEmptyText();
    InvalidateRect(
        list_,
        nullptr,
        FALSE);
}

void ShortcutManagerWindow::Layout() {
    if (!hwnd_) {
        return;
    }

    RECT client{};
    GetClientRect(
        hwnd_,
        &client);

    const int margin =
        Scale(16);
    const int gap =
        Scale(10);
    const int buttonHeight =
        Scale(
            ui::
                kStandardControlHeightLogical);

    int searchHeight =
        Scale(24);

    if (font_) {
        HDC dc =
            GetDC(hwnd_);

        if (dc) {
            HGDIOBJ oldFont =
                SelectObject(
                    dc,
                    font_);

            TEXTMETRICW metrics{};
            if (GetTextMetricsW(
                    dc,
                    &metrics)) {
                searchHeight =
                    std::min(
                        buttonHeight,
                        std::max(
                            Scale(22),
                            static_cast<int>(
                                metrics.tmHeight) +
                                Scale(6)));
            }

            SelectObject(
                dc,
                oldFont);
            ReleaseDC(
                hwnd_,
                dc);
        }
    }

    const int newButtonWidth =
        Scale(112);
    const int topY =
        margin;
    const int searchY =
        topY +
        std::max(
            0,
            (buttonHeight -
             searchHeight) / 2);

    const int availableWidth =
        std::max(
            1,
            static_cast<int>(
                client.right) -
                margin * 2);

    const int filterWidth =
        std::max(
            Scale(180),
            availableWidth -
                newButtonWidth -
                gap);

    const int listY =
        topY +
        buttonHeight +
        Scale(10);

    const int bottomButtonY =
        client.bottom -
        margin -
        buttonHeight;

    const int listBottom =
        bottomButtonY -
        Scale(10);

    const int listHeight =
        std::max(
            Scale(120),
            listBottom -
                listY);

    const int pathButtonWidth =
        Scale(108);
    const int actionWidth =
        Scale(80);
    const int actionGap =
        Scale(8);

    int deleteX =
        client.right -
        margin -
        actionWidth;
    const int editX =
        deleteX -
        actionGap -
        actionWidth;
    const int testX =
        editX -
        actionGap -
        actionWidth;

    HDWP defer =
        BeginDeferWindowPos(7);

    const auto move =
        [&](HWND control,
            int x,
            int y,
            int width,
            int height) {
            if (!defer ||
                !control) {
                return;
            }

            defer =
                DeferWindowPos(
                    defer,
                    control,
                    nullptr,
                    x,
                    y,
                    width,
                    height,
                    SWP_NOZORDER |
                        SWP_NOACTIVATE |
                        SWP_NOCOPYBITS);
        };

    move(
        filter_,
        margin,
        searchY,
        filterWidth,
        searchHeight);

    move(
        add_,
        margin +
            filterWidth +
            gap,
        topY,
        newButtonWidth,
        buttonHeight);

    move(
        list_,
        margin,
        listY,
        availableWidth,
        listHeight);

    move(
        pathConversion_,
        margin,
        bottomButtonY,
        pathButtonWidth,
        buttonHeight);

    move(
        test_,
        testX,
        bottomButtonY,
        actionWidth,
        buttonHeight);

    move(
        edit_,
        editX,
        bottomButtonY,
        actionWidth,
        buttonHeight);

    move(
        delete_,
        deleteX,
        bottomButtonY,
        actionWidth,
        buttonHeight);

    if (defer) {
        EndDeferWindowPos(defer);
    }

    UpdateColumnWidths();

    RedrawWindow(
        hwnd_,
        nullptr,
        nullptr,
        RDW_INVALIDATE |
            RDW_ERASE |
            RDW_ALLCHILDREN |
            RDW_UPDATENOW);
}

void ShortcutManagerWindow::RecreateFonts() {
    if (font_) {
        DeleteObject(
            font_);
        font_ = nullptr;
    }

    if (semiboldFont_) {
        DeleteObject(
            semiboldFont_);
        semiboldFont_ = nullptr;
    }

    const auto language =
        app_.SettingsData().language;

    font_ =
        ui::CreateFontHandle(
            ui::ApplicationFontSpec(
                language,
                ui::UiFontRole::Body),
            dpi_);

    semiboldFont_ =
        ui::CreateFontHandle(
            ui::ApplicationFontSpec(
                language,
                ui::UiFontRole::BodySemibold),
            dpi_);

    for (HWND control :
         std::array<HWND, 7>{
             filter_,
             add_,
             list_,
             pathConversion_,
             test_,
             edit_,
             delete_}) {
        if (control) {
            SendMessageW(
                control,
                WM_SETFONT,
                reinterpret_cast<WPARAM>(
                    font_),
                TRUE);
        }
    }

    if (list_) {
        ui::InitializeNextListView(
            list_,
            dpi_,
            font_,
            semiboldFont_
                ? semiboldFont_
                : font_);
    }
}

void ShortcutManagerWindow::
ApplyConfiguredPlacement() {
    if (!hwnd_) {
        return;
    }

    const auto& settings =
        app_.SettingsData();

    const HMONITOR monitor =
        ResolveShortcutManagerMonitor(
            settings);

    MONITORINFO info{
        sizeof(info)};

    if (!monitor ||
        !GetMonitorInfoW(
            monitor,
            &info)) {
        return;
    }

    RECT windowRect{};
    if (!GetWindowRect(
            hwnd_,
            &windowRect)) {
        return;
    }

    const int width =
        windowRect.right -
        windowRect.left;
    const int height =
        windowRect.bottom -
        windowRect.top;

    auto resolved =
        ResolveShortcutManagerRect(
            settings,
            info,
            width,
            height,
            dpi_);

    SetWindowPos(
        hwnd_,
        nullptr,
        resolved.left,
        resolved.top,
        resolved.right -
            resolved.left,
        resolved.bottom -
            resolved.top,
        SWP_NOZORDER |
            SWP_NOACTIVATE);

    // Moving to a different monitor can synchronously switch per-monitor DPI.
    // Re-run the same resolver once with the settled physical size; no
    // alternate placement implementation is allowed to drift from creation.
    RECT settled{};
    if (!GetWindowRect(
            hwnd_,
            &settled)) {
        return;
    }

    const HMONITOR settledMonitor =
        MonitorFromRect(
            &settled,
            MONITOR_DEFAULTTONEAREST);
    MONITORINFO settledInfo{
        sizeof(settledInfo)};

    if (!settledMonitor ||
        !GetMonitorInfoW(
            settledMonitor,
            &settledInfo)) {
        return;
    }

    resolved =
        ResolveShortcutManagerRect(
            settings,
            settledInfo,
            settled.right -
                settled.left,
            settled.bottom -
                settled.top,
            GetDpiForWindow(
                hwnd_));

    SetWindowPos(
        hwnd_,
        nullptr,
        resolved.left,
        resolved.top,
        resolved.right -
            resolved.left,
        resolved.bottom -
            resolved.top,
        SWP_NOZORDER |
            SWP_NOACTIVATE);
}

int ShortcutManagerWindow::
ClampTrackedColumnWidth(
    int column,
    int proposedWidth) const {
    if (!list_ ||
        column < 0 ||
        column >= 3) {
        return proposedWidth;
    }

    HWND header =
        ListView_GetHeader(
            list_);

    RECT client{};

    if (header) {
        GetClientRect(
            header,
            &client);
    } else {
        GetClientRect(
            list_,
            &client);
    }

    const int contentWidth =
        std::max(
            1,
            static_cast<int>(
                client.right -
                client.left));

    const std::array<int, 3>
        minimums{
            Scale(
                kKeywordColumnMinimumLogical),
            Scale(
                kNameColumnMinimumLogical),
            Scale(
                kTypeColumnMinimumLogical),
        };

    const int minimumTarget =
        Scale(
            kTargetColumnMinimumLogical);

    int otherWidth = 0;

    for (int index = 0;
         index < 3;
         ++index) {
        if (index == column) {
            continue;
        }

        otherWidth +=
            std::max(
                minimums[
                    static_cast<
                        std::size_t>(
                            index)],
                ListView_GetColumnWidth(
                    list_,
                    index));
    }

    const int maximum =
        std::max(
            minimums[
                static_cast<
                    std::size_t>(
                        column)],
            contentWidth -
                minimumTarget -
                otherWidth);

    return std::clamp(
        proposedWidth,
        minimums[
            static_cast<
                std::size_t>(
                    column)],
        maximum);
}


void ShortcutManagerWindow::
UpdateColumnWidths(
    int resizedColumn,
    int proposedWidth) {
    if (!list_ ||
        adjustingColumnWidths_) {
        return;
    }

    HWND header =
        ListView_GetHeader(
            list_);

    RECT client{};

    if (header) {
        GetClientRect(
            header,
            &client);
    } else {
        GetClientRect(
            list_,
            &client);
    }

    const int contentWidth =
        std::max(
            1,
            static_cast<int>(
                client.right -
                client.left));

    const std::array<int, 3>
        minimums{
            Scale(
                kKeywordColumnMinimumLogical),
            Scale(
                kNameColumnMinimumLogical),
            Scale(
                kTypeColumnMinimumLogical),
        };

    const int minimumTarget =
        Scale(
            kTargetColumnMinimumLogical);

    std::array<int, 3> widths{};

    if (!customColumnWidths_ &&
        resizedColumn < 0) {
        widths[0] =
            contentWidth *
            kKeywordColumnPercent / 100;
        widths[1] =
            contentWidth *
            kNameColumnPercent / 100;
        widths[2] =
            contentWidth *
            kTypeColumnPercent / 100;
    } else {
        for (int index = 0;
             index < 3;
             ++index) {
            widths[
                static_cast<
                    std::size_t>(
                        index)] =
                ListView_GetColumnWidth(
                    list_,
                    index);
        }
    }

    if (resizedColumn >= 0 &&
        resizedColumn < 3 &&
        proposedWidth >= 0) {
        widths[
            static_cast<
                std::size_t>(
                    resizedColumn)] =
            proposedWidth;
    }

    for (std::size_t index = 0;
         index < widths.size();
         ++index) {
        widths[index] =
            std::max(
                minimums[index],
                widths[index]);
    }

    const int minimumFirstThree =
        minimums[0] +
        minimums[1] +
        minimums[2];

    const int firstThreeLimit =
        std::max(
            minimumFirstThree,
            contentWidth -
                minimumTarget);

    if (resizedColumn >= 0 &&
        resizedColumn < 3) {
        const int otherWidth =
            widths[
                static_cast<
                    std::size_t>(
                        (resizedColumn +
                         1) % 3)] +
            widths[
                static_cast<
                    std::size_t>(
                        (resizedColumn +
                         2) % 3)];

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
        int firstThreeTotal =
            widths[0] +
            widths[1] +
            widths[2];

        if (firstThreeTotal >
            firstThreeLimit) {
            int excess =
                firstThreeTotal -
                firstThreeLimit;

            for (int index :
                 std::array<int, 3>{
                     1,
                     0,
                     2}) {
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
                            minimums[
                                position]);

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

    const int firstThreeTotal =
        widths[0] +
        widths[1] +
        widths[2];

    const int target =
        std::max(
            1,
            contentWidth -
                firstThreeTotal);

    adjustingColumnWidths_ =
        true;

    const int currentTarget =
        ListView_GetColumnWidth(
            list_,
            3);

    // When the dragged column grows, shrink Target first so the temporary
    // sum never exceeds the Header client width and cannot flash a horizontal
    // scrollbar. When the dragged column shrinks, grow Target after the first
    // three columns are committed.
    if (target < currentTarget) {
        ListView_SetColumnWidth(
            list_,
            3,
            target);
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

    if (target >= currentTarget &&
        currentTarget != target) {
        ListView_SetColumnWidth(
            list_,
            3,
            target);
    }

    adjustingColumnWidths_ =
        false;
}

bool ShortcutManagerWindow::
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

    HWND headerWindow =
        ListView_GetHeader(
            list_);

    if (notification->hwndFrom !=
            headerWindow &&
        notification->idFrom !=
            kIdList) {
        return false;
    }

    auto* header =
        reinterpret_cast<
            NMHEADERW*>(
                lParam);

    if (!header ||
        header->iItem < 0 ||
        header->iItem > 3) {
        return false;
    }

    const int column =
        header->iItem;

    if (column == 3) {
        if (beginTrack ||
            itemChanging ||
            track ||
            dividerDoubleClick) {
            result = TRUE;
            return true;
        }

        return false;
    }

    if (column < 0 ||
        column >= 3) {
        return false;
    }

    if (beginTrack) {
        columnTracking_ = true;
        trackedColumn_ = column;
        trackedColumnWidth_ =
            ListView_GetColumnWidth(
                list_,
                column);
        result = FALSE;
        return true;
    }

    if (track &&
        columnTracking_ &&
        trackedColumn_ == column &&
        header->pitem &&
        (header->pitem->mask &
         HDI_WIDTH) != 0) {
        const int clamped =
            ClampTrackedColumnWidth(
                column,
                header->pitem->cxy);

        header->pitem->cxy =
            clamped;
        trackedColumnWidth_ =
            clamped;

        // With HDS_FULLDRAG disabled, returning FALSE lets the Header move
        // only its tracking guide. No ListView column is resized here.
        result = FALSE;
        return true;
    }

    if (itemChanging &&
        header->pitem &&
        (header->pitem->mask &
         HDI_WIDTH) != 0) {
        const int clamped =
            ClampTrackedColumnWidth(
                column,
                header->pitem->cxy);

        header->pitem->cxy =
            clamped;

        if (columnTracking_ &&
            trackedColumn_ == column) {
            trackedColumnWidth_ =
                clamped;

            // Reject the Header's final native resize. HDN_ENDTRACK commits
            // the dragged column and elastic Target together in one step.
            result = TRUE;
            return true;
        }

        // Divider auto-size is not a drag. Apply the constrained result once
        // and reject the native one-column commit so Target remains elastic.
        customColumnWidths_ =
            true;
        UpdateColumnWidths(
            column,
            clamped);

        result = TRUE;
        return true;
    }

    if (endTrack &&
        columnTracking_ &&
        trackedColumn_ == column) {
        int finalWidth =
            trackedColumnWidth_;

        if (header->pitem &&
            (header->pitem->mask &
             HDI_WIDTH) != 0) {
            finalWidth =
                ClampTrackedColumnWidth(
                    column,
                    header->pitem->cxy);
        }

        columnTracking_ =
            false;
        trackedColumn_ = -1;
        trackedColumnWidth_ = -1;
        customColumnWidths_ =
            true;

        UpdateColumnWidths(
            column,
            finalWidth);

        RedrawWindow(
            list_,
            nullptr,
            nullptr,
            RDW_INVALIDATE |
                RDW_ERASE |
                RDW_ALLCHILDREN |
                RDW_UPDATENOW);

        result = FALSE;
        return true;
    }

    if (dividerDoubleClick) {
        // Let the native Header calculate the desired auto-size. Its
        // subsequent HDN_ITEMCHANGING is intercepted above and committed
        // together with the elastic Target.
        result = FALSE;
        return false;
    }

    if (itemChanged &&
        !columnTracking_ &&
        header->pitem &&
        (header->pitem->mask &
         HDI_WIDTH) != 0) {
        // ApplyLanguage() changes only HDI_TEXT. Treating that notification as
        // a user resize made customColumnWidths_ true before the first Layout,
        // so the intended 16/24/14/46 defaults were silently skipped.
        customColumnWidths_ =
            true;
        UpdateColumnWidths(
            column);
        result = FALSE;
        return true;
    }

    return false;
}

void ShortcutManagerWindow::
UpdateEmptyText() {
    if (!list_) {
        return;
    }

    const std::wstring filterText =
        filter_
            ? WindowText(filter_)
            : std::wstring{};

    const wchar_t* text =
        !filterText.empty()
            ? T(L"没有匹配的快捷项",
                L"No matching shortcuts")
            : T(L"还没有快捷项",
                L"No shortcuts yet");

    SendMessageW(
        list_,
        LVM_SETEMPTYTEXT,
        0,
        reinterpret_cast<LPARAM>(
            text));
}

void ShortcutManagerWindow::
DrawActionButton(
    const DRAWITEMSTRUCT& item) {
    const auto& palette =
        ui::kApplicationPalette;

    const UINT id =
        static_cast<UINT>(
            GetDlgCtrlID(
                item.hwndItem));

    const bool danger =
        id == kIdDelete;
    const bool disabled =
        (item.itemState &
         ODS_DISABLED) != 0;
    const bool pressed =
        (item.itemState &
         ODS_SELECTED) != 0;

    COLORREF fillColor =
        pressed && !disabled
            ? palette.pressedBackground
            : palette.controlBackground;
    COLORREF borderColor =
        palette.frame;
    COLORREF textColor =
        disabled
            ? palette.mutedText
            : danger
                ? RGB(190, 45, 45)
                : palette.text;

    if (danger &&
        pressed &&
        !disabled) {
        fillColor =
            RGB(255, 244, 244);
        borderColor =
            RGB(226, 185, 185);
    }

    RECT rect =
        item.rcItem;

    HBRUSH background =
        CreateSolidBrush(
            palette.windowBackground);
    FillRect(
        item.hDC,
        &rect,
        background);
    DeleteObject(
        background);

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

    wchar_t text[128]{};
    GetWindowTextW(
        item.hwndItem,
        text,
        static_cast<int>(
            std::size(text)));

    SetBkMode(
        item.hDC,
        TRANSPARENT);
    SetTextColor(
        item.hDC,
        textColor);

    HGDIOBJ oldFont =
        SelectObject(
            item.hDC,
            font_);

    RECT textRect =
        surface;
    InflateRect(
        &textRect,
        -Scale(10),
        0);

    DrawTextW(
        item.hDC,
        text,
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

LRESULT ShortcutManagerWindow::
HandleListCustomDraw(
    NMLVCUSTOMDRAW* draw) {
    if (!draw) {
        return CDRF_DODEFAULT;
    }

    if (draw->nmcd.dwDrawStage ==
        CDDS_PREPAINT) {
        return CDRF_NOTIFYITEMDRAW;
    }

    if (draw->nmcd.dwDrawStage !=
        CDDS_ITEMPREPAINT) {
        return CDRF_DODEFAULT;
    }

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
    row.right =
        client.right;

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
        draw->nmcd.hdc,
        &row,
        background);
    DeleteObject(
        background);

    SetBkMode(
        draw->nmcd.hdc,
        TRANSPARENT);
    SetTextColor(
        draw->nmcd.hdc,
        ui::NextListRowText(
            selected));

    HGDIOBJ oldFont =
        SelectObject(
            draw->nmcd.hdc,
            font_);

    const int padding =
        ui::NextListCellPadding(
            dpi_);
    int x =
        row.left;

    for (int column = 0;
         column < 4;
         ++column) {
        const int width =
            ListView_GetColumnWidth(
                list_,
                column);

        RECT cell{
            x + padding,
            row.top,
            x +
                width -
                padding,
            row.bottom,
        };

        wchar_t text[1024]{};
        ListView_GetItemText(
            list_,
            itemIndex,
            column,
            text,
            static_cast<int>(
                std::size(text)));

        DrawTextW(
            draw->nmcd.hdc,
            text,
            -1,
            &cell,
            DT_LEFT |
                DT_VCENTER |
                DT_SINGLELINE |
                DT_END_ELLIPSIS |
                DT_NOPREFIX);

        x += width;
    }

    SelectObject(
        draw->nmcd.hdc,
        oldFont);

    ui::DrawNextListRowSeparator(
        draw->nmcd.hdc,
        row);

    return CDRF_SKIPDEFAULT;
}

bool ShortcutManagerWindow::
HandleChildKeyDown(
    HWND source,
    WPARAM key) {
    (void)source;

    const bool control =
        (GetKeyState(VK_CONTROL) &
         0x8000) != 0;

    if (control &&
        (key == L'F' ||
         key == L'f')) {
        SetFocus(filter_);
        SendMessageW(
            filter_,
            EM_SETSEL,
            0,
            -1);
        return true;
    }

    if (control &&
        (key == L'N' ||
         key == L'n')) {
        AddShortcut();
        return true;
    }

    if (control &&
        key == VK_RETURN) {
        TestSelected();
        return true;
    }

    if (key == VK_ESCAPE) {
        if (!WindowText(
                filter_).empty()) {
            SetWindowTextW(
                filter_,
                L"");
            SetFocus(filter_);
        } else {
            CloseWindow();
        }
        return true;
    }

    return false;
}

std::wstring
ShortcutManagerWindow::SelectedId() const {
    if (!list_) {
        return {};
    }

    const int selected =
        ListView_GetNextItem(
            list_,
            -1,
            LVNI_SELECTED);

    if (selected < 0 ||
        selected >=
            static_cast<int>(
                visibleIds_.size())) {
        return {};
    }

    return visibleIds_[
        static_cast<std::size_t>(
            selected)];
}

const Command*
ShortcutManagerWindow::SelectedCommand()
    const {
    const std::wstring id =
        SelectedId();

    if (id.empty()) {
        return nullptr;
    }

    const auto it =
        std::find_if(
            app_.UserCommands().begin(),
            app_.UserCommands().end(),
            [&](const Command& command) {
                return command.id == id;
            });

    return it ==
            app_.UserCommands().end()
        ? nullptr
        : &*it;
}

void ShortcutManagerWindow::AddShortcut() {
    if (ShortcutEditorDialog::Show(
            app_,
            instance_,
            hwnd_)) {
        Refresh();
    }
}

void ShortcutManagerWindow::EditSelected() {
    const std::wstring id =
        SelectedId();

    if (id.empty()) {
        return;
    }

    if (ShortcutEditorDialog::Show(
            app_,
            instance_,
            hwnd_,
            id)) {
        Refresh(id);
    }
}

void ShortcutManagerWindow::DeleteSelected() {
    const Command* command =
        SelectedCommand();

    if (!command) {
        return;
    }

    std::wstring message =
        T(L"确定要删除“",
          L"Delete \"");

    message +=
        command->title.empty()
            ? command->keyword
            : command->title;

    message +=
        T(L"”吗？\n\n删除后无法撤销。",
          L"\"?\n\nThis action cannot be undone.");

    if (MessageBoxW(
            hwnd_,
            message.c_str(),
            T(L"删除快捷项",
              L"Delete shortcut"),
            MB_YESNO |
                MB_ICONWARNING) !=
        IDYES) {
        return;
    }

    const std::wstring id =
        command->id;

    if (!app_.DeleteUserCommand(id)) {
        MessageBoxW(
            hwnd_,
            T(L"删除失败。",
              L"Delete failed."),
            L"ALTRun Next",
            MB_OK |
                MB_ICONERROR);
        return;
    }

    Refresh();
}

void ShortcutManagerWindow::TestSelected() {
    const Command* command =
        SelectedCommand();

    if (command) {
        app_.TestCommand(*command);
    }
}

void ShortcutManagerWindow::ConvertPaths() {
    if (ShortcutPathConverterDialog::Show(
            app_,
            instance_,
            hwnd_)) {
        Refresh();
    }
}


void ShortcutManagerWindow::LocateSelected() {
    const Command* command =
        SelectedCommand();

    if (!command) {
        return;
    }

    if (!win::RevealInExplorer(
            command->target,
            app_.BaseDirectory(),
            command->type ==
                CommandType::Folder)) {
        MessageBoxW(
            hwnd_,
            T(L"无法打开目标所在目录。目标可能已移动、删除，或不是文件系统路径。",
              L"Could not open the target location. It may have moved, been deleted, or may not be a filesystem path."),
            L"ALTRun Next",
            MB_OK |
                MB_ICONINFORMATION);
    }
}

void ShortcutManagerWindow::CopySelectedTarget() {
    const Command* command =
        SelectedCommand();

    if (!command ||
        command->target.empty()) {
        return;
    }

    if (!win::SetClipboardUnicodeText(
            command->target)) {
        MessageBoxW(
            hwnd_,
            T(L"无法复制目标到剪贴板。",
              L"Could not copy the target to the clipboard."),
            L"ALTRun Next",
            MB_OK |
                MB_ICONERROR);
    }
}

void ShortcutManagerWindow::ShowContextMenu(
    POINT point) {
    if (!list_) {
        return;
    }

    const bool keyboardInvocation =
        point.x == -1 &&
        point.y == -1;

    int item =
        ListView_GetNextItem(
            list_,
            -1,
            LVNI_SELECTED);

    if (!keyboardInvocation) {
        POINT clientPoint = point;
        ScreenToClient(
            list_,
            &clientPoint);

        LVHITTESTINFO hit{};
        hit.pt = clientPoint;

        item =
            ListView_HitTest(
                list_,
                &hit);

        ListView_SetItemState(
            list_,
            -1,
            0,
            LVIS_SELECTED |
                LVIS_FOCUSED);

        if (item >= 0 &&
            static_cast<std::size_t>(
                item) <
                visibleIds_.size()) {
            ListView_SetItemState(
                list_,
                item,
                LVIS_SELECTED |
                    LVIS_FOCUSED,
                LVIS_SELECTED |
                    LVIS_FOCUSED);
            ListView_EnsureVisible(
                list_,
                item,
                FALSE);
        }
    } else if (item >= 0) {
        RECT row{};

        if (ListView_GetItemRect(
                list_,
                item,
                &row,
                LVIR_BOUNDS)) {
            point.x =
                row.left +
                (row.right - row.left) / 2;
            point.y =
                row.top +
                (row.bottom - row.top) / 2;
            ClientToScreen(
                list_,
                &point);
        }
    }

    const bool hasSelection =
        item >= 0 &&
        static_cast<std::size_t>(
            item) <
            visibleIds_.size();

    EnableWindow(
        edit_,
        hasSelection ? TRUE : FALSE);
    EnableWindow(
        delete_,
        hasSelection ? TRUE : FALSE);
    EnableWindow(
        test_,
        hasSelection ? TRUE : FALSE);

    if (keyboardInvocation &&
        point.x == -1 &&
        point.y == -1) {
        RECT listRect{};
        GetWindowRect(
            list_,
            &listRect);
        point.x =
            listRect.left +
            Scale(16);
        point.y =
            listRect.top +
            Scale(16);
    }

    HMENU menu =
        CreatePopupMenu();

    if (!menu) {
        return;
    }

    if (!hasSelection) {
        AppendMenuW(
            menu,
            MF_STRING,
            kShortcutContextAdd,
            T(L"新建快捷项…",
              L"New shortcut..."));

        SetMenuDefaultItem(
            menu,
            kShortcutContextAdd,
            FALSE);
    } else {
        const Command* command =
            SelectedCommand();

        if (!command) {
            DestroyMenu(menu);
            return;
        }

        AppendMenuW(
            menu,
            MF_STRING,
            kShortcutContextEdit,
            T(L"编辑…",
              L"Edit..."));
        SetMenuDefaultItem(
            menu,
            kShortcutContextEdit,
            FALSE);

        AppendMenuW(
            menu,
            MF_STRING,
            kShortcutContextTest,
            T(L"测试",
              L"Test"));

        const bool canLocate =
            CanRevealTargetInExplorer(
                command->target);

        if (canLocate ||
            !command->target.empty()) {
            AppendMenuW(
                menu,
                MF_SEPARATOR,
                0,
                nullptr);

            if (canLocate) {
                AppendMenuW(
                    menu,
                    MF_STRING,
                    kShortcutContextLocate,
                    T(L"打开所在目录",
                      L"Open containing folder"));
            }

            if (!command->target.empty()) {
                AppendMenuW(
                    menu,
                    MF_STRING,
                    kShortcutContextCopy,
                    T(L"复制目标",
                      L"Copy target"));
            }
        }

        AppendMenuW(
            menu,
            MF_SEPARATOR,
            0,
            nullptr);
        AppendMenuW(
            menu,
            MF_STRING,
            kShortcutContextDelete,
            T(L"删除",
              L"Delete"));
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

    switch (command) {
    case kShortcutContextAdd:
        AddShortcut();
        return;
    case kShortcutContextEdit:
        EditSelected();
        return;
    case kShortcutContextTest:
        TestSelected();
        return;
    case kShortcutContextLocate:
        LocateSelected();
        return;
    case kShortcutContextCopy:
        CopySelectedTarget();
        return;
    case kShortcutContextDelete:
        DeleteSelected();
        return;
    default:
        return;
    }
}


LRESULT CALLBACK
ShortcutManagerWindow::
ChildSubclassProc(
    HWND hwnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam,
    UINT_PTR subclassId,
    DWORD_PTR refData) {
    auto* self =
        reinterpret_cast<
            ShortcutManagerWindow*>(
                refData);

    if (self &&
        message == WM_KEYDOWN &&
        self->HandleChildKeyDown(
            hwnd,
            wParam)) {
        return 0;
    }

    if (self &&
        hwnd == self->list_ &&
        message == WM_NOTIFY) {
        LRESULT headerResult = 0;

        if (self->
                HandleHeaderNotification(
                    lParam,
                    headerResult)) {
            return headerResult;
        }
    }

    if (self &&
        hwnd == self->filter_) {
        if (message == WM_SETFOCUS ||
            message == WM_KILLFOCUS ||
            message == WM_SETTEXT) {
            InvalidateRect(
                hwnd,
                nullptr,
                TRUE);
        }

        if (message == WM_PAINT) {
            const LRESULT result =
                DefSubclassProc(
                    hwnd,
                    message,
                    wParam,
                    lParam);

            if (WindowText(hwnd).empty()) {
                HDC dc =
                    GetDC(hwnd);

                if (dc) {
                    RECT rect{};
                    SendMessageW(
                        hwnd,
                        EM_GETRECT,
                        0,
                        reinterpret_cast<
                            LPARAM>(&rect));

                    if (rect.right <=
                            rect.left ||
                        rect.bottom <=
                            rect.top) {
                        GetClientRect(
                            hwnd,
                            &rect);
                        rect.left +=
                            self->Scale(10);
                        rect.right -=
                            self->Scale(8);
                    }

                    SetBkMode(
                        dc,
                        TRANSPARENT);
                    SetTextColor(
                        dc,
                        ui::
                            kApplicationPalette
                                .mutedText);

                    HGDIOBJ oldFont =
                        SelectObject(
                            dc,
                            self->font_);

                    DrawTextW(
                        dc,
                        self->T(
                            L"搜索快捷项",
                            L"Search shortcuts"),
                        -1,
                        &rect,
                        DT_LEFT |
                            DT_VCENTER |
                            DT_SINGLELINE |
                            DT_NOPREFIX);

                    SelectObject(
                        dc,
                        oldFont);
                    ReleaseDC(
                        hwnd,
                        dc);
                }
            }

            return result;
        }
    }

    if (message == WM_NCDESTROY) {
        RemoveWindowSubclass(
            hwnd,
            ChildSubclassProc,
            subclassId);
    }

    return DefSubclassProc(
        hwnd,
        message,
        wParam,
        lParam);
}


LRESULT CALLBACK
ShortcutManagerWindow::WindowProc(
    HWND hwnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam) {
    ShortcutManagerWindow* self =
        nullptr;

    if (message == WM_NCCREATE) {
        const auto* create =
            reinterpret_cast<
                CREATESTRUCTW*>(
                    lParam);

        self =
            static_cast<
                ShortcutManagerWindow*>(
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
                ShortcutManagerWindow*>(
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

LRESULT ShortcutManagerWindow::HandleMessage(
    UINT message,
    WPARAM wParam,
    LPARAM lParam) {
    switch (message) {
    case WM_GETMINMAXINFO: {
        auto* info =
            reinterpret_cast<
                MINMAXINFO*>(
                    lParam);

        if (info) {
            info->ptMinTrackSize.x =
                Scale(kMinimumWidthLogical);
            info->ptMinTrackSize.y =
                Scale(kMinimumHeightLogical);
        }
        return 0;
    }

    case WM_DPICHANGED: {
        dpi_ =
            HIWORD(wParam);

        const auto* suggested =
            reinterpret_cast<
                RECT*>(lParam);

        if (suggested) {
            SetWindowPos(
                hwnd_,
                nullptr,
                suggested->left,
                suggested->top,
                suggested->right -
                    suggested->left,
                suggested->bottom -
                    suggested->top,
                SWP_NOZORDER |
                    SWP_NOACTIVATE);
        }

        RecreateFonts();
        Layout();
        InvalidateRect(
            hwnd_,
            nullptr,
            TRUE);
        return 0;
    }

    case WM_SIZE:
        Layout();
        return 0;

    case WM_EXITSIZEMOVE: {
        RECT rect{};
        if (GetWindowRect(
                hwnd_,
                &rect)) {
            app_.RememberShortcutManagerPosition(
                rect.left,
                rect.top);
        }
        return 0;
    }

    case WM_ERASEBKGND: {
        RECT rect{};
        GetClientRect(
            hwnd_,
            &rect);

        HBRUSH brush =
            CreateSolidBrush(
                ui::kApplicationPalette
                    .windowBackground);

        FillRect(
            reinterpret_cast<HDC>(
                wParam),
            &rect,
            brush);

        DeleteObject(brush);
        return TRUE;
    }

    case WM_DRAWITEM: {
        const auto* item =
            reinterpret_cast<
                DRAWITEMSTRUCT*>(
                    lParam);

        if (item &&
            (item->CtlID == kIdAdd ||
             item->CtlID == kIdEdit ||
             item->CtlID == kIdDelete ||
             item->CtlID == kIdTest ||
             item->CtlID ==
                kIdPathConversion)) {
            DrawActionButton(
                *item);
            return TRUE;
        }
        break;
    }

    case WM_CONTEXTMENU:
        if (reinterpret_cast<HWND>(
                wParam) == list_) {
            POINT point{
                GET_X_LPARAM(lParam),
                GET_Y_LPARAM(lParam),
            };
            ShowContextMenu(point);
            return 0;
        }
        break;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case kIdAdd:
            AddShortcut();
            return 0;
        case kIdEdit:
            EditSelected();
            return 0;
        case kIdDelete:
            DeleteSelected();
            return 0;
        case kIdTest:
            TestSelected();
            return 0;
        case kIdPathConversion:
            ConvertPaths();
            return 0;
        case kIdFilter:
            if (HIWORD(wParam) ==
                    EN_CHANGE &&
                !suppressFilterRefresh_) {
                InvalidateRect(
                    filter_,
                    nullptr,
                    TRUE);
                UpdateWindow(
                    filter_);
                Refresh();
            }
            return 0;
        default:
            break;
        }
        break;

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
            return ui::DrawNextListHeader(
                reinterpret_cast<
                    NMCUSTOMDRAW*>(
                        lParam),
                dpi_,
                semiboldFont_
                    ? semiboldFont_
                    : font_);
        }

        LRESULT headerResult = 0;

        if (HandleHeaderNotification(
                lParam,
                headerResult)) {
            return headerResult;
        }

        // The shared Header subclass owns all visible Header painting.
        // Swallow any remaining Header notifications here so they can never
        // fall through into the ListView's NM_CUSTOMDRAW/business handlers.
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

        if (header &&
            header->idFrom ==
                kIdList) {
            if (header->code ==
                NM_CUSTOMDRAW) {
                return HandleListCustomDraw(
                    reinterpret_cast<
                        NMLVCUSTOMDRAW*>(
                            lParam));
            }

            if (header->code ==
                NM_DBLCLK) {
                EditSelected();
                return 0;
            }

            if (header->code ==
                LVN_KEYDOWN) {
                const auto* key =
                    reinterpret_cast<
                        NMLVKEYDOWN*>(
                            lParam);

                if (key->wVKey ==
                    VK_DELETE) {
                    DeleteSelected();
                    return 0;
                }

                if (key->wVKey ==
                    VK_RETURN) {
                    EditSelected();
                    return 0;
                }
            }

            if (header->code ==
                    LVN_ITEMCHANGED ||
                header->code ==
                    NM_CLICK) {
                const BOOL selected =
                    SelectedId().empty()
                        ? FALSE
                        : TRUE;
                EnableWindow(
                    edit_,
                    selected);
                EnableWindow(
                    delete_,
                    selected);
                EnableWindow(
                    test_,
                    selected);
            }
        }
        break;
    }

    case WM_CLOSE:
        CloseWindow();
        return 0;

    case WM_DESTROY:
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
