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

    [[nodiscard]] const UsageMap& Data() const noexcept { return usage_; }
    [[nodiscard]] const std::filesystem::path& Path() const noexcept { return jsonPath_; }

private:
    bool LoadJson();
    bool MigrateLegacyTsv(
        const std::unordered_map<std::wstring, std::wstring>& legacyIdMap);
    bool Save() const;

    std::filesystem::path jsonPath_;
    std::filesystem::path legacyTsvPath_;
    UsageMap usage_;
};

} // namespace altrun
