#pragma once

#include "../core/CommandStore.hpp"
#include "../core/DynamicQueryProvider.hpp"
#include "../core/EverythingQuery.hpp"
#include "../core/LauncherResult.hpp"
#include "../core/Localization.hpp"
#include "../core/SearchEngine.hpp"
#include "../core/Settings.hpp"
#include "../core/UsageStore.hpp"
#include "../platform/EverythingBootstrapper.hpp"
#include "../platform/UpdateManager.hpp"
#include "../platform/WindowsContext.hpp"

#include <windows.h>

#include <atomic>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_set>
#include <vector>

namespace altrun {

class EverythingProvider;
class LauncherWindow;
class SettingsWindow;
class ShortcutManagerWindow;



class App {
public:
    explicit App(
        HINSTANCE instance,
        std::wstring startupHealthEvent = {},
        std::vector<std::wstring>
            startupShortcutPaths = {});
    ~App();

    int Run();
    void ReloadCommands();

    [[nodiscard]] std::vector<LauncherResult>
    Search(
        std::wstring_view query,
        std::size_t limit) const;

    [[nodiscard]] bool
    HasStaticQueryContinuation(
        std::wstring_view query) const noexcept;

    [[nodiscard]] bool
    CanRevealLauncher() const noexcept {
        return commandStore_
            .IndexSearchable();
    }

    void DeferLauncherReveal() noexcept {
        launcherRevealPending_ = true;
    }

    void BeginDynamicSearch(
        std::uint64_t generation,
        std::wstring query,
        std::size_t limit);

    [[nodiscard]] bool
    DynamicSearchEnabled() const;

    [[nodiscard]] const std::vector<Command>&
    UserCommands() const noexcept {
        return commandStore_
            .UserCommands();
    }

    [[nodiscard]] std::vector<ProviderStatus>
    ProviderStatuses() const;

    [[nodiscard]]
    EverythingIpcStatusSnapshot
    EverythingStatus() const;

    [[nodiscard]]
    win::EverythingBootstrapSnapshot
    EverythingBootstrapStatus() const;

    bool StartEverythingBootstrap(
        bool allowDownload);

    [[nodiscard]] std::wstring
    DataCompatibilityWarning() const;

    bool CreateUserCommand(
        Command command,
        std::wstring* createdId = nullptr);
    bool UpdateUserCommand(
        std::wstring_view id,
        Command command);
    bool DeleteUserCommand(
        std::wstring_view id);
    bool MoveUserCommand(
        std::wstring_view id,
        int direction);
    bool ApplyUserCommandPathUpdates(
        const std::vector<UserCommandPathUpdate>& updates);
    bool TestCommand(
        const Command& command,
        std::wstring_view runtimeInput = {});

    bool ImportUserCommands(
        const std::filesystem::path& path,
        std::size_t* imported = nullptr,
        std::size_t* skipped = nullptr);
    bool ExportUserCommands(
        const std::filesystem::path& path) const;
    bool ClearUsageHistory();
    void RebuildProgramIndex();
    bool RestoreDefaultSettings();

    [[nodiscard]] const Settings&
    SettingsData() const noexcept {
        return settingsStore_.Data();
    }

    [[nodiscard]] std::wstring_view
    Text(TextId id) const;

    void SetUiStyle(UiStyle style);
    void SetLanguage(Language language);
    bool SetShowResultIcons(bool enabled);
    bool SetStartWithWindows(bool enabled);
    bool SetStartupBehavior(
        StartupBehavior behavior);
    bool SetShowTrayIcon(bool enabled);
    bool SetAddToSendToMenu(bool enabled);
    bool SetPopupMonitor(
        std::string popupMonitor);
    bool SetHotkeySettings(
        std::vector<std::string> modifiers,
        std::string key);
    bool SetAuxiliaryHotkeySettings(
        bool enabled,
        std::vector<std::string> modifiers,
        std::string key);
    bool SetHotkeyBinding(
        std::string actionId,
        HotkeyBinding binding);
    bool ResetHotkeyBindings();
    [[nodiscard]] bool
    IsHotkeyActionRegistered(
        std::string_view actionId) const;
    [[nodiscard]] DWORD
    HotkeyActionLastError(
        std::string_view actionId) const noexcept;
    bool SetClassicBehavior(
        bool numericQuickLaunch,
        bool executeSingleResultImmediately,
        bool pinyinSearch);
    bool SetProviderEnabled(
        std::string id,
        bool enabled);
    bool SetProviderEnabledBatch(
        const ProviderEnableMap& changes);
    bool SetUpdateSettings(
        bool autoCheck,
        UpdateChannel channel);
    bool SetWindowPlacementSettings(
        std::string launcherPlacement,
        std::string settingsPlacement,
        std::string shortcutManagerPlacement);
    void RememberLauncherPosition(
        int x,
        int y);
    void RememberSettingsPosition(
        int x,
        int y);
    void RememberShortcutManagerPosition(
        int x,
        int y);
    [[nodiscard]] win::UpdateSnapshot
    UpdateStatus() const;
    [[nodiscard]] bool
    UpdateSettingsChangedSinceCheck() const;
    [[nodiscard]] bool
    UpdateWorkerRunning() const noexcept;
    bool StartUpdateCheck(
        bool force);
    bool StartUpdateDownloadAndInstall();
    bool RepairGlobalHotkey(
        bool forceRebind = true);

