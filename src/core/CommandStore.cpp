#include "CommandStore.hpp"

#include "../platform/WinUtil.hpp"

namespace altrun {

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

    for (const auto& command : userCommandStore_.Commands()) {
        if (!command.enabled) continue;
        AddCommand(command);
    }

    for (auto command : startMenuProvider_.Discover()) {
        AddCommand(std::move(command));
    }
}

void CommandStore::AddCommand(Command command) {
    const std::wstring targetKey = win::Lower(command.target);
    const std::wstring keywordKey = win::Lower(command.keyword);

    for (const auto& existing : commands_) {
        if (win::Lower(existing.target) == targetKey &&
            win::Lower(existing.keyword) == keywordKey) {
            return;
        }
    }

    commands_.push_back(std::move(command));
}

} // namespace altrun
