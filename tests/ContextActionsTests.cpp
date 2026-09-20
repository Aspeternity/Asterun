#include "core/ContextActions.hpp"

#include <cassert>
#include <iostream>

using namespace altrun;

int main() {
    {
        LauncherResult user;
        user.kind = ResultKind::UserCommand;
        user.title = L"google";
        user.subtitle = L"Google";
        user.target = L"https://www.google.com";
        user.action.kind =
            LauncherActionKind::ExecuteCommand;
        user.action.commandIndex = 2;

        const auto actions =
            EvaluateLauncherContextActions(
                user,
                true);

        assert(actions.primary);
        assert(actions.editShortcut);
        assert(actions.deleteShortcut);
        assert(!actions.addAsShortcut);
        assert(!actions.navigateCurrentFileManager);
        assert(!actions.locateInExplorer);
        assert(actions.copyTarget);
    }

    {
        LauncherResult app;
        app.kind = ResultKind::Application;
        app.title = L"telegram";
        app.subtitle = L"Telegram Desktop";
        app.target =
            L"C:\\Apps\\Telegram\\Telegram.exe";
        app.action.kind =
            LauncherActionKind::ExecuteCommand;
        app.action.commandIndex = 5;

        const auto actions =
            EvaluateLauncherContextActions(
                app,
                false);

        assert(actions.primary);
        assert(actions.addAsShortcut);
        assert(!actions.editShortcut);
        assert(!actions.deleteShortcut);
        assert(actions.locateInExplorer);
        assert(actions.copyTarget);

        const Command seed =
            ShortcutSeedFromLauncherResult(
                app);

        assert(seed.keyword.empty());
        assert(
            seed.title ==
            L"Telegram Desktop");
        assert(seed.target == app.target);
        assert(
            seed.type ==
            CommandType::Application);
        assert(seed.source == CommandSource::User);
    }

    {
        LauncherResult folder;
        folder.kind = ResultKind::Folder;
        folder.title = L"Projects";
        folder.subtitle = L"D:\\Work";
        folder.target =
            L"D:\\Work\\Projects";
        folder.action.kind =
            LauncherActionKind::OpenFolder;
        folder.action.payload =
            folder.target;

        const auto withoutContext =
            EvaluateLauncherContextActions(
                folder,
                false);
        assert(
            !withoutContext
                 .navigateCurrentFileManager);

        const auto withContext =
            EvaluateLauncherContextActions(
                folder,
                true);
        assert(
            withContext
                .navigateCurrentFileManager);
        assert(withContext.addAsShortcut);
        assert(withContext.locateInExplorer);

        const Command seed =
            ShortcutSeedFromLauncherResult(
                folder);
        assert(seed.keyword.empty());
        assert(seed.title == L"Projects");
        assert(
            seed.type ==
            CommandType::Folder);
    }

    {
        LauncherResult web;
        web.kind = ResultKind::Action;
        web.title = L"https://example.com";
        web.target = web.title;
        web.action.kind =
            LauncherActionKind::OpenUrl;
        web.action.payload =
            web.target;

        const auto actions =
            EvaluateLauncherContextActions(
                web,
                true);

        assert(actions.primary);
        assert(!actions.addAsShortcut);
        assert(!actions.editShortcut);
        assert(!actions.deleteShortcut);
        assert(!actions.locateInExplorer);
        assert(actions.copyTarget);
    }

    {
        LauncherResult packaged;
        packaged.kind =
            ResultKind::Application;
        packaged.title = L"storeapp";
        packaged.subtitle = L"Store App";
        packaged.target =
            L"shell:AppsFolder\\Package!App";
        packaged.action.kind =
            LauncherActionKind::ExecuteCommand;
        packaged.action.commandIndex = 7;

        const auto actions =
            EvaluateLauncherContextActions(
                packaged,
                false);

        assert(actions.addAsShortcut);
        assert(!actions.locateInExplorer);
    }

    assert(
        CanRevealTargetInExplorer(
            L"%WINDIR%\\System32\\cmd.exe"));
    assert(
        CanRevealTargetInExplorer(
            L"notepad.exe"));
    assert(
        !CanRevealTargetInExplorer(
            L"https://example.com"));
    assert(
        !CanRevealTargetInExplorer(
            L"steam://run/730"));
    assert(
        !CanRevealTargetInExplorer(
            L"shell:AppsFolder\\Package!App"));

    std::cout
        << "Context action policy tests passed\n";
    return 0;
}