    [[nodiscard]] bool
    IsGlobalHotkeyRegistered() const noexcept {
        return hotkeyRegistered_;
    }

    [[nodiscard]] DWORD
    GlobalHotkeyLastError() const noexcept {
        return hotkeyLastError_;
    }

    [[nodiscard]] bool
    IsAuxiliaryHotkeyRegistered() const noexcept {
        return auxiliaryHotkeyRegistered_;
    }

    [[nodiscard]] DWORD
    AuxiliaryHotkeyLastError() const noexcept {
        return auxiliaryHotkeyLastError_;
    }

    void ShowSettings();
    void ShowAbout();
    void ShowShortcutManager(
        std::wstring_view preferredId = {});
    void OpenDataFolder();
    void OpenProjectPage();

    bool ExecuteCommand(
        std::size_t index,
        std::wstring_view runtimeInput = {},
        bool forceRunAsAdmin = false,
        std::wstring_view query = {});

    bool ExecuteResult(
        const LauncherResult& result,
        LauncherExecutionIntent intent =
            LauncherExecutionIntent::Default,
        std::wstring_view query = {});

    void ClearActivationContext();

    [[nodiscard]] const win::WindowsContextSnapshot&
    LastActivationContext() const noexcept {
        return lastActivationContext_;
    }

    [[nodiscard]] const std::filesystem::path&
    DataDirectory() const noexcept {
        return dataDirectory_;
    }

    [[nodiscard]] const std::filesystem::path&
    BaseDirectory() const noexcept {
        return baseDirectory_;
    }

private:
    static constexpr int
        kGlobalHotkeyId = 0xA171;

    static constexpr int
        kAuxiliaryHotkeyId = 0xA172;

    static constexpr int
        kShortcutManagerHotkeyId = 0xA173;

    static constexpr UINT
        kProviderRefreshMessage =
            WM_APP + 0x171;

    static constexpr UINT
        kProviderChangedMessage =
            WM_APP + 0x172;

    static constexpr UINT
        kDynamicQueryMessage =
            WM_APP + 0x173;

    static constexpr UINT
        kEverythingBootstrapMessage =
            WM_APP + 0x174;

    static constexpr UINT
        kUpdateStatusMessage =
            WM_APP + 0x175;

    bool LaunchCommand(
        const Command& command,
        bool recordUsage,
        std::wstring_view runtimeInput = {},
        bool forceRunAsAdmin = false,
        std::wstring_view query = {});
    bool ApplyStartupRegistration(
        bool enabled) const;
    bool ApplySendToRegistration(
        bool enabled) const;
    bool ForwardShortcutRequestsToExistingInstance()
        const;
    bool RebindGlobalHotkey(
        const std::vector<std::string>& modifiers,
        std::string_view key);
    bool RebindAuxiliaryHotkey(
        bool enabled,
        const std::vector<std::string>& modifiers,
        std::string_view key);
    bool RebindShortcutManagerHotkey(
        bool enabled,
        const std::vector<std::string>& modifiers,
        std::string_view key);

    void StartProviderRefresh(
        std::vector<std::string>
            selectedIds = {});
    void HandleProviderRefreshCompleted(
        ProviderRefreshOutcome outcome);

