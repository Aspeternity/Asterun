#include "App.hpp"

#include "../platform/Hotkey.hpp"
#include "../platform/WinUtil.hpp"
#include "../ui/LauncherWindow.hpp"
#include "../ui/SettingsWindow.hpp"

#include <shellapi.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <unordered_map>

namespace altrun {

namespace {

bool ProbeDirectoryWritable(
    const std::filesystem::path& directory) {

    std::error_code ec;

    if (!std::filesystem::exists(
            directory,
            ec) ||
        ec) {
        return false;
    }

    const auto probe =
        directory /
        (".altrun-write-test-" +
         std::to_string(
             GetCurrentProcessId()) +
         ".tmp");

    {
        std::ofstream output(
            probe,
            std::ios::binary |
                std::ios::trunc);

        if (!output) {
            return false;
        }

        output << "write-test";
        output.flush();

        if (!output) {
            output.close();
            std::filesystem::remove(
                probe,
                ec);
            return false;
        }
    }

    ec.clear();
    std::filesystem::remove(
        probe,
        ec);

    return !ec;
}

} // namespace

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
    if (providerDebounceTimer_ != 0) {
        KillTimer(
            nullptr,
            providerDebounceTimer_);
        providerDebounceTimer_ = 0;
    }

    if (providerMonitorThread_.joinable()) {
        providerMonitorThread_.request_stop();
        providerMonitorThread_.join();
    }

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

    if (auxiliaryHotkeyRegistered_) {
        UnregisterHotKey(
            nullptr,
            kAuxiliaryHotkeyId);
        auxiliaryHotkeyRegistered_ =
            false;
    }

    if (singleInstanceMutex_) {
        CloseHandle(singleInstanceMutex_);
        singleInstanceMutex_ = nullptr;
    }
}

