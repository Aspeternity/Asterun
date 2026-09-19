#pragma once

#include "Command.hpp"

#include <filesystem>
#include <vector>

namespace altrun {

class CommandStore {
public:
    explicit CommandStore(std::filesystem::path baseDirectory);

    void Reload();
    [[nodiscard]] const std::vector<Command>& Commands() const noexcept { return commands_; }
    [[nodiscard]] const std::filesystem::path& CustomCommandsPath() const noexcept { return customCommandsPath_; }

private:
    void EnsureDefaultCustomCommands();
    void LoadCustomCommands();
    void LoadStartMenuCommands();
    void ScanStartMenuPath(const std::filesystem::path& root);
    void AddCommand(Command command);

    std::filesystem::path baseDirectory_;
    std::filesystem::path customCommandsPath_;
    std::vector<Command> commands_;
};

} // namespace altrun
