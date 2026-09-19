#pragma once

#include <string>
#include <string_view>
#include <unordered_map>

namespace altrun {

using ProviderEnableMap =
    std::unordered_map<std::string, bool>;

namespace providers {

inline constexpr std::string_view kStartMenu =
    "windows.startmenu";
inline constexpr std::string_view kPackaged =
    "windows.packaged";
inline constexpr std::string_view kAppPaths =
    "windows.apppaths";
inline constexpr std::string_view kPath =
    "windows.path";

[[nodiscard]] inline ProviderEnableMap
DefaultEnabled() {
    return {
        {std::string(kStartMenu), true},
        {std::string(kPackaged), true},
        {std::string(kAppPaths), true},
        {std::string(kPath), true},
    };
}

[[nodiscard]] inline bool IsEnabled(
    const ProviderEnableMap& enabled,
    std::string_view id,
    bool fallback = true) {

    const auto it =
        enabled.find(std::string(id));

    return it == enabled.end()
        ? fallback
        : it->second;
}

} // namespace providers
} // namespace altrun
