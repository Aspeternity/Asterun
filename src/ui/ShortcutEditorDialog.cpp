#include "ShortcutEditorDialog.hpp"

#include "UiMetrics.hpp"
#include "UiTypography.hpp"

#include "../app/App.hpp"
#include "../core/ShortcutEditorModel.hpp"
#include "../core/RuntimeInput.hpp"

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

constexpr int kEditorWidthLogical = 660;
constexpr int kCollapsedHeightLogical = 470;
constexpr int kExpandedHeightLogical = 700;
constexpr int kRuntimeTestExtraHeightLogical = 44;
constexpr int kTypeDropdownHeightLogical = 170;
constexpr int kRuntimeInputDropdownHeightLogical = 120;

constexpr UINT kIdName = 53101;
constexpr UINT kIdKeyword = 53102;
constexpr UINT kIdType = 53104;
constexpr UINT kIdTarget = 53105;
constexpr UINT kIdBrowseFile = 53106;
constexpr UINT kIdArguments = 53107;
constexpr UINT kIdWorkdir = 53108;
constexpr UINT kIdBrowseWorkdir = 53109;
constexpr UINT kIdAdmin = 53111;
constexpr UINT kIdTest = 53113;
constexpr UINT kIdSave = 53114;
constexpr UINT kIdCancel = 53115;
constexpr UINT kIdBrowseFolder = 53116;
constexpr UINT kIdAdvancedToggle = 53117;
constexpr UINT kIdRuntimeInput = 53118;
constexpr UINT kIdIcon = 53119;
constexpr UINT kIdBrowseIcon = 53120;
constexpr UINT kIdResetIcon = 53121;
constexpr UINT kIdTestInput = 53122;

