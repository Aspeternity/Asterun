#include "UsageStore.hpp"

#include "ConfigIO.hpp"
#include "TextCodec.hpp"

#include <algorithm>
#include <chrono>
#include <vector>
#include <fstream>
#include <string>

namespace altrun {

namespace {

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

std::int64_t UnixTimeNow() {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

} // namespace

UsageStore::UsageStore(
    std::filesystem::path jsonPath,
    std::filesystem::path legacyTsvPath)
    : jsonPath_(std::move(jsonPath)),
      legacyTsvPath_(std::move(legacyTsvPath)) {}

void UsageStore::Load(
    const std::unordered_map<std::wstring, std::wstring>& legacyIdMap) {

    usage_.clear();
    readOnlyDueToNewerSchema_ =
        false;
    unsupportedSchemaVersion_ = 0;
    recoveredFromBackup_ = false;

    if (LoadJson()) {
        return;
    }

    if (readOnlyDueToNewerSchema_) {
        return;
    }

    if (!legacyTsvPath_.empty() && std::filesystem::exists(legacyTsvPath_)) {
        MigrateLegacyTsv(legacyIdMap);
    }

    Save();
}

bool UsageStore::LoadJson() {
    auto load =
        config::LoadJsonWithBackup(
            jsonPath_,
            config::kUsageSchemaVersion);

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
        if (!root.contains("usage") || !root["usage"].is_object()) {
            return false;
        }

        for (auto it = root["usage"].begin(); it != root["usage"].end(); ++it) {
            if (!it.value().is_object()) continue;

            UsageStat stat;
            stat.launches = it.value().value("launches", std::uint64_t{0});
            stat.lastUsedUnix = it.value().value("lastUsedUnix", std::int64_t{0});

            usage_[text::FromUtf8(it.key())] = stat;
        }

        return true;
    } catch (...) {
        usage_.clear();
        return false;
    }
}

bool UsageStore::MigrateLegacyTsv(
    const std::unordered_map<std::wstring, std::wstring>& legacyIdMap) {

    std::ifstream input(legacyTsvPath_, std::ios::binary);
    if (!input) return false;

    std::string lineUtf8;
    bool migratedAny = false;

    while (std::getline(input, lineUtf8)) {
        if (!lineUtf8.empty() && lineUtf8.back() == '\r') lineUtf8.pop_back();

        const auto fields = SplitTabs(text::FromUtf8(lineUtf8));
        if (fields.size() < 3 || fields[0].empty()) continue;

        try {
            std::wstring id = fields[0];

            const auto mapped = legacyIdMap.find(id);
            if (mapped != legacyIdMap.end()) {
                id = mapped->second;
            }

            UsageStat stat;
            stat.launches = std::stoull(fields[1]);
            stat.lastUsedUnix = std::stoll(fields[2]);

            auto& existing = usage_[id];
            existing.launches += stat.launches;
            existing.lastUsedUnix =
                std::max(existing.lastUsedUnix, stat.lastUsedUnix);

            migratedAny = true;
        } catch (...) {
            // Ignore malformed legacy rows. Migration should never prevent
            // the launcher from starting.
        }
    }

    return migratedAny;
}

void UsageStore::Record(
    std::wstring_view commandId) {

    if (readOnlyDueToNewerSchema_) {
        return;
    }

    const UsageMap previous =
        usage_;

    auto& stat =
        usage_[std::wstring(commandId)];

    ++stat.launches;
    stat.lastUsedUnix =
        UnixTimeNow();

    if (!Save()) {
        usage_ = previous;
    }
}

bool UsageStore::Clear() {
    const UsageMap previous = usage_;
    usage_.clear();

    if (!Save()) {
        usage_ = previous;
        return false;
    }

    return true;
}

bool UsageStore::Save() const {
    if (readOnlyDueToNewerSchema_) {
        return false;
    }

    nlohmann::json usage = nlohmann::json::object();

    for (const auto& [id, stat] : usage_) {
        usage[text::ToUtf8(id)] = {
            {"launches", stat.launches},
            {"lastUsedUnix", stat.lastUsedUnix}
        };
    }

    nlohmann::json root = {
        {"schemaVersion", config::kUsageSchemaVersion},
        {"usage", std::move(usage)}
    };

    return config::SaveJsonAtomic(jsonPath_, root);
}

} // namespace altrun
