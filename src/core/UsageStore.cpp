#include "UsageStore.hpp"

#include "ConfigIO.hpp"
#include "TextCodec.hpp"

#include <algorithm>
#include <chrono>
#include <vector>
#include <fstream>
#include <limits>
#include <string>

namespace altrun {

namespace {

constexpr std::size_t kMaxQueriesPerCommand = 8;
constexpr std::size_t kMaxQueryLength = 32;
// Eight selections already reach the ranking bonus ceiling. Keep evidence
// bounded and age competing choices only for the same normalized query so
// a new habit can replace an old one without changing Usage schema 2.
constexpr std::uint32_t kMaxQueryEvidence = 8;

std::wstring QueryKey(std::wstring_view query) {
    if (relevance::HasExplicitSyntax(query)) {
        return {};
    }
    std::wstring key = relevance::Normalize(query);
    if (key.empty() || key.size() > kMaxQueryLength) {
        return {};
    }
    return key;
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

            if (it.value().contains("queries") &&
                it.value()["queries"].is_object()) {
                for (auto query = it.value()["queries"].begin();
                     query != it.value()["queries"].end() &&
                     stat.queryLaunches.size() < kMaxQueriesPerCommand;
                     ++query) {
                    if (!query.value().is_number_unsigned()) continue;
                    const std::wstring key = text::FromUtf8(query.key());
                    if (QueryKey(key) != key) continue;
                    const auto count = query.value().get<std::uint64_t>();
                    if (count > 0 &&
                        count <= std::numeric_limits<std::uint32_t>::max()) {
                        stat.queryLaunches[key] =
                            static_cast<std::uint32_t>(count);
                    }
                }
            }

            usage_[text::FromUtf8(it.key())] = stat;
        }

        if (load.schemaVersion < config::kUsageSchemaVersion) {
            // Preserve existing global counts while upgrading the on-disk
            // contract. There is no historical query to infer.
            Save();
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
    std::wstring_view commandId,
    std::wstring_view query) {

    if (readOnlyDueToNewerSchema_) {
        return;
    }

    const std::wstring selectedId(
        commandId);

    const auto [selectedIt, inserted] =
        usage_.try_emplace(
            selectedId);

    UsageStat previousSelected;

    if (!inserted) {
        previousSelected =
            selectedIt->second;
    }

    auto& stat =
        selectedIt->second;

    struct CompetingEvidenceRollback {
        UsageStat* stat{};
        std::uint32_t count{};
    };

    std::vector<CompetingEvidenceRollback>
        competingRollbacks;

    if (stat.launches < std::numeric_limits<std::uint64_t>::max()) {
        ++stat.launches;
    }
    stat.lastUsedUnix =
        UnixTimeNow();

    const std::wstring key = QueryKey(query);
    if (!key.empty()) {
        for (auto& [id, other] : usage_) {
            if (id == selectedId) continue;

            const auto competing =
                other.queryLaunches.find(
                    key);

            if (competing ==
                other.queryLaunches.end()) {
                continue;
            }

            competingRollbacks.push_back({
                &other,
                competing->second,
            });

            const auto evidence =
                std::min(
                    competing->second,
                    kMaxQueryEvidence);

            if (evidence <= 1) {
                other.queryLaunches.erase(
                    competing);
            } else {
                competing->second =
                    evidence - 1;
            }
        }
        auto found = stat.queryLaunches.find(key);
        if (found == stat.queryLaunches.end() &&
            stat.queryLaunches.size() >= kMaxQueriesPerCommand) {
            const auto least = std::min_element(
                stat.queryLaunches.begin(), stat.queryLaunches.end(),
                [](const auto& a, const auto& b) {
                    return a.second == b.second
                        ? a.first < b.first
                        : a.second < b.second;
                });
            stat.queryLaunches.erase(least);
        }
        auto& count = stat.queryLaunches[key];
        count = std::min(count, kMaxQueryEvidence - 1) + 1;
    }

    if (!Save()) {
        if (inserted) {
            usage_.erase(
                selectedIt);
        } else {
            selectedIt->second =
                std::move(
                    previousSelected);
        }

        for (const auto& rollback :
             competingRollbacks) {
            if (rollback.stat) {
                rollback.stat
                    ->queryLaunches[key] =
                    rollback.count;
            }
        }
    }
}

bool UsageStore::Clear() {
    UsageMap previous =
        std::move(
            usage_);

    usage_.clear();

    if (!Save()) {
        usage_ =
            std::move(
                previous);
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
        nlohmann::json entry = {
            {"launches", stat.launches},
            {"lastUsedUnix", stat.lastUsedUnix}
        };
        if (!stat.queryLaunches.empty()) {
            nlohmann::json queries = nlohmann::json::object();
            for (const auto& [key, count] : stat.queryLaunches) {
                queries[text::ToUtf8(key)] = count;
            }
            entry["queries"] = std::move(queries);
        }
        usage[text::ToUtf8(id)] = std::move(entry);
    }

    nlohmann::json root = {
        {"schemaVersion", config::kUsageSchemaVersion},
        {"usage", std::move(usage)}
    };

    return config::SaveJsonAtomic(jsonPath_, root);
}

} // namespace altrun
