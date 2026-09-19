#pragma once

#include <filesystem>

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
};

class SettingsStore {
public:
    explicit SettingsStore(std::filesystem::path path);

    void Load();
    void Save() const;

    void SetUiStyle(UiStyle style);
    void SetLanguage(Language language);

    [[nodiscard]] const Settings& Data() const noexcept { return settings_; }

private:
    std::filesystem::path path_;
    Settings settings_;
};

} // namespace altrun
