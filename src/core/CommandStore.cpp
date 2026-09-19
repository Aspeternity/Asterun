#include "CommandStore.hpp"

#include "../platform/WinUtil.hpp"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shlobj.h>

#include <fstream>
#include <unordered_set>

namespace altrun {

namespace {

std::wstring MakeId(std::wstring_view prefix, std::wstring_view key, std::wstring_view target) {
    return std::wstring(prefix) + L":" + win::Lower(key) + L":" + win::Lower(target);
}

std::filesystem::path KnownFolder(REFKNOWNFOLDERID id) {
    PWSTR path = nullptr;
    if (FAILED(SHGetKnownFolderPath(id, KF_FLAG_DEFAULT, nullptr, &path)) || path == nullptr) {
        return {};
    }
    std::filesystem::path result(path);
    CoTaskMemFree(path);
    return result;
}

} // namespace

CommandStore::CommandStore(std::filesystem::path baseDirectory)
    : baseDirectory_(std::move(baseDirectory)),
      customCommandsPath_(baseDirectory_ / "commands.tsv") {}

void CommandStore::Reload() {
    commands_.clear();
    EnsureDefaultCustomCommands();
    LoadCustomCommands();
    LoadStartMenuCommands();
}

void CommandStore::EnsureDefaultCustomCommands() {
    if (std::filesystem::exists(customCommandsPath_)) return;

    std::ofstream out(customCommandsPath_, std::ios::binary);
    out << "# ALTRun Next custom commands (UTF-8, tab-separated)\n";
    out << "# keyword<TAB>title<TAB>target<TAB>arguments<TAB>working_directory\n";
    out << "np\tNotepad\tnotepad.exe\t\t\n";
    out << "calc\tCalculator\tcalc.exe\t\t\n";
    out << "cmd\tCommand Prompt\tcmd.exe\t\t\n";
    out << "explorer\tFile Explorer\texplorer.exe\t\t\n";
    out << "pwsh\tPowerShell\tpowershell.exe\t\t\n";
}

void CommandStore::LoadCustomCommands() {
    std::ifstream input(customCommandsPath_, std::ios::binary);
    if (!input) return;

    std::string utf8Line;
    while (std::getline(input, utf8Line)) {
        if (!utf8Line.empty() && utf8Line.back() == '\r') utf8Line.pop_back();
        if (utf8Line.empty() || utf8Line[0] == '#') continue;

        const auto line = win::Utf8ToWide(utf8Line);
        auto fields = win::SplitTabs(line);
        if (fields.size() < 3) continue;
        while (fields.size() < 5) fields.emplace_back();

        Command command;
        command.keyword = win::Trim(fields[0]);
        command.title = win::Trim(fields[1]);
        command.target = win::Trim(fields[2]);
        command.arguments = win::Trim(fields[3]);
        command.workingDirectory = win::Trim(fields[4]);
        command.source = CommandSource::Custom;
        command.basePriority = 120;

        if (command.keyword.empty() || command.target.empty()) continue;
        if (command.title.empty()) command.title = command.keyword;
        command.id = MakeId(L"custom", command.keyword, command.target);
        AddCommand(std::move(command));
    }
}

void CommandStore::LoadStartMenuCommands() {
    ScanStartMenuPath(KnownFolder(FOLDERID_StartMenu));
    ScanStartMenuPath(KnownFolder(FOLDERID_CommonStartMenu));
}

void CommandStore::ScanStartMenuPath(const std::filesystem::path& root) {
    if (root.empty() || !std::filesystem::exists(root)) return;

    std::error_code ec;
    for (std::filesystem::recursive_directory_iterator it(root, std::filesystem::directory_options::skip_permission_denied, ec), end;
         it != end; it.increment(ec)) {
        if (ec) {
            ec.clear();
            continue;
        }
        if (!it->is_regular_file(ec)) continue;

        const auto extension = win::Lower(it->path().extension().wstring());
        if (extension != L".lnk" && extension != L".url" && extension != L".exe") continue;

        Command command;
        command.title = it->path().stem().wstring();
        command.keyword = win::CompactKeyword(command.title);
        command.target = it->path().wstring();
        command.source = CommandSource::StartMenu;
        command.basePriority = 0;
        if (command.keyword.empty()) command.keyword = win::Lower(command.title);
        command.id = MakeId(L"start", command.keyword, command.target);
        AddCommand(std::move(command));
    }
}

void CommandStore::AddCommand(Command command) {
    const std::wstring targetKey = win::Lower(command.target);
    for (const auto& existing : commands_) {
        if (win::Lower(existing.target) == targetKey && win::Lower(existing.keyword) == win::Lower(command.keyword)) {
            return;
        }
    }
    commands_.push_back(std::move(command));
}

} // namespace altrun
