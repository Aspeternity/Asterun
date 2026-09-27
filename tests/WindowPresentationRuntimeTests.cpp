#include "app/App.hpp"
#include "NumericIntentRuntimeFixture.hpp"
#include "platform/InstanceIpc.hpp"
#include "ui/SettingsWindow.hpp"
#include "ui/ShortcutEditorDialog.hpp"
#include "ui/ShortcutPathConverterDialog.hpp"
#include "ui/ShortcutManagerWindow.hpp"
#include "ui/TopLevelWindowPresentation.hpp"

#include <commctrl.h>
#include <objbase.h>
#include <cassert>
#include <iostream>
#include <string_view>

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

HWND modalOwner{};
bool observedEditor{};
bool observedPathConverter{};
bool observedModalDestroy{};
bool ownerEnabledAtModalDestroy{};
bool expectedOwnerEnabledAtModalDestroy{};

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
    const HWND editor = FindWindowW(L"ALTRunNext.ShortcutEditor", nullptr);
    if (!editor) return;
    assert(IsWindowVisible(editor));
    assert(!IsWindowEnabled(modalOwner));
    observedEditor = true;
    ArmModalDestroyProbe(editor);
    KillTimer(nullptr, timer);
    PostMessageW(editor, WM_CLOSE, 0, 0);
}

void CALLBACK CancelPathConverter(HWND, UINT, UINT_PTR timer, DWORD) {
    const HWND converter =
        FindWindowW(L"ALTRunNext.ShortcutPathConverter", nullptr);
    if (!converter) return;
    assert(IsWindowVisible(converter));
    assert(!IsWindowEnabled(modalOwner));
    observedPathConverter = true;
    ArmModalDestroyProbe(converter);
    KillTimer(nullptr, timer);
    PostMessageW(converter, WM_CLOSE, 0, 0);
}

} // namespace

int main() {
    using namespace altrun;
    namespace presentation = window_presentation;
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    const auto com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const HINSTANCE instance = GetModuleHandleW(nullptr);

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
        App app(instance);
        SettingsWindow settings(app, instance);
        for (int attempt = 0; attempt < 6; ++attempt) {
            assert(settings.Create());
            const HWND window = FindWindowW(L"ALTRunNext.Settings", nullptr);
            assert(window && !IsWindowVisible(window));
            int firstShows = 0;
            assert(SetWindowSubclass(window, FirstShowProbe, 1,
                reinterpret_cast<DWORD_PTR>(&firstShows)));
            settings.ShowAbout();
            assert(IsWindowVisible(window));
            assert(HasAboutHeading(window));
            assert(firstShows == 1);
            RemoveWindowSubclass(window, FirstShowProbe, 1);
            // Visible reopen and minimized restore keep the About page.
            settings.ShowAbout();
            ShowWindow(window, SW_MINIMIZE);
            settings.ShowAbout();
            assert(!IsIconic(window) && HasAboutHeading(window));
            // Repeated General -> Sources -> Appearance -> Sources must settle
            // the native frame on the very first visit, including high DPI.
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

        ShowWindow(owner, SW_HIDE);

        ShortcutManagerWindow manager(
            app,
            instance);

        const auto runManagerModalCycle =
            [&]() {
                manager.Show();

                const HWND managerWindow =
                    FindWindowW(
                        L"ALTRunNext.ShortcutManager",
                        nullptr);

                assert(
                    managerWindow &&
                    IsWindowVisible(
                        managerWindow));
                assert(
                    IsWindowEnabled(
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
                        L"ALTRunNext.ShortcutManager",
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
