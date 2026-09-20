#pragma once

#include "Command.hpp"
#include "CommandMerge.hpp"
#include "ProviderCache.hpp"
#include "ProviderIds.hpp"
#include "ProviderRegistry.hpp"
#include "UserCommandStore.hpp"

#include <cstdint>
#include <filesystem>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace altrun {

enum class ProviderRefreshOutcome {
    Failed = 0,
    Success = 1,
    Partial = 2,
};

struct ProviderStatus {
    std::string id;
    std::wstring name;
    bool enabled{false};
    std::size_t commandCount{0};
    std::size_t activeCommandCount{0};
    std::size_t suppressedCommandCount{0};
    std::int64_t lastRefreshUnix{0};
    std::int64_t lastAttemptUnix{0};
    bool lastAttemptSucceeded{true};
    std::wstring lastError;
};

class CommandStore {
public:
    CommandStore(
        std::filesystem::path baseDirectory,
        std::filesystem::path dataDirectory);

    void Reload(
        const ProviderEnableMap& enabled);
    void ReloadProviderCache(
        const ProviderEnableMap& enabled);

    [[nodiscard]] ProviderRefreshOutcome
    RefreshProviderCache(
        const ProviderEnableMap& enabled,
        const std::vector<std::string>&
            selectedIds = {});

    [[nodiscard]] std::vector<
        ProviderDescriptor>
    ProviderDescriptors() const;

    [[nodiscard]] std::vector<
        ProviderChangeToken>
    ProviderChangeTokens(
        const ProviderEnableMap& enabled) const;

    [[nodiscard]] std::vector<
        ProviderStatus>
    ProviderStatuses(
        const ProviderEnableMap& enabled) const;

    bool CreateUserCommand(
        Command command,
        std::wstring* createdId = nullptr);
    bool UpdateUserCommand(
        std::wstring_view id,
        Command command);
    bool DeleteUserCommand(
        std::wstring_view id);
    bool MoveUserCommand(
        std::wstring_view id,
        int direction);
    bool ApplyUserCommandPathUpdates(
        const std::vector<UserCommandPathUpdate>& updates);
    bool ImportUserCommands(
        const std::filesystem::path& path,
        bool legacyMode,
        std::size_t* imported = nullptr,
        std::size_t* skipped = nullptr);
    bool ExportUserCommands(
        const std::filesystem::path& path) const;

    [[nodiscard]] const std::vector<Command>&
    Commands() const noexcept {
        return commands_;
    }

    [[nodiscard]] const std::vector<Command>&
    UserCommands() const noexcept {
        return userCommandStore_
            .Commands();
    }

    [[nodiscard]] const std::unordered_map<
        std::wstring,
        std::wstring>&
    LegacyIdMap() const noexcept {
        return userCommandStore_
            .LegacyIdMap();
    }

    [[nodiscard]] const std::filesystem::path&
    UserCommandsPath() const noexcept {
        return userCommandStore_
            .Path();
    }

    [[nodiscard]] bool
    UserCommandsReadOnlyDueToNewerSchema() const noexcept {
        return userCommandStore_
            .IsReadOnlyDueToNewerSchema();
    }

    [[nodiscard]] int
    UserCommandsUnsupportedSchemaVersion() const noexcept {
        return userCommandStore_
            .UnsupportedSchemaVersion();
    }

    [[nodiscard]] bool
    UserCommandsRecoveredFromBackup() const noexcept {
        return userCommandStore_
            .WasRecoveredFromBackup();
    }

private:
    struct ProviderRuntimeDiagnostic {
        std::int64_t lastAttemptUnix{0};
        bool lastAttemptSucceeded{true};
        std::wstring lastError;
    };

    void RebuildMergedCommands();

    std::filesystem::path
        baseDirectory_;
    std::filesystem::path
        dataDirectory_;
    UserCommandStore
        userCommandStore_;
    ProviderCache
        providerCache_;
    ProviderRegistry
        providerRegistry_;
    std::vector<Command>
        providerCommands_;
    std::vector<Command>
        commands_;
    CommandMergeStats
        mergeStats_;

    mutable std::mutex
        providerDiagnosticsMutex_;
    std::unordered_map<
        std::string,
        ProviderRuntimeDiagnostic>
        providerDiagnostics_;
};

} // namespace altrun
