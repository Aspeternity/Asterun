#pragma once

#include "HotkeyRegistry.hpp"
#include "ProviderIds.hpp"

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

struct Settings {
    UiStyle uiStyle{UiStyle::Classic};
    Language language{Language::ZhCN};
    bool showResultIcons{false};

    bool startWithWindows{false};
    bool showOnStartup{false};
    bool hideAfterLaunch{true};
    bool clearQueryOnShow{true};
    bool hideOnFocusLost{true};
    bool showTrayIcon{true};
    std::string popupMonitor{"cursor"};

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
    bool wildcardMatching{false};
    bool numericQuickLaunch{false};
    std::string numericQuickLaunchOrder{"one-to-zero"};
    bool executeSingleResultImmediately{false};

    ProviderEnableMap providerEnabled{
        providers::DefaultEnabled()};
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
    bool SetShowOnStartup(bool enabled);
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
        bool wildcardMatching,
        bool numericQuickLaunch,
        std::string numericQuickLaunchOrder,
        bool executeSingleResultImmediately,
        bool pinyinSearch);
    bool SetProviderEnabled(
        std::string id,
        bool enabled);
    bool ResetDefaults();
    void SetGeneral(
        bool hideAfterLaunch,
        bool clearQueryOnShow,
        bool hideOnFocusLost,
        bool showTrayIcon,
        std::string popupMonitor);

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
