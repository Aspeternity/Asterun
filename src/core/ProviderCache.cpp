#include "ProviderCache.hpp"

#include "ConfigIO.hpp"
#include "ProviderIds.hpp"
#include "TextCodec.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace altrun {

namespace {

constexpr int kProviderCacheSchemaVersion = 2;

const char* TypeName(
    CommandType type) {
    switch (type) {
    case CommandType::Url:
        return "url";
    case CommandType::Folder:
        return "folder";
    case CommandType::CommandLine:
        return "command";
    case CommandType::Application:
    default:
        return "application";
    }
}

CommandType ParseType(
    std::string_view value) {
    if (value == "url") {
        return CommandType::Url;
    }
    if (value == "folder") {
        return CommandType::Folder;
    }
    if (value == "command" ||
        value == "commandline") {
        return CommandType::CommandLine;
    }
    return CommandType::Application;
}

const char* SourceName(
    CommandSource source) {
    switch (source) {
    case CommandSource::StartMenu:
        return "start-menu";
    case CommandSource::AppPaths:
        return "app-paths";
    case CommandSource::Path:
        return "path";
    case CommandSource::PackagedApp:
        return "packaged-app";
    case CommandSource::User:
    default:
        return nullptr;
    }
}

std::optional<CommandSource>
ParseSource(std::string_view value) {
    if (value == "start-menu") {
        return CommandSource::StartMenu;
    }
    if (value == "app-paths") {
        return CommandSource::AppPaths;
    }
    if (value == "path") {
        return CommandSource::Path;
    }
    if (value == "packaged-app") {
        return CommandSource::PackagedApp;
    }
    return std::nullopt;
}

const char* ProviderIdForSource(
    CommandSource source) {
    switch (source) {
    case CommandSource::StartMenu:
        return providers::kStartMenu.data();
    case CommandSource::PackagedApp:
        return providers::kPackaged.data();
    case CommandSource::AppPaths:
        return providers::kAppPaths.data();
    case CommandSource::Path:
        return providers::kPath.data();
    case CommandSource::User:
    default:
        return nullptr;
    }
}

bool SourceMatchesProvider(
    std::string_view providerId,
    CommandSource source) {

    const char* expected =
        ProviderIdForSource(source);

    return expected != nullptr &&
        providerId == expected;
}

std::optional<Command>
ParseCommand(
    const nlohmann::json& item) {

    if (!item.is_object()) {
        return std::nullopt;
    }

    const auto source =
        ParseSource(
            item.value(
                "source",
                std::string{}));

    if (!source) {
        return std::nullopt;
    }

    Command command;
    command.id =
        text::FromUtf8(
            item.value(
                "id",
                std::string{}));
    command.keyword =
        text::FromUtf8(
            item.value(
                "keyword",
                std::string{}));
    command.title =
        text::FromUtf8(
            item.value(
                "name",
                std::string{}));
    command.type =
        ParseType(
            item.value(
                "type",
                std::string(
                    "application")));
    command.target =
        text::FromUtf8(
            item.value(
                "target",
                std::string{}));
    command.arguments =
        text::FromUtf8(
            item.value(
                "arguments",
                std::string{}));
    command.workingDirectory =
        text::FromUtf8(
            item.value(
                "workingDirectory",
                std::string{}));
    command.icon =
        text::FromUtf8(
            item.value(
                "icon",
                std::string("auto")));
    command.enabled =
        item.value("enabled", true);
    command.runAsAdmin =
        item.value("runAsAdmin", false);
    command.pinned =
        item.value("pinned", false);
    command.sortOrder =
        item.value("sortOrder", 0);
    command.source = *source;
    command.basePriority =
        item.value(
            "basePriority",
            0);

    if (item.contains("aliases") &&
        item["aliases"].is_array()) {
        for (const auto& alias :
             item["aliases"]) {
            if (!alias.is_string()) {
                continue;
            }

            command.aliases.push_back(
                text::FromUtf8(
                    alias.get<std::string>()));
        }
    }

    if (command.id.empty() ||
        command.title.empty() ||
        command.target.empty()) {
        return std::nullopt;
    }

    if (command.keyword.empty()) {
        command.keyword =
            command.title;
    }

    if (command.icon.empty()) {
        command.icon = L"auto";
    }

    return command;
}

nlohmann::json CommandJson(
    const Command& command) {

    nlohmann::json aliases =
        nlohmann::json::array();

    for (const auto& alias :
         command.aliases) {
        aliases.push_back(
            text::ToUtf8(alias));
    }

    return {
        {"id", text::ToUtf8(command.id)},
        {"name", text::ToUtf8(command.title)},
        {"keyword", text::ToUtf8(command.keyword)},
        {"aliases", std::move(aliases)},
        {"type", TypeName(command.type)},
        {"target", text::ToUtf8(command.target)},
        {"arguments", text::ToUtf8(command.arguments)},
        {"workingDirectory", text::ToUtf8(command.workingDirectory)},
        {"icon", text::ToUtf8(command.icon)},
        {"enabled", command.enabled},
        {"runAsAdmin", command.runAsAdmin},
        {"pinned", command.pinned},
        {"sortOrder", command.sortOrder},
        {"source", SourceName(command.source)},
        {"basePriority", command.basePriority}
    };
}

} // namespace

