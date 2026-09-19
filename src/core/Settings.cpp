#include "Settings.hpp"

#include "ConfigIO.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <string>

namespace altrun {

namespace {

std::string TrimAscii(std::string value) {
    auto isSpace = [](unsigned char c) {
        return std::isspace(c) != 0;
    };

    value.erase(
        value.begin(),
        std::find_if(
            value.begin(),
            value.end(),
            [&](char c) {
                return !isSpace(
                    static_cast<
                        unsigned char>(c));
            }));

    value.erase(
        std::find_if(
            value.rbegin(),
            value.rend(),
            [&](char c) {
                return !isSpace(
                    static_cast<
                        unsigned char>(c));
            }).base(),
        value.end());

    return value;
}

std::string LowerAscii(
    std::string value) {

    std::transform(
        value.begin(),
        value.end(),
        value.begin(),
        [](unsigned char c) {
            return static_cast<char>(
                std::tolower(c));
        });

    return value;
}

const char* UiStyleName(
    UiStyle style) {

    return style ==
            UiStyle::ModernCompact
        ? "modern-compact"
        : "classic";
}

const char* LanguageName(
    Language language) {

    return language ==
            Language::EnUS
        ? "en-US"
        : "zh-CN";
}

} // namespace

SettingsStore::SettingsStore(
    std::filesystem::path jsonPath,
    std::filesystem::path legacyIniPath)
    : jsonPath_(
          std::move(jsonPath)),
      legacyIniPath_(
          std::move(legacyIniPath)) {}

void SettingsStore::Load() {
    settings_ = Settings{};

    if (LoadJson()) {
        return;
    }

    if (!legacyIniPath_.empty() &&
        std::filesystem::exists(
            legacyIniPath_)) {
        MigrateLegacyIni();
    }

    Save();
}

bool SettingsStore::LoadJson() {
    const auto json =
        config::LoadJsonWithBackup(
            jsonPath_);

    if (!json) {
        return false;
    }

    try {
        const auto& root = *json;

        if (root.contains(
                "appearance") &&
            root["appearance"]
                .is_object()) {

            const auto& appearance =
                root["appearance"];

            const std::string ui =
                LowerAscii(
                    appearance.value(
                        "launcher",
                        std::string(
                            "classic")));

            settings_.uiStyle =
                (ui == "modern" ||
                 ui == "modern-compact")
                    ? UiStyle::ModernCompact
                    : UiStyle::Classic;

            const std::string language =
                LowerAscii(
                    appearance.value(
                        "language",
                        std::string(
                            "zh-cn")));

            settings_.language =
                (language == "en" ||
                 language == "en-us")
                    ? Language::EnUS
                    : Language::ZhCN;
        }

        if (root.contains("general") &&
            root["general"].is_object()) {

            const auto& general =
                root["general"];

            settings_.startWithWindows =
                general.value(
                    "startWithWindows",
                    settings_
                        .startWithWindows);
            settings_.hideAfterLaunch =
                general.value(
                    "hideAfterLaunch",
                    settings_
                        .hideAfterLaunch);
            settings_.clearQueryOnShow =
                general.value(
                    "clearQueryOnShow",
                    settings_
                        .clearQueryOnShow);
            settings_.hideOnFocusLost =
                general.value(
                    "hideOnFocusLost",
                    settings_
                        .hideOnFocusLost);
            settings_.showTrayIcon =
                general.value(
                    "showTrayIcon",
                    settings_
                        .showTrayIcon);
            settings_.popupMonitor =
                general.value(
                    "popupMonitor",
                    settings_
                        .popupMonitor);
        }

        if (root.contains("hotkey") &&
            root["hotkey"].is_object()) {

            const auto& hotkey =
                root["hotkey"];

            settings_.hotkeyKey =
                LowerAscii(
                    hotkey.value(
                        "key",
                        settings_
                            .hotkeyKey));

            if (hotkey.contains(
                    "modifiers") &&
                hotkey["modifiers"]
                    .is_array()) {

                settings_
                    .hotkeyModifiers
                    .clear();

                for (const auto& item :
                     hotkey["modifiers"]) {
                    if (!item.is_string()) {
                        continue;
                    }

                    settings_
                        .hotkeyModifiers
                        .push_back(
                            LowerAscii(
                                item.get<
                                    std::string>()));
                }

                if (settings_
                        .hotkeyModifiers
                        .empty()) {
                    settings_
                        .hotkeyModifiers
                        .push_back("alt");
                }
            }
        }

        // Provider settings were introduced after the original schema.
        // Missing keys intentionally keep their default-enabled behavior,
        // so existing users upgrade without losing application sources.
        if (root.contains("providers") &&
            root["providers"].is_object()) {

            for (auto it =
                     root["providers"].begin();
                 it !=
                     root["providers"].end();
                 ++it) {

                if (!it.value()
                         .is_boolean()) {
                    continue;
                }

                settings_
                    .providerEnabled[
                        it.key()] =
                    it.value()
                        .get<bool>();
            }
        }

        return true;
    } catch (...) {
        return false;
    }
}

bool SettingsStore::MigrateLegacyIni() {
    std::ifstream input(
        legacyIniPath_,
        std::ios::binary);

    if (!input) {
        return false;
    }

    std::string section;
    std::string line;

    while (std::getline(
        input,
        line)) {

        if (!line.empty() &&
            line.back() == '\r') {
            line.pop_back();
        }

        line = TrimAscii(
            line);

        if (line.empty() ||
            line[0] == ';' ||
            line[0] == '#') {
            continue;
        }

        if (line.front() == '[' &&
            line.back() == ']') {
            section =
                LowerAscii(
                    TrimAscii(
                        line.substr(
                            1,
                            line.size() - 2)));
            continue;
        }

        const auto equals =
            line.find('=');

        if (equals ==
            std::string::npos) {
            continue;
        }

        const std::string key =
            LowerAscii(
                TrimAscii(
                    line.substr(
                        0,
                        equals)));

        const std::string value =
            LowerAscii(
                TrimAscii(
                    line.substr(
                        equals + 1)));

        if (section != "general") {
            continue;
        }

        if (key == "ui") {
            settings_.uiStyle =
                (value == "modern" ||
                 value ==
                     "modern-compact")
                    ? UiStyle::ModernCompact
                    : UiStyle::Classic;
        } else if (
            key == "language") {
            settings_.language =
                (value == "en" ||
                 value == "en-us")
                    ? Language::EnUS
                    : Language::ZhCN;
        }
    }

    return true;
}

bool SettingsStore::Save() const {
    nlohmann::json
        providersJson =
            nlohmann::json::object();

    for (const auto& [id, enabled] :
         settings_.providerEnabled) {
        providersJson[id] =
            enabled;
    }

    nlohmann::json root = {
        {"schemaVersion",
         config::kSchemaVersion},
        {"general", {
            {"startWithWindows",
             settings_.startWithWindows},
            {"hideAfterLaunch",
             settings_.hideAfterLaunch},
            {"clearQueryOnShow",
             settings_.clearQueryOnShow},
            {"hideOnFocusLost",
             settings_.hideOnFocusLost},
            {"showTrayIcon",
             settings_.showTrayIcon},
            {"popupMonitor",
             settings_.popupMonitor}
        }},
        {"hotkey", {
            {"modifiers",
             settings_.hotkeyModifiers},
            {"key",
             settings_.hotkeyKey}
        }},
        {"appearance", {
            {"launcher",
             UiStyleName(
                 settings_.uiStyle)},
            {"language",
             LanguageName(
                 settings_.language)}
        }},
        {"providers",
         std::move(providersJson)}
    };

    return config::SaveJsonAtomic(
        jsonPath_,
        root);
}

void SettingsStore::SetUiStyle(
    UiStyle style) {

    settings_.uiStyle = style;
    Save();
}

void SettingsStore::SetLanguage(
    Language language) {

    settings_.language = language;
    Save();
}

bool SettingsStore::SetStartWithWindows(
    bool enabled) {

    const Settings previous =
        settings_;

    settings_.startWithWindows =
        enabled;

    if (!Save()) {
        settings_ = previous;
        return false;
    }

    return true;
}

bool SettingsStore::SetHotkey(
    std::vector<std::string> modifiers,
    std::string key) {

    const Settings previous =
        settings_;

    settings_.hotkeyModifiers =
        std::move(modifiers);
    settings_.hotkeyKey =
        LowerAscii(
            std::move(key));

    if (!Save()) {
        settings_ = previous;
        return false;
    }

    return true;
}

bool SettingsStore::SetProviderEnabled(
    std::string id,
    bool enabled) {

    const Settings previous =
        settings_;

    settings_.providerEnabled[
        std::move(id)] =
        enabled;

    if (!Save()) {
        settings_ = previous;
        return false;
    }

    return true;
}

bool SettingsStore::ResetDefaults() {
    const Settings previous =
        settings_;

    settings_ = Settings{};

    if (!Save()) {
        settings_ = previous;
        return false;
    }

    return true;
}

void SettingsStore::SetGeneral(
    bool hideAfterLaunch,
    bool clearQueryOnShow,
    bool hideOnFocusLost,
    bool showTrayIcon,
    std::string popupMonitor) {

    settings_.hideAfterLaunch =
        hideAfterLaunch;
    settings_.clearQueryOnShow =
        clearQueryOnShow;
    settings_.hideOnFocusLost =
        hideOnFocusLost;
    settings_.showTrayIcon =
        showTrayIcon;
    settings_.popupMonitor =
        std::move(popupMonitor);

    Save();
}

} // namespace altrun
