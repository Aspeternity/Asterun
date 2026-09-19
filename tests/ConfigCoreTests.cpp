#include "core/Settings.hpp"
#include "core/UsageStore.hpp"
#include "core/UserCommandStore.hpp"

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

    UsageStore usage(data / "usage.json", legacyUsage);
    usage.Load(commandsReloaded.LegacyIdMap());

    assert(std::filesystem::exists(data / "usage.json"));
    assert(usage.Data().at(calcId).launches == 7);
    assert(usage.Data().at(calcId).lastUsedUnix == 1700000000);

    settings.SetLanguage(Language::ZhCN);
    assert(std::filesystem::exists(data / "settings.json.bak"));

    WriteText(data / "settings.json", "{ broken json");

    SettingsStore recovered(data / "settings.json", legacySettings);
    recovered.Load();
    assert(recovered.Data().language == Language::EnUS);

    std::error_code ec;
    std::filesystem::remove_all(root, ec);

    std::cout << "Config core migration tests passed\n";
    return 0;
}
