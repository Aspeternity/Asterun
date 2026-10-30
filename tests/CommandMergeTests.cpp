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
        // App Paths is a run-by-name table. Its MSIX entries name their owning
        // application through the per-user execution alias: an alias of a
        // listed application folds into it, an unlisted alias of a product
        // with a listed application is a component reachable only by its
        // exact name, and a headless package's alias is the product itself.
        const auto find = [](const std::vector<Command>& commands,
                             std::wstring_view id) -> const Command* {
            for (const auto& command : commands) {
                if (command.id == id) {
                    return &command;
                }
            }
            return nullptr;
        };
        const auto alias = [](std::wstring id, std::wstring title,
                              std::wstring appId) {
            auto command = Make(
                std::move(id), title, title,
                L"C:\\Program Files\\WindowsApps\\Pkg\\" + title + L".exe",
                CommandSource::AppPaths, true,
                L"file:c:\\program files\\windowsapps\\pkg\\" + title + L".exe");
            command.runByName.packagedAppId = std::move(appId);
            command.distinctiveTokens = {L"claude", L"ssh", L"proxy"};
            return command;
        };

        const auto claude = Make(
            L"packaged:claude", L"Claude", L"claude",
            L"Claude_pzs8sxrjxfjjc!Claude", CommandSource::PackagedApp,
            true, L"aumid:claude_pzs8sxrjxfjjc!claude");
        const auto paint = Make(
            L"packaged:paint", L"Paint", L"paint",
            L"Microsoft.Paint_8wekyb3d8bbwe!App", CommandSource::PackagedApp,
            true, L"aumid:microsoft.paint_8wekyb3d8bbwe!app");
        const auto proxy = alias(L"apppath:proxy", L"claude-ssh-proxy",
                                 L"Claude_pzs8sxrjxfjjc!SshProxy");
        const auto mspaint = alias(L"apppath:mspaint", L"mspaint",
                                   L"Microsoft.Paint_8wekyb3d8bbwe!App");
        const auto wsl = alias(
            L"apppath:wsl", L"wsl",
            L"MicrosoftCorporationII.WindowsSubsystemForLinux_8wekyb3d8bbwe!wsl");
        auto server = alias(
            L"apppath:server", L"WindowsPackageManagerServer",
            L"Microsoft.DesktopAppInstaller_8wekyb3d8bbwe!WinGetComServer");
        server.runByName.packageServer = true;

        const auto merged = MergeCommands(
            {}, {proxy, mspaint, wsl, server, claude, paint});

        assert(!HasId(merged.commands, L"apppath:mspaint"));
        const Command* paintEntry = find(merged.commands, L"packaged:paint");
        assert(paintEntry != nullptr);
        assert(paintEntry->aliases == std::vector<std::wstring>{L"mspaint"});

        const Command* proxyEntry = find(merged.commands, L"apppath:proxy");
        assert(proxyEntry != nullptr);
        assert(proxyEntry->catalogVisibility ==
               CatalogVisibility::StrongMatchOnly);
        assert(proxyEntry->distinctiveTokens.empty());

        const Command* wslEntry = find(merged.commands, L"apppath:wsl");
        assert(wslEntry != nullptr);
        assert(wslEntry->catalogVisibility == CatalogVisibility::Normal);
        assert(!wslEntry->distinctiveTokens.empty());

        const Command* serverEntry = find(merged.commands, L"apppath:server");
        assert(serverEntry != nullptr);
        assert(serverEntry->catalogVisibility == CatalogVisibility::Hidden);

        // Only what the catalog actually lists counts as the product's
        // surface: without the Windows Apps entry the alias stays normal.
        const auto unlisted = MergeCommands({}, {proxy});
        assert(find(unlisted.commands, L"apppath:proxy")->catalogVisibility ==
               CatalogVisibility::Normal);
    }

    {
        // Win32 App Paths: a name that duplicates a listed application folds
        // into it as an alias; Windows-serviced and Common Files executables
        // are components; so is anything inside an install root that already
        // has a listed application. A product whose only surface is App
        // Paths (no Start Menu shortcut) stays a normal application.
        const auto find = [](const std::vector<Command>& commands,
                             std::wstring_view id) -> const Command* {
            for (const auto& command : commands) {
                if (command.id == id) {
                    return &command;
                }
            }
            return nullptr;
        };
        const auto appPath = [](std::wstring id, std::wstring title,
                                std::wstring target, std::wstring root) {
            auto command = Make(std::move(id), title, title, target,
                                CommandSource::AppPaths, true,
                                L"file:" + target);
            command.runByName.productRoot = std::move(root);
            command.distinctiveTokens = {title};
            return command;
        };

        auto word = Make(
            L"start:word", L"Word", L"word", L"C:\\Start\\Word.lnk",
            CommandSource::StartMenu, true,
            L"file:c:\\program files\\microsoft office\\root\\office16\\winword.exe");
        auto notepadpp = Make(
            L"start:npp", L"Notepad++", L"notepad", L"C:\\Start\\Notepad++.lnk",
            CommandSource::StartMenu, true, L"file:d:\\notepad++\\notepad++.exe");
        auto manual = Make(
            L"start:manual", L"Acme Manual", L"acmemanual",
            L"C:\\Start\\Acme Manual.lnk", CommandSource::StartMenu, true,
            L"file:d:\\acme\\manual.exe");
        manual.catalogVisibility = CatalogVisibility::Hidden;

        const auto winword = appPath(
            L"apppath:winword", L"WINWORD",
            L"c:\\program files\\microsoft office\\root\\office16\\winword.exe",
            L"c:\\program files\\microsoft office");
        const auto npp = appPath(
            L"apppath:npp", L"notepad++", L"d:\\notepad++\\notepad++.exe",
            L"d:\\notepad++");
        const auto msoasb = appPath(
            L"apppath:msoasb", L"msoasb",
            L"c:\\program files\\microsoft office\\root\\office16\\msoasb.exe",
            L"c:\\program files\\microsoft office");
        auto wab = appPath(L"apppath:wab", L"wab",
                           L"c:\\program files\\windows mail\\wab.exe",
                           L"c:\\program files\\windows mail");
        wab.runByName.osComponent = true;
        auto tabtip = appPath(
            L"apppath:tabtip", L"TabTip",
            L"c:\\program files\\common files\\microsoft shared\\ink\\tabtip.exe",
            L"c:\\program files\\common files\\microsoft shared\\ink");
        tabtip.runByName.sharedComponent = true;
        const auto vmware = appPath(
            L"apppath:vmware", L"vmware",
            L"d:\\vmware\\vmware workstation\\vmware.exe",
            L"d:\\vmware\\vmware workstation");
        const auto acme = appPath(L"apppath:acme", L"acme",
                                  L"d:\\acme\\acme.exe", L"d:\\acme");
        // The listed shortcut runs the same EXE with a profile argument.
        auto googleChrome = Make(
            L"start:chrome", L"Google Chrome", L"googlechrome",
            L"C:\\Start\\Google Chrome.lnk", CommandSource::StartMenu, true,
            L"file:c:\\chrome\\chrome.exe|args:--profile-directory=Default");
        const auto chrome = appPath(L"apppath:chrome", L"chrome",
                                    L"c:\\chrome\\chrome.exe", L"c:\\chrome");

        const auto merged = MergeCommands(
            {}, {winword, npp, msoasb, wab, tabtip, vmware, acme, chrome,
                 word, notepadpp, manual, googleChrome});

        // The shortcut's arguments make it a distinct action, so the plain
        // EXE survives, but as a component of the listed product.
        const Command* chromeEntry = find(merged.commands, L"apppath:chrome");
        assert(chromeEntry != nullptr);
        assert(chromeEntry->catalogVisibility ==
               CatalogVisibility::StrongMatchOnly);
        assert(find(merged.commands, L"start:chrome")->aliases.empty());
        assert(merged.stats.suppressedAppPaths == 2);

        assert(!HasId(merged.commands, L"apppath:winword"));
        assert(find(merged.commands, L"start:word")->aliases ==
               std::vector<std::wstring>{L"winword"});
        assert(!HasId(merged.commands, L"apppath:npp"));
        assert(find(merged.commands, L"start:npp")->aliases.empty());

        for (std::wstring_view id :
             {L"apppath:msoasb", L"apppath:wab", L"apppath:tabtip"}) {
            const Command* entry = find(merged.commands, id);
            assert(entry != nullptr);
            assert(entry->catalogVisibility ==
                   CatalogVisibility::StrongMatchOnly);
            assert(entry->distinctiveTokens.empty());
        }

        assert(find(merged.commands, L"apppath:vmware")->catalogVisibility ==
               CatalogVisibility::Normal);
        // A hidden listing (documentation, uninstaller) is not the product's
        // surface, so the App Paths EXE remains its application.
        assert(find(merged.commands, L"apppath:acme")->catalogVisibility ==
               CatalogVisibility::Normal);
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
