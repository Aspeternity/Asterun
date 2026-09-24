#include "core/ConfigIO.hpp"
#include "core/HotkeyRegistry.hpp"
#include "core/ProviderIds.hpp"
#include "core/Settings.hpp"
#include "Version.hpp"

#include <nlohmann/json.hpp>

#include <array>
#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

using namespace altrun;

namespace {

std::string ExpectedDefaultUpdateChannelName() {
    return DefaultUpdateChannelForVersion(kVersion) ==
            UpdateChannel::Stable
        ? "stable"
        : "development";
}

std::string ReadText(
    const std::filesystem::path& path) {
    std::ifstream input(
        path,
        std::ios::binary);
    assert(input);
    return std::string(
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>());
}

std::vector<std::string>
StringArray(
    const nlohmann::json& value) {
    std::vector<std::string> result;
    if (!value.is_array()) {
        return result;
    }
    for (const auto& item : value) {
        if (item.is_string()) {
            result.push_back(
                item.get<std::string>());
        }
    }
    return result;
}

void CopyFixture(
    const std::filesystem::path& fixtureRoot,
    std::string_view name,
    const std::filesystem::path& destination) {
    std::filesystem::create_directories(
        destination.parent_path());
    std::filesystem::copy_file(
        fixtureRoot / std::string(name),
        destination,
        std::filesystem::copy_options::
            overwrite_existing);
}

void AssertBindingMatches(
    const Settings& settings,
    const nlohmann::json& bindings,
    std::string_view actionId) {
    const auto actual =
        EffectiveHotkeyBinding(
            settings.hotkeyBindings,
            actionId);

    if (!bindings.contains(
            std::string(actionId))) {
        const auto* descriptor =
            FindHotkeyAction(
                actionId);
        assert(descriptor);

        auto expected =
            descriptor->defaultBinding;
        CanonicalizeHotkeyBinding(
            expected);

        assert(actual.enabled ==
            expected.enabled);
        assert(actual.key ==
            expected.key);
        assert(actual.modifiers ==
            expected.modifiers);
        return;
    }

    const auto& expected =
        bindings[std::string(actionId)];

    assert(
        actual.enabled ==
        expected.at("enabled").get<bool>());
    assert(
        actual.key ==
        expected.at("key").get<std::string>());
    assert(
        actual.modifiers ==
        StringArray(
            expected.at("modifiers")));
}

void AssertCommonFields(
    const Settings& settings,
    const nlohmann::json& root) {
    const auto& general =
        root.at("general");

    assert(
        settings.startWithWindows ==
        general.at("startWithWindows")
            .get<bool>());

    StartupBehavior expectedStartup =
        StartupBehavior::Notification;

    if (general.contains(
            "startupBehavior")) {
        const std::string value =
            general.at("startupBehavior")
                .get<std::string>();

        if (value == "silent") {
            expectedStartup =
                StartupBehavior::Silent;
        } else if (
            value == "show-launcher") {
            expectedStartup =
                StartupBehavior::ShowLauncher;
        }
    } else if (
        general.contains(
            "showOnStartup")) {
        expectedStartup =
            general.at("showOnStartup")
                .get<bool>()
                ? StartupBehavior::
                      ShowLauncher
                : StartupBehavior::
                      Silent;
    }

    assert(
        settings.startupBehavior ==
        expectedStartup);

    assert(
        settings.showTrayIcon ==
        general.value(
            "showTrayIcon",
            true));
    assert(
        settings.addToSendToMenu ==
        general.value(
            "addToSendToMenu",
            false));
    assert(
        settings.popupMonitor ==
        general.value(
            "popupMonitor",
            std::string("cursor")));

    const auto& behavior =
        root.at("behavior");

    assert(
        settings.pinyinSearch ==
        behavior.value(
            "pinyinSearch",
            true));
    assert(
        settings.numericQuickLaunch ==
        behavior.at("numericQuickLaunch")
            .get<bool>());
    assert(
        settings.executeSingleResultImmediately ==
        behavior.at(
            "executeSingleResultImmediately")
            .get<bool>());

    const auto& appearance =
        root.at("appearance");
    assert(
        settings.uiStyle ==
        (appearance.at("launcher")
                 .get<std::string>() ==
             "modern-compact"
             ? UiStyle::ModernCompact
             : UiStyle::Classic));
    assert(
        settings.language ==
        (appearance.at("language")
                 .get<std::string>() ==
             "en-US"
             ? Language::EnUS
             : Language::ZhCN));

    if (appearance.contains(
            "showResultIcons")) {
        assert(
            settings.showResultIcons ==
            appearance.at(
                "showResultIcons")
                .get<bool>());
    } else {
        assert(
            !settings.showResultIcons);
    }

    for (auto it =
             root.at("providers").begin();
         it != root.at("providers").end();
         ++it) {
        assert(
            providers::IsEnabled(
                settings.providerEnabled,
                it.key(),
                false) ==
            it.value().get<bool>());
    }
}

void AssertDowngradeReadOnly(
    const std::filesystem::path& settingsPath) {
    const std::string before =
        ReadText(settingsPath);

    const auto downgrade =
        config::LoadJsonWithBackup(
            settingsPath,
            5);

    assert(
        downgrade.status ==
        config::JsonLoadStatus::
            UnsupportedSchema);
    assert(
        downgrade.schemaVersion ==
        config::kSettingsSchemaVersion);
    assert(ReadText(settingsPath) == before);
}

void AssertSchema3Migration(
    const std::filesystem::path& fixtureRoot,
    std::string_view fixtureName,
    const std::filesystem::path& workRoot,
    bool expectNavigateDisabled) {
    const auto path =
        workRoot /
        std::string(fixtureName);

    CopyFixture(
        fixtureRoot,
        fixtureName,
        path);

    const auto source =
        nlohmann::json::parse(
            ReadText(path));

    SettingsStore store(path);
    store.Load();

    assert(store.WasMigratedFromOlderSchema());
    assert(
        store.MigratedFromSchemaVersion() ==
        3);
    assert(
        !store.IsReadOnlyDueToNewerSchema());

    AssertCommonFields(
        store.Data(),
        source);

    const auto& legacy =
        source.at("hotkey");

    const auto primary =
        EffectiveHotkeyBinding(
            store.Data().hotkeyBindings,
            hotkey_actions::kActivate);
    assert(primary.enabled);
    assert(
        primary.modifiers ==
        StringArray(
            legacy.at("modifiers")));
    assert(
        primary.key ==
        legacy.at("key").get<std::string>());

    const auto& auxiliary =
        legacy.at("auxiliary");
    const auto actualAuxiliary =
        EffectiveHotkeyBinding(
            store.Data().hotkeyBindings,
            hotkey_actions::kActivateSecondary);
    assert(
        actualAuxiliary.enabled ==
        auxiliary.at("enabled").get<bool>());
    assert(
        actualAuxiliary.modifiers ==
        StringArray(
            auxiliary.at("modifiers")));
    assert(
        actualAuxiliary.key ==
        auxiliary.at("key").get<std::string>());

    const auto openSettings =
        EffectiveHotkeyBinding(
            store.Data().hotkeyBindings,
            hotkey_actions::kOpenSettings);
    assert(openSettings.enabled);
    assert(openSettings.key == "f2");

    const auto navigate =
        EffectiveHotkeyBinding(
            store.Data().hotkeyBindings,
            hotkey_actions::
                kNavigateCurrentFileManager);
    assert(
        navigate.enabled ==
        !expectNavigateDisabled);

    const auto copyTarget =
        EffectiveHotkeyBinding(
            store.Data().hotkeyBindings,
            hotkey_actions::
                kCopySelectedTarget);
    assert(copyTarget.enabled);

    const auto migrated =
        nlohmann::json::parse(
            ReadText(path));
    assert(
        migrated.at("schemaVersion")
            .get<int>() ==
        config::kSettingsSchemaVersion);
    assert(
        migrated.at("behavior")
            .at("pinyinSearch")
            .get<bool>());
    assert(
        !migrated.at("appearance")
             .at("showResultIcons")
             .get<bool>());
    assert(
        migrated.at("hotkeys")
            .at("bindings")
            .size() ==
        HotkeyActionRegistry().size());

    if (expectNavigateDisabled) {
        assert(
            !migrated.at("hotkeys")
                 .at("bindings")
                 .at(
                     std::string(
                         hotkey_actions::
                             kNavigateCurrentFileManager))
                 .at("enabled")
                 .get<bool>());
    }

    AssertDowngradeReadOnly(path);
}

void AssertSchema4Migration(
    const std::filesystem::path& fixtureRoot,
    std::string_view fixtureName,
    const std::filesystem::path& workRoot) {
    const auto path =
        workRoot /
        std::string(fixtureName);

    CopyFixture(
        fixtureRoot,
        fixtureName,
        path);

    const auto source =
        nlohmann::json::parse(
            ReadText(path));

    SettingsStore store(path);
    store.Load();

    assert(store.WasMigratedFromOlderSchema());
    assert(
        store.MigratedFromSchemaVersion() ==
        4);
    assert(
        !store.IsReadOnlyDueToNewerSchema());

    AssertCommonFields(
        store.Data(),
        source);
    assert(store.Data().pinyinSearch);

    const auto& bindings =
        source.at("hotkeys")
            .at("bindings");

    for (const auto& action :
         HotkeyActionRegistry()) {
        AssertBindingMatches(
            store.Data(),
            bindings,
            action.id);
    }

    const auto migrated =
        nlohmann::json::parse(
            ReadText(path));

    assert(
        migrated.at("schemaVersion")
            .get<int>() ==
        config::kSettingsSchemaVersion);
    assert(
        migrated.at("behavior")
            .at("pinyinSearch")
            .get<bool>());
    assert(
        !migrated.at("appearance")
             .at("showResultIcons")
             .get<bool>());

    AssertDowngradeReadOnly(path);
}

void AssertSchema5Migration(
    const std::filesystem::path& workRoot) {
    const auto path =
        workRoot /
        "schema5-to-schema6" /
        "settings.json";

    std::filesystem::create_directories(
        path.parent_path());

    nlohmann::json source = {
        {"schemaVersion", 5},
        {"appearance", {
            {"launcher", "modern-compact"},
            {"language", "en-US"}
        }},
        {"behavior", {
            {"pinyinSearch", true},
            {"wildcardMatching", false},
            {"numericQuickLaunch", false},
            {"numericQuickLaunchOrder",
             "one-to-zero"},
            {"executeSingleResultImmediately",
             false}
        }},
        {"general", {
            {"startWithWindows", false},
            {"showOnStartup", false},
            {"hideAfterLaunch", true},
            {"clearQueryOnShow", true},
            {"hideOnFocusLost", true},
            {"showTrayIcon", true},
            {"popupMonitor", "cursor"}
        }},
        {"providers", {
            {"windows.startmenu", true},
            {"windows.packaged", true},
            {"windows.apppaths", true},
            {"windows.path", true},
            {"everything.filesystem", false}
        }}
    };

    {
        std::ofstream output(
            path,
            std::ios::binary |
                std::ios::trunc);
        assert(output);
        output << source.dump(2);
    }

    SettingsStore store(path);
    store.Load();

    assert(
        store.WasMigratedFromOlderSchema());
    assert(
        store.MigratedFromSchemaVersion() ==
        5);
    assert(
        !store.IsReadOnlyDueToNewerSchema());
    assert(
        !store.Data().showResultIcons);

    const auto migrated =
        nlohmann::json::parse(
            ReadText(path));

    assert(
        migrated.at("schemaVersion")
            .get<int>() ==
        config::kSettingsSchemaVersion);
    assert(
        !migrated.at("appearance")
             .at("showResultIcons")
             .get<bool>());

    AssertDowngradeReadOnly(path);
}

void AssertSchema6Migration(
    const std::filesystem::path& workRoot) {
    const auto path =
        workRoot /
        "schema6-to-schema7" /
        "settings.json";

    std::filesystem::create_directories(
        path.parent_path());

    nlohmann::json source = {
        {"schemaVersion", 6},
        {"appearance", {
            {"launcher", "classic"},
            {"language", "zh-CN"},
            {"showResultIcons", false}
        }},
        {"behavior", {
            {"pinyinSearch", true},
            {"wildcardMatching", false},
            {"numericQuickLaunch", false},
            {"numericQuickLaunchOrder", "one-to-zero"},
            {"executeSingleResultImmediately", false}
        }},
        {"general", {
            {"startWithWindows", false},
            {"showOnStartup", false},
            {"hideAfterLaunch", true},
            {"clearQueryOnShow", true},
            {"hideOnFocusLost", true},
            {"showTrayIcon", true},
            {"popupMonitor", "cursor"}
        }},
        {"providers", {
            {"windows.startmenu", true},
            {"windows.packaged", true},
            {"windows.apppaths", true},
            {"windows.path", true},
            {"everything.filesystem", false}
        }}
    };

    {
        std::ofstream output(
            path,
            std::ios::binary |
                std::ios::trunc);
        assert(output);
        output << source.dump(2);
    }

    SettingsStore store(path);
    store.Load();

    assert(
        store.WasMigratedFromOlderSchema());
    assert(
        store.MigratedFromSchemaVersion() ==
        6);
    assert(
        !store.IsReadOnlyDueToNewerSchema());
    assert(
        store.Data().autoCheckUpdates);
    assert(
        store.Data().updateChannel ==
        DefaultUpdateChannelForVersion(kVersion));

    const auto migrated =
        nlohmann::json::parse(
            ReadText(path));

    assert(
        migrated.at("schemaVersion")
            .get<int>() ==
        config::kSettingsSchemaVersion);
    assert(
        migrated.at("update")
            .at("autoCheck")
            .get<bool>());
    assert(
        migrated.at("update")
            .at("channel")
            .get<std::string>() ==
        ExpectedDefaultUpdateChannelName());

    AssertDowngradeReadOnly(path);
}

void AssertSchema7Migration(
    const std::filesystem::path& workRoot) {
    const auto path =
        workRoot /
        "schema7-to-schema8" /
        "settings.json";

    std::filesystem::create_directories(
        path.parent_path());

    nlohmann::json source = {
        {"schemaVersion", 7},
        {"general", {
            {"startWithWindows", false},
            {"showOnStartup", false},
            {"hideAfterLaunch", true},
            {"clearQueryOnShow", true},
            {"hideOnFocusLost", true},
            {"showTrayIcon", true},
            {"popupMonitor", "active"}
        }},
        {"behavior", {
            {"pinyinSearch", true},
            {"wildcardMatching", true},
            {"numericQuickLaunch", true},
            {"numericQuickLaunchOrder", "zero-to-nine"},
            {"executeSingleResultImmediately", false}
        }},
        {"appearance", {
            {"launcher", "modern-compact"},
            {"language", "en-US"},
            {"showResultIcons", true}
        }},
        {"providers", {
            {"windows.startmenu", true},
            {"windows.packaged", true},
            {"windows.apppaths", true},
            {"windows.path", true},
            {"everything.filesystem", false}
        }},
        {"update", {
            {"autoCheck", false},
            {"channel", "development"}
        }}
    };

    {
        std::ofstream output(
            path,
            std::ios::binary |
                std::ios::trunc);
        assert(output);
        output << source.dump(2);
    }

    SettingsStore store(path);
    store.Load();

    assert(
        store.WasMigratedFromOlderSchema());
    assert(
        store.MigratedFromSchemaVersion() ==
        7);
    assert(
        !store.IsReadOnlyDueToNewerSchema());

    AssertCommonFields(
        store.Data(),
        source);

    assert(
        store.Data().launcherPlacement ==
        "top");
    assert(
        store.Data().settingsPlacement ==
        "center");
    assert(
        !store.Data().launcherLastPositionValid);
    assert(
        !store.Data().settingsLastPositionValid);

    const auto migrated =
        nlohmann::json::parse(
            ReadText(path));

    assert(
        migrated.at("schemaVersion")
            .get<int>() ==
        config::kSettingsSchemaVersion);

    const auto& placement =
        migrated.at(
            "windowPlacement");

    assert(
        placement.at("launcherMode")
            .get<std::string>() ==
        "top");
    assert(
        placement.at("settingsMode")
            .get<std::string>() ==
        "center");
    assert(
        !placement.at("launcherLastValid")
             .get<bool>());
    assert(
        !placement.at("settingsLastValid")
             .get<bool>());

    AssertDowngradeReadOnly(path);
}

void AssertCleanInstall(
    const std::filesystem::path& workRoot) {
    const auto path =
        workRoot /
        "clean-install" /
        "settings.json";

    assert(!std::filesystem::exists(path));

    SettingsStore store(path);
    store.Load();

    assert(std::filesystem::exists(path));
    assert(
        !store.WasMigratedFromOlderSchema());
    assert(
        !store.IsReadOnlyDueToNewerSchema());

    const auto root =
        nlohmann::json::parse(
            ReadText(path));

    assert(
        root.at("schemaVersion")
            .get<int>() ==
        config::kSettingsSchemaVersion);
    assert(
        root.at("windowPlacement")
            .at("launcherMode")
            .get<std::string>() ==
        "top");
    assert(
        root.at("windowPlacement")
            .at("settingsMode")
            .get<std::string>() ==
        "center");
    assert(
        !root.at("windowPlacement")
             .at("launcherLastValid")
             .get<bool>());
    assert(
        !root.at("windowPlacement")
             .at("settingsLastValid")
             .get<bool>());
    assert(
        store.Data().launcherPlacement ==
        "top");
    assert(
        store.Data().settingsPlacement ==
        "center");

    assert(
        root.at("behavior")
            .at("pinyinSearch")
            .get<bool>());
    assert(
        !root.at("appearance")
             .at("showResultIcons")
             .get<bool>());
    assert(
        !store.Data().showResultIcons);
    assert(
        store.Data().autoCheckUpdates);
    assert(
        store.Data().updateChannel ==
        DefaultUpdateChannelForVersion(kVersion));
    assert(
        root.at("update")
            .at("autoCheck")
            .get<bool>());
    assert(
        root.at("update")
            .at("channel")
            .get<std::string>() ==
        ExpectedDefaultUpdateChannelName());

    const auto& bindings =
        root.at("hotkeys")
            .at("bindings");
    assert(
        bindings.size() ==
        HotkeyActionRegistry().size());

    const auto primary =
        EffectiveHotkeyBinding(
            store.Data().hotkeyBindings,
            hotkey_actions::kActivate);
    assert(primary.enabled);
    assert(
        primary.modifiers ==
        std::vector<std::string>{"alt"});
    assert(primary.key == "space");

    const auto secondary =
        EffectiveHotkeyBinding(
            store.Data().hotkeyBindings,
            hotkey_actions::kActivateSecondary);
    assert(!secondary.enabled);
    assert(secondary.key == "pause");

    const auto openSettings =
        EffectiveHotkeyBinding(
            store.Data().hotkeyBindings,
            hotkey_actions::kOpenSettings);
    assert(openSettings.enabled);
    assert(openSettings.key == "f2");

    const auto navigate =
        EffectiveHotkeyBinding(
            store.Data().hotkeyBindings,
            hotkey_actions::
                kNavigateCurrentFileManager);
    assert(navigate.enabled);
    assert(
        navigate.modifiers ==
        std::vector<std::string>{"ctrl"});
    assert(navigate.key == "enter");

    const auto copyTarget =
        EffectiveHotkeyBinding(
            store.Data().hotkeyBindings,
            hotkey_actions::
                kCopySelectedTarget);
    assert(copyTarget.enabled);
    assert(
        copyTarget.modifiers ==
        (std::vector<std::string>{
            "ctrl", "shift"}));
    assert(copyTarget.key == "c");

    assert(providers::IsEnabled(
        store.Data().providerEnabled,
        providers::kStartMenu));
    assert(providers::IsEnabled(
        store.Data().providerEnabled,
        providers::kPackaged));
    assert(providers::IsEnabled(
        store.Data().providerEnabled,
        providers::kAppPaths));
    assert(providers::IsEnabled(
        store.Data().providerEnabled,
        providers::kPath));
    assert(!providers::IsEnabled(
        store.Data().providerEnabled,
        providers::kEverythingFilesystem,
        false));
}

} // namespace

