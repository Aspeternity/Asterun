#include "app/App.hpp"
#include "core/HotkeyRegistry.hpp"
#include "IsolatedDataDirectory.hpp"
#include "NumericIntentRuntimeFixture.hpp"
#include "platform/InstanceIpc.hpp"
#include "ui/SettingsWindow.hpp"
#include "ui/ShortcutEditorDialog.hpp"
#include "ui/UiComboBox.hpp"
#include "ui/UiEditChrome.hpp"
#include "ui/UiTheme.hpp"
#include "ui/UiTypography.hpp"
#include "ui/ShortcutPathConverterDialog.hpp"
#include "ui/ShortcutManagerWindow.hpp"
#include "ui/TopLevelWindowPresentation.hpp"

#include <commctrl.h>
#include <objbase.h>
#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <array>
#include <iostream>
#include <string_view>
#include <set>
#include <vector>

#ifdef NDEBUG
#error "window_presentation_runtime_tests requires assertions"
#endif

namespace {

struct ProcessResourceSnapshot {
    DWORD gdi{};
    DWORD user{};
    DWORD handles{};
};

ProcessResourceSnapshot ProcessResources() {
    ProcessResourceSnapshot snapshot;
    snapshot.gdi =
        GetGuiResources(
            GetCurrentProcess(),
            GR_GDIOBJECTS);
    snapshot.user =
        GetGuiResources(
            GetCurrentProcess(),
            GR_USEROBJECTS);

    assert(
        GetProcessHandleCount(
            GetCurrentProcess(),
            &snapshot.handles));

    return snapshot;
}


LRESULT CALLBACK RedrawSuspendProbe(HWND, UINT, WPARAM, LPARAM,
                                       UINT_PTR, DWORD_PTR);

void AssertBodyFont(HWND control, UINT dpi, int pointSize = 10) {
    assert(control && IsWindow(control));
    const auto font = reinterpret_cast<HFONT>(
        SendMessageW(control, WM_GETFONT, 0, 0));
    assert(font);
    LOGFONTW logical{};
    assert(GetObjectW(font, sizeof(logical), &logical) == sizeof(logical));
    assert(logical.lfHeight == altrun::ui::HintSafeFontHeight(
        logical.lfFaceName, -MulDiv(pointSize, static_cast<int>(dpi), 72)));
    assert(logical.lfWeight == FW_NORMAL);
}

void AssertEditorLabelFonts(HWND editor, UINT dpi, int pointSize = 10) {
    struct Probe { UINT dpi; int labels; int pointSize; } probe{dpi, 0, pointSize};
    EnumChildWindows(editor, [](HWND control, LPARAM data) -> BOOL {
        wchar_t cls[32]{};
        if (!GetClassNameW(control, cls, 32) ||
            lstrcmpiW(cls, L"Static") != 0 ||
            (GetWindowLongPtrW(control, GWL_STYLE) & SS_TYPEMASK) != SS_RIGHT ||
            GetWindowTextLengthW(control) == 0) return TRUE;
        auto& state = *reinterpret_cast<Probe*>(data);
        AssertBodyFont(control, state.dpi, state.pointSize);
        ++state.labels;
        return TRUE;
    }, reinterpret_cast<LPARAM>(&probe));
    // Keyword, name, target, type, and runtime form fields must all use
    // the same body metrics. No three-label semibold exception.
    assert(probe.labels >= 5);
}

void AssertNativeEditFrame(HWND parent, HWND edit, UINT dpi) {
    assert(edit && IsWindow(edit));
    assert(GetParent(edit) == parent);
    wchar_t cls[32]{};
    assert(GetClassNameW(edit, cls, 32));
    assert(lstrcmpiW(cls, L"Edit") == 0);
    const LONG_PTR style = GetWindowLongPtrW(edit, GWL_STYLE);
    // Win32 EDIT may consume WS_BORDER during WM_NCCREATE even when
    // CreateWindowExW received it. GWL_STYLE is not its rendering contract:
    // assert the actual nonclient insets and painted frame below.
    assert((style & WS_CHILD) != 0);
    assert((style & WS_TABSTOP) != 0);
    assert((style & ES_AUTOHSCROLL) != 0);
    assert((GetWindowLongPtrW(edit, GWL_EXSTYLE) & WS_EX_CLIENTEDGE) == 0);
    DWORD_PTR dpiRef{};
    assert(GetWindowSubclass(edit, altrun::ui::NativeEditChromeSubclass,
        altrun::ui::kNativeEditChromeSubclassId, &dpiRef));
    assert(dpiRef != 0 &&
        *reinterpret_cast<const UINT*>(dpiRef) == dpi);

    RECT window{}, client{};
    assert(GetWindowRect(edit, &window));
    assert(GetClientRect(edit, &client));
    // Hidden advanced/test inputs remain 0x0 until their first expansion.
    // They still require the native style/subclass but have no frame yet.
    if (window.right <= window.left || window.bottom <= window.top) {
        assert(!IsWindowVisible(edit));
        return;
    }
    POINT origin{};
    assert(ClientToScreen(edit, &origin));
    const int top = origin.y - window.top;
    const int bottom = window.bottom - origin.y - client.bottom;
    const int left = origin.x - window.left;
    const int right = window.right - origin.x - client.right;
    assert(top > 0 && bottom > 0);
    assert(std::abs(top - bottom) <= 2);
    assert(left >= 1 && right >= 1);

    const HDC textDc = GetDC(edit);
    assert(textDc);
    const HFONT body = reinterpret_cast<HFONT>(
        SendMessageW(edit, WM_GETFONT, 0, 0));
    const HGDIOBJ previous = body ? SelectObject(textDc, body) : nullptr;
    TEXTMETRICW metrics{};
    assert(GetTextMetricsW(textDc, &metrics));
    if (previous && previous != HGDI_ERROR)
        SelectObject(textDc, previous);
    ReleaseDC(edit, textDc);
    assert(client.bottom - client.top >= metrics.tmHeight);

    if (IsWindowVisible(edit)) {
        // Assert actual top-edge pixels, not just a geometrically valid
        // (but possibly clipped/invisible) parent-space frame rectangle.
        assert(RedrawWindow(edit, nullptr, nullptr,
            RDW_FRAME | RDW_INVALIDATE | RDW_NOERASE | RDW_UPDATENOW));
        const HDC chromeDc = GetWindowDC(edit);
        assert(chromeDc);
        const COLORREF color = IsWindowEnabled(edit) && GetFocus() == edit
            ? altrun::ui::kApplicationPalette.accent
            : altrun::ui::kApplicationPalette.frame;
        const int width = window.right - window.left;
        const int edgePixels = std::max(3, altrun::ui::Scale(2, dpi));
        for (int x : {width / 4, width / 2, 3 * width / 4}) {
            bool topEdgePresent = false;
            bool bottomEdgePresent = false;
            for (int edge = 0; edge < edgePixels; ++edge) {
                topEdgePresent |= GetPixel(chromeDc, x, edge) == color;
                bottomEdgePresent |=
                    GetPixel(chromeDc, x, window.bottom - window.top - 1 - edge)
                        == color;
            }
            if (!topEdgePresent || !bottomEdgePresent) {
                std::cerr << "UI-INPUT-01 missing horizontal EDIT frame: id="
                    << GetDlgCtrlID(edit) << " x=" << x
                    << " dpi=" << dpi << "\n";
            }
            assert(topEdgePresent && bottomEdgePresent);
        }
        const int height = window.bottom - window.top;
        // Corner arcs are anti-aliased (blended, never the exact frame
        // color); probe the straight vertical run between the two arcs.
        const int arc = altrun::ui::Scale(
            altrun::ui::kControlCornerRadiusLogical, dpi) + 2;
        const int straightTop = std::min(arc, height / 2);
        const int straightBottom = std::max(height - 1 - arc, height / 2);
        for (int y : {straightTop, height / 2, straightBottom}) {
            bool leftEdgePresent = false;
            bool rightEdgePresent = false;
            for (int edge = 0; edge < edgePixels; ++edge) {
                leftEdgePresent |= GetPixel(chromeDc, edge, y) == color;
                rightEdgePresent |= GetPixel(chromeDc, width - 1 - edge, y)
                    == color;
            }
            if (!leftEdgePresent || !rightEdgePresent) {
                std::cerr << "UI-INPUT-01 missing vertical EDIT frame: id="
                    << GetDlgCtrlID(edit) << " y=" << y
                    << " dpi=" << dpi << "\n";
            }
            assert(leftEdgePresent && rightEdgePresent);
        }

        // Negative pixel contract: one renderer only. The previous PR
        // incorrectly combined native WS_BORDER with our rounded outline,
        // creating dark [ and ] strokes just inside the pale outer frame.
        const COLORREF clear = altrun::ui::kApplicationPalette.controlBackground;
        assert(left >= 4 && right >= 4);
        for (const int y : {straightTop, height / 2, straightBottom}) {
            for (int x = 2; x < left; ++x) {
                const COLORREF pixel = GetPixel(chromeDc, x, y);
                if (pixel != clear) {
                    std::cerr << "UI-INPUT-01 duplicate left NC stroke: id="
                        << GetDlgCtrlID(edit) << " x=" << x << " y=" << y
                        << " dpi=" << dpi << "\n";
                }
                assert(pixel == clear);
            }
            for (int x = width - right; x < width - 2; ++x) {
                const COLORREF pixel = GetPixel(chromeDc, x, y);
                if (pixel != clear) {
                    std::cerr << "UI-INPUT-01 duplicate right NC stroke: id="
                        << GetDlgCtrlID(edit) << " x=" << x << " y=" << y
                        << " dpi=" << dpi << "\n";
                }
                assert(pixel == clear);
            }
        }
        ReleaseDC(edit, chromeDc);
    }
}

void AssertEditorNativeEditFrames(HWND editor, UINT dpi) {
    for (UINT id : {53101u, 53102u, 53105u, 53122u, 53107u, 53108u}) {
        AssertNativeEditFrame(editor, GetDlgItem(editor, id), dpi);
    }
}

void AssertRuntimeHintFits(HWND editor, UINT dpi) {
    constexpr int kRuntimeHintId = 53120;
    const HWND hint = GetDlgItem(editor, kRuntimeHintId);
    assert(hint && IsWindow(hint));
    AssertBodyFont(hint, dpi, 9);

    const int length = GetWindowTextLengthW(hint);
    assert(length > 0);
    std::wstring text(static_cast<std::size_t>(length) + 1, L'\0');
    assert(GetWindowTextW(hint, text.data(), length + 1) == length);
    text.resize(static_cast<std::size_t>(length));

    RECT available{};
    assert(GetClientRect(hint, &available));
    HDC dc = GetDC(hint);
    assert(dc);
    const auto font = reinterpret_cast<HFONT>(
        SendMessageW(hint, WM_GETFONT, 0, 0));
    assert(font);
    const HGDIOBJ previous = SelectObject(dc, font);
    assert(previous && previous != HGDI_ERROR);
    RECT measured{};
    assert(DrawTextW(dc, text.c_str(), -1, &measured,
        DT_CALCRECT | DT_SINGLELINE | DT_NOPREFIX) > 0);
    SelectObject(dc, previous);
    ReleaseDC(hint, dc);

    if (measured.right > available.right ||
        measured.bottom > available.bottom) {
        std::wcerr << L"UI-INPUT-01 clipped runtime hint: "
            << text << L" measured=" << measured.right << L"x"
            << measured.bottom << L" available=" << available.right
            << L"x" << available.bottom << L" dpi=" << dpi << L"\n";
    }
    assert(measured.right <= available.right);
    assert(measured.bottom <= available.bottom);
}

void AssertModalFontDpiUpdate(HWND window, HWND bodyControl, bool editor, int editorPointSize = 10) {
    const int expectedPointSize = editor ? editorPointSize : 10;
    const UINT initialDpi = GetDpiForWindow(window);
    assert(initialDpi != 0);
    const UINT changedDpi = initialDpi == 120 ? 144 : 120;
    const auto applyDpi = [&](UINT dpi, UINT fromDpi) {
        RECT suggested{};
        assert(GetWindowRect(window, &suggested));
        suggested.right = suggested.left +
            MulDiv(suggested.right - suggested.left,
                static_cast<int>(dpi), static_cast<int>(fromDpi));
        suggested.bottom = suggested.top +
            MulDiv(suggested.bottom - suggested.top,
                static_cast<int>(dpi), static_cast<int>(fromDpi));
        int suspended = 0;
        assert(SetWindowSubclass(window, RedrawSuspendProbe, 89,
            reinterpret_cast<DWORD_PTR>(&suspended)));
        SendMessageW(window, WM_DPICHANGED, MAKEWPARAM(dpi, dpi),
            reinterpret_cast<LPARAM>(&suggested));
        assert(suspended == 0);
        assert(RemoveWindowSubclass(window, RedrawSuspendProbe, 89));
        assert(IsWindow(window) && IsWindowVisible(window));
        AssertBodyFont(bodyControl, dpi, expectedPointSize);
        if (editor) {
            AssertEditorLabelFonts(window, dpi, expectedPointSize);
            AssertEditorNativeEditFrames(window, dpi);
            AssertRuntimeHintFits(window, dpi);
        }
    };
    AssertBodyFont(bodyControl, initialDpi, expectedPointSize);
    if (editor) {
        AssertEditorLabelFonts(window, initialDpi, expectedPointSize);
        AssertEditorNativeEditFrames(window, initialDpi);
        AssertRuntimeHintFits(window, initialDpi);
    }
    applyDpi(changedDpi, initialDpi);
    applyDpi(initialDpi, changedDpi);
}


// UI-R02: verify selected CJK UI face exists and contains all frequently
// reported Han glyphs, rather than relying on silent GDI face substitution.
int CALLBACK MarkFoundUiFont(const LOGFONTW*, const TEXTMETRICW*,
                             DWORD, LPARAM context) {
    *reinterpret_cast<bool*>(context) = true;
    return 0;
}

bool InstalledUiTypeface(HDC dc, const wchar_t* name) {
    LOGFONTW requested{};
    requested.lfCharSet = DEFAULT_CHARSET;
    lstrcpynW(requested.lfFaceName, name, LF_FACESIZE);
    bool found = false;
    EnumFontFamiliesExW(dc, &requested, MarkFoundUiFont,
        reinterpret_cast<LPARAM>(&found), 0);
    return found;
}

// The OpenType 'name' table of the face realized for `face`; identical tables
// mean the same physical face regardless of which family alias was requested.
std::vector<unsigned char> RealizedNameTable(HDC dc, const wchar_t* face) {
    LOGFONTW requested{};
    requested.lfHeight = -15;
    requested.lfCharSet = DEFAULT_CHARSET;
    lstrcpynW(requested.lfFaceName, face, LF_FACESIZE);
    const HFONT font = CreateFontIndirectW(&requested);
    assert(font);
    const HGDIOBJ old = SelectObject(dc, font);
    assert(old && old != HGDI_ERROR);
    constexpr DWORD kNameTable = 0x656d616e;  // 'name', little-endian tag
    const DWORD size = GetFontData(dc, kNameTable, 0, nullptr, 0);
    assert(size != GDI_ERROR && size != 0);
    std::vector<unsigned char> table(size);
    assert(GetFontData(dc, kNameTable, 0, table.data(), size) == size);
    assert(SelectObject(dc, old) == font);
    assert(DeleteObject(font));
    return table;
}

void AssertChineseUiTypeface() {
    HDC dc = GetDC(nullptr);
    assert(dc);
    const bool hasYaHei = InstalledUiTypeface(dc, L"Microsoft YaHei");
    const bool hasYaHeiUi = InstalledUiTypeface(dc, L"Microsoft YaHei UI");
    const wchar_t* expected = hasYaHei ? L"Microsoft YaHei"
        : hasYaHeiUi ? L"Microsoft YaHei UI" : L"Segoe UI";
    const auto spec = altrun::ui::ApplicationFontSpec(
        altrun::Language::ZhCN, altrun::ui::UiFontRole::Body);
    assert(lstrcmpiW(spec.face, expected) == 0);

    const HFONT font = altrun::ui::CreateFontHandle(spec, 96);
    assert(font);
    const HGDIOBJ old = SelectObject(dc, font);
    assert(old && old != HGDI_ERROR);
    wchar_t realized[LF_FACESIZE]{};
    assert(GetTextFaceW(dc, LF_FACESIZE, realized) != 0);
    if (hasYaHei || hasYaHeiUi) {
        // These common glyphs must be provided by the chosen face itself,
        // without per-character substitution to visually unrelated fonts.
        // GetTextFaceW reports the localized family name on CJK locales
        // (e.g. "微软雅黑" for Microsoft YaHei on zh-CN), so accept a different
        // spelling only when it names the very same physical face.
        if (lstrcmpiW(realized, expected) != 0) {
            assert(InstalledUiTypeface(dc, realized));
            assert(RealizedNameTable(dc, realized) ==
                   RealizedNameTable(dc, expected));
        }
        constexpr wchar_t sample[] = L"搜索来源新建快捷词";
        WORD glyphs[(sizeof(sample) / sizeof(wchar_t)) - 1]{};
        assert(GetGlyphIndicesW(dc, sample,
            static_cast<int>((sizeof(sample) / sizeof(wchar_t)) - 1),
            glyphs, GGI_MARK_NONEXISTING_GLYPHS) != GDI_ERROR);
        for (const WORD glyph : glyphs) assert(glyph != 0xffff);
    }
    assert(SelectObject(dc, old) == font);
    assert(DeleteObject(font));
    assert(ReleaseDC(nullptr, dc) == 1);
}

void AssertHintSafeFontHeights() {
    using altrun::ui::HintSafeFontHeight;
    for (const wchar_t* face : {L"Microsoft YaHei", L"Microsoft YaHei UI"}) {
        assert(HintSafeFontHeight(face, -16) == -15);
        assert(HintSafeFontHeight(face, -20) == -19);
        assert(HintSafeFontHeight(face, -21) == -22);
        for (const int clean : {-12, -13, -15, -17, -18, -19, -22, -27})
            assert(HintSafeFontHeight(face, clean) == clean);
        assert(HintSafeFontHeight(face, 20) == 20);
    }
    assert(HintSafeFontHeight(L"Segoe UI", -20) == -20);
    assert(HintSafeFontHeight(L"SimSun", -16) == -16);
    assert(HintSafeFontHeight(nullptr, -20) == -20);
}

void AssertApplicationTypographyPolicy() {
    for (const auto language :
         {altrun::Language::ZhCN, altrun::Language::EnUS}) {
        const auto body = altrun::ui::ApplicationFontSpec(
            language, altrun::ui::UiFontRole::Body);
        const auto emphasis = altrun::ui::ApplicationFontSpec(
            language, altrun::ui::UiFontRole::BodySemibold);
        const auto section = altrun::ui::ApplicationFontSpec(
            language, altrun::ui::UiFontRole::SectionTitle);
        const auto navigation = altrun::ui::ApplicationFontSpec(
            language, altrun::ui::UiFontRole::SettingsNavigation);
        const auto shortcut = altrun::ui::ApplicationFontSpec(
            language, altrun::ui::UiFontRole::ShortcutBody);
        const auto hint = altrun::ui::ApplicationFontSpec(
            language, altrun::ui::UiFontRole::ShortcutHint);
        const int navigationSize =
            language == altrun::Language::ZhCN ? 11 : 10;
        assert(navigation.pointSize == navigationSize);
        assert(shortcut.pointSize == 10);
        assert(hint.pointSize == 9);
        assert(navigation.weight == FW_NORMAL);
        assert(shortcut.weight == FW_NORMAL);
        assert(hint.weight == FW_NORMAL);
        if (language == altrun::Language::ZhCN) {
            const auto modern = altrun::ui::LauncherFontSpec(
                altrun::UiStyle::ModernCompact,
                language, altrun::ui::UiFontRole::Body);
            assert(lstrcmpiW(navigation.face, modern.face) == 0);
            assert(lstrcmpiW(shortcut.face, navigation.face) == 0);
            assert(lstrcmpiW(hint.face, navigation.face) == 0);
        }
        assert(body.pointSize == 10 && body.weight == FW_NORMAL);
        assert(emphasis.pointSize == 10 && emphasis.weight == FW_SEMIBOLD);
        assert(section.pointSize == 11 && section.weight == FW_SEMIBOLD);
        assert(lstrcmpW(body.face, emphasis.face) == 0);
        assert(lstrcmpW(body.face, section.face) == 0);
        for (const UINT dpi : {96u, 120u, 144u, 192u}) {
            const HFONT font = altrun::ui::CreateFontHandle(body, dpi);
            assert(font);
            LOGFONTW logical{};
            assert(GetObjectW(font, sizeof(logical), &logical) ==
                sizeof(logical));
            assert(logical.lfHeight == altrun::ui::HintSafeFontHeight(
                body.face, -MulDiv(10, static_cast<int>(dpi), 72)));
            // 10pt YaHei at 150% would be the 20px em that shrinks 索.
            if (language == altrun::Language::ZhCN &&
                (lstrcmpiW(body.face, L"Microsoft YaHei") == 0 ||
                 lstrcmpiW(body.face, L"Microsoft YaHei UI") == 0)) {
                assert(dpi != 144u || logical.lfHeight == -19);
            }
            assert(logical.lfWeight == FW_NORMAL);
            assert(DeleteObject(font));
        }
    }
}

bool EverythingServiceIsMissing() {
    SC_HANDLE manager =
        OpenSCManagerW(
            nullptr,
            nullptr,
            SC_MANAGER_CONNECT);

    if (!manager) {
        return false;
    }

    SC_HANDLE service =
        OpenServiceW(
            manager,
            L"Everything",
            SERVICE_QUERY_STATUS);

    if (service) {
        CloseServiceHandle(service);
        CloseServiceHandle(manager);
        return false;
    }

    const DWORD error =
        GetLastError();

    CloseServiceHandle(manager);

    return error ==
        ERROR_SERVICE_DOES_NOT_EXIST;
}

void AssertNoResourceGrowth(
    const ProcessResourceSnapshot& before,
    const ProcessResourceSnapshot& after) {

    // A small tolerance keeps the test independent of one-time USER/common-
    // control bookkeeping, while any per-open leak across the soak loop still
    // fails decisively.
    constexpr DWORD kTolerance = 4;

    assert(after.gdi <=
        before.gdi + kTolerance);
    assert(after.user <=
        before.user + kTolerance);
    assert(after.handles <=
        before.handles + kTolerance);
}

bool HasAboutHeading(HWND window) {
    bool found = false;
    EnumChildWindows(window, [](HWND child, LPARAM data) -> BOOL {
        wchar_t kind[32]{};
        wchar_t text[128]{};
        GetClassNameW(child, kind, 32);
        GetWindowTextW(child, text, 128);
        if (lstrcmpiW(kind, L"STATIC") == 0 &&
            IsWindowVisible(child) &&
            (std::wstring_view(text) == L"About" ||
             std::wstring_view(text) == L"关于")) {
            *reinterpret_cast<bool*>(data) = true;
        }
        return TRUE;
    }, reinterpret_cast<LPARAM>(&found));
    return found;
}

LRESULT CALLBACK FirstShowProbe(HWND window, UINT message, WPARAM wParam,
                               LPARAM lParam, UINT_PTR, DWORD_PTR data) {
    const auto result = DefSubclassProc(window, message, wParam, lParam);
    if (message == WM_WINDOWPOSCHANGED &&
        (reinterpret_cast<WINDOWPOS*>(lParam)->flags & SWP_SHOWWINDOW)) {
        assert(HasAboutHeading(window));
        ++*reinterpret_cast<int*>(data);
    }
    return result;
}

LRESULT CALLBACK RedrawSuspendProbe(HWND window, UINT message,
                                      WPARAM wParam, LPARAM lParam,
                                      UINT_PTR, DWORD_PTR data) {
    if (message == WM_SETREDRAW &&
        wParam == FALSE) {
        ++*reinterpret_cast<int*>(data);
    }

    return DefSubclassProc(
        window,
        message,
        wParam,
        lParam);
}

// Catch buttons briefly exposed during Settings page transitions. A final
// hidden state is insufficient: it misses the one-frame reset-link flash.
LRESULT CALLBACK HotkeyResetExposureProbe(
    HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam,
    UINT_PTR, DWORD_PTR data) {
    // SetWindowPos(SWP_SHOWWINDOW) does not necessarily dispatch
    // WM_SHOWWINDOW. Observe the actual visibility transition request.
    if (message == WM_WINDOWPOSCHANGING && lParam) {
        const auto* pos = reinterpret_cast<const WINDOWPOS*>(lParam);
        if ((pos->flags & SWP_SHOWWINDOW) != 0) {
            ++*reinterpret_cast<int*>(data);
        }
    }
    return DefSubclassProc(hwnd, message, wParam, lParam);
}

struct PaintMessageCounts {
    int paint{};
    int erase{};
};

LRESULT CALLBACK TextChangeProbe(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp,
                                 UINT_PTR, DWORD_PTR data) {
    if (msg == WM_SETTEXT) ++*reinterpret_cast<int*>(data);
    return DefSubclassProc(hwnd, msg, wp, lp);
}

LRESULT CALLBACK PaintMessageProbe(
    HWND window,
    UINT message,
    WPARAM wParam,
    LPARAM lParam,
    UINT_PTR,
    DWORD_PTR data) {

    auto& counts =
        *reinterpret_cast<
            PaintMessageCounts*>(
                data);

    if (message == WM_PAINT) {
        ++counts.paint;
    } else if (
        message == WM_ERASEBKGND) {
        ++counts.erase;
    }

    return DefSubclassProc(
        window,
        message,
        wParam,
        lParam);
}

HWND modalOwner{};
bool observedEditor{};
bool observedPathConverter{};
bool observedModalDestroy{};
bool ownerEnabledAtModalDestroy{};
bool expectedOwnerEnabledAtModalDestroy{};
bool observedKeywordCaretAtEnd{};
bool checkUiR02ModalDpi{};
int expectedEditorBodyPointSize = 10;
bool observedEditorDpiCheck{};
bool observedConverterDpiCheck{};

LRESULT CALLBACK ModalDestroyProbe(HWND window, UINT message, WPARAM wParam,
                                   LPARAM lParam, UINT_PTR subclassId,
                                   DWORD_PTR) {
    if (message == WM_NCDESTROY) {
        observedModalDestroy = true;
        ownerEnabledAtModalDestroy =
            IsWindowEnabled(modalOwner) != FALSE;
        assert(ownerEnabledAtModalDestroy ==
            expectedOwnerEnabledAtModalDestroy);
        RemoveWindowSubclass(window, ModalDestroyProbe, subclassId);
    }
    return DefSubclassProc(window, message, wParam, lParam);
}

void ArmModalDestroyProbe(HWND window) {
    observedModalDestroy = false;
    assert(SetWindowSubclass(window, ModalDestroyProbe, 2, 0));
}

void CALLBACK CancelEditor(HWND, UINT, UINT_PTR timer, DWORD) {
    const HWND editor = FindWindowW(L"Asterun.ShortcutEditor", nullptr);
    if (!editor) return;
    assert(IsWindowVisible(editor));
    assert(!IsWindowEnabled(modalOwner));
    observedEditor = true;
    if (checkUiR02ModalDpi) {
        AssertModalFontDpiUpdate(editor, GetDlgItem(editor, 53102),
            true, expectedEditorBodyPointSize);
        observedEditorDpiCheck = true;
    }
    ArmModalDestroyProbe(editor);
    KillTimer(nullptr, timer);
    PostMessageW(editor, WM_CLOSE, 0, 0);
}

void CALLBACK CancelPathConverter(HWND, UINT, UINT_PTR timer, DWORD) {
    const HWND converter =
        FindWindowW(L"Asterun.ShortcutPathConverter", nullptr);
    if (!converter) return;
    assert(IsWindowVisible(converter));
    assert(!IsWindowEnabled(modalOwner));
    observedPathConverter = true;
    if (checkUiR02ModalDpi) {
        AssertModalFontDpiUpdate(converter, GetDlgItem(converter, 54104), false);
        observedConverterDpiCheck = true;
    }
    ArmModalDestroyProbe(converter);
    KillTimer(nullptr, timer);
    PostMessageW(converter, WM_CLOSE, 0, 0);
}

void CALLBACK InspectKeywordCaretAndCancel(
    HWND,
    UINT,
    UINT_PTR timer,
    DWORD) {

    const HWND editor =
        FindWindowW(
            L"Asterun.ShortcutEditor",
            nullptr);

    if (!editor) {
        return;
    }

    constexpr UINT kKeywordId = 53102;
    const HWND keyword =
        GetDlgItem(
            editor,
            kKeywordId);
    assert(keyword);

    const int length =
        GetWindowTextLengthW(
            keyword);
    assert(length > 0);

    // The product contract is first-focus placement. Force that focus
    // synchronously in the headless runtime fixture instead of depending on
    // whether CI grants this modal the process foreground window.
    SetFocus(
        keyword);

    DWORD selectionStart = 0;
    DWORD selectionEnd = 0;
    SendMessageW(
        keyword,
        EM_GETSEL,
        reinterpret_cast<WPARAM>(
            &selectionStart),
        reinterpret_cast<LPARAM>(
            &selectionEnd));

    observedKeywordCaretAtEnd =
        selectionStart ==
            static_cast<DWORD>(
                length) &&
        selectionEnd ==
            static_cast<DWORD>(
                length);

    KillTimer(
        nullptr,
        timer);
    PostMessageW(
        editor,
        WM_CLOSE,
        0,
        0);
}

void CALLBACK ExerciseAdvancedToggle(
    HWND,
    UINT,
    UINT_PTR timer,
    DWORD) {

    const HWND editor =
        FindWindowW(
            L"Asterun.ShortcutEditor",
            nullptr);

    if (!editor) {
        return;
    }

    constexpr UINT kAdvancedToggleId = 53117;
    constexpr UINT kArgumentsId = 53107;
    constexpr UINT kAdminId = 53111;

    const HWND toggle =
        GetDlgItem(
            editor,
            kAdvancedToggleId);
    const HWND arguments =
        GetDlgItem(
            editor,
            kArgumentsId);
    const HWND admin =
        GetDlgItem(
            editor,
            kAdminId);
    const HWND typeCombo = GetDlgItem(editor, 53104);
    const HWND runtimeCombo = GetDlgItem(editor, 53118);

    assert(typeCombo && runtimeCombo);
    AssertRuntimeHintFits(editor, GetDpiForWindow(editor));
    // Longest runtime mode must not be clipped by the old 185-logical max.
    RECT runtimeBounds{};
    assert(GetWindowRect(runtimeCombo, &runtimeBounds));
    const int requiredRuntimeWidth =
        altrun::ui::MeasureNextComboBoxPreferredWidth(
            runtimeCombo, GetDpiForWindow(editor), 0, 1000);
    assert(runtimeBounds.right - runtimeBounds.left >=
        requiredRuntimeWidth);

    // A mode change must expose/hide the native test EDIT without losing
    // the advanced disclosure's geometry or disturbing the editor owner.
    const HWND testInput = GetDlgItem(editor, 53122);
    assert(testInput);
    // The initial command has RuntimeInputMode::None. Verify the child's
    // own style, not only its parent-dependent effective visibility.
    assert((GetWindowLongPtrW(testInput, GWL_STYLE) & WS_VISIBLE) == 0);
    assert(!IsWindowVisible(testInput));
    for (int mode : {1, 2, 0}) {
        assert(SendMessageW(runtimeCombo, CB_SETCURSEL, mode, 0) == mode);
        SendMessageW(editor, WM_COMMAND,
            MAKEWPARAM(53118, CBN_SELCHANGE),
            reinterpret_cast<LPARAM>(runtimeCombo));
        assert((IsWindowVisible(testInput) != FALSE) == (mode != 0));
        assert(((GetWindowLongPtrW(testInput, GWL_STYLE) &
            WS_VISIBLE) != 0) == (mode != 0));
        AssertRuntimeHintFits(editor, GetDpiForWindow(editor));
        if (mode != 0) {
            AssertNativeEditFrame(editor, testInput, GetDpiForWindow(editor));
            RECT runtimeRow{}, testRow{};
            assert(GetWindowRect(runtimeCombo, &runtimeRow));
            assert(GetWindowRect(testInput, &testRow));
            const int visibleGap = testRow.top - runtimeRow.bottom;
            // Distinguish a visual gap from merely non-overlapping HWNDs.
            const int minimum = altrun::ui::Scale(
                10, GetDpiForWindow(editor));
            if (visibleGap < minimum) {
                std::cerr << "UI-INPUT-01 runtime/test rows too close: "
                    << visibleGap << " < " << minimum << "\n";
            }
            assert(visibleGap >= minimum);
        }
    }

    RECT typeScreen{}, runtimeScreen{};
    assert(GetWindowRect(typeCombo, &typeScreen));
    assert(GetWindowRect(runtimeCombo, &runtimeScreen));
    MapWindowPoints(nullptr, editor,
        reinterpret_cast<POINT*>(&typeScreen), 2);
    MapWindowPoints(nullptr, editor,
        reinterpret_cast<POINT*>(&runtimeScreen), 2);
    const int visibleGap = runtimeScreen.top - typeScreen.bottom;
    const int expectedMidY = (typeScreen.bottom + runtimeScreen.top) / 2;
    if (visibleGap <= 0) {
        std::cerr << "UI01 separator geometry invalid: type.bottom="
            << typeScreen.bottom << " runtime.top=" << runtimeScreen.top << "\n";
    }
    assert(visibleGap > 2);
    RedrawWindow(editor, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
    HDC parentDc = GetDC(editor);
    assert(parentDc);
    const int lineX = MulDiv(36, GetDpiForWindow(editor), 96);
    bool separatorCentered = false;
    for (int dy = -1; dy <= 1; ++dy) {
        if (GetPixel(parentDc, lineX, expectedMidY + dy) ==
            altrun::ui::kApplicationPalette.separator) {
            separatorCentered = true;
        }
    }
    if (!separatorCentered) {
        std::cerr << "UI01 separator NOT centered: type.bottom="
            << typeScreen.bottom << " runtime.top=" << runtimeScreen.top
            << " midY=" << expectedMidY << " gap=" << visibleGap << "\n";
    }
    ReleaseDC(editor, parentDc);
    assert(separatorCentered);

    assert(toggle);
    assert(arguments);
    assert(admin);

    int topLevelRedrawSuspends = 0;
    assert(SetWindowSubclass(
        editor,
        RedrawSuspendProbe,
        23,
        reinterpret_cast<DWORD_PTR>(
            &topLevelRedrawSuspends)));
    int disclosureRedrawSuspends = 0;
    assert(SetWindowSubclass(
        toggle, RedrawSuspendProbe, 24,
        reinterpret_cast<DWORD_PTR>(&disclosureRedrawSuspends)));

    RECT collapsed{};
    RECT expanded{};
    RECT collapsedAgain{};
    RECT toggleBefore{};
    RECT toggleExpanded{};
    RECT toggleCollapsedAgain{};

    assert(GetWindowRect(
        toggle,
        &toggleBefore));

    assert(GetWindowRect(
        editor,
        &collapsed));
    assert(!IsWindowVisible(
        arguments));
    assert(!IsWindowVisible(
        admin));

    SendMessageW(
        editor,
        WM_COMMAND,
        MAKEWPARAM(
            kAdvancedToggleId,
            BN_CLICKED),
        reinterpret_cast<LPARAM>(
            toggle));

    assert(GetWindowRect(
        editor,
        &expanded));
    assert(
        (expanded.bottom -
         expanded.top) >
        (collapsed.bottom -
         collapsed.top));
    assert(IsWindowVisible(
        arguments));
    assert(IsWindowVisible(
        admin));
    assert(GetWindowRect(
        toggle,
        &toggleExpanded));
    assert(EqualRect(
        &toggleBefore,
        &toggleExpanded));
    assert(
        topLevelRedrawSuspends ==
        0);

    // One owner-drawn glyph and one emulated BM_CHECK state owner. The old
    // native BS_AUTOCHECKBOX + paint subclass produced focus/checked residue.
    assert((GetWindowLongPtrW(admin, GWL_STYLE) & BS_TYPEMASK) ==
           BS_OWNERDRAW);
    assert((GetWindowLongPtrW(admin, GWL_STYLE) & WS_TABSTOP) != 0);
    assert(SendMessageW(admin, BM_GETCHECK, 0, 0) == BST_UNCHECKED);
    SendMessageW(admin, BM_CLICK, 0, 0);
    assert(SendMessageW(admin, BM_GETCHECK, 0, 0) == BST_CHECKED);

    RECT adminClient{};
    GetClientRect(admin, &adminClient);
    HDC adminDc = GetDC(admin);
    assert(adminDc);
    HDC sampleDc = CreateCompatibleDC(adminDc);
    assert(sampleDc);
    HBITMAP sampleBitmap = CreateCompatibleBitmap(adminDc,
        adminClient.right, adminClient.bottom);
    assert(sampleBitmap);
    const HGDIOBJ oldSampleBitmap = SelectObject(sampleDc, sampleBitmap);
    assert(oldSampleBitmap);
    PatBlt(sampleDc, 0, 0, adminClient.right, adminClient.bottom, WHITENESS);
    DRAWITEMSTRUCT adminDraw{};
    adminDraw.CtlType = ODT_BUTTON;
    adminDraw.CtlID = kAdminId;
    adminDraw.hwndItem = admin;
    adminDraw.hDC = sampleDc;
    adminDraw.rcItem = adminClient;
    adminDraw.itemState = ODS_FOCUS;
    assert(SendMessageW(editor, WM_DRAWITEM, kAdminId,
        reinterpret_cast<LPARAM>(&adminDraw)) == TRUE);
    const int adminDpi = static_cast<int>(GetDpiForWindow(admin));
    const int glyphSide = MulDiv(16, adminDpi, 96);
    // The production checkbox aligns to CJK visible ink instead of the
    // nominal line-box center. Assert the painted glyph at that position.
    const HFONT alignFont = reinterpret_cast<HFONT>(
        SendMessageW(admin, WM_GETFONT, 0, 0));
    const HGDIOBJ beforeAlign = alignFont
        ? SelectObject(sampleDc, alignFont) : nullptr;
    TEXTMETRICW alignMetrics{};
    assert(GetTextMetricsW(sampleDc, &alignMetrics));
    if (beforeAlign && beforeAlign != HGDI_ERROR)
        SelectObject(sampleDc, beforeAlign);
    const int lineTop = (adminClient.bottom - alignMetrics.tmHeight) / 2;
    const int inkHeight = std::max(1, static_cast<int>(
        alignMetrics.tmAscent - alignMetrics.tmInternalLeading));
    const int glyphTop = lineTop + alignMetrics.tmAscent -
        inkHeight / 2 - glyphSide / 2;
    const int glyphX = MulDiv(2, adminDpi, 96);
    const auto readGlyph = [&]() {
        std::vector<COLORREF> samples;
        for (int y = glyphTop; y < glyphTop + glyphSide; ++y) {
            for (int x = glyphX; x < glyphX + glyphSide; ++x)
                samples.push_back(GetPixel(sampleDc, x, y));
        }
        return samples;
    };
    const auto checkedPixels = readGlyph();
    // With focus and while checked, the untouched gutter must remain the
    // editor's exact white background: no blue native focus-ring residue.
    assert(GetPixel(sampleDc, 0, glyphTop + MulDiv(3, adminDpi, 96)) ==
        altrun::ui::kApplicationPalette.windowBackground);

    // Both states must paint distinctly and show real subpixel edge shades
    // independently of the host OS theme and runner desktop configuration.
    SendMessageW(admin, BM_SETCHECK, BST_UNCHECKED, 0);
    assert(SendMessageW(admin, BM_GETCHECK, 0, 0) == BST_UNCHECKED);
    PatBlt(sampleDc, 0, 0, adminClient.right, adminClient.bottom, WHITENESS);
    assert(SendMessageW(editor, WM_DRAWITEM, kAdminId,
        reinterpret_cast<LPARAM>(&adminDraw)) == TRUE);
    const auto uncheckedPixels = readGlyph();
    // The reduced 16px outline must not leave dark pixels in the
    // surrounding label gutter. Older 18px frames fail this boundary.
    assert(GetPixel(sampleDc, glyphX + glyphSide, adminClient.bottom / 2) ==
        altrun::ui::kApplicationPalette.windowBackground);
    std::set<COLORREF> edgeColors(
        uncheckedPixels.begin(), uncheckedPixels.end());
    int visualChangePixels = 0;
    for (std::size_t i = 0; i < checkedPixels.size(); ++i)
        if (checkedPixels[i] != uncheckedPixels[i]) ++visualChangePixels;
    assert(visualChangePixels >= glyphSide);
    if (edgeColors.size() < 4) {
        std::cerr << "UI01 checkbox outline not smoothed; shades="
            << edgeColors.size() << "\n";
    }
    assert(edgeColors.size() >= 4);
    SendMessageW(admin, BM_SETCHECK, BST_CHECKED, 0);
    SelectObject(sampleDc, oldSampleBitmap);
    DeleteObject(sampleBitmap);
    DeleteDC(sampleDc);
    ReleaseDC(admin, adminDc);
    SendMessageW(admin, BM_CLICK, 0, 0);
    assert(SendMessageW(admin, BM_GETCHECK, 0, 0) == BST_UNCHECKED);
    for (int n = 0; n < 20; ++n) {
        SendMessageW(admin, BM_CLICK, 0, 0);
        assert(SendMessageW(admin, BM_GETCHECK, 0, 0) ==
               (n % 2 == 0 ? BST_CHECKED : BST_UNCHECKED));
    }
    // Owner-drawn BUTTONs deliver BN_DOUBLECLICKED for closely spaced
    // mouse clicks. A BM_CLICK-only test never exercised this path.
    SendMessageW(editor, WM_COMMAND,
        MAKEWPARAM(kAdminId, BN_DOUBLECLICKED),
        reinterpret_cast<LPARAM>(admin));
    assert(SendMessageW(admin, BM_GETCHECK, 0, 0) == BST_CHECKED);
    SendMessageW(editor, WM_COMMAND,
        MAKEWPARAM(kAdminId, BN_CLICKED),
        reinterpret_cast<LPARAM>(admin));
    assert(SendMessageW(admin, BM_GETCHECK, 0, 0) == BST_UNCHECKED);

    // The entire glyph + visible label must be within the clickable HWND.
    RECT adminHit{};
    assert(GetClientRect(admin, &adminHit));
    HDC widthDc = GetDC(admin);
    assert(widthDc);
    const HFONT adminFont = reinterpret_cast<HFONT>(
        SendMessageW(admin, WM_GETFONT, 0, 0));
    HGDIOBJ oldWidthFont = adminFont ? SelectObject(widthDc, adminFont) : nullptr;
    wchar_t adminLabel[128]{};
    GetWindowTextW(admin, adminLabel, 128);
    SIZE adminTextSize{};
    assert(GetTextExtentPoint32W(widthDc, adminLabel,
        lstrlenW(adminLabel), &adminTextSize));
    if (oldWidthFont) SelectObject(widthDc, oldWidthFont);
    ReleaseDC(admin, widthDc);
    const int labelHitMinimum = adminTextSize.cx +
        MulDiv(2 + 16 + 6 + 2, adminDpi, 96);
    assert(adminHit.right - adminHit.left >= labelHitMinimum);

    // Native BUTTON mouse routing (not BM_CLICK): a click on the right edge
    // of the label must toggle, as must the second mouse click of a pair.
    const int labelX = std::min(static_cast<int>(adminHit.right) - 3,
        labelHitMinimum - MulDiv(4, adminDpi, 96));
    const LPARAM pointer = MAKELPARAM(labelX,
        (adminHit.bottom - adminHit.top) / 2);
    SendMessageW(admin, WM_LBUTTONDOWN, MK_LBUTTON, pointer);
    SendMessageW(admin, WM_LBUTTONUP, 0, pointer);
    assert(SendMessageW(admin, BM_GETCHECK, 0, 0) == BST_CHECKED);
    SendMessageW(admin, WM_LBUTTONDBLCLK, MK_LBUTTON, pointer);
    SendMessageW(admin, WM_LBUTTONUP, 0, pointer);
    assert(SendMessageW(admin, BM_GETCHECK, 0, 0) == BST_UNCHECKED);

    // Space from keyboard focus must toggle the same state exactly once.
    SetFocus(admin);
    SendMessageW(admin, WM_KEYDOWN, VK_SPACE, 0);
    SendMessageW(admin, WM_KEYUP, VK_SPACE, 0);
    assert(SendMessageW(admin, BM_GETCHECK, 0, 0) == BST_CHECKED);
    SendMessageW(admin, BM_SETCHECK, BST_UNCHECKED, 0);

    SendMessageW(
        editor,
        WM_COMMAND,
        MAKEWPARAM(
            kAdvancedToggleId,
            BN_CLICKED),
        reinterpret_cast<LPARAM>(
            toggle));

    assert(GetWindowRect(
        editor,
        &collapsedAgain));
    assert(
        (collapsedAgain.bottom -
         collapsedAgain.top) ==
        (collapsed.bottom -
         collapsed.top));
    assert(!IsWindowVisible(
        arguments));
    assert(!IsWindowVisible(
        admin));
    assert(
        topLevelRedrawSuspends ==
        0);

    assert(GetWindowRect(
        toggle,
        &toggleCollapsedAgain));
    assert(EqualRect(
        &toggleBefore,
        &toggleCollapsedAgain));

    // Rapid synchronous toggles must leave the disclosure row at exactly the
    // same screen geometry. A top-level resize is allowed to issue its own
    // WM_ERASEBKGND on some Windows builds, so counting parent erase messages
    // is not a portable proxy for whether the separator itself flickers.
    // End collapsed so the following modal fixtures keep the original size.
    for (int pass = 0;
         pass < 8;
         ++pass) {
        SendMessageW(
            editor,
            WM_COMMAND,
            MAKEWPARAM(
                kAdvancedToggleId,
                BN_CLICKED),
            reinterpret_cast<LPARAM>(
                toggle));
    }
    assert(!IsWindowVisible(
        arguments));
    assert(!IsWindowVisible(
        admin));

    RECT toggleAfterRapid{};
    assert(GetWindowRect(
        toggle,
        &toggleAfterRapid));
    assert(EqualRect(
        &toggleBefore,
        &toggleAfterRapid));

    assert(disclosureRedrawSuspends == 0);
    assert(RemoveWindowSubclass(
        toggle, RedrawSuspendProbe, 24));
    assert(RemoveWindowSubclass(
        editor,
        RedrawSuspendProbe,
        23));

    KillTimer(
        nullptr,
        timer);
    PostMessageW(
        editor,
        WM_CLOSE,
        0,
        0);
}

} // namespace

int main() {
    using namespace altrun;
    namespace presentation = window_presentation;
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    const auto com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const HINSTANCE instance = GetModuleHandleW(nullptr);
    AssertHintSafeFontHeights();
    AssertApplicationTypographyPolicy();
    AssertChineseUiTypeface();

    const HWND owner = CreateWindowExW(WS_EX_TOOLWINDOW, L"STATIC", L"Owner",
        WS_POPUP, -20000, -20000, 200, 100, nullptr, nullptr, instance, nullptr);
    assert(owner);
    {
        presentation::ScopedRedrawSuspend hidden(owner);
        hidden.Resume();
        assert(!IsWindowVisible(owner));
    }
    // A hidden Launcher's stale rectangle must not anchor an external dialog.
    const auto cursorGeometry = presentation::ResolveOwnedPopupGeometry(
        nullptr, instance, 500, 300);
    const auto hiddenGeometry = presentation::ResolveOwnedPopupGeometry(
        owner, instance, 500, 300);
    assert(EqualRect(&cursorGeometry.outer, &hiddenGeometry.outer));

    ShowWindow(owner, SW_SHOWNOACTIVATE);
    {
        presentation::ScopedRedrawSuspend outer(owner);
        assert(!IsWindowVisible(owner));
        {
            presentation::ScopedRedrawSuspend inner(owner);
        }
        assert(!IsWindowVisible(owner));
        outer.Resume();
        assert(IsWindowVisible(owner));
    }
    ShowWindow(owner, SW_HIDE);
    assert(!instance_ipc::GrantForegroundToWindow(nullptr));

    {
        // Exercise the actual Settings implementation, not just rectangle
        // math: the pre-fix Create() made this HWND visible via WM_SETREDRAW.
        const test::IsolatedDataDirectory data(L"window-presentation-tests");
        App app(instance, {}, {}, data.Path());

        // Real first-run regression: when Windows has no Everything service,
        // enabling the provider must persist successfully and enter the local
        // non-downloading bootstrap path. The Settings UI previously collapsed
        // all failures into a misleading administrator-permission message, so
        // keep this exact clean-machine transition under Windows CI.
        if (EverythingServiceIsMissing() &&
            !providers::IsEnabled(
                app.SettingsData().providerEnabled,
                providers::kEverythingFilesystem,
                false)) {
            ProviderChangeDiagnostic diagnostic;

            assert(
                app.SetProviderEnabled(
                    std::string(
                        providers::
                            kEverythingFilesystem),
                    true,
                    false,
                    &diagnostic));
            assert(
                diagnostic.failure ==
                ProviderChangeFailure::None);
            assert(
                providers::IsEnabled(
                    app.SettingsData().providerEnabled,
                    providers::
                        kEverythingFilesystem,
                    false));

            diagnostic = {};

            assert(
                app.SetProviderEnabled(
                    std::string(
                        providers::
                            kEverythingFilesystem),
                    false,
                    false,
                    &diagnostic));
            assert(
                diagnostic.failure ==
                ProviderChangeFailure::None);
            assert(
                !providers::IsEnabled(
                    app.SettingsData().providerEnabled,
                    providers::
                        kEverythingFilesystem,
                    false));
        }

        SettingsWindow settings(app, instance);
        for (int attempt = 0; attempt < 6; ++attempt) {
            assert(settings.Create());
            const HWND window = FindWindowW(L"Asterun.Settings", nullptr);
            assert(window && !IsWindowVisible(window));
            int firstShows = 0;
            assert(SetWindowSubclass(window, FirstShowProbe, 1,
                reinterpret_cast<DWORD_PTR>(&firstShows)));
            settings.ShowAbout();
            assert(IsWindowVisible(window));
            assert(HasAboutHeading(window));
            assert(firstShows == 1);
            RemoveWindowSubclass(window, FirstShowProbe, 1);
            // Visible reopen and both programmatic/native taskbar-style
            // minimized restores keep the About page.
            settings.ShowAbout();

            ShowWindow(
                window,
                SW_MINIMIZE);
            settings.ShowAbout();
            assert(
                !IsIconic(window) &&
                HasAboutHeading(window));

            ShowWindow(
                window,
                SW_MINIMIZE);
            assert(IsIconic(window));
            SendMessageW(
                window,
                WM_SYSCOMMAND,
                SC_RESTORE,
                0);
            assert(
                !IsIconic(window) &&
                IsWindowVisible(window) &&
                HasAboutHeading(window));

            // Both selected and unselected navigation use one Regular font;
            // remaining Settings controls stay on the 10pt body role.
            const auto expectedNavFont = ui::ApplicationFontSpec(
                app.SettingsData().language,
                ui::UiFontRole::SettingsNavigation);
            for (const UINT id : {51001u, 51006u}) {
                const HWND nav = GetDlgItem(window, id);
                AssertBodyFont(nav, GetDpiForWindow(window),
                    expectedNavFont.pointSize);
                const HFONT handle = reinterpret_cast<HFONT>(
                    SendMessageW(nav, WM_GETFONT, 0, 0));
                LOGFONTW font{};
                assert(GetObjectW(handle, sizeof(font), &font) == sizeof(font));
                assert(lstrcmpiW(
                    font.lfFaceName, expectedNavFont.face) == 0);
            }
            // A default hotkey's Reset button must never become visible even
            // for a transient frame when opening the Hotkeys page.
            SendMessageW(window, WM_COMMAND, MAKEWPARAM(51001, BN_CLICKED), 0);
            std::array<HWND, 20> resetLinks{};
            std::size_t resetCount = 0;
            int prematureResetShows = 0;
            for (UINT index = 0; index < resetLinks.size(); ++index) {
                const HWND reset = GetDlgItem(window, 51760 + index);
                if (!reset) continue;
                assert(!IsWindowVisible(reset));
                assert(SetWindowSubclass(reset, HotkeyResetExposureProbe,
                    91, reinterpret_cast<DWORD_PTR>(&prematureResetShows)));
                resetLinks[resetCount++] = reset;
            }
            assert(resetCount > 0);
            for (int pass = 0; pass < 3; ++pass) {
                SendMessageW(window, WM_COMMAND, MAKEWPARAM(51006, BN_CLICKED), 0);
                for (std::size_t index = 0; index < resetCount; ++index) {
                    assert(!IsWindowVisible(resetLinks[index]));
                }
                SendMessageW(window, WM_COMMAND, MAKEWPARAM(51001, BN_CLICKED), 0);
            }
            assert(prematureResetShows == 0);
            for (std::size_t index = 0; index < resetCount; ++index) {
                assert(RemoveWindowSubclass(resetLinks[index],
                    HotkeyResetExposureProbe, 91));
            }

            // Reproduce the real high-DPI/small-work-area path where General
            // genuinely needs scrolling. Settings now owns an in-client
            // overlay indicator, so the top-level HWND must never regain a
            // native WS_VSCROLL frame or USER32 scroll range.
            RECT normalSettingsRect{};
            assert(GetWindowRect(
                window,
                &normalSettingsRect));
            const UINT settingsDpi =
                GetDpiForWindow(window);
            const int compactOuterHeight =
                MulDiv(
                    360,
                    static_cast<int>(
                        settingsDpi),
                    96);

            SetWindowPos(
                window,
                nullptr,
                0,
                0,
                normalSettingsRect.right -
                    normalSettingsRect.left,
                compactOuterHeight,
                SWP_NOMOVE |
                    SWP_NOZORDER |
                    SWP_NOACTIVATE);

            const auto assertNoNativeSettingsScroll =
                [&]() {
                    assert(
                        !(GetWindowLongPtrW(
                              window,
                              GWL_STYLE) &
                          WS_VSCROLL));

                    SCROLLINFO cleared{};
                    cleared.cbSize =
                        sizeof(cleared);
                    cleared.fMask =
                        SIF_RANGE |
                        SIF_PAGE |
                        SIF_POS;

                    if (GetScrollInfo(
                            window,
                            SB_VERT,
                            &cleared)) {
                        assert(
                            cleared.nMin == 0);
                        assert(
                            cleared.nMax == 0);
                        assert(
                            cleared.nPos == 0);
                    }
                };

            SendMessageW(
                window,
                WM_COMMAND,
                MAKEWPARAM(
                    51001,
                    BN_CLICKED),
                0);
            assertNoNativeSettingsScroll();

            // Wheel scrolling must move the active page immediately without
            // reviving the native scrollbar. Returning to the original offset
            // also verifies that the synchronous repaint path leaves layout
            // coordinates stable.
            const HWND startWithWindows =
                GetDlgItem(
                    window,
                    51100);
            assert(startWithWindows);

            RECT beforeWheel{};
            RECT afterWheelDown{};
            RECT afterWheelUp{};
            int topLevelRedrawSuspends = 0;

            assert(SetWindowSubclass(
                window,
                RedrawSuspendProbe,
                3,
                reinterpret_cast<DWORD_PTR>(
                    &topLevelRedrawSuspends)));

            assert(GetWindowRect(
                startWithWindows,
                &beforeWheel));

            SendMessageW(
                window,
                WM_MOUSEWHEEL,
                MAKEWPARAM(
                    0,
                    static_cast<WORD>(
                        static_cast<SHORT>(
                            -WHEEL_DELTA))),
                0);

            assert(GetWindowRect(
                startWithWindows,
                &afterWheelDown));
            assert(
                afterWheelDown.top <
                beforeWheel.top);
            assertNoNativeSettingsScroll();

            SendMessageW(
                window,
                WM_MOUSEWHEEL,
                MAKEWPARAM(
                    0,
                    static_cast<WORD>(
                        static_cast<SHORT>(
                            WHEEL_DELTA))),
                0);

            assert(GetWindowRect(
                startWithWindows,
                &afterWheelUp));
            assert(
                afterWheelUp.top ==
                beforeWheel.top);
            assertNoNativeSettingsScroll();

            // Scrolling must never suspend redraw on the top-level Settings
            // HWND. Doing so briefly changes USER32's visible/hit-test state
            // and can route a subsequent wheel message to the window behind.
            assert(
                topLevelRedrawSuspends ==
                0);
            assert(RemoveWindowSubclass(
                window,
                RedrawSuspendProbe,
                3));

            int topLevelPageSwitchRedrawSuspends = 0;
            assert(SetWindowSubclass(
                window,
                RedrawSuspendProbe,
                4,
                reinterpret_cast<DWORD_PTR>(
                    &topLevelPageSwitchRedrawSuspends)));

            for (const UINT pageId :
                 {51006u, 51005u, 51002u, 51003u, 51004u}) {
                SendMessageW(
                    window,
                    WM_COMMAND,
                    MAKEWPARAM(
                        pageId,
                        BN_CLICKED),
                    0);
                assertNoNativeSettingsScroll();

                SendMessageW(
                    window,
                    WM_COMMAND,
                    MAKEWPARAM(
                        51001,
                        BN_CLICKED),
                    0);
                assertNoNativeSettingsScroll();
            }

            // Navigation must keep the top-level Settings window continuously
            // visible. Temporarily disabling top-level redraw can expose the
            // Explorer/browser behind it for a frame.
            assert(
                topLevelPageSwitchRedrawSuspends ==
                0);
            assert(RemoveWindowSubclass(
                window,
                RedrawSuspendProbe,
                4));

            SetWindowPos(
                window,
                nullptr,
                0,
                0,
                normalSettingsRect.right -
                    normalSettingsRect.left,
                normalSettingsRect.bottom -
                    normalSettingsRect.top,
                SWP_NOMOVE |
                    SWP_NOZORDER |
                    SWP_NOACTIVATE);

            // Toggling one General setting must remain row-local. The old
            // success path called RefreshFromSettings(), invalidating every
            // switch row and making unrelated controls flash on rapid clicks.
            SendMessageW(
                window,
                WM_COMMAND,
                MAKEWPARAM(
                    51001,
                    BN_CLICKED),
                0);

            const HWND defaultEnglish =
                GetDlgItem(
                    window,
                    51133);
            const HWND pinyin =
                GetDlgItem(
                    window,
                    51134);
            assert(defaultEnglish);
            assert(pinyin);

            RedrawWindow(
                pinyin,
                nullptr,
                nullptr,
                RDW_INVALIDATE |
                    RDW_ERASE |
                    RDW_UPDATENOW);

            PaintMessageCounts siblingPaint{};
            assert(SetWindowSubclass(
                pinyin,
                PaintMessageProbe,
                5,
                reinterpret_cast<DWORD_PTR>(
                    &siblingPaint)));

            const bool originalEnglishPreference =
                app.SettingsData()
                    .defaultEnglishInputOnReveal;

            SendMessageW(
                window,
                WM_COMMAND,
                MAKEWPARAM(
                    51133,
                    BN_CLICKED),
                reinterpret_cast<LPARAM>(
                    defaultEnglish));

            assert(
                app.SettingsData()
                    .defaultEnglishInputOnReveal !=
                originalEnglishPreference);
            assert(siblingPaint.paint == 0);
            assert(siblingPaint.erase == 0);

            SendMessageW(
                window,
                WM_COMMAND,
                MAKEWPARAM(
                    51133,
                    BN_CLICKED),
                reinterpret_cast<LPARAM>(
                    defaultEnglish));

            assert(
                app.SettingsData()
                    .defaultEnglishInputOnReveal ==
                originalEnglishPreference);
            assert(siblingPaint.paint == 0);
            assert(siblingPaint.erase == 0);

            assert(RemoveWindowSubclass(
                pinyin,
                PaintMessageProbe,
                5));

            if (attempt == 0) {
                // Exercise real owner-drawn BUTTON input, not fake WM_COMMAND.
                // General is the visual/interaction reference for all pages.
                SetActiveWindow(window);
                int topLevelSuspends = 0;
                assert(SetWindowSubclass(window, RedrawSuspendProbe, 60,
                    reinterpret_cast<DWORD_PTR>(&topLevelSuspends)));
                const bool originalGeneral =
                    app.SettingsData().defaultEnglishInputOnReveal;
                for (int n = 0; n < 8; ++n) {
                    SendMessageW(defaultEnglish, BM_CLICK, 0, 0);
                    assert(app.SettingsData().defaultEnglishInputOnReveal ==
                        (n % 2 == 0 ? !originalGeneral : originalGeneral));
                    assert(IsWindowVisible(window));
                }
                SendMessageW(window, WM_COMMAND,
                    MAKEWPARAM(51006, BN_CLICKED), 0);
                std::size_t exitIndex = 0;
                for (const auto& action : HotkeyActionRegistry()) {
                    if (action.id == hotkey_actions::kExitApplication) break;
                    ++exitIndex;
                }
                assert(exitIndex + 1 < HotkeyActionRegistry().size());
                const HWND hotkey = GetDlgItem(window, 51740 + exitIndex);
                const HWND hotkeySibling = GetDlgItem(window, 51740 + exitIndex + 1);
                const HWND sidebar = GetDlgItem(window, 51001);
                assert(hotkey && hotkeySibling && sidebar);
                assert(IsWindowVisible(hotkey));
                PaintMessageCounts hotkeySiblingPaint{}, sidebarPaint{};
                assert(SetWindowSubclass(hotkeySibling, PaintMessageProbe, 61,
                    reinterpret_cast<DWORD_PTR>(&hotkeySiblingPaint)));
                assert(SetWindowSubclass(sidebar, PaintMessageProbe, 62,
                    reinterpret_cast<DWORD_PTR>(&sidebarPaint)));
                const bool originalHotkey = EffectiveHotkeyBinding(
                    app.SettingsData().hotkeyBindings,
                    hotkey_actions::kExitApplication).enabled;
                for (int n = 0; n < 8; ++n) {
                    SendMessageW(hotkey, BM_CLICK, 0, 0);
                    assert(EffectiveHotkeyBinding(
                        app.SettingsData().hotkeyBindings,
                        hotkey_actions::kExitApplication).enabled ==
                        (n % 2 == 0 ? !originalHotkey : originalHotkey));
                    assert(IsWindowVisible(window) && topLevelSuspends == 0);
                    UpdateWindow(hotkeySibling);
                    UpdateWindow(sidebar);
                    assert(hotkeySiblingPaint.paint == 0 &&
                           hotkeySiblingPaint.erase == 0 &&
                           sidebarPaint.paint == 0 &&
                           sidebarPaint.erase == 0);
                }
                assert(RemoveWindowSubclass(sidebar, PaintMessageProbe, 62));
                assert(RemoveWindowSubclass(hotkeySibling, PaintMessageProbe, 61));

                SendMessageW(window, WM_COMMAND,
                    MAKEWPARAM(51005, BN_CLICKED), 0);
                const HWND provider = GetDlgItem(window, 51601);
                const HWND sibling = GetDlgItem(window, 51602);
                assert(provider && sibling && IsWindowVisible(provider));
                PaintMessageCounts sourceSiblingPaint{};
                assert(SetWindowSubclass(sibling, PaintMessageProbe, 63,
                    reinterpret_cast<DWORD_PTR>(&sourceSiblingPaint)));
                const bool originalProvider = providers::IsEnabled(
                    app.SettingsData().providerEnabled, providers::kStartMenu, true);
                const ULONGLONG begin = GetTickCount64();
                for (int n = 0; n < 8; ++n) {
                    SendMessageW(provider, BM_CLICK, 0, 0);
                    UpdateWindow(sibling);
                    assert(IsWindowVisible(window) && topLevelSuspends == 0);
                    assert(sourceSiblingPaint.paint == 0 &&
                           sourceSiblingPaint.erase == 0);
                }
                SendMessageW(window, WM_TIMER, 0x51691, 0);
                assert(providers::IsEnabled(app.SettingsData().providerEnabled,
                    providers::kStartMenu, true) == originalProvider);
                assert(RemoveWindowSubclass(sibling, PaintMessageProbe, 63));

                // Unchanged Everything status must not set identical text on
                // every 1-second poll and indirectly invalidate other rows.
                HWND status = nullptr;
                EnumChildWindows(window, [](HWND child, LPARAM data) -> BOOL {
                    wchar_t name[24]{}, label[512]{};
                    GetClassNameW(child, name, 24);
                    if (lstrcmpiW(name, L"STATIC") != 0 ||
                        !IsWindowVisible(child)) return TRUE;
                    GetWindowTextW(child, label, 512);
                    if (std::wstring_view(label).find(L"Everything") !=
                        std::wstring_view::npos &&
                        (label[0] == L'○' || label[0] == L'◌' ||
                         label[0] == L'⚠')) {
                        *reinterpret_cast<HWND*>(data) = child;
                        return FALSE;
                    }
                    return TRUE;
                }, reinterpret_cast<LPARAM>(&status));
                assert(status);
                int redundantTextUpdates = 0;
                assert(SetWindowSubclass(status, TextChangeProbe, 64,
                    reinterpret_cast<DWORD_PTR>(&redundantTextUpdates)));
                for (int n = 0; n < 3; ++n)
                    SendMessageW(window, WM_TIMER, 0x51690, 0);
                assert(redundantTextUpdates == 0);
                assert(RemoveWindowSubclass(status, TextChangeProbe, 64));
                assert(RemoveWindowSubclass(window, RedrawSuspendProbe, 60));
                std::cout << "Native provider clicks + stable status: "
                          << (GetTickCount64() - begin) << " ms\n";
                SendMessageW(window, WM_COMMAND,
                    MAKEWPARAM(51001, BN_CLICKED), 0);
            }

            // Repeated General -> Sources -> Appearance -> Sources must keep
            // overlay scrolling frame-neutral on the first visit, including
            // high DPI.
            for (int pass = 0; pass < 3; ++pass) {
                SendMessageW(window, WM_COMMAND, MAKEWPARAM(51001, BN_CLICKED), 0);
                SendMessageW(window, WM_COMMAND, MAKEWPARAM(51005, BN_CLICKED), 0);
                assert(!(GetWindowLongPtrW(window, GWL_STYLE) & WS_VSCROLL));
                assert(!IsWindowVisible(GetDlgItem(window, 51608)));
                RECT first{};
                GetWindowRect(GetDlgItem(window, 51605), &first);
                SendMessageW(window, WM_COMMAND, MAKEWPARAM(51002, BN_CLICKED), 0);
                SendMessageW(window, WM_COMMAND, MAKEWPARAM(51005, BN_CLICKED), 0);
                RECT second{};
                GetWindowRect(GetDlgItem(window, 51605), &second);
                assert(EqualRect(&first, &second));
                assert(!(GetWindowLongPtrW(window, GWL_STYLE) & WS_VSCROLL));
                assert(!IsWindowVisible(GetDlgItem(window, 51608)));
            }
            ShowWindow(window, SW_HIDE);
            settings.RefreshFromSettings();
            assert(!IsWindowVisible(window));
            DestroyWindow(window); // no position persistence in this fixture
        }

        modalOwner = owner;
        Command seed;
        seed.title = L"Runtime fixture";
        seed.target = L"C:\\fixture.exe";
        for (const bool enabled : {true, false}) {
            EnableWindow(owner, enabled);
            expectedOwnerEnabledAtModalDestroy = enabled;
            observedEditor = false;
            observedModalDestroy = false;
            const auto timer = SetTimer(nullptr, 0, 20, CancelEditor);
            assert(timer);
            assert(!ShortcutEditorDialog::ShowNew(app, instance, owner, seed));
            KillTimer(nullptr, timer);
            assert(observedEditor);
            assert(observedModalDestroy);
            assert(ownerEnabledAtModalDestroy == enabled);
            assert((IsWindowEnabled(owner) != FALSE) == enabled);
            assert(!IsWindowVisible(owner));
            assert(GetActiveWindow() != owner);
        }

        // Editing an existing shortcut should place the insertion caret at
        // the end of the keyword string instead of position zero.
        Command caretProbe;
        caretProbe.keyword = L"caretprobe";
        caretProbe.title = L"Caret probe";
        caretProbe.target = L"notepad.exe";
        caretProbe.type = CommandType::Application;
        caretProbe.enabled = true;

        std::wstring caretProbeId;
        assert(app.CreateUserCommand(
            caretProbe,
            &caretProbeId));
        assert(!caretProbeId.empty());

        observedKeywordCaretAtEnd = false;
        modalOwner = owner;
        EnableWindow(
            owner,
            TRUE);
        const auto caretTimer =
            SetTimer(
                nullptr,
                0,
                20,
                InspectKeywordCaretAndCancel);
        assert(caretTimer);
        assert(
            !ShortcutEditorDialog::Show(
                app,
                instance,
                owner,
                caretProbeId));
        KillTimer(
            nullptr,
            caretTimer);
        assert(observedKeywordCaretAtEnd);
        assert(app.DeleteUserCommand(
            caretProbeId));

        // Advanced disclosure must expand/collapse to the correct final
        // geometry without ever suspending redraw on the top-level editor.
        // Exact native child-window position-message counts are intentionally
        // not asserted: BUTTON/EDIT bookkeeping can emit extra position
        // notifications without representing an extra visible layout frame.
        EnableWindow(
            owner,
            TRUE);
        ShowWindow(
            owner,
            SW_HIDE);
        modalOwner = owner;
        const auto advancedTimer =
            SetTimer(
                nullptr,
                0,
                20,
                ExerciseAdvancedToggle);
        assert(advancedTimer);
        assert(
            !ShortcutEditorDialog::ShowNew(
                app,
                instance,
                owner,
                seed));
        KillTimer(
            nullptr,
            advancedTimer);
        assert(
            IsWindowEnabled(
                owner));

        // A visible Shortcut Manager owner must already be re-enabled before
        // either modal child is destroyed. The old ordering destroyed the
        // active popup first, which let USER32 hand activation elsewhere and
        // then bounce back to the manager as a visible one-frame flash.
        EnableWindow(owner, TRUE);
        ShowWindow(owner, SW_SHOWNOACTIVATE);
        assert(IsWindowVisible(owner));
        expectedOwnerEnabledAtModalDestroy = true;

        observedEditor = false;
        observedModalDestroy = false;
        auto timer = SetTimer(nullptr, 0, 20, CancelEditor);
        assert(timer);
        assert(!ShortcutEditorDialog::ShowNew(app, instance, owner, seed));
        KillTimer(nullptr, timer);
        assert(observedEditor && observedModalDestroy);
        assert(ownerEnabledAtModalDestroy);
        assert(IsWindowEnabled(owner));
        assert(IsWindowVisible(owner));

        observedPathConverter = false;
        observedModalDestroy = false;
        timer = SetTimer(nullptr, 0, 20, CancelPathConverter);
        assert(timer);
        assert(!ShortcutPathConverterDialog::Show(app, instance, owner));
        KillTimer(nullptr, timer);
        assert(observedPathConverter && observedModalDestroy);
        assert(ownerEnabledAtModalDestroy);
        assert(IsWindowEnabled(owner));
        assert(IsWindowVisible(owner));

        // One actual modal per type performs a synthetic cross-monitor DPI
        // roundtrip while visible. Later resource-soak cycles stay unchanged.
        observedEditorDpiCheck = false;
        observedConverterDpiCheck = false;
        checkUiR02ModalDpi = true;
        expectedEditorBodyPointSize = ui::ApplicationFontSpec(
            app.SettingsData().language,
            ui::UiFontRole::ShortcutBody).pointSize;
        timer = SetTimer(nullptr, 0, 20, CancelEditor);
        assert(timer);
        assert(!ShortcutEditorDialog::ShowNew(app, instance, owner, seed));
        KillTimer(nullptr, timer);
        timer = SetTimer(nullptr, 0, 20, CancelPathConverter);
        assert(timer);
        assert(!ShortcutPathConverterDialog::Show(app, instance, owner));
        KillTimer(nullptr, timer);
        checkUiR02ModalDpi = false;
        assert(observedEditorDpiCheck && observedConverterDpiCheck);
        assert(IsWindowEnabled(owner));
        assert(IsWindowVisible(owner));

        ShowWindow(owner, SW_HIDE);

        ShortcutManagerWindow manager(
            app,
            instance);

        const auto runManagerModalCycle =
            [&]() {
                manager.Show();

                const HWND managerWindow =
                    FindWindowW(
                        L"Asterun.ShortcutManager",
                        nullptr);

                assert(
                    managerWindow &&
                    IsWindowVisible(
                        managerWindow));
                AssertNativeEditFrame(managerWindow,
                    GetDlgItem(managerWindow, 52106),
                    GetDpiForWindow(managerWindow));
                assert(
                    IsWindowEnabled(
                        managerWindow));

                // Exercise the same native SC_RESTORE path used when a user
                // clicks the minimized taskbar button.
                ShowWindow(
                    managerWindow,
                    SW_MINIMIZE);
                assert(
                    IsIconic(
                        managerWindow));
                SendMessageW(
                    managerWindow,
                    WM_SYSCOMMAND,
                    SC_RESTORE,
                    0);
                assert(
                    !IsIconic(
                        managerWindow) &&
                    IsWindowVisible(
                        managerWindow));

                modalOwner =
                    managerWindow;
                expectedOwnerEnabledAtModalDestroy =
                    true;

                observedEditor = false;
                observedModalDestroy = false;
                auto timer =
                    SetTimer(
                        nullptr,
                        0,
                        20,
                        CancelEditor);
                assert(timer);
                assert(
                    !ShortcutEditorDialog::
                        ShowNew(
                            app,
                            instance,
                            managerWindow,
                            seed));
                KillTimer(
                    nullptr,
                    timer);
                assert(
                    observedEditor &&
                    observedModalDestroy);
                assert(
                    IsWindowEnabled(
                        managerWindow));

                observedPathConverter =
                    false;
                observedModalDestroy = false;
                timer =
                    SetTimer(
                        nullptr,
                        0,
                        20,
                        CancelPathConverter);
                assert(timer);
                assert(
                    !ShortcutPathConverterDialog::
                        Show(
                            app,
                            instance,
                            managerWindow));
                KillTimer(
                    nullptr,
                    timer);
                assert(
                    observedPathConverter &&
                    observedModalDestroy);
                assert(
                    IsWindowEnabled(
                        managerWindow));

                SendMessageW(
                    managerWindow,
                    WM_CLOSE,
                    0,
                    0);

                assert(
                    FindWindowW(
                        L"Asterun.ShortcutManager",
                        nullptr) == nullptr);
            };

        // Warm common controls/window classes before taking the leak baseline.
        runManagerModalCycle();

        const auto resourcesBefore =
            ProcessResources();

        constexpr int kSoakCycles = 24;

        for (int cycle = 0;
             cycle < kSoakCycles;
             ++cycle) {
            runManagerModalCycle();
        }

        const auto resourcesAfter =
            ProcessResources();

        AssertNoResourceGrowth(
            resourcesBefore,
            resourcesAfter);

        std::cout
            << "Resource soak (" << kSoakCycles
            << " manager/editor/converter cycles): GDI "
            << resourcesBefore.gdi << " -> "
            << resourcesAfter.gdi << ", USER "
            << resourcesBefore.user << " -> "
            << resourcesAfter.user << ", handles "
            << resourcesBefore.handles << " -> "
            << resourcesAfter.handles << "\n";

        // LauncherWindow::WM_DESTROY posts WM_QUIT because it owns the real
        // application message loop. Keep the numeric-input fixture last in
        // this process so its intentional real-window teardown cannot make a
        // later modal RunModal() observe WM_QUIT and exit before its timer
        // callback runs.
        NumericIntentRuntimeFixture::Run(
            app,
            instance);
    }
    DestroyWindow(owner);
    if (SUCCEEDED(com)) CoUninitialize();
    std::cout << "Actual Settings/About and modal presentation checks passed\n";
}
