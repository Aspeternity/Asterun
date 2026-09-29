#include "app/App.hpp"
#include "ui/Feedback.hpp"
#include "ui/LauncherWindow.hpp"
#include "ui/ShortcutEditorDialog.hpp"
#include "ui/ShortcutPathConverterDialog.hpp"
#include "platform/WinUtil.hpp"
#include "platform/InstanceIpc.hpp"
#include <shlobj.h>
#include <cassert>
#include <fstream>
#include <functional>
#include <iostream>

namespace {
std::filesystem::path fixtureRoot;
std::wstring mutexName;
unsigned stopCalls{};
bool managedRunning{};
bool failMutex{};
unsigned forwarded{};
std::function<void()> modalCheck;
ULONGLONG modalDeadline{};
const wchar_t* modalClass{};
}
namespace altrun::win {
std::filesystem::path TestExecutableDirectory() { return fixtureRoot; }
ManagedEverythingStopResult TestStopManagedEverything(
    const std::filesystem::path&, std::stop_token = {}) {
    ++stopCalls;
    const bool wasRunning = managedRunning;
    managedRunning = false;
    return {wasRunning ? ManagedEverythingStopStatus::Stopped
                       : ManagedEverythingStopStatus::NotRunning, 0};
}
EverythingBootstrapSnapshot TestRunEverythingBootstrap(
    const std::filesystem::path&, bool, const EverythingBootstrapProgress&,
    std::stop_token, bool, bool) {
    return {};
}
}
namespace altrun::ui {
int TestShowMessage(HWND, const wchar_t*, const wchar_t*, UINT) { return IDOK; }
}
namespace {
HANDLE WINAPI TestCreateMutex(LPSECURITY_ATTRIBUTES attributes, BOOL owner, LPCWSTR) {
    if (failMutex) { SetLastError(ERROR_ACCESS_DENIED); return nullptr; }
    return CreateMutexW(attributes, owner, mutexName.c_str());
}
// App::Run is exercised without modifying a developer's shell registration.
LSTATUS WINAPI TestRegOpenKey(HKEY, LPCWSTR, DWORD, REGSAM, PHKEY key) {
    *key = nullptr;
    return ERROR_FILE_NOT_FOUND;
}
HRESULT WINAPI TestKnownFolder(REFKNOWNFOLDERID, DWORD, HANDLE, PWSTR* path) {
    *path = nullptr;
    return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
}
}
#define ExecutableDirectory TestExecutableDirectory
#define StopManagedEverything TestStopManagedEverything
#define RunEverythingBootstrap TestRunEverythingBootstrap
#define ShowMessage TestShowMessage
#define CreateMutexW TestCreateMutex
#define RegOpenKeyExW TestRegOpenKey
#define SHGetKnownFolderPath TestKnownFolder
#include "../src/app/App.cpp"
#undef SHGetKnownFolderPath
#undef RegOpenKeyExW
#undef CreateMutexW
#undef ShowMessage
#undef RunEverythingBootstrap
#undef StopManagedEverything
#undef ExecutableDirectory

namespace {
void WriteSettings(bool everything = false) {
    std::filesystem::create_directories(fixtureRoot / "data");
    std::ofstream file(fixtureRoot / "data/settings.json");
    file << R"({"schemaVersion":11,"general":{"startWithWindows":false,
        "showTrayIcon":false,"startupBehavior":"silent","soundEnabled":false,
        "addToSendToMenu":false},"update":{"autoCheck":false},"providers":{
        "windows.startmenu":false,"windows.packaged":false,"windows.apppaths":false,
        "windows.path":false,"everything.filesystem":)" << (everything ? "true" : "false") << "}}";
}
void DrainQuit() {
    MSG message{};
    while (PeekMessageW(&message, nullptr, WM_QUIT, WM_QUIT, PM_REMOVE)) {}
}
LRESULT CALLBACK ForwardProc(HWND hwnd, UINT msg, WPARAM w, LPARAM l) {
    if (msg == WM_COPYDATA) { ++forwarded; return TRUE; }
    return DefWindowProcW(hwnd, msg, w, l);
}
LRESULT CALLBACK RejectCreateProc(HWND hwnd, UINT msg, WPARAM w, LPARAM l) {
    if (msg == WM_NCCREATE) return FALSE;
    return DefWindowProcW(hwnd, msg, w, l);
}
void CALLBACK CheckModal(HWND, UINT, UINT_PTR, DWORD) {
    const HWND modal = FindWindowW(modalClass, nullptr);
    if (!modal) return;
    assert(GetTickCount64() < modalDeadline);
    modalCheck();
}
}

