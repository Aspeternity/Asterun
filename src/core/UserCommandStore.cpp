#include "UserCommandStore.hpp"

#include "ConfigIO.hpp"
#include "TextCodec.hpp"

#include <algorithm>
#include <cctype>
#include <array>
#include <cwctype>
#include <fstream>
#include <iomanip>
#include <random>
#include <sstream>
#include <string>

namespace altrun {

namespace {

std::wstring LowerWide(std::wstring_view value) {
    std::wstring out(value);
    std::transform(out.begin(), out.end(), out.begin(), [](wchar_t c) {
        return static_cast<wchar_t>(std::towlower(c));
    });
    return out;
}

std::wstring TrimWide(std::wstring_view value) {
    std::size_t start = 0;
    std::size_t end = value.size();

    while (start < end && std::iswspace(value[start])) ++start;
    while (end > start && std::iswspace(value[end - 1])) --end;

    return std::wstring(value.substr(start, end - start));
}

std::vector<std::wstring> SplitTabs(std::wstring_view line) {
    std::vector<std::wstring> fields;
    std::size_t start = 0;

    while (start <= line.size()) {
        const auto pos = line.find(L'\t', start);
        if (pos == std::wstring_view::npos) {
            fields.emplace_back(line.substr(start));
            break;
        }
        fields.emplace_back(line.substr(start, pos - start));
        start = pos + 1;
    }

    return fields;
}

std::wstring LegacyCommandId(
    std::wstring_view keyword,
    std::wstring_view target) {
    return L"custom:" + LowerWide(keyword) + L":" + LowerWide(target);
}

std::wstring GenerateUuidV4() {
    std::array<unsigned char, 16> bytes{};
    std::random_device random;

    for (auto& byte : bytes) {
        byte = static_cast<unsigned char>(random() & 0xFFu);
    }

    bytes[6] = static_cast<unsigned char>((bytes[6] & 0x0Fu) | 0x40u);
    bytes[8] = static_cast<unsigned char>((bytes[8] & 0x3Fu) | 0x80u);

    std::wostringstream out;
    out << std::hex << std::setfill(L'0');

    for (std::size_t i = 0; i < bytes.size(); ++i) {
        out << std::setw(2) << static_cast<unsigned int>(bytes[i]);
        if (i == 3 || i == 5 || i == 7 || i == 9) out << L'-';
    }

    return out.str();
}

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

CommandType ParseType(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });

    if (value == "url") return CommandType::Url;
    if (value == "folder") return CommandType::Folder;
    if (value == "command" || value == "commandline") return CommandType::CommandLine;
    return CommandType::Application;
}

Command MakeDefault(
    std::wstring keyword,
    std::wstring title,
    std::wstring target,
    int sortOrder) {

    Command command;
    command.id = GenerateUuidV4();
    command.keyword = std::move(keyword);
    command.title = std::move(title);
    command.target = std::move(target);
    command.type = CommandType::Application;
    command.icon = L"auto";
    command.enabled = true;
    command.sortOrder = sortOrder;
    command.source = CommandSource::User;
    command.basePriority = 120;
    return command;
}

} // namespace

UserCommandStore::UserCommandStore(
    std::filesystem::path jsonPath,
    std::filesystem::path legacyTsvPath)
    : jsonPath_(std::move(jsonPath)),
      legacyTsvPath_(std::move(legacyTsvPath)) {}

void UserCommandStore::Load() {
    commands_.clear();
    legacyIdMap_.clear();

    if (LoadJson()) {
        RebuildLegacyIdMap();
        return;
    }

    if (!legacyTsvPath_.empty() && std::filesystem::exists(legacyTsvPath_)) {
        if (!MigrateLegacyTsv()) {
            CreateDefaults();
        }
    } else {
        CreateDefaults();
    }

    RebuildLegacyIdMap();
    Save();
}

