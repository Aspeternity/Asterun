#include "CommandStore.hpp"

#include "../platform/WinUtil.hpp"

#include <algorithm>
#include <array>

namespace altrun {

namespace {

bool IsUserSource(CommandSource source) {
    return source == CommandSource::User;
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

std::wstring NameKey(const Command& command) {
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

} // namespace

CommandStore::CommandStore(
    std::filesystem::path baseDirectory,
    std::filesystem::path dataDirectory)
    : baseDirectory_(std::move(baseDirectory)),
      dataDirectory_(std::move(dataDirectory)),
      userCommandStore_(
          dataDirectory_ / "commands.json",
          baseDirectory_ / "commands.tsv") {}

void CommandStore::Reload() {
    userCommandStore_.Load();
    RebuildMergedCommands();
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

    if (!userCommandStore_.Remove(id)) {
        return false;
    }

    RebuildMergedCommands();
    return true;
}

bool CommandStore::MoveUserCommand(
    std::wstring_view id,
    int direction) {

    if (!userCommandStore_.Move(id, direction)) {
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

    return userCommandStore_.ExportTsv(path);
}

void CommandStore::RebuildMergedCommands() {
    commands_.clear();

    for (const auto& command :
         userCommandStore_.Commands()) {
        if (!command.enabled) {
            continue;
        }

        AddCommand(command);
    }

    const std::array<
        const ICommandProvider*,
        2> providers{
            &startMenuProvider_,
            &windowsAppProvider_,
        };

    for (const ICommandProvider* provider :
         providers) {
        for (auto command :
             provider->Discover()) {
            AddCommand(
                std::move(command));
        }
    }
}

void CommandStore::AddCommand(
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
         commands_) {

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

        // A user-defined shortcut is authoritative for the same target.
        // Multiple user shortcuts may intentionally point at the same app
        // under different keywords, so user/user duplicates are preserved.
        if (!incomingUser &&
            !targetKey.empty() &&
            existingTarget == targetKey) {
            return;
        }

        if (incomingUser &&
            !existingUser &&
            !targetKey.empty() &&
            existingTarget == targetKey) {
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

        // Different automatic providers can expose the same app through
        // a .lnk, App Paths, AppsFolder or PATH executable. Prefer the first
        // higher-priority provider when both the effective name and keyword
        // agree, even if their launch targets are represented differently.
        if (!nameKey.empty() &&
            nameKey == existingName &&
            keywordKey ==
                existingKeyword) {
            return;
        }
    }

    commands_.push_back(
        std::move(command));
}

} // namespace altrun
