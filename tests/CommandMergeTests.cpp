#include "core/CommandMerge.hpp"

#include <cassert>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

using namespace altrun;

namespace {

Command Make(
    std::wstring id,
    std::wstring title,
    std::wstring keyword,
    std::wstring target,
    CommandSource source,
    bool enabled = true) {

    Command command;
    command.id = std::move(id);
    command.title = std::move(title);
    command.keyword = std::move(keyword);
    command.target = std::move(target);
    command.source = source;
    command.enabled = enabled;
    return command;
}

bool HasId(
    const std::vector<Command>& commands,
    std::wstring_view id) {

    for (const auto& command :
         commands) {
        if (command.id == id) {
            return true;
        }
    }

    return false;
}

} // namespace

int main() {
    {
        const std::vector<Command> users{
            Make(
                L"user:code",
                L"Visual Studio Code",
                L"vscode",
                L"C:\\Apps\\Code.exe",
                CommandSource::User),
        };

        const std::vector<Command> providers{
            Make(
                L"path:code",
                L"Code",
                L"code",
                L"c:/apps/code.exe",
                CommandSource::Path),
        };

        const auto merged =
            MergeCommands(
                users,
                providers);

        assert(merged.commands.size() == 1);
        assert(HasId(
            merged.commands,
            L"user:code"));
        assert(
            merged.stats.acceptedUser == 1);
        assert(
            merged.stats.suppressedPath == 1);
    }

    {
        const std::vector<Command> providers{
            Make(
                L"path:terminal",
                L"Windows Terminal",
                L"terminal",
                L"C:\\Apps\\Terminal.exe",
                CommandSource::Path),
            Make(
                L"apppath:terminal",
                L"Windows Terminal",
                L"terminal",
                L"C:\\Apps\\Terminal.exe",
                CommandSource::AppPaths),
            Make(
                L"packaged:terminal",
                L"Windows Terminal",
                L"terminal",
                L"C:\\Apps\\Terminal.exe",
                CommandSource::PackagedApp),
            Make(
                L"start:terminal",
                L"Windows Terminal",
                L"terminal",
                L"C:\\Apps\\Terminal.exe",
                CommandSource::StartMenu),
        };

        const auto merged =
            MergeCommands(
                {},
                providers);

        assert(merged.commands.size() == 1);
        assert(HasId(
            merged.commands,
            L"start:terminal"));
        assert(
            merged.stats.acceptedStartMenu == 1);
        assert(
            merged.stats.suppressedPackaged == 1);
        assert(
            merged.stats.suppressedAppPaths == 1);
        assert(
            merged.stats.suppressedPath == 1);
    }

    {
        const std::vector<Command> providers{
            Make(
                L"apppath:notepad",
                L"Notepad",
                L"notepad",
                L"C:\\Windows\\notepad.exe",
                CommandSource::AppPaths),
            Make(
                L"packaged:notepad",
                L"Notepad",
                L"notepad",
                L"Microsoft.WindowsNotepad_8wekyb3d8bbwe!App",
                CommandSource::PackagedApp),
        };

        const auto merged =
            MergeCommands(
                {},
                providers);

        assert(merged.commands.size() == 1);
        assert(HasId(
            merged.commands,
            L"packaged:notepad"));
        assert(
            merged.stats.suppressedAppPaths == 1);
    }

    {
        const std::vector<Command> providers{
            Make(
                L"start:tool-a",
                L"Tool",
                L"toola",
                L"C:\\A\\tool.exe",
                CommandSource::StartMenu),
            Make(
                L"start:tool-b",
                L"Tool",
                L"toolb",
                L"C:\\B\\tool.exe",
                CommandSource::StartMenu),
        };

        const auto merged =
            MergeCommands(
                {},
                providers);

        assert(merged.commands.size() == 2);
    }

    {
        const std::vector<Command> users{
            Make(
                L"user:one",
                L"Tool",
                L"tool",
                L"C:\\Apps\\Tool.exe",
                CommandSource::User),
            Make(
                L"user:two",
                L"Tool copy",
                L"tool2",
                L"C:\\Apps\\Tool.exe",
                CommandSource::User),
        };

        const auto merged =
            MergeCommands(
                users,
                {});

        assert(merged.commands.size() == 2);
        assert(
            merged.stats.acceptedUser == 2);
    }

    {
        const std::vector<Command> providers{
            Make(
                L"start:disabled",
                L"Disabled",
                L"disabled",
                L"C:\\Disabled.exe",
                CommandSource::StartMenu,
                false),
            Make(
                L"path:enabled",
                L"Enabled",
                L"enabled",
                L"C:\\Enabled.exe",
                CommandSource::Path,
                true),
        };

        const auto merged =
            MergeCommands(
                {},
                providers);

        assert(merged.commands.size() == 1);
        assert(HasId(
            merged.commands,
            L"path:enabled"));
    }

    {
        const std::vector<Command> providers{
            Make(
                L"start:view",
                L"View App",
                L"view",
                L"C:\\View.exe",
                CommandSource::StartMenu),
            Make(
                L"path:view",
                L"View App",
                L"view",
                L"C:\\View.exe",
                CommandSource::Path),
        };

        const std::vector<const Command*>
            views{
                &providers[0],
                nullptr,
                &providers[1],
            };

        const auto merged =
            MergeCommandViews(
                {},
                views);

        assert(merged.commands.size() == 1);
        assert(HasId(
            merged.commands,
            L"start:view"));
        assert(
            merged.stats.suppressedPath == 1);
    }

    {
        // Provider dedupe must be stable even after the user promotes the
        // winning Start Menu entry into a personal shortcut. The lower
        // App Paths executable must not "revive" just because the Start Menu
        // target is now suppressed by an exact-target user shortcut.
        const std::vector<Command> users{
            Make(
                L"user:ts3",
                L"TeamSpeak 3 Client",
                L"ts3",
                L"C:\\ProgramData\\Microsoft\\Windows\\Start Menu\\Programs\\TeamSpeak 3 Client.lnk",
                CommandSource::User),
        };

        const std::vector<Command> providers{
            Make(
                L"start:teamspeak3",
                L"TeamSpeak 3 Client",
                L"teamspeak3client",
                L"C:\\ProgramData\\Microsoft\\Windows\\Start Menu\\Programs\\TeamSpeak 3 Client.lnk",
                CommandSource::StartMenu),
            Make(
                L"apppath:teamspeak3",
                L"TeamSpeak 3 Client",
                L"teamspeak3client",
                L"D:\\TeamSpeak 3 Client\\ts3client_win64.exe",
                CommandSource::AppPaths),
            Make(
                L"start:teamspeak6",
                L"TeamSpeak",
                L"teamspeak",
                L"C:\\ProgramData\\Microsoft\\Windows\\Start Menu\\Programs\\TeamSpeak.lnk",
                CommandSource::StartMenu),
        };

        const auto merged =
            MergeCommands(
                users,
                providers);

        assert(merged.commands.size() == 2);
        assert(HasId(
            merged.commands,
            L"user:ts3"));
        assert(HasId(
            merged.commands,
            L"start:teamspeak6"));
        assert(!HasId(
            merged.commands,
            L"start:teamspeak3"));
        assert(!HasId(
            merged.commands,
            L"apppath:teamspeak3"));
        assert(
            merged.stats.suppressedStartMenu == 1);
        assert(
            merged.stats.suppressedAppPaths == 1);
    }

    std::cout
        << "Command merge regression tests passed\n";

    return 0;
}
