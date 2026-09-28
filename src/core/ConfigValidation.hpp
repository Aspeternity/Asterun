#pragma once

#include <nlohmann/json.hpp>
#include <limits>

namespace altrun::config {

inline bool FieldsHaveType(const nlohmann::json& value,
    std::initializer_list<const char*> names, nlohmann::json::value_t type) {
    for (const auto* name : names) {
        if (!value.contains(name)) continue;
        const auto& field = value[name];
        if (type == nlohmann::json::value_t::number_integer) {
            if (!field.is_number_integer()) return false;
            if (field.is_number_unsigned() && field.get<std::uint64_t>() >
                static_cast<std::uint64_t>(std::numeric_limits<int>::max())) return false;
            const auto number = field.get<std::int64_t>();
            if (number < std::numeric_limits<int>::min() ||
                number > std::numeric_limits<int>::max()) return false;
        } else if (field.type() != type) return false;
    }
    return true;
}

inline bool ValidCommandRecord(const nlohmann::json& item) {
    using Type = nlohmann::json::value_t;
    return item.is_object() &&
        FieldsHaveType(item, {"id", "keyword", "name", "type", "target",
            "runtimeInputMode", "arguments", "workingDirectory"}, Type::string) &&
        FieldsHaveType(item, {"enabled", "pinned", "runAsAdmin"}, Type::boolean) &&
        FieldsHaveType(item, {"sortOrder"}, Type::number_integer) &&
        FieldsHaveType(item, {"aliases", "legacyIds"}, Type::array);
}

inline bool ValidCommands(const nlohmann::json& root) {
    if (!root.contains("commands") || !root["commands"].is_array()) return false;
    for (const auto& item : root["commands"]) {
        if (!ValidCommandRecord(item)) return false;
    }
    return true;
}

inline bool ValidUsageRecord(const nlohmann::json& item) {
    if (!item.is_object()) return false;
    if (item.contains("launches") && (!item["launches"].is_number_unsigned() &&
        !(item["launches"].is_number_integer() && item["launches"].get<std::int64_t>() >= 0))) return false;
    if (item.contains("lastUsedUnix") && !item["lastUsedUnix"].is_number_integer()) return false;
    return !item.contains("queries") || item["queries"].is_object();
}

inline bool ValidUsage(const nlohmann::json& root) {
    if (!root.contains("usage") || !root["usage"].is_object()) return false;
    for (const auto& item : root["usage"]) if (!ValidUsageRecord(item)) return false;
    return true;
}

// Missing fields retain migration defaults; unknown fields remain forward
// compatible. Known fields must have the type required by the decoder.
inline bool ValidSettingsNode(const nlohmann::json& node) {
    using Type = nlohmann::json::value_t;
    if (!node.is_object()) return false;
    if (!FieldsHaveType(node, {"launcher", "language", "startupBehavior",
        "popupMonitor", "launcherMode", "settingsMode", "shortcutManagerMode",
        "key", "channel"}, Type::string) ||
        !FieldsHaveType(node, {"startWithWindows", "showTrayIcon", "soundEnabled",
        "addToSendToMenu", "launcherLastValid", "settingsLastValid",
        "shortcutManagerLastValid", "enabled", "pinyinSearch", "numericQuickLaunch",
        "executeSingleResultImmediately", "autoCheck"}, Type::boolean) ||
        !FieldsHaveType(node, {"launcherLastX", "launcherLastY", "settingsLastX",
        "settingsLastY", "shortcutManagerLastX", "shortcutManagerLastY"}, Type::number_integer) ||
        !FieldsHaveType(node, {"modifiers"}, Type::array)) return false;
    for (const auto* section : {"appearance", "general", "windowPlacement", "hotkey",
         "auxiliary", "hotkeys", "bindings", "behavior", "everything", "update"}) {
        if (node.contains(section) && !ValidSettingsNode(node[section])) return false;
    }
    if (node.contains("bindings")) {
        for (const auto& binding : node["bindings"]) if (!ValidSettingsNode(binding)) return false;
    }
    if (node.contains("providers")) {
        if (!node["providers"].is_object()) return false;
        for (const auto& enabled : node["providers"]) if (!enabled.is_boolean()) return false;
    }
    return true;
}

} // namespace altrun::config
