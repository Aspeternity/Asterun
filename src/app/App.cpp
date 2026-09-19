#include "App.hpp"

#include "../platform/Hotkey.hpp"
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
          baseDirectory_ / "settings.ini"),
      searchEngine_(
          baseDirectory_ / "dict") {}

App::~App() {
    if (providerRefreshThread_.joinable()) {
        providerRefreshThread_.request_stop();
        providerRefreshThread_.join();
    }

    if (hotkeyRegistered_) {
        UnregisterHotKey(
            nullptr,
            kGlobalHotkeyId);
        hotkeyRegistered_ = false;
    }

    if (singleInstanceMutex_) {
        CloseHandle(singleInstanceMutex_);
        singleInstanceMutex_ = nullptr;
    }
}

int App::Run() {
    std::error_code ec;
    std::filesystem::create_directories(dataDirectory_, ec);

    uiThreadId_ = GetCurrentThreadId();

    settingsStore_.Load();

    SetLastError(ERROR_SUCCESS);
    singleInstanceMutex_ =
        CreateMutexW(
            nullptr,
            FALSE,
            L"Local\\Aspeternity.ALTRunNext.SingleInstance.v1");

    if (!singleInstanceMutex_) {
        MessageBoxW(
            nullptr,
            L"Unable to create the ALTRun Next single-instance guard.",
            L"ALTRun Next",
            MB_ICONERROR | MB_OK);
        return 1;
    }

    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        MessageBoxW(
            nullptr,
            settingsStore_.Data().language == Language::ZhCN
                ? L"ALTRun Next 已经在运行。\n\n请检查系统托盘，避免多个实例同时抢占全局热键。"
                : L"ALTRun Next is already running.\n\nCheck the system tray. Multiple instances are blocked to prevent global-hotkey conflicts.",
            L"ALTRun Next",
            MB_ICONINFORMATION | MB_OK);
        return 0;
    }

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

    if (!RebindGlobalHotkey(
            settingsStore_.Data().hotkeyModifiers,
            settingsStore_.Data().hotkeyKey)) {
        MessageBoxW(
            nullptr,
            Text(TextId::HotkeyBusy).data(),
            L"ALTRun Next",
            MB_ICONWARNING | MB_OK);
    }

    // Cached provider results are already searchable. Refresh automatic
    // discovery off the startup path and hot-reload it when ready.
    StartProviderRefresh();

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (msg.message == kProviderRefreshMessage &&
            msg.hwnd == nullptr) {
            HandleProviderRefreshCompleted(
                msg.wParam != 0);
            continue;
        }

        if (msg.message == WM_HOTKEY &&
            msg.hwnd == nullptr &&
            msg.wParam ==
                static_cast<WPARAM>(kGlobalHotkeyId)) {
            if (window_) {
                window_->Toggle();
            }
            continue;
        }

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
    StartProviderRefresh();
}

void App::StartProviderRefresh() {
    bool expected = false;

    if (!providerRefreshRunning_
             .compare_exchange_strong(
                 expected,
                 true)) {
        return;
    }

    if (providerRefreshThread_.joinable()) {
        providerRefreshThread_.join();
    }

    const DWORD targetThread =
        uiThreadId_;

    providerRefreshThread_ =
        std::jthread(
            [this, targetThread](
                std::stop_token stopToken) {

                bool success = false;

                try {
                    if (!stopToken.stop_requested()) {
                        auto discovered =
                            commandStore_
                                .DiscoverProviderCommands();

                        if (!stopToken.stop_requested()) {
                            success =
                                commandStore_
                                    .SaveProviderCache(
                                        discovered);
                        }
                    }
                } catch (...) {
                    success = false;
                }

                if (targetThread != 0) {
                    PostThreadMessageW(
                        targetThread,
                        kProviderRefreshMessage,
                        success ? 1 : 0,
                        0);
                }
            });
}

