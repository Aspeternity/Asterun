#pragma once

#include <windows.h>

#include <string>
#include <string_view>
#include <vector>

namespace altrun::hotkey {

UINT ModifiersFromNames(
    const std::vector<std::string>& modifiers,
    bool includeNoRepeat = true);

UINT KeyFromName(std::string_view key);
std::string KeyName(UINT virtualKey);
std::wstring KeyDisplayName(UINT virtualKey);

} // namespace altrun::hotkey
