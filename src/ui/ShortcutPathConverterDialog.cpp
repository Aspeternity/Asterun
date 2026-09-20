#include "ShortcutPathConverterDialog.hpp"

#include "../app/App.hpp"
#include "../core/Command.hpp"
#include "../core/UserCommandStore.hpp"
#include "../platform/WinUtil.hpp"

#include <commctrl.h>

#include <algorithm>
#include <array>
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
constexpr UINT kIdClose = 54106;

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
    if (hwnd_ &&
        IsWindow(hwnd_)) {
        DestroyWindow(hwnd_);
    }

    if (font_) {
        DeleteObject(font_);
        font_ = nullptr;
    }
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
    return MulDiv(
        value,
        static_cast<int>(dpi_),
        96);
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
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        1100,
        650,
        owner_,
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
        Scale(1100),
        Scale(650),
        SWP_NOMOVE |
            SWP_NOZORDER |
            SWP_NOACTIVATE);

    CreateControls();
    ApplyLanguage();
    Layout();
    Scan();

    RECT ownerRect{};
    RECT rect{};
    GetWindowRect(hwnd_, &rect);

    if (owner_ &&
        GetWindowRect(owner_, &ownerRect)) {
        const int width =
            rect.right - rect.left;
        const int height =
            rect.bottom - rect.top;

        SetWindowPos(
            hwnd_,
            nullptr,
            ownerRect.left +
                ((ownerRect.right -
                  ownerRect.left -
                  width) / 2),
            ownerRect.top +
                ((ownerRect.bottom -
                  ownerRect.top -
                  height) / 2),
            0,
            0,
            SWP_NOSIZE |
                SWP_NOZORDER |
                SWP_NOACTIVATE);
    }

    return true;
}