void App::HandleProviderRefreshCompleted(
    bool success) {

    if (providerRefreshThread_.joinable()) {
        providerRefreshThread_.join();
    }

    providerRefreshRunning_ = false;

    if (success) {
        commandStore_.ReloadProviderCache();

        if (window_) {
            window_->RefreshResults();
        }

        if (settingsWindow_) {
            settingsWindow_->RefreshCommands();
        }
    }

    if (settingsWindow_) {
        settingsWindow_
            ->OnProgramIndexRefreshCompleted(
                success);
    }
}

bool App::RestoreDefaultSettings() {
    const Settings previous =
        settingsStore_.Data();

    const Settings defaults{};

    if (!RebindGlobalHotkey(
            defaults.hotkeyModifiers,
            defaults.hotkeyKey)) {
        return false;
    }

    if (!ApplyStartupRegistration(false)) {
        RebindGlobalHotkey(
            previous.hotkeyModifiers,
            previous.hotkeyKey);
        return false;
    }

    if (!settingsStore_.ResetDefaults()) {
        ApplyStartupRegistration(
            previous.startWithWindows);

        RebindGlobalHotkey(
            previous.hotkeyModifiers,
            previous.hotkeyKey);

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

bool App::RebindGlobalHotkey(
    const std::vector<std::string>& modifiers,
    std::string_view key) {

    const UINT newModifiers =
        hotkey::ModifiersFromNames(modifiers);

    const UINT newVk =
        hotkey::KeyFromName(key);

    constexpr UINT kModifierMask =
        MOD_ALT | MOD_CONTROL | MOD_SHIFT | MOD_WIN;

    if (newVk == 0 ||
        (newModifiers & kModifierMask) == 0) {
        hotkeyLastError_ = ERROR_INVALID_PARAMETER;
        return false;
    }

    const bool hadOld =
        hotkeyRegistered_;

    const UINT oldModifiers =
        currentHotkeyModifiers_;

    const UINT oldVk =
        currentHotkeyVk_;

    // Always unregister and register again, even when the requested
    // combination is unchanged. The old implementation returned early
    // based only on its cached flag, which could report success after the
    // actual Windows registration had become unavailable.
    if (hadOld) {
        UnregisterHotKey(
            nullptr,
            kGlobalHotkeyId);
        hotkeyRegistered_ = false;
    }

    SetLastError(ERROR_SUCCESS);

    if (RegisterHotKey(
            nullptr,
            kGlobalHotkeyId,
            newModifiers,
            newVk)) {

        currentHotkeyModifiers_ =
            newModifiers;
        currentHotkeyVk_ =
            newVk;
        hotkeyRegistered_ = true;
        hotkeyLastError_ = ERROR_SUCCESS;
        return true;
    }

    const DWORD registrationError =
        GetLastError();

    // Re-establish the previous working binding when a new combination
    // cannot be registered. This makes changing a hotkey transactional.
    if (hadOld &&
        oldVk != 0 &&
        RegisterHotKey(
            nullptr,
            kGlobalHotkeyId,
            oldModifiers,
            oldVk)) {

        currentHotkeyModifiers_ =
            oldModifiers;
        currentHotkeyVk_ =
            oldVk;
        hotkeyRegistered_ = true;
    } else {
        currentHotkeyModifiers_ = 0;
        currentHotkeyVk_ = 0;
        hotkeyRegistered_ = false;
    }

    hotkeyLastError_ =
        registrationError != ERROR_SUCCESS
            ? registrationError
            : ERROR_HOTKEY_ALREADY_REGISTERED;

    return false;
}

bool App::RepairGlobalHotkey() {
    return RebindGlobalHotkey(
        settingsStore_.Data().hotkeyModifiers,
        settingsStore_.Data().hotkeyKey);
}

bool App::SetHotkeySettings(
    std::vector<std::string> modifiers,
    std::string key) {

    const auto previousModifiers =
        settingsStore_.Data().hotkeyModifiers;

    const auto previousKey =
        settingsStore_.Data().hotkeyKey;

    if (!RebindGlobalHotkey(
            modifiers,
            key)) {
        return false;
    }

    if (!settingsStore_.SetHotkey(
            std::move(modifiers),
            std::move(key))) {

        RebindGlobalHotkey(
            previousModifiers,
            previousKey);

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
