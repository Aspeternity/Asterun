#include "core/ConfigIO.hpp"
#include "core/ProviderCache.hpp"
#include "core/ProviderFingerprint.hpp"
#include "core/Settings.hpp"
#include "core/SettingsLayout.hpp"
#include "core/UsageStore.hpp"
#include "core/UserCommandStore.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <string_view>

using namespace altrun;

namespace {

void WriteText(const std::filesystem::path& path, const std::string& text) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    assert(out);
    out << text;
}

std::string ReadText(
    const std::filesystem::path& path) {

    std::ifstream input(
        path,
        std::ios::binary);

    assert(input);

    return std::string(
        std::istreambuf_iterator<char>(
            input),
        std::istreambuf_iterator<char>());
}

} // namespace

int main() {
    {
        const settings_layout::Rect work{
            0,
            0,
            1920,
            1040,
        };

        const auto centered =
            settings_layout::
                ResolveWindowOrigin(
                    work,
                    820,
                    620,
                    false,
                    45);
        assert(centered.x == 550);
        assert(centered.y == 210);

        const auto nearTop =
            settings_layout::
                ResolveWindowOrigin(
                    work,
                    820,
                    620,
                    true,
                    45);
        assert(nearTop.x == 550);
        assert(nearTop.y == 84);

        const auto constrained =
            settings_layout::
                ResolveWindowOrigin(
                    {100, 50, 700, 450},
                    720,
                    480,
                    true,
                    45);
        assert(constrained.x == 100);
        assert(constrained.y == 50);
    }

    const auto fingerprintA =
        fingerprint::Hash(
            std::wstring_view(
                L"provider-a"));
    const auto fingerprintA2 =
        fingerprint::Hash(
            std::wstring_view(
                L"provider-a"));
    const auto fingerprintB =
        fingerprint::Hash(
            std::wstring_view(
                L"provider-b"));

    assert(fingerprintA ==
        fingerprintA2);
    assert(fingerprintA !=
        fingerprintB);

    std::uint64_t mixedFingerprint =
        fingerprint::kOffset;
    fingerprint::Mix(
        mixedFingerprint,
        std::string_view(
            "windows.startmenu"));
    fingerprint::Mix(
        mixedFingerprint,
        std::uint64_t{42});
    assert(mixedFingerprint !=
        fingerprint::kOffset);

    const auto nonce =
        std::chrono::high_resolution_clock::now().time_since_epoch().count();

    const auto root =
        std::filesystem::temp_directory_path() /
        ("altrun-config-test-" + std::to_string(nonce));

    const auto data = root / "data";
    std::filesystem::create_directories(data);

    const auto legacySettings = root / "settings.ini";
    const auto legacyCommands = root / "commands.tsv";
    const auto legacyUsage = root / "usage.tsv";

    WriteText(
        legacySettings,
        "[general]\n"
        "ui=modern-compact\n"
        "language=en-US\n");

    WriteText(
        legacyCommands,
        "calc\tCalculator\tcalc.exe\t\t\n"
        "wx\tWeixin\tC:\\\\Apps\\\\Weixin.exe\t--test\tC:\\\\Apps\n");

    WriteText(
        legacyUsage,
        "custom:calc:calc.exe\t7\t1700000000\n"
        "start:legacy:legacy.lnk\t2\t1600000000\n");

    SettingsStore settings(data / "settings.json", legacySettings);
    settings.Load();

    assert(settings.Data().uiStyle == UiStyle::ModernCompact);
    assert(settings.Data().language == Language::EnUS);
    assert(std::filesystem::exists(data / "settings.json"));

    ProviderEnableMap providerBatch{
        {std::string(providers::kStartMenu), false},
        {std::string(providers::kPath), false},
    };
    assert(settings.SetProviderEnabledBatch(providerBatch));
    assert(!providers::IsEnabled(
        settings.Data().providerEnabled,
        providers::kStartMenu,
        true));
    assert(!providers::IsEnabled(
        settings.Data().providerEnabled,
        providers::kPath,
        true));

    SettingsStore providerBatchReloaded(
        data / "settings.json",
        legacySettings);
    providerBatchReloaded.Load();
    assert(!providers::IsEnabled(
        providerBatchReloaded.Data().providerEnabled,
        providers::kStartMenu,
        true));
    assert(!providers::IsEnabled(
        providerBatchReloaded.Data().providerEnabled,
        providers::kPath,
        true));

    UserCommandStore commands(data / "commands.json", legacyCommands);
    commands.Load();

    assert(commands.Commands().size() == 2);
    assert(std::filesystem::exists(data / "commands.json"));

    const auto calcId = commands.Commands()[0].id;
    assert(!calcId.empty());
    assert(calcId != L"custom:calc:calc.exe");

    const auto& map = commands.LegacyIdMap();
    const auto mapped = map.find(L"custom:calc:calc.exe");
    assert(mapped != map.end());
    assert(mapped->second == calcId);

    UserCommandStore commandsReloaded(data / "commands.json", legacyCommands);
    commandsReloaded.Load();
    assert(commandsReloaded.Commands().size() == 2);
    assert(commandsReloaded.Commands()[0].id == calcId);

    Command custom;
    custom.title = L"Telegram";
    custom.keyword = L"tg";
    custom.aliases = {L"telegram"};
    custom.target = L"C:\\Apps\\Telegram.exe";
    custom.enabled = true;
    custom.pinned = true;

    std::wstring createdId;
    assert(commandsReloaded.Create(custom, &createdId));
    assert(!createdId.empty());
    assert(commandsReloaded.Commands().size() == 3);

    custom.title = L"Telegram Desktop";
    custom.arguments = L"--test";
    custom.enabled = false;
    assert(commandsReloaded.Update(createdId, custom));

    const auto updated = std::find_if(
        commandsReloaded.Commands().begin(),
        commandsReloaded.Commands().end(),
        [&](const Command& command) {
            return command.id == createdId;
        });
    assert(updated != commandsReloaded.Commands().end());
    assert(updated->title == L"Telegram Desktop");
    assert(updated->arguments == L"--test");
    assert(updated->enabled);
    assert(!updated->pinned);

    assert(commandsReloaded.Move(createdId, -1));

    UserCommandStore crudReloaded(data / "commands.json", legacyCommands);
    crudReloaded.Load();
    const auto persisted = std::find_if(
        crudReloaded.Commands().begin(),
        crudReloaded.Commands().end(),
        [&](const Command& command) {
            return command.id == createdId;
        });
    assert(persisted != crudReloaded.Commands().end());
    assert(persisted->title == L"Telegram Desktop");
    assert(persisted->enabled);
    assert(!persisted->pinned);

    assert(crudReloaded.Remove(createdId));
    assert(crudReloaded.Commands().size() == 2);

    const auto exportedCommands = root / "commands-export.tsv";
    assert(crudReloaded.ExportTsv(exportedCommands));
    assert(std::filesystem::exists(exportedCommands));

    UserCommandStore importedCommands(
        data / "commands-imported.json");
    std::size_t importedCount = 0;
    std::size_t skippedCount = 0;
    assert(importedCommands.ImportTsv(
        exportedCommands,
        &importedCount,
        &skippedCount));
    assert(importedCount == 2);
    assert(skippedCount == 0);

    UserCommandStore importedReloaded(
        data / "commands-imported.json");
    importedReloaded.Load();
    assert(importedReloaded.Commands().size() == 2);

    UsageStore usage(data / "usage.json", legacyUsage);
    usage.Load(commandsReloaded.LegacyIdMap());

    assert(std::filesystem::exists(data / "usage.json"));
    assert(usage.Data().at(calcId).launches == 7);
    assert(usage.Data().at(calcId).lastUsedUnix == 1700000000);

    assert(usage.Clear());
    assert(usage.Data().empty());

    UsageStore clearedUsage(data / "usage.json", legacyUsage);
    clearedUsage.Load(commandsReloaded.LegacyIdMap());
    assert(clearedUsage.Data().empty());

    settings.SetLanguage(Language::ZhCN);
    assert(std::filesystem::exists(data / "settings.json.bak"));

    WriteText(data / "settings.json", "{ broken json");

    SettingsStore recovered(
        data / "settings.json",
        legacySettings);

    recovered.Load();

    assert(
        recovered.Data().language ==
        Language::EnUS);
    assert(
        recovered
            .WasRecoveredFromBackup());

    // Backup recovery now self-heals the corrupt primary while preserving the
    // known-good backup instead of copying corrupt bytes over it.
    const auto healedSettings =
        config::LoadJsonWithBackup(
            data / "settings.json",
            config::kSettingsSchemaVersion);

    assert(
        healedSettings.status ==
        config::JsonLoadStatus::
            LoadedPrimary);
    assert(healedSettings.value);

    const auto healedBackup =
        config::LoadJsonWithBackup(
            data / "settings.json.bak",
            config::kSettingsSchemaVersion);

    assert(healedBackup.value);

    SettingsStore featureSettings(data / "settings-features.json");
    featureSettings.Load();

    const auto initialFeatureSettings =
        config::LoadJsonWithBackup(
            data / "settings-features.json",
            config::kSettingsSchemaVersion);

    assert(initialFeatureSettings.value);
    assert(
        initialFeatureSettings.schemaVersion ==
        config::kSettingsSchemaVersion);

    assert(!featureSettings.Data().startWithWindows);
    assert(
        featureSettings.Data()
            .startupBehavior ==
        StartupBehavior::Notification);
    assert(featureSettings.Data().showTrayIcon);
    assert(!featureSettings.Data().addToSendToMenu);
    assert(!featureSettings.Data().auxiliaryHotkeyEnabled);
    assert(featureSettings.Data().auxiliaryHotkeyKey == "pause");
    assert(featureSettings.Data().pinyinSearch);
    assert(!featureSettings.Data().showResultIcons);
    assert(!featureSettings.Data().numericQuickLaunch);
    assert(
        !featureSettings.Data()
             .executeSingleResultImmediately);
    assert(
        featureSettings.Data()
            .launcherPlacement ==
        "top");
    assert(
        featureSettings.Data()
            .settingsPlacement ==
        "center");
    assert(
        featureSettings.Data()
            .shortcutManagerPlacement ==
        "center");

    assert(featureSettings.SetStartWithWindows(true));
    assert(featureSettings.Data().startWithWindows);

    assert(featureSettings.SetStartupBehavior(
        StartupBehavior::ShowLauncher));
    assert(
        featureSettings.Data()
            .startupBehavior ==
        StartupBehavior::ShowLauncher);
    assert(featureSettings.SetAddToSendToMenu(true));
    assert(featureSettings.Data().addToSendToMenu);

    assert(featureSettings.SetShowResultIcons(true));
    assert(featureSettings.Data().showResultIcons);

    assert(featureSettings.SetHotkey(
        {"ctrl", "shift"},
        "k"));
    assert(featureSettings.Data().hotkeyModifiers.size() == 2);
    assert(featureSettings.Data().hotkeyKey == "k");

    assert(featureSettings.SetAuxiliaryHotkey(
        true,
        {},
        "pause"));
    assert(
        featureSettings.Data()
            .auxiliaryHotkeyEnabled);
    assert(
        featureSettings.Data()
            .auxiliaryHotkeyModifiers
            .empty());
    assert(
        featureSettings.Data()
            .auxiliaryHotkeyKey ==
        "pause");

    assert(featureSettings.SetClassicBehavior(
        true,
        true,
        false));
    assert(!featureSettings.Data().pinyinSearch);
    assert(featureSettings.Data().numericQuickLaunch);
    assert(
        featureSettings.Data()
            .executeSingleResultImmediately);

    assert(featureSettings.SetClassicBehavior(
        false,
        false,
        true));
    assert(featureSettings.Data().pinyinSearch);
    assert(
        !featureSettings.Data()
             .numericQuickLaunch);
    assert(
        !featureSettings.Data()
             .executeSingleResultImmediately);

    assert(featureSettings.SetClassicBehavior(
        false,
        false,
        true));

    // Restore the enabled matrix used by the reload assertions below.
    assert(featureSettings.SetClassicBehavior(
        true,
        true,
        false));

    assert(featureSettings.SetWindowPlacement(
        "last",
        "top",
        "last"));
    assert(featureSettings.RememberLauncherPosition(
        111,
        222));
    assert(featureSettings.RememberSettingsPosition(
        333,
        444));
    assert(featureSettings.RememberShortcutManagerPosition(
        555,
        666));

    assert(providers::IsEnabled(
        featureSettings.Data().providerEnabled,
        providers::kStartMenu));
    assert(providers::IsEnabled(
        featureSettings.Data().providerEnabled,
        providers::kPackaged));
    assert(providers::IsEnabled(
        featureSettings.Data().providerEnabled,
        providers::kAppPaths));
    assert(!providers::IsEnabled(
        featureSettings.Data().providerEnabled,
        providers::kPath));
    assert(!providers::IsEnabled(
        featureSettings.Data().providerEnabled,
        providers::kEverythingFilesystem,
        false));

    assert(featureSettings.SetProviderEnabled(
        std::string(providers::kPath),
        false));
    assert(!providers::IsEnabled(
        featureSettings.Data().providerEnabled,
        providers::kPath));

    SettingsStore providerSettingsReloaded(
        data / "settings-features.json");
    providerSettingsReloaded.Load();
    assert(!providers::IsEnabled(
        providerSettingsReloaded.Data().providerEnabled,
        providers::kPath));
    assert(
        providerSettingsReloaded.Data()
            .startupBehavior ==
        StartupBehavior::ShowLauncher);
    assert(
        providerSettingsReloaded.Data()
            .addToSendToMenu);
    assert(
        providerSettingsReloaded.Data()
            .showResultIcons);
    assert(
        providerSettingsReloaded.Data()
            .auxiliaryHotkeyEnabled);
    assert(
        providerSettingsReloaded.Data()
            .auxiliaryHotkeyModifiers
            .empty());
    assert(
        providerSettingsReloaded.Data()
            .auxiliaryHotkeyKey ==
        "pause");
    assert(
        !providerSettingsReloaded.Data()
             .pinyinSearch);
    assert(
        providerSettingsReloaded.Data()
            .numericQuickLaunch);
    assert(
        providerSettingsReloaded.Data()
            .executeSingleResultImmediately);
    assert(
        providerSettingsReloaded.Data()
            .launcherPlacement ==
        "last");
    assert(
        providerSettingsReloaded.Data()
            .settingsPlacement ==
        "top");
    assert(
        providerSettingsReloaded.Data()
            .shortcutManagerPlacement ==
        "last");
    assert(
        providerSettingsReloaded.Data()
            .launcherLastPositionValid);
    assert(
        providerSettingsReloaded.Data()
            .launcherLastX == 111);
    assert(
        providerSettingsReloaded.Data()
            .launcherLastY == 222);
    assert(
        providerSettingsReloaded.Data()
            .settingsLastPositionValid);
    assert(
        providerSettingsReloaded.Data()
            .settingsLastX == 333);
    assert(
        providerSettingsReloaded.Data()
            .settingsLastY == 444);
    assert(
        providerSettingsReloaded.Data()
            .shortcutManagerLastPositionValid);
    assert(
        providerSettingsReloaded.Data()
            .shortcutManagerLastX == 555);
    assert(
        providerSettingsReloaded.Data()
            .shortcutManagerLastY == 666);

    // Schema 9 exposed launcher lifecycle details as user settings.
    // Schema 10 converts only the meaningful startup choice and drops the
    // pseudo-settings from the rewritten document.
    const auto schema9BehaviorPath =
        data / "settings-schema9-behavior-cleanup.json";

    WriteText(
        schema9BehaviorPath,
        "{\n"
        "  \"schemaVersion\": 9,\n"
        "  \"general\": {\n"
        "    \"startWithWindows\": false,\n"
        "    \"showOnStartup\": true,\n"
        "    \"hideAfterLaunch\": false,\n"
        "    \"clearQueryOnShow\": false,\n"
        "    \"hideOnFocusLost\": false,\n"
        "    \"showTrayIcon\": true,\n"
        "    \"popupMonitor\": \"cursor\"\n"
        "  },\n"
        "  \"behavior\": {\n"
        "    \"pinyinSearch\": true,\n"
        "    \"wildcardMatching\": false,\n"
        "    \"numericQuickLaunch\": true,\n"
        "    \"numericQuickLaunchOrder\": \"zero-to-nine\",\n"
        "    \"executeSingleResultImmediately\": false\n"
        "  }\n"
        "}\n");

    SettingsStore schema9Behavior(
        schema9BehaviorPath);
    schema9Behavior.Load();

    assert(schema9Behavior.WasMigratedFromOlderSchema());
    assert(schema9Behavior.MigratedFromSchemaVersion() == 9);
    assert(
        schema9Behavior.Data().startupBehavior ==
        StartupBehavior::ShowLauncher);
    assert(schema9Behavior.Data().numericQuickLaunch);

    const std::string schema10Text =
        ReadText(schema9BehaviorPath);
    assert(schema10Text.find("\"startupBehavior\"") !=
        std::string::npos);
    assert(schema10Text.find("\"showOnStartup\"") ==
        std::string::npos);
    assert(schema10Text.find("\"hideAfterLaunch\"") ==
        std::string::npos);
    assert(schema10Text.find("\"clearQueryOnShow\"") ==
        std::string::npos);
    assert(schema10Text.find("\"hideOnFocusLost\"") ==
        std::string::npos);
    assert(schema10Text.find("\"wildcardMatching\"") ==
        std::string::npos);
    assert(schema10Text.find("\"numericQuickLaunchOrder\"") ==
        std::string::npos);

    // schema-8 window-placement settings migrate through to the current schema. Existing
    // launcher/settings choices survive; Shortcut Manager receives the new
    // centered default and an invalid last-position marker.
    const auto schema8PlacementPath =
        data /
        "settings-schema8-window-placement.json";

    WriteText(
        schema8PlacementPath,
        "{\n"
        "  \"schemaVersion\": 8,\n"
        "  \"windowPlacement\": {\n"
        "    \"launcherMode\": \"last\",\n"
        "    \"settingsMode\": \"last\",\n"
        "    \"launcherLastValid\": true,\n"
        "    \"launcherLastX\": 10,\n"
        "    \"launcherLastY\": 20,\n"
        "    \"settingsLastValid\": true,\n"
        "    \"settingsLastX\": 30,\n"
        "    \"settingsLastY\": 40\n"
        "  }\n"
        "}\n");

    SettingsStore schema8Placement(
        schema8PlacementPath);
    schema8Placement.Load();

    assert(
        schema8Placement
            .WasMigratedFromOlderSchema());
    assert(
        schema8Placement
            .MigratedFromSchemaVersion() ==
        8);
    assert(
        schema8Placement.Data()
            .launcherPlacement ==
        "last");
    assert(
        schema8Placement.Data()
            .settingsPlacement ==
        "last");
    assert(
        schema8Placement.Data()
            .shortcutManagerPlacement ==
        "center");
    assert(
        !schema8Placement.Data()
             .shortcutManagerLastPositionValid);

    const auto migratedPlacementJson =
        config::LoadJsonWithBackup(
            schema8PlacementPath,
            config::kSettingsSchemaVersion);
    assert(migratedPlacementJson.value);
    assert(
        migratedPlacementJson.schemaVersion ==
        config::kSettingsSchemaVersion);
    assert(
        (*migratedPlacementJson.value)
            ["windowPlacement"]
            ["shortcutManagerMode"]
            .get<std::string>() ==
        "center");
    assert(
        !(*migratedPlacementJson.value)
             ["windowPlacement"]
             ["shortcutManagerLastValid"]
             .get<bool>());

    // New installs default PATH off, but an existing settings document
    // that predates an explicit windows.path key keeps the historical true.
    const auto legacyImplicitPath =
        data /
        "settings-legacy-implicit-path.json";

    WriteText(
        legacyImplicitPath,
        "{\n"
        "  \"schemaVersion\": 10,\n"
        "  \"providers\": {\n"
        "    \"windows.startmenu\": true\n"
        "  }\n"
        "}\n");

    SettingsStore legacyPathSettings(
        legacyImplicitPath);
    legacyPathSettings.Load();

    assert(providers::IsEnabled(
        legacyPathSettings.Data()
            .providerEnabled,
        providers::kPath));

    // Every provider toggle combination must survive a save/reload cycle.
    const std::array<std::string_view, 5>
        providerIds{
            providers::kStartMenu,
            providers::kPackaged,
            providers::kAppPaths,
            providers::kPath,
            providers::kEverythingFilesystem,
        };

    for (unsigned mask = 0;
         mask < 32;
         ++mask) {

        const auto matrixPath =
            data /
            ("settings-provider-matrix-" +
             std::to_string(mask) +
             ".json");

        SettingsStore matrix(
            matrixPath);

        matrix.Load();

        for (std::size_t i = 0;
             i < providerIds.size();
             ++i) {

            const bool enabled =
                (mask &
                 (1u <<
                  static_cast<unsigned>(i))) != 0;

            assert(matrix.SetProviderEnabled(
                std::string(
                    providerIds[i]),
                enabled));
        }

        SettingsStore reloaded(
            matrixPath);

        reloaded.Load();

        for (std::size_t i = 0;
             i < providerIds.size();
             ++i) {

            const bool expected =
                (mask &
                 (1u <<
                  static_cast<unsigned>(i))) != 0;

            assert(
                providers::IsEnabled(
                    reloaded.Data()
                        .providerEnabled,
                    providerIds[i]) ==
                expected);
        }
    }

    // v0.5 alpha schema-2 settings migrate in place to schema 3.
    // An experimental Everything opt-in must survive the migration.
    const auto v2EverythingSettings =
        data /
        "settings-v0.5-alpha-everything.json";

    WriteText(
        v2EverythingSettings,
        "{\n"
        "  \"schemaVersion\": 2,\n"
        "  \"providers\": {\n"
        "    \"windows.startmenu\": true,\n"
        "    \"windows.packaged\": true,\n"
        "    \"windows.apppaths\": true,\n"
        "    \"windows.path\": true,\n"
        "    \"everything.filesystem\": true\n"
        "  }\n"
        "}\n");

    SettingsStore migratedEverything(
        v2EverythingSettings);
    migratedEverything.Load();

    assert(
        migratedEverything
            .WasMigratedFromOlderSchema());
    assert(
        migratedEverything
            .MigratedFromSchemaVersion() ==
        2);
    assert(providers::IsEnabled(
        migratedEverything.Data()
            .providerEnabled,
        providers::kEverythingFilesystem,
        false));

    const auto migratedEverythingJson =
        config::LoadJsonWithBackup(
            v2EverythingSettings,
            config::kSettingsSchemaVersion);

    assert(migratedEverythingJson.value);
    assert(
        migratedEverythingJson.schemaVersion ==
        config::kSettingsSchemaVersion);
    assert(
        (*migratedEverythingJson.value)
            ["providers"]
            ["everything.filesystem"]
            .get<bool>());

    // Simulate v0.4.1 (schema ceiling 2) opening a beta schema-3 file.
    // Downgrade must be read-only and must not alter the newer document.
    const std::string betaV3BeforeDowngrade =
        ReadText(v2EverythingSettings);

    const auto v041DowngradeRead =
        config::LoadJsonWithBackup(
            v2EverythingSettings,
            2);

    assert(
        v041DowngradeRead.status ==
        config::JsonLoadStatus::
            UnsupportedSchema);
    assert(
        v041DowngradeRead.schemaVersion ==
        config::kSettingsSchemaVersion);
    assert(v041DowngradeRead.value);
    assert(
        ReadText(v2EverythingSettings) ==
        betaV3BeforeDowngrade);

    // Ordinary alpha users that never opted into Everything migrate to
    // schema 3 with the new dynamic provider safely disabled.
    const auto v2DefaultSettings =
        data /
        "settings-v0.5-alpha-default.json";

    WriteText(
        v2DefaultSettings,
        "{\n"
        "  \"schemaVersion\": 2,\n"
        "  \"providers\": {\n"
        "    \"windows.startmenu\": true,\n"
        "    \"windows.packaged\": true,\n"
        "    \"windows.apppaths\": true,\n"
        "    \"windows.path\": true\n"
        "  }\n"
        "}\n");

    SettingsStore migratedDefault(
        v2DefaultSettings);
    migratedDefault.Load();

    assert(
        migratedDefault
            .WasMigratedFromOlderSchema());
    assert(
        migratedDefault
            .MigratedFromSchemaVersion() ==
        2);
    assert(!providers::IsEnabled(
        migratedDefault.Data()
            .providerEnabled,
        providers::kEverythingFilesystem,
        false));

    const auto migratedDefaultJson =
        config::LoadJsonWithBackup(
            v2DefaultSettings,
            config::kSettingsSchemaVersion);
    assert(migratedDefaultJson.value);
    assert(
        migratedDefaultJson.schemaVersion ==
        config::kSettingsSchemaVersion);
    assert(
        !(*migratedDefaultJson.value)
             ["providers"]
             ["everything.filesystem"]
             .get<bool>());

    // v0.6 alpha.5 schema-3 settings migrate to the centralized
    // schema-4 Hotkey Registry. Existing primary/auxiliary values survive,
    // while launcher-local actions receive their published defaults.
    const auto v3HotkeySettings =
        data /
        "settings-v0.6-alpha5-hotkeys.json";

    WriteText(
        v3HotkeySettings,
        "{\n"
        "  \"schemaVersion\": 3,\n"
        "  \"hotkey\": {\n"
        "    \"modifiers\": [\"ctrl\", \"shift\"],\n"
        "    \"key\": \"k\",\n"
        "    \"auxiliary\": {\n"
        "      \"enabled\": true,\n"
        "      \"modifiers\": [],\n"
        "      \"key\": \"pause\"\n"
        "    }\n"
        "  }\n"
        "}\n");

    SettingsStore migratedHotkeys(
        v3HotkeySettings);
    migratedHotkeys.Load();

    assert(
        migratedHotkeys
            .WasMigratedFromOlderSchema());
    assert(
        migratedHotkeys
            .MigratedFromSchemaVersion() ==
        3);

    const auto migratedPrimary =
        EffectiveHotkeyBinding(
            migratedHotkeys.Data()
                .hotkeyBindings,
            hotkey_actions::kActivate);

    assert(migratedPrimary.enabled);
    assert(
        migratedPrimary.modifiers ==
        (std::vector<std::string>{
            "ctrl", "shift"}));
    assert(migratedPrimary.key == "k");

    const auto migratedAuxiliary =
        EffectiveHotkeyBinding(
            migratedHotkeys.Data()
                .hotkeyBindings,
            hotkey_actions::
                kActivateSecondary);

    assert(migratedAuxiliary.enabled);
    assert(
        migratedAuxiliary.modifiers
            .empty());
    assert(
        migratedAuxiliary.key ==
        "pause");

    const auto migratedSettingsAction =
        EffectiveHotkeyBinding(
            migratedHotkeys.Data()
                .hotkeyBindings,
            hotkey_actions::
                kOpenSettings);

    assert(migratedSettingsAction.enabled);
    assert(
        migratedSettingsAction.key ==
        "f2");

    const auto migratedHotkeyJson =
        config::LoadJsonWithBackup(
            v3HotkeySettings,
            config::kSettingsSchemaVersion);

    assert(migratedHotkeyJson.value);
    assert(
        migratedHotkeyJson.schemaVersion ==
        config::kSettingsSchemaVersion);
    assert(
        (*migratedHotkeyJson.value)
            .contains("hotkeys"));
    assert(
        (*migratedHotkeyJson.value)
            ["hotkeys"]
            ["bindings"]
            .contains(
                std::string(
                    hotkey_actions::
                        kCopySelectedTarget)));

    // The schema-3 compatibility mirror is deliberately retained so an
    // alpha.5 downgrade can read the user's primary/auxiliary values while
    // still treating the newer migrated document as read-only.
    assert(
        (*migratedHotkeyJson.value)
            ["hotkey"]
            ["key"]
            .get<std::string>() ==
        "k");

    const std::string
        newerSchemaBeforeDowngrade =
            ReadText(
                v3HotkeySettings);

    const auto alpha5DowngradeRead =
        config::LoadJsonWithBackup(
            v3HotkeySettings,
            3);

    assert(
        alpha5DowngradeRead.status ==
        config::JsonLoadStatus::
            UnsupportedSchema);
    assert(
        alpha5DowngradeRead.schemaVersion ==
        config::kSettingsSchemaVersion);
    assert(
        ReadText(
            v3HotkeySettings) ==
        newerSchemaBeforeDowngrade);

    // A schema-3 global binding may legitimately use a chord that alpha.6
    // introduces as a new launcher-local default. The established user global
    // binding wins; the conflicting new optional action is disabled.
    const auto v3ConflictSettings =
        data /
        "settings-v0.6-alpha5-hotkey-conflict.json";

    WriteText(
        v3ConflictSettings,
        "{\n"
        "  \"schemaVersion\": 3,\n"
        "  \"hotkey\": {\n"
        "    \"modifiers\": [\"ctrl\"],\n"
        "    \"key\": \"enter\",\n"
        "    \"auxiliary\": {\n"
        "      \"enabled\": false,\n"
        "      \"modifiers\": [],\n"
        "      \"key\": \"pause\"\n"
        "    }\n"
        "  }\n"
        "}\n");

    SettingsStore migratedConflict(
        v3ConflictSettings);
    migratedConflict.Load();

    const auto conflictPrimary =
        EffectiveHotkeyBinding(
            migratedConflict.Data()
                .hotkeyBindings,
            hotkey_actions::kActivate);
    const auto conflictNavigate =
        EffectiveHotkeyBinding(
            migratedConflict.Data()
                .hotkeyBindings,
            hotkey_actions::
                kNavigateCurrentFileManager);

    assert(conflictPrimary.enabled);
    assert(conflictPrimary.key == "enter");
    assert(
        conflictPrimary.modifiers ==
        std::vector<std::string>{"ctrl"});
    assert(
        !conflictNavigate.enabled);

    const auto conflictJson =
        config::LoadJsonWithBackup(
            v3ConflictSettings,
            config::kSettingsSchemaVersion);

    assert(conflictJson.value);
    assert(
        !(*conflictJson.value)
             ["hotkeys"]
             ["bindings"]
             ["result.navigateCurrentFileManager"]
             ["enabled"]
             .get<bool>());

    // Alpha-era settings without a providers object retain the current
    // default-enabled behavior for all discovery sources.
    const auto alphaSettings =
        data /
        "settings-alpha-no-providers.json";

    WriteText(
        alphaSettings,
        "{\n"
        "  \"schemaVersion\": 1,\n"
        "  \"general\": {\n"
        "    \"startWithWindows\": true\n"
        "  },\n"
        "  \"appearance\": {\n"
        "    \"launcher\": \"classic\",\n"
        "    \"language\": \"en-US\"\n"
        "  }\n"
        "}\n");

    SettingsStore alphaReloaded(
        alphaSettings);

    alphaReloaded.Load();

    assert(
        alphaReloaded.Data()
            .startWithWindows);
    assert(
        alphaReloaded.Data()
            .language ==
        Language::EnUS);

    for (const auto providerId :
         std::array<std::string_view, 4>{
             providers::kStartMenu,
             providers::kPackaged,
             providers::kAppPaths,
             providers::kPath}) {
        assert(providers::IsEnabled(
            alphaReloaded.Data()
                .providerEnabled,
            providerId));
    }
    assert(!providers::IsEnabled(
        alphaReloaded.Data()
            .providerEnabled,
        providers::kEverythingFilesystem,
        false));

    assert(
        !alphaReloaded.Data()
             .numericQuickLaunch);

    const auto upgradedAlphaSettings =
        config::LoadJsonWithBackup(
            alphaSettings,
            config::kSettingsSchemaVersion);

    assert(upgradedAlphaSettings.value);
    assert(
        upgradedAlphaSettings
            .schemaVersion ==
        config::kSettingsSchemaVersion);

    // Representative v0.4.0 schema-1 settings must preserve every known
    // preference while v0.4.1 supplies safe defaults for its new fields.
    const auto v040SettingsPath =
        data /
        "settings-v0.4.0.json";

    WriteText(
        v040SettingsPath,
        "{\n"
        "  \"schemaVersion\": 1,\n"
        "  \"general\": {\n"
        "    \"startWithWindows\": true,\n"
        "    \"hideAfterLaunch\": false,\n"
        "    \"clearQueryOnShow\": false,\n"
        "    \"hideOnFocusLost\": false,\n"
        "    \"showTrayIcon\": false,\n"
        "    \"popupMonitor\": \"primary\"\n"
        "  },\n"
        "  \"hotkey\": {\n"
        "    \"modifiers\": [\"ctrl\", \"shift\"],\n"
        "    \"key\": \"f12\"\n"
        "  },\n"
        "  \"appearance\": {\n"
        "    \"launcher\": \"modern-compact\",\n"
        "    \"language\": \"en-US\"\n"
        "  },\n"
        "  \"providers\": {\n"
        "    \"windows.startmenu\": true,\n"
        "    \"windows.packaged\": false,\n"
        "    \"windows.apppaths\": true,\n"
        "    \"windows.path\": false\n"
        "  }\n"
        "}\n");

    SettingsStore v041FromV040(
        v040SettingsPath);

    v041FromV040.Load();

    assert(
        v041FromV040
            .WasMigratedFromOlderSchema());
    assert(
        v041FromV040
            .MigratedFromSchemaVersion() ==
        1);
    assert(!providers::IsEnabled(
        v041FromV040.Data()
            .providerEnabled,
        providers::kEverythingFilesystem,
        false));

    assert(
        v041FromV040.Data()
            .startWithWindows);
    assert(
        !v041FromV040.Data()
             .showTrayIcon);
    assert(
        v041FromV040.Data()
            .popupMonitor ==
        "primary");

    assert(
        v041FromV040.Data()
            .hotkeyModifiers.size() ==
        2);
    assert(
        v041FromV040.Data()
            .hotkeyModifiers[0] ==
        "ctrl");
    assert(
        v041FromV040.Data()
            .hotkeyModifiers[1] ==
        "shift");
    assert(
        v041FromV040.Data()
            .hotkeyKey ==
        "f12");

    assert(
        v041FromV040.Data()
            .uiStyle ==
        UiStyle::ModernCompact);
    assert(
        v041FromV040.Data()
            .language ==
        Language::EnUS);

    assert(
        providers::IsEnabled(
            v041FromV040.Data()
                .providerEnabled,
            providers::kStartMenu));
    assert(
        !providers::IsEnabled(
            v041FromV040.Data()
                .providerEnabled,
            providers::kPackaged));
    assert(
        providers::IsEnabled(
            v041FromV040.Data()
                .providerEnabled,
            providers::kAppPaths));
    assert(
        !providers::IsEnabled(
            v041FromV040.Data()
                .providerEnabled,
            providers::kPath));

    assert(
        !v041FromV040.Data()
             .auxiliaryHotkeyEnabled);
    assert(
        !v041FromV040.Data()
             .numericQuickLaunch);
    assert(
        !v041FromV040.Data()
             .executeSingleResultImmediately);

    const auto migratedV040 =
        config::LoadJsonWithBackup(
            v040SettingsPath,
            config::kSettingsSchemaVersion);

    assert(migratedV040.value);
    assert(
        migratedV040.schemaVersion ==
        config::kSettingsSchemaVersion);

    // Simulate a v0.4.0-era reader opening the migrated file. The older
    // schema ceiling must report it as newer and leave its bytes untouched.
    const std::string
        migratedV040BeforeDowngrade =
            ReadText(
                v040SettingsPath);

    const auto downgradeRead =
        config::LoadJsonWithBackup(
            v040SettingsPath,
            1);

    assert(
        downgradeRead.status ==
        config::JsonLoadStatus::
            UnsupportedSchema);
    assert(
        downgradeRead.schemaVersion ==
        config::kSettingsSchemaVersion);
    assert(downgradeRead.value);
    assert(
        ReadText(
            v040SettingsPath) ==
        migratedV040BeforeDowngrade);

    // A future settings schema remains usable for known fields but is
    // read-only so an older binary cannot overwrite newer data.
    const auto futureSettingsPath =
        data /
        "settings-future.json";

    WriteText(
        futureSettingsPath,
        "{\n"
        "  \"schemaVersion\": 99,\n"
        "  \"general\": {\n"
        "    \"startWithWindows\": true\n"
        "  },\n"
        "  \"appearance\": {\n"
        "    \"launcher\": \"modern-compact\",\n"
        "    \"language\": \"en-US\"\n"
        "  },\n"
        "  \"providers\": {\n"
        "    \"windows.path\": false\n"
        "  },\n"
        "  \"futureOnly\": {\"keep\": true}\n"
        "}\n");

    const std::string
        futureSettingsBefore =
            ReadText(
                futureSettingsPath);

    SettingsStore futureSettings(
        futureSettingsPath);

    futureSettings.Load();

    assert(
        futureSettings
            .IsReadOnlyDueToNewerSchema());
    assert(
        futureSettings
            .UnsupportedSchemaVersion() ==
        99);
    assert(
        futureSettings.Data()
            .startWithWindows);
    assert(
        futureSettings.Data().uiStyle ==
        UiStyle::ModernCompact);
    assert(!providers::IsEnabled(
        futureSettings.Data()
            .providerEnabled,
        providers::kPath));
    assert(!futureSettings
        .SetStartWithWindows(false));
    assert(
        ReadText(futureSettingsPath) ==
        futureSettingsBefore);

    const auto futureCommandsPath =
        data /
        "commands-future.json";

    WriteText(
        futureCommandsPath,
        "{\n"
        "  \"schemaVersion\": 99,\n"
        "  \"commands\": [\n"
        "    {\n"
        "      \"id\": \"future-command\",\n"
        "      \"name\": \"Future Command\",\n"
        "      \"keyword\": \"future\",\n"
        "      \"target\": \"future.exe\"\n"
        "    }\n"
        "  ],\n"
        "  \"futureOnly\": true\n"
        "}\n");

    const std::string
        futureCommandsBefore =
            ReadText(
                futureCommandsPath);

    UserCommandStore futureCommands(
        futureCommandsPath);

    futureCommands.Load();

    assert(
        futureCommands
            .IsReadOnlyDueToNewerSchema());
    assert(
        futureCommands.Commands().size() ==
        1);
    assert(
        futureCommands.Commands()[0].id ==
        L"future-command");
    assert(!futureCommands.Remove(
        L"future-command"));
    assert(
        ReadText(futureCommandsPath) ==
        futureCommandsBefore);

    const auto futureUsagePath =
        data /
        "usage-future.json";

    WriteText(
        futureUsagePath,
        "{\n"
        "  \"schemaVersion\": 99,\n"
        "  \"usage\": {\n"
        "    \"future-command\": {\n"
        "      \"launches\": 8,\n"
        "      \"lastUsedUnix\": 1700000400\n"
        "    }\n"
        "  },\n"
        "  \"futureOnly\": true\n"
        "}\n");

    const std::string
        futureUsageBefore =
            ReadText(
                futureUsagePath);

    UsageStore futureUsage(
        futureUsagePath);

    futureUsage.Load();

    assert(
        futureUsage
            .IsReadOnlyDueToNewerSchema());
    assert(
        futureUsage.Data()
            .at(L"future-command")
            .launches == 8);

    futureUsage.Record(
        L"future-command");

    assert(
        futureUsage.Data()
            .at(L"future-command")
            .launches == 8);
    assert(!futureUsage.Clear());
    assert(
        ReadText(futureUsagePath) ==
        futureUsageBefore);

    // Recovery status remains visible to the running process after ConfigIO
    // repairs the primary document.
    const auto recoveredCommandsPath =
        data /
        "commands-recovered.json";

    WriteText(
        recoveredCommandsPath.string() +
            ".bak",
        "{\n"
        "  \"schemaVersion\": 1,\n"
        "  \"commands\": [\n"
        "    {\n"
        "      \"id\": \"recovered-command\",\n"
        "      \"name\": \"Recovered Command\",\n"
        "      \"keyword\": \"recovered\",\n"
        "      \"target\": \"recovered.exe\"\n"
        "    }\n"
        "  ]\n"
        "}\n");

    WriteText(
        recoveredCommandsPath,
        "{ broken json");

    UserCommandStore recoveredCommands(
        recoveredCommandsPath);

    recoveredCommands.Load();

    assert(
        recoveredCommands
            .WasRecoveredFromBackup());
    assert(
        recoveredCommands.Commands().size() ==
        1);
    assert(
        recoveredCommands.Commands()[0].id ==
        L"recovered-command");

    const auto healedCommands =
        config::LoadJsonWithBackup(
            recoveredCommandsPath,
            config::kCommandsSchemaVersion);

    assert(
        healedCommands.status ==
        config::JsonLoadStatus::
            LoadedPrimary);

    const auto recoveredUsagePath =
        data /
        "usage-recovered.json";

    WriteText(
        recoveredUsagePath.string() +
            ".bak",
        "{\n"
        "  \"schemaVersion\": 1,\n"
        "  \"usage\": {\n"
        "    \"recovered-command\": {\n"
        "      \"launches\": 3,\n"
        "      \"lastUsedUnix\": 1700000500\n"
        "    }\n"
        "  }\n"
        "}\n");

    WriteText(
        recoveredUsagePath,
        "{ broken json");

    UsageStore recoveredUsage(
        recoveredUsagePath);

    recoveredUsage.Load();

    assert(
        recoveredUsage
            .WasRecoveredFromBackup());
    assert(
        recoveredUsage.Data()
            .at(L"recovered-command")
            .launches == 3);

    const auto healedUsage =
        config::LoadJsonWithBackup(
            recoveredUsagePath,
            config::kUsageSchemaVersion);

    assert(
        healedUsage.status ==
        config::JsonLoadStatus::
            LoadedPrimary);

    assert(featureSettings.ResetDefaults());
    assert(!featureSettings.Data().startWithWindows);
    assert(
        featureSettings.Data().startupBehavior ==
        StartupBehavior::Notification);
    assert(featureSettings.Data().showTrayIcon);
    assert(!featureSettings.Data().addToSendToMenu);
    assert(
        !featureSettings.Data()
             .auxiliaryHotkeyEnabled);
    assert(
        featureSettings.Data()
            .auxiliaryHotkeyModifiers
            .empty());
    assert(
        featureSettings.Data()
            .auxiliaryHotkeyKey ==
        "pause");
    assert(featureSettings.Data().pinyinSearch);
    assert(
        !featureSettings.Data()
             .numericQuickLaunch);
    assert(
        !featureSettings.Data()
             .executeSingleResultImmediately);
    assert(!providers::IsEnabled(
        featureSettings.Data()
            .providerEnabled,
        providers::kEverythingFilesystem,
        false));
    assert(featureSettings.Data().hotkeyModifiers.size() == 1);
    assert(featureSettings.Data().hotkeyModifiers[0] == "alt");
    assert(featureSettings.Data().hotkeyKey == "space");
    assert(!providers::IsEnabled(
        featureSettings.Data().providerEnabled,
        providers::kPath));

    ProviderCache providerCache(
        data / "provider-cache.json");

    Command cachedStart;
    cachedStart.id = L"start:test";
    cachedStart.title = L"Test App";
    cachedStart.keyword = L"test";
    cachedStart.aliases = {L"tester"};
    cachedStart.target =
        L"C:\\ProgramData\\Test App.lnk";
    cachedStart.source =
        CommandSource::StartMenu;
    cachedStart.activationKind =
        LaunchActivationKind::
            ShellItem;
    cachedStart.canonicalIdentity =
        L"file:c:\\apps\\test.exe";
    cachedStart.surfaceClass =
        LaunchSurfaceClass::SystemUtility;
    cachedStart.applicationRole =
        ApplicationRole::DiagnosticTool;
    cachedStart.roleConfidence =
        RoleConfidence::High;
    cachedStart.catalogVisibility =
        CatalogVisibility::StrongMatchOnly;
    cachedStart.catalogGroupKey =
        L"product:testapp|root:c:\\apps";
    cachedStart.distinctiveTokens = {
        L"diagnostic",
        L"tool",
    };
    cachedStart.basePriority = 0;

    Command cachedUser;
    cachedUser.id = L"user:test";
    cachedUser.title = L"User App";
    cachedUser.keyword = L"user";
    cachedUser.target =
        L"C:\\Apps\\User.exe";
    cachedUser.source =
        CommandSource::User;
    cachedUser.basePriority = 120;

    ProviderCacheData cacheData;

    ProviderCacheEntry startEntry;
    startEntry.generatedAtUnix =
        1700000100;
    startEntry.commands = {
        cachedStart,
        cachedUser,
    };

    cacheData[
        std::string(
            providers::kStartMenu)] =
        startEntry;

    assert(providerCache.Save(
        cacheData));

    auto cachedData =
        providerCache.Load();

    const auto startCache =
        cachedData.find(
            std::string(
                providers::kStartMenu));

    assert(startCache !=
        cachedData.end());
    assert(
        startCache->second
            .generatedAtUnix ==
        1700000100);
    assert(
        startCache->second
            .commands.size() == 1);
    assert(
        startCache->second
            .commands[0].source ==
        CommandSource::StartMenu);
    assert(
        startCache->second
            .commands[0].surfaceClass ==
        LaunchSurfaceClass::SystemUtility);
    assert(
        startCache->second
            .commands[0]
            .aliases.size() == 1);
    assert(
        startCache->second
            .commands[0]
            .activationKind ==
        LaunchActivationKind::
            ShellItem);
    assert(
        startCache->second
            .commands[0]
            .canonicalIdentity ==
        L"file:c:\\apps\\test.exe");
    assert(
        startCache->second
            .commands[0]
            .applicationRole ==
        ApplicationRole::DiagnosticTool);
    assert(
        startCache->second
            .commands[0]
            .roleConfidence ==
        RoleConfidence::High);
    assert(
        startCache->second
            .commands[0]
            .catalogVisibility ==
        CatalogVisibility::
            StrongMatchOnly);
    assert(
        startCache->second
            .commands[0]
            .catalogGroupKey ==
        L"product:testapp|root:c:\\apps");
    assert(
        startCache->second
            .commands[0]
            .distinctiveTokens.size() ==
        2);

    Command cachedPackaged;
    cachedPackaged.id =
        L"packaged:test";
    cachedPackaged.title =
        L"Store Test";
    cachedPackaged.keyword =
        L"storetest";
    cachedPackaged.target =
        L"Test.Package_abc!App";
    cachedPackaged.source =
        CommandSource::PackagedApp;
    cachedPackaged.activationKind =
        LaunchActivationKind::
            PackagedApplication;
    cachedPackaged.canonicalIdentity =
        L"aumid:test.package_abc!app";
    cachedPackaged.basePriority = -5;

    ProviderCacheEntry packagedEntry;
    packagedEntry.generatedAtUnix =
        1700000200;
    packagedEntry.commands = {
        cachedPackaged,
    };

    cacheData[
        std::string(
            providers::kPackaged)] =
        packagedEntry;

    assert(providerCache.Save(
        cacheData));
    assert(std::filesystem::exists(
        data /
            "provider-cache.json.bak"));

    WriteText(
        data / "provider-cache.json",
        "{ broken json");

    cachedData =
        providerCache.Load();

    // The .bak is the previous known-good cache generation.
    assert(cachedData.size() == 1);
    assert(
        cachedData.at(
            std::string(
                providers::kStartMenu))
            .commands.size() == 1);

    // A provider-cache entry is accepted only when its command source
    // matches the stable provider ID that owns the entry.
    const auto mismatchedProviderCache =
        data /
        "provider-cache-mismatched.json";

    WriteText(
        mismatchedProviderCache,
        "{\n"
        "  \"schemaVersion\": 9,\n"
        "  \"providers\": {\n"
        "    \"windows.startmenu\": {\n"
        "      \"generatedAtUnix\": 1700000250,\n"
        "      \"commands\": [\n"
        "        {\n"
        "          \"id\": \"path:wrong-owner\",\n"
        "          \"name\": \"Wrong Owner\",\n"
        "          \"keyword\": \"wrong\",\n"
        "          \"target\": \"wrong.exe\",\n"
        "          \"source\": \"path\",\n"
        "          \"canonicalIdentity\": \"file:wrong.exe\",\n"
        "          \"applicationRole\": \"unknown\",\n"
        "          \"roleConfidence\": \"low\",\n"
        "          \"catalogVisibility\": \"normal\",\n"
        "          \"distinctiveTokens\": []\n"
        "        }\n"
        "      ]\n"
        "    }\n"
        "  }\n"
        "}\n");

    ProviderCache mismatchedCache(
        mismatchedProviderCache);

    const auto mismatchedData =
        mismatchedCache.Load();

    assert(
        mismatchedData.at(
            std::string(
                providers::kStartMenu))
            .commands.empty());

    // Schema 8 predates launch-role evidence and catalog visibility.
    // Generated state must rebuild rather than guess those fields.
    const auto staleSchema8ProviderCache =
        data /
        "provider-cache-schema8-stale.json";

    WriteText(
        staleSchema8ProviderCache,
        "{\n"
        "  \"schemaVersion\": 8,\n"
        "  \"providers\": {\n"
        "    \"windows.startmenu\": {\n"
        "      \"generatedAtUnix\": 1700000260,\n"
        "      \"commands\": [\n"
        "        {\n"
        "          \"id\": \"start:stale\",\n"
        "          \"name\": \"Stale Website\",\n"
        "          \"keyword\": \"stalewebsite\",\n"
        "          \"target\": \"stale.url\",\n"
        "          \"source\": \"start-menu\"\n"
        "        }\n"
        "      ]\n"
        "    }\n"
        "  }\n"
        "}\n");

    ProviderCache staleSchema8Cache(
        staleSchema8ProviderCache);

    assert(
        staleSchema8Cache.Load().empty());

    // A future generated cache is safe to ignore; providers will rebuild it.
    const auto futureProviderCache =
        data /
        "provider-cache-future.json";

    WriteText(
        futureProviderCache,
        "{\n"
        "  \"schemaVersion\": 99,\n"
        "  \"providers\": {}\n"
        "}\n");

    ProviderCache futureCache(
        futureProviderCache);

    assert(
        futureCache.Load().empty());

    const auto legacyProviderCache =
        data /
        "provider-cache-legacy.json";

    WriteText(
        legacyProviderCache,
        "{\n"
        "  \"schemaVersion\": 1,\n"
        "  \"generatedAtUnix\": 1700000300,\n"
        "  \"commands\": [\n"
        "    {\n"
        "      \"id\": \"start:legacy\",\n"
        "      \"name\": \"Legacy Start\",\n"
        "      \"keyword\": \"legacy\",\n"
        "      \"type\": \"application\",\n"
        "      \"target\": \"legacy.lnk\",\n"
        "      \"source\": \"start-menu\",\n"
        "      \"basePriority\": 0\n"
        "    },\n"
        "    {\n"
        "      \"id\": \"path:legacy\",\n"
        "      \"name\": \"Legacy Tool\",\n"
        "      \"keyword\": \"tool\",\n"
        "      \"type\": \"application\",\n"
        "      \"target\": \"tool.exe\",\n"
        "      \"source\": \"path\",\n"
        "      \"basePriority\": -35\n"
        "    }\n"
        "  ]\n"
        "}\n");

    ProviderCache legacyCache(
        legacyProviderCache);

    const auto migratedCache =
        legacyCache.Load();

    // Provider cache is generated state. Legacy schema-1 data must not bypass
    // the current positive-admission policy; live providers rebuild it.
    assert(migratedCache.empty());

    std::error_code ec;
    std::filesystem::remove_all(root, ec);

    std::cout << "Config core migration tests passed\n";
    return 0;
}
