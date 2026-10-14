#include "ui/Feedback.hpp"
#include "ui/UiTheme.hpp"
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
bool measuringEnglishCompact{};
int englishCompactWidth{};
bool compactPaletteVerified{};
struct OwnerModalProbe {
    bool disabled{};
    bool restored{};
    bool noticeAliveAtRestore{};
    int unexpectedHide{};
};
LRESULT CALLBACK OwnerModalSubclass(
    HWND owner, UINT message, WPARAM wParam, LPARAM lParam,
    UINT_PTR id, DWORD_PTR data) {
    auto& probe = *reinterpret_cast<OwnerModalProbe*>(data);
    if (message == WM_ENABLE) {
        if (wParam == FALSE) probe.disabled = true;
        else {
            probe.restored = true;
            const HWND notice = FindWindowW(L"Asterun.CompactNotice", nullptr);
            probe.noticeAliveAtRestore = notice && IsWindow(notice);
        }
    } else if (message == WM_SHOWWINDOW && wParam == FALSE) {
        ++probe.unexpectedHide;
    } else if (message == WM_NCDESTROY) {
        RemoveWindowSubclass(owner, OwnerModalSubclass, id);
    }
    return DefSubclassProc(owner, message, wParam, lParam);
}

BOOL CALLBACK CloseTestDialog(HWND window, LPARAM) {
    wchar_t title[128]{};
    GetWindowTextW(window, title, 128);
    const wchar_t* expected = deletion ? L"Delete shortcut" :
        measuringChinese ? L"测试运行" :
        measuringEnglishCompact ? L"Test" : L"Feedback runtime test";
    if (std::wcscmp(title, expected) != 0) return TRUE;
    // Asterun's notice has one explicit owner-drawn IDOK button. Native
    // TaskDialogs retain their existing supported TDM_CLICK_BUTTON path.
    const bool compact = measuringChinese || measuringEnglishCompact;
    HWND compactOk{};
    if (compact) {
        wchar_t className[80]{};
        GetClassNameW(window, className, static_cast<int>(std::size(className)));
        assert(std::wcscmp(className, L"Asterun.CompactNotice") == 0);
        compactOk = GetDlgItem(window, IDOK);
        if (!compactOk || !IsWindowVisible(compactOk) ||
            !IsWindowEnabled(compactOk)) return TRUE;
        assert((GetWindowLongPtrW(compactOk, GWL_STYLE) & BS_TYPEMASK) ==
               BS_OWNERDRAW);
        assert((GetWindowLongPtrW(compactOk, GWL_STYLE) & WS_TABSTOP) != 0);
        wchar_t label[16]{};
        GetWindowTextW(compactOk, label, static_cast<int>(std::size(label)));
        assert(std::wcscmp(label, measuringChinese ? L"确定" : L"OK") == 0);
        RECT bounds{};
        assert(GetClientRect(compactOk, &bounds));
        HDC buttonDc = GetDC(compactOk);
        assert(buttonDc);
        HDC buffer = CreateCompatibleDC(buttonDc);
        assert(buffer);
        HBITMAP bitmap = CreateCompatibleBitmap(buttonDc,
            bounds.right, bounds.bottom);
        assert(bitmap);
        HGDIOBJ previous = SelectObject(buffer, bitmap);
        assert(previous);
        DRAWITEMSTRUCT draw{};
        draw.CtlType = ODT_BUTTON;
        draw.CtlID = IDOK;
        draw.hwndItem = compactOk;
        draw.hDC = buffer;
        draw.rcItem = bounds;
        assert(SendMessageW(window, WM_DRAWITEM, IDOK,
               reinterpret_cast<LPARAM>(&draw)) == TRUE);
        assert(GetPixel(buffer, 12, bounds.bottom / 2) ==
               altrun::ui::kApplicationPalette.accent);
        compactPaletteVerified = true;
        SelectObject(buffer, previous);
        DeleteObject(bitmap);
        DeleteDC(buffer);
        ReleaseDC(compactOk, buttonDc);
    }
    observed = true;
    if (!deletion) {
        RECT bounds{};
        assert(GetWindowRect(window, &bounds));
        const UINT dpi = GetDpiForWindow(window);
        const int logicalWidth = MulDiv(bounds.right - bounds.left,
            96, dpi ? static_cast<int>(dpi) : 96);
        if (measuringChinese) chineseLogicalWidth = logicalWidth;
        else if (measuringEnglishCompact) englishCompactWidth = logicalWidth;
        else if (measuringLonger) longerLogicalWidth = logicalWidth;
        else compactLogicalWidth = logicalWidth;
    }
    if (compact) {
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
        L"测试运行", true) == IDOK);
    assert(observed);
    observed = false;
    measuringChinese = false;
    measuringEnglishCompact = true;
    assert(altrun::ui::ShowCompactNotice(nullptr, L"Enter a target first.",
        L"Test", false) == IDOK);
    assert(observed);
    assert(compactPaletteVerified);

    // Retain the original TaskDialog behavior for all ordinary warnings.
    // The one requested compact notice must be meaningfully narrower at
    // the same DPI/font than the native TaskDialog minimum.
    const bool suitablyCompact = compactLogicalWidth > 0 &&
        chineseLogicalWidth > 0 && englishCompactWidth > 0 &&
        longerLogicalWidth > 0 &&
        chineseLogicalWidth + 24 < compactLogicalWidth &&
        englishCompactWidth + 24 < compactLogicalWidth &&
        chineseLogicalWidth + 24 < longerLogicalWidth;
    if (!suitablyCompact) {
        std::fprintf(stderr, "Asterun notice widths ZH=%d EN=%d, TaskDialog short=%d long=%d (96dpi)\n",
            chineseLogicalWidth, englishCompactWidth,
            compactLogicalWidth, longerLogicalWidth);
    }
    assert(suitablyCompact);

    // Reproduce real editor ownership: a visible enabled parent must be
    // restored while its modal dialog still exists. Destroying the modal
    // first leaves the disabled parent with no active modal, causing an
    // intermediate activation/paint flash on some desktops.
    HWND owner = CreateWindowExW(0, L"STATIC", L"UI-01 modal owner",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        100, 100, 540, 380, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    assert(owner && IsWindowVisible(owner));
    OwnerModalProbe probe{};
    assert(SetWindowSubclass(owner, OwnerModalSubclass, 45,
        reinterpret_cast<DWORD_PTR>(&probe)));
    observed = false;
    assert(altrun::ui::ShowCompactNotice(owner, L"Enter a target first.",
        L"Test", false) == IDOK);
    assert(observed && probe.disabled && probe.restored);
    assert(probe.noticeAliveAtRestore);
    assert(probe.unexpectedHide == 0 && IsWindowVisible(owner) &&
           IsWindowEnabled(owner));
    assert(RemoveWindowSubclass(owner, OwnerModalSubclass, 45));
    DestroyWindow(owner);
    KillTimer(nullptr, timer);
    HANDLE after{};
    assert(GetCurrentActCtx(&after));
    assert(before == after); // Classic controls must keep the original activation context.
    if (before) ReleaseActCtx(before);
    if (after) ReleaseActCtx(after);
}
