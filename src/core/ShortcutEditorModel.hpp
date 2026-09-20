#pragma once

#include "Command.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace altrun {

struct ShortcutKeywordSet {
    std::wstring primary;
    std::vector<std::wstring> aliases;
};

struct ShortcutKeywordConflict {
    std::wstring token;
    std::wstring existingId;
    std::wstring existingTitle;
};

[[nodiscard]] ShortcutKeywordSet
ParseShortcutKeywords(
    std::wstring_view text);

[[nodiscard]] std::wstring
FormatShortcutKeywords(
    std::wstring_view primary,
    const std::vector<std::wstring>& aliases);

[[nodiscard]] std::optional<ShortcutKeywordConflict>
FindShortcutKeywordConflict(
    const Command& candidate,
    const std::vector<Command>& existingCommands,
    std::wstring_view ignoredId = {});

[[nodiscard]] bool
ShortcutMatchesFilter(
    const Command& command,
    std::wstring_view filterText);

[[nodiscard]] CommandType
InferShortcutCommandType(
    std::wstring_view target);

[[nodiscard]] std::wstring
SuggestShortcutTitle(
    std::wstring_view target,
    CommandType type,
    std::wstring_view fallback = {});

[[nodiscard]] std::wstring
DefaultShortcutWorkingDirectory(
    CommandType type,
    std::wstring_view resolvedTarget);

} // namespace altrun
