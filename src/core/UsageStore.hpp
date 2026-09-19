#pragma once

#include "SearchEngine.hpp"

#include <filesystem>
#include <string_view>
#include <unordered_map>

namespace altrun {

class UsageStore {
public:
    UsageStore(
        std::filesystem::path jsonPath,
        std::filesystem::path legacyTsvPath = {});

    void Load(
        const std::unordered_map<std::wstring, std::wstring>& legacyIdMap = {});

    void Record(std::wstring_view commandId);
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

    std::filesystem::path jsonPath_;
    std::filesystem::path legacyTsvPath_;
    UsageMap usage_;
    bool readOnlyDueToNewerSchema_{false};
    int unsupportedSchemaVersion_{0};
    bool recoveredFromBackup_{false};
};

} // namespace altrun
