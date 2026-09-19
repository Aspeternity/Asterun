#include "ProviderCache.hpp"

#include "ConfigIO.hpp"
#include "TextCodec.hpp"

#include <chrono>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace altrun {

namespace {

constexpr int kProviderCacheSchemaVersion = 1;

const char* TypeName(CommandType type) {
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

CommandType ParseType(std::string_view value) {
    if (value == "url") return CommandType::Url;
    if (value == "folder") return CommandType::Folder;
    if (value == "command" || value == "commandline") {
        return CommandType::CommandLine;
    }
    return CommandType::Application;
}

const char* SourceName(CommandSource source) {
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
    if (value == "start-menu") return CommandSource::StartMenu;
    if (value == "app-paths") return CommandSource::AppPaths;
    if (value == "path") return CommandSource::Path;
    if (value == "packaged-app") return CommandSource::PackagedApp;
    return std::nullopt;
}

} // namespace

ProviderCache::ProviderCache(
    std::filesystem::path path)
    : path_(std::move(path)) {}

std::vector<Command>
ProviderCache::Load() const {
    std::vector<Command> commands;

    const auto json =
        config::LoadJsonWithBackup(path_);

    if (!json) {
        return commands;
    }

    try {
        const auto& root = *json;

        if (root.value("schemaVersion", 0) !=
                kProviderCacheSchemaVersion ||
            !root.contains("commands") ||
            !root["commands"].is_array()) {
            return {};
        }

        for (const auto& item :
             root["commands"]) {
            if (!item.is_object()) {
                continue;
            }

            const auto source =
                ParseSource(
                    item.value(
                        "source",
                        std::string{}));

            if (!source) {
                continue;
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
                item.value("basePriority", 0);

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
                continue;
            }

            if (command.keyword.empty()) {
                command.keyword =
                    command.title;
            }

            if (command.icon.empty()) {
                command.icon = L"auto";
            }

            commands.push_back(
                std::move(command));
        }
    } catch (...) {
        commands.clear();
    }

    return commands;
}

bool ProviderCache::Save(
    const std::vector<Command>& commands) const {

    nlohmann::json commandArray =
        nlohmann::json::array();

    for (const auto& command :
         commands) {
        const char* sourceName =
            SourceName(command.source);

        if (sourceName == nullptr) {
            continue;
        }

        nlohmann::json aliases =
            nlohmann::json::array();

        for (const auto& alias :
             command.aliases) {
            aliases.push_back(
                text::ToUtf8(alias));
        }

        commandArray.push_back({
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
            {"source", sourceName},
            {"basePriority", command.basePriority}
        });
    }

    const auto generatedAtUnix =
        std::chrono::duration_cast<
            std::chrono::seconds>(
                std::chrono::system_clock::now()
                    .time_since_epoch())
            .count();

    nlohmann::json root = {
        {"schemaVersion", kProviderCacheSchemaVersion},
        {"generatedAtUnix", generatedAtUnix},
        {"commands", std::move(commandArray)}
    };

    return config::SaveJsonAtomic(
        path_,
        root);
}

} // namespace altrun
