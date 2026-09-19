#pragma once

#include "../core/CommandStore.hpp"
#include "../core/Localization.hpp"
#include "../core/SearchEngine.hpp"
#include "../core/Settings.hpp"
#include "../core/UsageStore.hpp"

#include <windows.h>

#include <atomic>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_set>
#include <vector>

namespace altrun {

class LauncherWindow;
class SettingsWindow;

class App {
public:
    explicit App(HINSTANCE instance);
    ~App();

    int Run();
    void ReloadCommands();

    [[nodiscard]] std::vector<SearchResult>
    Search(
        std::wstring_view query,
        std::size_t limit) const;

    [[nodiscard]] const Command&
    GetCommand(
        std::size_t index) const;

    [[nodiscard]] const std::vector<Command>&
    UserCommands() const noexcept {
        return commandStore_
            .UserCommands();
    }

    [[nodiscard]] std::vector<ProviderStatus>
    ProviderStatuses() const;

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
    bool TestCommand(
        const Command& command);

    bool ImportUserCommands(
        const std::filesystem::path& path,
        bool legacyMode,
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
    bool SetStartWithWindows(bool enabled);
    bool SetShowOnStartup(bool enabled);
    bool SetHotkeySettings(
        std::vector<std::string> modifiers,
        std::string key);
    bool SetAuxiliaryHotkeySettings(
        bool enabled,
        std::vector<std::string> modifiers,
        std::string key);
    bool SetClassicBehavior(
        bool wildcardMatching,
        bool numericQuickLaunch,
        std::string numericQuickLaunchOrder,
        bool executeSingleResultImmediately);
    bool SetProviderEnabled(
        std::string id,
        bool enabled);
    bool RepairGlobalHotkey();

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

    void SetGeneralSettings(
        bool hideAfterLaunch,
        bool clearQueryOnShow,
        bool hideOnFocusLost,
        bool showTrayIcon,
        std::string popupMonitor);

    void ShowSettings();
    void OpenDataFolder();
    void OpenProjectPage();

    bool ExecuteCommand(
        std::size_t index);

    [[nodiscard]] const std::filesystem::path&
    DataDirectory() const noexcept {
        return dataDirectory_;
    }

private:
    static constexpr int
        kGlobalHotkeyId = 0xA171;

    static constexpr int
        kAuxiliaryHotkeyId = 0xA172;

    static constexpr UINT
        kProviderRefreshMessage =
            WM_APP + 0x171;

    static constexpr UINT
        kProviderChangedMessage =
            WM_APP + 0x172;

    bool LaunchCommand(
        const Command& command,
        bool recordUsage);
    bool ApplyStartupRegistration(
        bool enabled) const;
    bool RebindGlobalHotkey(
        const std::vector<std::string>& modifiers,
        std::string_view key);
    bool RebindAuxiliaryHotkey(
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

    HINSTANCE instance_{};
    std::filesystem::path
        baseDirectory_;
    std::filesystem::path
        dataDirectory_;
    CommandStore commandStore_;
    UsageStore usageStore_;
    SettingsStore settingsStore_;
    SearchEngine searchEngine_;
    std::unique_ptr<LauncherWindow>
        window_;
    std::unique_ptr<SettingsWindow>
        settingsWindow_;

    std::jthread
        providerRefreshThread_;
    std::jthread
        providerMonitorThread_;
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
};

} // namespace altrun
