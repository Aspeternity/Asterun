#include "ShortcutPathConverterDialog.hpp"

#include "TopLevelWindowPresentation.hpp"
#include "UiMetrics.hpp"
#include "UiTypography.hpp"

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

constexpr LPARAM kGroupHeaderItemParam =
    static_cast<LPARAM>(-1);

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
                1100,
                650);

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

    const std::array<int, 4> widths{
        155,
        350,
        350,
        130,
    };

    const std::array<const wchar_t*, 4>
        initialLabels{
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

    const std::array<const wchar_t*, 4>
        labels{
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

    switch (draw->nmcd.dwDrawStage) {
    case CDDS_PREPAINT:
        return CDRF_NOTIFYITEMDRAW;

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

        FillRect(
            draw->nmcd.hdc,
            &rect,
            GetSysColorBrush(
                COLOR_3DFACE));

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
            GetSysColor(
                COLOR_BTNTEXT));

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
                GetSysColor(
                    COLOR_3DSHADOW));

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

void ShortcutPathConverterDialog::Scan() {
    if (!list_) {
        return;
    }

    ListView_DeleteAllItems(list_);
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

        InsertGroupHeader(groupTitle);

        for (auto& row : commandRows) {
            InsertPreviewRow(
                std::move(row));
        }
    }

    std::wstring note =
        T(L"处理目标、工作目录和自定义图标；Arguments、URL、UNC 与裸命令保持不变。找到 ",
          L"Target, Working Directory and custom icon paths are handled; Arguments, URL, UNC and bare commands stay unchanged. Found ");

    note +=
        std::to_wstring(
            convertibleShortcutCount);

    note +=
        T(L" 个快捷项，共 ",
          L" shortcuts with ");

    note +=
        std::to_wstring(
            rows_.size());

    note +=
        T(L" 个可转换字段。不存在的路径默认不勾选。",
          L" convertible fields. Missing paths are unchecked by default.");

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

    const int itemCount =
        ListView_GetItemCount(list_);

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

    case WM_NOTIFY: {
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

        return 0;
    }

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
                CloseWindow();
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
