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
int chineseLogicalWidth{};
bool measuringLonger{};
bool measuringChinese{};

BOOL CALLBACK CloseTestDialog(HWND window, LPARAM) {
    wchar_t title[128]{};
    GetWindowTextW(window, title, 128);
    const wchar_t* expected = deletion ? L"Delete shortcut" :
        measuringChinese ? L"测试运行" : L"Feedback runtime test";
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
        if (measuringChinese) chineseLogicalWidth = logicalWidth;
        else if (measuringLonger) longerLogicalWidth = logicalWidth;
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
    observed = false;
    measuringLonger = false;
    measuringChinese = true;
    buttonToClick = IDOK;
    assert(altrun::ui::ShowMessage(nullptr, L"请先填写目标。",
        L"测试运行", MB_OK | MB_ICONWARNING) == IDOK);
    assert(observed);

    // Compare actual native layouts on the same OS/font/DPI, not a hard-coded
    // pixel cap against cxWidth expressed in dialog units. Both localized
    // short messages must be narrower than the automatically sized long one.
    if (!(compactLogicalWidth > 0 && chineseLogicalWidth > 0 &&
          longerLogicalWidth > 0 &&
          compactLogicalWidth < longerLogicalWidth &&
          chineseLogicalWidth < longerLogicalWidth)) {
        std::fprintf(stderr, "TaskDialog sizing EN=%d ZH=%d long=%d at 96dpi\n",
            compactLogicalWidth, chineseLogicalWidth, longerLogicalWidth);
    }
    assert(compactLogicalWidth > 0 && chineseLogicalWidth > 0 &&
           longerLogicalWidth > 0 &&
           compactLogicalWidth < longerLogicalWidth &&
           chineseLogicalWidth < longerLogicalWidth);
    KillTimer(nullptr, timer);
    HANDLE after{};
    assert(GetCurrentActCtx(&after));
    assert(before == after); // Classic controls must keep the original activation context.
    if (before) ReleaseActCtx(before);
    if (after) ReleaseActCtx(after);
}
