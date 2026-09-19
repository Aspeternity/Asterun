#include "Settings.hpp"

#include "ConfigIO.hpp"
#include "HotkeyRegistry.hpp"

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

void SyncLegacyHotkeyMirrors(
    Settings& settings) {
    const auto primary =
        EffectiveHotkeyBinding(
            settings.hotkeyBindings,
            hotkey_actions::kActivate);

    settings.hotkeyModifiers =
        primary.modifiers;
    settings.hotkeyKey =
        primary.key;

    const auto auxiliary =
        EffectiveHotkeyBinding(
            settings.hotkeyBindings,
            hotkey_actions::
                kActivateSecondary);

    settings.auxiliaryHotkeyEnabled =
        auxiliary.enabled;
    settings.auxiliaryHotkeyModifiers =
        auxiliary.modifiers;
    settings.auxiliaryHotkeyKey =
        auxiliary.key;
}

void ImportLegacyHotkeys(
    Settings& settings) {
    HotkeyBinding primary{
        true,
        settings.hotkeyModifiers,
        settings.hotkeyKey};
    CanonicalizeHotkeyBinding(primary);

    HotkeyBinding auxiliary{
        settings.auxiliaryHotkeyEnabled,
        settings.auxiliaryHotkeyModifiers,
        settings.auxiliaryHotkeyKey};
    CanonicalizeHotkeyBinding(auxiliary);

    settings.hotkeyBindings[
        std::string(
            hotkey_actions::kActivate)] =
        std::move(primary);

    settings.hotkeyBindings[
        std::string(
            hotkey_actions::
                kActivateSecondary)] =
        std::move(auxiliary);
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
    readOnlyDueToNewerSchema_ =
        false;
    unsupportedSchemaVersion_ = 0;
    recoveredFromBackup_ = false;
    migratedFromOlderSchema_ = false;
    migratedFromSchemaVersion_ = 0;

    if (LoadJson()) {
        return;
    }

    // A newer schema may contain fields this version does not understand.
    // Never migrate/default-save over it during downgrade.
    if (readOnlyDueToNewerSchema_) {
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
    auto load =
        config::LoadJsonWithBackup(
            jsonPath_,
            config::kSettingsSchemaVersion);

    recoveredFromBackup_ =
        load.status ==
            config::JsonLoadStatus::
                RecoveredBackup;

    if (load.status ==
        config::JsonLoadStatus::
            UnsupportedSchema) {

        // Read known fields for downgrade usability, but never write the
        // document back from an older schema implementation.
        readOnlyDueToNewerSchema_ =
            true;
        unsupportedSchemaVersion_ =
            load.schemaVersion;
    }

    if (!load.value) {
        return false;
    }

    try {
        const auto& root =
            *load.value;

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
            settings_.showOnStartup =
                general.value(
                    "showOnStartup",
                    settings_
                        .showOnStartup);
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

            if (hotkey.contains(
                    "auxiliary") &&
                hotkey["auxiliary"]
                    .is_object()) {

                const auto& auxiliary =
                    hotkey["auxiliary"];

                settings_
                    .auxiliaryHotkeyEnabled =
                        auxiliary.value(
                            "enabled",
                            settings_
                                .auxiliaryHotkeyEnabled);

                settings_.auxiliaryHotkeyKey =
                    LowerAscii(
                        auxiliary.value(
                            "key",
                            settings_
                                .auxiliaryHotkeyKey));

                if (auxiliary.contains(
                        "modifiers") &&
                    auxiliary["modifiers"]
                        .is_array()) {

                    settings_
                        .auxiliaryHotkeyModifiers
                        .clear();

                    for (const auto& item :
                         auxiliary["modifiers"]) {
                        if (!item.is_string()) {
                            continue;
                        }

                        settings_
                            .auxiliaryHotkeyModifiers
                            .push_back(
                                LowerAscii(
                                    item.get<
                                        std::string>()));
                    }
                }
            }
        }

        if (load.schemaVersion >= 4 &&
            root.contains("hotkeys") &&
            root["hotkeys"].is_object()) {

            const auto& hotkeys =
                root["hotkeys"];

            if (hotkeys.contains("bindings") &&
                hotkeys["bindings"]
                    .is_object()) {

                const auto& bindings =
                    hotkeys["bindings"];

                for (const auto& action :
                     HotkeyActionRegistry()) {
                    if (!bindings.contains(
                            action.id) ||
                        !bindings[action.id]
                             .is_object()) {
                        continue;
                    }

                    const auto& item =
                        bindings[action.id];

                    HotkeyBinding binding =
                        EffectiveHotkeyBinding(
                            settings_
                                .hotkeyBindings,
                            action.id);

                    binding.enabled =
                        item.value(
                            "enabled",
                            binding.enabled);
                    binding.key =
                        LowerAscii(
                            item.value(
                                "key",
                                binding.key));

                    if (item.contains(
                            "modifiers") &&
                        item["modifiers"]
                            .is_array()) {
                        binding.modifiers
                            .clear();

                        for (const auto& modifier :
                             item["modifiers"]) {
                            if (modifier.is_string()) {
                                binding.modifiers
                                    .push_back(
                                        LowerAscii(
                                            modifier.get<
                                                std::string>()));
                            }
                        }
                    }

                    CanonicalizeHotkeyBinding(
                        binding);

                    if (ValidateHotkeyBinding(
                            action.id,
                            binding)) {
                        settings_
                            .hotkeyBindings[
                                action.id] =
                            std::move(binding);
                    }
                }
            }

            SyncLegacyHotkeyMirrors(
                settings_);
        } else {
            ImportLegacyHotkeys(
                settings_);
        }

        if (root.contains("behavior") &&
            root["behavior"].is_object()) {

            const auto& behavior =
                root["behavior"];

            settings_.wildcardMatching =
                behavior.value(
                    "wildcardMatching",
                    settings_
                        .wildcardMatching);

            settings_.numericQuickLaunch =
                behavior.value(
                    "numericQuickLaunch",
                    settings_
                        .numericQuickLaunch);

            const std::string numericOrder =
                LowerAscii(
                    behavior.value(
                        "numericQuickLaunchOrder",
                        settings_
                            .numericQuickLaunchOrder));

            settings_.numericQuickLaunchOrder =
                numericOrder == "zero-to-nine"
                    ? "zero-to-nine"
                    : "one-to-zero";

            settings_
                .executeSingleResultImmediately =
                    behavior.value(
                        "executeSingleResultImmediately",
                        settings_
                            .executeSingleResultImmediately);
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

        if (!readOnlyDueToNewerSchema_ &&
            load.schemaVersion > 0 &&
            load.schemaVersion <
                config::kSettingsSchemaVersion) {
            const int previousSchema =
                load.schemaVersion;

            if (Save()) {
                migratedFromOlderSchema_ =
                    true;
                migratedFromSchemaVersion_ =
                    previousSchema;
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
    if (readOnlyDueToNewerSchema_) {
        return false;
    }

    nlohmann::json
        providersJson =
            nlohmann::json::object();

    for (const auto& [id, enabled] :
         settings_.providerEnabled) {
        providersJson[id] =
            enabled;
    }

    nlohmann::json
        hotkeyBindingsJson =
            nlohmann::json::object();

    for (const auto& action :
         HotkeyActionRegistry()) {
        const auto binding =
            EffectiveHotkeyBinding(
                settings_.hotkeyBindings,
                action.id);

        hotkeyBindingsJson[action.id] = {
            {"enabled", binding.enabled},
            {"modifiers",
             binding.modifiers},
            {"key", binding.key},
        };
    }

    const auto primary =
        EffectiveHotkeyBinding(
            settings_.hotkeyBindings,
            hotkey_actions::kActivate);
    const auto auxiliary =
        EffectiveHotkeyBinding(
            settings_.hotkeyBindings,
            hotkey_actions::
                kActivateSecondary);

    nlohmann::json root = {
        {"schemaVersion",
         config::kSettingsSchemaVersion},
        {"general", {
            {"startWithWindows",
             settings_.startWithWindows},
            {"showOnStartup",
             settings_.showOnStartup},
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
        // Compatibility mirror retained so schema-3 binaries can still
        // read the user's global bindings during a read-only downgrade.
        {"hotkey", {
            {"modifiers",
             primary.modifiers},
            {"key",
             primary.key},
            {"auxiliary", {
                {"enabled",
                 auxiliary.enabled},
                {"modifiers",
                 auxiliary.modifiers},
                {"key",
                 auxiliary.key}
            }}
        }},
        {"hotkeys", {
            {"bindings",
             std::move(
                 hotkeyBindingsJson)}
        }},
        {"behavior", {
            {"wildcardMatching",
             settings_.wildcardMatching},
            {"numericQuickLaunch",
             settings_.numericQuickLaunch},
            {"numericQuickLaunchOrder",
             settings_
                 .numericQuickLaunchOrder},
            {"executeSingleResultImmediately",
             settings_
                 .executeSingleResultImmediately}
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

    if (readOnlyDueToNewerSchema_) {
        return;
    }

    const Settings previous =
        settings_;

    settings_.uiStyle = style;

    if (!Save()) {
        settings_ = previous;
    }
}

void SettingsStore::SetLanguage(
    Language language) {

    if (readOnlyDueToNewerSchema_) {
        return;
    }

    const Settings previous =
        settings_;

    settings_.language = language;

    if (!Save()) {
        settings_ = previous;
    }
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

bool SettingsStore::SetShowOnStartup(
    bool enabled) {

    const Settings previous =
        settings_;

    settings_.showOnStartup =
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
    return SetHotkeyBinding(
        std::string(
            hotkey_actions::kActivate),
        HotkeyBinding{
            true,
            std::move(modifiers),
            std::move(key)});
}

bool SettingsStore::SetAuxiliaryHotkey(
    bool enabled,
    std::vector<std::string> modifiers,
    std::string key) {
    return SetHotkeyBinding(
        std::string(
            hotkey_actions::
                kActivateSecondary),
        HotkeyBinding{
            enabled,
            std::move(modifiers),
            std::move(key)});
}

bool SettingsStore::SetHotkeyBinding(
    std::string actionId,
    HotkeyBinding binding) {
    if (readOnlyDueToNewerSchema_) {
        return false;
    }

    CanonicalizeHotkeyBinding(binding);

    if (!ValidateHotkeyBinding(
            actionId,
            binding) ||
        FindHotkeyConflict(
            settings_.hotkeyBindings,
            actionId,
            binding)) {
        return false;
    }

    const Settings previous =
        settings_;

    settings_.hotkeyBindings[
        std::move(actionId)] =
        std::move(binding);

    SyncLegacyHotkeyMirrors(
        settings_);

    if (!Save()) {
        settings_ = previous;
        return false;
    }

    return true;
}

bool SettingsStore::ResetHotkeyBindings() {
    if (readOnlyDueToNewerSchema_) {
        return false;
    }

    const Settings previous =
        settings_;

    settings_.hotkeyBindings =
        DefaultHotkeyBindings();

    SyncLegacyHotkeyMirrors(
        settings_);

    if (!Save()) {
        settings_ = previous;
        return false;
    }

    return true;
}

bool SettingsStore::SetClassicBehavior(
    bool wildcardMatching,
    bool numericQuickLaunch,
    std::string numericQuickLaunchOrder,
    bool executeSingleResultImmediately) {

    const Settings previous =
        settings_;

    settings_.wildcardMatching =
        wildcardMatching;
    settings_.numericQuickLaunch =
        numericQuickLaunch;

    numericQuickLaunchOrder =
        LowerAscii(
            std::move(
                numericQuickLaunchOrder));

    settings_.numericQuickLaunchOrder =
        numericQuickLaunchOrder ==
                "zero-to-nine"
            ? "zero-to-nine"
            : "one-to-zero";

    settings_
        .executeSingleResultImmediately =
            executeSingleResultImmediately;

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

    if (readOnlyDueToNewerSchema_) {
        return;
    }

    const Settings previous =
        settings_;

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

    if (!Save()) {
        settings_ = previous;
    }
}

} // namespace altrun
