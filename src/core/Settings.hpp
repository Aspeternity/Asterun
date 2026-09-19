#pragma once

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

    std::vector<std::string> hotkeyModifiers{"alt"};
    std::string hotkeyKey{"space"};
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
    void SetGeneral(
        bool hideAfterLaunch,
        bool clearQueryOnShow,
        bool hideOnFocusLost,
        bool showTrayIcon,
        std::string popupMonitor);

    [[nodiscard]] const Settings& Data() const noexcept { return settings_; }
    [[nodiscard]] const std::filesystem::path& Path() const noexcept { return jsonPath_; }

private:
    bool LoadJson();
    bool MigrateLegacyIni();

    std::filesystem::path jsonPath_;
    std::filesystem::path legacyIniPath_;
    Settings settings_;
};

} // namespace altrun
