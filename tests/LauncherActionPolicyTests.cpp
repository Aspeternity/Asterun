#include "core/LauncherActionPolicy.hpp"

#include <cassert>
#include <iostream>

using namespace altrun;

int main() {
    LauncherResult folder;
    folder.kind =
        ResultKind::Folder;
    folder.target =
        L"D:\\Research\\CKD";
    folder.action.kind =
        LauncherActionKind::
            OpenFolder;
    folder.action.payload =
        folder.target;

    {
        const auto action =
            ResolveLauncherAction(
                folder,
                LauncherExecutionIntent::
                    Default,
                true,
                false,
                false);

        assert(
            action.kind ==
            LauncherActionKind::
                OpenFolder);
    }

    {
        const auto action =
            ResolveLauncherAction(
                folder,
                LauncherExecutionIntent::
                    NavigateCurrentFileManager,
                true,
                false,
                false);

        assert(
            action.kind ==
            LauncherActionKind::
                NavigateExplorer);
    }

    {
        const auto action =
            ResolveLauncherAction(
                folder,
                LauncherExecutionIntent::
                    NavigateCurrentFileManager,
                false,
                false,
                true);

        assert(
            action.kind ==
            LauncherActionKind::
                NavigateTotalCommander);
    }

    {
        const auto action =
            ResolveLauncherAction(
                folder,
                LauncherExecutionIntent::
                    CopySelectedText,
                true,
                false,
                false);

        assert(
            action.kind ==
            LauncherActionKind::
                CopyText);
        assert(
            action.commandIndex ==
            static_cast<std::size_t>(
                -1));
        assert(
            action.payload ==
            folder.target);
    }

    {
        LauncherResult web;
        web.kind =
            ResultKind::Action;
        web.target =
            L"https://example.com";
        web.action.kind =
            LauncherActionKind::
                OpenUrl;
        web.action.payload =
            L"https://example.com/?q=test";

        const auto action =
            ResolveLauncherAction(
                web,
                LauncherExecutionIntent::
                    CopySelectedText,
                false,
                false,
                false);

        assert(
            action.kind ==
            LauncherActionKind::
                CopyText);
        assert(
            action.payload ==
            web.action.payload);
    }

    {
        LauncherResult userCommand;
        userCommand.kind =
            ResultKind::UserCommand;
        userCommand.target =
            L"powershell.exe";
        userCommand.action.kind =
            LauncherActionKind::
                ExecuteCommand;
        userCommand.action.commandIndex =
            3;

        const auto action =
            ResolveLauncherAction(
                userCommand,
                LauncherExecutionIntent::
                    CopySelectedText,
                false,
                false,
                false);

        assert(
            action.kind ==
            LauncherActionKind::
                CopyText);
        assert(
            action.payload ==
            userCommand.target);
        assert(
            action.commandIndex ==
            static_cast<std::size_t>(
                -1));
    }

    {
        LauncherResult empty;
        empty.action.kind =
            LauncherActionKind::
                ExecuteCommand;
        empty.action.commandIndex =
            4;

        const auto action =
            ResolveLauncherAction(
                empty,
                LauncherExecutionIntent::
                    CopySelectedText,
                false,
                false,
                false);

        assert(
            action.kind ==
            LauncherActionKind::
                CopyText);
        assert(action.payload.empty());
    }

    std::cout
        << "Launcher action policy tests passed\n";
    return 0;
}
