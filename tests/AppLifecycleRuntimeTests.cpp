#include "app/App.hpp"
#include "ui/Feedback.hpp"
#include "ui/LauncherWindow.hpp"
#include "ui/ShortcutEditorDialog.hpp"
#include "ui/ShortcutPathConverterDialog.hpp"
#include "platform/WinUtil.hpp"
#include "platform/InstanceIpc.hpp"
#include "platform/NotificationIdentity.hpp"
#include "IsolatedDataDirectory.hpp"
#include <shlobj.h>
#include <cassert>
#include <fstream>
#include <functional>
#include <future>
#include <iostream>
#include <iterator>
#include <string_view>
#include <unordered_map>

namespace {
std::filesystem::path fixtureRoot;
std::wstring mutexName;
unsigned stopCalls{};
bool managedRunning{};
bool failMutex{};
unsigned forwarded{};
unsigned registeredNotifications{};
std::wstring lastMessage;
std::function<void()> modalCheck;
ULONGLONG modalDeadline{};
const wchar_t* modalClass{};
bool mockHotkeys{};
int failNextHotkeyId{};
std::unordered_map<int, HWND> mockHotkeyOwners;
BOOL WINAPI ProbeRegisterHotKey(HWND target, int id, UINT mods, UINT key) {
    if (!mockHotkeys) return ::RegisterHotKey(target, id, mods, key);
    if (failNextHotkeyId == id) {
        failNextHotkeyId = 0;
        SetLastError(ERROR_HOTKEY_ALREADY_REGISTERED);
        return FALSE;
    }
    if (mockHotkeyOwners.contains(id)) {
        SetLastError(ERROR_HOTKEY_ALREADY_REGISTERED);
        return FALSE;
    }
    mockHotkeyOwners[id] = target;
    return TRUE;
}
BOOL WINAPI ProbeUnregisterHotKey(HWND target, int id) {
    if (!mockHotkeys) return ::UnregisterHotKey(target, id);
    const auto entry = mockHotkeyOwners.find(id);
    if (entry == mockHotkeyOwners.end() || entry->second != target) {
        SetLastError(ERROR_HOTKEY_NOT_REGISTERED);
        return FALSE;
    }
    mockHotkeyOwners.erase(entry);
    return TRUE;
}
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
namespace altrun::notification_identity {
void Register(HINSTANCE) noexcept { ++registeredNotifications; }
}
namespace altrun::ui {
int TestShowMessage(HWND, const wchar_t* message, const wchar_t*, UINT) {
    lastMessage = message ? message : L"";
    return IDOK;
}
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
#define RegisterHotKey ProbeRegisterHotKey
#define UnregisterHotKey ProbeUnregisterHotKey
#define SHGetKnownFolderPath TestKnownFolder
#include "../src/app/App.cpp"
#undef SHGetKnownFolderPath
#undef RegOpenKeyExW
#undef RegisterHotKey
#undef UnregisterHotKey
#undef CreateMutexW
#undef ShowMessage
#undef RunEverythingBootstrap
#undef StopManagedEverything
#undef ExecutableDirectory

namespace {
void WriteSettings(bool everything = false) {
    std::filesystem::create_directories(fixtureRoot / "data");
    std::ofstream file(fixtureRoot / "data/settings.json");
    file << R"({"schemaVersion":12,"general":{"startWithWindows":false,
        "showTrayIcon":false,"startupBehavior":"silent","soundEnabled":false,
        "addToSendToMenu":false},"update":{"autoCheck":false},"providers":{
        "windows.startmenu":false,"windows.packaged":false,"windows.apppaths":false,
        "windows.path":false,"everything.filesystem":)" << (everything ? "true" : "false") << "}}";
}
std::string ReadFixtureBytes(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
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
    static void NoWritesBeforeOwnership(HINSTANCE instance) {
        const auto previousFixtureRoot = fixtureRoot;
        const auto isolated = fixtureRoot / "f04-early-ownership";
        std::filesystem::remove_all(isolated);
        std::filesystem::create_directories(isolated);
        fixtureRoot = isolated;
        const auto data = fixtureRoot / "data";
        registeredNotifications = 0;

        HANDLE existing = CreateMutexW(nullptr, FALSE, mutexName.c_str());
        assert(existing);
        {
            App secondary(instance);
            assert(secondary.Run() == 0 && !secondary.ownsPrimaryInstance_);
        }
        // A rejected second instance must not create data/settings.json.
        assert(!std::filesystem::exists(data));
        assert(registeredNotifications == 0);
        CloseHandle(existing);

        failMutex = true;
        {
            App denied(instance);
            assert(denied.Run() == 1 && !denied.ownsPrimaryInstance_);
        }
        failMutex = false;
        assert(!std::filesystem::exists(data));
        assert(registeredNotifications == 0);

        {
            // While Update.exe holds the guard and its marker, a second
            // instance leaves silently, also when an elevated updater's guard
            // cannot be opened.
            const HANDLE marker = CreateMutexW(
                nullptr, FALSE, app_identity::kUpdateInProgressMutexName);
            assert(marker);
            existing = CreateMutexW(nullptr, FALSE, mutexName.c_str());
            assert(existing);
            for (const bool denied : {false, true}) {
                failMutex = denied;
                lastMessage.clear();
                App updating(instance);
                assert(updating.Run() == 0 && !updating.ownsPrimaryInstance_);
                assert(lastMessage.empty());
            }
            failMutex = false;
            CloseHandle(existing);
            CloseHandle(marker);
            assert(!std::filesystem::exists(data));
            assert(registeredNotifications == 0);
        }

        std::filesystem::create_directories(data);
        const auto primaryPath = data / "settings.json";
        const auto backupPath = data / "settings.json.bak";
        const std::string invalid = R"({"schemaVersion":12,"general":{"soundEnabled":"invalid"}})";
        const std::string goodBackup = R"({"schemaVersion":12,"appearance":{"language":"en-US"},"general":{"soundEnabled":false}})";
        { std::ofstream file(primaryPath, std::ios::binary); file << invalid; }
        { std::ofstream file(backupPath, std::ios::binary); file << goodBackup; }
        existing = CreateMutexW(nullptr, FALSE, mutexName.c_str());
        assert(existing);
        lastMessage.clear();
        {
            App secondary(instance);
            assert(secondary.Run() == 0 && !secondary.ownsPrimaryInstance_);
        }
        // Read the effective language from the good backup without repairing it.
        assert(lastMessage.find(L"Asterun is already running") != std::wstring::npos);
        assert(ReadFixtureBytes(primaryPath) == invalid);
        assert(ReadFixtureBytes(backupPath) == goodBackup);
        assert(registeredNotifications == 0);
        CloseHandle(existing);

        const std::string oldSchema = R"({"schemaVersion":11,"appearance":{"language":"en-US"},"general":{"soundEnabled":false}})";
        { std::ofstream file(primaryPath, std::ios::binary); file << oldSchema; }
        std::filesystem::remove(backupPath);
        existing = CreateMutexW(nullptr, FALSE, mutexName.c_str());
        assert(existing);
        lastMessage.clear();
        {
            App secondary(instance);
            assert(secondary.Run() == 0 && !secondary.ownsPrimaryInstance_);
        }
        // Even a valid old schema must not be migrated by a duplicate process.
        assert(lastMessage.find(L"Asterun is already running") != std::wstring::npos);
        assert(ReadFixtureBytes(primaryPath) == oldSchema);
        assert(!std::filesystem::exists(backupPath));
        assert(registeredNotifications == 0);
        CloseHandle(existing);

        fixtureRoot = previousFixtureRoot;
        std::filesystem::remove_all(isolated);
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
                App sender(instance, {}, {L"C:\\Fixture\\工具.exe", L"C:\\Fixture\\second.txt"});
                assert(sender.Run() == 0);
                assert(!sender.ownsPrimaryInstance_ && !sender.everythingProvider_);
            }
            assert(forwarded == before + 2);
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
            const auto registeredBefore = registeredNotifications;
            {
                App primary(instance);
                PostQuitMessage(0);
                assert(primary.Run() == 0);
                assert(primary.ownsPrimaryInstance_);
                CloseLauncher(primary);
            }
            assert(registeredNotifications == registeredBefore + 1);
            assert(stopCalls == 1 && !managedRunning);
            DrainQuit();
        }
    }

    static void SearchCacheLifetime(HINSTANCE instance) {
        WriteSettings();
        App app(instance);
        app.settingsStore_.Load();
        app.ReloadCommands();
        Command command;
        command.keyword = L"cachefixture";
        command.title = L"Cache fixture";
        command.target = L"C:\\Windows\\notepad.exe";
        std::wstring id;
        assert(app.CreateUserCommand(command, &id));
        assert(!app.Search(L"cachefixture", 10).empty());
        assert(!app.baseSearchIndex_.empty());
        app.activationContext_.kind = win::WindowsContextKind::Explorer;
        app.activationContext_.explorerBrowserWindow = reinterpret_cast<HWND>(1);
        app.activationContext_.explorerViewWindow = reinterpret_cast<HWND>(2);
        app.activationContext_.explorerFolder = L"C:\\Context";
        auto emptyContext = [&] {
            const auto& cache = app.contextSearchCache_;
            assert(cache.generation == 0 && cache.folder.empty());
            assert(cache.commands.capacity() == 0 && cache.indices.capacity() == 0);
            assert(cache.prepared.capacity() == 0);
        };
        for (int cycle = 0; cycle < 24; ++cycle) {
            command.arguments = L"{folder}";
            assert(app.UpdateUserCommand(id, command));
            // Invalidation occurs immediately, even with no Launcher/search.
            assert(app.baseSearchIndex_.capacity() == 0 && app.baseSearchGeneration_ == 0);
            auto first = app.Search(L"cachefixture", 10);
            assert(!first.empty());
            auto* commands = app.contextSearchCache_.commands.data();
            auto* prepared = app.contextSearchCache_.prepared.data();
            const auto generation = app.contextSearchCache_.generation;
            auto again = app.Search(L"cachefixture", 10);
            assert(again.size() == first.size() && again[0].id == first[0].id && again[0].score == first[0].score);
            assert(app.contextSearchCache_.commands.data() == commands);
            assert(app.contextSearchCache_.prepared.data() == prepared);
            assert(app.contextSearchCache_.generation == generation);
            {
                const auto context = app.activationContext_;
                LauncherWindow hidden(app, instance);
                hidden.Hide();
                assert(app.contextSearchCache_.commands.data() == commands);
                assert(app.contextSearchCache_.prepared.data() == prepared);
                app.activationContext_ = context;
            }
            command.arguments.clear();
            assert(app.UpdateUserCommand(id, command)); // remove last folder template
            emptyContext();
            assert(!app.Search(L"cachefixture", 10).empty());
            assert(!app.baseSearchIndex_.empty());
        }
        command.arguments = L"{folder}";
        assert(app.UpdateUserCommand(id, command));
        assert(!app.Search(L"cachefixture", 10).empty());
        // Publication makes both old generations unusable immediately.
        app.ReloadCommands(); emptyContext();
        assert(!app.Search(L"cachefixture", 10).empty());
        assert(app.DeleteUserCommand(id)); emptyContext();
    }

    static void UpdateWatchdog(HINSTANCE instance) {
        // Exercise the unchanged App generation arbitration as completion races
        // the absolute watchdog. Transport-level handle races are tested separately.
        for (int cycle = 0; cycle < 32; ++cycle) {
            App app(instance);
            app.updateGeneration_ = 700;
            app.updateStatus_.stage = win::UpdateStage::Checking;
            app.updateStatus_.running = true;
            app.updateWorkerRunning_ = true;
            const auto now = GetTickCount64();
            assert(now > kUpdateCheckWatchdogMs);
            app.updateWorkerStartedTick_ = now - kUpdateCheckWatchdogMs;
            std::promise<void> go, done;
            auto ready = go.get_future(); auto finished = done.get_future();
            app.updateThread_ = std::jthread([&](std::stop_token) {
                ready.wait();
                {
                    std::scoped_lock lock(app.updateMutex_);
                    if (app.updateGeneration_ == 700) {
                        app.updateStatus_.stage = win::UpdateStage::Available;
                        app.updateStatus_.running = false;
                    }
                }
                app.updateWorkerRunning_ = false;
                done.set_value();
            });
            if (cycle == 0) { // watchdog wins before completion can publish
                app.HandleUpdateStatusMessage(700);
                assert(app.updateThread_.get_stop_token().stop_requested());
                assert(app.updateGeneration_ == 701);
                go.set_value();
            } else {
                go.set_value();
                if (cycle == 1) finished.wait(); // completion wins before watchdog
                app.HandleUpdateStatusMessage(700);
            }
            assert(finished.wait_for(std::chrono::seconds(2)) == std::future_status::ready);
            app.HandleUpdateStatusMessage(700);
            assert(!app.updateThread_.joinable());
            const auto status = app.UpdateStatus();
            assert(!status.running);
            if (app.updateGeneration_ == 701) {
                assert(status.failure == win::UpdateFailure::CheckTimedOut);
                assert(status.nativeError == ERROR_TIMEOUT);
            } else assert(status.stage == win::UpdateStage::Available);
        }
    }

    static void UpdateUserActions(HINSTANCE instance) {
        App app(instance);
        // A running check is not a package download and cannot be cancelled
        // from the action button; nothing but Failed can be retried.
        app.updateStatus_.stage = win::UpdateStage::Checking;
        app.updateStatus_.running = true;
        assert(!app.CancelUpdateDownload());
        assert(!app.RetryUpdate());

        for (const auto stage : {win::UpdateStage::Downloading,
                 win::UpdateStage::Verifying, win::UpdateStage::Extracting}) {
            app.updateStatus_.stage = stage;
            app.updateStatus_.running = true;
            app.updateInstallWhenReady_ = true;
            app.updateThread_ = std::jthread([](std::stop_token token) {
                while (!token.stop_requested())
                    std::this_thread::sleep_for(std::chrono::milliseconds(5));
            });
            assert(app.CancelUpdateDownload());
            // The install handoff is withdrawn before the worker is stopped.
            assert(!app.updateInstallWhenReady_);
            assert(app.updateThread_.get_stop_token().stop_requested());
            app.updateThread_.join();
        }

        app.updateStatus_.stage = win::UpdateStage::ReadyToInstall;
        app.updateStatus_.running = false;
        assert(!app.CancelUpdateDownload());
    }

    static void UpdateOfferRefreshWatchdog(HINSTANCE instance) {
        // A stalled quiet refresh is abandoned without hiding the offer.
        App app(instance);
        app.updateGeneration_ = 900;
        app.updateStatus_.stage = win::UpdateStage::Available;
        app.updateStatus_.availableVersion = "9.0.0";
        app.updateManifest_ = UpdateManifest{};
        app.updateCheckAutomatic_ = true;
        app.updateRefreshingOffer_ = true;
        app.updateWorkerRunning_ = true;
        const auto now = GetTickCount64();
        assert(now > kUpdateCheckWatchdogMs);
        app.updateWorkerStartedTick_ = now - kUpdateCheckWatchdogMs;
        app.updateThread_ = std::jthread([&app](std::stop_token token) {
            while (!token.stop_requested())
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            app.updateWorkerRunning_ = false;
        });
        app.HandleUpdateStatusMessage(900);
        assert(app.updateGeneration_ == 901);
        assert(app.updateThread_.get_stop_token().stop_requested());
        assert(!app.updateRefreshingOffer_ && !app.updateCheckAutomatic_);
        app.updateThread_.join();
        const auto status = app.UpdateStatus();
        assert(status.stage == win::UpdateStage::Available);
        assert(status.availableVersion == "9.0.0" && app.updateManifest_);
    }

    static void ModalHotkeyRouting(HINSTANCE instance) {
        WriteSettings();
        assert(mockHotkeyOwners.empty());
        {
            App app(instance);
            app.settingsStore_.Load();
            app.ReloadCommands();
            assert(app.CanRevealLauncher());
            app.uiThreadId_ = GetCurrentThreadId();
            assert(app.CreateUpdateDispatchWindow());
            const HWND dispatcher = app.updateDispatchWindow_;
            assert(dispatcher);
            mockHotkeys = true;
            assert(app.RebindGlobalHotkey({"alt"}, "f24"));
            assert(app.RebindAuxiliaryHotkey(true, {"ctrl"}, "f23"));
            assert(app.RebindShortcutManagerHotkey(true, {"shift"}, "f22"));
            for (int id : {App::kGlobalHotkeyId, App::kAuxiliaryHotkeyId,
                           App::kShortcutManagerHotkeyId}) {
                assert(mockHotkeyOwners.at(id) == dispatcher);
            }

            // A failed rebind must restore the same registration window.
            failNextHotkeyId = App::kGlobalHotkeyId;
            assert(!app.RebindGlobalHotkey({"ctrl"}, "f21"));
            assert(app.hotkeyRegistered_);
            assert(app.hotkeyLastError_ == ERROR_HOTKEY_ALREADY_REGISTERED);
            assert(mockHotkeyOwners.at(App::kGlobalHotkeyId) == dispatcher);

            app.window_ = std::make_unique<LauncherWindow>(app, instance);
            assert(app.window_->Create());
            assert(!app.window_->IsVisible());
            HWND owner = CreateWindowExW(0, L"STATIC", L"modal-hotkey-owner",
                WS_POPUP | WS_VISIBLE, 0, 0, 200, 100,
                nullptr, nullptr, instance, nullptr);
            assert(owner);
            for (bool converter : {true, false}) {
                int phase = 0;
                modalClass = converter ? L"Asterun.ShortcutPathConverter" :
                    L"Asterun.ShortcutEditor";
                modalDeadline = GetTickCount64() + 6000;
                modalCheck = [&] {
                    const HWND modal = FindWindowW(modalClass, nullptr);
                    assert(modal && !IsWindowEnabled(owner));
                    if (phase == 0) {
                        assert(PostMessageW(dispatcher, WM_HOTKEY,
                            App::kGlobalHotkeyId, MAKELPARAM(MOD_ALT, VK_F24)));
                        ++phase;
                    } else if (phase == 1) {
                        assert(app.window_->IsVisible());
                        assert(PostMessageW(dispatcher, WM_HOTKEY,
                            App::kAuxiliaryHotkeyId, MAKELPARAM(MOD_CONTROL, VK_F23)));
                        ++phase;
                    } else if (phase == 2) {
                        assert(!app.window_->IsVisible());
                        assert(PostMessageW(modal, WM_CLOSE, 0, 0));
                        ++phase;
                    }
                };
                const auto timer = SetTimer(nullptr, 0, 30, CheckModal);
                assert(timer);
                if (converter) assert(!ShortcutPathConverterDialog::Show(app, instance, owner));
                else assert(!ShortcutEditorDialog::Show(app, instance, owner));
                KillTimer(nullptr, timer);
                assert(phase == 3 && IsWindowEnabled(owner));
            }
            DestroyWindow(owner);
            CloseLauncher(app);
            // App destructor must unregister all three HWND-owned hotkeys
            // before destroying the shared dispatch window.
        }
        assert(mockHotkeyOwners.empty());
        mockHotkeys = false;
        failNextHotkeyId = 0;
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
                        case 0: // matching file rows resolve a waiting numbered launch
                        case 1: { // stale rows must not resolve a newer intent
                            window.searchGeneration_ = 210;
                            window.pendingNumericIntent_.active = true;
                            window.pendingNumericIntent_.probeRequired = true;
                            DynamicQueryResponse response;
                            response.generation = kind == 0 ? 210 : 209;
                            app.dynamicQueryPending_ = std::move(response);
                            message = App::kDynamicQueryMessage;
                            verify = [&] {
                                assert(window.pendingNumericIntent_.active == (kind == 1));
                                assert(app.detectedProviderIds_.empty()); // no AppsFolder ID collision
                            };
                            break;
                        }
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
int main(int argc, char* argv[]) {
    altrun::test::RemoveAbandonedTestDirectories(L"Asterun-Batch1");
    fixtureRoot = std::filesystem::temp_directory_path() /
        (L"Asterun-Batch1-" + std::to_wstring(GetCurrentProcessId()));
    mutexName = L"Local\\Asterun.Batch1.Test." + std::to_wstring(GetCurrentProcessId());
    const auto com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const auto instance = GetModuleHandleW(nullptr);
    const bool modalHotkeysOnly = argc == 2 &&
        std::string_view(argv[1]) == "--modal-hotkeys";
    assert(argc == 1 || modalHotkeysOnly);
    if (modalHotkeysOnly) {
        // Win32 modal pumps and WM_QUIT are thread-scoped. Run this strict
        // hotkey regression in its own process, not after lifecycle shutdown
        // tests that intentionally destroy the launcher's main window.
        altrun::AppLifecycleRuntimeFixture::ModalHotkeyRouting(instance);
    } else {
        altrun::AppLifecycleRuntimeFixture::NoWritesBeforeOwnership(instance);
        altrun::AppLifecycleRuntimeFixture::Ownership(instance);
        altrun::AppLifecycleRuntimeFixture::ModalDelivery(instance);
        altrun::AppLifecycleRuntimeFixture::UpdateWatchdog(instance);
        altrun::AppLifecycleRuntimeFixture::UpdateUserActions(instance);
        altrun::AppLifecycleRuntimeFixture::UpdateOfferRefreshWatchdog(instance);
        altrun::AppLifecycleRuntimeFixture::SearchCacheLifetime(instance);
    }
    if (SUCCEEDED(com)) CoUninitialize();
    std::filesystem::remove_all(fixtureRoot);
    std::cout << (modalHotkeysOnly
        ? "Modal hotkey dispatcher regressions passed\n"
        : "Primary ownership and modal dispatcher regressions passed\n");
}
