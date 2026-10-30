#include "Feedback.hpp"
#include "AppIcon.hpp"
#include "ShortcutEditorDialog.hpp"

#include "TopLevelWindowPresentation.hpp"
#include "UiComboBox.hpp"
#include "UiComboPopup.hpp"
#include "UiEditChrome.hpp"
#include "UiFocusCues.hpp"
#include "UiMetrics.hpp"
#include "UiTheme.hpp"
#include "UiTypography.hpp"

#include "../app/App.hpp"
#include "../core/ShortcutEditorModel.hpp"
#include "../core/RuntimeInput.hpp"

#include <commctrl.h>
#include <commdlg.h>
#include <objbase.h>
#include <shlobj.h>
#include <shobjidl.h>

#include <algorithm>
#include <array>
#include <cwctype>
#include <filesystem>
#include <vector>

namespace altrun {
namespace {

constexpr wchar_t kShortcutEditorClass[] =
    L"Asterun.ShortcutEditor";

constexpr int kEditorWidthLogical = 590;
constexpr int kInitialEditorHeightLogical = 420;
constexpr int kRuntimeTestExtraHeightLogical = 42;
// Preserve the space below Test input; this adds breathing room above it.
constexpr int kRuntimeTestTopGapLogical = 14;
constexpr int kControlRowHeightLogical = 28;
constexpr int kFooterButtonHeightLogical = 32;
constexpr int kFooterBottomMarginLogical = 16;
constexpr int kFooterSeparatorGapLogical = 10;
constexpr int kContentFooterGapLogical = 16;
constexpr int kAdminTopGapLogical = 4;
// Space between the Name/Keywords row and the Target label. The keyword
// hint now sits beside its label, so no extra hint band is reserved here.
constexpr int kIdentityToTargetGapLogical = 8;
// Glyph-to-label gap of the custom admin checkbox (native checkboxes use
// roughly 5-6px; 9px read as detached from its label).
constexpr int kAdminLabelGapLogical = 6;
constexpr int kInlineComboMinWidthLogical = 158;
constexpr int kInlineComboMaxWidthLogical = 235;
constexpr int kAdvancedRightInsetLogical = 18;

constexpr COLORREF kEditorHintText =
    RGB(112, 119, 128);

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
constexpr UINT kIdRuntimeHint = 53120;
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
// The administrator choice is a single owner-drawn BUTTON. A native
// BS_AUTOCHECKBOX subclass painted over the native glyph and focus indicator;
// button-state repaints then exposed two different checkbox frames.
LRESULT CALLBACK AdminCheckboxSubclass(
    HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam,
    UINT_PTR id, DWORD_PTR) {
    if (message == BM_GETCHECK) {
        return GetWindowLongPtrW(hwnd, GWLP_USERDATA)
            ? BST_CHECKED : BST_UNCHECKED;
    }
    if (message == BM_SETCHECK) {
        SetWindowLongPtrW(hwnd, GWLP_USERDATA,
            wParam == BST_CHECKED ? 1 : 0);
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }
    if (message == WM_ERASEBKGND) {
        // PaintAdminCheckbox fills the whole item opaquely.
        return TRUE;
    }
    if (message == WM_NCDESTROY) {
        RemoveWindowSubclass(hwnd, AdminCheckboxSubclass, id);
    }
    return DefSubclassProc(hwnd, message, wParam, lParam);
}

// An owner-drawn BUTTON erases itself with the parent's WM_CTLCOLORBTN brush
// before WM_DRAWITEM. For the Advanced header that erase blanked the section
// separator for a moment on every text change/click, so rapid toggling made
// the line flash. DrawAdvancedHeader paints the full item opaquely.
LRESULT CALLBACK NoEraseOwnerDrawSubclass(
    HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam,
    UINT_PTR id, DWORD_PTR) {
    if (message == WM_ERASEBKGND) {
        return TRUE;
    }
    if (message == WM_NCDESTROY) {
        RemoveWindowSubclass(hwnd, NoEraseOwnerDrawSubclass, id);
    }
    return DefSubclassProc(hwnd, message, wParam, lParam);
}

void PaintAdminCheckbox(const DRAWITEMSTRUCT& draw, HFONT font, UINT dpi) {
    if (!draw.hDC || !draw.hwndItem) return;
    const int width = draw.rcItem.right - draw.rcItem.left;
    const int height = draw.rcItem.bottom - draw.rcItem.top;
    if (width <= 0 || height <= 0) return;
    const auto scale = [dpi](int n) {
        return MulDiv(n, static_cast<int>(dpi ? dpi : 96), 96);
    };
    HDC memory = CreateCompatibleDC(draw.hDC);
    HBITMAP bitmap = memory
        ? CreateCompatibleBitmap(draw.hDC, width, height) : nullptr;
    HGDIOBJ oldBitmap = memory && bitmap
        ? SelectObject(memory, bitmap) : nullptr;
    const bool buffered = oldBitmap && oldBitmap != HGDI_ERROR;
    HDC dc = buffered ? memory : draw.hDC;
    RECT area = buffered
        ? RECT{0, 0, width, height} : draw.rcItem;
    const auto& palette = ui::kApplicationPalette;
    const bool enabled = (draw.itemState & ODS_DISABLED) == 0;
    const bool checked = IsChecked(draw.hwndItem);
    const bool pressed = (draw.itemState & ODS_SELECTED) != 0;

    HBRUSH background = CreateSolidBrush(palette.windowBackground);
    FillRect(dc, &area,
        background ? background : GetSysColorBrush(COLOR_WINDOW));
    if (background) DeleteObject(background);

    const int size = scale(16);
    const int left = area.left + scale(2);
    // Align the box with the visible glyph body, not the font line box:
    // YaHei's ascent/internal leading place CJK ink above its box center.
    const HGDIOBJ prevFont = font ? SelectObject(dc, font) : nullptr;
    TEXTMETRICW metrics{};
    int visualCenter = (area.top + area.bottom) / 2;
    if (GetTextMetricsW(dc, &metrics)) {
        const int lineTop = area.top +
            (area.bottom - area.top - metrics.tmHeight) / 2;
        const int baseline = lineTop + metrics.tmAscent;
        const int inkHeight = std::max(1,
            static_cast<int>(metrics.tmAscent - metrics.tmInternalLeading));
        visualCenter = baseline - inkHeight / 2;
    }
    const int top = visualCenter - size / 2;
    const RECT square{left, top, left + size, top + size};

    // Shared glyph: same outline/radius/accent as every custom checkbox.
    ui::PaintSmoothCheckbox(dc, square.left, square.top, size,
        scale(ui::kControlCornerRadiusLogical), checked, enabled, pressed,
        palette.windowBackground, palette.accent, palette.controlBackground);
    wchar_t label[128]{};
    GetWindowTextW(draw.hwndItem, label,
        static_cast<int>(std::size(label)));
    SetBkMode(dc, TRANSPARENT);
    // A mouse click also focuses a BUTTON. Changing the whole label to
    // accent on focus made it appear to flash rather than just checking it.
    // Keep the readable label neutral; the accent-filled checkbox carries
    // selection state and the standard tab navigation remains intact.
    SetTextColor(dc, enabled ? palette.text : palette.mutedText);
    RECT text{
        left + size + scale(kAdminLabelGapLogical), area.top,
        area.right - scale(2), area.bottom
    };
    DrawTextW(dc, label, -1, &text,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    if (prevFont && prevFont != HGDI_ERROR) SelectObject(dc, prevFont);

    if (buffered) {
        BitBlt(draw.hDC, draw.rcItem.left, draw.rcItem.top,
               width, height, memory, 0, 0, SRCCOPY);
        SelectObject(memory, oldBitmap);
    }
    if (bitmap) DeleteObject(bitmap);
    if (memory) DeleteDC(memory);
}

enum class PickerResult {
    Selected,
    Cancelled,
    Unavailable,
};

class ScopedComApartment {
public:
    ScopedComApartment()
        : result_(
              CoInitializeEx(
                  nullptr,
                  COINIT_APARTMENTTHREADED |
                      COINIT_DISABLE_OLE1DDE)),
          uninitialize_(
              SUCCEEDED(result_)) {}

    ~ScopedComApartment() {
        if (uninitialize_) {
            CoUninitialize();
        }
    }

    [[nodiscard]] bool Ready() const {
        return SUCCEEDED(result_) ||
            result_ ==
                RPC_E_CHANGED_MODE;
    }

private:
    HRESULT result_{};
    bool uninitialize_{false};
};

void SeedShellDialogFromPath(
    IFileDialog* dialog,
    std::wstring_view currentValue,
    bool folderPicker) {
    if (!dialog) {
        return;
    }

    const std::wstring current =
        TrimWide(currentValue);

    if (current.empty()) {
        return;
    }

    std::filesystem::path path(
        current);

    const DWORD attributes =
        GetFileAttributesW(
            current.c_str());

    const bool isDirectory =
        attributes !=
            INVALID_FILE_ATTRIBUTES &&
        (attributes &
         FILE_ATTRIBUTE_DIRECTORY) != 0;

    std::filesystem::path folder =
        isDirectory
            ? path
            : path.parent_path();

    if (!folder.empty()) {
        IShellItem* folderItem =
            nullptr;

        if (SUCCEEDED(
                SHCreateItemFromParsingName(
                    folder.c_str(),
                    nullptr,
                    IID_PPV_ARGS(
                        &folderItem))) &&
            folderItem) {
            dialog->SetFolder(
                folderItem);
            folderItem->Release();
        }
    }

    if (!folderPicker &&
        !isDirectory &&
        !path.filename().empty()) {
        dialog->SetFileName(
            path.filename().c_str());
    }
}

PickerResult PickFileModern(
    HWND owner,
    const wchar_t* title,
    const COMDLG_FILTERSPEC* filters,
    UINT filterCount,
    std::wstring_view currentValue,
    std::wstring& selectedPath) {
    ScopedComApartment apartment;
    if (!apartment.Ready()) {
        return PickerResult::Unavailable;
    }

    IFileOpenDialog* dialog =
        nullptr;

    const HRESULT createResult =
        CoCreateInstance(
            CLSID_FileOpenDialog,
            nullptr,
            CLSCTX_INPROC_SERVER,
            IID_PPV_ARGS(&dialog));

    if (FAILED(createResult) ||
        !dialog) {
        return PickerResult::Unavailable;
    }

    DWORD options = 0;
    if (SUCCEEDED(
            dialog->GetOptions(
                &options))) {
        dialog->SetOptions(
            options |
            FOS_FORCEFILESYSTEM |
            FOS_FILEMUSTEXIST |
            FOS_PATHMUSTEXIST |
            FOS_NOCHANGEDIR);
    }

    if (title &&
        *title) {
        dialog->SetTitle(title);
    }

    if (filters &&
        filterCount > 0) {
        dialog->SetFileTypes(
            filterCount,
            filters);
        dialog->SetFileTypeIndex(1);
    }

    SeedShellDialogFromPath(
        dialog,
        currentValue,
        false);

    const HRESULT showResult =
        dialog->Show(owner);

    if (showResult ==
        HRESULT_FROM_WIN32(
            ERROR_CANCELLED)) {
        dialog->Release();
        return PickerResult::Cancelled;
    }

    if (FAILED(showResult)) {
        dialog->Release();
        return PickerResult::Unavailable;
    }

    IShellItem* item =
        nullptr;
    const HRESULT result =
        dialog->GetResult(
            &item);

    if (FAILED(result) ||
        !item) {
        dialog->Release();
        return PickerResult::Unavailable;
    }

    PWSTR path = nullptr;
    const HRESULT pathResult =
        item->GetDisplayName(
            SIGDN_FILESYSPATH,
            &path);

    if (SUCCEEDED(pathResult) &&
        path) {
        selectedPath.assign(path);
    }

    CoTaskMemFree(path);
    item->Release();
    dialog->Release();

    return selectedPath.empty()
        ? PickerResult::Unavailable
        : PickerResult::Selected;
}

PickerResult PickFolderModern(
    HWND owner,
    const wchar_t* title,
    std::wstring_view currentValue,
    std::wstring& selectedPath) {
    ScopedComApartment apartment;
    if (!apartment.Ready()) {
        return PickerResult::Unavailable;
    }

    IFileOpenDialog* dialog =
        nullptr;

    const HRESULT createResult =
        CoCreateInstance(
            CLSID_FileOpenDialog,
            nullptr,
            CLSCTX_INPROC_SERVER,
            IID_PPV_ARGS(&dialog));

    if (FAILED(createResult) ||
        !dialog) {
        return PickerResult::Unavailable;
    }

    DWORD options = 0;
    if (SUCCEEDED(
            dialog->GetOptions(
                &options))) {
        dialog->SetOptions(
            options |
            FOS_PICKFOLDERS |
            FOS_FORCEFILESYSTEM |
            FOS_PATHMUSTEXIST |
            FOS_NOCHANGEDIR);
    }

    if (title &&
        *title) {
        dialog->SetTitle(title);
    }

    SeedShellDialogFromPath(
        dialog,
        currentValue,
        true);

    const HRESULT showResult =
        dialog->Show(owner);

    if (showResult ==
        HRESULT_FROM_WIN32(
            ERROR_CANCELLED)) {
        dialog->Release();
        return PickerResult::Cancelled;
    }

    if (FAILED(showResult)) {
        dialog->Release();
        return PickerResult::Unavailable;
    }

    IShellItem* item =
        nullptr;
    const HRESULT result =
        dialog->GetResult(
            &item);

    if (FAILED(result) ||
        !item) {
        dialog->Release();
        return PickerResult::Unavailable;
    }

    PWSTR path = nullptr;
    const HRESULT pathResult =
        item->GetDisplayName(
            SIGDN_FILESYSPATH,
            &path);

    if (SUCCEEDED(pathResult) &&
        path) {
        selectedPath.assign(path);
    }

    CoTaskMemFree(path);
    item->Release();
    dialog->Release();

    return selectedPath.empty()
        ? PickerResult::Unavailable
        : PickerResult::Selected;
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

    if (semiboldFont_) {
        DeleteObject(semiboldFont_);
        semiboldFont_ = nullptr;
    }
    if (hintFont_) {
        DeleteObject(hintFont_);
        hintFont_ = nullptr;
    }

    if (backgroundBrush_) {
        DeleteObject(backgroundBrush_);
        backgroundBrush_ = nullptr;
    }

    if (footerBrush_) {
        DeleteObject(footerBrush_);
        footerBrush_ = nullptr;
    }
}

void ShortcutEditorDialog::CloseWindow() {
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
        altrun::ui::ShowMessage(
            owner,
            app.SettingsData().language ==
                    Language::ZhCN
                ? L"无法创建快捷项编辑窗口。"
                : L"Could not create the shortcut editor.",
            L"Asterun",
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
        altrun::ui::ShowMessage(
            owner,
            app.SettingsData().language ==
                    Language::ZhCN
                ? L"无法创建快捷项编辑窗口。"
                : L"Could not create the shortcut editor.",
            L"Asterun",
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
        ui::LoadApplicationIcon(
            instance_);
    wc.hIconSm =
        ui::LoadApplicationIcon(
            instance_,
            true);
    wc.hbrBackground = nullptr;

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
                kEditorWidthLogical,
                kInitialEditorHeightLogical);

    hwnd_ = CreateWindowExW(
        WS_EX_DLGMODALFRAME |
            WS_EX_CONTROLPARENT,
        kShortcutEditorClass,
        L"",
        WS_POPUP |
            WS_CAPTION |
            WS_SYSMENU |
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

    if (commandId.empty()) {
        BeginNew(seed);
    } else {
        LoadCommand(commandId);
    }

    UpdateAdvancedVisibility();
    ResizeForContent();
    Layout();

    // Content-dependent height is resolved while hidden. Re-center the final
    // physical rectangle through the shared owned-popup policy before any
    // compositor-visible frame can exist.
    window_presentation::
        CenterExistingWindow(
            hwnd_,
            owner_);

    return true;
}

bool ShortcutEditorDialog::RunModal() {
    const bool ownerWasEnabled = owner_ && IsWindowEnabled(owner_);
    if (ownerWasEnabled) {
        EnableWindow(owner_, FALSE);
    }

    // Place the caret at the end on the keyword EDIT's actual first focus,
    // not merely after window creation. Activation timing differs between
    // interactive desktops and CI, and native EDIT focus handling can reset a
    // selection that was applied before EN_SETFOCUS.
    keywordInitialCaretPending_ = true;

    window_presentation::
        RevealFullyPainted(
            hwnd_);
    SetForegroundWindow(hwnd_);
    if (GetForegroundWindow() == hwnd_) {
        SetFocus(keyword_);
    }

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

        // Save remains the editor's default action even though the visual
        // button is owner-drawn. Keep the old Enter workflow for native Edit
        // and closed ComboBox controls without intercepting Enter from other
        // buttons/checkboxes or from an open drop-down list.
        if (msg.message == WM_KEYDOWN &&
            msg.wParam == VK_RETURN &&
            (GetKeyState(VK_MENU) & 0x8000) == 0 &&
            (GetKeyState(VK_CONTROL) & 0x8000) == 0) {
            const HWND focus =
                GetFocus();

            const bool editFocus =
                focus == keyword_ ||
                focus == name_ ||
                focus == target_ ||
                focus == testInput_ ||
                focus == arguments_ ||
                focus == workdir_;

            const bool typeFocus =
                focus == type_ &&
                SendMessageW(
                    type_,
                    CB_GETDROPPEDSTATE,
                    0,
                    0) == FALSE;

            const bool runtimeFocus =
                focus == runtimeInput_ &&
                SendMessageW(
                    runtimeInput_,
                    CB_GETDROPPEDSTATE,
                    0,
                    0) == FALSE;

            if (editFocus ||
                typeFocus ||
                runtimeFocus) {
                Save();
                continue;
            }
        }

        // Esc cancels the editor, like the Shortcut Manager and the path
        // converter. IsDialogMessage would only send IDCANCEL, which this
        // window does not own. An open combo list consumes Esc itself first.
        if (msg.message == WM_KEYDOWN &&
            msg.wParam == VK_ESCAPE &&
            (msg.hwnd == hwnd_ ||
             IsChild(hwnd_, msg.hwnd)) &&
            !ui::IsNextComboPopupVisible(type_) &&
            !ui::IsNextComboPopupVisible(runtimeInput_)) {
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

void ShortcutEditorDialog::CreateControls() {
    const auto makeStatic =
        [&](HWND& control,
            DWORD alignment = SS_LEFT,
            UINT id = 0) {
            control = CreateWindowExW(
                0,
                L"STATIC",
                L"",
                WS_CHILD |
                    WS_VISIBLE |
                    alignment,
                0,
                0,
                0,
                0,
                hwnd_,
                id ? reinterpret_cast<HMENU>(
                    static_cast<UINT_PTR>(id)) : nullptr,
                instance_,
                nullptr);
        };

    const auto makeEdit =
        [&](HWND& control,
            UINT id) {
            control = CreateWindowExW(
                0,
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

            if (control) {
                SendMessageW(
                    control,
                    EM_SETMARGINS,
                    EC_LEFTMARGIN |
                        EC_RIGHTMARGIN,
                    MAKELPARAM(
                        Scale(7),
                        Scale(7)));
                (void)ui::AttachNativeEditChrome(control, &dpi_);
            }
        };

    const auto makeButton =
        [&](HWND& control,
            UINT id,
            DWORD style =
                BS_OWNERDRAW) {
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
    // One line beside the label: ellipsize instead of wrapping into a
    // second line that the single-line box would clip.
    makeStatic(keywordHint_, SS_LEFTNOWORDWRAP | SS_ENDELLIPSIS);

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

    makeStatic(
        typeLabel_,
        SS_RIGHT);
    type_ =
        ui::CreateNextComboBox(
            hwnd_,
            instance_,
            kIdType,
            ui::kApplicationPalette
                .windowBackground);
    makeStatic(typeHint_);

    makeStatic(
        runtimeInputLabel_,
        SS_RIGHT);
    runtimeInput_ =
        ui::CreateNextComboBox(
            hwnd_,
            instance_,
            kIdRuntimeInput,
            ui::kApplicationPalette
                .windowBackground);
    makeStatic(runtimeInputHint_, SS_LEFT, kIdRuntimeHint);

    makeStatic(
        testInputLabel_,
        SS_RIGHT);
    makeEdit(
        testInput_,
        kIdTestInput);

    makeButton(
        advancedToggle_,
        kIdAdvancedToggle,
        BS_OWNERDRAW);
    if (advancedToggle_) {
        SetWindowSubclass(
            advancedToggle_, NoEraseOwnerDrawSubclass, 1, 0);
    }

    makeStatic(
        argumentsLabel_,
        SS_RIGHT);
    makeEdit(
        arguments_,
        kIdArguments);

    makeStatic(
        workdirLabel_,
        SS_RIGHT);
    makeEdit(
        workdir_,
        kIdWorkdir);
    makeButton(
        browseWorkdir_,
        kIdBrowseWorkdir);

    makeButton(
        admin_,
        kIdAdmin,
        BS_OWNERDRAW);
    if (admin_) {
        SetWindowSubclass(admin_, AdminCheckboxSubclass, 1, 0);
    }

    makeButton(
        test_,
        kIdTest);
    makeButton(
        save_,
        kIdSave);
    makeButton(
        cancel_,
        kIdCancel);

    backgroundBrush_ =
        CreateSolidBrush(
            ui::kApplicationPalette.windowBackground);
    footerBrush_ =
        CreateSolidBrush(
            ui::kApplicationPalette.bottomBackground);
    ApplyFonts();
}

void ShortcutEditorDialog::ApplyFonts(BOOL repaint) {
    const auto language = app_.SettingsData().language;
    HFONT body = ui::CreateFontHandle(
        ui::ApplicationFontSpec(language, ui::UiFontRole::ShortcutBody),
        dpi_);
    if (!body) return;

    auto emphasisSpec =
        ui::ApplicationFontSpec(language, ui::UiFontRole::ShortcutBody);
    emphasisSpec.weight = FW_SEMIBOLD;
    HFONT emphasis = ui::CreateFontHandle(emphasisSpec, dpi_);
    if (!emphasis) {
        DeleteObject(body);
        return;
    }
    HFONT hint = ui::CreateFontHandle(
        ui::ApplicationFontSpec(language, ui::UiFontRole::ShortcutHint),
        dpi_);
    if (!hint) {
        DeleteObject(emphasis);
        DeleteObject(body);
        return;
    }

    // Install replacement fonts before destroying old ones. Native EDIT,
    // ComboBox and STATIC controls retain HFONT handles via WM_SETFONT.
    const HFONT previousBody = font_;
    const HFONT previousEmphasis = semiboldFont_;
    const HFONT previousHint = hintFont_;
    font_ = body;
    semiboldFont_ = emphasis;
    hintFont_ = hint;

    const HWND controls[] = {
        keywordLabel_, keyword_, keywordHint_, nameLabel_, name_,
        targetLabel_, target_, browseFile_, browseFolder_, typeLabel_,
        type_, typeHint_, runtimeInputLabel_, runtimeInput_,
        runtimeInputHint_, testInputLabel_, testInput_, advancedToggle_,
        argumentsLabel_, arguments_, workdirLabel_, workdir_,
        browseWorkdir_, admin_, test_, save_, cancel_,
    };
    for (HWND control : controls) {
        if (control) {
            // All form labels use one body font. Only the Advanced header and
            // primary action retain a semantic semibold paint role.
            SendMessageW(control, WM_SETFONT,
                reinterpret_cast<WPARAM>(font_), repaint);
        }
    }

    for (HWND hintControl :
         {keywordHint_, typeHint_, runtimeInputHint_}) {
        if (hintControl) {
            SendMessageW(hintControl, WM_SETFONT,
                reinterpret_cast<WPARAM>(hintFont_), repaint);
        }
    }

    if (previousBody) DeleteObject(previousBody);
    if (previousEmphasis) DeleteObject(previousEmphasis);
    if (previousHint) DeleteObject(previousHint);
}

void ShortcutEditorDialog::ApplyLanguage() {
    UpdateWindowTitle();

    SetWindowTextW(
        keywordLabel_,
        T(L"快捷词 *",
          L"Keywords *"));
    SetWindowTextW(
        keywordHint_,
        T(L"（多个用逗号分隔，第一个优先级最高）",
          L"(comma-separated; first has priority)"));
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
        T(L"文件…",
          L"File…"));
    SetWindowTextW(
        browseFolder_,
        T(L"文件夹…",
          L"Folder…"));
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
        T(L"工作目录",
          L"Working directory"));
    SendMessageW(
        workdir_,
        EM_SETCUEBANNER,
        TRUE,
        reinterpret_cast<LPARAM>(
            T(L"留空时自动使用目标所在目录",
              L"Blank = target directory")));
    SetWindowTextW(
        browseWorkdir_,
        T(L"选择…",
          L"Browse…"));
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

    ui::ApplyNextComboBoxMetrics(
        type_,
        dpi_);
    ui::ApplyNextComboBoxMetrics(
        runtimeInput_,
        dpi_);

    UpdateAdvancedVisibility();
    UpdateTypeState();
    UpdateRuntimeInputHint();
    UpdateRuntimeTestVisibility();
}

void ShortcutEditorDialog::
UpdateWindowTitle() {
    if (!hwnd_) {
        return;
    }

    SetWindowTextW(
        hwnd_,
        commandId_.empty()
            ? T(L"新建快捷项",
                L"New shortcut")
            : T(L"编辑快捷项",
                L"Edit shortcut"));
}


void ShortcutEditorDialog::Layout(
    BOOL repaint) {

    if (!hwnd_) {
        return;
    }

    const auto moveControl =
        [&](HWND control,
            int x,
            int y,
            int width,
            int height) {
            if (!control) {
                return;
            }

            if (repaint) {
                MoveWindow(
                    control,
                    x,
                    y,
                    width,
                    height,
                    TRUE);
                return;
            }

            SetWindowPos(
                control,
                nullptr,
                x,
                y,
                width,
                height,
                SWP_NOZORDER |
                    SWP_NOACTIVATE |
                    SWP_NOREDRAW |
                    SWP_NOCOPYBITS);
        };

    RECT client{};
    GetClientRect(hwnd_, &client);

    const int margin = Scale(24);
    // 11pt at 144 DPI requires a 20-logical-pixel label slot.
    const int labelHeight =
        Scale(app_.SettingsData().language == Language::ZhCN ? 20 : 18);
    const int hintHeight =
        Scale(app_.SettingsData().language == Language::ZhCN ? 18 : 16);
    const int gap = Scale(8);
    const int formLabelGap = Scale(12);
    const int columnGap = Scale(18);
    const int controlRowHeight =
        Scale(kControlRowHeightLogical);
    const int fileButtonWidth = Scale(62);
    const int folderButtonWidth = Scale(78);
    const int contentWidth =
        client.right - margin * 2;

    // Native single-line Edit text is top-biased inside an arbitrarily tall
    // client rectangle. Fit the border to the active body font instead of
    // replacing the control or changing its IME/keyboard semantics.
    int editHeight = Scale(24);

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
                editHeight =
                    std::min(
                        controlRowHeight,
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

    const auto editTop =
        [&](int rowTop) {
            return rowTop +
                std::max(
                    0,
                    (controlRowHeight -
                     editHeight) / 2);
        };

    const auto rowTextTop =
        [&](int rowTop) {
            return rowTop +
                std::max(
                    0,
                    (controlRowHeight -
                     labelHeight) / 2);
        };
    // Label and hint STATICs paint from the top of their box, while the
    // closed Next ComboBox is taller than the nominal row and centers its
    // own text. Size each STATIC to its font's line box and center that on
    // the combo's real height so all three texts share one center line.
    const auto fontMetrics =
        [&](HFONT font) {
            TEXTMETRICW metrics{};
            if (HDC dc = GetDC(hwnd_)) {
                const HGDIOBJ previous =
                    font ? SelectObject(dc, font) : nullptr;
                if (!GetTextMetricsW(dc, &metrics)) {
                    metrics = {};
                }
                if (previous && previous != HGDI_ERROR) {
                    SelectObject(dc, previous);
                }
                ReleaseDC(hwnd_, dc);
            }
            return metrics;
        };
    const TEXTMETRICW labelMetrics =
        fontMetrics(font_);
    const TEXTMETRICW hintMetrics =
        fontMetrics(hintFont_ ? hintFont_ : font_);
    const int labelLineHeight =
        labelMetrics.tmHeight > 0
            ? static_cast<int>(labelMetrics.tmHeight)
            : labelHeight;
    const int hintLineHeight =
        hintMetrics.tmHeight > 0
            ? static_cast<int>(hintMetrics.tmHeight)
            : hintHeight;
    const auto comboCenterY =
        [&](HWND combo, int rowTop) {
            RECT bounds{};
            int height = controlRowHeight;
            if (combo && GetWindowRect(combo, &bounds) &&
                bounds.bottom > bounds.top) {
                height = static_cast<int>(
                    bounds.bottom - bounds.top);
            }
            return rowTop + height / 2;
        };

    int y = Scale(16);

    // Primary fields keep the familiar stacked-label layout.
    const int nameWidth =
        std::max(
            Scale(190),
            (contentWidth -
             columnGap) * 36 / 100);
    const int keywordLeft =
        margin +
        nameWidth +
        columnGap;
    const int keywordWidth =
        contentWidth -
        nameWidth -
        columnGap;

    moveControl(
        nameLabel_,
        margin,
        y,
        nameWidth,
        labelHeight);
    // The keyword hint rides on the label line, right after "Keywords *",
    // so the Name and Keywords columns end at the same height instead of
    // leaving an empty band under Name.
    int keywordLabelWidth = keywordWidth;
    if (HDC dc = GetDC(hwnd_)) {
        const HGDIOBJ previous =
            font_ ? SelectObject(dc, font_) : nullptr;
        wchar_t text[128]{};
        const int length = GetWindowTextW(
            keywordLabel_, text, static_cast<int>(std::size(text)));
        SIZE extent{};
        if (GetTextExtentPoint32W(dc, text, length, &extent)) {
            keywordLabelWidth = std::min(
                keywordWidth,
                static_cast<int>(extent.cx) + Scale(2));
        }
        if (previous && previous != HGDI_ERROR) {
            SelectObject(dc, previous);
        }
        ReleaseDC(hwnd_, dc);
    }
    moveControl(
        keywordLabel_,
        keywordLeft,
        y,
        keywordLabelWidth,
        labelHeight);

    // Align baselines: the hint uses the smaller hint font.
    const int keywordHintLeft =
        keywordLeft +
        keywordLabelWidth +
        Scale(4);
    const int keywordHintTop =
        y +
        std::max(
            0,
            static_cast<int>(
                labelMetrics.tmAscent -
                hintMetrics.tmAscent));
    moveControl(
        keywordHint_,
        keywordHintLeft,
        keywordHintTop,
        std::max(
            0,
            keywordLeft +
                keywordWidth -
                keywordHintLeft),
        hintLineHeight);
    y += Scale(20);

    const int identityEditTop =
        editTop(y);

    moveControl(
        name_,
        margin,
        identityEditTop,
        nameWidth,
        editHeight);
    moveControl(
        keyword_,
        keywordLeft,
        identityEditTop,
        keywordWidth,
        editHeight);
    y += controlRowHeight +
        Scale(kIdentityToTargetGapLogical);

    moveControl(
        targetLabel_,
        margin,
        y,
        contentWidth,
        labelHeight);
    y += Scale(20);

    const int targetWidth =
        contentWidth -
        fileButtonWidth -
        folderButtonWidth -
        gap * 2;
    const int targetEditTop =
        editTop(y);

    moveControl(
        target_,
        margin,
        targetEditTop,
        targetWidth,
        editHeight);
    moveControl(
        browseFile_,
        margin +
            targetWidth +
            gap,
        targetEditTop,
        fileButtonWidth,
        editHeight);
    moveControl(
        browseFolder_,
        margin +
            targetWidth +
            gap +
            fileButtonWidth +
            gap,
        targetEditTop,
        folderButtonWidth,
        editHeight);
    y += controlRowHeight +
        Scale(8);

    // Inline form rows share one right-aligned label column. This keeps short
    // labels such as "Icon" visually attached to the value column instead of
    // leaving a large empty band between label and control.
    const int formLabelWidth =
        Scale(
            app_.SettingsData().language ==
                    Language::ZhCN
                ? 76
                : 108);
    const int formFieldLeft =
        margin +
        formLabelWidth +
        formLabelGap;
    const int formFieldWidth =
        client.right -
        margin -
        formFieldLeft;
    const int typeWidth =
        std::max(
            ui::MeasureNextComboBoxPreferredWidth(
                type_,
                dpi_,
                kInlineComboMinWidthLogical,
                kInlineComboMaxWidthLogical),
            ui::MeasureNextComboBoxPreferredWidth(
                runtimeInput_,
                dpi_,
                kInlineComboMinWidthLogical,
                kInlineComboMaxWidthLogical));

    // Target type and Runtime input deliberately share one value-column width,
    // while the shared Next ComboBox owns text/chrome measurement.

    // Inline combos sit at text-field height and on the text-field row
    // line, instead of the taller shared default used by Settings cards.
    ui::SetNextComboBoxClosedHeight(type_, editHeight);
    ui::SetNextComboBoxClosedHeight(runtimeInput_, editHeight);

    ui::MoveNextComboBox(
        type_,
        formFieldLeft,
        editTop(y),
        typeWidth,
        dpi_,
        repaint);
    const int typeCenterY =
        comboCenterY(type_, editTop(y));
    moveControl(
        typeLabel_,
        margin,
        typeCenterY - labelLineHeight / 2,
        formLabelWidth,
        labelLineHeight);

    const int typeHintLeft =
        formFieldLeft +
        typeWidth +
        gap;

    moveControl(
        typeHint_,
        typeHintLeft,
        typeCenterY - hintLineHeight / 2,
        client.right -
            margin -
            typeHintLeft,
        hintLineHeight);
    // Center the divider on the visible gap between the two combos'
    // real edges rather than on nominal row arithmetic.
    const int typeComboBottom =
        editTop(y) + editHeight;
    y += controlRowHeight +
        Scale(6);
    y += Scale(12);
    runtimeSeparatorY_ =
        (typeComboBottom + editTop(y)) / 2;

    const int runtimeWidth =
        typeWidth;

    ui::MoveNextComboBox(
        runtimeInput_,
        formFieldLeft,
        editTop(y),
        runtimeWidth,
        dpi_,
        repaint);
    const int runtimeCenterY =
        comboCenterY(runtimeInput_, editTop(y));
    moveControl(
        runtimeInputLabel_,
        margin,
        runtimeCenterY - labelLineHeight / 2,
        formLabelWidth,
        labelLineHeight);

    const int runtimeHintLeft =
        formFieldLeft +
        runtimeWidth +
        gap;

    moveControl(
        runtimeInputHint_,
        runtimeHintLeft,
        runtimeCenterY - hintLineHeight / 2,
        client.right -
            margin -
            runtimeHintLeft,
        hintLineHeight);
    y += controlRowHeight +
        Scale(6);

    if (SelectedRuntimeInputMode() !=
        RuntimeInputMode::None) {
        y += Scale(kRuntimeTestTopGapLogical);
        moveControl(
            testInputLabel_,
            margin,
            rowTextTop(y),
            formLabelWidth,
            labelHeight);
        moveControl(
            testInput_,
            formFieldLeft,
            editTop(y),
            formFieldWidth,
            editHeight);
        y += Scale(
            kRuntimeTestExtraHeightLogical);
    }

    moveControl(
        advancedToggle_,
        margin,
        y,
        contentWidth,
        controlRowHeight);
    y += controlRowHeight +
        Scale(6);

    if (advancedExpanded_) {
        const int advancedRowAdvance =
            controlRowHeight +
            Scale(6);
        const int advancedRightInset =
            Scale(
                kAdvancedRightInsetLogical);
        const int advancedFieldWidth =
            std::max(
                0,
                formFieldWidth -
                    advancedRightInset);
        const int advancedFieldRight =
            client.right -
            margin -
            advancedRightInset;

        moveControl(
            argumentsLabel_,
            margin,
            rowTextTop(y),
            formLabelWidth,
            labelHeight);
        moveControl(
            arguments_,
            formFieldLeft,
            editTop(y),
            advancedFieldWidth,
            editHeight);
        y += advancedRowAdvance;

        const int workdirButtonWidth =
            Scale(68);
        const int workdirEditWidth =
            advancedFieldWidth -
            workdirButtonWidth -
            gap;

        moveControl(
            workdirLabel_,
            margin,
            rowTextTop(y),
            formLabelWidth,
            labelHeight);
        moveControl(
            workdir_,
            formFieldLeft,
            editTop(y),
            workdirEditWidth,
            editHeight);
        moveControl(
            browseWorkdir_,
            advancedFieldRight -
                workdirButtonWidth,
            editTop(y),
            workdirButtonWidth,
            editHeight);
        y += advancedRowAdvance;

        y += Scale(
            kAdminTopGapLogical);

        // BS_OWNERDRAW has no meaningful BCM_GETIDEALSIZE for a custom
        // checkbox+label. The old ideal-size branch could clip the label's
        // clickable HWND even while the text appeared next to the glyph.
        int adminWidth = advancedFieldWidth;
        if (HDC dc = GetDC(hwnd_)) {
            const HGDIOBJ previous = font_ ? SelectObject(dc, font_) : nullptr;
            wchar_t label[128]{};
            const int length = GetWindowTextW(
                admin_, label, static_cast<int>(std::size(label)));
            SIZE extent{};
            if (GetTextExtentPoint32W(dc, label, length, &extent)) {
                // Match PaintAdminCheckbox: 2px inset + 16px glyph +
                // label gap + text + 2px right inset, with a small hit slop.
                adminWidth = std::min(advancedFieldWidth,
                    static_cast<int>(extent.cx) +
                        Scale(2 + 16 + kAdminLabelGapLogical + 2 + 6));
            }
            if (previous) SelectObject(dc, previous);
            ReleaseDC(hwnd_, dc);
        }

        moveControl(
            admin_,
            formFieldLeft,
            y,
            adminWidth,
            Scale(26));
    }

    const int buttonWidth =
        Scale(88);
    const int buttonHeight =
        Scale(kFooterButtonHeightLogical);
    const int buttonY =
        client.bottom -
        Scale(kFooterBottomMarginLogical) -
        buttonHeight;

    footerSeparatorY_ =
        buttonY -
        Scale(kFooterSeparatorGapLogical);

    moveControl(
        test_,
        margin,
        buttonY,
        buttonWidth,
        buttonHeight);
    moveControl(
        cancel_,
        client.right -
            margin -
            buttonWidth,
        buttonY,
        buttonWidth,
        buttonHeight);
    moveControl(
        save_,
        client.right -
            margin -
            buttonWidth * 2 -
            gap,
        buttonY,
        buttonWidth,
        buttonHeight);

    if (repaint) {
        InvalidateRect(
            hwnd_,
            nullptr,
            FALSE);
    }
}

void ShortcutEditorDialog::DrawEditorChrome(
    HDC dc) const {
    if (!dc ||
        !hwnd_) {
        return;
    }

    RECT client{};
    GetClientRect(hwnd_, &client);

    const auto& palette =
        ui::kApplicationPalette;

    HBRUSH windowBrush =
        backgroundBrush_
            ? backgroundBrush_
            : GetSysColorBrush(
                  COLOR_WINDOW);

    FillRect(
        dc,
        &client,
        windowBrush);

    if (footerSeparatorY_ > 0) {
        RECT footer{
            client.left,
            footerSeparatorY_ + 1,
            client.right,
            client.bottom,
        };

        HBRUSH footerBrush =
            footerBrush_
                ? footerBrush_
                : windowBrush;
        FillRect(
            dc,
            &footer,
            footerBrush);
    }

    HPEN separatorPen =
        CreatePen(
            PS_SOLID,
            1,
            palette.separator);
    HGDIOBJ oldPen =
        SelectObject(
            dc,
            separatorPen);

    const int margin =
        Scale(24);

    if (runtimeSeparatorY_ > 0) {
        MoveToEx(
            dc,
            margin,
            runtimeSeparatorY_,
            nullptr);
        LineTo(
            dc,
            client.right - margin,
            runtimeSeparatorY_);
    }

    if (footerSeparatorY_ > 0) {
        MoveToEx(
            dc,
            0,
            footerSeparatorY_,
            nullptr);
        LineTo(
            dc,
            client.right,
            footerSeparatorY_);
    }

    SelectObject(
        dc,
        oldPen);
    DeleteObject(
        separatorPen);
}

void ShortcutEditorDialog::DrawAdvancedHeader(
    const DRAWITEMSTRUCT& draw) const {
    if (!hwnd_ || draw.hwndItem != advancedToggle_) return;
    const int width = draw.rcItem.right - draw.rcItem.left;
    const int height = draw.rcItem.bottom - draw.rcItem.top;
    if (width <= 0 || height <= 0) return;

    // Paint label and entire separator into one completed off-screen frame.
    // The previous on-screen FillRect preceded the line and exposed a
    // blank separator when Advanced was clicked repeatedly.
    HDC memory = CreateCompatibleDC(draw.hDC);
    HBITMAP bitmap = memory
        ? CreateCompatibleBitmap(draw.hDC, width, height) : nullptr;
    HGDIOBJ oldBitmap = memory && bitmap ? SelectObject(memory, bitmap) : nullptr;
    const bool buffered = memory && bitmap && oldBitmap && oldBitmap != HGDI_ERROR;
    HDC dc = buffered ? memory : draw.hDC;
    RECT rect = buffered ? RECT{0, 0, width, height} : draw.rcItem;
    const auto& palette = ui::kApplicationPalette;
    const bool disabled = (draw.itemState & ODS_DISABLED) != 0;

    HBRUSH background = CreateSolidBrush(palette.windowBackground);
    FillRect(dc, &rect, background ? background : GetSysColorBrush(COLOR_WINDOW));
    if (background) DeleteObject(background);
    wchar_t label[128]{};
    GetWindowTextW(draw.hwndItem, label, static_cast<int>(std::size(label)));
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, disabled ? palette.mutedText : palette.text);
    const HGDIOBJ oldFont =
        SelectObject(dc, semiboldFont_ ? semiboldFont_ : font_);
    RECT text = rect;
    text.left += Scale(2);
    text.right -= Scale(2);
    DrawTextW(dc, label, -1, &text, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    SIZE extent{};
    GetTextExtentPoint32W(dc, label, lstrlenW(label), &extent);
    const int lineY = (rect.top + rect.bottom) / 2;
    const int lineStart = std::min(static_cast<int>(rect.right),
        static_cast<int>(rect.left) + Scale(2) +
        static_cast<int>(extent.cx) + Scale(14));
    HPEN separator = CreatePen(PS_SOLID, 1, palette.separator);
    if (separator) {
        const HGDIOBJ oldPen = SelectObject(dc, separator);
        MoveToEx(dc, lineStart, lineY, nullptr);
        LineTo(dc, rect.right, lineY);
        SelectObject(dc, oldPen);
        DeleteObject(separator);
    }
    // Native BUTTON retains click and Space. Omit the classic dotted focus.
    SelectObject(dc, oldFont);
    if (buffered) {
        BitBlt(draw.hDC, draw.rcItem.left, draw.rcItem.top,
            width, height, memory, 0, 0, SRCCOPY);
        SelectObject(memory, oldBitmap);
    }
    if (bitmap) DeleteObject(bitmap);
    if (memory) DeleteDC(memory);
}

void ShortcutEditorDialog::DrawActionButton(
    const DRAWITEMSTRUCT& item) const {
    if (!hwnd_ ||
        !item.hwndItem) {
        return;
    }

    const auto& palette =
        ui::kApplicationPalette;
    const UINT id =
        static_cast<UINT>(
            item.CtlID);

    const bool primary =
        id == kIdSave;
    const bool footer =
        id == kIdTest ||
        id == kIdSave ||
        id == kIdCancel;
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
        disabled
            ? palette.mutedText
            : palette.text;

    if (primary) {
        if (disabled) {
            fillColor =
                palette.accentBackground;
            borderColor =
                palette.frame;
            textColor =
                palette.mutedText;
        } else {
            fillColor =
                pressed
                    ? RGB(0, 96, 180)
                    : palette.accent;
            borderColor =
                fillColor;
            textColor =
                RGB(255, 255, 255);
        }
    } else if (pressed &&
               !disabled) {
        fillColor =
            palette.pressedBackground;
        borderColor =
            palette.separator;
    }

    RECT rect =
        item.rcItem;

    RECT surface =
        rect;
    InflateRect(
        &surface,
        -1,
        -1);

    // Same smooth outline and radius as the neighbouring text fields.
    ui::PaintSmoothRoundedFrame(
        item.hDC,
        rect,
        surface,
        Scale(ui::kControlCornerRadiusLogical),
        footer
            ? palette.bottomBackground
            : palette.windowBackground,
        borderColor,
        fillColor);

    wchar_t text[128]{};
    GetWindowTextW(
        item.hwndItem,
        text,
        static_cast<int>(
            _countof(text)));

    SetBkMode(
        item.hDC,
        TRANSPARENT);
    SetTextColor(
        item.hDC,
        textColor);

    HGDIOBJ oldFont =
        SelectObject(
            item.hDC,
            primary &&
                    semiboldFont_
                ? semiboldFont_
                : font_);

    RECT textRect =
        surface;
    InflateRect(
        &textRect,
        -Scale(8),
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

    // Honor the window's keyboard-cue state like native buttons: the dotted
    // rectangle belongs to keyboard navigation, not to a mouse click.
    if ((item.itemState & ODS_FOCUS) != 0 &&
        (item.itemState & ODS_NOFOCUSRECT) == 0) {
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

int ShortcutEditorDialog::
DesiredClientHeight() const {
    const int controlRowHeight =
        Scale(kControlRowHeightLogical);

    int y = Scale(16);

    y += Scale(20);
    y += controlRowHeight +
        Scale(kIdentityToTargetGapLogical);

    y += Scale(20);
    y += controlRowHeight +
        Scale(8);

    y += controlRowHeight +
        Scale(6);

    y += Scale(12);
    y += controlRowHeight +
        Scale(6);

    if (SelectedRuntimeInputMode() !=
        RuntimeInputMode::None) {
        y += Scale(kRuntimeTestTopGapLogical);
        y += Scale(kRuntimeTestExtraHeightLogical);
    }

    const int advancedTop = y;
    y += controlRowHeight +
        Scale(6);

    int contentBottom =
        advancedTop +
        controlRowHeight;

    if (advancedExpanded_) {
        const int advancedRowAdvance =
            controlRowHeight +
            Scale(6);

        y += advancedRowAdvance * 2;

        contentBottom =
            y +
            Scale(
                kAdminTopGapLogical) +
            Scale(26);
    }

    return contentBottom +
        Scale(
            kContentFooterGapLogical +
            kFooterSeparatorGapLogical +
            kFooterButtonHeightLogical +
            kFooterBottomMarginLogical);
}

void ShortcutEditorDialog::ResizeForContent() {
    if (!hwnd_) {
        return;
    }

    RECT window{};
    RECT client{};

    GetWindowRect(
        hwnd_,
        &window);
    GetClientRect(
        hwnd_,
        &client);

    const int nonClientHeight =
        std::max(
            0,
            static_cast<int>(
                (window.bottom -
                 window.top) -
                (client.bottom -
                 client.top)));

    const int desiredOuterHeight =
        DesiredClientHeight() +
        nonClientHeight;

    SetWindowPos(
        hwnd_,
        nullptr,
        0,
        0,
        Scale(kEditorWidthLogical),
        desiredOuterHeight,
        SWP_NOMOVE |
            SWP_NOZORDER |
            SWP_NOACTIVATE |
            SWP_NOREDRAW);
}

void ShortcutEditorDialog::RefreshDynamicLayout() {
    if (!hwnd_) {
        return;
    }

    // Resizing the dialog synchronously sends WM_SIZE. Suppress that one
    // intermediate layout pass and settle the complete dynamic form once,
    // after the final client size is known.
    dynamicLayoutInProgress_ = true;
    ResizeForContent();
    dynamicLayoutInProgress_ = false;

    // Keep every child position change paint-free while the dynamic form is
    // settling. This includes the shared ComboBox wrapper, whose quiet path
    // avoids native LISTBOX bookkeeping and pending invalidations.
    Layout(FALSE);

    // The parent WM_PAINT fully fills the editor background, so an erase pass
    // is unnecessary and creates a visible white/pressed/white transition in
    // the Advanced separator during rapid toggles. Commit one final frame.
    RedrawWindow(
        hwnd_,
        nullptr,
        nullptr,
        RDW_INVALIDATE |
            RDW_NOERASE |
            RDW_ALLCHILDREN |
            RDW_UPDATENOW);
}

void ShortcutEditorDialog::UpdateAdvancedVisibility() {
    for (HWND control : {
             argumentsLabel_,
             arguments_,
             workdirLabel_,
             workdir_,
             browseWorkdir_,
             admin_}) {
        if (!control) {
            continue;
        }

        const bool currentlyVisible =
            (GetWindowLongPtrW(
                 control,
                 GWL_STYLE) &
             WS_VISIBLE) != 0;

        if (currentlyVisible ==
            advancedExpanded_) {
            continue;
        }

        SetWindowPos(
            control,
            nullptr,
            0,
            0,
            0,
            0,
            SWP_NOMOVE |
                SWP_NOSIZE |
                SWP_NOZORDER |
                SWP_NOACTIVATE |
                SWP_NOREDRAW |
                (advancedExpanded_
                     ? SWP_SHOWWINDOW
                     : SWP_HIDEWINDOW));
    }

    // Do not toggle WM_SETREDRAW on this visible child: USER32 temporarily
    // changes its visible state, exposing a blank section separator on rapid
    // clicks. WM_SETTEXT only invalidates the button; RefreshDynamicLayout
    // commits the final header and geometry synchronously immediately after.
    if (advancedToggle_) {
        SetWindowTextW(
            advancedToggle_,
            advancedExpanded_
                ? T(L"▾ 高级选项",
                    L"▾ Advanced")
                : T(L"▸ 高级选项",
                    L"▸ Advanced"));
    }
}

void ShortcutEditorDialog::UpdateRuntimeTestVisibility() {
    const bool visible =
        SelectedRuntimeInputMode() !=
        RuntimeInputMode::None;
    // This also runs while the modal parent is hidden. IsWindowVisible
    // would report FALSE for every child regardless of its own WS_VISIBLE
    // bit, leaving a supposedly hidden test field visible on first reveal.
    const bool labelVisible =
        (GetWindowLongPtrW(testInputLabel_, GWL_STYLE) & WS_VISIBLE) != 0;
    const bool inputVisible =
        (GetWindowLongPtrW(testInput_, GWL_STYLE) & WS_VISIBLE) != 0;

    if (labelVisible == visible &&
        inputVisible == visible) {
        return;
    }

    // Stage both visibility changes without intermediate child paints.
    // RefreshDynamicLayout commits one fully laid-out parent/child frame.
    const UINT flags = SWP_NOMOVE | SWP_NOSIZE |
        SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOREDRAW |
        (visible ? SWP_SHOWWINDOW : SWP_HIDEWINDOW);
    SetWindowPos(testInputLabel_, nullptr, 0, 0, 0, 0, flags);
    SetWindowPos(testInput_, nullptr, 0, 0, 0, 0, flags);

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

    const std::wstring target =
        TrimWide(
            ControlText(target_));

    if (selected <= 0 &&
        target.empty()) {
        // InferShortcutCommandType() intentionally keeps Application as its
        // internal fallback for an empty target. Do not expose that fallback
        // as if auto-detection had already succeeded in a brand-new editor.
        SetWindowTextW(
            typeHint_,
            T(L"等待输入目标",
              L"Waiting for target"));

        UpdateRuntimeInputHint();
        return;
    }

    const CommandType type =
        selected <= 0
            ? InferShortcutCommandType(
                  target)
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

    // An empty auto-detected target still maps to Application internally;
    // don't promise argument-appending until the user enters a real target.
    if (SendMessageW(type_, CB_GETCURSEL, 0, 0) <= 0 &&
        preview.target.empty()) {
        SetWindowTextW(runtimeInputHint_,
            mode == RuntimeInputMode::UrlEncoded
                ? T(L"编码后用 {input} 指定位置。",
                    L"Encode; use {input}.")
                : T(L"原样传递；可用 {input} 定位。",
                    L"Pass unchanged; use {input}."));
        return;
    }

    if (!CanAcceptRuntimeInput(
            preview)) {
        SetWindowTextW(
            runtimeInputHint_,
            T(L"用 {input} 指定位置。",
              L"Use {input} for placement."));
        return;
    }

    if (mode ==
        RuntimeInputMode::UrlEncoded) {
        SetWindowTextW(
            runtimeInputHint_,
            T(L"UTF-8 编码后替换 {input}。",
              L"UTF-8 encode into {input}."));
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
            T(L"追加到参数，或用 {input} 定位。",
              L"Append or use {input}."));
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
    UpdateWindowTitle();

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
        it->runAsAdmin;

    UpdateAdvancedVisibility();
    UpdateTypeState();
    UpdateRuntimeTestVisibility();
}

void ShortcutEditorDialog::BeginNew(
    const Command* seed) {
    commandId_.clear();
    UpdateWindowTitle();

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
        altrun::ui::ShowMessage(
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
        altrun::ui::ShowMessage(
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
            altrun::ui::ShowMessage(
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
        altrun::ui::ShowMessage(
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
    CloseWindow();
    return true;
}

void ShortcutEditorDialog::Test() {
    const Command command =
        CollectCommand();

    if (command.target.empty()) {
        altrun::ui::ShowCompactNotice(
            hwnd_,
            T(L"请先填写目标。",
              L"Enter a target first."),
            T(L"测试运行",
              L"Test"),
            app_.SettingsData().language == Language::ZhCN);
        return;
    }

    if (command.runtimeInputMode !=
            RuntimeInputMode::None &&
        !CanAcceptRuntimeInput(
            command)) {
        altrun::ui::ShowMessage(
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
        altrun::ui::ShowMessage(
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
    const std::wstring current =
        TrimWide(
            ControlText(target_));

    const COMDLG_FILTERSPEC filters[] = {
        {
            T(L"程序和快捷方式",
              L"Programs and shortcuts"),
            L"*.exe;*.lnk;*.bat;*.cmd;*.com;*.ps1;*.url",
        },
        {
            T(L"所有文件",
              L"All files"),
            L"*.*",
        },
    };

    std::wstring selected;
    const PickerResult modern =
        PickFileModern(
            hwnd_,
            T(L"选择目标文件",
              L"Choose target file"),
            filters,
            static_cast<UINT>(
                _countof(filters)),
            current,
            selected);

    if (modern ==
        PickerResult::Cancelled) {
        return;
    }

    if (modern ==
        PickerResult::Selected) {
        SetWindowTextW(
            target_,
            selected.c_str());

        SendMessageW(
            type_,
            CB_SETCURSEL,
            0,
            0);

        UpdateTypeState();
        MaybeAutoFillName();
        return;
    }

    // Compatibility fallback for systems where the modern shell dialog is
    // unavailable or COM cannot provide it in the current apartment.
    std::array<wchar_t, 32768>
        file{};

    if (!current.empty() &&
        current.size() <
            file.size()) {
        std::copy(
            current.begin(),
            current.end(),
            file.begin());
    }

    const wchar_t* filter =
        app_.SettingsData().language ==
                Language::ZhCN
            ? L"程序和快捷方式\0*.exe;*.lnk;*.bat;*.cmd;*.com;*.ps1;*.url\0所有文件\0*.*\0\0"
            : L"Programs and shortcuts\0*.exe;*.lnk;*.bat;*.cmd;*.com;*.ps1;*.url\0All files\0*.*\0\0";

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
    open.lpstrTitle =
        T(L"选择目标文件",
          L"Choose target file");
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
    const std::wstring current =
        TrimWide(
            ControlText(target_));

    std::wstring selected;
    const PickerResult modern =
        PickFolderModern(
            hwnd_,
            T(L"选择目标文件夹",
              L"Choose target folder"),
            current,
            selected);

    if (modern ==
        PickerResult::Cancelled) {
        return;
    }

    if (modern ==
        PickerResult::Selected) {
        SetWindowTextW(
            target_,
            selected.c_str());

        SendMessageW(
            type_,
            CB_SETCURSEL,
            0,
            0);

        UpdateTypeState();
        MaybeAutoFillName();
        return;
    }

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
    std::wstring current =
        TrimWide(
            ControlText(workdir_));

    if (current.empty()) {
        current =
            TrimWide(
                ControlText(target_));
    }

    std::wstring selected;
    const PickerResult modern =
        PickFolderModern(
            hwnd_,
            T(L"选择工作目录",
              L"Choose working directory"),
            current,
            selected);

    if (modern ==
        PickerResult::Cancelled) {
        return;
    }

    if (modern ==
        PickerResult::Selected) {
        SetWindowTextW(
            workdir_,
            selected.c_str());
        return;
    }

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

    const auto dismissComboFocus =
        [&]() {
            const HWND focused =
                GetFocus();

            if (focused == type_ ||
                focused == runtimeInput_) {
                SetFocus(hwnd_);
            }
        };

    if (ui::HandleFocusCueMessage(hwnd_, message, wParam)) {
        return 0;
    }

    switch (message) {
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_MBUTTONDOWN:
        // Match Settings: clicking the dialog surface should dismiss the
        // persistent native ComboBox selection/focus highlight.
        dismissComboFocus();
        break;

    case WM_PARENTNOTIFY:
        if (LOWORD(wParam) ==
                WM_LBUTTONDOWN ||
            LOWORD(wParam) ==
                WM_RBUTTONDOWN ||
            LOWORD(wParam) ==
                WM_MBUTTONDOWN) {
            // Do not steal focus during an interactive child control's mouse
            // down. Doing so can cancel the BUTTON down/up sequence before
            // BN_CLICKED is emitted (most visible on Advanced immediately
            // after using a ComboBox). Native interactive controls naturally
            // take focus themselves. We only dismiss a lingering ComboBox
            // focus when the click is on a passive STATIC surface.
            POINT point{};
            GetCursorPos(
                &point);
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
        }
        break;

    case WM_NCLBUTTONDOWN:
    case WM_NCRBUTTONDOWN:
    case WM_NCMBUTTONDOWN:
        dismissComboFocus();
        break;

    case WM_DPICHANGED: {
        const UINT nextDpi = HIWORD(wParam);
        if (nextDpi == 0) return 0;
        dpi_ = nextDpi;

        // WM_SIZE is synchronous during SetWindowPos: defer child layout
        // until fonts and the final content-dependent height are ready.
        const auto* suggested = reinterpret_cast<const RECT*>(lParam);
        if (suggested) {
            dynamicLayoutInProgress_ = true;
            SetWindowPos(hwnd_, nullptr, suggested->left, suggested->top,
                suggested->right - suggested->left,
                suggested->bottom - suggested->top,
                SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOREDRAW);
            dynamicLayoutInProgress_ = false;
        }
        ApplyFonts(FALSE);
        RefreshDynamicLayout();
        return 0;
    }

    case WM_SIZE:
        if (!dynamicLayoutInProgress_) {
            Layout();
        }
        return 0;

    case WM_ERASEBKGND:
        // WM_PAINT fills the editor once; an erase-time chrome pass
        // produces a second incomplete frame during rapid resizing.
        return TRUE;

    case WM_PAINT: {
        PAINTSTRUCT paint{};
        HDC dc =
            BeginPaint(
                hwnd_,
                &paint);
        DrawEditorChrome(dc);
        EndPaint(
            hwnd_,
            &paint);
        return 0;
    }

    case WM_CTLCOLORBTN: {
        // Owner-drawn buttons erase with this brush. The default (button
        // face gray) never matches the editor and shows between erase and
        // WM_DRAWITEM; the surface each button sits on is at worst invisible.
        const HWND control =
            reinterpret_cast<HWND>(lParam);
        const bool footer =
            control == test_ ||
            control == save_ ||
            control == cancel_;
        const HBRUSH surface =
            footer && footerBrush_
                ? footerBrush_
                : backgroundBrush_;
        return reinterpret_cast<LRESULT>(
            surface
                ? surface
                : GetSysColorBrush(
                      COLOR_WINDOW));
    }

    case WM_CTLCOLORSTATIC: {
        HDC dc =
            reinterpret_cast<HDC>(
                wParam);
        HWND control =
            reinterpret_cast<HWND>(
                lParam);

        const bool hint =
            control == keywordHint_ ||
            control == typeHint_ ||
            control == runtimeInputHint_;

        SetBkMode(
            dc,
            TRANSPARENT);
        SetTextColor(
            dc,
            hint
                ? kEditorHintText
                : ui::kApplicationPalette
                      .text);

        return reinterpret_cast<LRESULT>(
            backgroundBrush_
                ? backgroundBrush_
                : GetSysColorBrush(
                      COLOR_WINDOW));
    }

    case WM_CTLCOLORLISTBOX:
        return ui::ColorNextComboBoxList(
            reinterpret_cast<HDC>(
                wParam));

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
        const auto* draw =
            reinterpret_cast<
                DRAWITEMSTRUCT*>(
                    lParam);

        if (!draw) {
            break;
        }

        if (draw->CtlType ==
                ODT_COMBOBOX) {
            ui::DrawNextComboBoxItem(
                *draw,
                dpi_);
            return TRUE;
        }

        const UINT id =
            static_cast<UINT>(
                draw->CtlID);

        if (id ==
            kIdAdvancedToggle) {
            DrawAdvancedHeader(
                *draw);
            return TRUE;
        }

        if (id == kIdAdmin) {
            PaintAdminCheckbox(*draw, font_, dpi_);
            return TRUE;
        }

        if (id == kIdBrowseFile ||
            id == kIdBrowseFolder ||
            id == kIdBrowseWorkdir ||
            id == kIdTest ||
            id == kIdSave ||
            id == kIdCancel) {
            DrawActionButton(
                *draw);
            return TRUE;
        }
        break;
    }

    case WM_COMMAND: {
        const UINT id =
            LOWORD(wParam);
        const UINT notify =
            HIWORD(wParam);
        HWND commandControl =
            reinterpret_cast<HWND>(
                lParam);

        if (id == kIdType ||
            id == kIdRuntimeInput) {
            ui::RefreshNextComboBoxState(
                commandControl,
                notify);
        }

        switch (id) {
        case kIdKeyword:
            if (notify ==
                    EN_SETFOCUS &&
                keywordInitialCaretPending_) {
                keywordInitialCaretPending_ =
                    false;

                const int length =
                    GetWindowTextLengthW(
                        keyword_);

                SendMessageW(
                    keyword_,
                    EM_SETSEL,
                    static_cast<WPARAM>(
                        length),
                    static_cast<LPARAM>(
                        length));
                SendMessageW(
                    keyword_,
                    EM_SCROLLCARET,
                    0,
                    0);
            } else if (
                notify ==
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
                ui::HideFocusCues(hwnd_);
            }
            return 0;

        case kIdBrowseFolder:
            if (HIWORD(wParam) ==
                BN_CLICKED) {
                BrowseTargetFolder();
                ui::HideFocusCues(hwnd_);
            }
            return 0;

        case kIdArguments:
        case kIdWorkdir:
            if (HIWORD(wParam) ==
                EN_CHANGE) {
                UpdateRuntimeInputHint();
            }
            return 0;

        case kIdAdvancedToggle: {
            const bool toggleActivated =
                notify == BN_CLICKED ||
                notify ==
                    BN_DOUBLECLICKED;

            if (toggleActivated) {
                ToggleAdvanced();
            }
            return 0;
        }

        case kIdBrowseWorkdir:
            if (HIWORD(wParam) ==
                BN_CLICKED) {
                BrowseWorkingDirectory();
                ui::HideFocusCues(hwnd_);
            }
            return 0;

        case kIdAdmin:
            // Owner-drawn BUTTONs report the second quick mouse click as
            // BN_DOUBLECLICKED, not necessarily BN_CLICKED. Treat both as
            // exactly one activation, just like the Advanced disclosure.
            if (notify == BN_CLICKED || notify == BN_DOUBLECLICKED) {
                SetChecked(admin_, !IsChecked(admin_));
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
                CloseWindow();
            }
            return 0;

        default:
            break;
        }
        break;
    }

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
