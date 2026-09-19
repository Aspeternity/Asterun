#include "App.hpp"

#include "../platform/WinUtil.hpp"
#include "../ui/LauncherWindow.hpp"
#include "../ui/SettingsWindow.hpp"

#include <shellapi.h>

namespace altrun {

App::App(HINSTANCE instance)
    : instance_(instance),
      baseDirectory_(win::ExecutableDirectory()),
      dataDirectory_(baseDirectory_ / "data"),
      commandStore_(baseDirectory_, dataDirectory_),
      usageStore_(
          dataDirectory_ / "usage.json",
          baseDirectory_ / "usage.tsv"),
      settingsStore_(
          dataDirectory_ / "settings.json",
          baseDirectory_ / "settings.ini") {}

App::~App() = default;

int App::Run() {
    std::error_code ec;
    std::filesystem::create_directories(dataDirectory_, ec);

    settingsStore_.Load();
    commandStore_.Reload();
    usageStore_.Load(commandStore_.LegacyIdMap());

    window_ = std::make_unique<LauncherWindow>(*this, instance_);
    if (!window_->Create()) {
        MessageBoxW(
            nullptr,
            Text(TextId::CreateWindowFailed).data(),
            L"ALTRun Next",
            MB_ICONERROR | MB_OK);
        return 1;
    }

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return static_cast<int>(msg.wParam);
}

void App::ReloadCommands() {
    commandStore_.Reload();
    if (window_) window_->RefreshResults();
}

std::vector<SearchResult> App::Search(
    std::wstring_view query,
    std::size_t limit) const {

    return searchEngine_.Search(
        commandStore_.Commands(),
        usageStore_.Data(),
        query,
        limit);
}

const Command& App::GetCommand(std::size_t index) const {
    return commandStore_.Commands().at(index);
}

std::wstring_view App::Text(TextId id) const {
    return LocalizedText(id, settingsStore_.Data().language);
}

void App::SetUiStyle(UiStyle style) {
    settingsStore_.SetUiStyle(style);
    if (window_) window_->ApplyAppearance();
    if (settingsWindow_) settingsWindow_->RefreshFromSettings();
}

void App::SetLanguage(Language language) {
    settingsStore_.SetLanguage(language);
    if (window_) window_->ApplyLanguage();
    if (settingsWindow_) settingsWindow_->ApplyLanguage();
}

void App::SetGeneralSettings(
    bool hideAfterLaunch,
    bool clearQueryOnShow,
    bool hideOnFocusLost,
    bool showTrayIcon,
    std::string popupMonitor) {

    settingsStore_.SetGeneral(
        hideAfterLaunch,
        clearQueryOnShow,
        hideOnFocusLost,
        showTrayIcon,
        std::move(popupMonitor));

    if (window_) window_->ApplyGeneralSettings();
    if (settingsWindow_) settingsWindow_->RefreshFromSettings();
}

void App::ShowSettings() {
    if (!settingsWindow_) {
        settingsWindow_ = std::make_unique<SettingsWindow>(*this, instance_);
        if (!settingsWindow_->Create()) {
            settingsWindow_.reset();
            MessageBoxW(
                nullptr,
                L"Unable to create Settings window.",
                L"ALTRun Next",
                MB_ICONERROR | MB_OK);
            return;
        }
    }

    settingsWindow_->Show();
}

void App::OpenDataFolder() {
    std::error_code ec;
    std::filesystem::create_directories(dataDirectory_, ec);
    ShellExecuteW(
        nullptr,
        L"open",
        dataDirectory_.c_str(),
        nullptr,
        nullptr,
        SW_SHOWNORMAL);
}

void App::OpenProjectPage() {
    ShellExecuteW(
        nullptr,
        L"open",
        L"https://github.com/Aspeternity/ALTRunNext",
        nullptr,
        nullptr,
        SW_SHOWNORMAL);
}

bool App::ExecuteCommand(std::size_t index) {
    const auto& command = commandStore_.Commands().at(index);

    const std::wstring target = win::ExpandEnvironment(command.target);
    const std::wstring args = win::ExpandEnvironment(command.arguments);
    const std::wstring cwd = win::ExpandEnvironment(command.workingDirectory);

    SHELLEXECUTEINFOW info{};
    info.cbSize = sizeof(info);
    info.fMask = SEE_MASK_NOASYNC | SEE_MASK_FLAG_NO_UI;
    info.hwnd = nullptr;
    info.lpVerb = command.runAsAdmin ? L"runas" : nullptr;
    info.lpFile = target.c_str();
    info.lpParameters = args.empty() ? nullptr : args.c_str();
    info.lpDirectory = cwd.empty() ? nullptr : cwd.c_str();
    info.nShow = SW_SHOWNORMAL;

    if (!ShellExecuteExW(&info)) {
        const DWORD error = GetLastError();
        std::wstring message =
            std::wstring(Text(TextId::UnableToLaunch)) +
            L"\n" +
            target +
            L"\n\n" +
            win::FormatWin32Error(error);

        MessageBoxW(
            nullptr,
            message.c_str(),
            L"ALTRun Next",
            MB_ICONERROR | MB_OK);

        return false;
    }

    usageStore_.Record(command.id);
    return true;
}

} // namespace altrun
