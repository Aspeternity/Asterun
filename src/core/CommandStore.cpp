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
    commands_.clear();

    userCommandStore_.Load();

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
