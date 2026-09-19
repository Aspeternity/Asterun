#include "CommandTemplate.hpp"

namespace altrun {
namespace {

constexpr std::wstring_view
    kFolderToken = L"{folder}";

[[nodiscard]] bool
ContainsFolderToken(
    std::wstring_view value) {
    return value.find(
               kFolderToken) !=
        std::wstring_view::npos;
}

} // namespace

bool UsesFolderTemplate(
    const Command& command) {
    return
        ContainsFolderToken(
            command.target) ||
        ContainsFolderToken(
            command.arguments) ||
        ContainsFolderToken(
            command.workingDirectory);
}

std::wstring ReplaceFolderTemplate(
    std::wstring_view value,
    std::wstring_view folder) {
    std::wstring result(value);

    std::size_t position = 0;

    while ((position =
                result.find(
                    kFolderToken,
                    position)) !=
           std::wstring::npos) {
        result.replace(
            position,
            kFolderToken.size(),
            folder);

        position += folder.size();
    }

    return result;
}

Command ResolveFolderTemplate(
    const Command& command,
    std::wstring_view folder) {
    Command resolved = command;

    resolved.target =
        ReplaceFolderTemplate(
            command.target,
            folder);
    resolved.arguments =
        ReplaceFolderTemplate(
            command.arguments,
            folder);
    resolved.workingDirectory =
        ReplaceFolderTemplate(
            command.workingDirectory,
            folder);

    return resolved;
}

} // namespace altrun
