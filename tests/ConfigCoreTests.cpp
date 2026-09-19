#include "core/ConfigIO.hpp"
#include "core/ProviderCache.hpp"
#include "core/ProviderFingerprint.hpp"
#include "core/Settings.hpp"
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
    assert(!updated->enabled);
    assert(updated->pinned);

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
    assert(!persisted->enabled);

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
        false,
        &importedCount,
        &skippedCount));
    assert(importedCount == 2);
    assert(skippedCount == 0);

    UserCommandStore importedReloaded(
        data / "commands-imported.json");
    importedReloaded.Load();
    assert(importedReloaded.Commands().size() == 2);

    const auto legacyBeta = root / "legacy-altrun.ini";
    WriteText(
        legacyBeta,
        "[Shortcuts]\n"
        "paint=mspaint.exe\n"
        "term\tTerminal\tcmd.exe\t/k echo test\tC:\\\\Windows\n");

    UserCommandStore legacyImported(
        data / "commands-legacy-imported.json");
    importedCount = 0;
    skippedCount = 0;
    assert(legacyImported.ImportTsv(
        legacyBeta,
        true,
        &importedCount,
        &skippedCount));
    assert(importedCount == 2);

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
    assert(!featureSettings.Data().showOnStartup);
    assert(!featureSettings.Data().auxiliaryHotkeyEnabled);
    assert(featureSettings.Data().auxiliaryHotkeyKey == "pause");
    assert(!featureSettings.Data().wildcardMatching);
    assert(!featureSettings.Data().numericQuickLaunch);
    assert(
        featureSettings.Data().numericQuickLaunchOrder ==
        "one-to-zero");
    assert(
        !featureSettings.Data()
             .executeSingleResultImmediately);

    assert(featureSettings.SetStartWithWindows(true));
    assert(featureSettings.Data().startWithWindows);

    assert(featureSettings.SetShowOnStartup(true));
    assert(featureSettings.Data().showOnStartup);

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
        "zero-to-nine",
        true));
    assert(featureSettings.Data().wildcardMatching);
    assert(featureSettings.Data().numericQuickLaunch);
    assert(
        featureSettings.Data()
            .numericQuickLaunchOrder ==
        "zero-to-nine");
    assert(
        featureSettings.Data()
            .executeSingleResultImmediately);

    assert(providers::IsEnabled(
        featureSettings.Data().providerEnabled,
        providers::kStartMenu));
    assert(providers::IsEnabled(
        featureSettings.Data().providerEnabled,
        providers::kPackaged));
    assert(providers::IsEnabled(
        featureSettings.Data().providerEnabled,
        providers::kAppPaths));
    assert(providers::IsEnabled(
        featureSettings.Data().providerEnabled,
        providers::kPath));

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
            .showOnStartup);
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
        providerSettingsReloaded.Data()
            .wildcardMatching);
    assert(
        providerSettingsReloaded.Data()
            .numericQuickLaunch);
    assert(
        providerSettingsReloaded.Data()
            .numericQuickLaunchOrder ==
        "zero-to-nine");
    assert(
        providerSettingsReloaded.Data()
            .executeSingleResultImmediately);

    // Every provider toggle combination must survive a save/reload cycle.
    const std::array<std::string_view, 4>
        providerIds{
            providers::kStartMenu,
            providers::kPackaged,
            providers::kAppPaths,
            providers::kPath,
        };

    for (unsigned mask = 0;
         mask < 16;
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
         providerIds) {
        assert(providers::IsEnabled(
            alphaReloaded.Data()
                .providerEnabled,
            providerId));
    }

    assert(
        !alphaReloaded.Data()
             .showOnStartup);
    assert(
        !alphaReloaded.Data()
             .wildcardMatching);
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
    assert(!featureSettings.Data().showOnStartup);
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
    assert(
        !featureSettings.Data()
             .wildcardMatching);
    assert(
        !featureSettings.Data()
             .numericQuickLaunch);
    assert(
        featureSettings.Data()
            .numericQuickLaunchOrder ==
        "one-to-zero");
    assert(
        !featureSettings.Data()
             .executeSingleResultImmediately);
    assert(featureSettings.Data().hotkeyModifiers.size() == 1);
    assert(featureSettings.Data().hotkeyModifiers[0] == "alt");
    assert(featureSettings.Data().hotkeyKey == "space");
    assert(providers::IsEnabled(
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
    cachedStart.basePriority = 20;

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
            .commands[0]
            .aliases.size() == 1);

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
        "  \"schemaVersion\": 2,\n"
        "  \"providers\": {\n"
        "    \"windows.startmenu\": {\n"
        "      \"generatedAtUnix\": 1700000250,\n"
        "      \"commands\": [\n"
        "        {\n"
        "          \"id\": \"path:wrong-owner\",\n"
        "          \"name\": \"Wrong Owner\",\n"
        "          \"keyword\": \"wrong\",\n"
        "          \"target\": \"wrong.exe\",\n"
        "          \"source\": \"path\"\n"
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

    assert(
        migratedCache.at(
            std::string(
                providers::kStartMenu))
            .generatedAtUnix ==
        1700000300);
    assert(
        migratedCache.at(
            std::string(
                providers::kPath))
            .commands.size() == 1);

    std::error_code ec;
    std::filesystem::remove_all(root, ec);

    std::cout << "Config core migration tests passed\n";
    return 0;
}