ProviderCache::ProviderCache(
    std::filesystem::path path)
    : path_(std::move(path)) {}

ProviderCacheData
ProviderCache::Load() const {
    ProviderCacheData data;

    auto load =
        config::LoadJsonWithBackup(
            path_,
            kProviderCacheSchemaVersion);

    // Provider cache is generated state. A cache written by a newer schema
    // is intentionally ignored and rebuilt instead of being interpreted by
    // an older binary.
    if (!load.value ||
        load.status ==
            config::JsonLoadStatus::
                UnsupportedSchema) {
        return data;
    }

    try {
        const auto& root =
            *load.value;
        const int version =
            root.value(
                "schemaVersion",
                0);

        if (version ==
            kProviderCacheSchemaVersion) {

            if (!root.contains("providers") ||
                !root["providers"].is_object()) {
                return {};
            }

            for (auto it =
                     root["providers"].begin();
                 it !=
                     root["providers"].end();
                 ++it) {

                if (!it.value().is_object()) {
                    continue;
                }

                ProviderCacheEntry entry;
                entry.generatedAtUnix =
                    it.value().value(
                        "generatedAtUnix",
                        std::int64_t{0});

                if (it.value().contains(
                        "commands") &&
                    it.value()["commands"]
                        .is_array()) {

                    for (const auto& item :
                         it.value()["commands"]) {
                        auto command =
                            ParseCommand(item);

                        if (command &&
                            command->source !=
                                CommandSource::User &&
                            SourceMatchesProvider(
                                it.key(),
                                command->source)) {

                            entry.commands.push_back(
                                std::move(*command));
                        }
                    }
                }

                data.emplace(
                    it.key(),
                    std::move(entry));
            }

            return data;
        }

        // v0.4.0-alpha.2 used a flat schemaVersion 1 cache.
        // Group those commands by their stable provider IDs in memory;
        // the next successful refresh writes schemaVersion 2.
        if (version == 1 &&
            root.contains("commands") &&
            root["commands"].is_array()) {

            const std::int64_t generatedAt =
                root.value(
                    "generatedAtUnix",
                    std::int64_t{0});

            for (const auto& item :
                 root["commands"]) {
                auto command =
                    ParseCommand(item);

                if (!command ||
                    command->source ==
                        CommandSource::User) {
                    continue;
                }

                const char* providerId =
                    ProviderIdForSource(
                        command->source);

                if (providerId == nullptr) {
                    continue;
                }

                auto& entry =
                    data[providerId];

                entry.generatedAtUnix =
                    generatedAt;

                entry.commands.push_back(
                    std::move(*command));
            }
        }
    } catch (...) {
        data.clear();
    }

    return data;
}

bool ProviderCache::Save(
    const ProviderCacheData& data) const {

    nlohmann::json providersJson =
        nlohmann::json::object();

    for (const auto& [providerId, entry] :
         data) {

        nlohmann::json commandsJson =
            nlohmann::json::array();

        for (const auto& command :
             entry.commands) {
            if (command.source ==
                CommandSource::User) {
                continue;
            }

            commandsJson.push_back(
                CommandJson(command));
        }

        providersJson[providerId] = {
            {"generatedAtUnix",
             entry.generatedAtUnix},
            {"commands",
             std::move(commandsJson)}
        };
    }

    nlohmann::json root = {
        {"schemaVersion",
         kProviderCacheSchemaVersion},
        {"providers",
         std::move(providersJson)}
    };

    return config::SaveJsonAtomic(
        path_,
        root);
}

} // namespace altrun
