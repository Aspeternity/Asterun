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
    // A native MessageBox is not a TaskDialog. WM_COMMAND with lParam=0
    // does not emulate a real button click and can leave its modal loop
    // running indefinitely. Only interact once the actual OK child exists.
    HWND compactOk{};
    if (measuringChinese) {
        compactOk = GetDlgItem(window, IDOK);
        if (!compactOk || !IsWindowVisible(window) ||
            !IsWindowVisible(compactOk) || !IsWindowEnabled(compactOk)) {
            return TRUE; // UI is still initializing; retry on the next tick.
        }
    }
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
    if (measuringChinese) {
        SendMessageW(compactOk, BM_CLICK, 0, 0);
    } else {
        SendMessageW(window, TDM_CLICK_BUTTON, buttonToClick, 0);
    }
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
    assert(altrun::ui::ShowCompactNotice(nullptr, L"请先填写目标。",
        L"测试运行") == IDOK);
    assert(observed);

    // Retain the original TaskDialog behavior for all ordinary warnings.
    // The one requested compact notice must be meaningfully narrower at
    // the same DPI/font than the native TaskDialog minimum.
    const bool suitablyCompact = compactLogicalWidth > 0 &&
        chineseLogicalWidth > 0 && longerLogicalWidth > 0 &&
        chineseLogicalWidth + 24 < compactLogicalWidth &&
        chineseLogicalWidth + 24 < longerLogicalWidth;
    if (!suitablyCompact) {
        std::fprintf(stderr, "Native dialogs: compactNotice=%d nativeShort=%d nativeLong=%d (96dpi)\n",
            chineseLogicalWidth, compactLogicalWidth, longerLogicalWidth);
    }
    assert(suitablyCompact);
    KillTimer(nullptr, timer);
    HANDLE after{};
    assert(GetCurrentActCtx(&after));
    assert(before == after); // Classic controls must keep the original activation context.
    if (before) ReleaseActCtx(before);
    if (after) ReleaseActCtx(after);
}