bool UserCommandStore::LoadJson() {
    const auto json = config::LoadJsonWithBackup(jsonPath_);
    if (!json) return false;

    try {
        const auto& root = *json;
        if (!root.contains("commands") || !root["commands"].is_array()) {
            return false;
        }

        bool repaired = false;
        int fallbackOrder = 0;

        for (const auto& item : root["commands"]) {
            if (!item.is_object()) continue;

            Command command;
            command.id = text::FromUtf8(item.value("id", std::string{}));
            if (command.id.empty()) {
                command.id = GenerateUuidV4();
                repaired = true;
            }

            command.keyword =
                text::FromUtf8(item.value("keyword", std::string{}));
            command.title =
                text::FromUtf8(item.value("name", std::string{}));
            command.type =
                ParseType(item.value("type", std::string("application")));
            command.target =
                text::FromUtf8(item.value("target", std::string{}));
            command.arguments =
                text::FromUtf8(item.value("arguments", std::string{}));
            command.workingDirectory =
                text::FromUtf8(item.value("workingDirectory", std::string{}));
            command.icon =
                text::FromUtf8(item.value("icon", std::string("auto")));
            command.enabled = item.value("enabled", true);
            command.runAsAdmin = item.value("runAsAdmin", false);
            command.pinned = item.value("pinned", false);
            command.sortOrder = item.value("sortOrder", fallbackOrder++);
            command.source = CommandSource::User;
            command.basePriority = 120;

            if (item.contains("aliases") && item["aliases"].is_array()) {
                for (const auto& alias : item["aliases"]) {
                    if (!alias.is_string()) continue;
                    command.aliases.push_back(text::FromUtf8(alias.get<std::string>()));
                }
            }

            if (item.contains("legacyIds") && item["legacyIds"].is_array()) {
                for (const auto& legacyId : item["legacyIds"]) {
                    if (!legacyId.is_string()) continue;
                    command.legacyIds.push_back(
                        text::FromUtf8(legacyId.get<std::string>()));
                }
            }

            if (command.keyword.empty() || command.target.empty()) continue;
            if (command.title.empty()) command.title = command.keyword;
            if (command.icon.empty()) command.icon = L"auto";

            commands_.push_back(std::move(command));
        }

        if (repaired) Save();
        return true;
    } catch (...) {
        commands_.clear();
        return false;
    }
}

bool UserCommandStore::MigrateLegacyTsv() {
    std::ifstream input(legacyTsvPath_, std::ios::binary);
    if (!input) return false;

    std::string utf8Line;
    int sortOrder = 0;

    while (std::getline(input, utf8Line)) {
        if (!utf8Line.empty() && utf8Line.back() == '\r') utf8Line.pop_back();
        if (utf8Line.empty() || utf8Line[0] == '#') continue;

        const auto line = text::FromUtf8(utf8Line);
        auto fields = SplitTabs(line);
        if (fields.size() < 3) continue;
        while (fields.size() < 5) fields.emplace_back();

        Command command;
        command.id = GenerateUuidV4();
        command.keyword = TrimWide(fields[0]);
        command.title = TrimWide(fields[1]);
        command.target = TrimWide(fields[2]);
        command.arguments = TrimWide(fields[3]);
        command.workingDirectory = TrimWide(fields[4]);
        command.type = CommandType::Application;
        command.icon = L"auto";
        command.enabled = true;
        command.sortOrder = sortOrder++;
        command.source = CommandSource::User;
        command.basePriority = 120;

        if (command.keyword.empty() || command.target.empty()) continue;
        if (command.title.empty()) command.title = command.keyword;

        command.legacyIds.push_back(
            LegacyCommandId(command.keyword, command.target));

        commands_.push_back(std::move(command));
    }

    return !commands_.empty();
}

void UserCommandStore::CreateDefaults() {
    commands_.clear();
    commands_.push_back(MakeDefault(L"np", L"Notepad", L"notepad.exe", 0));
    commands_.push_back(MakeDefault(L"calc", L"Calculator", L"calc.exe", 1));
    commands_.push_back(MakeDefault(L"cmd", L"Command Prompt", L"cmd.exe", 2));
    commands_.push_back(MakeDefault(L"explorer", L"File Explorer", L"explorer.exe", 3));
    commands_.push_back(MakeDefault(L"pwsh", L"PowerShell", L"powershell.exe", 4));
}

void UserCommandStore::RebuildLegacyIdMap() {
    legacyIdMap_.clear();

    for (const auto& command : commands_) {
        for (const auto& legacyId : command.legacyIds) {
            if (!legacyId.empty()) {
                legacyIdMap_[legacyId] = command.id;
            }
        }
    }
}

bool UserCommandStore::Save() const {
    nlohmann::json commandArray = nlohmann::json::array();

    for (const auto& command : commands_) {
        nlohmann::json aliases = nlohmann::json::array();
        for (const auto& alias : command.aliases) {
            aliases.push_back(text::ToUtf8(alias));
        }

        nlohmann::json legacyIds = nlohmann::json::array();
        for (const auto& legacyId : command.legacyIds) {
            legacyIds.push_back(text::ToUtf8(legacyId));
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
            {"legacyIds", std::move(legacyIds)}
        });
    }

    nlohmann::json root = {
        {"schemaVersion", config::kSchemaVersion},
        {"commands", std::move(commandArray)}
    };

    return config::SaveJsonAtomic(jsonPath_, root);
}

} // namespace altrun
