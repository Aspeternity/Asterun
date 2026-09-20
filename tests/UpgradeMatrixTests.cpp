#include "core/ConfigIO.hpp"
#include "core/HotkeyRegistry.hpp"
#include "core/ProviderIds.hpp"
#include "core/Settings.hpp"

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
    assert(bindings.contains(
        std::string(actionId)));

    const auto& expected =
        bindings[std::string(actionId)];
    const auto actual =
        EffectiveHotkeyBinding(
            settings.hotkeyBindings,
            actionId);

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
        general.at("startWithWindows").get<bool>());
    assert(
        settings.showOnStartup ==
        general.at("showOnStartup").get<bool>());
    assert(
        settings.hideAfterLaunch ==
        general.at("hideAfterLaunch").get<bool>());
    assert(
        settings.clearQueryOnShow ==
        general.at("clearQueryOnShow").get<bool>());
    assert(
        settings.hideOnFocusLost ==
        general.at("hideOnFocusLost").get<bool>());
    assert(
        settings.showTrayIcon ==
        general.at("showTrayIcon").get<bool>());
    assert(
        settings.popupMonitor ==
        general.at("popupMonitor").get<std::string>());

    const auto& behavior =
        root.at("behavior");
    assert(
        settings.wildcardMatching ==
        behavior.at("wildcardMatching").get<bool>());
    assert(
        settings.numericQuickLaunch ==
        behavior.at("numericQuickLaunch").get<bool>());
    assert(
        settings.numericQuickLaunchOrder ==
        behavior.at("numericQuickLaunchOrder")
            .get<std::string>());
    assert(
        settings.executeSingleResultImmediately ==
        behavior.at("executeSingleResultImmediately")
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
            3);

    assert(
        downgrade.status ==
        config::JsonLoadStatus::
            UnsupportedSchema);
    assert(downgrade.schemaVersion == 4);
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
        4);
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

void AssertSchema4Compatibility(
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

    const std::string before =
        ReadText(path);
    const auto source =
        nlohmann::json::parse(before);

    SettingsStore store(path);
    store.Load();

    assert(
        !store.WasMigratedFromOlderSchema());
    assert(
        !store.IsReadOnlyDueToNewerSchema());

    // Schema 4 -> 4 must be a true compatibility load. Merely opening the
    // file must not normalize, reorder or otherwise rewrite user settings.
    assert(ReadText(path) == before);

    AssertCommonFields(
        store.Data(),
        source);

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
        4);

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
        AssertSchema4Compatibility(
            fixtureRoot,
            fixture,
            workRoot);
    }

    std::filesystem::remove_all(
        workRoot);

    std::cout
        << "Upgrade matrix tests passed: clean install, "
           "v0.5.0/alpha.5 schema 3 -> 4, "
           "alpha.6.1/beta.1/beta.2 schema 4 -> 4, "
           "downgrade read-only\n";

    return 0;
}
