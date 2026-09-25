#include "Feedback.hpp"
#include "../ResourceIds.h"

#include <commctrl.h>
#include <mmsystem.h>
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
    constexpr int sounds[]{IDW_STARTUP, IDW_REVEAL, IDW_EXECUTE, IDW_FAILURE};
    // Embedded PCM, asynchronous, no queue, no looping or default Windows beep.
    // A new accepted cue replaces the previous one instead of accumulating audio.
    PlaySoundW(MAKEINTRESOURCEW(sounds[static_cast<std::size_t>(cue)]),
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
} // namespace altrun::ui
