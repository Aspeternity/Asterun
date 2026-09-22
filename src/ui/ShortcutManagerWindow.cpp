#include "ShortcutManagerWindow.hpp"

#include "ShortcutEditorDialog.hpp"
#include "ShortcutPathConverterDialog.hpp"
#include "UiMetrics.hpp"
#include "UiTheme.hpp"
#include "UiTypography.hpp"
#include "../app/App.hpp"
#include "../core/Command.hpp"
#include "../core/ContextActions.hpp"
#include "../core/ShortcutEditorModel.hpp"
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

    if (rowHeightImageList_) {
        ImageList_Destroy(
            rowHeightImageList_);
        rowHeightImageList_ = nullptr;
    }

    if (font_) {
        DeleteObject(font_);
        font_ = nullptr;
    }

    if (semiboldFont_) {
        DeleteObject(
            semiboldFont_);
        semiboldFont_ = nullptr;
    }
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

    hwnd_ = CreateWindowExW(
        WS_EX_APPWINDOW,
        kShortcutManagerClass,
        L"",
        WS_OVERLAPPEDWINDOW |
            WS_CLIPCHILDREN,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        980,
        650,
        nullptr,
        nullptr,
        instance_,
        this);

    if (!hwnd_) {
        return false;
    }

    dpi_ = GetDpiForWindow(hwnd_);

    SetWindowPos(
        hwnd_,
        nullptr,
        0,
        0,
        Scale(980),
        Scale(650),
        SWP_NOMOVE |
            SWP_NOZORDER |
            SWP_NOACTIVATE);

    CreateControls();
    ApplyLanguage();
    Layout();
    Refresh();

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
        WS_EX_CLIENTEDGE,
        L"EDIT",
        L"",
        WS_CHILD |
            WS_VISIBLE |
            WS_TABSTOP |
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
        WS_EX_CLIENTEDGE,
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

    ListView_SetExtendedListViewStyle(
        list_,
        LVS_EX_FULLROWSELECT |
            LVS_EX_DOUBLEBUFFER);

    const auto& palette =
        ui::kApplicationPalette;

    ListView_SetBkColor(
        list_,
        palette.controlBackground);
    ListView_SetTextBkColor(
        list_,
        palette.controlBackground);
    ListView_SetTextColor(
        list_,
        palette.text);

    SendMessageW(
        filter_,
        EM_SETMARGINS,
        EC_LEFTMARGIN |
            EC_RIGHTMARGIN,
        MAKELPARAM(
            Scale(10),
            Scale(10)));

    RecreateFonts();
    RebuildRowHeightImageList();

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
        210,
        T(L"快捷词", L"Keywords"));
    addColumn(
        1,
        180,
        T(L"名称", L"Name"));
    addColumn(
        2,
        90,
        T(L"类型", L"Type"));
    addColumn(
        3,
        320,
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

    SendMessageW(
        filter_,
        EM_SETCUEBANNER,
        TRUE,
        reinterpret_cast<LPARAM>(
            T(L"搜索快捷项",
              L"Search shortcuts")));

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

    Refresh(preferredId);

    ShowWindow(hwnd_, SW_SHOW);
    ShowWindow(hwnd_, SW_RESTORE);
    SetForegroundWindow(hwnd_);
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

    if (selected < 0 &&
        !visibleIds_.empty()) {
        selected = 0;
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
        Scale(18);
    const int gap =
        Scale(10);
    const int buttonHeight =
        Scale(
            ui::
                kStandardControlHeightLogical);

    const int newButtonWidth =
        Scale(120);
    const int topY =
        margin;

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

    MoveWindow(
        filter_,
        margin,
        topY,
        filterWidth,
        buttonHeight,
        TRUE);

    MoveWindow(
        add_,
        margin +
            filterWidth +
            gap,
        topY,
        newButtonWidth,
        buttonHeight,
        TRUE);

    const int listY =
        topY +
        buttonHeight +
        Scale(12);

    const int bottomButtonY =
        client.bottom -
        margin -
        buttonHeight;

    const int listBottom =
        bottomButtonY -
        Scale(12);

    MoveWindow(
        list_,
        margin,
        listY,
        availableWidth,
        std::max(
            Scale(120),
            listBottom -
                listY),
        TRUE);

    const int pathButtonWidth =
        Scale(116);

    MoveWindow(
        pathConversion_,
        margin,
        bottomButtonY,
        pathButtonWidth,
        buttonHeight,
        TRUE);

    const int actionWidth =
        Scale(84);
    const int actionGap =
        Scale(8);

    int actionX =
        client.right -
        margin -
        actionWidth;

    MoveWindow(
        delete_,
        actionX,
        bottomButtonY,
        actionWidth,
        buttonHeight,
        TRUE);

    actionX -=
        actionWidth +
        actionGap;

    MoveWindow(
        edit_,
        actionX,
        bottomButtonY,
        actionWidth,
        buttonHeight,
        TRUE);

    actionX -=
        actionWidth +
        actionGap;

    MoveWindow(
        test_,
        actionX,
        bottomButtonY,
        actionWidth,
        buttonHeight,
        TRUE);

    UpdateColumnWidths();
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

    font_ =
        ui::CreateFontHandle(
            ui::ApplicationFontSpec(
                app_.SettingsData().language,
                ui::UiFontRole::Body),
            dpi_);

    semiboldFont_ =
        ui::CreateFontHandle(
            ui::ApplicationFontSpec(
                app_.SettingsData().language,
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
        if (HWND header =
                ListView_GetHeader(
                    list_)) {
            SendMessageW(
                header,
                WM_SETFONT,
                reinterpret_cast<WPARAM>(
                    font_),
                TRUE);
        }
    }
}

void ShortcutManagerWindow::
RebuildRowHeightImageList() {
    if (!list_) {
        return;
    }

    if (rowHeightImageList_) {
        ListView_SetImageList(
            list_,
            nullptr,
            LVSIL_SMALL);
        ImageList_Destroy(
            rowHeightImageList_);
        rowHeightImageList_ =
            nullptr;
    }

    rowHeightImageList_ =
        ImageList_Create(
            1,
            Scale(30),
            ILC_COLOR32,
            1,
            1);

    if (!rowHeightImageList_) {
        return;
    }

    HBITMAP spacer =
        CreateBitmap(
            1,
            Scale(30),
            1,
            32,
            nullptr);

    if (spacer) {
        ImageList_Add(
            rowHeightImageList_,
            spacer,
            nullptr);
        DeleteObject(spacer);
    }

    ListView_SetImageList(
        list_,
        rowHeightImageList_,
        LVSIL_SMALL);
}

void ShortcutManagerWindow::
UpdateColumnWidths() {
    if (!list_) {
        return;
    }

    RECT client{};
    GetClientRect(
        list_,
        &client);

    const int keywords =
        Scale(210);
    const int name =
        Scale(180);
    const int type =
        Scale(90);
    const int minimumTarget =
        Scale(220);

    const int target =
        std::max(
            minimumTarget,
            static_cast<int>(
                client.right) -
                keywords -
                name -
                type -
                Scale(4));

    ListView_SetColumnWidth(
        list_,
        0,
        keywords);
    ListView_SetColumnWidth(
        list_,
        1,
        name);
    ListView_SetColumnWidth(
        list_,
        2,
        type);
    ListView_SetColumnWidth(
        list_,
        3,
        target);
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

    const bool primary =
        id == kIdAdd;
    const bool danger =
        id == kIdDelete;
    const bool disabled =
        (item.itemState &
         ODS_DISABLED) != 0;
    const bool pressed =
        (item.itemState &
         ODS_SELECTED) != 0;

    COLORREF fillColor =
        palette.controlBackground;
    COLORREF borderColor =
        palette.frame;
    COLORREF textColor =
        palette.text;

    if (primary) {
        fillColor =
            disabled
                ? RGB(196, 205, 214)
                : pressed
                    ? RGB(0, 99, 177)
                    : palette.accent;
        borderColor =
            fillColor;
        textColor =
            RGB(255, 255, 255);
    } else if (danger) {
        textColor =
            disabled
                ? palette.mutedText
                : RGB(190, 45, 45);
        borderColor =
            disabled
                ? palette.frame
                : RGB(226, 185, 185);
        fillColor =
            pressed && !disabled
                ? RGB(255, 244, 244)
                : palette.controlBackground;
    } else if (pressed &&
               !disabled) {
        fillColor =
            palette.pressedBackground;
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
            primary && semiboldFont_
                ? semiboldFont_
                : font_);

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

    const auto& palette =
        ui::kApplicationPalette;

    switch (draw->nmcd.dwDrawStage) {
    case CDDS_PREPAINT:
        return CDRF_NOTIFYITEMDRAW;

    case CDDS_ITEMPREPAINT: {
        const bool selected =
            (draw->nmcd.uItemState &
             CDIS_SELECTED) != 0;

        draw->clrText =
            selected
                ? palette.selectionText
                : palette.text;
        draw->clrTextBk =
            selected
                ? palette.selectionBackground
                : palette.controlBackground;

        return CDRF_NOTIFYPOSTPAINT;
    }

    case CDDS_ITEMPOSTPAINT: {
        RECT row{};
        const int itemIndex =
            static_cast<int>(
                draw->nmcd.dwItemSpec);

        if (ListView_GetItemRect(
                list_,
                itemIndex,
                &row,
                LVIR_BOUNDS)) {
            HPEN separator =
                CreatePen(
                    PS_SOLID,
                    1,
                    palette.separator);
            HGDIOBJ oldPen =
                SelectObject(
                    draw->nmcd.hdc,
                    separator);

            MoveToEx(
                draw->nmcd.hdc,
                row.left,
                row.bottom - 1,
                nullptr);
            LineTo(
                draw->nmcd.hdc,
                row.right,
                row.bottom - 1);

            SelectObject(
                draw->nmcd.hdc,
                oldPen);
            DeleteObject(separator);
        }

        return CDRF_DODEFAULT;
    }

    default:
        return CDRF_DODEFAULT;
    }
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
            ShowWindow(
                hwnd_,
                SW_HIDE);
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
        T(L"确定删除快捷项“",
          L"Delete shortcut \"");

    message +=
        command->title.empty()
            ? command->keyword
            : command->title;

    message +=
        T(L"”吗？\n\n此操作会立即写入 commands.json。",
          L"\"?\n\nThe change will be written to commands.json immediately.");

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
            T(L"无法在资源管理器中定位此目标。目标可能已移动、删除，或不是文件系统路径。",
              L"Could not show this target in File Explorer. It may have moved, been deleted, or may not be a filesystem path."),
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
            T(L"新建快捷项...",
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
            T(L"编辑快捷项...",
              L"Edit shortcut..."));
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
                    T(L"在资源管理器中定位",
                      L"Show in File Explorer"));
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
            T(L"删除快捷项",
              L"Delete shortcut"));
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
                Scale(720);
            info->ptMinTrackSize.y =
                Scale(480);
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
        RebuildRowHeightImageList();
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
                EN_CHANGE) {
                Refresh();
            }
            return 0;
        default:
            break;
        }
        break;

    case WM_NOTIFY: {
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
