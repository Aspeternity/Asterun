#pragma once

#include "SearchEngine.hpp"

#include <filesystem>
#include <condition_variable>
#include <mutex>
#include <string_view>
#include <thread>
#include <unordered_map>

namespace altrun {

class UsageStore {
public:
    UsageStore(
        std::filesystem::path jsonPath,
        std::filesystem::path legacyTsvPath = {});
    ~UsageStore();

    void Load(
        const std::unordered_map<std::wstring, std::wstring>& legacyIdMap = {});

    void Record(std::wstring_view commandId,
                std::wstring_view query = {});
    // Drain pending writes, including one final retry on failure.
    bool Flush();
    bool Remove(std::wstring_view commandId);
    bool PruneMissingAutomatic(const std::vector<Command>& liveCommands);
    bool Clear();

    [[nodiscard]] const UsageMap& Data() const noexcept { return usage_; }
    [[nodiscard]] const std::filesystem::path& Path() const noexcept { return jsonPath_; }

    [[nodiscard]] bool IsReadOnlyDueToNewerSchema() const noexcept {
        return readOnlyDueToNewerSchema_;
    }

    [[nodiscard]] int UnsupportedSchemaVersion() const noexcept {
        return unsupportedSchemaVersion_;
    }

    [[nodiscard]] bool WasRecoveredFromBackup() const noexcept {
        return recoveredFromBackup_;
    }

private:
    bool LoadJson();
    bool MigrateLegacyTsv(
        const std::unordered_map<std::wstring, std::wstring>& legacyIdMap);
    bool Save() const;
    bool SaveSnapshot(const UsageMap& snapshot) const;
    void PersistLoop(std::stop_token stop);

    std::filesystem::path jsonPath_;
    std::filesystem::path legacyTsvPath_;
    UsageMap usage_;
    bool preserveInvalidInput_{false};
    bool readOnlyDueToNewerSchema_{false};
    int unsupportedSchemaVersion_{0};
    bool recoveredFromBackup_{false};
    mutable std::mutex persistMutex_;
    std::condition_variable_any persistWake_;
    std::jthread persistThread_;
    std::uint64_t generation_{0};
    std::uint64_t persistedGeneration_{0};
};

} // namespace altrun
