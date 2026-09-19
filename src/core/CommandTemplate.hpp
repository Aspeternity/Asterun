#pragma once

#include "Command.hpp"

#include <string>
#include <string_view>

namespace altrun {

[[nodiscard]] bool
UsesFolderTemplate(
    const Command& command);

[[nodiscard]] std::wstring
ReplaceFolderTemplate(
    std::wstring_view value,
    std::wstring_view folder);

[[nodiscard]] Command
ResolveFolderTemplate(
    const Command& command,
    std::wstring_view folder);

} // namespace altrun
