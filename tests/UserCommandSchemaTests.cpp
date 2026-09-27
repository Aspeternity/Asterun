#include "core/ConfigIO.hpp"
#include "core/UserCommandStore.hpp"

#include <algorithm>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

#include <nlohmann/json.hpp>

using namespace altrun;

namespace {

void WriteText(
    const std::filesystem::path& path,
    const std::string& text) {
    std::filesystem::create_directories(
        path.parent_path());

    std::ofstream output(
        path,
        std::ios::binary |
            std::ios::trunc);

    assert(output);
    output << text;
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
    const auto root =
        std::filesystem::temp_directory_path() /
        "altrun-command-schema-v2-test";

    std::error_code ec;
    std::filesystem::remove_all(
        root,
        ec);
    std::filesystem::create_directories(
        root);

    const auto path =
        root / "commands.json";

    nlohmann::json schema1 = {
        {"schemaVersion", 1},
        {"commands",
         nlohmann::json::array({
             {
                 {"id", "legacy-web"},
                 {"name", "Google Search"},
                 {"keyword", "g"},
                 {"aliases",
                  nlohmann::json::array(
                      {"google"})},
                 {"type", "url"},
                 {"target",
                  "https://www.google.com/search?q={query}"},
                 {"arguments", ""},
                 {"workingDirectory", ""},
                 {"icon", "auto"},
                 {"enabled", true},
                 {"runAsAdmin", false},
                 {"pinned", false},
                 {"sortOrder", 0},
                 {"legacyIds",
                  nlohmann::json::array()},
             },
             {
                 {"id", "legacy-app"},
                 {"name", "Notepad"},
                 {"keyword", "np"},
                 {"aliases",
                  nlohmann::json::array()},
                 {"type", "application"},
                 {"target", "notepad.exe"},
                 {"arguments", ""},
                 {"workingDirectory", ""},
                 {"icon", "auto"},
                 {"enabled", true},
                 {"runAsAdmin", false},
                 {"pinned", false},
                 {"sortOrder", 10},
                 {"legacyIds",
                  nlohmann::json::array()},
             },
         })},
    };

    WriteText(
        path,
        schema1.dump(2));

    UserCommandStore store(path);
    store.Load();

    assert(store.Commands().size() == 2);
    assert(
        store.Commands()[0]
            .runtimeInputMode ==
        RuntimeInputMode::UrlEncoded);
    assert(
        store.Commands()[1]
            .runtimeInputMode ==
        RuntimeInputMode::None);

    const auto migrated =
        nlohmann::json::parse(
            ReadText(path));

    assert(
        migrated.at("schemaVersion")
            .get<int>() == 2);
    assert(
        migrated.at("commands")
            .at(0)
            .at("runtimeInputMode")
            .get<std::string>() ==
        "url-encoded");
    assert(
        migrated.at("commands")
            .at(1)
            .at("runtimeInputMode")
            .get<std::string>() ==
        "none");
    assert(
        !migrated.at("commands")
             .at(0)
             .contains("icon"));
    assert(
        !migrated.at("commands")
             .at(1)
             .contains("icon"));

    const std::string beforeDowngrade =
        ReadText(path);

    const auto downgrade =
        config::LoadJsonWithBackup(
            path,
            1);

    assert(
        downgrade.status ==
        config::JsonLoadStatus::
            UnsupportedSchema);
    assert(
        downgrade.schemaVersion == 2);
    assert(
        ReadText(path) ==
        beforeDowngrade);

    Command raw;
    raw.keyword = L"ping";
    raw.title = L"Ping";
    raw.type =
        CommandType::CommandLine;
    raw.target = L"ping.exe";
    raw.runtimeInputMode =
        RuntimeInputMode::Raw;

    std::wstring createdId;
    assert(
        store.Create(
            raw,
            &createdId));

    UserCommandStore reloaded(path);
    reloaded.Load();

    const auto it =
        std::find_if(
            reloaded.Commands().begin(),
            reloaded.Commands().end(),
            [&](const Command& command) {
                return command.id ==
                    createdId;
            });

    assert(
        it !=
        reloaded.Commands().end());
    assert(
        it->runtimeInputMode ==
        RuntimeInputMode::Raw);

    const auto exportPath =
        root / "commands-export.tsv";
    assert(
        reloaded.ExportTsv(
            exportPath));

    const std::string exported =
        ReadText(exportPath);
    assert(
        exported.find(
            "commands TSV v3") !=
        std::string::npos);
    assert(
        exported.find(
            "runtimeInputMode") !=
        std::string::npos);
    assert(
        exported.find(
            "\ticon") ==
        std::string::npos);

    const auto importedPath =
        root / "commands-imported.json";
    UserCommandStore imported(
        importedPath);
    imported.Load();

    std::size_t importedCount = 0;
    std::size_t skippedCount = 0;
    assert(
        imported.ImportTsv(
            exportPath,
            &importedCount,
            &skippedCount));
    assert(importedCount >= 1);

    const auto importedIt =
        std::find_if(
            imported.Commands().begin(),
            imported.Commands().end(),
            [](const Command& command) {
                return command.keyword ==
                    L"ping";
            });
    assert(
        importedIt !=
        imported.Commands().end());
    assert(
        importedIt->runtimeInputMode ==
        RuntimeInputMode::Raw);

    const auto legacyFlagsPath =
        root / "commands-legacy-flags.json";
    nlohmann::json legacyFlags = {
        {"schemaVersion", 2},
        {"commands",
         nlohmann::json::array({
             {
                 {"id", "legacy-flags"},
                 {"name", "Legacy Flags"},
                 {"keyword", "legacy"},
                 {"aliases",
                  nlohmann::json::array(
                      {"old"})},
                 {"type", "application"},
                 {"target", "legacy.exe"},
                 {"arguments", ""},
                 {"workingDirectory", ""},
                 {"runtimeInputMode", "none"},
                 {"icon", "auto"},
                 {"enabled", false},
                 {"runAsAdmin", false},
                 {"pinned", true},
                 {"sortOrder", 0},
                 {"legacyIds",
                  nlohmann::json::array()},
             },
         })},
    };
    WriteText(
        legacyFlagsPath,
        legacyFlags.dump(2));

    UserCommandStore legacyFlagsStore(
        legacyFlagsPath);
    legacyFlagsStore.Load();

    assert(legacyFlagsStore.Commands().size() == 1);
    assert(legacyFlagsStore.Commands()[0].enabled);
    assert(!legacyFlagsStore.Commands()[0].pinned);

    const auto normalizedFlags =
        nlohmann::json::parse(
            ReadText(legacyFlagsPath));
    assert(
        normalizedFlags.at("commands")
            .at(0)
            .at("enabled")
            .get<bool>());
    assert(
        !normalizedFlags.at("commands")
             .at(0)
             .at("pinned")
             .get<bool>());
    assert(
        !normalizedFlags.at("commands")
             .at(0)
             .contains("icon"));

    Command legacyCreate;
    legacyCreate.keyword = L"legacy-create";
    legacyCreate.title = L"Legacy Create";
    legacyCreate.target = L"legacy-create.exe";
    legacyCreate.enabled = false;
    legacyCreate.pinned = true;
    assert(legacyFlagsStore.Create(legacyCreate));
    assert(legacyFlagsStore.Commands().back().enabled);
    assert(!legacyFlagsStore.Commands().back().pinned);

    std::filesystem::remove_all(
        root,
        ec);

    return 0;
}
