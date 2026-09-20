#include "CommandStore.hpp"

#include <chrono>
#include <optional>
#include <utility>

namespace altrun {

namespace {

std::int64_t NowUnix() {
    return std::chrono::
        duration_cast<
            std::chrono::seconds>(
                std::chrono::
                    system_clock::now()
                    .time_since_epoch())
        .count();
}

std::optional<CommandSource>
SourceForProviderId(
    std::string_view id) {

    if (id == providers::kStartMenu) {
        return CommandSource::StartMenu;
    }

    if (id == providers::kPackaged) {
        return CommandSource::PackagedApp;
    }

    if (id == providers::kAppPaths) {
        return CommandSource::AppPaths;
    }

    if (id == providers::kPath) {
        return CommandSource::Path;
    }

    return std::nullopt;
}

} // namespace

CommandStore::CommandStore(
    std::filesystem::path baseDirectory,
    std::filesystem::path dataDirectory)
    : baseDirectory_(
          std::move(baseDirectory)),
      dataDirectory_(
          std::move(dataDirectory)),
      userCommandStore_(
          dataDirectory_ /
              "commands.json",
          baseDirectory_ /
              "commands.tsv"),
      providerCache_(
          dataDirectory_ /
              "provider-cache.json") {}

void CommandStore::Reload(
    const ProviderEnableMap& enabled) {

    userCommandStore_.Load();

    ReloadProviderCache(
        enabled);
}

void CommandStore::ReloadProviderCache(
    const ProviderEnableMap& enabled) {

    providerCommands_.clear();

    const ProviderCacheData cache =
        providerCache_.Load();

    for (const auto& descriptor :
         providerRegistry_
             .Descriptors()) {

        if (!providers::IsEnabled(
                enabled,
                descriptor.id,
                descriptor
                    .defaultEnabled)) {
            continue;
        }

        const auto it =
            cache.find(
                descriptor.id);

        if (it == cache.end()) {
            continue;
        }

        providerCommands_.insert(
            providerCommands_.end(),
            it->second.commands.begin(),
            it->second.commands.end());
    }

    RebuildMergedCommands();
}

ProviderRefreshOutcome
CommandStore::RefreshProviderCache(
    const ProviderEnableMap& enabled,
    const std::vector<std::string>&
        selectedIds) {

    ProviderCacheData cache =
        providerCache_.Load();

    const auto results =
        providerRegistry_.Discover(
            enabled,
            selectedIds);

    if (results.empty()) {
        return ProviderRefreshOutcome::
            Success;
    }

    std::size_t succeeded = 0;
    std::size_t failed = 0;

    const std::int64_t generatedAt =
        NowUnix();

    {
        std::scoped_lock lock(
            providerDiagnosticsMutex_);

        for (const auto& result :
             results) {

            auto& diagnostic =
                providerDiagnostics_[
                    result.id];

            diagnostic.lastAttemptUnix =
                generatedAt;
            diagnostic
                .lastAttemptSucceeded =
                    result.success;
            diagnostic.lastError =
                result.success
                    ? std::wstring{}
                    : result.error;
        }
    }

    for (const auto& result :
         results) {

        if (!result.success) {
            ++failed;
            continue;
        }

        ProviderCacheEntry entry;
        entry.generatedAtUnix =
            generatedAt;
        entry.commands =
            result.commands;

        cache[result.id] =
            std::move(entry);

        ++succeeded;
    }

    // If every selected/enabled provider failed, leave the previous cache
    // untouched and keep the failure diagnostics visible in Settings.
    if (succeeded == 0) {
        return ProviderRefreshOutcome::
            Failed;
    }

    if (!providerCache_.Save(cache)) {
        std::scoped_lock lock(
            providerDiagnosticsMutex_);

        for (const auto& result :
             results) {
            if (!result.success) {
                continue;
            }

            auto& diagnostic =
                providerDiagnostics_[
                    result.id];

            diagnostic
                .lastAttemptSucceeded =
                    false;
            diagnostic.lastError =
                L"Unable to persist provider cache.";
        }

        return ProviderRefreshOutcome::
            Failed;
    }

    return failed == 0
        ? ProviderRefreshOutcome::
              Success
        : ProviderRefreshOutcome::
              Partial;
}

std::vector<ProviderDescriptor>
CommandStore::ProviderDescriptors() const {
    return providerRegistry_
        .Descriptors();
}

std::vector<ProviderChangeToken>
CommandStore::ProviderChangeTokens(
    const ProviderEnableMap& enabled) const {

    return providerRegistry_
        .ChangeTokens(enabled);
}

std::vector<ProviderStatus>
CommandStore::ProviderStatuses(
    const ProviderEnableMap& enabled) const {

    const ProviderCacheData cache =
        providerCache_.Load();

    std::unordered_map<
        std::string,
        ProviderRuntimeDiagnostic>
        diagnostics;

    {
        std::scoped_lock lock(
            providerDiagnosticsMutex_);

        diagnostics =
            providerDiagnostics_;
    }

    std::vector<ProviderStatus>
        statuses;

    for (const auto& descriptor :
         providerRegistry_.Descriptors()) {

        ProviderStatus status;
        status.id = descriptor.id;
        status.name = descriptor.name;
        status.enabled =
            providers::IsEnabled(
                enabled,
                descriptor.id,
                descriptor.defaultEnabled);

        const auto cacheIt =
            cache.find(
                descriptor.id);

        if (cacheIt != cache.end()) {
            status.commandCount =
                cacheIt->second.commands.size();

            status.lastRefreshUnix =
                cacheIt->second
                    .generatedAtUnix;
        }

        if (const auto source =
                SourceForProviderId(
                    descriptor.id)) {

            if (status.enabled) {
                status.activeCommandCount =
                    mergeStats_.Accepted(
                        *source);

                status.suppressedCommandCount =
                    mergeStats_.Suppressed(
                        *source);
            }
        }

        const auto diagnosticIt =
            diagnostics.find(
                descriptor.id);

        if (diagnosticIt !=
            diagnostics.end()) {

            status.lastAttemptUnix =
                diagnosticIt->second
                    .lastAttemptUnix;

            status.lastAttemptSucceeded =
                diagnosticIt->second
                    .lastAttemptSucceeded;

            status.lastError =
                diagnosticIt->second
                    .lastError;
        }

        statuses.push_back(
            std::move(status));
    }

    return statuses;
}

bool CommandStore::CreateUserCommand(
    Command command,
    std::wstring* createdId) {

    if (!userCommandStore_.Create(
            std::move(command),
            createdId)) {
        return false;
    }

    RebuildMergedCommands();
    return true;
}

bool CommandStore::UpdateUserCommand(
    std::wstring_view id,
    Command command) {

    if (!userCommandStore_.Update(
            id,
            std::move(command))) {
        return false;
    }

    RebuildMergedCommands();
    return true;
}

bool CommandStore::DeleteUserCommand(
    std::wstring_view id) {

    if (!userCommandStore_.Remove(
            id)) {
        return false;
    }

    RebuildMergedCommands();
    return true;
}

bool CommandStore::MoveUserCommand(
    std::wstring_view id,
    int direction) {

    if (!userCommandStore_.Move(
            id,
            direction)) {
        return false;
    }

    RebuildMergedCommands();
    return true;
}


bool CommandStore::ApplyUserCommandPathUpdates(
    const std::vector<UserCommandPathUpdate>& updates) {
    if (!userCommandStore_.ApplyPathUpdates(
            updates)) {
        return false;
    }

    RebuildMergedCommands();
    return true;
}

bool CommandStore::ImportUserCommands(
    const std::filesystem::path& path,
    bool legacyMode,
    std::size_t* imported,
    std::size_t* skipped) {

    if (!userCommandStore_.ImportTsv(
            path,
            legacyMode,
            imported,
            skipped)) {
        return false;
    }

    RebuildMergedCommands();
    return true;
}

bool CommandStore::ExportUserCommands(
    const std::filesystem::path& path) const {

    return userCommandStore_
        .ExportTsv(path);
}

void CommandStore::RebuildMergedCommands() {
    CommandMergeResult merged =
        MergeCommands(
            userCommandStore_
                .Commands(),
            providerCommands_);

    commands_ =
        std::move(
            merged.commands);

    mergeStats_ =
        merged.stats;
}

} // namespace altrun
