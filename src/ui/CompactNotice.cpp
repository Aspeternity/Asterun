#include "Feedback.hpp"
#include "AppIcon.hpp"
#include "UiTheme.hpp"
#include "TopLevelWindowPresentation.hpp"

#include <algorithm>
#include <iterator>
#include <string>

namespace altrun::ui {
namespace {

// The editor's short validation notice shares its colors and rounded action
// button with the form. Keep this one-purpose window separate from the normal
// TaskDialog flow (confirmation, uninstall, update and hotkey warnings).
constexpr wchar_t kCompactNoticeClass[] = L"Asterun.CompactNotice";
constexpr int kNoticeWidthLogical = 288;
constexpr int kNoticeHeightLogical = 128;
constexpr int kNoticeFooterLogical = 54;

struct CompactNoticeState {
    HWND hwnd{};
    HWND owner{};
    HWND ok{};
    HFONT font{};
    std::wstring message;
    UINT dpi{96};
    bool done{};
};

int NoticeScale(int value, UINT dpi) {
    return MulDiv(value, static_cast<int>(dpi ? dpi : 96), 96);
}

void DrawCompactNotice(CompactNoticeState& state, HDC dc) {
    if (!dc || !state.hwnd) return;
    RECT client{};
    GetClientRect(state.hwnd, &client);
    const auto& color = kApplicationPalette;
    HBRUSH white = CreateSolidBrush(color.windowBackground);
    FillRect(dc, &client, white ? white : GetSysColorBrush(COLOR_WINDOW));
    if (white) DeleteObject(white);

    const int footerTop = client.bottom - NoticeScale(kNoticeFooterLogical, state.dpi);
    RECT footer{0, footerTop, client.right, client.bottom};
    HBRUSH background = CreateSolidBrush(color.bottomBackground);
    FillRect(dc, &footer, background ? background : GetSysColorBrush(COLOR_WINDOW));
    if (background) DeleteObject(background);
    RECT rule{0, footerTop, client.right, footerTop + 1};
    HBRUSH divider = CreateSolidBrush(color.separator);
    if (divider) {
        FillRect(dc, &rule, divider);
        DeleteObject(divider);
    }

    HGDIOBJ oldFont = state.font ? SelectObject(dc, state.font) : nullptr;
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, color.text);
    RECT content{
        NoticeScale(22, state.dpi),
        NoticeScale(12, state.dpi),
        client.right - NoticeScale(22, state.dpi),
        footerTop - NoticeScale(7, state.dpi)
    };
    DrawTextW(dc, state.message.c_str(), -1, &content,
        DT_LEFT | DT_VCENTER | DT_WORDBREAK | DT_NOPREFIX);
    if (oldFont) SelectObject(dc, oldFont);
}

void DrawCompactNoticeButton(const CompactNoticeState& state,
                             const DRAWITEMSTRUCT& item) {
    const int width = item.rcItem.right - item.rcItem.left;
    const int height = item.rcItem.bottom - item.rcItem.top;
    if (width <= 0 || height <= 0) return;
    HDC memory = CreateCompatibleDC(item.hDC);
    HBITMAP bitmap = memory
        ? CreateCompatibleBitmap(item.hDC, width, height) : nullptr;
    HGDIOBJ oldBitmap = memory && bitmap ? SelectObject(memory, bitmap) : nullptr;
    const bool buffered = oldBitmap && oldBitmap != HGDI_ERROR;
    HDC dc = buffered ? memory : item.hDC;
    RECT area = buffered
        ? RECT{0, 0, width, height} : item.rcItem;

    HBRUSH background = CreateSolidBrush(kApplicationPalette.bottomBackground);
    FillRect(dc, &area, background
        ? background : GetSysColorBrush(COLOR_WINDOW));
    if (background) DeleteObject(background);

    const bool pressed = (item.itemState & ODS_SELECTED) != 0;
    const COLORREF accent = pressed ? RGB(0, 96, 180) : kApplicationPalette.accent;
    HBRUSH fill = CreateSolidBrush(accent);
    HPEN edge = CreatePen(PS_SOLID, 1, accent);
    if (fill && edge) {
        HGDIOBJ oldFill = SelectObject(dc, fill);
        HGDIOBJ oldEdge = SelectObject(dc, edge);
        RoundRect(dc, area.left + 1, area.top + 1,
            area.right - 1, area.bottom - 1,
            NoticeScale(6, state.dpi), NoticeScale(6, state.dpi));
        SelectObject(dc, oldEdge);
        SelectObject(dc, oldFill);
    }
    if (edge) DeleteObject(edge);
    if (fill) DeleteObject(fill);

    wchar_t label[40]{};
    GetWindowTextW(item.hwndItem, label, static_cast<int>(std::size(label)));
    HGDIOBJ oldFont = state.font ? SelectObject(dc, state.font) : nullptr;
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(255, 255, 255));
    DrawTextW(dc, label, -1, &area,
        DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    if (oldFont) SelectObject(dc, oldFont);

    if (buffered) {
        BitBlt(item.hDC, item.rcItem.left, item.rcItem.top,
               width, height, memory, 0, 0, SRCCOPY);
        SelectObject(memory, oldBitmap);
    }
    if (bitmap) DeleteObject(bitmap);
    if (memory) DeleteDC(memory);
}

LRESULT CALLBACK CompactNoticeProc(HWND hwnd, UINT message,
                                   WPARAM wParam, LPARAM lParam) {
    if (message == WM_NCCREATE) {
        const auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        auto* state = static_cast<CompactNoticeState*>(create->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
        state->hwnd = hwnd;
        // DefWindowProc must initialize WS_CAPTION window text and native
        // non-client state. Returning TRUE directly leaves the title empty.
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }
    auto* state = reinterpret_cast<CompactNoticeState*>(
        GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (!state) return DefWindowProcW(hwnd, message, wParam, lParam);

    switch (message) {
    case WM_ERASEBKGND:
        return TRUE;
    case WM_PAINT: {
        PAINTSTRUCT paint{};
        HDC dc = BeginPaint(hwnd, &paint);
        DrawCompactNotice(*state, dc);
        EndPaint(hwnd, &paint);
        return 0;
    }
    case WM_PRINTCLIENT:
        DrawCompactNotice(*state, reinterpret_cast<HDC>(wParam));
        return 0;
    case WM_DRAWITEM: {
        auto* item = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
        if (item && item->CtlID == IDOK && item->hDC) {
            DrawCompactNoticeButton(*state, *item);
            return TRUE;
        }
        break;
    }
    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK && HIWORD(wParam) == BN_CLICKED) {
            state->done = true;
            return 0;
        }
        break;
    case WM_CLOSE:
    case WM_DESTROY:
        state->done = true;
        return 0;
    case WM_NCDESTROY:
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
        state->hwnd = nullptr;
        break;
    default:
        break;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

int ShowStyledCompactNotice(HWND owner, const wchar_t* message,
                            const wchar_t* title, bool chinese) {
    const HINSTANCE instance = GetModuleHandleW(nullptr);
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.hInstance = instance;
    wc.lpfnWndProc = CompactNoticeProc;
    wc.lpszClassName = kCompactNoticeClass;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hIcon = LoadApplicationIcon(instance);
    wc.hIconSm = LoadApplicationIcon(instance, true);
    if (!RegisterClassExW(&wc) &&
        GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return 0;
    }

    CompactNoticeState state{};
    state.owner = owner;
    state.message = message ? message : L"";
    state.dpi = owner && IsWindow(owner)
        ? GetDpiForWindow(owner) : GetDpiForSystem();
    if (state.dpi == 0) state.dpi = 96;

    const wchar_t* face = chinese ? L"Microsoft YaHei UI" : L"Segoe UI";
    state.font = CreateFontW(-MulDiv(10, static_cast<int>(state.dpi), 72),
        0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, face);

    constexpr DWORD style = WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_CLIPCHILDREN;
    constexpr DWORD exStyle = WS_EX_DLGMODALFRAME | WS_EX_CONTROLPARENT;
    RECT bounds{0, 0,
        NoticeScale(kNoticeWidthLogical, state.dpi),
        NoticeScale(kNoticeHeightLogical, state.dpi)};
    if (!AdjustWindowRectExForDpi(&bounds, style, FALSE, exStyle, state.dpi)) {
        if (state.font) DeleteObject(state.font);
        return 0;
    }
    const int width = bounds.right - bounds.left;
    const int height = bounds.bottom - bounds.top;
    const HMONITOR monitor = MonitorFromWindow(
        owner && IsWindow(owner) ? owner : GetDesktopWindow(),
        MONITOR_DEFAULTTONEAREST);
    MONITORINFO info{sizeof(info)};
    if (!GetMonitorInfoW(monitor, &info)) {
        info.rcWork = {0, 0, GetSystemMetrics(SM_CXSCREEN),
                      GetSystemMetrics(SM_CYSCREEN)};
    }
    RECT reference = info.rcWork;
    if (owner && IsWindow(owner) && IsWindowVisible(owner) &&
        !IsIconic(owner)) {
        GetWindowRect(owner, &reference);
    }
    const int centerX = (reference.left + reference.right) / 2;
    const int centerY = (reference.top + reference.bottom) / 2;
    const int workLeft = static_cast<int>(info.rcWork.left);
    const int workRight = static_cast<int>(info.rcWork.right);
    const int workTop = static_cast<int>(info.rcWork.top);
    const int workBottom = static_cast<int>(info.rcWork.bottom);
    const int left = std::clamp(centerX - width / 2, workLeft,
        std::max(workLeft, workRight - width));
    const int top = std::clamp(centerY - height / 2, workTop,
        std::max(workTop, workBottom - height));

    HWND window = CreateWindowExW(exStyle, kCompactNoticeClass,
        title ? title : L"Asterun", style, left, top, width, height,
        owner, nullptr, instance, &state);
    if (!window) {
        if (state.font) DeleteObject(state.font);
        return 0;
    }
    // Match the rest of Asterun: no DWM appearance/disappearance animation.
    window_presentation::Configure(window);
    RECT client{};
    GetClientRect(window, &client);
    const int buttonWidth = NoticeScale(88, state.dpi);
    const int buttonHeight = NoticeScale(32, state.dpi);
    const int footerTop = client.bottom - NoticeScale(kNoticeFooterLogical, state.dpi);
    state.ok = CreateWindowExW(0, L"BUTTON", chinese ? L"确定" : L"OK",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
        client.right - NoticeScale(18, state.dpi) - buttonWidth,
        footerTop + (NoticeScale(kNoticeFooterLogical, state.dpi) - buttonHeight) / 2,
        buttonWidth, buttonHeight, window,
        reinterpret_cast<HMENU>(static_cast<UINT_PTR>(IDOK)), instance, nullptr);
    if (!state.ok) {
        DestroyWindow(window);
        if (state.font) DeleteObject(state.font);
        return 0;
    }
    SendMessageW(state.ok, WM_SETFONT,
        reinterpret_cast<WPARAM>(state.font), FALSE);

    const bool reenable = owner && IsWindow(owner) && IsWindowEnabled(owner);
    if (reenable) EnableWindow(owner, FALSE);
    // Show only the fully painted first frame; activation happens once.
    window_presentation::RevealFullyPainted(window);
    SetActiveWindow(window);
    SetFocus(state.ok);
    bool sawQuit = false;
    int quitCode = 0;
    MSG msg{};
    while (!state.done) {
        const BOOL received = GetMessageW(&msg, nullptr, 0, 0);
        if (received <= 0) {
            if (received == 0) {
                sawQuit = true;
                quitCode = static_cast<int>(msg.wParam);
            }
            break;
        }
        if ((msg.hwnd == window || IsChild(window, msg.hwnd)) &&
            msg.message == WM_KEYDOWN &&
            (msg.wParam == VK_RETURN || msg.wParam == VK_ESCAPE)) {
            if (msg.wParam == VK_RETURN)
                SendMessageW(state.ok, BM_CLICK, 0, 0);
            else
                SendMessageW(window, WM_CLOSE, 0, 0);
            continue;
        }
        if (!IsDialogMessageW(window, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    // Restore the owner while its active modal still exists. If the popup
    // is destroyed first, Windows sees a disabled owner with no modal, and
    // can briefly activate/repaint a different window behind the editor.
    if (reenable && IsWindow(owner)) {
        EnableWindow(owner, TRUE);
        if (IsWindowVisible(owner) && !IsIconic(owner))
            SetActiveWindow(owner);
    }
    if (IsWindow(window)) {
        SetWindowPos(window, nullptr, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
            SWP_NOACTIVATE | SWP_HIDEWINDOW);
        DestroyWindow(window);
    }
    if (state.font) DeleteObject(state.font);
    if (sawQuit) PostQuitMessage(quitCode);
    return IDOK;
}

} // namespace

int ShowCompactNotice(HWND owner, const wchar_t* text, const wchar_t* title,
                      bool chinese) {
    // A single-purpose, Asterun-styled dialog for missing input only.
    const int result = ShowStyledCompactNotice(owner, text, title, chinese);
    return result ? result : ShowMessage(owner, text, title, MB_OK);
}
} // namespace altrun::ui
