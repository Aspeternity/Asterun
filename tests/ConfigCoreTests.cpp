#include "core/ProviderCache.hpp"
#include "core/Settings.hpp"
#include "core/UsageStore.hpp"
#include "core/UserCommandStore.hpp"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>

using namespace altrun;

namespace {

void WriteText(const std::filesystem::path& path, const std::string& text) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    assert(out);
    out << text;
}

} // namespace

int main() {
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

    SettingsStore recovered(data / "settings.json", legacySettings);
    recovered.Load();
    assert(recovered.Data().language == Language::EnUS);

    SettingsStore featureSettings(data / "settings-features.json");
    featureSettings.Load();
    assert(!featureSettings.Data().startWithWindows);
    assert(featureSettings.SetStartWithWindows(true));
    assert(featureSettings.Data().startWithWindows);

    assert(featureSettings.SetHotkey(
        {"ctrl", "shift"},
        "k"));
    assert(featureSettings.Data().hotkeyModifiers.size() == 2);
    assert(featureSettings.Data().hotkeyKey == "k");

    assert(featureSettings.ResetDefaults());
    assert(!featureSettings.Data().startWithWindows);
    assert(featureSettings.Data().hotkeyModifiers.size() == 1);
    assert(featureSettings.Data().hotkeyModifiers[0] == "alt");
    assert(featureSettings.Data().hotkeyKey == "space");

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

    assert(providerCache.Save(
        {cachedStart, cachedUser}));

    auto cachedCommands =
        providerCache.Load();

    assert(cachedCommands.size() == 1);
    assert(
        cachedCommands[0].source ==
        CommandSource::StartMenu);
    assert(
        cachedCommands[0].aliases.size() == 1);
    assert(
        cachedCommands[0].basePriority == 20);

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

    assert(providerCache.Save(
        {cachedStart, cachedPackaged}));
    assert(std::filesystem::exists(
        data / "provider-cache.json.bak"));

    WriteText(
        data / "provider-cache.json",
        "{ broken json");

    cachedCommands =
        providerCache.Load();

    assert(cachedCommands.size() == 1);
    assert(
        cachedCommands[0].id ==
        cachedStart.id);

    std::error_code ec;
    std::filesystem::remove_all(root, ec);

    std::cout << "Config core migration tests passed\n";
    return 0;
}
