#include "StartMenuProvider.hpp"

#include "ProviderFingerprint.hpp"
#include "ProviderIds.hpp"
#include "../platform/WinUtil.hpp"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shlobj.h>

#include <algorithm>
#include <cstdint>
#include <system_error>

namespace altrun {

namespace {

std::filesystem::path KnownFolder(
    REFKNOWNFOLDERID id) {

    PWSTR path = nullptr;

    if (FAILED(SHGetKnownFolderPath(
            id,
            KF_FLAG_DEFAULT,
            nullptr,
            &path)) ||
        path == nullptr) {
        return {};
    }

    std::filesystem::path result(path);
    CoTaskMemFree(path);
    return result;
}

std::wstring MakeId(
    std::wstring_view keyword,
    std::wstring_view target) {

    return L"start:" +
        win::Lower(keyword) +
        L":" +
        win::Lower(target);
}

bool IsStartMenuEntry(
    const std::filesystem::path& path) {

    const auto extension =
        win::Lower(
            path.extension().wstring());

    return extension == L".lnk" ||
           extension == L".url" ||
           extension == L".exe";
}

} // namespace

const ProviderDescriptor&
StartMenuProvider::Descriptor() const noexcept {
    static const ProviderDescriptor descriptor{
        std::string(providers::kStartMenu),
        L"Start Menu",
        true,
        40,
    };
    return descriptor;
}

std::vector<Command>
StartMenuProvider::Discover() const {
    std::vector<Command> commands;

    ScanPath(
        KnownFolder(FOLDERID_StartMenu),
        commands);
    ScanPath(
        KnownFolder(FOLDERID_CommonStartMenu),
        commands);

    return commands;
}

std::uint64_t
StartMenuProvider::ChangeToken() const {
    std::vector<std::uint64_t> items;

    FingerprintPath(
        KnownFolder(FOLDERID_StartMenu),
        items);
    FingerprintPath(
        KnownFolder(FOLDERID_CommonStartMenu),
        items);

    std::sort(
        items.begin(),
        items.end());

    std::uint64_t hash =
        fingerprint::kOffset;

    fingerprint::Mix(
        hash,
        static_cast<std::uint64_t>(
            items.size()));

    for (const auto item : items) {
        fingerprint::Mix(
            hash,
            item);
    }

    return hash;
}

void StartMenuProvider::ScanPath(
    const std::filesystem::path& root,
    std::vector<Command>& output) const {

    if (root.empty() ||
        !std::filesystem::exists(root)) {
        return;
    }

    std::error_code ec;

    for (std::filesystem::recursive_directory_iterator it(
             root,
             std::filesystem::directory_options::
                 skip_permission_denied,
             ec),
         end;
         it != end;
         it.increment(ec)) {

        if (ec) {
            ec.clear();
            continue;
        }

        if (!it->is_regular_file(ec) ||
            !IsStartMenuEntry(
                it->path())) {
            continue;
        }

        Command command;
        command.title =
            it->path().stem().wstring();
        command.keyword =
            win::CompactKeyword(
                command.title);
        command.target =
            it->path().wstring();
        command.type =
            CommandType::Application;
        command.icon = L"auto";
        command.enabled = true;
        command.source =
            CommandSource::StartMenu;
        command.basePriority = 0;

        if (command.keyword.empty()) {
            command.keyword =
                win::Lower(
                    command.title);
        }

        command.id =
            MakeId(
                command.keyword,
                command.target);

        output.push_back(
            std::move(command));
    }
}

void StartMenuProvider::FingerprintPath(
    const std::filesystem::path& root,
    std::vector<std::uint64_t>& items) const {

    if (root.empty()) {
        return;
    }

    std::error_code ec;

    if (!std::filesystem::is_directory(
            root,
            ec)) {
        return;
    }

    for (std::filesystem::recursive_directory_iterator it(
             root,
             std::filesystem::directory_options::
                 skip_permission_denied,
             ec),
         end;
         it != end;
         it.increment(ec)) {

        if (ec) {
            ec.clear();
            continue;
        }

        if (!it->is_regular_file(ec) ||
            !IsStartMenuEntry(
                it->path())) {
            continue;
        }

        std::uint64_t itemHash =
            fingerprint::kOffset;

        fingerprint::Mix(
            itemHash,
            it->path().wstring());

        const auto writeTime =
            it->last_write_time(ec);

        if (!ec) {
            fingerprint::Mix(
                itemHash,
                static_cast<std::uint64_t>(
                    writeTime
                        .time_since_epoch()
                        .count()));
        } else {
            ec.clear();
        }

        const auto size =
            it->file_size(ec);

        if (!ec) {
            fingerprint::Mix(
                itemHash,
                static_cast<std::uint64_t>(
                    size));
        } else {
            ec.clear();
        }

        items.push_back(
            itemHash);
    }
}

} // namespace altrun
