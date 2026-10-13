#include "ui/Feedback.hpp"
#include <commctrl.h>
#include <cassert>
#include <cwchar>
#include <cstdio>

namespace {
bool observed{};
bool deletion{};
int buttonToClick{IDCANCEL};
int compactLogicalWidth{};
int longerLogicalWidth{};
bool measuringLonger{};

BOOL CALLBACK CloseTestDialog(HWND window, LPARAM) {
    wchar_t title[128]{};
    GetWindowTextW(window, title, 128);
    const wchar_t* expected = deletion ? L"Delete shortcut" : L"Feedback runtime test";
    if (std::wcscmp(title, expected) != 0) return TRUE;
    // TaskDialog button IDs are public message IDs, not a contract for child
    // HWND IDs or hierarchy. Exercise the real action through its supported API.
    observed = true;
    if (!deletion) {
        RECT bounds{};
        assert(GetWindowRect(window, &bounds));
        const UINT dpi = GetDpiForWindow(window);
        const int logicalWidth = MulDiv(bounds.right - bounds.left,
            96, dpi ? static_cast<int>(dpi) : 96);
        if (measuringLonger) longerLogicalWidth = logicalWidth;
        else compactLogicalWidth = logicalWidth;
    }
    SendMessageW(window, TDM_CLICK_BUTTON, buttonToClick, 0);
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
    buttonToClick = IDYES;
    assert(altrun::ui::ConfirmShortcutDeletion(nullptr, L"Example shortcut", false));
    assert(observed); // Only confirms intent; this test never deletes any data.
    observed = false;
    deletion = false;
    buttonToClick = IDNO;
    assert(altrun::ui::ShowMessage(nullptr, L"Silent confirmation", L"Feedback runtime test",
        MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) == IDNO);
    assert(observed);
    observed = false;
    measuringLonger = true;
    // Longer content deliberately keeps native TaskDialog's automatic sizing
    // as a reference on the same runner, DPI and system font.
    assert(altrun::ui::ShowMessage(nullptr,
        L"Longer instructions require more room to remain readable; "
        L"please check the options carefully before continuing.",
        L"Feedback runtime test", MB_YESNO | MB_ICONWARNING) == IDNO);
    assert(observed);
    std::fprintf(stderr,
        "UI01_NATIVE_TASKDIALOG_SIZES compact=%d baseline_long=%d (96dpi pixels)\n",
        compactLogicalWidth, longerLogicalWidth);
    assert(compactLogicalWidth > 0 && compactLogicalWidth <= 340);
    KillTimer(nullptr, timer);
    HANDLE after{};
    assert(GetCurrentActCtx(&after));
    assert(before == after); // Classic controls must keep the original activation context.
    if (before) ReleaseActCtx(before);
    if (after) ReleaseActCtx(after);
}
