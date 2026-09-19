#pragma once

#include "../core/CommandStore.hpp"
#include "../core/Localization.hpp"
#include "../core/SearchEngine.hpp"
#include "../core/Settings.hpp"
#include "../core/UsageStore.hpp"

#include <windows.h>

#include <filesystem>
#include <memory>
#include <string_view>
#include <vector>

namespace altrun {

class LauncherWindow;

class App {
public:
    explicit App(HINSTANCE instance);
    ~App();

    int Run();
    void ReloadCommands();

    [[nodiscard]] std::vector<SearchResult> Search(
        std::wstring_view query,
        std::size_t limit) const;

    [[nodiscard]] const Command& GetCommand(std::size_t index) const;

    [[nodiscard]] const Settings& SettingsData() const noexcept {
        return settingsStore_.Data();
    }

    [[nodiscard]] std::wstring_view Text(TextId id) const;

    void SetUiStyle(UiStyle style);
    void SetLanguage(Language language);
    bool ExecuteCommand(std::size_t index);

private:
    HINSTANCE instance_{};
    std::filesystem::path baseDirectory_;
    std::filesystem::path dataDirectory_;
    CommandStore commandStore_;
    UsageStore usageStore_;
    SettingsStore settingsStore_;
    SearchEngine searchEngine_;
    std::unique_ptr<LauncherWindow> window_;
};

} // namespace altrun