    void StartProviderMonitor();
    void HandleProviderChangedSignal();
    void FlushDetectedProviderChanges();
    void HandleDynamicQueryCompleted();
    void HandleEverythingBootstrapCompleted(
        std::uint64_t generation);
    void StopManagedEverythingLifecycle();
    static LRESULT CALLBACK
    UpdateDispatchWindowProc(
        HWND hwnd,
        UINT message,
        WPARAM wParam,
        LPARAM lParam);
    bool CreateUpdateDispatchWindow();
    void DestroyUpdateDispatchWindow();
    void PostUpdateStatusNotification(
        std::uint64_t generation);
    void HandleUpdateStatusMessage(
        std::uint64_t generation);
    void StartUpdateReconcileTimer();
    void StopUpdateReconcileTimer();
    void InvalidateUpdateCheckForChannelChange();
    bool BeginPreparedUpdate();
    void SignalStartupHealthEvent();
    void CaptureActivationContext();

    HINSTANCE instance_{};
    std::filesystem::path
        baseDirectory_;
    std::filesystem::path
        dataDirectory_;
    CommandStore commandStore_;
    UsageStore usageStore_;
    SettingsStore settingsStore_;
    SearchEngine searchEngine_;
    std::unique_ptr<EverythingProvider>
        everythingProvider_;
    std::unique_ptr<LauncherWindow>
        window_;
    std::unique_ptr<SettingsWindow>
        settingsWindow_;
    std::unique_ptr<ShortcutManagerWindow>
        shortcutManagerWindow_;
    win::WindowsContextSnapshot
        activationContext_;
    win::WindowsContextSnapshot
        lastActivationContext_;

    std::jthread
        providerRefreshThread_;
    std::jthread
        providerMonitorThread_;
    std::jthread
        everythingBootstrapThread_;
    std::jthread
        updateThread_;
    std::atomic_bool
        providerRefreshRunning_{false};

    bool providerRefreshFullPending_{false};
    std::unordered_set<std::string>
        providerRefreshIdsPending_;

    std::mutex
        providerMonitorConfigMutex_;
    ProviderEnableMap
        providerMonitorEnabled_;

    std::mutex
        detectedProviderMutex_;
    std::unordered_set<std::string>
        detectedProviderIds_;
    UINT_PTR providerDebounceTimer_{0};

    std::mutex dynamicQueryMutex_;
    std::optional<DynamicQueryResponse>
        dynamicQueryPending_;

    mutable std::mutex
        everythingBootstrapMutex_;
    win::EverythingBootstrapSnapshot
        everythingBootstrapStatus_;
    std::uint64_t
        everythingBootstrapGeneration_{0};

    HWND updateDispatchWindow_{};
    mutable std::mutex
        updateMutex_;
    win::UpdateSnapshot
        updateStatus_;
    std::optional<UpdateManifest>
        updateManifest_;
    std::atomic<std::uint64_t>
        updateGeneration_{0};
    std::atomic_bool
        updateWorkerRunning_{false};
    std::atomic<std::uint64_t>
        updateWorkerStartedTick_{0};
    UINT_PTR updateReconcileTimer_{0};
    bool updateSettingsChangedSinceCheck_{false};
    bool updateInstallWhenReady_{false};
    std::wstring startupHealthEvent_;
    std::vector<std::wstring>
        startupShortcutPaths_;
    bool suppressStartupPresentation_{
        false};
    bool launcherRevealPending_{
        false};

    DWORD uiThreadId_{0};
    HANDLE singleInstanceMutex_{};
    bool dataDirectoryWritable_{true};
    bool hotkeyRegistered_{false};
    UINT currentHotkeyModifiers_{0};
    UINT currentHotkeyVk_{0};
    DWORD hotkeyLastError_{
        ERROR_SUCCESS};

    bool auxiliaryHotkeyRegistered_{false};
    UINT currentAuxiliaryHotkeyModifiers_{0};
    UINT currentAuxiliaryHotkeyVk_{0};
    DWORD auxiliaryHotkeyLastError_{
        ERROR_SUCCESS};

    bool shortcutManagerHotkeyRegistered_{false};
    UINT currentShortcutManagerHotkeyModifiers_{0};
    UINT currentShortcutManagerHotkeyVk_{0};
    DWORD shortcutManagerHotkeyLastError_{
        ERROR_SUCCESS};
};

} // namespace altrun
