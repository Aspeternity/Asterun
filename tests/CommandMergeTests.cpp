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
    bool enabled = true,
    std::wstring canonicalIdentity = {}) {

    Command command;
    command.id = std::move(id);
    command.title = std::move(title);
    command.keyword = std::move(keyword);
    command.target = std::move(target);
    command.source = source;
    command.enabled = enabled;
    command.canonicalIdentity =
        std::move(canonicalIdentity);
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
        // A packaged app's App Paths EXE can be a noninteractive internal
        // entry point. Keep the registered AUMID even when its display name
        // differs; an unrelated package or an ordinary EXE is not a match.
        auto packaged = Make(
            L"packaged:acme", L"Acme App", L"acmeapp",
            L"Acme.ShopCenter_abc123!App", CommandSource::PackagedApp,
            true, L"aumid:acme.shopcenter_abc123!app");
        const auto stub = Make(
            L"apppath:stub", L"center", L"center",
            L"C:\\Program Files\\WindowsApps\\Acme.ShopCenter_1.2.3.0_x64__abc123\\Center.exe",
            CommandSource::AppPaths);
        const auto standalone = Make(
            L"apppath:standalone", L"independent", L"independent",
            L"C:\\Apps\\independent.exe", CommandSource::AppPaths);

        auto merged = MergeCommands({}, {stub, packaged, standalone});
        assert(!HasId(merged.commands, L"apppath:stub"));
        assert(HasId(merged.commands, L"packaged:acme"));
        assert(HasId(merged.commands, L"apppath:standalone"));
        assert(merged.stats.suppressedAppPaths == 1);

        auto unrelatedTool = stub;
        unrelatedTool.id = L"apppath:tool";
        unrelatedTool.target =
            L"C:\\Program Files\\WindowsApps\\Acme.ShopCenter_1.2.3.0_x64__abc123\\Helper.exe";
        merged = MergeCommands({}, {unrelatedTool, packaged});
        assert(HasId(merged.commands, L"apppath:tool"));

        packaged.canonicalIdentity = L"aumid:another.app_abc123!app";
        merged = MergeCommands({}, {stub, packaged});
        assert(HasId(merged.commands, L"apppath:stub"));

        merged = MergeCommands({}, {stub});
        assert(HasId(merged.commands, L"apppath:stub"));
    }

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
        // Canonical identity, not display text, owns provider dedupe.
        const auto chromeIdentity =
            BuildCanonicalLaunchIdentity(
                LaunchActivationKind::
                    ShellItem,
                L"C:\\Program Files\\Google\\Chrome\\Application\\chrome.exe");

        const std::vector<Command> providers{
            Make(
                L"start:googlechrome",
                L"Google Chrome",
                L"googlechrome",
                L"C:\\ProgramData\\Microsoft\\Windows\\Start Menu\\Programs\\Google Chrome.lnk",
                CommandSource::StartMenu,
                true,
                chromeIdentity),
            Make(
                L"apppath:chrome",
                L"chrome",
                L"chrome",
                L"C:\\Program Files\\Google\\Chrome\\Application\\chrome.exe",
                CommandSource::AppPaths,
                true,
                chromeIdentity),
        };

        const auto merged =
            MergeCommands(
                {},
                providers);

        assert(merged.commands.size() == 1);
        assert(HasId(
            merged.commands,
            L"start:googlechrome"));
        assert(
            merged.stats.suppressedAppPaths ==
            1);
    }

    {
        // A distinct launch action on the same executable is not the same
        // catalog identity and must survive provider canonicalization.
        const auto normalIdentity =
            BuildCanonicalLaunchIdentity(
                LaunchActivationKind::
                    ShellItem,
                L"C:\\Apps\\Browser.exe");

        const auto privateIdentity =
            BuildCanonicalLaunchIdentity(
                LaunchActivationKind::
                    ShellItem,
                L"C:\\Apps\\Browser.exe",
                L"--incognito");

        const std::vector<Command> providers{
            Make(
                L"apppath:browser",
                L"Browser",
                L"browser",
                L"C:\\Apps\\Browser.exe",
                CommandSource::AppPaths,
                true,
                normalIdentity),
            Make(
                L"start:browser-private",
                L"Browser Private",
                L"browserprivate",
                L"C:\\Start\\Browser Private.lnk",
                CommandSource::StartMenu,
                true,
                privateIdentity),
        };

        const auto merged =
            MergeCommands(
                {},
                providers);

        assert(merged.commands.size() == 2);
    }

    {
        // A user-created Desktop shortcut and a Start Menu Provider shortcut
        // may be different .lnk files but resolve to the same launch action.
        // The explicit User shortcut is authoritative when canonical identity
        // proves equivalence.
        const auto chromeIdentity =
            BuildCanonicalLaunchIdentity(
                LaunchActivationKind::
                    ShellItem,
                LR"(C:\Program Files\Google\Chrome\Application\chrome.exe)");

        const std::vector<Command> users{
            Make(
                L"user:chrome",
                L"Google Chrome",
                L"chrome",
                LR"(C:\Users\Public\Desktop\Google Chrome.lnk)",
                CommandSource::User,
                true,
                chromeIdentity),
        };

        const std::vector<Command> providers{
            Make(
                L"start:chrome",
                L"Google Chrome",
                L"googlechrome",
                LR"(C:\ProgramData\Microsoft\Windows\Start Menu\Programs\Google Chrome.lnk)",
                CommandSource::StartMenu,
                true,
                chromeIdentity),
        };

        const auto merged =
            MergeCommands(
                users,
                providers);

        assert(merged.commands.size() == 1);
        assert(HasId(
            merged.commands,
            L"user:chrome"));
        assert(
            merged.stats.acceptedUser == 1);
        assert(
            merged.stats.suppressedStartMenu ==
            1);
    }

    {
        // Same executable with a genuinely different action must remain two
        // results. Canonical identity includes arguments, preventing an
        // incognito/user action from suppressing the normal Provider entry.
        const auto normalIdentity =
            BuildCanonicalLaunchIdentity(
                LaunchActivationKind::
                    ShellItem,
                LR"(C:\Apps\Browser.exe)");

        const auto privateIdentity =
            BuildCanonicalLaunchIdentity(
                LaunchActivationKind::
                    ShellItem,
                LR"(C:\Apps\Browser.exe)",
                L"--incognito");

        const std::vector<Command> users{
            Make(
                L"user:browser-private",
                L"Browser Private",
                L"private",
                LR"(C:\Users\Public\Desktop\Browser.lnk)",
                CommandSource::User,
                true,
                privateIdentity),
        };

        const std::vector<Command> providers{
            Make(
                L"start:browser",
                L"Browser",
                L"browser",
                LR"(C:\ProgramData\Microsoft\Windows\Start Menu\Programs\Browser.lnk)",
                CommandSource::StartMenu,
                true,
                normalIdentity),
        };

        const auto merged =
            MergeCommands(
                users,
                providers);

        assert(merged.commands.size() == 2);
        assert(HasId(
            merged.commands,
            L"user:browser-private"));
        assert(HasId(
            merged.commands,
            L"start:browser"));
    }

    {
        // The same executable path with different explicit arguments is a
        // different launch action and must not be collapsed merely because
        // the surface path matches.
        const auto normalIdentity =
            BuildCanonicalLaunchIdentity(
                LaunchActivationKind::
                    ShellItem,
                LR"(C:\Apps\Browser.exe)");

        const auto privateIdentity =
            BuildCanonicalLaunchIdentity(
                LaunchActivationKind::
                    ShellItem,
                LR"(C:\Apps\Browser.exe)",
                L"--incognito");

        Command user = Make(
            L"user:browser-direct-private",
            L"Browser Private",
            L"private",
            LR"(C:\Apps\Browser.exe)",
            CommandSource::User,
            true,
            privateIdentity);
        user.arguments =
            L"--incognito";

        Command provider = Make(
            L"apppath:browser-direct",
            L"Browser",
            L"browser",
            LR"(C:\Apps\Browser.exe)",
            CommandSource::AppPaths,
            true,
            normalIdentity);

        const auto merged =
            MergeCommands(
                {user},
                {provider});

        assert(merged.commands.size() == 2);
        assert(HasId(
            merged.commands,
            L"user:browser-direct-private"));
        assert(HasId(
            merged.commands,
            L"apppath:browser-direct"));
    }

    {
        // Matching display text alone is never sufficient across User and
        // Provider boundaries.
        const std::vector<Command> users{
            Make(
                L"user:same-name",
                L"Tool",
                L"tool",
                LR"(C:\UserApps\Tool.exe)",
                CommandSource::User),
        };

        const std::vector<Command> providers{
            Make(
                L"start:same-name",
                L"Tool",
                L"tool",
                LR"(C:\ProviderApps\Tool.exe)",
                CommandSource::StartMenu),
        };

        const auto merged =
            MergeCommands(
                users,
                providers);

        assert(merged.commands.size() == 2);
    }

    {
        // Preserve the old user-promotion invariant with a generic fixture.
        const auto providerIdentity =
            BuildCanonicalLaunchIdentity(
                LaunchActivationKind::
                    ShellItem,
                L"C:\\Apps\\ChatClient.exe");

        const std::vector<Command> users{
            Make(
                L"user:chat",
                L"Chat Client",
                L"chat",
                L"C:\\Start\\Chat Client.lnk",
                CommandSource::User),
        };

        const std::vector<Command> providers{
            Make(
                L"start:chat",
                L"Chat Client",
                L"chatclient",
                L"C:\\Start\\Chat Client.lnk",
                CommandSource::StartMenu,
                true,
                providerIdentity),
            Make(
                L"apppath:chat",
                L"chatclient",
                L"chatclient",
                L"C:\\Apps\\ChatClient.exe",
                CommandSource::AppPaths,
                true,
                providerIdentity),
        };

        const auto merged =
            MergeCommands(
                users,
                providers);

        assert(merged.commands.size() == 1);
        assert(HasId(
            merged.commands,
            L"user:chat"));
        assert(
            merged.stats.suppressedStartMenu ==
            1);
        assert(
            merged.stats.suppressedAppPaths ==
            1);
    }

    std::cout
        << "Command merge regression tests passed\n";

    return 0;
}
