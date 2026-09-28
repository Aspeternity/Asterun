#include "UsageStore.hpp"

#include "ConfigIO.hpp"
#include "ConfigValidation.hpp"
#include "TextCodec.hpp"

#include <algorithm>
#include <chrono>
#include <vector>
#include <fstream>
#include <limits>
#include <string>
#include <unordered_set>

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

UsageStore::~UsageStore() { (void) Flush(); }

void UsageStore::PersistLoop(std::stop_token stop) {
    try {
        std::unique_lock lock(persistMutex_);
        for (;;) {
            persistWake_.wait(lock, stop, [&] {
                return generation_ != persistedGeneration_;
            });
            if (generation_ == persistedGeneration_) return;
            // Rapid selections share one disk transaction; a stop request
            // (explicit Flush or destruction) skips this short delay.
            while (!stop.stop_requested()) {
                const auto observed = generation_;
                if (!persistWake_.wait_for(lock, std::chrono::milliseconds(40),
                    [&] { return stop.stop_requested() || generation_ != observed; })) break;
            }
            const auto selected = generation_;
            UsageMap snapshot = usage_;
            lock.unlock();
            const bool saved = SaveSnapshot(snapshot);
            lock.lock();
            if (saved) persistedGeneration_ = selected;
            if (stop.stop_requested()) {
                if (!saved || persistedGeneration_ == generation_) return;
            } else if (!saved) {
                persistWake_.wait(lock, stop, [&] { return generation_ != selected; });
                if (stop.stop_requested()) return;
            }
        }
    } catch (...) {
        // Flush retries the current in-memory state if snapshotting failed.
    }
}

bool UsageStore::Flush() {
    if (persistThread_.joinable()) {
        persistThread_.request_stop();
        persistWake_.notify_all();
        persistThread_.join();
    }
    std::scoped_lock lock(persistMutex_);
    if (generation_ == persistedGeneration_) return true;
    if (!SaveSnapshot(usage_)) return false;
    persistedGeneration_ = generation_;
    return true;
}

void UsageStore::Load(
    const std::unordered_map<std::wstring, std::wstring>& legacyIdMap) {

    if (!Flush()) return;
    usage_.clear();
    generation_ = persistedGeneration_ = 0;
    readOnlyDueToNewerSchema_ =
        false;
    unsupportedSchemaVersion_ = 0;
    recoveredFromBackup_ = false;
    preserveInvalidInput_ = false;

    if (LoadJson()) {
        return;
    }

    if (readOnlyDueToNewerSchema_ || preserveInvalidInput_) {
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
            config::kUsageSchemaVersion,
            config::ValidUsage);

    preserveInvalidInput_ =
        load.status == config::JsonLoadStatus::InvalidExisting ||
        (load.status == config::JsonLoadStatus::RecoveredBackup && !load.primaryRepaired);

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
            if (!config::ValidUsageRecord(it.value())) continue;

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

    if (readOnlyDueToNewerSchema_ || preserveInvalidInput_) {
        return;
    }

    std::scoped_lock lock(persistMutex_);
    const std::wstring selectedId(
        commandId);

    const auto [selectedIt, inserted] =
        usage_.try_emplace(
            selectedId);

    (void) inserted;

    auto& stat =
        selectedIt->second;

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

    ++generation_;
    if (!persistThread_.joinable()) {
        persistThread_ = std::jthread([this](std::stop_token stop) {
            PersistLoop(stop);
        });
    }
    persistWake_.notify_all();
}

bool UsageStore::Clear() {
    (void) Flush();
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

bool UsageStore::Remove(std::wstring_view commandId) {
    (void) Flush();
    const auto found = usage_.find(std::wstring(commandId));
    if (found == usage_.end()) return true;
    UsageStat previous = std::move(found->second);
    usage_.erase(found);
    if (Save()) return true;
    usage_.emplace(std::wstring(commandId), std::move(previous));
    return false;
}

bool UsageStore::PruneMissingAutomatic(
    const std::vector<Command>& liveCommands) {
    if (readOnlyDueToNewerSchema_ || preserveInvalidInput_) return false;
    constexpr std::int64_t kGraceSeconds = 365LL * 24 * 60 * 60;
    const auto now = UnixTimeNow();
    std::unordered_set<std::wstring> present;
    present.reserve(liveCommands.size());
    for (const auto& command : liveCommands) present.insert(command.id);
    const auto obsolete = [&](const auto& item) {
        const auto& id = item.first;
        const auto lastUsed = item.second.lastUsedUnix;
        const bool automatic = id.starts_with(L"start:") ||
            id.starts_with(L"apppath:") || id.starts_with(L"path:") ||
            id.starts_with(L"packaged:");
        return automatic && lastUsed > 0 &&
            lastUsed < now - kGraceSeconds && !present.contains(id);
    };
    if (std::none_of(usage_.begin(), usage_.end(), obsolete)) return true;
    (void) Flush();
    UsageMap previous = usage_;
    std::erase_if(usage_, obsolete);
    if (Save()) return true;
    usage_ = std::move(previous);
    return false;
}

bool UsageStore::Save() const {
    return SaveSnapshot(usage_);
}

bool UsageStore::SaveSnapshot(const UsageMap& snapshot) const {
    if (readOnlyDueToNewerSchema_ || preserveInvalidInput_) {
        return false;
    }

    try {

    nlohmann::json usage = nlohmann::json::object();

    for (const auto& [id, stat] : snapshot) {
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

    return config::SaveJsonAtomic(jsonPath_, root, config::ValidUsage);
    } catch (...) {
        return false;
    }
}

} // namespace altrun