namespace altrun {
struct AppLifecycleRuntimeFixture {
    static void CloseLauncher(App& app) {
        if (app.window_ && app.window_->hwnd_) DestroyWindow(app.window_->hwnd_);
        app.window_.reset();
        DrainQuit();
    }
    static void Ownership(HINSTANCE instance) {
        // The real Run() paths use an isolated named mutex and isolated files.
        for (bool running : {false, true}) {
            WriteSettings(true);
            managedRunning = running;
            stopCalls = 0;
            HANDLE primary = CreateMutexW(nullptr, FALSE, mutexName.c_str());
            assert(primary);
            {
                App secondary(instance);
                assert(secondary.Run() == 0);
                assert(!secondary.ownsPrimaryInstance_ && !secondary.everythingProvider_);
            }
            assert(stopCalls == 0 && managedRunning == running);

            WNDCLASSEXW wc{sizeof(wc)};
            wc.hInstance = instance;
            wc.lpfnWndProc = ForwardProc;
            wc.lpszClassName = instance_ipc::kLauncherWindowClass;
            assert(RegisterClassExW(&wc));
            HWND destination = CreateWindowExW(0, wc.lpszClassName, L"", WS_POPUP,
                0, 0, 1, 1, nullptr, nullptr, instance, nullptr);
            assert(destination);
            const auto before = forwarded;
            {
                App sender(instance, {}, {L"C:\\Fixture\\工具.exe"});
                assert(sender.Run() == 0);
                assert(!sender.ownsPrimaryInstance_ && !sender.everythingProvider_);
            }
            assert(forwarded == before + 1);
            assert(stopCalls == 0 && managedRunning == running);
            DestroyWindow(destination);
            assert(UnregisterClassW(wc.lpszClassName, instance));
            CloseHandle(primary);

            failMutex = true;
            {
                App failed(instance);
                assert(failed.Run() == 1 && !failed.ownsPrimaryInstance_);
            }
            failMutex = false;
            assert(stopCalls == 0 && managedRunning == running);

            wc.lpfnWndProc = RejectCreateProc;
            assert(RegisterClassExW(&wc));
            {
                App failed(instance);
                assert(failed.Run() == 1);
                assert(failed.singleInstanceMutex_ && !failed.ownsPrimaryInstance_);
                assert(!failed.everythingProvider_);
            }
            assert(UnregisterClassW(wc.lpszClassName, instance));
            assert(stopCalls == 0 && managedRunning == running);
        }
        // Successful Run and normal destruction still perform primary cleanup,
        // regardless of whether the managed process currently exists.
        for (bool running : {false, true}) {
            WriteSettings();
            managedRunning = running;
            stopCalls = 0;
            {
                App primary(instance);
                PostQuitMessage(0);
                assert(primary.Run() == 0);
                assert(primary.ownsPrimaryInstance_);
                CloseLauncher(primary);
            }
            assert(stopCalls == 1 && !managedRunning);
            DrainQuit();
        }
    }

