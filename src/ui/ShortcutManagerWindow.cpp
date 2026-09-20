#include "ShortcutManagerWindow.hpp"

#include "ShortcutEditorDialog.hpp"
#include "../app/App.hpp"
#include "../core/Command.hpp"

#include <commctrl.h>

#include <algorithm>
#include <array>

namespace altrun {

namespace {

constexpr wchar_t kShortcutManagerClass[] =
    L"ALTRunNext.ShortcutManager";

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
    return MulDiv(
        value,
        static_cast<int>(dpi_),
        96);
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
    makeButton(moveUp_, kIdMoveUp);
    makeButton(moveDown_, kIdMoveDown);
    makeButton(close_, kIdClose);

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

    font_ = CreateFontW(
        -MulDiv(
            10,
            static_cast<int>(dpi_),
            72),
        0,
        0,
        0,
        FW_NORMAL,
        FALSE,
        FALSE,
        FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY,
        DEFAULT_PITCH |
            FF_DONTCARE,
        app_.SettingsData().language ==
                Language::ZhCN
            ? L"Microsoft YaHei UI"
            : L"Segoe UI");

    for (HWND control :
         std::array<HWND, 8>{
             add_,
             edit_,
             delete_,
             test_,
             moveUp_,
             moveDown_,
             close_,
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
            int width) {
            LVCOLUMNW column{};
            column.mask =
                LVCF_WIDTH |
                LVCF_SUBITEM;
            column.cx = Scale(width);
            column.iSubItem = index;

            ListView_InsertColumn(
                list_,
                index,
                &column);
        };

    addColumn(0, 150);
    addColumn(1, 190);
    addColumn(2, 110);
    addColumn(3, 430);
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
        moveUp_,
        T(L"上移", L"Move up"));
    SetWindowTextW(
        moveDown_,
        T(L"下移", L"Move down"));
    SetWindowTextW(
        close_,
        T(L"关闭", L"Close"));

    const std::array<const wchar_t*, 4>
        labels{
            T(L"快捷词", L"Keyword"),
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

    std::vector<const Command*>
        commands;
    commands.reserve(
        app_.UserCommands().size());

    for (const auto& command :
         app_.UserCommands()) {
        commands.push_back(&command);
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
            command.enabled
                ? command.keyword
                : L"(" +
                    command.keyword +
                    L")";

        if (command.pinned) {
            keyword =
                L"★ " + keyword;
        }

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
    EnableWindow(moveUp_, hasSelection);
    EnableWindow(moveDown_, hasSelection);
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
         std::array<HWND, 6>{
             add_,
             edit_,
             delete_,
             test_,
             moveUp_,
             moveDown_}) {
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

    MoveWindow(
        list_,
        margin,
        margin +
            buttonHeight +
            Scale(12),
        std::max(
            1,
            client.right -
                margin * 2),
        std::max(
            1,
            client.bottom -
                margin * 2 -
                buttonHeight -
                Scale(12)),
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

void ShortcutManagerWindow::MoveSelected(
    int direction) {
    const std::wstring id =
        SelectedId();

    if (id.empty()) {
        return;
    }

    if (app_.MoveUserCommand(
            id,
            direction)) {
        Refresh(id);
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
        case kIdMoveUp:
            MoveSelected(-1);
            return 0;
        case kIdMoveDown:
            MoveSelected(1);
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
                EnableWindow(
                    moveUp_,
                    selected);
                EnableWindow(
                    moveDown_,
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
