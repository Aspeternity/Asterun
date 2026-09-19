#pragma once

#include "../core/CommandStore.hpp"
#include "../core/Localization.hpp"
#include "../core/SearchEngine.hpp"
#include "../core/Settings.hpp"
#include "../core/UsageStore.hpp"

#include <windows.h>

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
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

    [[nodiscard]] std::vector<SearchResult> Search(
        std::wstring_view query,
        std::size_t limit) const;

    [[nodiscard]] const Command& GetCommand(std::size_t index) const;

    [[nodiscard]] const std::vector<Command>& UserCommands() const noexcept {
        return commandStore_.UserCommands();
    }

    bool CreateUserCommand(Command command, std::wstring* createdId = nullptr);
    bool UpdateUserCommand(std::wstring_view id, Command command);
    bool DeleteUserCommand(std::wstring_view id);
    bool MoveUserCommand(std::wstring_view id, int direction);
    bool TestCommand(const Command& command);

    [[nodiscard]] const Settings& SettingsData() const noexcept {
        return settingsStore_.Data();
    }

    [[nodiscard]] std::wstring_view Text(TextId id) const;

    void SetUiStyle(UiStyle style);
    void SetLanguage(Language language);
    void SetGeneralSettings(
        bool hideAfterLaunch,
        bool clearQueryOnShow,
        bool hideOnFocusLost,
        bool showTrayIcon,
        std::string popupMonitor);

    void ShowSettings();
    void OpenDataFolder();
    void OpenProjectPage();

    bool ExecuteCommand(std::size_t index);

    [[nodiscard]] const std::filesystem::path& DataDirectory() const noexcept {
        return dataDirectory_;
    }

private:
    bool LaunchCommand(const Command& command, bool recordUsage);

    HINSTANCE instance_{};
    std::filesystem::path baseDirectory_;
    std::filesystem::path dataDirectory_;
    CommandStore commandStore_;
    UsageStore usageStore_;
    SettingsStore settingsStore_;
    SearchEngine searchEngine_;
    std::unique_ptr<LauncherWindow> window_;
    std::unique_ptr<SettingsWindow> settingsWindow_;
};

} // namespace altrun
