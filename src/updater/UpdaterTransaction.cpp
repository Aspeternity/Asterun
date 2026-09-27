#include "UpdaterTransaction.hpp"

#include <algorithm>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace altrun::updater {
namespace {

[[nodiscard]] std::wstring
LowerPath(
    const std::filesystem::path& path) {
    std::wstring value =
        path.lexically_normal()
            .wstring();

    std::transform(
        value.begin(),
        value.end(),
        value.begin(),
        [](wchar_t c) {
            if (c == L'/') {
                return L'\\';
            }
            return static_cast<wchar_t>(
                std::towlower(c));
        });

    while (value.size() > 3 &&
           value.back() == L'\\') {
        value.pop_back();
    }

    return value;
}

[[nodiscard]] bool
ReadTrimmedText(
    const std::filesystem::path& path,
    std::wstring& value) {
    std::ifstream input(
        path,
        std::ios::binary);

    if (!input) {
        return false;
    }

    std::string content{
        std::istreambuf_iterator<char>{
            input},
        std::istreambuf_iterator<char>{}};

    while (!content.empty() &&
           (content.back() == '\r' ||
            content.back() == '\n' ||
            content.back() == ' ' ||
            content.back() == '\t')) {
        content.pop_back();
    }

    value.clear();
    value.reserve(content.size());

    for (unsigned char c : content) {
        if (c > 0x7f) {
            return false;
        }

        value.push_back(
            static_cast<wchar_t>(c));
    }

    return !value.empty();
}

[[nodiscard]] bool
IsDataRelative(
    const std::filesystem::path& relative) {
    const auto first =
        relative.begin();

    if (relative.empty() ||
        first == relative.end()) {
        return false;
    }

    std::wstring value =
        first->wstring();

    std::transform(
        value.begin(),
        value.end(),
        value.begin(),
        [](wchar_t c) {
            return static_cast<wchar_t>(
                std::towlower(c));
        });

    return value == L"data";
}

[[nodiscard]] bool
IsReparsePoint(
    const std::filesystem::path& path) {
#ifdef _WIN32
    const DWORD attributes =
        GetFileAttributesW(
            path.c_str());

    return attributes !=
               INVALID_FILE_ATTRIBUTES &&
        (attributes &
         FILE_ATTRIBUTE_REPARSE_POINT) != 0;
#else
    std::error_code ec;
    return std::filesystem::is_symlink(
               std::filesystem::
                   symlink_status(
                       path,
                       ec)) &&
        !ec;
#endif
}

[[nodiscard]] bool
PlainDirectory(
    const std::filesystem::path& path) {
    std::error_code ec;

    return
        std::filesystem::is_directory(
            path,
            ec) &&
        !ec &&
        !IsReparsePoint(path);
}

[[nodiscard]] bool
EnsureInstallDirectory(
    const TransactionPaths& paths,
    const std::filesystem::path& relative,
    TransactionJournal& journal) {
    if (relative.empty() ||
        relative ==
            std::filesystem::path(
                L".")) {
        return PlainDirectory(
            paths.install);
    }

    std::filesystem::path current =
        paths.install;
    std::filesystem::path currentRelative;
    std::error_code ec;

    for (const auto& component :
         relative) {
        current /=
            component;
        currentRelative /=
            component;

        const bool exists =
            std::filesystem::exists(
                current,
                ec);

        if (ec) {
            return false;
        }

        if (exists) {
            if (!PlainDirectory(
                    current)) {
                return false;
            }

            continue;
        }

        ec.clear();

        if (!std::filesystem::
                 create_directory(
                     current,
                     ec) ||
            ec) {
            return false;
        }

        journal.createdDirectories
            .push_back(
                currentRelative);
    }

    return true;
}

[[nodiscard]] bool
CopyOneFile(
    const TransactionPaths& paths,
    const std::filesystem::path& source,
    const std::filesystem::path& relative,
    TransactionJournal& journal) {
    const auto destination =
        paths.install /
        relative;
    const auto backup =
        paths.backup /
        relative;

    if (!EnsureInstallDirectory(
            paths,
            relative.parent_path(),
            journal)) {
        return false;
    }

    std::error_code ec;
    const bool exists =
        std::filesystem::exists(
            destination,
            ec);

    if (ec) {
        return false;
    }

    const bool wasNew =
        !exists;

    if (!wasNew) {
        if (!std::filesystem::
                 is_regular_file(
                     destination,
                     ec) ||
            ec ||
            IsReparsePoint(
                destination)) {
            return false;
        }

        std::filesystem::
            create_directories(
                backup.parent_path(),
                ec);

        if (ec) {
            return false;
        }

        std::filesystem::copy_file(
            destination,
            backup,
            std::filesystem::
                copy_options::
                    overwrite_existing,
            ec);

        if (ec) {
            return false;
        }
    }

    std::filesystem::copy_file(
        source,
        destination,
        std::filesystem::
            copy_options::
                overwrite_existing,
        ec);

    if (ec) {
        std::error_code recoveryError;

        if (wasNew) {
            std::filesystem::remove(
                destination,
                recoveryError);
        } else {
            std::filesystem::copy_file(
                backup,
                destination,
                std::filesystem::
                    copy_options::
                        overwrite_existing,
                recoveryError);
        }

        return false;
    }

    journal.files.push_back({
        relative,
        wasNew,
    });

    return true;
}

struct SourceEntry {
    std::filesystem::path path;
    std::filesystem::path relative;
    bool directory{false};
};

[[nodiscard]] bool
CollectSourceEntries(
    const TransactionPaths& paths,
    std::vector<SourceEntry>& entries) {
    std::error_code ec;

    for (std::filesystem::
             recursive_directory_iterator
             it(paths.source, ec),
         end;
         !ec && it != end;
         it.increment(ec)) {
        const auto relative =
            std::filesystem::relative(
                it->path(),
                paths.source,
                ec);

        if (ec ||
            relative.empty() ||
            relative ==
                std::filesystem::path(
                    L".")) {
            return false;
        }

        if (IsDataRelative(
                relative)) {
            if (it->is_directory(ec) &&
                !ec) {
                it.disable_recursion_pending();
            }

            ec.clear();
            continue;
        }

        if (IsReparsePoint(
                it->path())) {
            return false;
        }

        const bool directory =
            it->is_directory(ec);

        if (ec) {
            return false;
        }

        if (!directory &&
            !it->is_regular_file(ec)) {
            return false;
        }

        if (ec) {
            return false;
        }

        entries.push_back({
            it->path(),
            relative,
            directory,
        });
    }

    if (ec) {
        return false;
    }

    std::sort(
        entries.begin(),
        entries.end(),
        [](const SourceEntry& left,
           const SourceEntry& right) {
            return left.relative
                       .generic_wstring() <
                right.relative
                    .generic_wstring();
        });

    return true;
}

} // namespace

