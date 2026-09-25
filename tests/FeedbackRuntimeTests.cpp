#include "ui/Feedback.hpp"
#include <commctrl.h>
#include <cassert>
#include <cwchar>

namespace {
bool observed{};
bool deletion{};

BOOL CALLBACK CloseTestDialog(HWND window, LPARAM) {
    wchar_t title[128]{};
    GetWindowTextW(window, title, 128);
    const wchar_t* expected = deletion ? L"Delete shortcut" : L"Feedback runtime test";
    if (std::wcscmp(title, expected) != 0) return TRUE;
    if (deletion) {
        wchar_t label[32]{};
        GetDlgItemTextW(window, IDYES, label, 32);
        assert(std::wcscmp(label, L"Delete") == 0);
        GetDlgItemTextW(window, IDCANCEL, label, 32);
        assert(std::wcscmp(label, L"Cancel") == 0);
    }
    observed = true;
    SendMessageW(window, TDM_CLICK_BUTTON, deletion ? IDCANCEL : IDNO, 0);
    return FALSE;
}

void CALLBACK ClosePrompt(HWND, UINT, UINT_PTR, DWORD) {
    EnumThreadWindows(GetCurrentThreadId(), CloseTestDialog, 0);
}
}

int main() {
    altrun::ui::SetFeedbackEnabled(false);
    HANDLE before{};
    assert(GetCurrentActCtx(&before));
    const UINT_PTR timer = SetTimer(nullptr, 0, 100, ClosePrompt);
    assert(timer != 0);
    deletion = true;
    assert(!altrun::ui::ConfirmShortcutDeletion(nullptr, L"Example shortcut", false));
    assert(observed); // A resource/activation failure must not masquerade as Cancel.
    observed = false;
    deletion = false;
    assert(altrun::ui::ShowMessage(nullptr, L"Silent confirmation", L"Feedback runtime test",
        MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) == IDNO);
    assert(observed);
    KillTimer(nullptr, timer);
    HANDLE after{};
    assert(GetCurrentActCtx(&after));
    assert(before == after); // Classic controls must keep the original activation context.
    if (before) ReleaseActCtx(before);
    if (after) ReleaseActCtx(after);
}
