#pragma once

#include <filesystem>
#include <string>
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

void Rollback(
    const TransactionPaths& paths,
    const TransactionJournal& journal);

} // namespace altrun::updater
