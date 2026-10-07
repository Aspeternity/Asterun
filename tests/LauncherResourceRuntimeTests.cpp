#include "app/App.hpp"
#include "core/EverythingProvider.hpp"
#include "ui/LauncherWindow.hpp"
#include "NumericIntentRuntimeFixture.hpp"
#include <windows.h>
#include <imm.h>
#include <psapi.h>
#include <cassert>
#include <cstring>
#include <iostream>
#include <vector>
#include <unordered_set>

namespace {
unsigned loads{}, failLoad{};
bool failInfo{}, failDc{};
unsigned imeContextGets{};
unsigned imeOpenStatusReads{};
unsigned imeCompositionCancels{};
unsigned imeSetOpenRequests{};
unsigned imeContextReleases{};
BOOL imeCurrentOpenStatus{TRUE};
std::unordered_set<HGDIOBJ> ownedBitmaps;
std::unordered_set<HDC> ownedDcs;
HANDLE WINAPI TestLoadImage(HINSTANCE instance, LPCWSTR name, UINT type, int x, int y, UINT flags) {
    if (type == IMAGE_BITMAP && ++loads == failLoad) return nullptr;
    HANDLE image = LoadImageW(instance, name, type, x, y, flags);
    if (type == IMAGE_BITMAP && image) assert(ownedBitmaps.insert(image).second);
    return image;
}
int WINAPI TestGetObject(HANDLE object, int bytes, LPVOID info) {
    return failInfo ? 0 : GetObjectW(object, bytes, info);
}
HDC WINAPI TestCreateDc(HDC dc) {
    if (failDc) return nullptr;
    HDC created = CreateCompatibleDC(dc);
    if (created) assert(ownedDcs.insert(created).second);
    return created;
}
BOOL WINAPI TestDeleteObject(HGDIOBJ object) {
    const BOOL result = DeleteObject(object);
    if (ownedBitmaps.contains(object)) { assert(result); ownedBitmaps.erase(object); }
    return result;
}
BOOL WINAPI TestDeleteDc(HDC dc) {
    const BOOL result = DeleteDC(dc);
    if (ownedDcs.contains(dc)) { assert(result); ownedDcs.erase(dc); }
    return result;
}
HIMC WINAPI TestImmGetContext(HWND window) {
    assert(window);
    ++imeContextGets;
    return reinterpret_cast<HIMC>(
        static_cast<ULONG_PTR>(1));
}
BOOL WINAPI TestImmGetOpenStatus(HIMC context) {
    assert(context);
    ++imeOpenStatusReads;
    return imeCurrentOpenStatus;
}
BOOL WINAPI TestImmNotifyIME(HIMC context, DWORD action, DWORD index, DWORD value) {
    assert(context);
    assert(action == NI_COMPOSITIONSTR);
    assert(index == CPS_CANCEL);
    assert(value == 0);
    ++imeCompositionCancels;
    return TRUE;
}
BOOL WINAPI TestImmSetOpenStatus(HIMC context, BOOL open) {
    assert(context);
    ++imeSetOpenRequests;
    imeCurrentOpenStatus = open;
    return TRUE;
}
BOOL WINAPI TestImmReleaseContext(HWND window, HIMC context) {
    assert(window);
    assert(context);
    ++imeContextReleases;
    return TRUE;
}
void ResetImeProbe(BOOL openStatus = TRUE) {
    imeContextGets = 0;
    imeOpenStatusReads = 0;
    imeCompositionCancels = 0;
    imeSetOpenRequests = 0;
    imeContextReleases = 0;
    imeCurrentOpenStatus = openStatus;
}
void ClearImeProbeCounts() {
    imeContextGets = 0;
    imeOpenStatusReads = 0;
    imeCompositionCancels = 0;
    imeSetOpenRequests = 0;
    imeContextReleases = 0;
}
}
#define DeleteObject TestDeleteObject
#define DeleteDC TestDeleteDc
#define LoadImageW TestLoadImage
#define GetObjectW TestGetObject
#define CreateCompatibleDC TestCreateDc
#define ImmGetContext TestImmGetContext
#define ImmGetOpenStatus TestImmGetOpenStatus
#define ImmNotifyIME TestImmNotifyIME
#define ImmSetOpenStatus TestImmSetOpenStatus
#define ImmReleaseContext TestImmReleaseContext
#include "../src/ui/LauncherWindow.cpp"
#undef ImmReleaseContext
#undef ImmSetOpenStatus
#undef ImmNotifyIME
#undef ImmGetOpenStatus
#undef ImmGetContext
#undef DeleteDC
#undef DeleteObject
#undef CreateCompatibleDC
#undef GetObjectW
#undef LoadImageW

