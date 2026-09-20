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
#include <unordered_set>

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

const char* RuntimeInputModeName(
    RuntimeInputMode mode) {
    switch (mode) {
    case RuntimeInputMode::Raw:
        return "raw";
    case RuntimeInputMode::UrlEncoded:
        return "url-encoded";
    case RuntimeInputMode::None:
    default:
        return "none";
    }
}

RuntimeInputMode ParseRuntimeInputMode(
    std::string value) {
    std::transform(
        value.begin(),
        value.end(),
        value.begin(),
        [](unsigned char c) {
            return static_cast<char>(
                std::tolower(c));
        });

    if (value == "raw") {
        return RuntimeInputMode::Raw;
    }

    if (value == "url-encoded" ||
        value == "urlencoded" ||
        value == "url") {
        return RuntimeInputMode::UrlEncoded;
    }

    return RuntimeInputMode::None;
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

std::wstring SanitizeTsv(std::wstring value) {
    for (wchar_t& c : value) {
        if (c == L'\t' || c == L'\r' || c == L'\n') {
            c = L' ';
        }
    }
    return value;
}

std::vector<std::wstring> SplitAliases(std::wstring_view value) {
    std::vector<std::wstring> aliases;
    std::wstring current;

    const auto flush = [&]() {
        const auto trimmed = TrimWide(current);
        current.clear();
        if (trimmed.empty()) return;

        const auto key = LowerWide(trimmed);
        const auto duplicate = std::find_if(
            aliases.begin(),
            aliases.end(),
            [&](const std::wstring& existing) {
                return LowerWide(existing) == key;
            });

        if (duplicate == aliases.end()) {
            aliases.push_back(trimmed);
        }
    };

    for (const wchar_t c : value) {
        if (c == L',' || c == L';' || c == L'，' || c == L'；') {
            flush();
        } else {
            current.push_back(c);
        }
    }

    flush();
    return aliases;
}

bool ParseBoolWide(std::wstring_view value, bool fallback) {
    const auto lower = LowerWide(TrimWide(value));
    if (lower == L"1" || lower == L"true" || lower == L"yes" || lower == L"on") {
        return true;
    }
    if (lower == L"0" || lower == L"false" || lower == L"no" || lower == L"off") {
        return false;
    }
    return fallback;
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
    readOnlyDueToNewerSchema_ =
        false;
    unsupportedSchemaVersion_ = 0;
    recoveredFromBackup_ = false;

    if (LoadJson()) {
        RebuildLegacyIdMap();
        return;
    }

    if (readOnlyDueToNewerSchema_) {
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
    auto load =
        config::LoadJsonWithBackup(
            jsonPath_,
            config::kCommandsSchemaVersion);

    recoveredFromBackup_ =
        load.status ==
            config::JsonLoadStatus::
                RecoveredBackup;

    if (load.status ==
        config::JsonLoadStatus::
            UnsupportedSchema) {

        readOnlyDueToNewerSchema_ =
            true;
        unsupportedSchemaVersion_ =
            load.schemaVersion;
    }

    if (!load.value) {
        return false;
    }

    try {
        const auto& root =
            *load.value;
        if (!root.contains("commands") || !root["commands"].is_array()) {
            return false;
        }

        bool repaired =
            load.schemaVersion <
            config::kCommandsSchemaVersion;
        const bool migratingSchema1 =
            load.schemaVersion > 0 &&
            load.schemaVersion < 2;
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
            command.runtimeInputMode =
                ParseRuntimeInputMode(
                    item.value(
                        "runtimeInputMode",
                        std::string("none")));

            // v0.6/v0.7 schema-1 web aliases used {query} implicitly.
            // Promote them into the explicit schema-2 runtime-input model.
            if (migratingSchema1 &&
                command.runtimeInputMode ==
                    RuntimeInputMode::None &&
                command.type ==
                    CommandType::Url &&
                command.target.find(
                    L"{query}") !=
                    std::wstring::npos) {
                command.runtimeInputMode =
                    RuntimeInputMode::UrlEncoded;
            }

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

        if (repaired &&
            !readOnlyDueToNewerSchema_) {
            Save();
        }

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

bool UserCommandStore::Create(
    Command command,
    std::wstring* createdId) {

    command.keyword = TrimWide(command.keyword);
    command.title = TrimWide(command.title);
    command.target = TrimWide(command.target);
    command.arguments = TrimWide(command.arguments);
    command.workingDirectory = TrimWide(command.workingDirectory);
    command.icon = TrimWide(command.icon);

    if (command.keyword.empty() || command.target.empty()) {
        return false;
    }

    if (command.title.empty()) {
        command.title = command.keyword;
    }

    if (command.id.empty()) {
        command.id = GenerateUuidV4();
    }

    for (const auto& existing : commands_) {
        if (existing.id == command.id) {
            return false;
        }
    }

    int nextOrder = 0;
    for (const auto& existing : commands_) {
        nextOrder = std::max(nextOrder, existing.sortOrder + 10);
    }

    command.sortOrder = nextOrder;
    command.source = CommandSource::User;
    command.basePriority = 120;

    if (command.icon.empty()) {
        command.icon = L"auto";
    }

    const auto previous = commands_;
    commands_.push_back(std::move(command));
    RebuildLegacyIdMap();

    if (!Save()) {
        commands_ = previous;
        RebuildLegacyIdMap();
        return false;
    }

    if (createdId) {
        *createdId = commands_.back().id;
    }

    return true;
}

bool UserCommandStore::Update(
    std::wstring_view id,
    Command command) {

    const auto it = std::find_if(
        commands_.begin(),
        commands_.end(),
        [&](const Command& existing) {
            return existing.id == id;
        });

    if (it == commands_.end()) {
        return false;
    }

    command.keyword = TrimWide(command.keyword);
    command.title = TrimWide(command.title);
    command.target = TrimWide(command.target);
    command.arguments = TrimWide(command.arguments);
    command.workingDirectory = TrimWide(command.workingDirectory);
    command.icon = TrimWide(command.icon);

    if (command.keyword.empty() || command.target.empty()) {
        return false;
    }

    if (command.title.empty()) {
        command.title = command.keyword;
    }

    const auto previous = commands_;

    command.id = it->id;
    command.sortOrder = it->sortOrder;
    command.legacyIds = it->legacyIds;
    command.source = CommandSource::User;
    command.basePriority = 120;

    if (command.icon.empty()) {
        command.icon = L"auto";
    }

    *it = std::move(command);
    RebuildLegacyIdMap();

    if (!Save()) {
        commands_ = previous;
        RebuildLegacyIdMap();
        return false;
    }

    return true;
}

bool UserCommandStore::Remove(std::wstring_view id) {
    const auto previous = commands_;

    const auto oldSize = commands_.size();
    std::erase_if(
        commands_,
        [&](const Command& command) {
            return command.id == id;
        });

    if (commands_.size() == oldSize) {
        return false;
    }

    RebuildLegacyIdMap();

    if (!Save()) {
        commands_ = previous;
        RebuildLegacyIdMap();
        return false;
    }

    return true;
}

bool UserCommandStore::Move(
    std::wstring_view id,
    int direction) {

    if (direction == 0 || commands_.size() < 2) {
        return false;
    }

    std::stable_sort(
        commands_.begin(),
        commands_.end(),
        [](const Command& a, const Command& b) {
            if (a.sortOrder != b.sortOrder) {
                return a.sortOrder < b.sortOrder;
            }
            return a.keyword < b.keyword;
        });

    const auto it = std::find_if(
        commands_.begin(),
        commands_.end(),
        [&](const Command& command) {
            return command.id == id;
        });

    if (it == commands_.end()) {
        return false;
    }

    const auto index =
        static_cast<std::ptrdiff_t>(
            std::distance(commands_.begin(), it));

    const auto targetIndex =
        index + (direction < 0 ? -1 : 1);

    if (targetIndex < 0 ||
        targetIndex >=
            static_cast<std::ptrdiff_t>(commands_.size())) {
        return false;
    }

    const auto previous = commands_;

    std::swap(
        commands_[static_cast<std::size_t>(index)].sortOrder,
        commands_[static_cast<std::size_t>(targetIndex)].sortOrder);

    std::swap(
        commands_[static_cast<std::size_t>(index)],
        commands_[static_cast<std::size_t>(targetIndex)]);

    if (!Save()) {
        commands_ = previous;
        return false;
    }

    return true;
}


bool UserCommandStore::ApplyPathUpdates(
    const std::vector<UserCommandPathUpdate>& updates) {
    if (readOnlyDueToNewerSchema_ ||
        updates.empty()) {
        return false;
    }

    const auto previous = commands_;
    std::unordered_set<std::wstring>
        seen;

    for (const auto& update :
         updates) {
        if (update.id.empty() ||
            !seen.insert(
                update.id).second) {
            commands_ = previous;
            return false;
        }

        const auto it =
            std::find_if(
                commands_.begin(),
                commands_.end(),
                [&](const Command& command) {
                    return command.id ==
                        update.id;
                });

        if (it == commands_.end()) {
            commands_ = previous;
            return false;
        }

        if (update.target) {
            const std::wstring value =
                TrimWide(*update.target);

            if (value.empty()) {
                commands_ = previous;
                return false;
            }

            it->target = value;
        }

        if (update.workingDirectory) {
            it->workingDirectory =
                TrimWide(
                    *update.workingDirectory);
        }

        if (update.icon) {
            const std::wstring value =
                TrimWide(*update.icon);

            it->icon =
                value.empty()
                    ? L"auto"
                    : value;
        }
    }

    if (!Save()) {
        commands_ = previous;
        return false;
    }

    return true;
}

bool UserCommandStore::ImportTsv(
    const std::filesystem::path& path,
    bool legacyMode,
    std::size_t* imported,
    std::size_t* skipped) {

    if (imported) *imported = 0;
    if (skipped) *skipped = 0;

    std::ifstream input(path, std::ios::binary);
    if (!input) return false;

    const auto previous = commands_;
    std::size_t importedCount = 0;
    std::size_t skippedCount = 0;

    int nextOrder = 0;
    for (const auto& command : commands_) {
        nextOrder = std::max(nextOrder, command.sortOrder + 10);
    }

    std::string lineUtf8;
    bool firstLine = true;

    while (std::getline(input, lineUtf8)) {
        if (firstLine) {
            firstLine = false;
            if (lineUtf8.size() >= 3 &&
                static_cast<unsigned char>(lineUtf8[0]) == 0xEF &&
                static_cast<unsigned char>(lineUtf8[1]) == 0xBB &&
                static_cast<unsigned char>(lineUtf8[2]) == 0xBF) {
                lineUtf8.erase(0, 3);
            }
        }

        if (!lineUtf8.empty() && lineUtf8.back() == '\r') {
            lineUtf8.pop_back();
        }

        const std::wstring line = TrimWide(text::FromUtf8(lineUtf8));
        if (line.empty() || line[0] == L'#' || line[0] == L';') {
            continue;
        }

        if (legacyMode &&
            line.front() == L'[' &&
            line.back() == L']') {
            continue;
        }

        Command command;
        command.id = GenerateUuidV4();
        command.icon = L"auto";
        command.enabled = true;
        command.source = CommandSource::User;
        command.basePriority = 120;
        command.sortOrder = nextOrder;

        const auto fields = SplitTabs(line);

        if (!legacyMode && fields.size() >= 11) {
            command.keyword = TrimWide(fields[0]);
            command.title = TrimWide(fields[1]);
            command.aliases = SplitAliases(fields[2]);
            command.type = ParseType(text::ToUtf8(TrimWide(fields[3])));
            command.target = TrimWide(fields[4]);
            command.arguments = TrimWide(fields[5]);
            command.workingDirectory = TrimWide(fields[6]);
            command.enabled = ParseBoolWide(fields[7], true);
            command.runAsAdmin = ParseBoolWide(fields[8], false);
            command.pinned = ParseBoolWide(fields[9], false);
            if (fields.size() >= 12) {
                command.runtimeInputMode =
                    ParseRuntimeInputMode(
                        text::ToUtf8(
                            TrimWide(
                                fields[11])));
            }
            if (fields.size() >= 13) {
                const std::wstring icon =
                    TrimWide(fields[12]);
                command.icon =
                    icon.empty()
                        ? L"auto"
                        : icon;
            }

            try {
                command.sortOrder = std::stoi(TrimWide(fields[10]));
            } catch (...) {
                command.sortOrder = nextOrder;
            }
        } else if (fields.size() >= 3) {
            command.keyword = TrimWide(fields[0]);
            command.title = TrimWide(fields[1]);
            command.target = TrimWide(fields[2]);
            if (fields.size() >= 4) {
                command.arguments = TrimWide(fields[3]);
            }
            if (fields.size() >= 5) {
                command.workingDirectory = TrimWide(fields[4]);
            }
        } else if (legacyMode) {
            const auto equals = line.find(L'=');
            if (equals == std::wstring::npos) {
                ++skippedCount;
                continue;
            }

            command.keyword = TrimWide(line.substr(0, equals));
            command.title = command.keyword;
            command.target = TrimWide(line.substr(equals + 1));
        } else {
            ++skippedCount;
            continue;
        }

        if (command.keyword.empty() || command.target.empty()) {
            ++skippedCount;
            continue;
        }

        if (command.title.empty()) {
            command.title = command.keyword;
        }

        const auto keywordKey = LowerWide(command.keyword);
        const auto targetKey = LowerWide(command.target);

        const bool duplicate = std::any_of(
            commands_.begin(),
            commands_.end(),
            [&](const Command& existing) {
                return LowerWide(existing.keyword) == keywordKey &&
                       LowerWide(existing.target) == targetKey;
            });

        if (duplicate) {
            ++skippedCount;
            continue;
        }

        command.legacyIds.push_back(
            LegacyCommandId(command.keyword, command.target));

        commands_.push_back(std::move(command));
        nextOrder += 10;
        ++importedCount;
    }

    RebuildLegacyIdMap();

    if (importedCount > 0 && !Save()) {
        commands_ = previous;
        RebuildLegacyIdMap();
        return false;
    }

    if (imported) *imported = importedCount;
    if (skipped) *skipped = skippedCount;
    return true;
}

bool UserCommandStore::ExportTsv(
    const std::filesystem::path& path) const {

    std::ofstream output(
        path,
        std::ios::binary | std::ios::trunc);

    if (!output) return false;

    output.write("\xEF\xBB\xBF", 3);
    output <<
        "# ALTRun Next commands TSV v3\n"
        "# keyword\tname\taliases\ttype\ttarget\targuments\tworkingDirectory\tenabled\trunAsAdmin\tpinned\tsortOrder\truntimeInputMode\ticon\n";

    std::vector<const Command*> ordered;
    ordered.reserve(commands_.size());

    for (const auto& command : commands_) {
        ordered.push_back(&command);
    }

    std::stable_sort(
        ordered.begin(),
        ordered.end(),
        [](const Command* a, const Command* b) {
            if (a->sortOrder != b->sortOrder) {
                return a->sortOrder < b->sortOrder;
            }
            return a->keyword < b->keyword;
        });

    for (const Command* command : ordered) {
        std::wstring aliases;
        for (std::size_t i = 0; i < command->aliases.size(); ++i) {
            if (i > 0) aliases += L",";
            aliases += SanitizeTsv(command->aliases[i]);
        }

        std::wostringstream line;
        line
            << SanitizeTsv(command->keyword) << L'\t'
            << SanitizeTsv(command->title) << L'\t'
            << aliases << L'\t'
            << text::FromUtf8(TypeName(command->type)) << L'\t'
            << SanitizeTsv(command->target) << L'\t'
            << SanitizeTsv(command->arguments) << L'\t'
            << SanitizeTsv(command->workingDirectory) << L'\t'
            << (command->enabled ? L"1" : L"0") << L'\t'
            << (command->runAsAdmin ? L"1" : L"0") << L'\t'
            << (command->pinned ? L"1" : L"0") << L'\t'
            << command->sortOrder << L'\t'
            << text::FromUtf8(
                   RuntimeInputModeName(
                       command->runtimeInputMode))
            << L'\t'
            << SanitizeTsv(
                   command->icon.empty()
                       ? L"auto"
                       : command->icon)
            << L'\n';

        const std::string utf8 = text::ToUtf8(line.str());
        output.write(
            utf8.data(),
            static_cast<std::streamsize>(utf8.size()));
    }

    return output.good();
}

bool UserCommandStore::Save() const {
    if (readOnlyDueToNewerSchema_) {
        return false;
    }

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
            {"runtimeInputMode", RuntimeInputModeName(command.runtimeInputMode)},
            {"icon", text::ToUtf8(command.icon)},
            {"enabled", command.enabled},
            {"runAsAdmin", command.runAsAdmin},
            {"pinned", command.pinned},
            {"sortOrder", command.sortOrder},
            {"legacyIds", std::move(legacyIds)}
        });
    }

    nlohmann::json root = {
        {"schemaVersion", config::kCommandsSchemaVersion},
        {"commands", std::move(commandArray)}
    };

    return config::SaveJsonAtomic(jsonPath_, root);
}

} // namespace altrun
