#include "App.hpp"

#include "../core/EverythingProvider.hpp"
#include "../core/ClassicBehavior.hpp"
#include "../core/ClipboardAction.hpp"
#include "../core/CommandTemplate.hpp"
#include "../core/ShortcutEditorModel.hpp"
#include "../core/HotkeyRegistry.hpp"
#include "../core/LauncherActionPolicy.hpp"
#include "../core/ProviderIds.hpp"
#include "../core/RelevancePolicy.hpp"
#include "../core/ResultMerger.hpp"
#include "../core/RuntimeInput.hpp"
#include "../core/WebAction.hpp"
#include "Version.hpp"
#include "../platform/Hotkey.hpp"
#include "../platform/InstanceIpc.hpp"
#include "../platform/WinClipboard.hpp"
#include "../platform/WinUtil.hpp"
#include "../ui/LauncherWindow.hpp"
#include "../ui/SettingsWindow.hpp"
#include "../ui/ShortcutManagerWindow.hpp"

#include <shellapi.h>
#include <shlobj.h>
#include <shobjidl.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <unordered_map>

namespace altrun {

namespace {

constexpr std::uint64_t
    kUpdateCheckWatchdogMs =
        60ULL * 1000ULL;

constexpr wchar_t
    kUpdateDispatchClass[] =
        L"ALTRunNext.UpdateDispatch";

constexpr UINT_PTR
    kUpdateReconcileTimerId =
        0xA175;

[[nodiscard]] std::string_view
ProviderIdForCommand(
    CommandSource source) {
    switch (source) {
    case CommandSource::StartMenu:
        return providers::kStartMenu;
    case CommandSource::PackagedApp:
        return providers::kPackaged;
    case CommandSource::AppPaths:
        return providers::kAppPaths;
    case CommandSource::Path:
        return providers::kPath;
    case CommandSource::User:
        return "user.commands";
    }

    return "user.commands";
}

[[nodiscard]] std::wstring
CommandDetail(
    const Command& command) {
    std::wstring detail =
        command.target;

    if (!command.arguments.empty()) {
        detail += L"  ";
        detail += command.arguments;
    }

    return detail;
}

std::wstring FormatHotkeyBindingForStartup(
    const HotkeyBinding& binding) {
    std::wstring result;

    const auto append =
        [&](std::wstring_view value) {
            if (!result.empty()) {
                result += L" + ";
            }
            result += value;
        };

    for (const auto& modifier :
         binding.modifiers) {
        if (modifier == "ctrl") {
            append(L"Ctrl");
        } else if (modifier == "alt") {
            append(L"Alt");
        } else if (modifier == "shift") {
            append(L"Shift");
        } else if (modifier == "win") {
            append(L"Win");
        }
    }

    const UINT key =
        hotkey::KeyFromName(
            binding.key);

    if (key != 0) {
        append(
            hotkey::KeyDisplayName(
                key));
    } else {
        append(
            std::wstring(
                binding.key.begin(),
                binding.key.end()));
    }

    return result;
}

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

App::App(
    HINSTANCE instance,
    std::wstring startupHealthEvent,
    std::vector<std::wstring>
        startupShortcutPaths)
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
          baseDirectory_ / "dict"),
      startupHealthEvent_(
          std::move(
              startupHealthEvent)),
      startupShortcutPaths_(
          std::move(
              startupShortcutPaths)),
      suppressStartupPresentation_(
          !startupHealthEvent_.empty()) {}

App::~App() {
    StopUpdateReconcileTimer();
    ++updateGeneration_;

    if (updateThread_.joinable()) {
        updateThread_.request_stop();
        updateThread_.join();
    }

    DestroyUpdateDispatchWindow();

    StopManagedEverythingLifecycle();

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

    if (shortcutManagerHotkeyRegistered_) {
        UnregisterHotKey(
            nullptr,
            kShortcutManagerHotkeyId);
        shortcutManagerHotkeyRegistered_ =
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

    if (providers::IsEnabled(
            settingsStore_.Data()
                .providerEnabled,
            providers::
                kEverythingFilesystem,
            false)) {
        everythingProvider_ =
            std::make_unique<
                EverythingProvider>();
    }

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
        if (!startupShortcutPaths_.empty()) {
            if (ForwardShortcutRequestsToExistingInstance()) {
                return 0;
            }

            MessageBoxW(
                nullptr,
                settingsStore_.Data().language == Language::ZhCN
                    ? L"ALTRun Next 已经在运行，但无法把“发送到”请求交给现有实例。"
                    : L"ALTRun Next is already running, but the Send To request could not be forwarded to the existing instance.",
                L"ALTRun Next",
                MB_ICONWARNING | MB_OK);
            return 1;
        }

        MessageBoxW(
            nullptr,
            settingsStore_.Data().language == Language::ZhCN
                ? L"ALTRun Next 已经在运行。\n\n请检查系统托盘，避免多个实例同时抢占全局热键。"
                : L"ALTRun Next is already running.\n\nCheck the system tray. Multiple instances are blocked to prevent global-hotkey conflicts.",
            L"ALTRun Next",
            MB_ICONINFORMATION | MB_OK);
        return 0;
    }

    // Update notifications use a message-only HWND so nested Windows message
    // loops dispatch them normally. A thread-message fallback remains only
    // for the exceptional case where this invisible dispatcher cannot exist.
    CreateUpdateDispatchWindow();

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

    if (settingsStore_.Data()
            .addToSendToMenu) {
        ApplySendToRegistration(true);
    }

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

    // A post-update health signal means the new executable loaded its data
    // and created the real desktop window successfully. Signal before any
    // optional warning dialog can make the updater mistake user think-time
    // for a failed startup.
    SignalStartupHealthEvent();

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

    const auto shortcutManagerBinding =
        EffectiveHotkeyBinding(
            settingsStore_.Data()
                .hotkeyBindings,
            hotkey_actions::
                kOpenShortcutManager);

    if (!RebindShortcutManagerHotkey(
            shortcutManagerBinding.enabled,
            shortcutManagerBinding.modifiers,
            shortcutManagerBinding.key)) {
        MessageBoxW(
            nullptr,
            settingsStore_.Data().language ==
                    Language::ZhCN
                ? L"“打开快捷项管理”全局快捷键注册失败。请检查该组合键是否已被其他程序占用。"
                : L"The global Shortcut Manager hotkey could not be registered. Check whether another application already uses the binding.",
            L"ALTRun Next",
            MB_ICONWARNING | MB_OK);
    }

    if (providers::IsEnabled(
            settingsStore_.Data()
                .providerEnabled,
            providers::
                kEverythingFilesystem,
            false)) {
        StartEverythingBootstrap(false);
    }

    if (!startupShortcutPaths_.empty()) {
        for (const auto& path :
             startupShortcutPaths_) {
            window_->
                QueueNewShortcutForPath(
                    path);
        }
    } else if (
        !suppressStartupPresentation_) {
        switch (settingsStore_.Data()
                    .startupBehavior) {
        case StartupBehavior::ShowLauncher:
            window_->Show();
            break;
        case StartupBehavior::Notification: {
            const auto binding =
                EffectiveHotkeyBinding(
                    settingsStore_.Data()
                        .hotkeyBindings,
                    hotkey_actions::kActivate);
            window_->ShowStartupNotification(
                FormatHotkeyBindingForStartup(
                    binding));
            break;
        }
        case StartupBehavior::Silent:
        default:
            break;
        }
    }

    // Cached provider results are already searchable. Refresh automatic
    // discovery off the startup path, then keep lightweight provider
    // fingerprints under observation for source-specific updates.
    StartProviderRefresh();
    StartProviderMonitor();

    if (settingsStore_.Data()
            .autoCheckUpdates) {
        StartUpdateCheck(false);
    }

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (msg.message ==
                kDynamicQueryMessage &&
            msg.hwnd == nullptr) {
            HandleDynamicQueryCompleted();
            continue;
        }

        if (msg.message ==
                kEverythingBootstrapMessage &&
            msg.hwnd == nullptr) {
            HandleEverythingBootstrapCompleted(
                static_cast<std::uint64_t>(
                    msg.wParam));
            continue;
        }

        if (msg.message ==
                kUpdateStatusMessage &&
            msg.hwnd == nullptr) {
            HandleUpdateStatusMessage(
                static_cast<std::uint64_t>(
                    msg.wParam));
            continue;
        }

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
            updateDispatchWindow_ == nullptr &&
            updateReconcileTimer_ != 0 &&
            msg.wParam ==
                updateReconcileTimer_) {

            HandleUpdateStatusMessage(
                updateGeneration_.load());
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
            msg.hwnd == nullptr) {
            if (msg.wParam ==
                    static_cast<WPARAM>(
                        kShortcutManagerHotkeyId)) {
                ShowShortcutManager();
                continue;
            }

            if (msg.wParam ==
                    static_cast<WPARAM>(
                        kGlobalHotkeyId) ||
                msg.wParam ==
                    static_cast<WPARAM>(
                        kAuxiliaryHotkeyId)) {

                if (window_) {
                    // Capture the foreground Windows context before ALTRun Next
                    // takes focus. Hiding an already-visible launcher must not
                    // replace the session snapshot with ALTRun Next itself.
                    if (!window_->IsVisible()) {
                        CaptureActivationContext();
                    }

                    window_->Toggle();
                }
                continue;
            }
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

    if (shortcutManagerWindow_) {
        shortcutManagerWindow_->Refresh();
    }
}

bool App::HasStaticQueryContinuation(
    std::wstring_view query) const noexcept {

    return classic_behavior::
        HasStrongCommandContinuation(
            commandStore_.Commands(),
            query);
}

std::vector<LauncherResult> App::Search(
    std::wstring_view query,
    std::size_t limit) const {

    const auto& sourceCommands =
        commandStore_.Commands();

    const std::wstring_view
        contextFolder =
            activationContext_
                .CurrentFilesystemFolder();

    // Keep a same-order working set so {folder} can be resolved for search,
    // presentation and {query} web aliases without mutating persisted
    // commands. Contextual commands are intentionally absent when there is
    // no real filesystem folder to substitute.
    std::vector<Command>
        searchableCommands;
    std::vector<std::size_t>
        sourceIndices;

    searchableCommands.reserve(
        sourceCommands.size());
    sourceIndices.reserve(
        sourceCommands.size());

    for (std::size_t index = 0;
         index < sourceCommands.size();
         ++index) {
        const auto& source =
            sourceCommands[index];

        if (source.source ==
                CommandSource::User &&
            UsesFolderTemplate(
                source)) {
            if (contextFolder.empty()) {
                continue;
            }

            searchableCommands.push_back(
                ResolveFolderTemplate(
                    source,
                    contextFolder));
        } else {
            searchableCommands.push_back(
                source);
        }

        sourceIndices.push_back(index);
    }

    const auto matches =
        searchEngine_.Search(
            searchableCommands,
            usageStore_.Data(),
            query,
            limit,
            true,
            settingsStore_.Data()
                .pinyinSearch);

    std::vector<LauncherResult> results;
    results.reserve(matches.size());

    for (const auto& match : matches) {
        if (match.commandIndex >=
            searchableCommands.size()) {
            continue;
        }

        const auto& command =
            searchableCommands[
                match.commandIndex];

        const std::size_t
            sourceIndex =
                sourceIndices[
                    match.commandIndex];

        LauncherResult result;
        result.id = command.id;
        result.providerId =
            std::string(
                ProviderIdForCommand(
                    command.source));
        result.kind =
            command.source == CommandSource::User
                ? ResultKind::UserCommand
                : ResultKind::Application;
        result.title =
            command.keyword.empty()
                ? command.title
                : command.keyword;
        result.subtitle = command.title;
        result.target = command.target;
        result.iconSource =
            command.icon.empty() ||
            command.icon == L"auto"
                ? command.target
                : command.icon;
        result.detail =
            CommandDetail(command);
        result.score = match.score;
        result.relevanceMatch =
            match.relevanceMatch;
        result.surfaceClass =
            command.surfaceClass;
        result.usageScore =
            match.usageScore;
        result.pinned =
            command.pinned;
        result.action.kind =
            LauncherActionKind::
                ExecuteCommand;
        result.action.commandIndex =
            sourceIndex;

        results.push_back(
            std::move(result));
    }

    auto inputActions =
        BuildRuntimeInputActionResults(
            searchableCommands,
            query,
            limit);

    auto webActions =
        BuildWebActionResults(
            searchableCommands,
            query,
            limit);

    // Runtime/Web actions receive the context-resolved working set, so remap
    // their working-set index back to the persisted CommandStore index.
    for (auto& action : inputActions) {
        if (action.action.commandIndex ==
            static_cast<std::size_t>(-1)) {
            continue;
        }

        if (action.action.commandIndex >=
            sourceIndices.size()) {
            action.action.commandIndex =
                static_cast<std::size_t>(-1);
            continue;
        }

        action.action.commandIndex =
            sourceIndices[
                action.action.commandIndex];
    }

    // WebAction receives the context-resolved working set, so remap its
    // working-set index back to the persisted CommandStore index before the
    // action reaches execution/usage tracking.
    for (auto& action : webActions) {
        if (action.action.commandIndex ==
            static_cast<std::size_t>(-1)) {
            continue;
        }

        if (action.action.commandIndex >=
            sourceIndices.size()) {
            action.action.commandIndex =
                static_cast<std::size_t>(-1);
            continue;
        }

        action.action.commandIndex =
            sourceIndices[
                action.action.commandIndex];
    }

    auto clipboardActions =
        BuildClipboardActionResults(
            query,
            limit,
            Text(
                TextId::
                    CopyTextAction));

    if (inputActions.empty() &&
        webActions.empty() &&
        clipboardActions.empty()) {
        return results;
    }

    const auto removeBaseCommand =
        [&](const LauncherResult& action) {
            if (action.action.commandIndex ==
                static_cast<std::size_t>(-1)) {
                return;
            }

            results.erase(
                std::remove_if(
                    results.begin(),
                    results.end(),
                    [&](const LauncherResult& result) {
                        return result.action.kind ==
                                LauncherActionKind::
                                    ExecuteCommand &&
                            result.action.commandIndex ==
                                action.action.commandIndex;
                    }),
                results.end());
        };

    for (const auto& action :
         inputActions) {
        removeBaseCommand(action);
    }

    for (const auto& action : webActions) {
        removeBaseCommand(action);
    }

    std::vector<LauncherResult>
        runtimeActions;

    runtimeActions.reserve(
        inputActions.size() +
        webActions.size() +
        clipboardActions.size());

    for (auto& action :
         inputActions) {
        runtimeActions.push_back(
            std::move(action));
    }

    for (auto& action : webActions) {
        runtimeActions.push_back(
            std::move(action));
    }

    for (auto& action :
         clipboardActions) {
        runtimeActions.push_back(
            std::move(action));
    }

    return MergeLauncherResultsRanked(
        results,
        runtimeActions,
        limit);
}

bool App::DynamicSearchEnabled()
    const {
    return everythingProvider_ &&
        providers::IsEnabled(
            settingsStore_.Data()
                .providerEnabled,
            providers::
                kEverythingFilesystem,
            false);
}

void App::BeginDynamicSearch(
    std::uint64_t generation,
    std::wstring query,
    std::size_t limit) {
    if (!DynamicSearchEnabled() ||
        !relevance::
            ShouldRunDynamicFilesystemQuery(
                query)) {
        return;
    }

    DynamicQueryRequest request;
    request.generation = generation;
    request.query = std::move(query);
    request.limit = limit;

    everythingProvider_->QueryAsync(
        std::move(request),
        [this](
            DynamicQueryResponse response) {
            {
                std::scoped_lock lock(
                    dynamicQueryMutex_);
                dynamicQueryPending_ =
                    std::move(response);
            }

            if (uiThreadId_ != 0) {
                PostThreadMessageW(
                    uiThreadId_,
                    kDynamicQueryMessage,
                    0,
                    0);
            }
        });
}

std::vector<ProviderStatus>
App::ProviderStatuses() const {
    return commandStore_
        .ProviderStatuses(
            settingsStore_.Data()
                .providerEnabled);
}

EverythingIpcStatusSnapshot
App::EverythingStatus() const {
    return everythingProvider_
        ? everythingProvider_->Status()
        : EverythingIpcStatusSnapshot{};
}

win::EverythingBootstrapSnapshot
App::EverythingBootstrapStatus() const {
    std::scoped_lock lock(
        everythingBootstrapMutex_);

    return everythingBootstrapStatus_;
}

bool App::StartEverythingBootstrap(
    bool allowDownload) {
    if (!providers::IsEnabled(
            settingsStore_.Data()
                .providerEnabled,
            providers::
                kEverythingFilesystem,
            false)) {
        return false;
    }

    {
        std::scoped_lock lock(
            everythingBootstrapMutex_);

        if (everythingBootstrapStatus_
                .running) {
            return false;
        }
    }

    if (everythingBootstrapThread_
            .joinable()) {
        everythingBootstrapThread_
            .join();
    }

    {
        std::scoped_lock lock(
            everythingBootstrapMutex_);

        everythingBootstrapStatus_ = {};
        everythingBootstrapStatus_.stage =
            win::EverythingBootstrapStage::
                Discovering;
        everythingBootstrapStatus_.running =
            true;
    }

    const auto dataDirectory =
        dataDirectory_;
    const DWORD targetThread =
        uiThreadId_;
    const std::uint64_t generation =
        ++everythingBootstrapGeneration_;

    everythingBootstrapThread_ =
        std::jthread(
            [this,
             dataDirectory,
             allowDownload,
             targetThread,
             generation](
                std::stop_token stopToken) {
                const auto progress =
                    [this](
                        const win::
                            EverythingBootstrapSnapshot&
                                snapshot) {
                        std::scoped_lock lock(
                            everythingBootstrapMutex_);
                        everythingBootstrapStatus_ =
                            snapshot;
                    };

                const auto result =
                    win::RunEverythingBootstrap(
                        dataDirectory,
                        allowDownload,
                        progress,
                        stopToken);

                {
                    std::scoped_lock lock(
                        everythingBootstrapMutex_);
                    everythingBootstrapStatus_ =
                        result;
                }

                if (targetThread != 0) {
                    PostThreadMessageW(
                        targetThread,
                        kEverythingBootstrapMessage,
                        static_cast<WPARAM>(
                            generation),
                        0);
                }
            });

    if (settingsWindow_) {
        settingsWindow_->
            OnDynamicProviderStatusChanged();
    }

    return true;
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
    if (shortcutManagerWindow_) {
        if (createdId) {
            shortcutManagerWindow_->Refresh(
                *createdId);
        } else {
            shortcutManagerWindow_->Refresh();
        }
    }
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
    if (shortcutManagerWindow_) {
        shortcutManagerWindow_->Refresh(id);
    }
    return true;
}

bool App::DeleteUserCommand(
    std::wstring_view id) {

    if (!commandStore_.DeleteUserCommand(id)) {
        return false;
    }

    if (window_) window_->RefreshResults();
    if (shortcutManagerWindow_) {
        shortcutManagerWindow_->Refresh();
    }
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
    if (shortcutManagerWindow_) {
        shortcutManagerWindow_->Refresh(id);
    }
    return true;
}


bool App::ApplyUserCommandPathUpdates(
    const std::vector<UserCommandPathUpdate>& updates) {
    if (!commandStore_
             .ApplyUserCommandPathUpdates(
                 updates)) {
        return false;
    }

    if (window_) {
        window_->RefreshResults();
    }

    if (shortcutManagerWindow_) {
        shortcutManagerWindow_->Refresh();
    }

    return true;
}

bool App::TestCommand(
    const Command& command,
    std::wstring_view runtimeInput) {
    return LaunchCommand(
        command,
        false,
        runtimeInput);
}

bool App::ImportUserCommands(
    const std::filesystem::path& path,
    std::size_t* imported,
    std::size_t* skipped) {

    if (!commandStore_.ImportUserCommands(
            path,
            imported,
            skipped)) {
        return false;
    }

    if (window_) {
        window_->RefreshResults();
    }

    if (shortcutManagerWindow_) {
        shortcutManagerWindow_->Refresh();
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

void App::HandleDynamicQueryCompleted() {
    std::optional<DynamicQueryResponse>
        response;

    {
        std::scoped_lock lock(
            dynamicQueryMutex_);

        if (dynamicQueryPending_) {
            response =
                std::move(
                    dynamicQueryPending_);
            dynamicQueryPending_.reset();
        }
    }

    if (!response) {
        return;
    }

    if (window_) {
        window_->ApplyDynamicResults(
            response->generation,
            std::move(
                response->results));
    }

    if (settingsWindow_) {
        settingsWindow_->
            OnDynamicProviderStatusChanged();
    }
}

void App::StopManagedEverythingLifecycle() {
    // Invalidate any already-posted bootstrap completion before waiting for
    // the worker. A late UI message from the old generation must never
    // recreate the provider or restart the managed client after disable.
    ++everythingBootstrapGeneration_;

    if (everythingBootstrapThread_
            .joinable()) {
        everythingBootstrapThread_
            .request_stop();
        everythingBootstrapThread_
            .join();
    }

    // Stop query work before shutting down the managed IPC owner.
    everythingProvider_.reset();

    {
        std::scoped_lock lock(
            dynamicQueryMutex_);
        dynamicQueryPending_.reset();
    }

    // Best effort and ownership-safe: this API refuses to issue -exit unless
    // the active default Everything IPC window belongs to our exact managed
    // executable path. Normal application exit leaves the service policy
    // unchanged; disabling the Everything provider separately stops and
    // disables an ALTRun-owned service through SetProviderEnabled().
    (void)win::StopManagedEverything(
        dataDirectory_);

    {
        std::scoped_lock lock(
            everythingBootstrapMutex_);
        everythingBootstrapStatus_ = {};
    }
}

void App::HandleEverythingBootstrapCompleted(
    std::uint64_t generation) {
    if (generation !=
        everythingBootstrapGeneration_) {
        return;
    }

    if (everythingBootstrapThread_
            .joinable()) {
        everythingBootstrapThread_
            .join();
    }

    const bool everythingEnabled =
        providers::IsEnabled(
            settingsStore_.Data()
                .providerEnabled,
            providers::
                kEverythingFilesystem,
            false);

    const auto bootstrap =
        EverythingBootstrapStatus();

    if (everythingEnabled &&
        !everythingProvider_) {
        everythingProvider_ =
            std::make_unique<
                EverythingProvider>();
    }

    if (window_) {
        window_->RefreshResults();
    }

    if (settingsWindow_) {
        settingsWindow_->
            OnDynamicProviderStatusChanged();
    }

    if (everythingEnabled &&
        bootstrap.failure ==
            win::EverythingBootstrapFailure::
                Cancelled) {
        StartEverythingBootstrap(false);
    }
}

LRESULT CALLBACK
App::UpdateDispatchWindowProc(
    HWND hwnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam) {
    App* self =
        reinterpret_cast<App*>(
            GetWindowLongPtrW(
                hwnd,
                GWLP_USERDATA));

    if (message == WM_NCCREATE) {
        const auto* create =
            reinterpret_cast<
                CREATESTRUCTW*>(
                    lParam);

        self =
            static_cast<App*>(
                create->lpCreateParams);

        SetWindowLongPtrW(
            hwnd,
            GWLP_USERDATA,
            reinterpret_cast<LONG_PTR>(
                self));
    }

    if (self) {
        if (message ==
                kUpdateStatusMessage) {
            self->HandleUpdateStatusMessage(
                static_cast<std::uint64_t>(
                    wParam));
            return 0;
        }

        if (message == WM_TIMER &&
            wParam ==
                kUpdateReconcileTimerId) {
            self->HandleUpdateStatusMessage(
                self->updateGeneration_
                    .load());
            return 0;
        }

        if (message == WM_NCDESTROY &&
            self->updateDispatchWindow_ ==
                hwnd) {
            self->updateDispatchWindow_ =
                nullptr;
        }
    }

    return DefWindowProcW(
        hwnd,
        message,
        wParam,
        lParam);
}

bool App::CreateUpdateDispatchWindow() {
    if (updateDispatchWindow_ &&
        IsWindow(
            updateDispatchWindow_)) {
        return true;
    }

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.hInstance = instance_;
    wc.lpfnWndProc =
        UpdateDispatchWindowProc;
    wc.lpszClassName =
        kUpdateDispatchClass;

    if (!RegisterClassExW(&wc) &&
        GetLastError() !=
            ERROR_CLASS_ALREADY_EXISTS) {
        return false;
    }

    updateDispatchWindow_ =
        CreateWindowExW(
            0,
            kUpdateDispatchClass,
            L"",
            0,
            0,
            0,
            0,
            0,
            HWND_MESSAGE,
            nullptr,
            instance_,
            this);

    return updateDispatchWindow_ !=
        nullptr;
}

void App::DestroyUpdateDispatchWindow() {
    StopUpdateReconcileTimer();

    if (updateDispatchWindow_ &&
        IsWindow(
            updateDispatchWindow_)) {
        const HWND window =
            updateDispatchWindow_;
        updateDispatchWindow_ =
            nullptr;
        DestroyWindow(window);
    } else {
        updateDispatchWindow_ =
            nullptr;
    }
}

void App::PostUpdateStatusNotification(
    std::uint64_t generation) {
    const HWND dispatcher =
        updateDispatchWindow_;

    if (dispatcher &&
        IsWindow(dispatcher) &&
        PostMessageW(
            dispatcher,
            kUpdateStatusMessage,
            static_cast<WPARAM>(
                generation),
            0)) {
        return;
    }

    // Last-resort fallback for dispatcher creation/post failure. Normal
    // update delivery never depends on this lossy thread-message path.
    if (uiThreadId_ != 0) {
        PostThreadMessageW(
            uiThreadId_,
            kUpdateStatusMessage,
            static_cast<WPARAM>(
                generation),
            0);
    }
}

void App::StartUpdateReconcileTimer() {
    if (updateReconcileTimer_ != 0) {
        return;
    }

    // Window-targeted WM_TIMER survives nested Windows message loops that can
    // consume hwnd==nullptr thread messages before App::Run() sees them.
    if (updateDispatchWindow_ &&
        IsWindow(
            updateDispatchWindow_)) {
        updateReconcileTimer_ =
            SetTimer(
                updateDispatchWindow_,
                kUpdateReconcileTimerId,
                250,
                nullptr);
        return;
    }

    // Exceptional fallback only if the message-only window could not exist.
    updateReconcileTimer_ =
        SetTimer(
            nullptr,
            0,
            250,
            nullptr);
}

void App::StopUpdateReconcileTimer() {
    if (updateReconcileTimer_ == 0) {
        return;
    }

    if (updateDispatchWindow_ &&
        IsWindow(
            updateDispatchWindow_)) {
        KillTimer(
            updateDispatchWindow_,
            kUpdateReconcileTimerId);
    } else {
        KillTimer(
            nullptr,
            updateReconcileTimer_);
    }

    updateReconcileTimer_ = 0;
}

void App::HandleUpdateStatusMessage(
    std::uint64_t generation) {
    const std::uint64_t currentGeneration =
        updateGeneration_.load();

    if (generation !=
        currentGeneration) {
        if (!updateWorkerRunning_.load()) {
            if (updateThread_.joinable()) {
                updateThread_.join();
            }

            updateWorkerStartedTick_.store(
                0);
            StopUpdateReconcileTimer();
        }

        if (settingsWindow_) {
            settingsWindow_->
                OnUpdateStatusChanged();
        }
        return;
    }

    auto status =
        UpdateStatus();
    bool workerRunning =
        updateWorkerRunning_.load();

    // Reconciliation/repaint can only mirror App state. If synchronous
    // WinHTTP itself stops progressing, bound Checking and invalidate the old
    // generation before requesting cancellation.
    if (status.stage ==
            win::UpdateStage::Checking &&
        status.running &&
        workerRunning) {
        const std::uint64_t started =
            updateWorkerStartedTick_.load();
        const std::uint64_t now =
            static_cast<std::uint64_t>(
                GetTickCount64());

        if (started != 0 &&
            now >= started &&
            now - started >=
                kUpdateCheckWatchdogMs) {
            bool committedTimeout =
                false;

            {
                std::scoped_lock lock(
                    updateMutex_);

                if (generation ==
                        updateGeneration_
                            .load() &&
                    updateStatus_.stage ==
                        win::UpdateStage::
                            Checking &&
                    updateStatus_.running &&
                    updateWorkerRunning_
                        .load()) {
                    ++updateGeneration_;

                    updateStatus_.stage =
                        win::UpdateStage::
                            Failed;
                    updateStatus_.failure =
                        win::UpdateFailure::
                            CheckTimedOut;
                    updateStatus_.nativeError =
                        ERROR_TIMEOUT;
                    updateStatus_.running =
                        false;
                    updateManifest_.reset();
                    updateInstallWhenReady_ =
                        false;
                    committedTimeout = true;
                }
            }

            if (committedTimeout) {
                if (updateThread_.joinable()) {
                    updateThread_.request_stop();
                }

                if (settingsWindow_) {
                    settingsWindow_->
                        OnUpdateStatusChanged();
                }

                // Keep the timer until the cancelled worker has unwound; the
                // stale-generation path then joins it and stops reconciliation.
                return;
            }
        }
    }

    status = UpdateStatus();
    workerRunning =
        updateWorkerRunning_.load();

    if (!status.running &&
        !workerRunning) {
        if (updateThread_.joinable()) {
            updateThread_.join();
        }

        updateWorkerStartedTick_.store(
            0);
        StopUpdateReconcileTimer();
    }

    bool beginPreparedUpdate = false;

    {
        std::scoped_lock lock(
            updateMutex_);

        if (updateStatus_.stage ==
                win::UpdateStage::
                    ReadyToInstall &&
            updateInstallWhenReady_) {
            // Worker-side writes use the same mutex; consume the handoff flag
            // atomically with the stage check instead of racing from the UI.
            updateInstallWhenReady_ =
                false;
            beginPreparedUpdate = true;
        }
    }

    if (settingsWindow_) {
        settingsWindow_->
            OnUpdateStatusChanged();
    }

    if (beginPreparedUpdate) {
        BeginPreparedUpdate();
    }
}

bool App::BeginPreparedUpdate() {
    win::UpdateSnapshot snapshot;

    {
        std::scoped_lock lock(
            updateMutex_);
        snapshot = updateStatus_;
        updateStatus_.stage =
            win::UpdateStage::Applying;
        updateStatus_.running = false;
    }

    if (settingsWindow_) {
        settingsWindow_->
            OnUpdateStatusChanged();
    }

    std::uint32_t nativeError = 0;

    if (!win::LaunchPreparedUpdate(
            baseDirectory_,
            dataDirectory_,
            snapshot,
            kVersion,
            static_cast<std::uint32_t>(
                GetCurrentProcessId()),
            nativeError)) {
        {
            std::scoped_lock lock(
                updateMutex_);
            updateStatus_.stage =
                win::UpdateStage::Failed;
            updateStatus_.failure =
                nativeError ==
                        ERROR_FILE_NOT_FOUND
                    ? win::UpdateFailure::
                          UpdaterMissing
                    : win::UpdateFailure::
                          LaunchUpdaterFailed;
            updateStatus_.nativeError =
                nativeError;
            updateStatus_.running = false;
        }

        if (settingsWindow_) {
            settingsWindow_->
                OnUpdateStatusChanged();
        }

        return false;
    }

    PostQuitMessage(0);
    return true;
}

void App::SignalStartupHealthEvent() {
    if (startupHealthEvent_.empty()) {
        return;
    }

    HANDLE event =
        OpenEventW(
            EVENT_MODIFY_STATE,
            FALSE,
            startupHealthEvent_
                .c_str());

    if (event) {
        SetEvent(event);
        CloseHandle(event);
    }

    startupHealthEvent_.clear();
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

    const auto previousShortcutManager =
        EffectiveHotkeyBinding(
            previous.hotkeyBindings,
            hotkey_actions::
                kOpenShortcutManager);
    const auto defaultShortcutManager =
        EffectiveHotkeyBinding(
            defaults.hotkeyBindings,
            hotkey_actions::
                kOpenShortcutManager);

    // Release optional global bindings first so old custom chords cannot
    // collide with default chords owned by this process during reset.
    if (!RebindShortcutManagerHotkey(
            false,
            defaultShortcutManager.modifiers,
            defaultShortcutManager.key) ||
        !RebindAuxiliaryHotkey(
            false,
            defaults.auxiliaryHotkeyModifiers,
            defaults.auxiliaryHotkeyKey)) {
        return false;
    }

    const auto rollbackHotkeys =
        [&]() {
            RebindGlobalHotkey(
                previous.hotkeyModifiers,
                previous.hotkeyKey);
            RebindAuxiliaryHotkey(
                previous.auxiliaryHotkeyEnabled,
                previous.auxiliaryHotkeyModifiers,
                previous.auxiliaryHotkeyKey);
            RebindShortcutManagerHotkey(
                previousShortcutManager.enabled,
                previousShortcutManager.modifiers,
                previousShortcutManager.key);
        };

    if (!RebindGlobalHotkey(
            defaults.hotkeyModifiers,
            defaults.hotkeyKey)) {
        rollbackHotkeys();
        return false;
    }

    if (!RebindShortcutManagerHotkey(
            defaultShortcutManager.enabled,
            defaultShortcutManager.modifiers,
            defaultShortcutManager.key)) {
        rollbackHotkeys();
        return false;
    }

    if (!ApplySendToRegistration(false)) {
        rollbackHotkeys();
        return false;
    }

    if (!ApplyStartupRegistration(false)) {
        ApplySendToRegistration(
            previous.addToSendToMenu);
        rollbackHotkeys();
        return false;
    }

    if (!settingsStore_.ResetDefaults()) {
        ApplyStartupRegistration(
            previous.startWithWindows);
        ApplySendToRegistration(
            previous.addToSendToMenu);
        rollbackHotkeys();
        return false;
    }

    if (previous.updateChannel !=
        settingsStore_.Data()
            .updateChannel) {
        InvalidateUpdateCheckForChannelChange();
    }

    commandStore_.ReloadProviderCache(
        settingsStore_.Data()
            .providerEnabled);

    everythingProvider_.reset();

    {
        std::scoped_lock lock(
            dynamicQueryMutex_);
        dynamicQueryPending_.reset();
    }

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
    if (shortcutManagerWindow_) {
        shortcutManagerWindow_->ApplyLanguage();
    }
}

bool App::SetShowResultIcons(
    bool enabled) {
    if (!settingsStore_.SetShowResultIcons(
            enabled)) {
        return false;
    }

    if (window_) {
        window_->
            ApplyResultIconPreference();
    }

    if (settingsWindow_) {
        settingsWindow_->
            RefreshFromSettings();
    }

    return true;
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

bool App::RebindShortcutManagerHotkey(
    bool enabled,
    const std::vector<std::string>& modifiers,
    std::string_view key) {

    if (!enabled) {
        if (shortcutManagerHotkeyRegistered_) {
            UnregisterHotKey(
                nullptr,
                kShortcutManagerHotkeyId);
        }

        shortcutManagerHotkeyRegistered_ = false;
        currentShortcutManagerHotkeyModifiers_ = 0;
        currentShortcutManagerHotkeyVk_ = 0;
        shortcutManagerHotkeyLastError_ =
            ERROR_SUCCESS;
        return true;
    }

    const UINT newModifiers =
        hotkey::ModifiersFromNames(
            modifiers);
    const UINT newVk =
        hotkey::KeyFromName(key);

    constexpr UINT kModifierMask =
        MOD_ALT | MOD_CONTROL |
        MOD_SHIFT | MOD_WIN;

    if (newVk == 0 ||
        (newModifiers &
         kModifierMask) == 0) {
        shortcutManagerHotkeyLastError_ =
            ERROR_INVALID_PARAMETER;
        return false;
    }

    const bool hadOld =
        shortcutManagerHotkeyRegistered_;
    const UINT oldModifiers =
        currentShortcutManagerHotkeyModifiers_;
    const UINT oldVk =
        currentShortcutManagerHotkeyVk_;

    if (hadOld) {
        UnregisterHotKey(
            nullptr,
            kShortcutManagerHotkeyId);
        shortcutManagerHotkeyRegistered_ =
            false;
    }

    SetLastError(ERROR_SUCCESS);

    if (RegisterHotKey(
            nullptr,
            kShortcutManagerHotkeyId,
            newModifiers,
            newVk)) {
        currentShortcutManagerHotkeyModifiers_ =
            newModifiers;
        currentShortcutManagerHotkeyVk_ =
            newVk;
        shortcutManagerHotkeyRegistered_ =
            true;
        shortcutManagerHotkeyLastError_ =
            ERROR_SUCCESS;
        return true;
    }

    const DWORD registrationError =
        GetLastError();

    if (hadOld &&
        oldVk != 0 &&
        RegisterHotKey(
            nullptr,
            kShortcutManagerHotkeyId,
            oldModifiers,
            oldVk)) {
        currentShortcutManagerHotkeyModifiers_ =
            oldModifiers;
        currentShortcutManagerHotkeyVk_ =
            oldVk;
        shortcutManagerHotkeyRegistered_ =
            true;
    } else {
        currentShortcutManagerHotkeyModifiers_ =
            0;
        currentShortcutManagerHotkeyVk_ = 0;
        shortcutManagerHotkeyRegistered_ =
            false;
    }

    shortcutManagerHotkeyLastError_ =
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

bool App::SetStartupBehavior(
    StartupBehavior behavior) {

    if (!settingsStore_
             .SetStartupBehavior(
                 behavior)) {
        return false;
    }

    if (settingsWindow_) {
        settingsWindow_->
            RefreshFromSettings();
    }

    return true;
}

bool App::SetShowTrayIcon(
    bool enabled) {

    if (!settingsStore_
             .SetShowTrayIcon(
                 enabled)) {
        return false;
    }

    if (window_) {
        window_->ApplyGeneralSettings();
    }

    if (settingsWindow_) {
        settingsWindow_->
            RefreshFromSettings();
    }

    return true;
}

bool App::SetAddToSendToMenu(
    bool enabled) {

    const bool previous =
        settingsStore_.Data()
            .addToSendToMenu;

    if (!ApplySendToRegistration(
            enabled)) {
        return false;
    }

    if (!settingsStore_
             .SetAddToSendToMenu(
                 enabled)) {
        ApplySendToRegistration(
            previous);
        return false;
    }

    if (settingsWindow_) {
        settingsWindow_->
            RefreshFromSettings();
    }

    return true;
}

bool App::SetPopupMonitor(
    std::string popupMonitor) {

    if (!settingsStore_
             .SetPopupMonitor(
                 std::move(
                     popupMonitor))) {
        return false;
    }

    if (window_) {
        window_->ApplyGeneralSettings();
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
    return SetHotkeyBinding(
        std::string(
            hotkey_actions::kActivate),
        HotkeyBinding{
            true,
            std::move(modifiers),
            std::move(key)});
}

bool App::SetAuxiliaryHotkeySettings(
    bool enabled,
    std::vector<std::string> modifiers,
    std::string key) {
    return SetHotkeyBinding(
        std::string(
            hotkey_actions::
                kActivateSecondary),
        HotkeyBinding{
            enabled,
            std::move(modifiers),
            std::move(key)});
}

bool App::SetHotkeyBinding(
    std::string actionId,
    HotkeyBinding binding) {
    CanonicalizeHotkeyBinding(binding);

    const auto* action =
        FindHotkeyAction(actionId);

    if (!action ||
        !ValidateHotkeyBinding(
            actionId,
            binding) ||
        FindHotkeyConflict(
            settingsStore_.Data()
                .hotkeyBindings,
            actionId,
            binding)) {
        SetLastError(
            ERROR_INVALID_PARAMETER);
        return false;
    }

    const auto previous =
        EffectiveHotkeyBinding(
            settingsStore_.Data()
                .hotkeyBindings,
            actionId);

    bool rebound = true;

    if (actionId ==
        hotkey_actions::kActivate) {
        rebound =
            RebindGlobalHotkey(
                binding.modifiers,
                binding.key);
    } else if (
        actionId ==
        hotkey_actions::
            kActivateSecondary) {
        rebound =
            RebindAuxiliaryHotkey(
                binding.enabled,
                binding.modifiers,
                binding.key);
    } else if (
        actionId ==
        hotkey_actions::
            kOpenShortcutManager) {
        rebound =
            RebindShortcutManagerHotkey(
                binding.enabled,
                binding.modifiers,
                binding.key);
    }

    if (!rebound) {
        return false;
    }

    if (!settingsStore_
             .SetHotkeyBinding(
                 actionId,
                 binding)) {
        if (actionId ==
            hotkey_actions::kActivate) {
            RebindGlobalHotkey(
                previous.modifiers,
                previous.key);
        } else if (
            actionId ==
            hotkey_actions::
                kActivateSecondary) {
            RebindAuxiliaryHotkey(
                previous.enabled,
                previous.modifiers,
                previous.key);
        } else if (
            actionId ==
            hotkey_actions::
                kOpenShortcutManager) {
            RebindShortcutManagerHotkey(
                previous.enabled,
                previous.modifiers,
                previous.key);
        }

        return false;
    }

    if (settingsWindow_) {
        settingsWindow_->
            RefreshFromSettings();
    }

    return true;
}

bool App::ResetHotkeyBindings() {
    const auto previous =
        settingsStore_.Data()
            .hotkeyBindings;
    const auto defaults =
        DefaultHotkeyBindings();

    const auto oldPrimary =
        EffectiveHotkeyBinding(
            previous,
            hotkey_actions::kActivate);
    const auto oldAuxiliary =
        EffectiveHotkeyBinding(
            previous,
            hotkey_actions::kActivateSecondary);
    const auto oldShortcutManager =
        EffectiveHotkeyBinding(
            previous,
            hotkey_actions::
                kOpenShortcutManager);

    const auto primary =
        EffectiveHotkeyBinding(
            defaults,
            hotkey_actions::kActivate);
    const auto auxiliary =
        EffectiveHotkeyBinding(
            defaults,
            hotkey_actions::kActivateSecondary);
    const auto shortcutManager =
        EffectiveHotkeyBinding(
            defaults,
            hotkey_actions::
                kOpenShortcutManager);

    // Release optional global bindings before rebuilding defaults so an old
    // custom chord cannot collide with another default owned by this process.
    if (!RebindShortcutManagerHotkey(
            false,
            shortcutManager.modifiers,
            shortcutManager.key) ||
        !RebindAuxiliaryHotkey(
            false,
            auxiliary.modifiers,
            auxiliary.key)) {
        return false;
    }

    if (!RebindGlobalHotkey(
            primary.modifiers,
            primary.key)) {
        RebindAuxiliaryHotkey(
            oldAuxiliary.enabled,
            oldAuxiliary.modifiers,
            oldAuxiliary.key);
        RebindShortcutManagerHotkey(
            oldShortcutManager.enabled,
            oldShortcutManager.modifiers,
            oldShortcutManager.key);
        return false;
    }

    if (!RebindAuxiliaryHotkey(
            auxiliary.enabled,
            auxiliary.modifiers,
            auxiliary.key) ||
        !RebindShortcutManagerHotkey(
            shortcutManager.enabled,
            shortcutManager.modifiers,
            shortcutManager.key)) {
        RebindGlobalHotkey(
            oldPrimary.modifiers,
            oldPrimary.key);
        RebindAuxiliaryHotkey(
            oldAuxiliary.enabled,
            oldAuxiliary.modifiers,
            oldAuxiliary.key);
        RebindShortcutManagerHotkey(
            oldShortcutManager.enabled,
            oldShortcutManager.modifiers,
            oldShortcutManager.key);
        return false;
    }

    if (!settingsStore_
             .ResetHotkeyBindings()) {
        RebindGlobalHotkey(
            oldPrimary.modifiers,
            oldPrimary.key);
        RebindAuxiliaryHotkey(
            oldAuxiliary.enabled,
            oldAuxiliary.modifiers,
            oldAuxiliary.key);
        RebindShortcutManagerHotkey(
            oldShortcutManager.enabled,
            oldShortcutManager.modifiers,
            oldShortcutManager.key);
        return false;
    }

    if (settingsWindow_) {
        settingsWindow_->
            RefreshFromSettings();
    }

    return true;
}


bool App::IsHotkeyActionRegistered(
    std::string_view actionId) const {
    if (actionId ==
        hotkey_actions::kActivate) {
        return hotkeyRegistered_;
    }

    if (actionId ==
        hotkey_actions::
            kActivateSecondary) {
        const auto binding =
            EffectiveHotkeyBinding(
                settingsStore_.Data()
                    .hotkeyBindings,
                actionId);

        return !binding.enabled ||
            auxiliaryHotkeyRegistered_;
    }

    if (actionId ==
        hotkey_actions::
            kOpenShortcutManager) {
        const auto binding =
            EffectiveHotkeyBinding(
                settingsStore_.Data()
                    .hotkeyBindings,
                actionId);

        return !binding.enabled ||
            shortcutManagerHotkeyRegistered_;
    }

    const auto binding =
        EffectiveHotkeyBinding(
            settingsStore_.Data()
                .hotkeyBindings,
            actionId);

    return !binding.enabled ||
        FindHotkeyAction(actionId) != nullptr;
}

DWORD App::HotkeyActionLastError(
    std::string_view actionId) const noexcept {
    if (actionId ==
        hotkey_actions::kActivate) {
        return hotkeyLastError_;
    }

    if (actionId ==
        hotkey_actions::
            kActivateSecondary) {
        return auxiliaryHotkeyLastError_;
    }

    if (actionId ==
        hotkey_actions::
            kOpenShortcutManager) {
        return shortcutManagerHotkeyLastError_;
    }

    return ERROR_SUCCESS;
}

bool App::SetClassicBehavior(
    bool numericQuickLaunch,
    bool executeSingleResultImmediately,
    bool pinyinSearch) {

    const bool wasPinyinEnabled =
        settingsStore_.Data()
            .pinyinSearch;

    if (!settingsStore_
             .SetClassicBehavior(
                 numericQuickLaunch,
                 executeSingleResultImmediately,
                 pinyinSearch)) {
        return false;
    }

    if (wasPinyinEnabled &&
        !pinyinSearch) {
        searchEngine_
            .ReleasePinyinResources();
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

    if (providerId ==
        providers::
            kEverythingFilesystem) {
        const bool wasEnabled =
            providers::IsEnabled(
                settingsStore_.Data()
                    .providerEnabled,
                providers::
                    kEverythingFilesystem,
                false);

        if (wasEnabled == enabled) {
            return true;
        }

        win::ManagedEverythingServicePolicyResult
            servicePolicy;

        if (enabled) {
            servicePolicy =
                win::SetManagedEverythingServiceEnabled(
                    dataDirectory_,
                    true);
        } else {
            StopManagedEverythingLifecycle();

            servicePolicy =
                win::SetManagedEverythingServiceEnabled(
                    dataDirectory_,
                    false);
        }

        const bool servicePolicyFailed =
            servicePolicy.status ==
                win::ManagedEverythingServicePolicyStatus::
                    ElevationCancelled ||
            servicePolicy.status ==
                win::ManagedEverythingServicePolicyStatus::
                    Failed;

        if (servicePolicyFailed) {
            if (wasEnabled) {
                if (!everythingProvider_) {
                    everythingProvider_ =
                        std::make_unique<
                            EverythingProvider>();
                }

                StartEverythingBootstrap(
                    false);
            }

            return false;
        }

        if (!settingsStore_
                 .SetProviderEnabled(
                     std::move(id),
                     enabled)) {
            if (servicePolicy.status ==
                win::ManagedEverythingServicePolicyStatus::
                    Applied) {
                (void)win::
                    SetManagedEverythingServiceEnabled(
                        dataDirectory_,
                        wasEnabled);
            }

            if (wasEnabled) {
                if (!everythingProvider_) {
                    everythingProvider_ =
                        std::make_unique<
                            EverythingProvider>();
                }

                StartEverythingBootstrap(
                    false);
            }

            return false;
        }

        if (enabled) {
            if (!everythingProvider_) {
                everythingProvider_ =
                    std::make_unique<
                        EverythingProvider>();
            }

            StartEverythingBootstrap(
                false);
        }

        if (window_) {
            window_->RefreshResults();
        }

        if (settingsWindow_) {
            settingsWindow_
                ->RefreshFromSettings();
        }

        return true;
    }

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


bool App::SetProviderEnabledBatch(
    const ProviderEnableMap& changes) {

    ProviderEnableMap effective;
    std::vector<std::string>
        enabledChanges;

    const auto& current =
        settingsStore_.Data()
            .providerEnabled;

    for (const auto& [id, enabled] :
         changes) {
        if (id ==
            providers::
                kEverythingFilesystem) {
            continue;
        }

        if (providers::IsEnabled(
                current,
                id,
                true) ==
            enabled) {
            continue;
        }

        effective[id] = enabled;

        if (enabled) {
            enabledChanges.push_back(
                id);
        }
    }

    if (effective.empty()) {
        return true;
    }

    if (!settingsStore_
             .SetProviderEnabledBatch(
                 effective)) {
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
        settingsWindow_->
            RefreshFromSettings();
    }

    if (!enabledChanges.empty()) {
        StartProviderRefresh(
            std::move(
                enabledChanges));
    }

    return true;
}

bool App::SetUpdateSettings(
    bool autoCheck,
    UpdateChannel channel) {
    const auto previous =
        settingsStore_.Data();

    if (previous.autoCheckUpdates ==
            autoCheck &&
        previous.updateChannel ==
            channel) {
        return true;
    }

    if (!settingsStore_
             .SetUpdateSettings(
                 autoCheck,
                 channel)) {
        return false;
    }

    const bool channelChanged =
        previous.updateChannel !=
        channel;

    if (!channelChanged) {
        return true;
    }

    InvalidateUpdateCheckForChannelChange();
    return true;
}

void App::InvalidateUpdateCheckForChannelChange() {
    bool checking = false;
    bool preserveActiveUpdate = false;

    {
        std::scoped_lock lock(
            updateMutex_);

        checking =
            updateStatus_.stage ==
                win::UpdateStage::Checking &&
            updateStatus_.running;

        preserveActiveUpdate =
            updateStatus_.stage ==
                win::UpdateStage::Downloading ||
            updateStatus_.stage ==
                win::UpdateStage::Verifying ||
            updateStatus_.stage ==
                win::UpdateStage::Extracting ||
            updateStatus_.stage ==
                win::UpdateStage::ReadyToInstall ||
            updateStatus_.stage ==
                win::UpdateStage::Applying;

        updateSettingsChangedSinceCheck_ =
            true;

        if (checking) {
            ++updateGeneration_;
        }

        if (!preserveActiveUpdate) {
            updateStatus_ = {};
            updateManifest_.reset();
            updateInstallWhenReady_ =
                false;
        }
    }

    if (checking &&
        updateThread_.joinable()) {
        updateThread_.request_stop();
    }

    if (settingsWindow_) {
        settingsWindow_->
            OnUpdateStatusChanged();
    }
}

win::UpdateSnapshot
App::UpdateStatus() const {
    std::scoped_lock lock(
        updateMutex_);
    return updateStatus_;
}

bool App::UpdateSettingsChangedSinceCheck()
    const {
    std::scoped_lock lock(
        updateMutex_);
    return
        updateSettingsChangedSinceCheck_;
}

bool App::UpdateWorkerRunning()
    const noexcept {
    return updateWorkerRunning_.load();
}

bool App::StartUpdateCheck(
    bool force) {
    if (!force &&
        !settingsStore_.Data()
             .autoCheckUpdates) {
        return false;
    }

    if (!force) {
        const auto now =
            std::chrono::system_clock::
                to_time_t(
                    std::chrono::
                        system_clock::now());

        if (!win::UpdateAutoCheckDue(
                dataDirectory_,
                static_cast<std::int64_t>(
                    now))) {
            return false;
        }
    }

    if (updateWorkerRunning_.load()) {
        return false;
    }

    {
        std::scoped_lock lock(
            updateMutex_);

        if (updateStatus_.running) {
            return false;
        }
    }

    if (updateThread_.joinable()) {
        updateThread_.join();
    }

    const auto channel =
        settingsStore_.Data()
            .updateChannel;
    const std::uint64_t generation =
        ++updateGeneration_;

    {
        std::scoped_lock lock(
            updateMutex_);
        updateStatus_ = {};
        updateStatus_.stage =
            win::UpdateStage::Checking;
        updateStatus_.running = true;
        updateStatus_.currentVersion =
            std::string(kVersion);
        updateManifest_.reset();
        updateSettingsChangedSinceCheck_ =
            false;
        updateInstallWhenReady_ =
            false;
    }

    updateWorkerStartedTick_.store(
        static_cast<std::uint64_t>(
            GetTickCount64()));
    updateWorkerRunning_.store(true);

    updateThread_ =
        std::jthread(
            [this,
             channel,
             generation](
                std::stop_token stopToken) {
                const auto progress =
                    [this,
                     generation](
                        const win::
                            UpdateSnapshot&
                                snapshot) {
                        if (generation !=
                            updateGeneration_
                                .load()) {
                            return;
                        }

                        {
                            std::scoped_lock lock(
                                updateMutex_);

                            if (generation !=
                                updateGeneration_
                                    .load()) {
                                return;
                            }

                            updateStatus_ =
                                snapshot;
                        }

                        PostUpdateStatusNotification(
                            generation);
                    };

                const auto result =
                    win::CheckForUpdate(
                        dataDirectory_,
                        kVersion,
                        channel,
                        progress,
                        stopToken);

                if (generation ==
                    updateGeneration_.load()) {
                    std::scoped_lock lock(
                        updateMutex_);

                    if (generation ==
                        updateGeneration_
                            .load()) {
                        updateStatus_ =
                            result.snapshot;
                        updateManifest_ =
                            result.manifest;
                    }
                }

                updateWorkerRunning_.store(
                    false);
                updateWorkerStartedTick_.store(
                    0);

                PostUpdateStatusNotification(
                    generation);
            });

    StartUpdateReconcileTimer();

    if (settingsWindow_) {
        settingsWindow_->
            OnUpdateStatusChanged();
    }

    return true;
}

bool App::StartUpdateDownloadAndInstall() {
    UpdateManifest manifest;

    {
        std::scoped_lock lock(
            updateMutex_);

        if (updateStatus_.running ||
            updateStatus_.stage !=
                win::UpdateStage::
                    Available ||
            !updateManifest_) {
            return false;
        }

        manifest = *updateManifest_;
    }

    if (updateWorkerRunning_.load()) {
        return false;
    }

    if (updateThread_.joinable()) {
        updateThread_.join();
    }

    const auto channel =
        settingsStore_.Data()
            .updateChannel;
    const std::uint64_t generation =
        ++updateGeneration_;

    {
        std::scoped_lock lock(
            updateMutex_);
        updateStatus_.running = true;
        updateStatus_.stage =
            win::UpdateStage::
                Downloading;
        updateStatus_.failure =
            win::UpdateFailure::None;
        updateStatus_.nativeError = 0;
        updateStatus_.downloadedBytes =
            0;
        updateStatus_.totalBytes = 0;
        updateInstallWhenReady_ =
            true;
    }

    updateWorkerStartedTick_.store(
        static_cast<std::uint64_t>(
            GetTickCount64()));
    updateWorkerRunning_.store(true);

    updateThread_ =
        std::jthread(
            [this,
             channel,
             manifest =
                 std::move(manifest),
             generation](
                std::stop_token stopToken) {
                const auto progress =
                    [this,
                     generation](
                        const win::
                            UpdateSnapshot&
                                snapshot) {
                        if (generation !=
                            updateGeneration_
                                .load()) {
                            return;
                        }

                        {
                            std::scoped_lock lock(
                                updateMutex_);

                            if (generation !=
                                updateGeneration_
                                    .load()) {
                                return;
                            }

                            updateStatus_ =
                                snapshot;
                        }

                        PostUpdateStatusNotification(
                            generation);
                    };

                const auto result =
                    win::PrepareUpdate(
                        dataDirectory_,
                        channel,
                        manifest,
                        kVersion,
                        progress,
                        stopToken);

                if (generation ==
                    updateGeneration_.load()) {
                    std::scoped_lock lock(
                        updateMutex_);

                    if (generation ==
                        updateGeneration_
                            .load()) {
                        updateStatus_ =
                            result.snapshot;

                        if (result.snapshot
                                .stage !=
                            win::UpdateStage::
                                ReadyToInstall) {
                            updateInstallWhenReady_ =
                                false;
                        }
                    }
                }

                updateWorkerRunning_.store(
                    false);
                updateWorkerStartedTick_.store(
                    0);

                PostUpdateStatusNotification(
                    generation);
            });

    StartUpdateReconcileTimer();

    if (settingsWindow_) {
        settingsWindow_->
            OnUpdateStatusChanged();
    }

    return true;
}

bool App::SetWindowPlacementSettings(
    std::string launcherPlacement,
    std::string settingsPlacement,
    std::string shortcutManagerPlacement) {

    if (!settingsStore_.SetWindowPlacement(
            std::move(
                launcherPlacement),
            std::move(
                settingsPlacement),
            std::move(
                shortcutManagerPlacement))) {
        return false;
    }

    if (window_) {
        window_->ApplyGeneralSettings();
    }

    if (settingsWindow_) {
        settingsWindow_->
            RefreshFromSettings();
    }

    return true;
}

void App::RememberLauncherPosition(
    int x,
    int y) {
    settingsStore_.
        RememberLauncherPosition(
            x,
            y);
}

void App::RememberSettingsPosition(
    int x,
    int y) {
    settingsStore_.
        RememberSettingsPosition(
            x,
            y);
}

void App::RememberShortcutManagerPosition(
    int x,
    int y) {
    settingsStore_.
        RememberShortcutManagerPosition(
            x,
            y);
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

bool App::ApplySendToRegistration(
    bool enabled) const {

    PWSTR sendToRaw = nullptr;

    const HRESULT folderResult =
        SHGetKnownFolderPath(
            FOLDERID_SendTo,
            KF_FLAG_CREATE,
            nullptr,
            &sendToRaw);

    if (FAILED(folderResult) ||
        !sendToRaw) {
        CoTaskMemFree(sendToRaw);
        return false;
    }

    std::filesystem::path linkPath(
        sendToRaw);
    CoTaskMemFree(sendToRaw);

    linkPath /= L"ALTRun Next.lnk";

    if (!enabled) {
        std::error_code ec;
        std::filesystem::remove(
            linkPath,
            ec);
        return !ec;
    }

    std::array<wchar_t, 32768>
        executable{};

    const DWORD length =
        GetModuleFileNameW(
            nullptr,
            executable.data(),
            static_cast<DWORD>(
                executable.size()));

    if (length == 0 ||
        length >= executable.size()) {
        return false;
    }

    IShellLinkW* shellLink =
        nullptr;

    const HRESULT createResult =
        CoCreateInstance(
            CLSID_ShellLink,
            nullptr,
            CLSCTX_INPROC_SERVER,
            IID_PPV_ARGS(
                &shellLink));

    if (FAILED(createResult) ||
        !shellLink) {
        return false;
    }

    bool success =
        SUCCEEDED(
            shellLink->SetPath(
                executable.data())) &&
        SUCCEEDED(
            shellLink->SetArguments(
                L"--add-shortcut")) &&
        SUCCEEDED(
            shellLink->SetWorkingDirectory(
                baseDirectory_.c_str())) &&
        SUCCEEDED(
            shellLink->SetIconLocation(
                executable.data(),
                0)) &&
        SUCCEEDED(
            shellLink->SetDescription(
                L"Add to ALTRun Next shortcuts"));

    IPersistFile* persist =
        nullptr;

    if (success) {
        success =
            SUCCEEDED(
                shellLink->QueryInterface(
                    IID_PPV_ARGS(
                        &persist)));
    }

    if (success &&
        persist) {
        success =
            SUCCEEDED(
                persist->Save(
                    linkPath.c_str(),
                    TRUE));
    }

    if (persist) {
        persist->Release();
    }
    shellLink->Release();

    return success;
}

bool App::
ForwardShortcutRequestsToExistingInstance()
    const {

    HWND target = nullptr;

    for (int attempt = 0;
         attempt < 40 && !target;
         ++attempt) {
        target =
            FindWindowW(
                instance_ipc::
                    kLauncherWindowClass,
                nullptr);

        if (!target) {
            Sleep(50);
        }
    }

    if (!target) {
        return false;
    }

    for (const auto& path :
         startupShortcutPaths_) {
        if (path.empty()) {
            continue;
        }

        COPYDATASTRUCT copyData{};
        copyData.dwData =
            instance_ipc::
                kAddShortcutCopyData;
        copyData.cbData =
            static_cast<DWORD>(
                (path.size() + 1) *
                sizeof(wchar_t));
        copyData.lpData =
            const_cast<wchar_t*>(
                path.c_str());

        DWORD_PTR result = 0;

        if (!SendMessageTimeoutW(
                target,
                WM_COPYDATA,
                0,
                reinterpret_cast<LPARAM>(
                    &copyData),
                SMTO_ABORTIFHUNG |
                    SMTO_BLOCK,
                3000,
                &result) ||
            result == 0) {
            return false;
        }
    }

    return true;
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

void App::ShowAbout() {
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

    settingsWindow_->ShowAbout();
}

void App::ShowShortcutManager(
    std::wstring_view preferredId) {
    if (!shortcutManagerWindow_) {
        shortcutManagerWindow_ =
            std::make_unique<
                ShortcutManagerWindow>(
                    *this,
                    instance_);
    }

    shortcutManagerWindow_->Show(
        preferredId);
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

bool App::ExecuteCommand(
    std::size_t index,
    std::wstring_view runtimeInput,
    bool forceRunAsAdmin) {
    return LaunchCommand(
        commandStore_.Commands().at(index),
        true,
        runtimeInput,
        forceRunAsAdmin);
}

bool App::ExecuteResult(
    const LauncherResult& result,
    LauncherExecutionIntent intent) {
    const bool forceRunAsAdmin =
        intent ==
        LauncherExecutionIntent::
            RunAsAdministrator;

    const ActionEvaluation evaluation =
        EvaluateLauncherAction(
            result,
            intent,
            activationContext_
                .HasExplorer(),
            activationContext_
                .HasFileDialog(),
            activationContext_
                .HasTotalCommander());
    const LauncherAction& action =
        evaluation.action;

    if (action.kind ==
        LauncherActionKind::ExecuteCommand) {
        if (action.commandIndex ==
            static_cast<std::size_t>(-1)) {
            return false;
        }

        return ExecuteCommand(
            action.commandIndex,
            action.payload,
            forceRunAsAdmin);
    }

    const std::wstring& target =
        action.payload.empty()
            ? result.target
            : action.payload;

    if (action.kind ==
        LauncherActionKind::
            CopyText) {
        if (target.empty()) {
            return false;
        }

        if (win::
                SetClipboardUnicodeText(
                    target)) {
            return true;
        }

        MessageBoxW(
            nullptr,
            std::wstring(
                Text(
                    TextId::
                        UnableToCopy))
                .c_str(),
            L"ALTRun Next",
            MB_ICONERROR | MB_OK);

        return false;
    }

    if (action.kind ==
        LauncherActionKind::
            NavigateExplorer) {
        const auto context =
            activationContext_;

        return win::
            NavigateExplorerToFolder(
                context,
                target);
    }

    if (action.kind ==
        LauncherActionKind::
            NavigateFileDialog) {
        const auto context =
            activationContext_;

        return win::
            NavigateFileDialogToFolder(
                context,
                target);
    }

    if (action.kind ==
        LauncherActionKind::
            NavigateTotalCommander) {
        const auto context =
            activationContext_;

        return win::
            NavigateTotalCommanderToFolder(
                context,
                target);
    }

    switch (action.kind) {
    case LauncherActionKind::OpenFile:
    case LauncherActionKind::OpenFolder:
    case LauncherActionKind::OpenUrl:
        break;
    case LauncherActionKind::NavigateExplorer:
    case LauncherActionKind::NavigateFileDialog:
    case LauncherActionKind::NavigateTotalCommander:
    case LauncherActionKind::CopyText:
    case LauncherActionKind::ExecuteCommand:
        return false;
    }

    if (target.empty()) {
        return false;
    }

    SHELLEXECUTEINFOW info{};
    info.cbSize = sizeof(info);
    info.fMask =
        SEE_MASK_NOASYNC |
        SEE_MASK_FLAG_NO_UI;
    info.hwnd = nullptr;
    info.lpVerb =
        forceRunAsAdmin
            ? L"runas"
            : L"open";
    info.lpFile = target.c_str();
    info.nShow = SW_SHOWNORMAL;

    if (ShellExecuteExW(&info)) {
        if (result.action.commandIndex !=
                static_cast<std::size_t>(-1) &&
            result.action.commandIndex <
                commandStore_.Commands().size()) {
            const auto& source =
                commandStore_.Commands().at(
                    result.action.commandIndex);
            if (!source.id.empty()) {
                usageStore_.Record(source.id);
            }
        }
        return true;
    }

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

void App::CaptureActivationContext() {
    activationContext_ =
        win::CaptureWindowsContext(
            GetForegroundWindow());
    lastActivationContext_ =
        activationContext_;
}

void App::ClearActivationContext() {
    activationContext_ = {};
}

bool App::LaunchCommand(
    const Command& command,
    bool recordUsage,
    std::wstring_view runtimeInput,
    bool forceRunAsAdmin) {

    Command resolved = command;

    if (command.source ==
            CommandSource::User &&
        UsesFolderTemplate(
            command)) {
        const std::wstring_view folder =
            activationContext_
                .CurrentFilesystemFolder();

        if (folder.empty()) {
            MessageBoxW(
                nullptr,
                settingsStore_.Data().language ==
                        Language::ZhCN
                    ? L"此命令需要 {folder}，但当前没有可用的文件系统目录上下文。\n\n请从文件资源管理器或 Total Commander 的真实目录中唤起 ALTRun Next。"
                    : L"This command requires {folder}, but no filesystem-folder context is available.\n\nInvoke ALTRun Next from a real folder in File Explorer or Total Commander.",
                L"ALTRun Next",
                MB_ICONINFORMATION | MB_OK);
            return false;
        }

        resolved =
            ResolveFolderTemplate(
                command,
                folder);
    }

    if (resolved.runtimeInputMode !=
        RuntimeInputMode::None) {
        resolved =
            ResolveRuntimeInput(
                resolved,
                runtimeInput);
    }

    const bool bareTargetIsPath =
        resolved.type ==
        CommandType::Folder;

    const std::wstring target =
        win::ResolvePortablePath(
            resolved.target,
            baseDirectory_,
            bareTargetIsPath);

    // Arguments deliberately remain environment-expanded only. v0.7
    // path portability does not rewrite or reinterpret argument tokens.
    const std::wstring args =
        win::ExpandEnvironment(
            resolved.arguments);

    std::wstring cwd =
        win::ResolvePortablePath(
            resolved.workingDirectory,
            baseDirectory_,
            true);

    if (cwd.empty() &&
        resolved.source ==
            CommandSource::User) {
        cwd =
            DefaultShortcutWorkingDirectory(
                resolved.type,
                target);
    }

    SHELLEXECUTEINFOW info{};
    info.cbSize = sizeof(info);
    info.fMask = SEE_MASK_NOASYNC | SEE_MASK_FLAG_NO_UI;
    info.hwnd = nullptr;
    info.lpVerb =
        (forceRunAsAdmin ||
         resolved.runAsAdmin)
            ? L"runas"
            : nullptr;
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

    if (recordUsage &&
        !command.id.empty()) {
        usageStore_.Record(
            command.id);
    }

    return true;
}

} // namespace altrun
