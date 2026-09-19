#include "CommandStore.hpp"

#include "../platform/WinUtil.hpp"

#include <algorithm>
#include <chrono>
#include <utility>

namespace altrun {

namespace {

bool IsUserSource(
    CommandSource source) {

    return source ==
        CommandSource::User;
}

std::wstring NormalizeForDedup(
    std::wstring_view value) {

    std::wstring result =
        win::Lower(
            win::Trim(value));

    std::replace(
        result.begin(),
        result.end(),
        L'/',
        L'\\');

    return result;
}

std::wstring NameKey(
    const Command& command) {

    std::wstring key =
        win::CompactKeyword(
            command.title);

    if (key.empty()) {
        key =
            win::CompactKeyword(
                command.keyword);
    }

    return key;
}

std::int64_t NowUnix() {
    return std::chrono::
        duration_cast<
            std::chrono::seconds>(
                std::chrono::
                    system_clock::now()
                    .time_since_epoch())
        .count();
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

        for (const auto& command :
             it->second.commands) {
            AddCommandTo(
                providerCommands_,
                command);
        }
    }

    RebuildMergedCommands();
}

ProviderRefreshOutcome
CommandStore::RefreshProviderCache(
    const ProviderEnableMap& enabled,
    const std::vector<std::string>&
        selectedIds) const {

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

    // If every enabled provider failed, leave the previous cache untouched.
    if (succeeded == 0) {
        return ProviderRefreshOutcome::
            Failed;
    }

    if (!providerCache_.Save(cache)) {
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

        const auto it =
            cache.find(
                descriptor.id);

        if (it != cache.end()) {
            status.commandCount =
                it->second.commands.size();
            status.lastRefreshUnix =
                it->second.generatedAtUnix;
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
    commands_.clear();

    for (const auto& command :
         userCommandStore_
             .Commands()) {
        if (!command.enabled) {
            continue;
        }

        AddCommand(command);
    }

    for (const auto& command :
         providerCommands_) {
        if (!command.enabled) {
            continue;
        }

        AddCommand(command);
    }
}

void CommandStore::AddCommand(
    Command command) {

    AddCommandTo(
        commands_,
        std::move(command));
}

void CommandStore::AddCommandTo(
    std::vector<Command>& output,
    Command command) {

    const std::wstring targetKey =
        NormalizeForDedup(
            command.target);

    const std::wstring keywordKey =
        win::Lower(
            command.keyword);

    const std::wstring nameKey =
        NameKey(command);

    for (const auto& existing :
         output) {

        const bool existingUser =
            IsUserSource(
                existing.source);

        const bool incomingUser =
            IsUserSource(
                command.source);

        const std::wstring
            existingTarget =
                NormalizeForDedup(
                    existing.target);

        if (!incomingUser &&
            !targetKey.empty() &&
            existingTarget ==
                targetKey) {
            return;
        }

        if (incomingUser &&
            !existingUser &&
            !targetKey.empty() &&
            existingTarget ==
                targetKey) {
            continue;
        }

        if (existingUser ||
            incomingUser) {
            continue;
        }

        const std::wstring
            existingKeyword =
                win::Lower(
                    existing.keyword);

        const std::wstring
            existingName =
                NameKey(existing);

        if (!nameKey.empty() &&
            nameKey ==
                existingName &&
            keywordKey ==
                existingKeyword) {
            return;
        }
    }

    output.push_back(
        std::move(command));
}

} // namespace altrun
