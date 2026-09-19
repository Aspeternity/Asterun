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
    ApplyStartupRegistration(
        settingsStore_.Data().startWithWindows);
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

bool App::CreateUserCommand(
    Command command,
    std::wstring* createdId) {

    if (!commandStore_.CreateUserCommand(
            std::move(command),
            createdId)) {
        return false;
    }

    if (window_) window_->RefreshResults();
    return true;
}

bool App::UpdateUserCommand(
    std::wstring_view id,
    Command command) {

    if (!commandStore_.UpdateUserCommand(
            id,
            std::move(command))) {
        return false;
    }

    if (window_) window_->RefreshResults();
    return true;
}

bool App::DeleteUserCommand(
    std::wstring_view id) {

    if (!commandStore_.DeleteUserCommand(id)) {
        return false;
    }

    if (window_) window_->RefreshResults();
    return true;
}

bool App::MoveUserCommand(
    std::wstring_view id,
    int direction) {

    if (!commandStore_.MoveUserCommand(
            id,
            direction)) {
        return false;
    }

    if (window_) window_->RefreshResults();
    return true;
}

bool App::TestCommand(const Command& command) {
    return LaunchCommand(command, false);
}

bool App::ImportUserCommands(
    const std::filesystem::path& path,
    bool legacyMode,
    std::size_t* imported,
    std::size_t* skipped) {

    if (!commandStore_.ImportUserCommands(
            path,
            legacyMode,
            imported,
            skipped)) {
        return false;
    }

    if (window_) {
        window_->RefreshResults();
    }

    if (settingsWindow_) {
        settingsWindow_->RefreshCommands();
    }

    return true;
}

bool App::ExportUserCommands(
    const std::filesystem::path& path) const {

    return commandStore_.ExportUserCommands(path);
}

bool App::ClearUsageHistory() {
    if (!usageStore_.Clear()) {
        return false;
    }

    if (window_) {
        window_->RefreshResults();
    }

    return true;
}

void App::RebuildProgramIndex() {
    commandStore_.Reload();

    if (window_) {
        window_->RefreshResults();
    }

    if (settingsWindow_) {
        settingsWindow_->RefreshCommands();
    }
}

bool App::RestoreDefaultSettings() {
    const Settings previous =
        settingsStore_.Data();

    const Settings defaults{};

    if (window_ &&
        !window_->RebindHotkey(
            defaults.hotkeyModifiers,
            defaults.hotkeyKey)) {
        return false;
    }

    if (!ApplyStartupRegistration(false)) {
        if (window_) {
            window_->RebindHotkey(
                previous.hotkeyModifiers,
                previous.hotkeyKey);
        }
        return false;
    }

    if (!settingsStore_.ResetDefaults()) {
        ApplyStartupRegistration(
            previous.startWithWindows);

        if (window_) {
            window_->RebindHotkey(
                previous.hotkeyModifiers,
                previous.hotkeyKey);
        }

        return false;
    }

    if (window_) {
        window_->ApplyAppearance();
        window_->ApplyLanguage();
        window_->ApplyGeneralSettings();
    }

    if (settingsWindow_) {
        settingsWindow_->ApplyLanguage();
        settingsWindow_->RefreshFromSettings();
    }

    return true;
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

bool App::SetStartWithWindows(bool enabled) {
    const bool previous =
        settingsStore_.Data().startWithWindows;

    if (!ApplyStartupRegistration(enabled)) {
        return false;
    }

    if (!settingsStore_.SetStartWithWindows(enabled)) {
        ApplyStartupRegistration(previous);
        return false;
    }

    if (settingsWindow_) {
        settingsWindow_->RefreshFromSettings();
    }

    return true;
}

bool App::SetHotkeySettings(
    std::vector<std::string> modifiers,
    std::string key) {

    const auto previousModifiers =
        settingsStore_.Data().hotkeyModifiers;

    const auto previousKey =
        settingsStore_.Data().hotkeyKey;

    if (window_ &&
        !window_->RebindHotkey(
            modifiers,
            key)) {
        return false;
    }

    if (!settingsStore_.SetHotkey(
            std::move(modifiers),
            std::move(key))) {

        if (window_) {
            window_->RebindHotkey(
                previousModifiers,
                previousKey);
        }

        return false;
    }

    if (settingsWindow_) {
        settingsWindow_->RefreshFromSettings();
    }

    return true;
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

bool App::ApplyStartupRegistration(
    bool enabled) const {

    constexpr wchar_t kRunKey[] =
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";

    constexpr wchar_t kValueName[] =
        L"ALTRunNext";

    HKEY key{};

    const LSTATUS openStatus =
        RegCreateKeyExW(
            HKEY_CURRENT_USER,
            kRunKey,
            0,
            nullptr,
            REG_OPTION_NON_VOLATILE,
            KEY_SET_VALUE,
            nullptr,
            &key,
            nullptr);

    if (openStatus != ERROR_SUCCESS) {
        return false;
    }

    bool success = false;

    if (enabled) {
        std::vector<wchar_t> executable(32768);
        const DWORD length =
            GetModuleFileNameW(
                nullptr,
                executable.data(),
                static_cast<DWORD>(
                    executable.size()));

        if (length > 0 &&
            length < executable.size()) {

            std::wstring command = L"\"";
            command.append(
                executable.data(),
                length);
            command += L"\"";

            const DWORD bytes =
                static_cast<DWORD>(
                    (command.size() + 1) *
                    sizeof(wchar_t));

            success =
                RegSetValueExW(
                    key,
                    kValueName,
                    0,
                    REG_SZ,
                    reinterpret_cast<const BYTE*>(
                        command.c_str()),
                    bytes) == ERROR_SUCCESS;
        }
    } else {
        const LSTATUS status =
            RegDeleteValueW(
                key,
                kValueName);

        success =
            status == ERROR_SUCCESS ||
            status == ERROR_FILE_NOT_FOUND;
    }

    RegCloseKey(key);
    return success;
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
    return LaunchCommand(
        commandStore_.Commands().at(index),
        true);
}

bool App::LaunchCommand(
    const Command& command,
    bool recordUsage) {

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

    if (recordUsage && !command.id.empty()) {
        usageStore_.Record(command.id);
    }

    return true;
}

} // namespace altrun