int App::Run() {
    std::error_code ec;

    std::filesystem::create_directories(
        dataDirectory_,
        ec);

    dataDirectoryWritable_ =
        !ec &&
        ProbeDirectoryWritable(
            dataDirectory_);

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

    if (!dataDirectoryWritable_) {
        MessageBoxW(
            nullptr,
            settingsStore_.Data().language == Language::ZhCN
                ? L"ALTRun Next 的 data 目录当前不可写。\n\n程序仍会继续运行，但设置、快捷项和使用记录可能无法保存。请将程序移动到可写目录或检查文件夹权限。"
                : L"The ALTRun Next data directory is not writable.\n\nThe launcher will continue running, but settings, shortcuts and usage history may not persist. Move ALTRun Next to a writable folder or check folder permissions.",
            L"ALTRun Next",
            MB_ICONWARNING | MB_OK);
    }

    ApplyStartupRegistration(
        settingsStore_.Data().startWithWindows);
    commandStore_.Reload(
        settingsStore_.Data()
            .providerEnabled);
    usageStore_.Load(
        commandStore_.LegacyIdMap());

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

    if (!RebindAuxiliaryHotkey(
            settingsStore_.Data()
                .auxiliaryHotkeyEnabled,
            settingsStore_.Data()
                .auxiliaryHotkeyModifiers,
            settingsStore_.Data()
                .auxiliaryHotkeyKey)) {

        MessageBoxW(
            nullptr,
            settingsStore_.Data().language ==
                    Language::ZhCN
                ? L"辅助热键注册失败，主热键仍可继续使用。请检查该按键是否已被其他程序占用。"
                : L"The auxiliary hotkey could not be registered. The primary hotkey remains available. Check whether another application already uses the binding.",
            L"ALTRun Next",
            MB_ICONWARNING | MB_OK);
    }

    if (settingsStore_.Data().showOnStartup) {
        window_->Show();
    }

    // Cached provider results are already searchable. Refresh automatic
    // discovery off the startup path, then keep lightweight provider
    // fingerprints under observation for source-specific updates.
    StartProviderRefresh();
    StartProviderMonitor();

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (msg.message == kProviderRefreshMessage &&
            msg.hwnd == nullptr) {
            HandleProviderRefreshCompleted(
                static_cast<
                    ProviderRefreshOutcome>(
                        msg.wParam));
            continue;
        }

        if (msg.message ==
                kProviderChangedMessage &&
            msg.hwnd == nullptr) {
            HandleProviderChangedSignal();
            continue;
        }

        if (msg.message == WM_TIMER &&
            msg.hwnd == nullptr &&
            providerDebounceTimer_ != 0 &&
            msg.wParam ==
                providerDebounceTimer_) {

            KillTimer(
                nullptr,
                providerDebounceTimer_);

            providerDebounceTimer_ = 0;

            FlushDetectedProviderChanges();
            continue;
        }

        if (msg.message == WM_HOTKEY &&
            msg.hwnd == nullptr &&
            (msg.wParam ==
                 static_cast<WPARAM>(
                     kGlobalHotkeyId) ||
             msg.wParam ==
                 static_cast<WPARAM>(
                     kAuxiliaryHotkeyId))) {

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
    commandStore_.Reload(
        settingsStore_.Data()
            .providerEnabled);

    if (window_) {
        window_->RefreshResults();
    }
}

std::vector<SearchResult> App::Search(
    std::wstring_view query,
    std::size_t limit) const {

    return searchEngine_.Search(
        commandStore_.Commands(),
        usageStore_.Data(),
        query,
        limit,
        settingsStore_.Data()
            .wildcardMatching);
}

const Command& App::GetCommand(
    std::size_t index) const {

    return commandStore_
        .Commands()
        .at(index);
}

std::vector<ProviderStatus>
App::ProviderStatuses() const {
    return commandStore_
        .ProviderStatuses(
            settingsStore_.Data()
                .providerEnabled);
}

std::wstring
App::DataCompatibilityWarning() const {

    struct SchemaIssue {
        const wchar_t* file;
        int schemaVersion;
    };

    std::vector<SchemaIssue>
        schemaIssues;

    std::vector<const wchar_t*>
        recoveredFiles;

    if (settingsStore_
            .IsReadOnlyDueToNewerSchema()) {
        schemaIssues.push_back({
            L"settings.json",
            settingsStore_
                .UnsupportedSchemaVersion(),
        });
    }

    if (commandStore_
            .UserCommandsReadOnlyDueToNewerSchema()) {
        schemaIssues.push_back({
            L"commands.json",
            commandStore_
                .UserCommandsUnsupportedSchemaVersion(),
        });
    }

    if (usageStore_
            .IsReadOnlyDueToNewerSchema()) {
        schemaIssues.push_back({
            L"usage.json",
            usageStore_
                .UnsupportedSchemaVersion(),
        });
    }

    if (settingsStore_
            .WasRecoveredFromBackup()) {
        recoveredFiles.push_back(
            L"settings.json");
    }

    if (commandStore_
            .UserCommandsRecoveredFromBackup()) {
        recoveredFiles.push_back(
            L"commands.json");
    }

    if (usageStore_
            .WasRecoveredFromBackup()) {
        recoveredFiles.push_back(
            L"usage.json");
    }

    if (dataDirectoryWritable_ &&
        schemaIssues.empty() &&
        recoveredFiles.empty()) {
        return {};
    }

    const bool zh =
        settingsStore_.Data().language ==
            Language::ZhCN;

    std::wstring message;

    if (!dataDirectoryWritable_) {
        message +=
            zh
                ? L"数据目录不可写：设置、快捷项和使用记录可能无法保存。"
                : L"Data directory is not writable; settings, shortcuts and usage history may not persist.";
    }

    if (!schemaIssues.empty()) {
        if (!message.empty()) {
            message += L"\r\n\r\n";
        }

        message +=
            zh
                ? L"检测到由较新版本生成的数据文件。为避免降级覆盖数据，以下文件已进入只读兼容模式："
                : L"Data created by a newer ALTRun Next version was detected. To prevent downgrade data loss, these files are read-only:";

        message += L"\r\n";

        for (std::size_t i = 0;
             i < schemaIssues.size();
             ++i) {

            message += L"  ";
            message +=
                schemaIssues[i].file;
            message += L"  (schema ";
            message += std::to_wstring(
                schemaIssues[i]
                    .schemaVersion);
            message += L")";

            if (i + 1 <
                schemaIssues.size()) {
                message += L"\r\n";
            }
        }
    }

    if (!recoveredFiles.empty()) {
        if (!message.empty()) {
            message += L"\r\n\r\n";
        }

        message +=
            zh
                ? L"本次启动已从 .bak 自动恢复并修复以下数据文件："
                : L"This startup recovered and repaired these data files from .bak:";

        message += L"\r\n";

        for (std::size_t i = 0;
             i < recoveredFiles.size();
             ++i) {

            message += L"  ";
            message +=
                recoveredFiles[i];

            if (i + 1 <
                recoveredFiles.size()) {
                message += L"\r\n";
            }
        }
    }

    return message;
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

void App::StartProviderRefresh(
    std::vector<std::string>
        selectedIds) {

    const auto& enabled =
        settingsStore_.Data()
            .providerEnabled;

    if (!selectedIds.empty()) {
        selectedIds.erase(
            std::remove_if(
                selectedIds.begin(),
                selectedIds.end(),
                [&](const std::string& id) {
                    return !providers::IsEnabled(
                        enabled,
                        id,
                        true);
                }),
            selectedIds.end());

        std::sort(
            selectedIds.begin(),
            selectedIds.end());

        selectedIds.erase(
            std::unique(
                selectedIds.begin(),
                selectedIds.end()),
            selectedIds.end());

        if (selectedIds.empty()) {
            return;
        }
    }

    bool expected = false;

    if (!providerRefreshRunning_
             .compare_exchange_strong(
                 expected,
                 true)) {

        if (selectedIds.empty()) {
            providerRefreshFullPending_ =
                true;
            providerRefreshIdsPending_
                .clear();
        } else if (
            !providerRefreshFullPending_) {
            providerRefreshIdsPending_
                .insert(
                    selectedIds.begin(),
                    selectedIds.end());
        }

        return;
    }

    if (providerRefreshThread_
            .joinable()) {
        providerRefreshThread_
            .join();
    }

    const DWORD targetThread =
        uiThreadId_;

    const ProviderEnableMap
        enabledSnapshot =
            settingsStore_.Data()
                .providerEnabled;

    providerRefreshThread_ =
        std::jthread(
            [this,
             targetThread,
             enabledSnapshot,
             selectedIds =
                 std::move(
                     selectedIds)](
                std::stop_token
                    stopToken) {

                ProviderRefreshOutcome
                    outcome =
                        ProviderRefreshOutcome::
                            Failed;

                try {
                    if (!stopToken
                             .stop_requested()) {
                        outcome =
                            commandStore_
                                .RefreshProviderCache(
                                    enabledSnapshot,
                                    selectedIds);
                    }
                } catch (...) {
                    outcome =
                        ProviderRefreshOutcome::
                            Failed;
                }

                if (targetThread != 0) {
                    PostThreadMessageW(
                        targetThread,
                        kProviderRefreshMessage,
                        static_cast<WPARAM>(
                            outcome),
                        0);
                }
            });
}

void App::HandleProviderRefreshCompleted(
    ProviderRefreshOutcome outcome) {

    if (providerRefreshThread_
            .joinable()) {
        providerRefreshThread_
            .join();
    }

    providerRefreshRunning_ = false;

    if (outcome !=
        ProviderRefreshOutcome::Failed) {

        commandStore_
            .ReloadProviderCache(
                settingsStore_.Data()
                    .providerEnabled);

        if (window_) {
            window_->RefreshResults();
        }

        if (settingsWindow_) {
            settingsWindow_
                ->RefreshCommands();
        }
    }

    if (settingsWindow_) {
        settingsWindow_
            ->OnProgramIndexRefreshCompleted(
                static_cast<int>(
                    outcome));
    }

    if (providerRefreshFullPending_) {
        providerRefreshFullPending_ =
            false;
        providerRefreshIdsPending_
            .clear();
        StartProviderRefresh();
        return;
    }

    if (!providerRefreshIdsPending_
             .empty()) {

        std::vector<std::string>
            pending(
                providerRefreshIdsPending_
                    .begin(),
                providerRefreshIdsPending_
                    .end());

        providerRefreshIdsPending_
            .clear();

        StartProviderRefresh(
            std::move(pending));
    }
}

void App::StartProviderMonitor() {
    if (providerMonitorThread_
            .joinable()) {
        return;
    }

    {
        std::scoped_lock lock(
            providerMonitorConfigMutex_);

        providerMonitorEnabled_ =
            settingsStore_.Data()
                .providerEnabled;
    }

    const DWORD targetThread =
        uiThreadId_;

    providerMonitorThread_ =
        std::jthread(
            [this, targetThread](
                std::stop_token
                    stopToken) {

                std::unordered_map<
                    std::string,
                    std::uint64_t>
                    baseline;

                ProviderEnableMap
                    enabledSnapshot;

                {
                    std::scoped_lock lock(
                        providerMonitorConfigMutex_);

                    enabledSnapshot =
                        providerMonitorEnabled_;
                }

                for (const auto& token :
                     commandStore_
                         .ProviderChangeTokens(
                             enabledSnapshot)) {
                    if (token.success) {
                        baseline[token.id] =
                            token.token;
                    }
                }

                constexpr auto
                    kPollInterval =
                        std::chrono::
                            milliseconds(5000);

                constexpr auto
                    kSleepSlice =
                        std::chrono::
                            milliseconds(100);

                while (!stopToken
                            .stop_requested()) {

                    auto remaining =
                        kPollInterval;

                    while (remaining >
                               std::chrono::
                                   milliseconds(0) &&
                           !stopToken
                                .stop_requested()) {

                        const auto slice =
                            std::min(
                                remaining,
                                kSleepSlice);

                        std::this_thread::
                            sleep_for(slice);

                        remaining -= slice;
                    }

                    if (stopToken
                            .stop_requested()) {
                        break;
                    }

                    {
                        std::scoped_lock lock(
                            providerMonitorConfigMutex_);

                        enabledSnapshot =
                            providerMonitorEnabled_;
                    }

                    std::vector<std::string>
                        changed;

                    for (const auto& token :
                         commandStore_
                             .ProviderChangeTokens(
                                 enabledSnapshot)) {

                        if (!token.success) {
                            continue;
                        }

                        const auto previous =
                            baseline.find(
                                token.id);

                        if (previous !=
                                baseline.end() &&
                            previous->second !=
                                token.token) {
                            changed.push_back(
                                token.id);
                        }

                        baseline[token.id] =
                            token.token;
                    }

                    for (auto it =
                             baseline.begin();
                         it != baseline.end();) {
                        if (!providers::IsEnabled(
                                enabledSnapshot,
                                it->first,
                                true)) {
                            it =
                                baseline.erase(it);
                        } else {
                            ++it;
                        }
                    }

                    if (changed.empty()) {
                        continue;
                    }

                    {
                        std::scoped_lock lock(
                            detectedProviderMutex_);

                        detectedProviderIds_
                            .insert(
                                changed.begin(),
                                changed.end());
                    }

                    if (targetThread != 0) {
                        PostThreadMessageW(
                            targetThread,
                            kProviderChangedMessage,
                            0,
                            0);
                    }
                }
            });
}

void App::HandleProviderChangedSignal() {
    if (providerDebounceTimer_ != 0) {
        KillTimer(
            nullptr,
            providerDebounceTimer_);
        providerDebounceTimer_ = 0;
    }

    providerDebounceTimer_ =
        SetTimer(
            nullptr,
            0,
            750,
            nullptr);

    if (providerDebounceTimer_ == 0) {
        FlushDetectedProviderChanges();
    }
}

void App::FlushDetectedProviderChanges() {
    std::unordered_set<std::string>
        detected;

    {
        std::scoped_lock lock(
            detectedProviderMutex_);

        detected.swap(
            detectedProviderIds_);
    }

    if (detected.empty()) {
        return;
    }

    std::vector<std::string>
        enabledChanges;

    const auto& enabled =
        settingsStore_.Data()
            .providerEnabled;

    for (const auto& id :
         detected) {
        if (providers::IsEnabled(
                enabled,
                id,
                true)) {
            enabledChanges.push_back(
                id);
        }
    }

    if (!enabledChanges.empty()) {
        StartProviderRefresh(
            std::move(
                enabledChanges));
    }
}

bool App::RestoreDefaultSettings() {
    const Settings previous =
        settingsStore_.Data();

    const Settings defaults{};

    // Release the optional binding first. An auxiliary hotkey may have
    // been configured to the default primary binding (Alt + Space) while
    // the primary used another key. Resetting in the opposite order would
    // make RegisterHotKey report a false conflict against our own process.
    if (!RebindAuxiliaryHotkey(
            false,
            defaults.auxiliaryHotkeyModifiers,
            defaults.auxiliaryHotkeyKey)) {
        return false;
    }

    if (!RebindGlobalHotkey(
            defaults.hotkeyModifiers,
            defaults.hotkeyKey)) {

        RebindAuxiliaryHotkey(
            previous.auxiliaryHotkeyEnabled,
            previous.auxiliaryHotkeyModifiers,
            previous.auxiliaryHotkeyKey);
        return false;
    }

    if (!ApplyStartupRegistration(false)) {
        RebindGlobalHotkey(
            previous.hotkeyModifiers,
            previous.hotkeyKey);
        RebindAuxiliaryHotkey(
            previous.auxiliaryHotkeyEnabled,
            previous.auxiliaryHotkeyModifiers,
            previous.auxiliaryHotkeyKey);
        return false;
    }

    if (!settingsStore_.ResetDefaults()) {
        ApplyStartupRegistration(
            previous.startWithWindows);

        RebindGlobalHotkey(
            previous.hotkeyModifiers,
            previous.hotkeyKey);
        RebindAuxiliaryHotkey(
            previous.auxiliaryHotkeyEnabled,
            previous.auxiliaryHotkeyModifiers,
            previous.auxiliaryHotkeyKey);

        return false;
    }

    commandStore_.ReloadProviderCache(
        settingsStore_.Data()
            .providerEnabled);

    {
        std::scoped_lock lock(
            providerMonitorConfigMutex_);

        providerMonitorEnabled_ =
            settingsStore_.Data()
                .providerEnabled;
    }

    if (window_) {
        window_->ApplyAppearance();
        window_->ApplyLanguage();
        window_->ApplyGeneralSettings();
        window_->RefreshResults();
    }

    StartProviderRefresh();

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

bool App::RebindAuxiliaryHotkey(
    bool enabled,
    const std::vector<std::string>& modifiers,
    std::string_view key) {

    if (!enabled) {
        if (auxiliaryHotkeyRegistered_) {
            UnregisterHotKey(
                nullptr,
                kAuxiliaryHotkeyId);
        }

        auxiliaryHotkeyRegistered_ =
            false;
        currentAuxiliaryHotkeyModifiers_ =
            0;
        currentAuxiliaryHotkeyVk_ = 0;
        auxiliaryHotkeyLastError_ =
            ERROR_SUCCESS;
        return true;
    }

    const UINT newModifiers =
        hotkey::ModifiersFromNames(
            modifiers);

    const UINT newVk =
        hotkey::KeyFromName(key);

    if (newVk == 0) {
        auxiliaryHotkeyLastError_ =
            ERROR_INVALID_PARAMETER;
        return false;
    }

    const bool hadOld =
        auxiliaryHotkeyRegistered_;

    const UINT oldModifiers =
        currentAuxiliaryHotkeyModifiers_;

    const UINT oldVk =
        currentAuxiliaryHotkeyVk_;

    if (hadOld) {
        UnregisterHotKey(
            nullptr,
            kAuxiliaryHotkeyId);
        auxiliaryHotkeyRegistered_ =
            false;
    }

    SetLastError(ERROR_SUCCESS);

    if (RegisterHotKey(
            nullptr,
            kAuxiliaryHotkeyId,
            newModifiers,
            newVk)) {

        currentAuxiliaryHotkeyModifiers_ =
            newModifiers;
        currentAuxiliaryHotkeyVk_ =
            newVk;
        auxiliaryHotkeyRegistered_ =
            true;
        auxiliaryHotkeyLastError_ =
            ERROR_SUCCESS;
        return true;
    }

    const DWORD registrationError =
        GetLastError();

    if (hadOld &&
        oldVk != 0 &&
        RegisterHotKey(
            nullptr,
            kAuxiliaryHotkeyId,
            oldModifiers,
            oldVk)) {

        currentAuxiliaryHotkeyModifiers_ =
            oldModifiers;
        currentAuxiliaryHotkeyVk_ =
            oldVk;
        auxiliaryHotkeyRegistered_ =
            true;
    } else {
        currentAuxiliaryHotkeyModifiers_ =
            0;
        currentAuxiliaryHotkeyVk_ = 0;
        auxiliaryHotkeyRegistered_ =
            false;
    }

    auxiliaryHotkeyLastError_ =
        registrationError != ERROR_SUCCESS
            ? registrationError
            : ERROR_HOTKEY_ALREADY_REGISTERED;

    return false;
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

bool App::RepairGlobalHotkey(
    bool forceRebind) {

    const bool primary =
        !forceRebind &&
        hotkeyRegistered_
            ? true
            : RebindGlobalHotkey(
                  settingsStore_.Data()
                      .hotkeyModifiers,
                  settingsStore_.Data()
                      .hotkeyKey);

    bool auxiliary = true;

    if (settingsStore_.Data()
            .auxiliaryHotkeyEnabled) {

        auxiliary =
            !forceRebind &&
            auxiliaryHotkeyRegistered_
                ? true
                : RebindAuxiliaryHotkey(
                      true,
                      settingsStore_.Data()
                          .auxiliaryHotkeyModifiers,
                      settingsStore_.Data()
                          .auxiliaryHotkeyKey);
    } else if (
        auxiliaryHotkeyRegistered_) {

        auxiliary =
            RebindAuxiliaryHotkey(
                false,
                {},
                settingsStore_.Data()
                    .auxiliaryHotkeyKey);
    }

    if (settingsWindow_) {
        settingsWindow_->
            RefreshFromSettings();
    }

    return primary && auxiliary;
}

bool App::SetShowOnStartup(
    bool enabled) {

    if (!settingsStore_
             .SetShowOnStartup(
                 enabled)) {
        return false;
    }

    if (settingsWindow_) {
        settingsWindow_->
            RefreshFromSettings();
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

bool App::SetAuxiliaryHotkeySettings(
    bool enabled,
    std::vector<std::string> modifiers,
    std::string key) {

    const bool previousEnabled =
        settingsStore_.Data()
            .auxiliaryHotkeyEnabled;

    const auto previousModifiers =
        settingsStore_.Data()
            .auxiliaryHotkeyModifiers;

    const auto previousKey =
        settingsStore_.Data()
            .auxiliaryHotkeyKey;

    if (!RebindAuxiliaryHotkey(
            enabled,
            modifiers,
            key)) {
        return false;
    }

    if (!settingsStore_
             .SetAuxiliaryHotkey(
                 enabled,
                 std::move(modifiers),
                 std::move(key))) {

        RebindAuxiliaryHotkey(
            previousEnabled,
            previousModifiers,
            previousKey);

        return false;
    }

    if (settingsWindow_) {
        settingsWindow_->
            RefreshFromSettings();
    }

    return true;
}

bool App::SetClassicBehavior(
    bool wildcardMatching,
    bool numericQuickLaunch,
    std::string numericQuickLaunchOrder,
    bool executeSingleResultImmediately) {

    if (!settingsStore_
             .SetClassicBehavior(
                 wildcardMatching,
                 numericQuickLaunch,
                 std::move(
                     numericQuickLaunchOrder),
                 executeSingleResultImmediately)) {
        return false;
    }

    if (window_) {
        window_->RefreshResults();
    }

    if (settingsWindow_) {
        settingsWindow_->
            RefreshFromSettings();
    }

    return true;
}

bool App::SetProviderEnabled(
    std::string id,
    bool enabled) {

    const std::string providerId =
        id;

    if (!settingsStore_
             .SetProviderEnabled(
                 std::move(id),
                 enabled)) {
        return false;
    }

    commandStore_
        .ReloadProviderCache(
            settingsStore_.Data()
                .providerEnabled);

    {
        std::scoped_lock lock(
            providerMonitorConfigMutex_);

        providerMonitorEnabled_ =
            settingsStore_.Data()
                .providerEnabled;
    }

    if (window_) {
        window_->RefreshResults();
    }

    if (settingsWindow_) {
        settingsWindow_
            ->RefreshFromSettings();
    }

    if (enabled) {
        StartProviderRefresh(
            {providerId});
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
