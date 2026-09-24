#pragma once

#include "HotkeyRegistry.hpp"
#include "ProviderIds.hpp"
#include "UpdatePolicy.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace altrun {

enum class UiStyle {
    Classic,
    ModernCompact,
};

enum class Language {
    ZhCN,
    EnUS,
};

enum class StartupBehavior {
    Silent,
    Notification,
    ShowLauncher,
};

struct Settings {
    UiStyle uiStyle{UiStyle::Classic};
    Language language{Language::ZhCN};
    bool showResultIcons{false};

    bool startWithWindows{false};
    StartupBehavior startupBehavior{
        StartupBehavior::Notification};
    bool showTrayIcon{true};
    bool addToSendToMenu{false};
    std::string popupMonitor{"cursor"};
    std::string launcherPlacement{"top"};
    std::string settingsPlacement{"center"};
    std::string shortcutManagerPlacement{"center"};
    bool launcherLastPositionValid{false};
    int launcherLastX{0};
    int launcherLastY{0};
    bool settingsLastPositionValid{false};
    int settingsLastX{0};
    int settingsLastY{0};
    bool shortcutManagerLastPositionValid{false};
    int shortcutManagerLastX{0};
    int shortcutManagerLastY{0};

    std::vector<std::string>
        hotkeyModifiers{"alt"};
    std::string hotkeyKey{"space"};

    bool auxiliaryHotkeyEnabled{false};
    std::vector<std::string>
        auxiliaryHotkeyModifiers{};
    std::string auxiliaryHotkeyKey{"pause"};

    // schemaVersion 4 source of truth. The legacy primary/auxiliary
    // members above are compatibility mirrors for read-only downgrades.
    HotkeyBindingMap hotkeyBindings{
        DefaultHotkeyBindings()};

    bool pinyinSearch{true};
    bool numericQuickLaunch{false};
    bool executeSingleResultImmediately{false};

    ProviderEnableMap providerEnabled{
        providers::DefaultEnabled()};

    bool autoCheckUpdates{true};
    UpdateChannel updateChannel{
        UpdateChannel::Stable};
};

class SettingsStore {
public:
    SettingsStore(
        std::filesystem::path jsonPath,
        std::filesystem::path legacyIniPath = {});

    void Load();
    bool Save() const;

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
    bool SetHotkey(
        std::vector<std::string> modifiers,
        std::string key);
    bool SetAuxiliaryHotkey(
        bool enabled,
        std::vector<std::string> modifiers,
        std::string key);
    bool SetHotkeyBinding(
        std::string actionId,
        HotkeyBinding binding);
    bool ResetHotkeyBindings();
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
    bool SetWindowPlacement(
        std::string launcherPlacement,
        std::string settingsPlacement,
        std::string shortcutManagerPlacement);
    bool RememberLauncherPosition(
        int x,
        int y);
    bool RememberSettingsPosition(
        int x,
        int y);
    bool RememberShortcutManagerPosition(
        int x,
        int y);
    bool ResetDefaults();

    [[nodiscard]] const Settings&
    Data() const noexcept {
        return settings_;
    }

    [[nodiscard]] const std::filesystem::path&
    Path() const noexcept {
        return jsonPath_;
    }

    [[nodiscard]] bool
    IsReadOnlyDueToNewerSchema() const noexcept {
        return readOnlyDueToNewerSchema_;
    }

    [[nodiscard]] int
    UnsupportedSchemaVersion() const noexcept {
        return unsupportedSchemaVersion_;
    }

    [[nodiscard]] bool
    WasRecoveredFromBackup() const noexcept {
        return recoveredFromBackup_;
    }

    [[nodiscard]] bool
    WasMigratedFromOlderSchema() const noexcept {
        return migratedFromOlderSchema_;
    }

    [[nodiscard]] int
    MigratedFromSchemaVersion() const noexcept {
        return migratedFromSchemaVersion_;
    }

private:
    bool LoadJson();
    bool MigrateLegacyIni();

    std::filesystem::path jsonPath_;
    std::filesystem::path legacyIniPath_;
    Settings settings_;
    bool readOnlyDueToNewerSchema_{false};
    int unsupportedSchemaVersion_{0};
    bool recoveredFromBackup_{false};
    bool migratedFromOlderSchema_{false};
    int migratedFromSchemaVersion_{0};
};

} // namespace altrun