[[nodiscard]] std::wstring
TrimWide(std::wstring_view value) {
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

[[nodiscard]] int
ExplicitTypeIndex(CommandType type) {
    switch (type) {
    case CommandType::Application:
        return 1;
    case CommandType::Url:
        return 2;
    case CommandType::Folder:
        return 3;
    case CommandType::CommandLine:
        return 4;
    }

    return 1;
}

[[nodiscard]] CommandType
ExplicitTypeFromIndex(int index) {
    switch (index) {
    case 2:
        return CommandType::Url;
    case 3:
        return CommandType::Folder;
    case 4:
        return CommandType::CommandLine;
    case 1:
    default:
        return CommandType::Application;
    }
}

[[nodiscard]] bool
IsChecked(HWND control) {
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
        checked
            ? BST_CHECKED
            : BST_UNCHECKED,
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

    if (!dialog.Create(
            commandId,
            nullptr)) {
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

bool ShortcutEditorDialog::ShowNew(
    App& app,
    HINSTANCE instance,
    HWND owner,
    const Command& seed) {
    ShortcutEditorDialog dialog(
        app,
        instance,
        owner);

    if (!dialog.Create(
            {},
            &seed)) {
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
    return ui::Scale(
        value,
        dpi_);
}

bool ShortcutEditorDialog::Create(
    std::wstring_view commandId,
    const Command* seed) {
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
        kEditorWidthLogical,
        kCollapsedHeightLogical,
        owner_,
        nullptr,
        instance_,
        this);

    if (!hwnd_) {
        return false;
    }

    dpi_ = GetDpiForWindow(hwnd_);

    CreateControls();
    ApplyLanguage();

    if (commandId.empty()) {
        BeginNew(seed);
    } else {
        LoadCommand(commandId);
    }

    UpdateAdvancedVisibility();
    ResizeForContent();
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
        [&](HWND& control) {
            control = CreateWindowExW(
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
        };

    const auto makeEdit =
        [&](HWND& control,
            UINT id) {
            control = CreateWindowExW(
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
        [&](HWND& control,
            UINT id,
            DWORD style =
                BS_PUSHBUTTON) {
            control = CreateWindowExW(
                0,
                L"BUTTON",
                L"",
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

    makeStatic(keywordLabel_);
    makeEdit(keyword_, kIdKeyword);
    makeStatic(keywordHint_);

    makeStatic(nameLabel_);
    makeEdit(name_, kIdName);

    makeStatic(targetLabel_);
    makeEdit(target_, kIdTarget);
    makeButton(
        browseFile_,
        kIdBrowseFile);
    makeButton(
        browseFolder_,
        kIdBrowseFolder);

    makeStatic(typeLabel_);
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
    makeStatic(typeHint_);

    makeStatic(runtimeInputLabel_);
    runtimeInput_ = CreateWindowExW(
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
                kIdRuntimeInput)),
        instance_,
        nullptr);
    makeStatic(runtimeInputHint_);

    makeStatic(testInputLabel_);
    makeEdit(
        testInput_,
        kIdTestInput);

    makeButton(
        advancedToggle_,
        kIdAdvancedToggle);

    makeStatic(argumentsLabel_);
    makeEdit(
        arguments_,
        kIdArguments);

    makeStatic(workdirLabel_);
    makeEdit(
        workdir_,
        kIdWorkdir);
    makeButton(
        browseWorkdir_,
        kIdBrowseWorkdir);

    makeStatic(iconLabel_);
    makeEdit(
        icon_,
        kIdIcon);
    makeButton(
        browseIcon_,
        kIdBrowseIcon);
    makeButton(
        resetIcon_,
        kIdResetIcon);

    makeButton(
        admin_,
        kIdAdmin,
        BS_AUTOCHECKBOX);

    makeButton(
        test_,
        kIdTest);
    makeButton(
        save_,
        kIdSave,
        BS_DEFPUSHBUTTON);
    makeButton(
        cancel_,
        kIdCancel);

    font_ =
        ui::CreateFontHandle(
            ui::ApplicationFontSpec(
                app_.SettingsData().language,
                ui::UiFontRole::Body),
            dpi_);

    const HWND allControls[] = {
        keywordLabel_,
        keyword_,
        keywordHint_,
        nameLabel_,
        name_,
        targetLabel_,
        target_,
        browseFile_,
        browseFolder_,
        typeLabel_,
        type_,
        typeHint_,
        runtimeInputLabel_,
        runtimeInput_,
        runtimeInputHint_,
        testInputLabel_,
        testInput_,
        advancedToggle_,
        argumentsLabel_,
        arguments_,
        workdirLabel_,
        workdir_,
        browseWorkdir_,
        iconLabel_,
        icon_,
        browseIcon_,
        resetIcon_,
        admin_,
        test_,
        save_,
        cancel_,
    };

    for (HWND control : allControls) {
        SendMessageW(
            control,
            WM_SETFONT,
            reinterpret_cast<WPARAM>(
                font_),
            TRUE);
    }
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
        keywordLabel_,
        T(L"快捷词 *",
          L"Keywords *"));
    SetWindowTextW(
        keywordHint_,
        T(L"多个快捷词用逗号分隔，第一个优先级最高。",
          L"Separate multiple keywords with commas; the first has priority."));
    SetWindowTextW(
        nameLabel_,
        T(L"名称",
          L"Name"));
    SetWindowTextW(
        targetLabel_,
        T(L"目标 *",
          L"Target *"));
    SetWindowTextW(
        browseFile_,
        T(L"文件...",
          L"File..."));
    SetWindowTextW(
        browseFolder_,
        T(L"文件夹...",
          L"Folder..."));
    SetWindowTextW(
        typeLabel_,
        T(L"目标类型",
          L"Target type"));

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
         std::array<const wchar_t*, 5>{
             T(L"自动识别",
               L"Auto detect"),
             T(L"应用程序",
               L"Application"),
             T(L"网址",
               L"URL"),
             T(L"文件夹",
               L"Folder"),
             T(L"命令行",
               L"Command line")}) {
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
    SendMessageW(
        type_,
        CB_SETMINVISIBLE,
        5,
        0);

    SetWindowTextW(
        runtimeInputLabel_,
        T(L"运行时输入",
          L"Runtime input"));

    const int runtimeSelected =
        std::max(
            0,
            static_cast<int>(
                SendMessageW(
                    runtimeInput_,
                    CB_GETCURSEL,
                    0,
                    0)));

    SendMessageW(
        runtimeInput_,
        CB_RESETCONTENT,
        0,
        0);

    for (const auto* text :
         std::array<const wchar_t*, 3>{
             T(L"不接受额外输入",
               L"No extra input"),
             T(L"原样传递",
               L"Pass through"),
             T(L"URL 编码（UTF-8）",
               L"URL encode (UTF-8)")}) {
        SendMessageW(
            runtimeInput_,
            CB_ADDSTRING,
            0,
            reinterpret_cast<LPARAM>(
                text));
    }

    SendMessageW(
        runtimeInput_,
        CB_SETCURSEL,
        runtimeSelected,
        0);
    SendMessageW(
        runtimeInput_,
        CB_SETMINVISIBLE,
        3,
        0);

    SetWindowTextW(
        testInputLabel_,
        T(L"测试输入",
          L"Test input"));
    SendMessageW(
        testInput_,
        EM_SETCUEBANNER,
        TRUE,
        reinterpret_cast<LPARAM>(
            T(L"例如：8.8.8.8 / 心脏 MRI",
              L"Example: 8.8.8.8 / search text")));

    SetWindowTextW(
        argumentsLabel_,
        T(L"固定参数",
          L"Fixed arguments"));
    SetWindowTextW(
        workdirLabel_,
        T(L"工作目录（留空自动使用目标所在目录）",
          L"Working directory (blank = target directory)"));
    SetWindowTextW(
        browseWorkdir_,
        T(L"选择...",
          L"Browse..."));
    SetWindowTextW(
        iconLabel_,
        T(L"图标（留空 = 自动跟随目标）",
          L"Icon (blank = follow target)"));
    SendMessageW(
        icon_,
        EM_SETCUEBANNER,
        TRUE,
        reinterpret_cast<LPARAM>(
            T(L"自动",
              L"Auto")));
    SetWindowTextW(
        browseIcon_,
        T(L"选择...",
          L"Choose..."));
    SetWindowTextW(
        resetIcon_,
        T(L"自动",
          L"Auto"));

    SetWindowTextW(
        admin_,
        T(L"以管理员身份运行",
          L"Run as administrator"));
    SetWindowTextW(
        test_,
        T(L"测试",
          L"Test"));
    SetWindowTextW(
        save_,
        T(L"保存",
          L"Save"));
    SetWindowTextW(
        cancel_,
        T(L"取消",
          L"Cancel"));

    UpdateAdvancedVisibility();
    UpdateTypeState();
    UpdateRuntimeInputHint();
    UpdateRuntimeTestVisibility();
}

void ShortcutEditorDialog::Layout() {
    if (!hwnd_) {
        return;
    }

    RECT client{};
    GetClientRect(hwnd_, &client);

    const int margin = Scale(22);
    const int labelHeight = Scale(20);
    const int fieldHeight = Scale(30);
    const int smallHeight = Scale(18);
    const int gap = Scale(8);
    const int fileButtonWidth = Scale(62);
    const int folderButtonWidth = Scale(78);
    const int contentWidth =
        client.right - margin * 2;

    int y = Scale(18);

    MoveWindow(
        keywordLabel_,
        margin,
        y,
        contentWidth,
        labelHeight,
        TRUE);
    y += Scale(22);

    MoveWindow(
        keyword_,
        margin,
        y,
        contentWidth,
        fieldHeight,
        TRUE);
    y += Scale(34);

    MoveWindow(
        keywordHint_,
        margin,
        y,
        contentWidth,
        smallHeight,
        TRUE);
    y += Scale(30);

    MoveWindow(
        nameLabel_,
        margin,
        y,
        contentWidth,
        labelHeight,
        TRUE);
    y += Scale(22);

    MoveWindow(
        name_,
        margin,
        y,
        contentWidth,
        fieldHeight,
        TRUE);
    y += Scale(44);

    MoveWindow(
        targetLabel_,
        margin,
        y,
        contentWidth,
        labelHeight,
        TRUE);
    y += Scale(22);

    const int targetWidth =
        contentWidth -
        fileButtonWidth -
        folderButtonWidth -
        gap * 2;

    MoveWindow(
        target_,
        margin,
        y,
        targetWidth,
        fieldHeight,
        TRUE);
    MoveWindow(
        browseFile_,
        margin +
            targetWidth +
            gap,
        y,
        fileButtonWidth,
        fieldHeight,
        TRUE);
    MoveWindow(
        browseFolder_,
        margin +
            targetWidth +
            gap +
            fileButtonWidth +
            gap,
        y,
        folderButtonWidth,
        fieldHeight,
        TRUE);
    y += Scale(44);

    const int typeLabelWidth =
        Scale(82);
    const int typeWidth =
        Scale(180);

    MoveWindow(
        typeLabel_,
        margin,
        y + Scale(4),
        typeLabelWidth,
        labelHeight,
        TRUE);
    MoveWindow(
        type_,
        margin + typeLabelWidth,
        y,
        typeWidth,
        Scale(
            kTypeDropdownHeightLogical),
        TRUE);
    MoveWindow(
        typeHint_,
        margin +
            typeLabelWidth +
            typeWidth +
            gap,
        y + Scale(4),
        contentWidth -
            typeLabelWidth -
            typeWidth -
            gap,
        labelHeight,
        TRUE);
    y += Scale(42);

    const int runtimeLabelWidth =
        Scale(82);
    const int runtimeWidth =
        Scale(200);

    MoveWindow(
        runtimeInputLabel_,
        margin,
        y + Scale(4),
        runtimeLabelWidth,
        labelHeight,
        TRUE);
    MoveWindow(
        runtimeInput_,
        margin + runtimeLabelWidth,
        y,
        runtimeWidth,
        Scale(
            kRuntimeInputDropdownHeightLogical),
        TRUE);
    MoveWindow(
        runtimeInputHint_,
        margin +
            runtimeLabelWidth +
            runtimeWidth +
            gap,
        y + Scale(4),
        contentWidth -
            runtimeLabelWidth -
            runtimeWidth -
            gap,
        Scale(34),
        TRUE);
    y += Scale(48);

    if (SelectedRuntimeInputMode() !=
        RuntimeInputMode::None) {
        const int testLabelWidth =
            Scale(82);

        MoveWindow(
            testInputLabel_,
            margin,
            y + Scale(4),
            testLabelWidth,
            labelHeight,
            TRUE);
        MoveWindow(
            testInput_,
            margin + testLabelWidth,
            y,
            contentWidth -
                testLabelWidth,
            fieldHeight,
            TRUE);
        y += Scale(
            kRuntimeTestExtraHeightLogical);
    }

    MoveWindow(
        advancedToggle_,
        margin,
        y,
        Scale(132),
        Scale(28),
        TRUE);
    y += Scale(40);

    if (advancedExpanded_) {
        MoveWindow(
            argumentsLabel_,
            margin,
            y,
            contentWidth,
            labelHeight,
            TRUE);
        y += Scale(22);

        MoveWindow(
            arguments_,
            margin,
            y,
            contentWidth,
            fieldHeight,
            TRUE);
        y += Scale(44);

        MoveWindow(
            workdirLabel_,
            margin,
            y,
            contentWidth,
            labelHeight,
            TRUE);
        y += Scale(22);

        const int workdirButtonWidth =
            Scale(66);
        MoveWindow(
            workdir_,
            margin,
            y,
            contentWidth -
                workdirButtonWidth -
                gap,
            fieldHeight,
            TRUE);
        MoveWindow(
            browseWorkdir_,
            client.right -
                margin -
                workdirButtonWidth,
            y,
            workdirButtonWidth,
            fieldHeight,
            TRUE);
        y += Scale(44);

        MoveWindow(
            iconLabel_,
            margin,
            y,
            contentWidth,
            labelHeight,
            TRUE);
        y += Scale(22);

        const int iconButtonWidth =
            Scale(66);
        const int autoButtonWidth =
            Scale(54);
        const int iconEditWidth =
            contentWidth -
            iconButtonWidth -
            autoButtonWidth -
            gap * 2;

        MoveWindow(
            icon_,
            margin,
            y,
            iconEditWidth,
            fieldHeight,
            TRUE);
        MoveWindow(
            browseIcon_,
            margin +
                iconEditWidth +
                gap,
            y,
            iconButtonWidth,
            fieldHeight,
            TRUE);
        MoveWindow(
            resetIcon_,
            margin +
                iconEditWidth +
                gap +
                iconButtonWidth +
                gap,
            y,
            autoButtonWidth,
            fieldHeight,
            TRUE);
        y += Scale(44);

        MoveWindow(
            admin_,
            margin,
            y,
            contentWidth,
            Scale(28),
            TRUE);
    }

    const int buttonWidth =
        Scale(92);
    const int buttonHeight =
        Scale(32);
    const int buttonY =
        client.bottom -
        margin -
        buttonHeight;

    MoveWindow(
        test_,
        margin,
        buttonY,
        buttonWidth,
        buttonHeight,
        TRUE);
    MoveWindow(
        cancel_,
        client.right -
            margin -
            buttonWidth,
        buttonY,
        buttonWidth,
        buttonHeight,
        TRUE);
    MoveWindow(
        save_,
        client.right -
            margin -
            buttonWidth * 2 -
            gap,
        buttonY,
        buttonWidth,
        buttonHeight,
        TRUE);
}

void ShortcutEditorDialog::ResizeForContent() {
    if (!hwnd_) {
        return;
    }

    SetWindowPos(
        hwnd_,
        nullptr,
        0,
        0,
        Scale(kEditorWidthLogical),
        Scale(
            (advancedExpanded_
                ? kExpandedHeightLogical
                : kCollapsedHeightLogical) +
            (SelectedRuntimeInputMode() !=
                    RuntimeInputMode::None
                ? kRuntimeTestExtraHeightLogical
                : 0)),
        SWP_NOMOVE |
            SWP_NOZORDER |
            SWP_NOACTIVATE |
            SWP_NOREDRAW);
}

void ShortcutEditorDialog::RefreshDynamicLayout() {
    if (!hwnd_) {
        return;
    }

    ResizeForContent();
    Layout();

    // Runtime-input and Advanced toggles can move most child controls at
    // once. Erase the parent and invalidate all children after the complete
    // move so the previous control rectangles cannot remain as paint trails.
    RedrawWindow(
        hwnd_,
        nullptr,
        nullptr,
        RDW_INVALIDATE |
            RDW_ERASE |
            RDW_ALLCHILDREN);
}

void ShortcutEditorDialog::UpdateAdvancedVisibility() {
    const int command =
        advancedExpanded_
            ? SW_SHOW
            : SW_HIDE;

    for (HWND control : {
             argumentsLabel_,
             arguments_,
             workdirLabel_,
             workdir_,
             browseWorkdir_,
             iconLabel_,
             icon_,
             browseIcon_,
             resetIcon_,
             admin_}) {
        ShowWindow(
            control,
            command);
    }

    SetWindowTextW(
        advancedToggle_,
        advancedExpanded_
            ? T(L"▾ 高级选项",
                L"▾ Advanced")
            : T(L"▸ 高级选项",
                L"▸ Advanced"));
}

void ShortcutEditorDialog::UpdateRuntimeTestVisibility() {
    const bool visible =
        SelectedRuntimeInputMode() !=
        RuntimeInputMode::None;

    ShowWindow(
        testInputLabel_,
        visible ? SW_SHOW : SW_HIDE);
    ShowWindow(
        testInput_,
        visible ? SW_SHOW : SW_HIDE);

    RefreshDynamicLayout();
}

void ShortcutEditorDialog::ToggleAdvanced() {
    advancedExpanded_ =
        !advancedExpanded_;

    UpdateAdvancedVisibility();
    RefreshDynamicLayout();
}

RuntimeInputMode
ShortcutEditorDialog::SelectedRuntimeInputMode()
    const {
    const int selected =
        static_cast<int>(
            SendMessageW(
                runtimeInput_,
                CB_GETCURSEL,
                0,
                0));

    switch (selected) {
    case 1:
        return RuntimeInputMode::Raw;
    case 2:
        return RuntimeInputMode::UrlEncoded;
    case 0:
    default:
        return RuntimeInputMode::None;
    }
}

CommandType ShortcutEditorDialog::SelectedType()
    const {
    const int selected =
        static_cast<int>(
            SendMessageW(
                type_,
                CB_GETCURSEL,
                0,
                0));

    if (selected <= 0) {
        return InferShortcutCommandType(
            ControlText(target_));
    }

    return ExplicitTypeFromIndex(
        selected);
}

void ShortcutEditorDialog::UpdateTypeState() {
    if (!typeHint_) {
        return;
    }

    const int selected =
        static_cast<int>(
            SendMessageW(
                type_,
                CB_GETCURSEL,
                0,
                0));

    const CommandType type =
        selected <= 0
            ? InferShortcutCommandType(
                  ControlText(target_))
            : ExplicitTypeFromIndex(
                  selected);

    const wchar_t* typeName = nullptr;

    switch (type) {
    case CommandType::Url:
        typeName =
            T(L"网址", L"URL");
        break;
    case CommandType::Folder:
        typeName =
            T(L"文件夹", L"Folder");
        break;
    case CommandType::CommandLine:
        typeName =
            T(L"命令行", L"Command line");
        break;
    case CommandType::Application:
    default:
        typeName =
            T(L"应用程序", L"Application");
        break;
    }

    std::wstring text =
        selected <= 0
            ? T(L"识别为：",
                L"Detected: ")
            : T(L"手动指定：",
                L"Override: ");
    text += typeName;

    SetWindowTextW(
        typeHint_,
        text.c_str());

    UpdateRuntimeInputHint();
}

void ShortcutEditorDialog::UpdateRuntimeInputHint() {
    if (!runtimeInputHint_) {
        return;
    }

    const RuntimeInputMode mode =
        SelectedRuntimeInputMode();

    if (mode ==
        RuntimeInputMode::None) {
        SetWindowTextW(
            runtimeInputHint_,
            T(L"只输入快捷词时直接启动。",
              L"Launch directly from the keyword."));
        return;
    }

    Command preview;
    preview.type =
        SelectedType();
    preview.runtimeInputMode =
        mode;
    preview.target =
        TrimWide(
            ControlText(target_));
    preview.arguments =
        TrimWide(
            ControlText(arguments_));
    preview.workingDirectory =
        TrimWide(
            ControlText(workdir_));

    if (!CanAcceptRuntimeInput(
            preview)) {
        SetWindowTextW(
            runtimeInputHint_,
            T(L"请加入 {input} 指定插入位置。",
              L"Add {input} to choose the insertion point."));
        return;
    }

    if (mode ==
        RuntimeInputMode::UrlEncoded) {
        SetWindowTextW(
            runtimeInputHint_,
            T(L"UTF-8 URL 编码后替换 {input}。",
              L"UTF-8 URL encoded, then replaces {input}."));
        return;
    }

    if (!HasRuntimeInputPlaceholder(
            preview) &&
        (preview.type ==
             CommandType::Application ||
         preview.type ==
             CommandType::CommandLine)) {
        SetWindowTextW(
            runtimeInputHint_,
            T(L"自动追加到固定参数；也可用 {input} 指定位置。",
              L"Appended to fixed arguments, or place with {input}."));
        return;
    }

    SetWindowTextW(
        runtimeInputHint_,
        T(L"原样替换 {input}。",
          L"Replaces {input} unchanged."));
}

void ShortcutEditorDialog::SetNameText(
    std::wstring_view text,
    bool automatic) {
    suppressNameChange_ = true;

    const std::wstring value(text);
    SetWindowTextW(
        name_,
        value.c_str());

    suppressNameChange_ = false;
    nameAuto_ = automatic;
}

void ShortcutEditorDialog::MaybeAutoFillName() {
    const std::wstring current =
        TrimWide(
            ControlText(name_));

    if (!nameAuto_ &&
        !current.empty()) {
        return;
    }

    const auto keywords =
        ParseShortcutKeywords(
            ControlText(keyword_));

    const std::wstring suggested =
        SuggestShortcutTitle(
            ControlText(target_),
            SelectedType(),
            keywords.primary);

    if (!suggested.empty()) {
        SetNameText(
            suggested,
            true);
    }
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

    const std::wstring keywordText =
        FormatShortcutKeywords(
            it->keyword,
            it->aliases);

    SetWindowTextW(
        keyword_,
        keywordText.c_str());
    SetWindowTextW(
        target_,
        it->target.c_str());
    SetWindowTextW(
        arguments_,
        it->arguments.c_str());
    SetWindowTextW(
        workdir_,
        it->workingDirectory.c_str());
    SetWindowTextW(
        icon_,
        (it->icon.empty() ||
         it->icon == L"auto")
            ? L""
            : it->icon.c_str());
    SetWindowTextW(
        testInput_,
        L"");

    int runtimeInputIndex = 0;
    switch (it->runtimeInputMode) {
    case RuntimeInputMode::Raw:
        runtimeInputIndex = 1;
        break;
    case RuntimeInputMode::UrlEncoded:
        runtimeInputIndex = 2;
        break;
    case RuntimeInputMode::None:
    default:
        runtimeInputIndex = 0;
        break;
    }

    SendMessageW(
        runtimeInput_,
        CB_SETCURSEL,
        runtimeInputIndex,
        0);

    const CommandType inferred =
        InferShortcutCommandType(
            it->target);

    SendMessageW(
        type_,
        CB_SETCURSEL,
        inferred == it->type
            ? 0
            : ExplicitTypeIndex(
                  it->type),
        0);

    const std::wstring suggested =
        SuggestShortcutTitle(
            it->target,
            it->type,
            it->keyword);

    SetNameText(
        it->title,
        it->title == suggested);

    SetChecked(
        admin_,
        it->runAsAdmin);

    advancedExpanded_ =
        !it->arguments.empty() ||
        !it->workingDirectory.empty() ||
        (!it->icon.empty() &&
         it->icon != L"auto") ||
        it->runAsAdmin;

    UpdateAdvancedVisibility();
    UpdateTypeState();
    UpdateRuntimeTestVisibility();
}

void ShortcutEditorDialog::BeginNew(
    const Command* seed) {
    commandId_.clear();

    const Command empty;
    const Command& initial =
        seed ? *seed : empty;

    SetWindowTextW(
        keyword_,
        L"");
    SetWindowTextW(
        target_,
        initial.target.c_str());
    SetWindowTextW(
        arguments_,
        initial.arguments.c_str());
    SetWindowTextW(
        workdir_,
        initial.workingDirectory.c_str());
    SetWindowTextW(
        icon_,
        initial.icon.empty() ||
                initial.icon == L"auto"
            ? L""
            : initial.icon.c_str());
    SetWindowTextW(
        testInput_,
        L"");

    std::wstring initialName =
        initial.title;

    if (initialName.empty() &&
        !initial.target.empty()) {
        initialName =
            SuggestShortcutTitle(
                initial.target,
                initial.type);
    }

    SetNameText(
        initialName,
        true);

    const CommandType inferred =
        InferShortcutCommandType(
            initial.target);

    SendMessageW(
        type_,
        CB_SETCURSEL,
        !seed ||
                inferred ==
                    initial.type
            ? 0
            : ExplicitTypeIndex(
                  initial.type),
        0);

    int runtimeInputIndex = 0;

    if (seed) {
        switch (initial.runtimeInputMode) {
        case RuntimeInputMode::Raw:
            runtimeInputIndex = 1;
            break;
        case RuntimeInputMode::UrlEncoded:
            runtimeInputIndex = 2;
            break;
        case RuntimeInputMode::None:
        default:
            break;
        }
    }

    SendMessageW(
        runtimeInput_,
        CB_SETCURSEL,
        runtimeInputIndex,
        0);

    SetChecked(
        admin_,
        seed &&
            initial.runAsAdmin);

    advancedExpanded_ =
        seed &&
        (!initial.arguments.empty() ||
         !initial.workingDirectory.empty() ||
         (!initial.icon.empty() &&
          initial.icon != L"auto") ||
         initial.runAsAdmin);

    UpdateAdvancedVisibility();
    UpdateTypeState();
    UpdateRuntimeTestVisibility();
}

std::wstring ShortcutEditorDialog::ControlText(
    HWND control) const {
    const int length =
        GetWindowTextLengthW(
            control);

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

Command ShortcutEditorDialog::CollectCommand()
    const {
    Command command;

    const auto keywords =
        ParseShortcutKeywords(
            ControlText(keyword_));

    command.keyword =
        keywords.primary;
    command.aliases =
        keywords.aliases;
    command.type =
        SelectedType();
    command.target =
        TrimWide(
            ControlText(target_));
    command.arguments =
        TrimWide(
            ControlText(arguments_));
    command.workingDirectory =
        TrimWide(
            ControlText(workdir_));
    command.runtimeInputMode =
        SelectedRuntimeInputMode();

    command.title =
        TrimWide(
            ControlText(name_));

    if (command.title.empty()) {
        command.title =
            SuggestShortcutTitle(
                command.target,
                command.type,
                command.keyword);
    }

    if (command.title.empty()) {
        command.title =
            command.keyword;
    }

    const std::wstring icon =
        TrimWide(
            ControlText(icon_));
    command.icon =
        icon.empty()
            ? L"auto"
            : icon;
    command.enabled = true;
    command.runAsAdmin =
        IsChecked(admin_);
    command.pinned = false;
    command.source =
        CommandSource::User;
    command.basePriority = 120;

    return command;
}

bool ShortcutEditorDialog::Save() {
    Command command =
        CollectCommand();

    if (command.keyword.empty() ||
        command.target.empty()) {
        MessageBoxW(
            hwnd_,
            T(L"快捷词和目标为必填项。",
              L"Keywords and target are required."),
            T(L"无法保存快捷项",
              L"Cannot save shortcut"),
            MB_OK |
                MB_ICONWARNING);
        return false;
    }

    if (command.runtimeInputMode !=
            RuntimeInputMode::None &&
        !CanAcceptRuntimeInput(
            command)) {
        MessageBoxW(
            hwnd_,
            T(L"当前目标类型无法自动放置运行时输入。\n\n请在目标、固定参数或工作目录中加入 {input}。",
              L"This target type has no automatic location for runtime input.\n\nAdd {input} to the target, fixed arguments or working directory."),
            T(L"运行时输入配置不完整",
              L"Runtime input needs a placeholder"),
            MB_OK |
                MB_ICONWARNING);
        return false;
    }

    const auto conflict =
        FindShortcutKeywordConflict(
            command,
            app_.UserCommands(),
            commandId_);

    if (conflict) {
        std::wstring message =
            T(L"快捷词“",
              L"Keyword \"");
        message += conflict->token;
        message +=
            T(L"”已被快捷项“",
              L"\" is already used by shortcut \"");
        message += conflict->existingTitle;
        message +=
            T(L"”使用。\n\n仍然保存吗？",
              L"\".\n\nSave anyway?");

        const int answer =
            MessageBoxW(
                hwnd_,
                message.c_str(),
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
            T(L"测试运行",
              L"Test"),
            MB_OK |
                MB_ICONWARNING);
        return;
    }

    if (command.runtimeInputMode !=
            RuntimeInputMode::None &&
        !CanAcceptRuntimeInput(
            command)) {
        MessageBoxW(
            hwnd_,
            T(L"当前运行时输入配置不完整，请先加入 {input} 或改用应用程序/命令行自动追加。",
              L"Runtime input is incomplete. Add {input}, or use Application/Command line auto-append."),
            T(L"测试运行",
              L"Test"),
            MB_OK |
                MB_ICONWARNING);
        return;
    }

    const std::wstring runtimeInput =
        TrimWide(
            ControlText(testInput_));

    if (command.runtimeInputMode !=
            RuntimeInputMode::None &&
        runtimeInput.empty()) {
        MessageBoxW(
            hwnd_,
            T(L"请先填写“测试输入”，这样可以直接验证动态参数。",
              L"Enter Test input first to verify the dynamic argument."),
            T(L"测试运行",
              L"Test"),
            MB_OK |
                MB_ICONWARNING);
        SetFocus(testInput_);
        return;
    }

    app_.TestCommand(
        command,
        runtimeInput);
}

void ShortcutEditorDialog::BrowseTargetFile() {
    std::array<wchar_t, 32768>
        file{};

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
        L"Programs and shortcuts\0*.exe;*.lnk;*.bat;*.cmd;*.com;*.ps1;*.url\0"
        L"All files\0*.*\0\0";

    OPENFILENAMEW open{};
    open.lStructSize =
        sizeof(open);
    open.hwndOwner =
        hwnd_;
    open.lpstrFile =
        file.data();
    open.nMaxFile =
        static_cast<DWORD>(
            file.size());
    open.lpstrFilter =
        filter;
    open.nFilterIndex = 1;
    open.Flags =
        OFN_FILEMUSTEXIST |
        OFN_PATHMUSTEXIST |
        OFN_EXPLORER |
        OFN_NOCHANGEDIR;

    if (!GetOpenFileNameW(
            &open)) {
        return;
    }

    SetWindowTextW(
        target_,
        file.data());

    SendMessageW(
        type_,
        CB_SETCURSEL,
        0,
        0);

    UpdateTypeState();
    MaybeAutoFillName();
}

void ShortcutEditorDialog::BrowseTargetFolder() {
    BROWSEINFOW browse{};
    browse.hwndOwner =
        hwnd_;
    browse.lpszTitle =
        T(L"选择目标文件夹",
          L"Choose target folder");
    browse.ulFlags =
        BIF_RETURNONLYFSDIRS |
        BIF_NEWDIALOGSTYLE |
        BIF_EDITBOX;

    PIDLIST_ABSOLUTE item =
        SHBrowseForFolderW(
            &browse);

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

        SendMessageW(
            type_,
            CB_SETCURSEL,
            0,
            0);

        UpdateTypeState();
        MaybeAutoFillName();
    }

    CoTaskMemFree(item);
}

void ShortcutEditorDialog::
BrowseWorkingDirectory() {
    BROWSEINFOW browse{};
    browse.hwndOwner =
        hwnd_;
    browse.lpszTitle =
        T(L"选择工作目录",
          L"Choose working directory");
    browse.ulFlags =
        BIF_RETURNONLYFSDIRS |
        BIF_NEWDIALOGSTYLE |
        BIF_EDITBOX;

    PIDLIST_ABSOLUTE item =
        SHBrowseForFolderW(
            &browse);

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

void ShortcutEditorDialog::BrowseIcon() {
    std::array<wchar_t, 32768>
        file{};

    const std::wstring current =
        TrimWide(
            ControlText(icon_));

    if (!current.empty() &&
        current.size() <
            file.size()) {
        std::copy(
            current.begin(),
            current.end(),
            file.begin());
    }

    const wchar_t filter[] =
        L"Icon sources\0*.ico;*.exe;*.dll;*.lnk\0"
        L"All files\0*.*\0\0";

    OPENFILENAMEW open{};
    open.lStructSize =
        sizeof(open);
    open.hwndOwner =
        hwnd_;
    open.lpstrFile =
        file.data();
    open.nMaxFile =
        static_cast<DWORD>(
            file.size());
    open.lpstrFilter =
        filter;
    open.nFilterIndex = 1;
    open.Flags =
        OFN_FILEMUSTEXIST |
        OFN_PATHMUSTEXIST |
        OFN_EXPLORER |
        OFN_NOCHANGEDIR;

    if (!GetOpenFileNameW(
            &open)) {
        return;
    }

    SetWindowTextW(
        icon_,
        file.data());
}

void ShortcutEditorDialog::ResetIcon() {
    SetWindowTextW(
        icon_,
        L"");
    SetFocus(icon_);
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
        case kIdKeyword:
            if (HIWORD(wParam) ==
                EN_KILLFOCUS) {
                MaybeAutoFillName();
            }
            return 0;

        case kIdName:
            if (HIWORD(wParam) ==
                    EN_CHANGE &&
                !suppressNameChange_) {
                nameAuto_ = false;
            }
            return 0;

        case kIdTarget:
            if (HIWORD(wParam) ==
                EN_CHANGE) {
                UpdateTypeState();
            } else if (
                HIWORD(wParam) ==
                EN_KILLFOCUS) {
                MaybeAutoFillName();
            }
            return 0;

        case kIdType:
            if (HIWORD(wParam) ==
                CBN_SELCHANGE) {
                UpdateTypeState();
                MaybeAutoFillName();
            }
            return 0;

        case kIdRuntimeInput:
            if (HIWORD(wParam) ==
                CBN_SELCHANGE) {
                UpdateRuntimeInputHint();
                UpdateRuntimeTestVisibility();
            }
            return 0;

        case kIdBrowseFile:
            if (HIWORD(wParam) ==
                BN_CLICKED) {
                BrowseTargetFile();
            }
            return 0;

        case kIdBrowseFolder:
            if (HIWORD(wParam) ==
                BN_CLICKED) {
                BrowseTargetFolder();
            }
            return 0;

        case kIdArguments:
        case kIdWorkdir:
            if (HIWORD(wParam) ==
                EN_CHANGE) {
                UpdateRuntimeInputHint();
            }
            return 0;

        case kIdIcon:
            return 0;

        case kIdBrowseIcon:
            if (HIWORD(wParam) ==
                BN_CLICKED) {
                BrowseIcon();
            }
            return 0;

        case kIdResetIcon:
            if (HIWORD(wParam) ==
                BN_CLICKED) {
                ResetIcon();
            }
            return 0;

        case kIdAdvancedToggle:
            if (HIWORD(wParam) ==
                BN_CLICKED) {
                ToggleAdvanced();
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
