#include "ShortcutManagerWindow.hpp"

#include "ShortcutEditorDialog.hpp"
#include "ShortcutPathConverterDialog.hpp"
#include "UiMetrics.hpp"
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

    if (font_) {
        DeleteObject(font_);
        font_ = nullptr;
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
                    BS_PUSHBUTTON,
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

    makeButton(add_, kIdAdd);
    makeButton(edit_, kIdEdit);
    makeButton(delete_, kIdDelete);
    makeButton(test_, kIdTest);
    makeButton(
        pathConversion_,
        kIdPathConversion);
    makeButton(close_, kIdClose);

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

    ListView_SetExtendedListViewStyle(
        list_,
        LVS_EX_FULLROWSELECT |
            LVS_EX_GRIDLINES |
            LVS_EX_DOUBLEBUFFER);

    font_ =
        ui::CreateFontHandle(
            ui::ApplicationFontSpec(
                app_.SettingsData().language,
                ui::UiFontRole::Body),
            dpi_);

    for (HWND control :
         std::array<HWND, 8>{
             add_,
             edit_,
             delete_,
             test_,
             pathConversion_,
             close_,
             filter_,
             list_}) {
        SendMessageW(
            control,
            WM_SETFONT,
            reinterpret_cast<WPARAM>(
                font_),
            TRUE);
    }

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
        230,
        T(L"快捷词", L"Keywords"));
    addColumn(
        1,
        190,
        T(L"名称", L"Name"));
    addColumn(
        2,
        110,
        T(L"类型", L"Type"));
    addColumn(
        3,
        350,
        T(L"目标 / 命令行",
          L"Target / command"));
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
        T(L"添加", L"Add"));
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
        T(L"路径转换...", L"Path conversion..."));
    SetWindowTextW(
        close_,
        T(L"关闭", L"Close"));

    SendMessageW(
        filter_,
        EM_SETCUEBANNER,
        TRUE,
        reinterpret_cast<LPARAM>(
            T(L"筛选快捷项：快捷词、名称或目标",
              L"Filter shortcuts: keyword, name or target")));

    const std::array<const wchar_t*, 4>
        labels{
            T(L"快捷词", L"Keywords"),
            T(L"名称", L"Name"),
            T(L"类型", L"Type"),
            T(L"目标 / 命令行",
              L"Target / command"),
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
}

void ShortcutManagerWindow::Layout() {
    if (!hwnd_) {
        return;
    }

    RECT client{};
    GetClientRect(hwnd_, &client);

    const int margin = Scale(14);
    const int gap = Scale(8);
    const int buttonWidth = Scale(92);
    const int buttonHeight = Scale(34);
    int x = margin;

    for (HWND button :
         std::array<HWND, 5>{
             add_,
             edit_,
             delete_,
             test_,
             pathConversion_}) {
        MoveWindow(
            button,
            x,
            margin,
            buttonWidth,
            buttonHeight,
            TRUE);
        x +=
            buttonWidth +
            gap;
    }

    MoveWindow(
        close_,
        client.right -
            margin -
            buttonWidth,
        margin,
        buttonWidth,
        buttonHeight,
        TRUE);

    const int filterY =
        margin +
        buttonHeight +
        Scale(12);
    const int filterHeight =
        Scale(30);
    const int listY =
        filterY +
        filterHeight +
        Scale(10);

    MoveWindow(
        filter_,
        margin,
        filterY,
        std::max<int>(
            1,
            static_cast<int>(
                client.right) -
                margin * 2),
        filterHeight,
        TRUE);

    MoveWindow(
        list_,
        margin,
        listY,
        std::max<int>(
            1,
            static_cast<int>(
                client.right) -
                margin * 2),
        std::max<int>(
            1,
            static_cast<int>(
                client.bottom) -
                listY -
                margin),
        TRUE);
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
    case WM_SIZE:
        Layout();
        return 0;

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
        case kIdClose:
            ShowWindow(hwnd_, SW_HIDE);
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
