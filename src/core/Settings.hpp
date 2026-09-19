#pragma once

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

    bool startWithWindows{false};
    bool hideAfterLaunch{true};
    bool clearQueryOnShow{true};
    bool hideOnFocusLost{true};
    bool showTrayIcon{true};
    std::string popupMonitor{"cursor"};

    std::vector<std::string>
        hotkeyModifiers{"alt"};
    std::string hotkeyKey{"space"};

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
    bool SetStartWithWindows(bool enabled);
    bool SetHotkey(
        std::vector<std::string> modifiers,
        std::string key);
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

private:
    bool LoadJson();
    bool MigrateLegacyIni();

    std::filesystem::path jsonPath_;
    std::filesystem::path legacyIniPath_;
    Settings settings_;
    bool readOnlyDueToNewerSchema_{false};
    int unsupportedSchemaVersion_{0};
};

} // namespace altrun
