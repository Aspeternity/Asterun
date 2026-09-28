#pragma once

#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

namespace altrun::updater {

struct TransactionPaths {
    std::filesystem::path source;
    std::filesystem::path install;
    std::filesystem::path backup;
    std::wstring version;
};

struct FileApplyRecord {
    std::filesystem::path relative;
    bool wasNew{false};
};

struct TransactionJournal {
    std::vector<FileApplyRecord> files;
    std::vector<std::filesystem::path>
        createdDirectories;
};

[[nodiscard]] bool
ValidateSource(
    const TransactionPaths& paths);

[[nodiscard]] bool
ApplyPackage(
    const TransactionPaths& paths,
    TransactionJournal& journal);

struct RecoveryFailure {
    std::filesystem::path relative;
    std::error_code error;
};

struct RollbackResult {
    std::vector<RecoveryFailure> failures;
    [[nodiscard]] bool Complete() const noexcept { return failures.empty(); }
};

[[nodiscard]] RollbackResult Rollback(
    const TransactionPaths& paths,
    const TransactionJournal& journal);

} // namespace altrun::updater
