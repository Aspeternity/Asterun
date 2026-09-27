#pragma once

#include "Command.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace altrun {

struct UserCommandPathUpdate {
    std::wstring id;
    std::optional<std::wstring> target;
    std::optional<std::wstring> workingDirectory;
};

class UserCommandStore {
public:
    UserCommandStore(
        std::filesystem::path jsonPath,
        std::filesystem::path legacyTsvPath = {});

    void Load();
    bool Save() const;

    bool Create(Command command, std::wstring* createdId = nullptr);
    bool Update(std::wstring_view id, Command command);
    bool Remove(std::wstring_view id);
    bool Move(std::wstring_view id, int direction);
    bool ApplyPathUpdates(
        const std::vector<UserCommandPathUpdate>& updates);
    bool ImportTsv(
        const std::filesystem::path& path,
        std::size_t* imported = nullptr,
        std::size_t* skipped = nullptr);
    bool ExportTsv(const std::filesystem::path& path) const;

    [[nodiscard]] const std::vector<Command>& Commands() const noexcept {
        return commands_;
    }

    [[nodiscard]] const std::unordered_map<std::wstring, std::wstring>& LegacyIdMap() const noexcept {
        return legacyIdMap_;
    }

    [[nodiscard]] const std::filesystem::path& Path() const noexcept {
        return jsonPath_;
    }

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
    bool MigrateLegacyTsv();
    void CreateDefaults();
    void RebuildLegacyIdMap();

    std::filesystem::path jsonPath_;
    std::filesystem::path legacyTsvPath_;
    std::vector<Command> commands_;
    std::unordered_map<std::wstring, std::wstring> legacyIdMap_;
    bool readOnlyDueToNewerSchema_{false};
    int unsupportedSchemaVersion_{0};
    bool recoveredFromBackup_{false};
};

} // namespace altrun