    static void ModalDelivery(HINSTANCE instance) {
        WriteSettings();
        App app(instance);
        app.settingsStore_.Load();
        app.uiThreadId_ = GetCurrentThreadId();
        assert(app.CreateUpdateDispatchWindow());
        app.window_ = std::make_unique<LauncherWindow>(app, instance);
        assert(app.window_->Create());
        auto& window = *app.window_;
        const HWND owner = CreateWindowExW(0, L"STATIC", L"owner", WS_POPUP,
            0, 0, 200, 100, nullptr, nullptr, instance, nullptr);
        assert(owner);

        for (bool converter : {false, true}) {
            // Each notification is posted by a worker while the actual editor
            // or path converter's nested GetMessage loop is running.
            for (int kind = 0; kind < 8; ++kind) {
                std::cout << (converter ? "Path converter" : "Shortcut editor")
                          << " notification case " << kind << std::endl;
                bool posted = false;
                std::function<void()> verify;
                modalClass = converter ? L"Asterun.ShortcutPathConverter" : L"Asterun.ShortcutEditor";
                modalDeadline = GetTickCount64() + 5000;
                modalCheck = [&] {
                    const HWND modal = FindWindowW(modalClass, nullptr);
                    if (!posted) {
                        posted = true;
                        UINT message{};
                        WPARAM parameter{};
                        LPARAM payload{};
                        switch (kind) {
                        case 0: // matching continuation is consumed even after focus moved
                        case 1: // stale token must not consume a newer intent
                            window.pendingNumericIntent_.active = true;
                            window.pendingNumericIntent_.token = 42;
                            message = App::kNumericProbeMessage;
                            parameter = kind == 0 ? 42 : 41;
                            payload = static_cast<LPARAM>(classic_behavior::ContinuationEvidence::Present);
                            verify = [&] {
                                assert(window.pendingNumericIntent_.active == (kind == 1));
                                assert(app.detectedProviderIds_.empty()); // no AppsFolder ID collision
                            };
                            break;
                        case 2:
                        case 3: {
                            window.searchGeneration_ = 200;
                            window.dynamicResults_.clear();
                            DynamicQueryResponse response;
                            response.generation = kind == 2 ? 200 : 199;
                            LauncherResult first, second;
                            first.id = L"first"; second.id = L"second";
                            response.results = {first, second};
                            app.dynamicQueryPending_ = std::move(response);
                            message = App::kDynamicQueryMessage;
                            verify = [&] {
                                assert(!app.dynamicQueryPending_);
                                assert(window.dynamicResults_.size() == (kind == 2 ? 2U : 0U));
                                if (kind == 2) {
                                    assert(window.dynamicResults_[0].id == L"first");
                                    assert(window.dynamicResults_[1].id == L"second");
                                }
                            };
                            break;
                        }
                        case 4:
                            app.everythingBootstrapGeneration_ = 300;
                            app.everythingBootstrapThread_ = std::jthread([] {});
                            message = App::kEverythingBootstrapMessage;
                            parameter = 300;
                            verify = [&] { assert(!app.everythingBootstrapThread_.joinable()); };
                            break;
                        case 5:
                            app.providerRefreshRunning_ = true;
                            app.providerRefreshThread_ = std::jthread([] {});
                            message = App::kProviderRefreshMessage;
                            parameter = static_cast<WPARAM>(ProviderRefreshOutcome::Failed);
                            verify = [&] {
                                assert(!app.providerRefreshRunning_);
                                assert(!app.providerRefreshThread_.joinable());
                            };
                            break;
                        case 6:
                            app.detectedProviderIds_.insert(std::string(providers::kPath));
                            message = App::kProviderChangedMessage;
                            verify = [&] {
                                // Keep the modal open through the real debounce timer.
                                if (app.providerDebounceTimer_ != 0) return;
                                assert(app.detectedProviderIds_.empty());
                            };
                            break;
                        case 7:
                            app.updateGeneration_ = 400;
                            app.updateThread_ = std::jthread([] {});
                            message = App::kUpdateStatusMessage;
                            parameter = 400;
                            verify = [&] { assert(!app.updateThread_.joinable()); };
                            break;
                        }
                        std::jthread producer([&] { app.PostUiNotification(message, parameter, payload); });
                        producer.join();
                        return;
                    }
                    verify();
                    if (kind == 6 && app.providerDebounceTimer_ != 0) return;
                    PostMessageW(modal, WM_CLOSE, 0, 0);
                };
                const auto timer = SetTimer(nullptr, 0, 30, CheckModal);
                assert(timer);
                if (converter) {
                    assert(!ShortcutPathConverterDialog::Show(app, instance, owner));
                } else {
                    assert(!ShortcutEditorDialog::Show(app, instance, owner));
                }
                KillTimer(nullptr, timer);
                assert(posted);
                verify();
                window.CancelPendingNumericIntent();
            }
        }
        // Shutdown must join late producers while their target is still alive.
        const HWND dispatcher = app.updateDispatchWindow_;
        app.providerRefreshThread_ = std::jthread([&app, dispatcher](std::stop_token stop) {
            while (!stop.stop_requested()) std::this_thread::yield();
            assert(IsWindow(dispatcher));
            app.PostUiNotification(App::kProviderRefreshMessage);
        });
        CloseLauncher(app);
        DestroyWindow(owner);
    }
};
}
int main() {
    fixtureRoot = std::filesystem::temp_directory_path() /
        (L"Asterun-Batch1-" + std::to_wstring(GetCurrentProcessId()));
    mutexName = L"Local\\Asterun.Batch1.Test." + std::to_wstring(GetCurrentProcessId());
    const auto com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const auto instance = GetModuleHandleW(nullptr);
    altrun::AppLifecycleRuntimeFixture::Ownership(instance);
    altrun::AppLifecycleRuntimeFixture::ModalDelivery(instance);
    if (SUCCEEDED(com)) CoUninitialize();
    std::filesystem::remove_all(fixtureRoot);
    std::cout << "Primary ownership and modal dispatcher regressions passed\n";
}
