#include "ShortcutEditorDialog.hpp"

#include "../app/App.hpp"

#include <commctrl.h>
#include <commdlg.h>
#include <shlobj.h>

#include <algorithm>
#include <array>
#include <cwctype>
#include <filesystem>

namespace altrun {

namespace {

constexpr wchar_t kShortcutEditorClass[] =
    L"ALTRunNext.ShortcutEditor";

constexpr UINT kIdName = 53101;
constexpr UINT kIdKeyword = 53102;
constexpr UINT kIdAliases = 53103;
constexpr UINT kIdType = 53104;
constexpr UINT kIdTarget = 53105;
constexpr UINT kIdBrowseTarget = 53106;
constexpr UINT kIdArguments = 53107;
constexpr UINT kIdWorkdir = 53108;
constexpr UINT kIdBrowseWorkdir = 53109;
constexpr UINT kIdEnabled = 53110;
constexpr UINT kIdAdmin = 53111;
constexpr UINT kIdPinned = 53112;
constexpr UINT kIdTest = 53113;
constexpr UINT kIdSave = 53114;
constexpr UINT kIdCancel = 53115;

std::wstring TrimWide(
    std::wstring_view value) {
    std::size_t first = 0;
    std::size_t last = value.size();

    while (first < last &&
           std::iswspace(value[first])) {
        ++first;
    }
    while (last > first &&
           std::iswspace(value[last - 1])) {
        --last;
    }

    return std::wstring(
        value.substr(first, last - first));
}

std::wstring LowerWide(
    std::wstring_view value) {
    std::wstring out(value);
    std::transform(
        out.begin(),
        out.end(),
        out.begin(),
        [](wchar_t c) {
            return static_cast<wchar_t>(
                std::towlower(c));
        });
    return out;
}

int TypeIndex(CommandType type) {
    switch (type) {
    case CommandType::Url:
        return 1;
    case CommandType::Folder:
        return 2;
    case CommandType::CommandLine:
        return 3;
    case CommandType::Application:
    default:
        return 0;
    }
}

CommandType TypeFromIndex(int index) {
    switch (index) {
    case 1:
        return CommandType::Url;
    case 2:
        return CommandType::Folder;
    case 3:
        return CommandType::CommandLine;
    default:
        return CommandType::Application;
    }
}

bool IsChecked(HWND control) {
    return SendMessageW(
        control,
        BM_GETCHECK,
        0,
        0) == BST_CHECKED;
}

void SetChecked(
    HWND control,
    bool checked) {
    SendMessageW(
        control,
        BM_SETCHECK,
        checked ? BST_CHECKED : BST_UNCHECKED,
        0);
}

} // namespace

ShortcutEditorDialog::ShortcutEditorDialog(
    App& app,
    HINSTANCE instance,
    HWND owner)
    : app_(app),
      instance_(instance),
      owner_(owner) {}

ShortcutEditorDialog::~ShortcutEditorDialog() {
    if (hwnd_ &&
        IsWindow(hwnd_)) {
        DestroyWindow(hwnd_);
    }

    if (font_) {
        DeleteObject(font_);
        font_ = nullptr;
    }
}

bool ShortcutEditorDialog::Show(
    App& app,
    HINSTANCE instance,
    HWND owner,
    std::wstring_view commandId) {
    ShortcutEditorDialog dialog(
        app,
        instance,
        owner);

    if (!dialog.Create(commandId)) {
        MessageBoxW(
            owner,
            app.SettingsData().language ==
                    Language::ZhCN
                ? L"无法创建快捷项编辑窗口。"
                : L"Could not create the shortcut editor.",
            L"ALTRun Next",
            MB_OK | MB_ICONERROR);
        return false;
    }

    return dialog.RunModal();
}

const wchar_t* ShortcutEditorDialog::T(
    const wchar_t* zh,
    const wchar_t* en) const {
    return app_.SettingsData().language ==
            Language::ZhCN
        ? zh
        : en;
}

int ShortcutEditorDialog::Scale(
    int value) const {
    return MulDiv(
        value,
        static_cast<int>(dpi_),
        96);
}

bool ShortcutEditorDialog::Create(
    std::wstring_view commandId) {
    INITCOMMONCONTROLSEX controls{
        sizeof(controls),
        ICC_STANDARD_CLASSES,
    };
    InitCommonControlsEx(&controls);

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.hInstance = instance_;
    wc.lpfnWndProc = WindowProc;
    wc.lpszClassName =
        kShortcutEditorClass;
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
        kShortcutEditorClass,
        L"",
        WS_POPUP |
            WS_CAPTION |
            WS_SYSMENU |
            WS_CLIPCHILDREN,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        650,
        560,
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
        Scale(650),
        Scale(560),
        SWP_NOMOVE |
            SWP_NOZORDER |
            SWP_NOACTIVATE);