bool ValidateSource(
    const TransactionPaths& paths) {
    std::error_code ec;

    if (paths.source.empty() ||
        paths.install.empty() ||
        paths.backup.empty() ||
        !paths.source.is_absolute() ||
        !paths.install.is_absolute() ||
        !paths.backup.is_absolute() ||
        paths.install ==
            paths.install.root_path() ||
        LowerPath(paths.source) ==
            LowerPath(paths.install) ||
        LowerPath(paths.backup) ==
            LowerPath(paths.install) ||
        !PlainDirectory(
            paths.source) ||
        !PlainDirectory(
            paths.install)) {
        return false;
    }

    for (const auto* name : {
             L"ALTRunNext.exe",
             L"Update.exe",
             L"Uninstall.exe",
             L"VERSION",
         }) {
        const auto path =
            paths.source /
            name;

        if (!std::filesystem::
                 is_regular_file(
                     path,
                     ec) ||
            ec ||
            IsReparsePoint(path)) {
            return false;
        }
    }

    std::wstring stagedVersion;

    return ReadTrimmedText(
               paths.source /
                   L"VERSION",
               stagedVersion) &&
        stagedVersion ==
            paths.version;
}

bool ApplyPackage(
    const TransactionPaths& paths,
    TransactionJournal& journal) {
    journal.files.clear();
    journal.createdDirectories.clear();

    if (!ValidateSource(paths)) {
        return false;
    }

    std::error_code ec;

    std::filesystem::remove_all(
        paths.backup,
        ec);
    ec.clear();

    std::filesystem::create_directories(
        paths.backup,
        ec);

    if (ec) {
        return false;
    }

    std::vector<SourceEntry>
        entries;

    if (!CollectSourceEntries(
            paths,
            entries)) {
        return false;
    }

    for (const auto& entry :
         entries) {
        if (entry.directory) {
            if (!EnsureInstallDirectory(
                    paths,
                    entry.relative,
                    journal)) {
                return false;
            }

            continue;
        }

        if (!CopyOneFile(
                paths,
                entry.path,
                entry.relative,
                journal)) {
            return false;
        }
    }

    return true;
}

void Rollback(
    const TransactionPaths& paths,
    const TransactionJournal& journal) {
    std::error_code ec;

    for (auto it =
             journal.files.rbegin();
         it !=
             journal.files.rend();
         ++it) {
        const auto destination =
            paths.install /
            it->relative;

        if (it->wasNew) {
            std::filesystem::remove(
                destination,
                ec);
            ec.clear();
            continue;
        }

        const auto backup =
            paths.backup /
            it->relative;

        if (std::filesystem::
                is_regular_file(
                    backup,
                    ec) &&
            !ec &&
            !IsReparsePoint(
                destination)) {
            std::filesystem::copy_file(
                backup,
                destination,
                std::filesystem::
                    copy_options::
                        overwrite_existing,
                ec);
        }

        ec.clear();
    }

    for (auto it =
             journal.createdDirectories
                 .rbegin();
         it !=
             journal.createdDirectories
                 .rend();
         ++it) {
        const auto directory =
            paths.install /
            *it;

        if (PlainDirectory(
                directory)) {
            std::filesystem::remove(
                directory,
                ec);
        }

        ec.clear();
    }
}

} // namespace altrun::updater
