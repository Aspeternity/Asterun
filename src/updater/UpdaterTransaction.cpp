#include "UpdaterTransaction.hpp"

#include <algorithm>
#include <chrono>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <set>
#include <string>
#include <system_error>
#include <thread>
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

// Antivirus scanners and indexers briefly open freshly written executables,
// and the previous launcher's image can stay mapped for a moment after its
// process exits. Only those sharing failures are retried.
constexpr int kTransientFileAttempts = 20;
constexpr std::chrono::milliseconds
    kTransientFileDelay{250};

[[nodiscard]] bool
IsTransientFileError(
    const std::error_code& error) {
#ifdef _WIN32
    if (error.category() !=
        std::system_category()) {
        return false;
    }

    switch (error.value()) {
    case ERROR_SHARING_VIOLATION:
    case ERROR_LOCK_VIOLATION:
    case ERROR_ACCESS_DENIED:
    case ERROR_USER_MAPPED_FILE:
        return true;
    default:
        return false;
    }
#else
    return error ==
            std::errc::device_or_resource_busy ||
        error ==
            std::errc::text_file_busy;
#endif
}

template <typename Operation>
[[nodiscard]] bool
RetryTransient(
    Operation operation,
    std::error_code& ec) {
    for (int attempt = 1;; ++attempt) {
        ec.clear();
        operation(ec);

        if (!ec) {
            return true;
        }

        if (attempt >=
                kTransientFileAttempts ||
            !IsTransientFileError(ec)) {
            return false;
        }

        std::this_thread::sleep_for(
            kTransientFileDelay);
    }
}