    CreateControls();
    ApplyLanguage();

    if (commandId.empty()) {
        BeginNew();
    } else {
        LoadCommand(commandId);
    }

    Layout();

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

bool ShortcutEditorDialog::RunModal() {
    if (owner_) {
        EnableWindow(owner_, FALSE);
    }

    ShowWindow(hwnd_, SW_SHOW);
    SetForegroundWindow(hwnd_);
    SetFocus(keyword_);

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

void ShortcutEditorDialog::CreateControls() {
    const auto makeStatic =
        [&](HWND& target,
            const wchar_t* text) {
            target = CreateWindowExW(
                0,
                L"STATIC",
                text,
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
        };

    const auto makeEdit =
        [&](HWND& target,
            UINT id) {
            target = CreateWindowExW(
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
                        id)),
                instance_,
                nullptr);
        };

    const auto makeButton =
        [&](HWND& target,
            const wchar_t* text,
            UINT id,
            DWORD style =
                BS_PUSHBUTTON) {
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

    makeStatic(nameLabel_, L"");
    makeEdit(name_, kIdName);

    makeStatic(keywordLabel_, L"");
    makeEdit(keyword_, kIdKeyword);

    makeStatic(aliasesLabel_, L"");
    makeEdit(aliases_, kIdAliases);

    makeStatic(typeLabel_, L"");
    type_ = CreateWindowExW(
        0,
        L"COMBOBOX",
        L"",
        WS_CHILD |
            WS_VISIBLE |
            WS_TABSTOP |
            CBS_DROPDOWNLIST |
            WS_VSCROLL,
        0,
        0,
        0,
        0,
        hwnd_,
        reinterpret_cast<HMENU>(
            static_cast<UINT_PTR>(
                kIdType)),
        instance_,
        nullptr);

    makeStatic(targetLabel_, L"");
    makeEdit(target_, kIdTarget);
    makeButton(
        browseTarget_,
        L"...",
        kIdBrowseTarget);

    makeStatic(argumentsLabel_, L"");
    makeEdit(arguments_, kIdArguments);

    makeStatic(workdirLabel_, L"");
    makeEdit(workdir_, kIdWorkdir);
    makeButton(
        browseWorkdir_,
        L"...",
        kIdBrowseWorkdir);

    makeButton(
        enabled_,
        L"",
        kIdEnabled,
        BS_AUTOCHECKBOX);
    makeButton(
        admin_,
        L"",
        kIdAdmin,
        BS_AUTOCHECKBOX);
    makeButton(
        pinned_,
        L"",
        kIdPinned,
        BS_AUTOCHECKBOX);

    makeButton(
        test_,
        L"",
        kIdTest);
    makeButton(
        save_,
        L"",
        kIdSave,
        BS_DEFPUSHBUTTON);
    makeButton(
        cancel_,
        L"",
        kIdCancel);

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
         std::array<HWND, 21>{
             nameLabel_,
             name_,
             keywordLabel_,
             keyword_,
             aliasesLabel_,
             aliases_,
             typeLabel_,
             type_,
             targetLabel_,
             target_,
             browseTarget_,
             argumentsLabel_,
             arguments_,
             workdirLabel_,
             workdir_,
             browseWorkdir_,
             enabled_,
             admin_,
             pinned_,
             test_,
             save_}) {
        SendMessageW(
            control,
            WM_SETFONT,
            reinterpret_cast<WPARAM>(
                font_),
            TRUE);
    }

    SendMessageW(
        cancel_,
        WM_SETFONT,
        reinterpret_cast<WPARAM>(
            font_),
        TRUE);
}

void ShortcutEditorDialog::ApplyLanguage() {
    SetWindowTextW(
        hwnd_,
        commandId_.empty()
            ? T(L"新建快捷项",
                L"New shortcut")
            : T(L"编辑快捷项",
                L"Edit shortcut"));

    SetWindowTextW(
        nameLabel_,
        T(L"名称 *", L"Name *"));
    SetWindowTextW(
        keywordLabel_,
        T(L"主快捷词 *",
          L"Primary keyword *"));
    SetWindowTextW(
        aliasesLabel_,
        T(L"别名（逗号分隔）",
          L"Aliases (comma separated)"));
    SetWindowTextW(
        typeLabel_,
        T(L"类型", L"Type"));
    SetWindowTextW(
        targetLabel_,
        T(L"目标 *", L"Target *"));
    SetWindowTextW(
        argumentsLabel_,
        T(L"参数", L"Arguments"));
    SetWindowTextW(
        workdirLabel_,
        T(L"工作目录",
          L"Working directory"));

    SetWindowTextW(
        enabled_,
        T(L"启用", L"Enabled"));
    SetWindowTextW(
        admin_,
        T(L"以管理员身份运行",
          L"Run as administrator"));
    SetWindowTextW(
        pinned_,
        T(L"置顶", L"Pinned"));
    SetWindowTextW(
        test_,
        T(L"测试", L"Test"));
    SetWindowTextW(
        save_,
        T(L"保存", L"Save"));
    SetWindowTextW(
        cancel_,
        T(L"取消", L"Cancel"));

    const int selected =
        std::max(
            0,
            static_cast<int>(
                SendMessageW(
                    type_,
                    CB_GETCURSEL,
                    0,
                    0)));

    SendMessageW(
        type_,
        CB_RESETCONTENT,
        0,
        0);

    for (const auto* text :
         std::array<const wchar_t*, 4>{
             T(L"应用程序",
               L"Application"),
             T(L"网址", L"URL"),
             T(L"文件夹", L"Folder"),
             T(L"命令", L"Command")}) {
        SendMessageW(
            type_,
            CB_ADDSTRING,
            0,
            reinterpret_cast<LPARAM>(
                text));
    }

    SendMessageW(
        type_,
        CB_SETCURSEL,
        selected,
        0);
}

void ShortcutEditorDialog::Layout() {
    RECT client{};
    GetClientRect(hwnd_, &client);

    const int margin = Scale(24);
    const int labelWidth = Scale(120);
    const int browseWidth = Scale(44);
    const int gap = Scale(8);
    const int rowHeight = Scale(42);
    const int fieldX =
        margin + labelWidth;
    const int fieldWidth =
        client.right -
        fieldX -
        margin;

    int y = Scale(24);

    const auto placeField =
        [&](HWND label,
            HWND field,
            HWND browse = nullptr) {
            MoveWindow(
                label,
                margin,
                y + Scale(6),
                labelWidth - gap,
                Scale(26),
                TRUE);

            const int width =
                browse
                    ? fieldWidth -
                        browseWidth -
                        gap
                    : fieldWidth;

            MoveWindow(
                field,
                fieldX,
                y,
                width,
                Scale(32),
                TRUE);

            if (browse) {
                MoveWindow(
                    browse,
                    fieldX +
                        width +
                        gap,
                    y,
                    browseWidth,
                    Scale(32),
                    TRUE);
            }

            y += rowHeight;
        };

    placeField(nameLabel_, name_);
    placeField(keywordLabel_, keyword_);
    placeField(aliasesLabel_, aliases_);
    placeField(typeLabel_, type_);
    placeField(
        targetLabel_,
        target_,
        browseTarget_);
    placeField(
        argumentsLabel_,
        arguments_);
    placeField(
        workdirLabel_,
        workdir_,
        browseWorkdir_);

    const int optionWidth =
        (fieldWidth - gap * 2) / 3;

    MoveWindow(
        enabled_,
        fieldX,
        y,
        optionWidth,
        Scale(30),
        TRUE);
    MoveWindow(
        admin_,
        fieldX +
            optionWidth +
            gap,
        y,
        optionWidth,
        Scale(30),
        TRUE);
    MoveWindow(
        pinned_,
        fieldX +
            (optionWidth + gap) * 2,
        y,
        optionWidth,
        Scale(30),
        TRUE);

    y += Scale(52);

    const int buttonWidth = Scale(96);

    MoveWindow(
        test_,
        margin,
        y,
        buttonWidth,
        Scale(34),
        TRUE);
    MoveWindow(
        cancel_,
        client.right -
            margin -
            buttonWidth,
        y,
        buttonWidth,
        Scale(34),
        TRUE);
    MoveWindow(
        save_,
        client.right -
            margin -
            buttonWidth * 2 -
            gap,
        y,
        buttonWidth,
        Scale(34),
        TRUE);
}

void ShortcutEditorDialog::LoadCommand(
    std::wstring_view commandId) {
    const auto it =
        std::find_if(
            app_.UserCommands().begin(),
            app_.UserCommands().end(),
            [&](const Command& command) {
                return command.id ==
                    commandId;
            });

    if (it ==
        app_.UserCommands().end()) {
        BeginNew();
        return;
    }

    commandId_ = it->id;

    SetWindowTextW(
        name_,
        it->title.c_str());
    SetWindowTextW(
        keyword_,
        it->keyword.c_str());

    std::wstring aliasText;
    for (std::size_t index = 0;
         index < it->aliases.size();
         ++index) {
        if (index > 0) {
            aliasText += L", ";
        }
        aliasText += it->aliases[index];
    }

    SetWindowTextW(
        aliases_,
        aliasText.c_str());

    SendMessageW(
        type_,
        CB_SETCURSEL,
        TypeIndex(it->type),
        0);

    SetWindowTextW(
        target_,
        it->target.c_str());
    SetWindowTextW(
        arguments_,
        it->arguments.c_str());
    SetWindowTextW(
        workdir_,
        it->workingDirectory.c_str());

    SetChecked(
        enabled_,
        it->enabled);
    SetChecked(
        admin_,
        it->runAsAdmin);
    SetChecked(
        pinned_,
        it->pinned);

    ApplyLanguage();
}

void ShortcutEditorDialog::BeginNew() {
    commandId_.clear();

    SetWindowTextW(name_, L"");
    SetWindowTextW(keyword_, L"");
    SetWindowTextW(aliases_, L"");
    SetWindowTextW(target_, L"");
    SetWindowTextW(arguments_, L"");
    SetWindowTextW(workdir_, L"");

    SendMessageW(
        type_,
        CB_SETCURSEL,
        0,
        0);

    SetChecked(enabled_, true);
    SetChecked(admin_, false);
    SetChecked(pinned_, false);

    ApplyLanguage();
}

std::wstring ShortcutEditorDialog::ControlText(
    HWND control) const {
    const int length =
        GetWindowTextLengthW(control);

    std::wstring value(
        static_cast<std::size_t>(
            length + 1),
        L'\0');

    GetWindowTextW(
        control,
        value.data(),
        length + 1);

    value.resize(
        static_cast<std::size_t>(
            length));

    return value;
}

std::vector<std::wstring>
ShortcutEditorDialog::ParseAliases(
    std::wstring_view text) const {
    std::vector<std::wstring> aliases;
    std::wstring current;

    const auto flush = [&]() {
        const auto value =
            TrimWide(current);
        current.clear();

        if (value.empty()) {
            return;
        }

        const auto duplicate =
            std::find_if(
                aliases.begin(),
                aliases.end(),
                [&](const auto& existing) {
                    return LowerWide(existing) ==
                        LowerWide(value);
                });

        if (duplicate ==
            aliases.end()) {
            aliases.push_back(value);
        }
    };

    for (const wchar_t c : text) {
        if (c == L',' ||
            c == L';' ||
            c == L'，' ||
            c == L'；') {
            flush();
        } else {
            current.push_back(c);
        }
    }

    flush();
    return aliases;
}

Command ShortcutEditorDialog::CollectCommand()
    const {
    Command command;

    command.title =
        TrimWide(
            ControlText(name_));
    command.keyword =
        TrimWide(
            ControlText(keyword_));
    command.aliases =
        ParseAliases(
            ControlText(aliases_));
    command.type =
        TypeFromIndex(
            static_cast<int>(
                SendMessageW(
                    type_,
                    CB_GETCURSEL,
                    0,
                    0)));
    command.target =
        TrimWide(
            ControlText(target_));
    command.arguments =
        TrimWide(
            ControlText(arguments_));
    command.workingDirectory =
        TrimWide(
            ControlText(workdir_));
    command.icon = L"auto";
    command.enabled =
        IsChecked(enabled_);
    command.runAsAdmin =
        IsChecked(admin_);
    command.pinned =
        IsChecked(pinned_);
    command.source =
        CommandSource::User;
    command.basePriority = 120;

    return command;
}

bool ShortcutEditorDialog::Save() {
    Command command =
        CollectCommand();

    if (command.title.empty() ||
        command.keyword.empty() ||
        command.target.empty()) {
        MessageBoxW(
            hwnd_,
            T(L"名称、主快捷词和目标为必填项。",
              L"Name, primary keyword and target are required."),
            T(L"无法保存快捷项",
              L"Cannot save shortcut"),
            MB_OK |
                MB_ICONWARNING);
        return false;
    }

    const std::wstring keyword =
        LowerWide(command.keyword);

    const bool duplicate =
        std::any_of(
            app_.UserCommands().begin(),
            app_.UserCommands().end(),
            [&](const Command& existing) {
                return
                    existing.id !=
                        commandId_ &&
                    LowerWide(
                        existing.keyword) ==
                        keyword;
            });

    if (duplicate) {
        const int answer =
            MessageBoxW(
                hwnd_,
                T(L"这个主快捷词已被另一个快捷项使用。\n\n仍然保存吗？",
                  L"This primary keyword is already used by another shortcut.\n\nSave anyway?"),
                T(L"快捷词冲突",
                  L"Keyword conflict"),
                MB_YESNO |
                    MB_ICONWARNING);

        if (answer != IDYES) {
            return false;
        }
    }

    bool saved = false;

    if (commandId_.empty()) {
        std::wstring createdId;
        saved =
            app_.CreateUserCommand(
                std::move(command),
                &createdId);

        if (saved) {
            commandId_ =
                std::move(createdId);
        }
    } else {
        saved =
            app_.UpdateUserCommand(
                commandId_,
                std::move(command));
    }

    if (!saved) {
        MessageBoxW(
            hwnd_,
            T(L"写入 commands.json 失败，原数据未被替换。",
              L"Failed to write commands.json. Existing data was not replaced."),
            T(L"保存失败",
              L"Save failed"),
            MB_OK |
                MB_ICONERROR);
        return false;
    }

    changed_ = true;
    DestroyWindow(hwnd_);
    return true;
}

void ShortcutEditorDialog::Test() {
    const Command command =
        CollectCommand();

    if (command.target.empty()) {
        MessageBoxW(
            hwnd_,
            T(L"请先填写目标。",
              L"Enter a target first."),
            T(L"测试运行", L"Test"),
            MB_OK |
                MB_ICONWARNING);
        return;
    }

    app_.TestCommand(command);
}

void ShortcutEditorDialog::BrowseTarget() {
    const auto selectedType =
        TypeFromIndex(
            static_cast<int>(
                SendMessageW(
                    type_,
                    CB_GETCURSEL,
                    0,
                    0)));

    if (selectedType ==
        CommandType::Folder) {
        BROWSEINFOW browse{};
        browse.hwndOwner = hwnd_;
        browse.lpszTitle =
            T(L"选择目标文件夹",
              L"Choose target folder");
        browse.ulFlags =
            BIF_RETURNONLYFSDIRS |
            BIF_NEWDIALOGSTYLE |
            BIF_EDITBOX;

        PIDLIST_ABSOLUTE item =
            SHBrowseForFolderW(&browse);

        if (!item) {
            return;
        }

        std::array<wchar_t, 32768>
            path{};

        if (SHGetPathFromIDListW(
                item,
                path.data())) {
            SetWindowTextW(
                target_,
                path.data());
        }

        CoTaskMemFree(item);
        return;
    }

    std::array<wchar_t, 32768> file{};
    const auto current =
        ControlText(target_);

    if (!current.empty() &&
        current.size() <
            file.size()) {
        std::copy(
            current.begin(),
            current.end(),
            file.begin());
    }

    const wchar_t filter[] =
        L"Programs and shortcuts\0*.exe;*.lnk;*.bat;*.cmd;*.com;*.url\0"
        L"All files\0*.*\0\0";

    OPENFILENAMEW open{};
    open.lStructSize =
        sizeof(open);
    open.hwndOwner = hwnd_;
    open.lpstrFile =
        file.data();
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

    const std::filesystem::path path(
        file.data());

    SetWindowTextW(
        target_,
        path.wstring().c_str());

    if (TrimWide(
            ControlText(name_))
            .empty()) {
        SetWindowTextW(
            name_,
            path.stem()
                .wstring()
                .c_str());
    }

    if (TrimWide(
            ControlText(workdir_))
            .empty()) {
        SetWindowTextW(
            workdir_,
            path.parent_path()
                .wstring()
                .c_str());
    }
}

void ShortcutEditorDialog::
BrowseWorkingDirectory() {
    BROWSEINFOW browse{};
    browse.hwndOwner = hwnd_;
    browse.lpszTitle =
        T(L"选择工作目录",
          L"Choose working directory");
    browse.ulFlags =
        BIF_RETURNONLYFSDIRS |
        BIF_NEWDIALOGSTYLE |
        BIF_EDITBOX;

    PIDLIST_ABSOLUTE item =
        SHBrowseForFolderW(&browse);

    if (!item) {
        return;
    }

    std::array<wchar_t, 32768>
        path{};

    if (SHGetPathFromIDListW(
            item,
            path.data())) {
        SetWindowTextW(
            workdir_,
            path.data());
    }

    CoTaskMemFree(item);
}

LRESULT CALLBACK
ShortcutEditorDialog::WindowProc(
    HWND hwnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam) {
    ShortcutEditorDialog* self =
        nullptr;

    if (message == WM_NCCREATE) {
        const auto* create =
            reinterpret_cast<
                CREATESTRUCTW*>(
                    lParam);

        self =
            static_cast<
                ShortcutEditorDialog*>(
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
                ShortcutEditorDialog*>(
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

LRESULT ShortcutEditorDialog::HandleMessage(
    UINT message,
    WPARAM wParam,
    LPARAM lParam) {
    switch (message) {
    case WM_SIZE:
        Layout();
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case kIdBrowseTarget:
            if (HIWORD(wParam) ==
                BN_CLICKED) {
                BrowseTarget();
            }
            return 0;

        case kIdBrowseWorkdir:
            if (HIWORD(wParam) ==
                BN_CLICKED) {
                BrowseWorkingDirectory();
            }
            return 0;

        case kIdTest:
            if (HIWORD(wParam) ==
                BN_CLICKED) {
                Test();
            }
            return 0;

        case kIdSave:
            if (HIWORD(wParam) ==
                BN_CLICKED) {
                Save();
            }
            return 0;

        case kIdCancel:
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