int main(
    int argc,
    char** argv) {
    assert(argc == 2);

    const std::filesystem::path
        fixtureRoot = argv[1];

    const auto nonce =
        std::chrono::high_resolution_clock::
            now()
            .time_since_epoch()
            .count();

    const auto workRoot =
        std::filesystem::temp_directory_path() /
        ("altrun-upgrade-matrix-" +
         std::to_string(nonce));

    std::filesystem::create_directories(
        workRoot);

    AssertCleanInstall(workRoot);
    AssertSchema7Migration(workRoot);
    AssertSchema6Migration(workRoot);
    AssertSchema5Migration(workRoot);

    AssertSchema3Migration(
        fixtureRoot,
        "v0.5.0-schema3.json",
        workRoot,
        false);

    AssertSchema3Migration(
        fixtureRoot,
        "v0.6.0-alpha.5-schema3-conflict.json",
        workRoot,
        true);

    for (const auto fixture :
         std::array<std::string_view, 3>{
             "v0.6.0-alpha.6.1-schema4.json",
             "v0.6.0-beta.1-schema4.json",
             "v0.6.0-beta.2-schema4.json"}) {
        AssertSchema4Migration(
            fixtureRoot,
            fixture,
            workRoot);
    }

    std::filesystem::remove_all(
        workRoot);

    std::cout
        << "Upgrade matrix tests passed: clean install, "
           "schema 3/4/5/6/7 -> 8 with placement/update defaults, "
           "schema downgrade read-only\n";

    return 0;
}