bool ShortcutPathConverterDialog::RunModal() {
    if (owner_) {
        EnableWindow(owner_, FALSE);
    }

    ShowWindow(hwnd_, SW_SHOW);
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

    makeButton(
        portable_,
        L"",
        kIdPortable,
        BS_AUTORADIOBUTTON |
            WS_GROUP);

    makeButton(
        absolute_,
        L"",
        kIdAbsolute,
        BS_AUTORADIOBUTTON);

    makeButton(
        rescan_,
        L"",
        kIdRescan,
        BS_PUSHBUTTON);

    list_ = CreateWindowExW(
        WS_EX_CLIENTEDGE,
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
            LVS_EX_GRIDLINES |
            LVS_EX_DOUBLEBUFFER |
            LVS_EX_CHECKBOXES);

    note_ = CreateWindowExW(
        0,
        L"STATIC",
        L"",
        WS_CHILD |
            WS_VISIBLE |
            SS_LEFT,
        0,
        0,
        0,
        0,
        hwnd_,
        nullptr,
        instance_,
        nullptr);

    makeButton(
        apply_,
        L"",
        kIdApply,
        BS_DEFPUSHBUTTON);

    makeButton(
        close_,
        L"",
        kIdClose,
        BS_PUSHBUTTON);

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
         std::array<HWND, 7>{
             portable_,
             absolute_,
             rescan_,
             list_,
             note_,
             apply_,
             close_}) {
        SendMessageW(
            control,
            WM_SETFONT,
            reinterpret_cast<WPARAM>(
                font_),
            TRUE);
    }

    SendMessageW(
        portable_,
        BM_SETCHECK,
        BST_CHECKED,
        0);

    const std::array<int, 5> widths{
        145,
        135,
        285,
        285,
        120,
    };

    const std::array<const wchar_t*, 5>
        initialLabels{
            T(L"快捷项", L"Shortcut"),
            T(L"字段", L"Field"),
            T(L"当前路径", L"Current path"),
            T(L"转换后", L"Converted"),
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

void ShortcutPathConverterDialog::ApplyLanguage() {
    SetWindowTextW(
        hwnd_,
        T(L"ALTRun Next 路径转换",
          L"ALTRun Next Path Conversion"));

    SetWindowTextW(
        portable_,
        T(L"便携化（绝对路径 → 相对路径 / 环境变量）",
          L"Portable (absolute → relative / environment variable)"));

    SetWindowTextW(
        absolute_,
        T(L"展开（相对路径 / 环境变量 → 当前机器绝对路径）",
          L"Expand (relative / environment variable → absolute)"));

    SetWindowTextW(
        rescan_,
        T(L"重新扫描", L"Rescan"));

    SetWindowTextW(
        apply_,
        T(L"应用所选", L"Apply selected"));

    SetWindowTextW(
        close_,
        T(L"关闭", L"Close"));

    const std::array<const wchar_t*, 5>
        labels{
            T(L"快捷项", L"Shortcut"),
            T(L"字段", L"Field"),
            T(L"当前路径", L"Current path"),
            T(L"转换后", L"Converted"),
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
}

void ShortcutPathConverterDialog::Layout() {
    if (!hwnd_) {
        return;
    }

    RECT client{};
    GetClientRect(hwnd_, &client);

    const int margin = Scale(18);
    const int gap = Scale(10);
    const int radioHeight = Scale(30);
    const int rescanWidth = Scale(110);
    const int buttonWidth = Scale(120);
    const int buttonHeight = Scale(34);

    int y = margin;

    MoveWindow(
        portable_,
        margin,
        y,
        Scale(390),
        radioHeight,
        TRUE);

    MoveWindow(
        absolute_,
        margin + Scale(405),
        y,
        Scale(430),
        radioHeight,
        TRUE);

    MoveWindow(
        rescan_,
        static_cast<int>(
            client.right) -
            margin -
            rescanWidth,
        y,
        rescanWidth,
        buttonHeight,
        TRUE);

    y += Scale(48);

    const int bottomArea =
        Scale(86);

    MoveWindow(
        list_,
        margin,
        y,
        std::max<int>(
            1,
            static_cast<int>(
                client.right) -
                margin * 2),
        std::max<int>(
            1,
            static_cast<int>(
                client.bottom) -
                y -
                bottomArea),
        TRUE);

    const int noteY =
        static_cast<int>(
            client.bottom) -
        Scale(72);

    MoveWindow(
        note_,
        margin,
        noteY,
        std::max<int>(
            1,
            static_cast<int>(
                client.right) -
                margin * 2 -
                buttonWidth * 2 -
                gap * 2),
        Scale(52),
        TRUE);

    MoveWindow(
        apply_,
        static_cast<int>(
            client.right) -
            margin -
            buttonWidth * 2 -
            gap,
        static_cast<int>(
            client.bottom) -
            margin -
            buttonHeight,
        buttonWidth,
        buttonHeight,
        TRUE);

    MoveWindow(
        close_,
        static_cast<int>(
            client.right) -
            margin -
            buttonWidth,
        static_cast<int>(
            client.bottom) -
            margin -
            buttonHeight,
        buttonWidth,
        buttonHeight,
        TRUE);
}

void ShortcutPathConverterDialog::Scan() {
    if (!list_) {
        return;
    }

    ListView_DeleteAllItems(list_);
    rows_.clear();

    const auto addPreview =
        [&](const Command& command,
            Field field,
            std::wstring_view value,
            bool bareRelativeIsPath) {
            if (value.empty()) {
                return;
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
                return;
            }

            Row row;
            row.commandId =
                command.id;
            row.shortcut =
                command.keyword.empty()
                    ? command.title
                    : command.keyword;
            row.field = field;
            row.current =
                std::wstring(value);
            row.converted =
                preview->converted;
            row.resolved =
                preview->resolved;
            row.exists =
                preview->exists;

            const int itemIndex =
                static_cast<int>(
                    rows_.size());

            rows_.push_back(row);

            LVITEMW item{};
            item.mask = LVIF_TEXT;
            item.iItem = itemIndex;
            item.iSubItem = 0;
            item.pszText =
                rows_.back()
                    .shortcut
                    .data();

            ListView_InsertItem(
                list_,
                &item);

            ListView_SetItemText(
                list_,
                itemIndex,
                1,
                const_cast<wchar_t*>(
                    field == Field::Target
                        ? T(L"目标",
                            L"Target")
                        : T(L"工作目录",
                            L"Working directory")));

            ListView_SetItemText(
                list_,
                itemIndex,
                2,
                rows_.back()
                    .current
                    .data());

            ListView_SetItemText(
                list_,
                itemIndex,
                3,
                rows_.back()
                    .converted
                    .data());

            ListView_SetItemText(
                list_,
                itemIndex,
                4,
                const_cast<wchar_t*>(
                    row.exists
                        ? T(L"可访问",
                            L"Accessible")
                        : T(L"路径不存在",
                            L"Missing")));

            ListView_SetCheckState(
                list_,
                itemIndex,
                row.exists ? TRUE : FALSE);
        };

    for (const auto& command :
         app_.UserCommands()) {
        if (command.type !=
            CommandType::Url) {
            addPreview(
                command,
                Field::Target,
                command.target,
                command.type ==
                    CommandType::Folder);
        }

        addPreview(
            command,
            Field::WorkingDirectory,
            command.workingDirectory,
            true);
    }

    std::wstring note =
        T(L"仅处理目标和工作目录；Arguments、URL、UNC 与裸命令保持不变。找到 ",
          L"Only Target and Working Directory are handled; Arguments, URL, UNC and bare commands stay unchanged. Found ");

    note +=
        std::to_wstring(
            rows_.size());

    note +=
        T(L" 项可预览转换。不存在的目标默认不勾选。",
          L" convertible fields. Missing targets are unchecked by default.");

    SetWindowTextW(
        note_,
        note.c_str());

    EnableWindow(
        apply_,
        rows_.empty()
            ? FALSE
            : TRUE);
}

void ShortcutPathConverterDialog::ApplySelected() {
    std::vector<UserCommandPathUpdate>
        updates;

    for (int index = 0;
         index <
            static_cast<int>(
                rows_.size());
         ++index) {
        if (!ListView_GetCheckState(
                list_,
                index)) {
            continue;
        }

        const Row& row =
            rows_[
                static_cast<
                    std::size_t>(
                        index)];

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

        if (row.field ==
            Field::Target) {
            it->target =
                row.converted;
        } else {
            it->workingDirectory =
                row.converted;
        }
    }

    if (updates.empty()) {
        MessageBoxW(
            hwnd_,
            T(L"没有选中要应用的路径转换。",
              L"No path conversions are selected."),
            T(L"路径转换",
              L"Path Conversion"),
            MB_OK |
                MB_ICONINFORMATION);
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

    MessageBoxW(
        hwnd_,
        T(L"所选路径已应用。",
          L"Selected path conversions were applied."),
        T(L"路径转换",
          L"Path Conversion"),
        MB_OK |
            MB_ICONINFORMATION);

    Scan();
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
    case WM_SIZE:
        Layout();
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case kIdPortable:
            if (HIWORD(wParam) ==
                BN_CLICKED) {
                mode_ = Mode::Portable;
                Scan();
            }
            return 0;

        case kIdAbsolute:
            if (HIWORD(wParam) ==
                BN_CLICKED) {
                mode_ = Mode::Absolute;
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

        case kIdClose:
            if (HIWORD(wParam) ==
                BN_CLICKED) {
                DestroyWindow(hwnd_);
            }
            return 0;

        default:
            break;
        }
        break;

    case WM_CLOSE:
        DestroyWindow(hwnd_);
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