// Writes `source` beside `destination`, then swaps it in with one rename, so
// a failed or interrupted copy never leaves a truncated application file.
[[nodiscard]] bool
ReplaceFileAtomically(
    const std::filesystem::path& source,
    const std::filesystem::path& destination,
    std::error_code& ec) {
    auto staged = destination;
    staged += L".asterun-update-new";

    std::error_code ignored;
    std::filesystem::remove(
        staged,
        ignored);

    if (!RetryTransient(
            [&](std::error_code& error) {
                std::filesystem::copy_file(
                    source,
                    staged,
                    std::filesystem::copy_options::
                        overwrite_existing,
                    error);
            },
            ec)) {
        std::filesystem::remove(
            staged,
            ignored);
        return false;
    }

    const bool replaced =
        RetryTransient(
            [&](std::error_code& error) {
#ifdef _WIN32
                if (!MoveFileExW(
                        staged.c_str(),
                        destination.c_str(),
                        MOVEFILE_REPLACE_EXISTING |
                            MOVEFILE_WRITE_THROUGH)) {
                    error = std::error_code(
                        static_cast<int>(
                            GetLastError()),
                        std::system_category());
                }
#else
                std::filesystem::rename(
                    staged,
                    destination,
                    error);
#endif
            },
            ec);

    if (!replaced) {
        std::filesystem::remove(
            staged,
            ignored);
    }

    return replaced;
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

        if (!RetryTransient(
                [&](std::error_code& error) {
                    std::filesystem::copy_file(
                        destination,
                        backup,
                        std::filesystem::
                            copy_options::
                                overwrite_existing,
                        error);
                },
                ec)) {
            return false;
        }
    }

    // Record before touching the destination so the caller's rollback always
    // covers it, even though a failed replace leaves the destination intact.
    journal.files.push_back({relative, wasNew});
    return ReplaceFileAtomically(
        source,
        destination,
        ec);
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
             L"Asterun.exe",
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

    // Validate the complete package before creating any recovery files.
    std::vector<SourceEntry>
        entries;
    if (!CollectSourceEntries(
            paths,
            entries)) {
        return false;
    }

    std::error_code ec;
    std::filesystem::create_directories(
        paths.backup.parent_path(),
        ec);
    if (ec) {
        return false;
    }

    // An existing directory may contain the only copy of a failed update's
    // recovery files. Never remove, merge into or overwrite it.
    if (!std::filesystem::create_directory(
            paths.backup,
            ec) || ec) {
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

RollbackResult Rollback(
    const TransactionPaths& paths,
    const TransactionJournal& journal) {
    RollbackResult result;
    const auto fail = [&](const std::filesystem::path& relative, std::error_code error) {
        result.failures.push_back({relative, error ? error :
            std::make_error_code(std::errc::io_error)});
    };
    for (auto it = journal.files.rbegin(); it != journal.files.rend(); ++it) {
        const auto destination = paths.install / it->relative;
        std::error_code ec;
        auto parent = destination.parent_path();
        bool safe = PlainDirectory(paths.install);
        while (safe && parent != paths.install && parent != parent.root_path()) {
            safe = PlainDirectory(parent);
            parent = parent.parent_path();
        }
        if (!safe || IsReparsePoint(destination)) {
            fail(it->relative, std::make_error_code(std::errc::permission_denied));
            continue;
        }
        if (it->wasNew) {
            (void)RetryTransient([&](std::error_code& error) {
                std::filesystem::remove(destination, error);
            }, ec);
        } else {
            const auto backup = paths.backup / it->relative;
            if (!std::filesystem::is_regular_file(backup, ec) || ec || IsReparsePoint(backup)) {
                fail(it->relative, ec);
                continue;
            }
            (void)ReplaceFileAtomically(backup, destination, ec);
        }
        if (ec) fail(it->relative, ec);
    }
    for (auto it = journal.createdDirectories.rbegin();
         it != journal.createdDirectories.rend(); ++it) {
        const auto directory = paths.install / *it;
        std::error_code ec;
        if (PlainDirectory(directory)) {
            std::filesystem::remove(directory, ec);
        } else if (IsReparsePoint(directory) ||
                   (std::filesystem::exists(directory, ec) && !ec)) {
            ec = std::make_error_code(std::errc::permission_denied);
        }
        if (ec) fail(*it, ec);
    }
    return result;
}

void PruneObsoletePackageFiles(
    const TransactionPaths& paths,
    const TransactionJournal& journal) {
    std::set<std::wstring> shipped;
    for (const auto& record : journal.files) {
        shipped.insert(LowerPath(record.relative));
    }

    // Only trees that belong wholly to the package; top-level user files,
    // data/ and recovery directories are never candidates.
    for (const auto* tree : {L"dict", L"third_party"}) {
        const auto root = paths.install / tree;
        if (!PlainDirectory(root)) {
            continue;
        }

        std::vector<std::filesystem::path> obsolete;
        std::vector<std::filesystem::path> directories{root};
        std::error_code ec;
        for (std::filesystem::recursive_directory_iterator it(root, ec), end;
             !ec && it != end; it.increment(ec)) {
            if (IsReparsePoint(it->path())) {
                it.disable_recursion_pending();
                continue;
            }
            // Relative to the tree the iterator walks, so the spelling of
            // paths.install (a trailing separator, say) cannot mismatch.
            const auto inTree = it->path().lexically_relative(root);
            if (inTree.empty() || *inTree.begin() == L"..") {
                continue;
            }
            std::error_code typeError;
            if (it->is_directory(typeError)) {
                directories.push_back(it->path());
            } else if (it->is_regular_file(typeError) &&
                       !shipped.contains(LowerPath(
                           std::filesystem::path(tree) / inTree))) {
                obsolete.push_back(it->path());
            }
        }

        for (const auto& file : obsolete) {
            std::error_code ignored;
            std::filesystem::remove(file, ignored);
        }

        // Pre-order enumeration: children follow their parent, so removing in
        // reverse empties the deepest first. Non-empty directories stay.
        for (auto it = directories.rbegin(); it != directories.rend(); ++it) {
            std::error_code ignored;
            if (std::filesystem::is_empty(*it, ignored) && !ignored) {
                std::filesystem::remove(*it, ignored);
            }
        }
    }
}

} // namespace altrun::updater
