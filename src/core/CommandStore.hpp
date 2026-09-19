#pragma once

#include "Command.hpp"
#include "StartMenuProvider.hpp"
#include "UserCommandStore.hpp"
#include "WindowsAppProvider.hpp"

#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace altrun {

class CommandStore {
public:
    CommandStore(
        std::filesystem::path baseDirectory,
        std::filesystem::path dataDirectory);

    void Reload();

    bool CreateUserCommand(Command command, std::wstring* createdId = nullptr);
    bool UpdateUserCommand(std::wstring_view id, Command command);
    bool DeleteUserCommand(std::wstring_view id);
    bool MoveUserCommand(std::wstring_view id, int direction);
    bool ImportUserCommands(
        const std::filesystem::path& path,
        bool legacyMode,
        std::size_t* imported = nullptr,
        std::size_t* skipped = nullptr);
    bool ExportUserCommands(const std::filesystem::path& path) const;

    [[nodiscard]] const std::vector<Command>& Commands() const noexcept {
        return commands_;
    }

    [[nodiscard]] const std::vector<Command>& UserCommands() const noexcept {
        return userCommandStore_.Commands();
    }

    [[nodiscard]] const std::unordered_map<std::wstring, std::wstring>& LegacyIdMap() const noexcept {
        return userCommandStore_.LegacyIdMap();
    }

    [[nodiscard]] const std::filesystem::path& UserCommandsPath() const noexcept {
        return userCommandStore_.Path();
    }

private:
    void RebuildMergedCommands();
    void AddCommand(Command command);

    std::filesystem::path baseDirectory_;
    std::filesystem::path dataDirectory_;
    UserCommandStore userCommandStore_;
    StartMenuProvider startMenuProvider_;
    WindowsAppProvider windowsAppProvider_;
    std::vector<Command> commands_;
};

} // namespace altrun
