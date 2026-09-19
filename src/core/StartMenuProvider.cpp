#include "StartMenuProvider.hpp"

#include "../platform/WinUtil.hpp"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shlobj.h>

namespace altrun {

namespace {

std::filesystem::path KnownFolder(REFKNOWNFOLDERID id) {
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

} // namespace

std::vector<Command> StartMenuProvider::Discover() const {
    std::vector<Command> commands;
    ScanPath(KnownFolder(FOLDERID_StartMenu), commands);
    ScanPath(KnownFolder(FOLDERID_CommonStartMenu), commands);
    return commands;
}

void StartMenuProvider::ScanPath(
    const std::filesystem::path& root,
    std::vector<Command>& output) const {

    if (root.empty() || !std::filesystem::exists(root)) return;

    std::error_code ec;

    for (std::filesystem::recursive_directory_iterator it(
             root,
             std::filesystem::directory_options::skip_permission_denied,
             ec),
         end;
         it != end;
         it.increment(ec)) {

        if (ec) {
            ec.clear();
            continue;
        }

        if (!it->is_regular_file(ec)) continue;

        const auto extension =
            win::Lower(it->path().extension().wstring());

        if (extension != L".lnk" &&
            extension != L".url" &&
            extension != L".exe") {
            continue;
        }

        Command command;
        command.title = it->path().stem().wstring();
        command.keyword = win::CompactKeyword(command.title);
        command.target = it->path().wstring();
        command.type = CommandType::Application;
        command.icon = L"auto";
        command.enabled = true;
        command.source = CommandSource::StartMenu;
        command.basePriority = 0;

        if (command.keyword.empty()) {
            command.keyword = win::Lower(command.title);
        }

        command.id = MakeId(command.keyword, command.target);
        output.push_back(std::move(command));
    }
}

} // namespace altrun