// Include COM after the product TU: rpcndr.h defines the legacy small macro.
#include <objbase.h>

namespace {
struct Resources {
    DWORD gdi, user, handles;
    SIZE_T privateBytes, workingSet;
};
Resources Sample(const char* label) {
    PROCESS_MEMORY_COUNTERS_EX memory{}; memory.cb = sizeof(memory);
    assert(GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memory), sizeof(memory)));
    Resources r{GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS), GetGuiResources(GetCurrentProcess(), GR_USEROBJECTS)};
    assert(GetProcessHandleCount(GetCurrentProcess(), &r.handles));
    r.privateBytes = memory.PrivateUsage; r.workingSet = memory.WorkingSetSize;
    std::cout << label << ",PrivateBytes=" << r.privateBytes << ",WorkingSet=" << r.workingSet
        << ",GDI=" << r.gdi << ",USER=" << r.user << ",Handles=" << r.handles << '\n';
    return r;
}
void DrainQuit() { MSG msg{}; while (PeekMessageW(&msg, nullptr, WM_QUIT, WM_QUIT, PM_REMOVE)) {} }
}
namespace altrun {
struct LauncherResourceRuntimeFixture {
    static void Empty(const LauncherWindow& w) {
        assert(!w.classicBitmapDc_ && !w.classicBackgroundBitmap_);
        for (auto h : w.classicShortcutBitmaps_) assert(!h);
        for (auto h : w.classicCloseBitmaps_) assert(!h);
        assert(w.classicBackgroundSize_.cx == 0 && w.classicBackgroundSize_.cy == 0);
    }
    static void Destroy(LauncherWindow& w) { if (w.hwnd_) DestroyWindow(w.hwnd_); DrainQuit(); }
    static void Dpi(LauncherWindow& w, UINT dpi) {
        RECT rect{}; GetWindowRect(w.hwnd_, &rect);
        SendMessageW(w.hwnd_, WM_DPICHANGED, MAKEWPARAM(dpi, dpi), reinterpret_cast<LPARAM>(&rect));
        assert(w.dpi_ == dpi);
    }
    static void VerifyTypography(App& app, HINSTANCE instance) {
        auto& settings = const_cast<Settings&>(app.SettingsData());
        const auto previousLanguage = settings.language;
        const auto previousStyle = settings.uiStyle;
        settings.uiStyle = UiStyle::ModernCompact;
        for (auto language : {Language::ZhCN, Language::EnUS}) {
            settings.language = language;
            LauncherWindow window(app, instance); assert(window.Create());
            for (UINT dpi : {96U, 120U, 144U, 168U, 192U, 240U, 288U}) {
                Dpi(window, dpi);
                HDC dc = GetDC(window.edit_); assert(dc);
                RECT edit{}; assert(GetClientRect(window.edit_, &edit));
                assert(reinterpret_cast<HFONT>(SendMessageW(window.edit_, WM_GETFONT, 0, 0)) == window.searchFont_);
                const auto spec = ui::LauncherFontSpec(settings.uiStyle, language, ui::UiFontRole::LauncherSearch);
                const bool cjkFace = lstrcmpW(spec.face, L"Segoe UI") != 0;
                for (HFONT font : {window.searchFont_, window.normalFont_, window.boldFont_, window.auxiliaryFont_}) {
                    assert(font);
                    LOGFONTW logical{};
                    assert(GetObjectW(font, sizeof(logical), &logical) == sizeof(logical));
                    assert(lstrcmpW(logical.lfFaceName, spec.face) == 0 && logical.lfWeight == FW_NORMAL);
                    auto old = SelectObject(dc, font); assert(old && old != HGDI_ERROR);
                    TEXTMETRICW metrics{}; assert(GetTextMetricsW(dc, &metrics));
                    if (font == window.searchFont_) {
                        // Measure the realized font, not just its requested em height.
                        assert(metrics.tmHeight <= edit.bottom - edit.top);
                    }
                    const wchar_t* sample = cjkFace
                        ? L"腾讯会议 中文输入 Palworld 幻兽帕鲁 PowerShell"
                        : L"PowerShell Palworld 0123456789";
                    const int length = lstrlenW(sample);
                    std::vector<WORD> glyphs(static_cast<std::size_t>(length));
                    assert(GetGlyphIndicesW(dc, sample, length, glyphs.data(), GGI_MARK_NONEXISTING_GLYPHS) != GDI_ERROR);
                    for (WORD glyph : glyphs) assert(glyph != 0xffff);
                    SIZE extent{}; assert(GetTextExtentPoint32W(dc, sample, length, &extent));
                    assert(extent.cx > 0 && extent.cy > 0);
                    SelectObject(dc, old);
                }
                assert(ReleaseDC(window.edit_, dc));
                std::cout << "TYPOGRAPHY language=" << (language == Language::ZhCN ? "zh" : "en")
                    << " dpi=" << dpi << " direct-CJK-glyphs=" << cjkFace << std::endl;
            }
            Destroy(window);
        }
        settings.language = previousLanguage;
        settings.uiStyle = previousStyle;
    }
    static std::vector<unsigned char> Pixels(LauncherWindow& w) {
        constexpr int width = 900, height = 300;
        BITMAPINFO info{}; info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = width; info.bmiHeader.biHeight = -height;
        info.bmiHeader.biPlanes = 1; info.bmiHeader.biBitCount = 32; info.bmiHeader.biCompression = BI_RGB;
        void* bits{}; HBITMAP bitmap = CreateDIBSection(nullptr, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
        HDC dc = CreateCompatibleDC(nullptr); assert(bitmap && dc);
        auto old = SelectObject(dc, bitmap);
        std::memset(bits, 0, width * height * 4);
        RECT client{0, 0, width, height};
        w.PaintClassicBackground(dc, client);
        w.PaintClassicLogo(dc, 20, 20);
        RECT close{width - 70, 20, width - 20, 70};
        w.PaintClassicClose(dc, close);
        GdiFlush();
        auto* begin = static_cast<unsigned char*>(bits);
        std::vector<unsigned char> result(begin, begin + width * height * 4);
        SelectObject(dc, old); DeleteDC(dc); DeleteObject(bitmap);
        return result;
    }
    struct ShowProbe {
        LauncherWindow* window;
        unsigned changes{};
        std::uint64_t editRefreshes{};
    };
    static LRESULT CALLBACK ObserveChange(HWND hwnd, UINT message, WPARAM w, LPARAM l,
                                         UINT_PTR, DWORD_PTR data) {
        auto& probe = *reinterpret_cast<ShowProbe*>(data);
        const bool change = message == WM_COMMAND && LOWORD(w) == 1001 && HIWORD(w) == EN_CHANGE;
        const auto before = probe.window->searchGeneration_;
        const auto result = DefSubclassProc(hwnd, message, w, l);
        if (change) {
            ++probe.changes;
            probe.editRefreshes += probe.window->searchGeneration_ - before;
        }
        return result;
    }
    static LRESULT CALLBACK OmitResetNotification(HWND hwnd, UINT message, WPARAM w, LPARAM l,
                                                  UINT_PTR, DWORD_PTR) {
        // Test-only: model a reset that leaves the already-empty EDIT unchanged
        // without delivering EN_CHANGE. The explicit Show fallback must still run.
        if (message == WM_SETTEXT) return TRUE;
        return DefSubclassProc(hwnd, message, w, l);
    }
    static void VerifyInputAfterShow(App& app, LauncherWindow& window) {
        auto& settings = const_cast<Settings&>(app.SettingsData());
        const auto change = [&](UINT message, WPARAM w, LPARAM l = 0) {
            const auto before = window.searchGeneration_;
            SendMessageW(window.edit_, message, w, l);
            assert(window.searchGeneration_ == before + 1);
        };
        window.Hide(); window.Show();
        change(WM_CHAR, L'a', 1); change(WM_CHAR, L'b', 1);
        assert(window.CurrentQuery() == L"ab");
        SendMessageW(window.edit_, EM_SETSEL, 1, 2);
        change(WM_CHAR, L'c', 1);
        assert(window.CurrentQuery() == L"ac");
        SendMessageW(window.edit_, WM_IME_STARTCOMPOSITION, 0, 0);
        assert(window.imeComposing_);
        change(WM_SETTEXT, 0, reinterpret_cast<LPARAM>(L"中文"));
        assert(window.CurrentQuery() == L"中文" && window.imeComposing_);
        SendMessageW(window.edit_, WM_IME_ENDCOMPOSITION, 0, 0);
        assert(!window.imeComposing_);
        change(WM_SETTEXT, 0, reinterpret_cast<LPARAM>(L"中文输入"));

        LauncherResult first, second, dynamic;
        first.id = L"first"; first.title = L"Alpha";
        second.id = L"second"; second.title = L"Beta";
        dynamic.id = L"dynamic"; dynamic.title = L"Gamma";
        dynamic.providerId = "everything.filesystem";
        window.staticResults_ = {first, second}; window.dynamicResults_.clear();
        window.RebuildVisibleResults(false, false, false);
        assert(SendMessageW(window.list_, LB_GETCURSEL, 0, 0) == 0);
        const auto generation = window.searchGeneration_;
        SendMessageW(window.edit_, WM_KEYDOWN, VK_DOWN, 1);
        assert(SendMessageW(window.list_, LB_GETCURSEL, 0, 0) == 1);
        SendMessageW(window.edit_, WM_KEYDOWN, VK_DOWN, 1);
        assert(SendMessageW(window.list_, LB_GETCURSEL, 0, 0) == 0);
        SendMessageW(window.edit_, WM_KEYDOWN, VK_UP, 1);
        assert(SendMessageW(window.list_, LB_GETCURSEL, 0, 0) == 1);
        SendMessageW(window.edit_, WM_KEYDOWN, VK_TAB, 1);
        assert(SendMessageW(window.list_, LB_GETCURSEL, 0, 0) == 0);
        assert(window.searchGeneration_ == generation);

        // Observe the actual single-result execution decision without launching
        // anything: this synthetic result has no executable command index.
        settings.executeSingleResultImmediately = true;
        window.staticResults_ = {first};
        window.immediateExecutionPending_ = true;
        SendMessageW(window.edit_, WM_IME_STARTCOMPOSITION, 0, 0);
        window.RebuildVisibleResults(true, false, false);
        assert(window.immediateExecutionPending_); // composing: no execution
        SendMessageW(window.edit_, WM_IME_ENDCOMPOSITION, 0, 0);
        window.RebuildVisibleResults(true, false, false);
        assert(!window.immediateExecutionPending_); // ExecuteResultSnapshot reached
        window.immediateExecutionPending_ = true;
        window.RebuildVisibleResults(true, false, true);
        assert(window.immediateExecutionPending_); // empty query never auto-executes
        settings.executeSingleResultImmediately = false;
        window.immediateExecutionPending_ = false;

        // Real dynamic query scheduling uses a deliberately absent test endpoint.
        // Inject completion at the public UI entry point; transport is separately
        // covered by the existing IPC/provider tests, with no network dependency.
        EverythingIpcClientOptions options;
        options.everythingWindowClass = L"Asterun.Batch4.AbsentEverythingEndpoint";
        options.discoverNamedInstances = false;
        app.everythingProvider_ = std::make_unique<EverythingProvider>(options);
        settings.providerEnabled["everything.filesystem"] = true;
        assert(app.DynamicSearchEnabled());
        window.Hide(); window.Show();
        assert(!window.dynamicQueryPending_); // empty Show does not start Everything
        change(WM_SETTEXT, 0, reinterpret_cast<LPARAM>(L"batch4dynamic"));
        assert(window.dynamicQueryPending_);
        const auto dynamicGeneration = window.searchGeneration_;
        window.staticResults_ = {first, second}; window.dynamicResults_.clear();
        window.RebuildVisibleResults(false, false, false);
        SendMessageW(window.list_, LB_SETCURSEL, 1, 0);
        const auto selected = window.results_[1].id;
        window.ApplyDynamicResults(dynamicGeneration - 1, {dynamic});
        assert(window.dynamicQueryPending_ && window.dynamicResults_.empty());
        window.ApplyDynamicResults(dynamicGeneration, {dynamic});
        assert(!window.dynamicQueryPending_ && window.results_.size() == 3);
        assert(window.searchGeneration_ == dynamicGeneration); // no synchronous search
        const auto selection = SendMessageW(window.list_, LB_GETCURSEL, 0, 0);
        assert(selection != LB_ERR && window.results_[static_cast<std::size_t>(selection)].id == selected);
        const auto expected = MergeLauncherResultsRanked(window.staticResults_, {&dynamic, 1}, window.maxResults_);
        for (std::size_t i = 0; i < expected.size(); ++i) assert(window.results_[i].id == expected[i].id);
        window.Hide(); window.Show();
        const auto nextGeneration = window.searchGeneration_;
        window.ApplyDynamicResults(dynamicGeneration, {dynamic});
        assert(window.dynamicResults_.empty() && window.searchGeneration_ == nextGeneration);
        settings.providerEnabled["everything.filesystem"] = false;
        app.everythingProvider_.reset(); // join callbacks before App test state expires
    }
    static void VerifyRevealInputPreference(
        App& app,
        HINSTANCE instance) {

        auto& settings =
            const_cast<Settings&>(
                app.SettingsData());
        const bool previous =
            settings
                .defaultEnglishInputOnReveal;

        LauncherWindow window(
            app,
            instance);
        assert(window.Create());

        settings.defaultEnglishInputOnReveal =
            true;
        ResetImeProbe(TRUE);

        window.PrepareInputForReveal(
            false);

        assert(imeContextGets == 1);
        assert(imeOpenStatusReads == 1);
        assert(imeCompositionCancels == 1);
        assert(imeSetOpenRequests == 1);
        assert(imeCurrentOpenStatus == FALSE);
        assert(imeContextReleases == 1);
        assert(window.imeRevealOverrideActive_);
        assert(window.imeRevealOriginalOpen_);

        // Hiding a session restores the IME state Asterun temporarily closed.
        ClearImeProbeCounts();

        window.RestoreInputOverride();

        assert(imeContextGets == 1);
        assert(imeOpenStatusReads == 1);
        assert(imeCompositionCancels == 0);
        assert(imeSetOpenRequests == 1);
        assert(imeCurrentOpenStatus == TRUE);
        assert(imeContextReleases == 1);
        assert(!window.imeRevealOverrideActive_);

        // A user's manual switch back to Chinese wins. Restore observes that
        // the IME is already open and must not write over that state.
        ResetImeProbe(TRUE);
        window.PrepareInputForReveal(
            false);
        assert(imeCurrentOpenStatus == FALSE);
        imeCurrentOpenStatus = TRUE;
        ClearImeProbeCounts();

        window.RestoreInputOverride();

        assert(imeContextGets == 1);
        assert(imeOpenStatusReads == 1);
        assert(imeSetOpenRequests == 0);
        assert(imeCurrentOpenStatus == TRUE);
        assert(imeContextReleases == 1);

        // Re-entering Show in the same visible session must not impose a new
        // override after the user changes input mode.
        ResetImeProbe(TRUE);

        window.PrepareInputForReveal(
            true);

        assert(imeContextGets == 0);
        assert(imeOpenStatusReads == 0);
        assert(imeCompositionCancels == 0);
        assert(imeSetOpenRequests == 0);
        assert(imeContextReleases == 0);

        // If the EDIT already starts in direct input mode, do not manufacture
        // an override session or a later restore.
        ResetImeProbe(FALSE);

        window.PrepareInputForReveal(
            false);

        assert(imeContextGets == 1);
        assert(imeOpenStatusReads == 1);
        assert(imeCompositionCancels == 0);
        assert(imeSetOpenRequests == 0);
        assert(imeContextReleases == 1);
        assert(!window.imeRevealOverrideActive_);

        ClearImeProbeCounts();
        window.RestoreInputOverride();
        assert(imeContextGets == 0);
        assert(imeSetOpenRequests == 0);

        // The opt-out restores historical behavior for every fresh session.
        settings.defaultEnglishInputOnReveal =
            false;
        ResetImeProbe(TRUE);

        window.PrepareInputForReveal(
            false);

        assert(imeContextGets == 0);
        assert(imeOpenStatusReads == 0);
        assert(imeCompositionCancels == 0);
        assert(imeSetOpenRequests == 0);
        assert(imeContextReleases == 0);
        assert(imeCurrentOpenStatus == TRUE);

        settings.defaultEnglishInputOnReveal =
            previous;
        Destroy(window);

        std::cout
            << "Launcher reveal English-input session restore passed"
            << std::endl;
    }

    static void VerifyTopLevelForegroundHandoff(
        App& app,
        HINSTANCE instance) {

        LauncherWindow window(
            app,
            instance);
        assert(window.Create());

        // Background GitHub runners are not guaranteed to grant foreground
        // activation. LauncherWindow::Show() correctly hides again on a
        // resulting WA_INACTIVE, which made this foreground-specific test
        // fail before it reached the handoff under CI. Expose the already
        // created launcher without activation, then opportunistically acquire
        // foreground only when USER32 permits it.
        ShowWindow(
            window.hwnd_,
            SW_SHOWNOACTIVATE);
        assert(window.IsVisible());

        bool foregroundObservable =
            SetForegroundWindow(
                window.hwnd_) != FALSE &&
            GetForegroundWindow() ==
                window.hwnd_;

        SendMessageW(
            window.hwnd_,
            WM_COMMAND,
            MAKEWPARAM(
                LauncherWindow::
                    kMenuSettings,
                0),
            0);

        const HWND settings =
            FindWindowW(
                L"Asterun.Settings",
                nullptr);

        assert(settings);
        assert(IsWindowVisible(settings));
        assert(!window.IsVisible());

        // Interactive Windows CI exposes foreground ownership. Keep the state
        // checks unconditional, and assert the actual handoff whenever USER32
        // allows this test process to observe foreground transitions.
        if (foregroundObservable) {
            assert(
                GetForegroundWindow() ==
                settings);
        }

        SendMessageW(
            settings,
            WM_CLOSE,
            0,
            0);
        assert(
            !IsWindow(
                settings));

        Destroy(window);

        std::cout
            << "Launcher -> Settings foreground handoff passed"
            << std::endl;
    }

    static void MeasureShows(App& app, HINSTANCE instance) {
        auto& settings = const_cast<Settings&>(app.SettingsData());
        for (const auto* provider : {"windows.startmenu", "windows.packaged", "windows.apppaths",
                                     "windows.path", "everything.filesystem"}) {
            settings.providerEnabled[provider] = false;
        }
        app.ReloadCommands();
        assert(app.CanRevealLauncher());
        Command command;
        command.keyword = L"batch4fixture"; command.title = L"Batch4 alpha";
        command.target = LR"(C:\AsterunBatch4Fixture\alpha.exe)";
        std::wstring firstId, secondId;
        assert(app.CreateUserCommand(command, &firstId));
        command.title = L"Batch4 beta"; command.target = LR"(C:\AsterunBatch4Fixture\beta.exe)";
        assert(app.CreateUserCommand(command, &secondId));
        app.usageStore_.Record(secondId);
        for (auto style : {UiStyle::ModernCompact, UiStyle::Classic}) {
            settings.uiStyle = style;
            LauncherWindow window(app, instance); assert(window.Create());
            ShowProbe probe{&window};
            assert(SetWindowSubclass(window.hwnd_, ObserveChange, 1, reinterpret_cast<DWORD_PTR>(&probe)));
            const auto measure = [&](const char* scenario, bool show) {
                probe.changes = 0; probe.editRefreshes = 0;
                const auto before = window.searchGeneration_;
                if (show) window.Show();
                else SetWindowTextW(window.edit_, L"");
                const auto total = window.searchGeneration_ - before;
                // No Hide inside this interval; RefreshResults alone advances generation.
                assert(window.CurrentQuery().empty());
                assert(total == 1 && probe.changes == 1 && probe.editRefreshes == 1);
                if (show) {
                    assert(window.IsVisible());
                    const auto candidates = app.Search(L"", window.maxResults_ * 3);
                    const auto expected = MergeLauncherResultsRanked(candidates, {}, window.maxResults_);
                    assert(!expected.empty() && window.results_.size() == expected.size());
                    for (std::size_t i = 0; i < expected.size(); ++i) {
                        assert(window.results_[i].id == expected[i].id);
                        assert(window.results_[i].score == expected[i].score);
                        assert(window.results_[i].usageScore == expected[i].usageScore);
                    }
                    assert(SendMessageW(window.list_, LB_GETCURSEL, 0, 0) == 0);
                }
                std::cout << "SHOW_MEASURE style=" << (style == UiStyle::Classic ? "Classic" : "Modern")
                    << " case=" << scenario << " EN_CHANGE=" << probe.changes
                    << " edit_refresh=" << probe.editRefreshes << " explicit_refresh="
                    << total - probe.editRefreshes << " total=" << total << std::endl;
            };
            measure("first-empty", true);
            window.Hide(); measure("hide-empty", true);
            SetWindowTextW(window.edit_, L"previous query");
            window.Hide(); measure("hide-nonempty", true);
            measure("set-empty-from-empty", false);
            SetWindowTextW(window.edit_, L"previous query");
            measure("set-empty-from-nonempty", false);
            for (int repeat = 0; repeat < 8; ++repeat) {
                window.Hide(); measure("repeat-empty", true);
                SetWindowTextW(window.edit_, L"previous query");
                SendMessageW(window.list_, LB_SETCURSEL, 1, 0);
                window.Hide(); measure("repeat-nonempty", true);
            }
            window.Hide(); // already-empty EDIT: suppress only this test notification
            assert(SetWindowSubclass(window.edit_, OmitResetNotification, 2, 0));
            probe.changes = 0; probe.editRefreshes = 0;
            const auto beforeFallback = window.searchGeneration_;
            window.Show();
            assert(window.searchGeneration_ == beforeFallback + 1 && probe.changes == 0);
            assert(RemoveWindowSubclass(window.edit_, OmitResetNotification, 2));
            std::cout << "SHOW_FALLBACK total=1 EN_CHANGE=0" << std::endl;
            VerifyInputAfterShow(app, window);
            RemoveWindowSubclass(window.hwnd_, ObserveChange, 1);
            Destroy(window);
            NumericIntentRuntimeFixture::Run(app, instance);
        }
        assert(app.DeleteUserCommand(firstId));
        assert(app.DeleteUserCommand(secondId));
        std::cout << "Show input/IME/selection/single-result/numeric/dynamic/usage regression passed" << std::endl;
    }
    static void Run(HINSTANCE instance) {
        App app(instance);
        auto& settings = const_cast<Settings&>(app.settingsStore_.Data());
        settings.showTrayIcon = false; settings.soundEnabled = false;
        settings.autoCheckUpdates = false;
        const auto resourceCount = static_cast<unsigned>(kClassicShortcutResourceIds.size() + kClassicCloseResourceIds.size() + 1);
        // Warm Win32's first DC/bitmap initialization before process-wide counts.
        {
            LauncherWindow warmup(app, instance);
            assert(warmup.EnsureClassicResources());
            warmup.ReleaseClassicResources();
        }
        GdiFlush();
        assert(ownedBitmaps.empty() && ownedDcs.empty());
        // Every partial load must roll back. Also permit retry after DC/info failure.
        for (unsigned failure = 1; failure <= resourceCount + 2; ++failure) {
            LauncherWindow w(app, instance);
            const auto before = GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
            loads = 0; failLoad = failure <= resourceCount ? failure : 0;
            failInfo = failure == resourceCount + 1; failDc = failure == resourceCount + 2;
            assert(!w.EnsureClassicResources()); Empty(w);
            GdiFlush(); assert(ownedBitmaps.empty() && ownedDcs.empty());
            std::cout << "Failure " << failure << ": before=" << before << ", after failure="
                << GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS) << std::endl;
            assert(GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS) == before);
            failLoad = 0; failInfo = failDc = false;
            assert(w.EnsureClassicResources());
            const auto loaded = loads; assert(w.EnsureClassicResources()); assert(loads == loaded);
            w.ReleaseClassicResources(); Empty(w);
            GdiFlush(); assert(ownedBitmaps.empty() && ownedDcs.empty());
            std::cout << "After retry=" << GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS) << std::endl;
            assert(GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS) == before);
        }
        // Compare lazy Classic pixels against Classic-first initialization at all asset DPIs.
        std::vector<std::vector<unsigned char>> reference;
        settings.uiStyle = UiStyle::Classic;
        {
            LauncherWindow eager(app, instance); assert(eager.Create());
            for (UINT dpi : {96U, 120U, 144U, 192U, 240U}) { Dpi(eager, dpi); reference.push_back(Pixels(eager)); }
            Destroy(eager);
        }
        Resources warm{};
        for (int cycle = 0; cycle < 21; ++cycle) {
            settings.uiStyle = UiStyle::ModernCompact; loads = 0;
            {
                LauncherWindow w(app, instance); assert(w.Create());
                Empty(w); assert(loads == 0);
                settings.uiStyle = UiStyle::Classic; w.ApplyAppearance();
                assert(loads == resourceCount && w.classicBitmapDc_);
                auto dc = w.classicBitmapDc_; auto bitmap = w.classicBackgroundBitmap_;
                settings.uiStyle = UiStyle::ModernCompact; w.ApplyAppearance();
                settings.uiStyle = UiStyle::Classic; w.ApplyAppearance();
                std::size_t i = 0;
                for (UINT dpi : {96U, 120U, 144U, 192U, 240U}) { Dpi(w, dpi); assert(Pixels(w) == reference[i++]); }
                assert(loads == resourceCount && dc == w.classicBitmapDc_ && bitmap == w.classicBackgroundBitmap_);
                Destroy(w);
            }
            assert(ownedBitmaps.empty() && ownedDcs.empty());
            if (cycle == 0) warm = Sample("warm");
            if (cycle == 10 || cycle == 20) {
                auto current = Sample(cycle == 10 ? "cycle10" : "cycle20");
                // Compare repeated-operation counts, not machine-specific memory ceilings.
                assert(current.gdi <= warm.gdi + 2 && current.user <= warm.user + 2);
                assert(current.handles <= warm.handles + 4);
            }
        }
        VerifyTypography(app, instance);
        VerifyRevealInputPreference(
            app,
            instance);
        VerifyTopLevelForegroundHandoff(
            app,
            instance);
        MeasureShows(app, instance);
    }
};
}
int main() {
    const auto com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    altrun::LauncherResourceRuntimeFixture::Run(GetModuleHandleW(nullptr));
    if (SUCCEEDED(com)) CoUninitialize();
    std::cout << "Classic lazy resources: partial failures, reuse, DPI pixels and lifecycle passed\n";
}
