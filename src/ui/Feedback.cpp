#include "Feedback.hpp"
#include "../ResourceIds.h"

#include <commctrl.h>
#include <mmsystem.h>
#include <iterator>
#include <string>

namespace altrun::ui {
namespace {
FeedbackPolicy policy;

HRESULT ShowNativeDialog(const TASKDIALOGCONFIG& config, int& result) {
    // Activate Common Controls v6 only for this dialog. The launcher's frozen
    // native controls keep their existing process activation context and theme.
    ACTCTXW activation{};
    activation.cbSize = sizeof(activation);
    activation.dwFlags = ACTCTX_FLAG_HMODULE_VALID | ACTCTX_FLAG_RESOURCE_NAME_VALID;
    activation.hModule = GetModuleHandleW(nullptr);
    activation.lpResourceName = MAKEINTRESOURCEW(IDR_FEEDBACK_CONTEXT);
    const HANDLE context = CreateActCtxW(&activation);
    if (context == INVALID_HANDLE_VALUE) return E_FAIL;
    ULONG_PTR cookie{};
    HRESULT status = E_FAIL;
    if (ActivateActCtx(context, &cookie)) {
        const HMODULE controls = LoadLibraryExW(L"comctl32.dll", nullptr,
                                                LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (controls) {
            using ShowDialog = HRESULT (WINAPI*)(const TASKDIALOGCONFIG*, int*, int*, BOOL*);
            const auto show = reinterpret_cast<ShowDialog>(GetProcAddress(controls, "TaskDialogIndirect"));
            if (show) status = show(&config, &result, nullptr, nullptr);
            FreeLibrary(controls);
        }
        DeactivateActCtx(0, cookie);
    }
    ReleaseActCtx(context);
    return status;
}

HRESULT CALLBACK OnDialog(HWND window, UINT notification, WPARAM, LPARAM,
                          LONG_PTR context) {
    if (notification == TDN_CREATED) {
        const auto flags = static_cast<UINT>(context);
        if ((flags & MB_TOPMOST) != 0) {
            SetWindowPos(window, HWND_TOPMOST, 0, 0, 0, 0,
                         SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        }
        if ((flags & MB_SETFOREGROUND) != 0) SetForegroundWindow(window);
        if ((flags & MB_ICONMASK) == MB_ICONERROR) PlayFeedback(FeedbackCue::Failure);
    }
    return S_OK;
}
} // namespace

void SetFeedbackEnabled(bool enabled) {
    policy.SetEnabled(enabled);
    if (!enabled) PlaySoundW(nullptr, nullptr, 0);
}

void PlayFeedback(FeedbackCue cue) {
    if (!policy.Accept(cue, GetTickCount64())) return;
    // Use the authorized original ALTRun Popup.wav for every accepted
    // application feedback event. The existing policy still owns when a cue is
    // allowed; a new cue replaces the previous one instead of queueing audio.
    PlaySoundW(MAKEINTRESOURCEW(IDW_CLASSIC_POPUP),
               GetModuleHandleW(nullptr), SND_RESOURCE | SND_ASYNC | SND_NODEFAULT);
}

int ShowMessage(HWND owner, const wchar_t* text, const wchar_t* title, UINT flags) {
    TASKDIALOGCONFIG config{};
    config.cbSize = sizeof(config);
    config.hwndParent = owner;
    config.dwFlags = TDF_POSITION_RELATIVE_TO_WINDOW | TDF_ALLOW_DIALOG_CANCELLATION;
    config.pszWindowTitle = title;
    config.pszContent = text;
    config.pfCallback = OnDialog;
    config.lpCallbackData = static_cast<LONG_PTR>(flags);
    const UINT kind = flags & MB_TYPEMASK;
    if (kind == MB_YESNO || kind == MB_YESNOCANCEL) {
        config.dwCommonButtons = TDCBF_YES_BUTTON | TDCBF_NO_BUTTON;
        if (kind == MB_YESNOCANCEL) config.dwCommonButtons |= TDCBF_CANCEL_BUTTON;
        config.nDefaultButton = (flags & MB_DEFMASK) == MB_DEFBUTTON2 ? IDNO : IDYES;
    } else if (kind == MB_OKCANCEL) {
        config.dwCommonButtons = TDCBF_OK_BUTTON | TDCBF_CANCEL_BUTTON;
        config.nDefaultButton = (flags & MB_DEFMASK) == MB_DEFBUTTON2 ? IDCANCEL : IDOK;
    } else {
        config.dwCommonButtons = TDCBF_OK_BUTTON;
        config.nDefaultButton = IDOK;
    }
    // Do not set a stock TaskDialog icon: stock warning/error icons also request
    // OS sounds independently of the user's application sound preference.
    int result = IDCANCEL;
    if (FAILED(ShowNativeDialog(config, result))) {
        // Preserve a readable failure path if Common Controls cannot create a dialog.
        return MessageBoxW(owner, text, title, flags & ~MB_ICONMASK);
    }
    if (result == IDCANCEL && kind == MB_YESNO) return IDNO;
    if (result == IDCANCEL && kind == MB_OK) return IDOK;
    return result;
}

bool ConfirmShortcutDeletion(HWND owner, std::wstring_view name, bool chinese) {
    std::wstring question = chinese ? L"确定删除“" : L"Delete \"";
    question.append(name);
    question += chinese ? L"”吗？" : L"\"?";
    const wchar_t* detail = chinese
        ? L"仅删除快捷项，不会删除目标文件。"
        : L"Only the shortcut will be removed. The target file will not be deleted.";
    const TASKDIALOG_BUTTON buttons[]{
        {IDYES, chinese ? L"删除" : L"Delete"},
        {IDCANCEL, chinese ? L"取消" : L"Cancel"},
    };
    TASKDIALOGCONFIG config{};
    config.cbSize = sizeof(config);
    config.hwndParent = owner;
    config.dwFlags = TDF_POSITION_RELATIVE_TO_WINDOW | TDF_ALLOW_DIALOG_CANCELLATION;
    config.pszWindowTitle = chinese ? L"删除快捷项" : L"Delete shortcut";
    config.pszMainInstruction = question.c_str();
    config.pszContent = detail;
    config.cButtons = 2;
    config.pButtons = buttons;
    config.nDefaultButton = IDCANCEL;
    int result = IDCANCEL;
    // Fail closed: no deletion when the confirmation could not be displayed.
    return SUCCEEDED(ShowNativeDialog(config, result)) && result == IDYES;
}

bool ConfirmEverythingSetup(
    HWND owner,
    bool chinese) {
    const TASKDIALOG_BUTTON buttons[]{
        {IDYES, chinese ? L"继续" : L"Continue"},
        {IDCANCEL, chinese ? L"取消" : L"Cancel"},
    };

    TASKDIALOGCONFIG config{};
    config.cbSize = sizeof(config);
    config.hwndParent = owner;
    config.dwFlags =
        TDF_POSITION_RELATIVE_TO_WINDOW |
        TDF_ALLOW_DIALOG_CANCELLATION;
    config.pszWindowTitle =
        chinese
            ? L"Everything 文件搜索"
            : L"Everything file search";
    config.pszMainInstruction =
        chinese
            ? L"启用 Everything 文件搜索"
            : L"Enable Everything file search";
    config.pszContent =
        chinese
            ? L"Asterun 会优先使用电脑上已有的 Everything。\n\n"
              L"如果没有找到，将从 voidtools 官方获取最新稳定版并自动完成必要设置。"
              L"首次启用文件索引时，Windows 可能会请求一次管理员权限。\n\n"
              L"下载文件会在使用前验证完整性。"
            : L"Asterun will use an existing Everything installation when possible.\n\n"
              L"If none is available, it will get the latest stable release from voidtools and configure what is needed automatically. "
              L"Windows may request administrator approval once when file indexing is first enabled.\n\n"
              L"Downloaded files are integrity-checked before use.";
    config.pszFooter =
        chinese
            ? L"Everything 由 voidtools 提供。托盘图标可在“搜索来源”中随时设置。"
            : L"Everything is provided by voidtools. Its managed tray icon can be changed anytime in Search sources.";
    config.cButtons =
        static_cast<UINT>(
            std::size(buttons));
    config.pButtons = buttons;
    config.nDefaultButton = IDCANCEL;

    int result = IDCANCEL;

    if (FAILED(
            ShowNativeDialog(
                config,
                result))) {
        result =
            MessageBoxW(
                owner,
                config.pszContent,
                config.pszWindowTitle,
                MB_YESNO |
                    MB_DEFBUTTON2);
    }

    return result == IDYES;
}

UninstallDataChoice
ChooseUninstallData(
    HWND owner,
    bool chinese) {
    constexpr int kPreserve = 1001;
    constexpr int kDelete = 1002;

    const TASKDIALOG_BUTTON buttons[]{
        {kPreserve,
         chinese
             ? L"卸载并保留个人数据"
             : L"Uninstall and keep personal data"},
        {kDelete,
         chinese
             ? L"彻底卸载"
             : L"Remove everything"},
        {IDCANCEL,
         chinese
             ? L"取消"
             : L"Cancel"},
    };

    TASKDIALOGCONFIG config{};
    config.cbSize = sizeof(config);
    config.hwndParent = owner;
    config.dwFlags =
        TDF_POSITION_RELATIVE_TO_WINDOW |
        TDF_ALLOW_DIALOG_CANCELLATION;
    config.pszWindowTitle =
        chinese
            ? L"卸载 Asterun"
            : L"Uninstall Asterun";
    config.pszMainInstruction =
        chinese
            ? L"是否保留个人数据？"
            : L"Keep your personal data?";
    config.pszContent =
        chinese
            ? L"Asterun 和由其管理的 Everything 组件都会被移除；你自己安装的 Everything 不会受到影响。\n\n"
              L"快捷项、设置和使用记录可以保留，方便以后重新安装。“彻底卸载”会同时删除这些个人数据。"
            : L"Asterun and the Everything components it manages will be removed; Everything installations you manage yourself are not changed.\n\n"
              L"Shortcuts, settings and usage history can be kept for a future reinstall. “Remove everything” deletes this personal data as well.";
    config.cButtons =
        static_cast<UINT>(
            std::size(buttons));
    config.pButtons = buttons;
    config.nDefaultButton = kPreserve;

    int result = IDCANCEL;

    if (FAILED(
            ShowNativeDialog(
                config,
                result))) {
        const int fallback =
            MessageBoxW(
                owner,
                chinese
                    ? L"是否同时删除快捷项、设置和使用记录？\n\n"
                      L"“是”=彻底删除；“否”=保留个人数据。"
                    : L"Also delete shortcuts, settings and usage history?\n\n"
                      L"Yes removes everything; No keeps personal data.",
                config.pszWindowTitle,
                MB_YESNOCANCEL |
                    MB_DEFBUTTON2);

        if (fallback == IDYES) {
            return UninstallDataChoice::Delete;
        }
        if (fallback == IDNO) {
            return UninstallDataChoice::Preserve;
        }
        return UninstallDataChoice::Cancel;
    }

    if (result == kPreserve) {
        return UninstallDataChoice::Preserve;
    }
    if (result == kDelete) {
        return UninstallDataChoice::Delete;
    }
    return UninstallDataChoice::Cancel;
}

bool ConfirmPermanentUserDataDeletion(
    HWND owner,
    bool chinese) {
    const TASKDIALOG_BUTTON buttons[]{
        {IDYES,
         chinese
             ? L"删除个人数据"
             : L"Delete personal data"},
        {IDCANCEL,
         chinese
             ? L"返回"
             : L"Go back"},
    };

    TASKDIALOGCONFIG config{};
    config.cbSize = sizeof(config);
    config.hwndParent = owner;
    config.dwFlags =
        TDF_POSITION_RELATIVE_TO_WINDOW |
        TDF_ALLOW_DIALOG_CANCELLATION;
    config.pszWindowTitle =
        chinese
            ? L"彻底卸载 Asterun"
            : L"Remove Asterun completely";
    config.pszMainInstruction =
        chinese
            ? L"同时删除个人数据？"
            : L"Delete personal data too?";
    config.pszContent =
        chinese
            ? L"快捷项、设置和使用记录删除后无法恢复。"
            : L"Shortcuts, settings and usage history cannot be recovered after deletion.";
    config.cButtons =
        static_cast<UINT>(
            std::size(buttons));
    config.pButtons = buttons;
    config.nDefaultButton = IDCANCEL;

    int result = IDCANCEL;
    return SUCCEEDED(
               ShowNativeDialog(
                   config,
                   result)) &&
        result == IDYES;
}
} // namespace altrun::ui
