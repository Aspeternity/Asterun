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
                    NavigateCurrentExplorer,
                true,
                false);

        assert(
            action.kind ==
            LauncherActionKind::
                NavigateExplorer);
        assert(
            action.payload ==
            folder.target);
    }

    {
        const auto action =
            ResolveLauncherAction(
                folder,
                LauncherExecutionIntent::
                    NavigateCurrentExplorer,
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
                    Default,
                false,
                true);

        assert(
            action.kind ==
            LauncherActionKind::
                NavigateFileDialog);
        assert(
            action.payload ==
            folder.target);
    }

    {
        // Ctrl+Enter is intentionally not consumed by the file-dialog action:
        // the physical Ctrl key is still down while the synchronous action
        // executes. Normal Enter is the dialog-jump gesture.
        const auto action =
            ResolveLauncherAction(
                folder,
                LauncherExecutionIntent::
                    NavigateCurrentExplorer,
                false,
                true);

        assert(
            action.kind ==
            LauncherActionKind::
                OpenFolder);
    }

    {
        LauncherResult file;
        file.kind =
            ResultKind::File;
        file.target =
            L"D:\\Research\\paper.pdf";
        file.action.kind =
            LauncherActionKind::
                OpenFile;
        file.action.payload =
            file.target;

        const auto action =
            ResolveLauncherAction(
                file,
                LauncherExecutionIntent::
                    Default,
                false,
                true);

        assert(
            action.kind ==
            LauncherActionKind::
                OpenFile);
    }

    std::cout
        << "Launcher action policy tests passed\n";
    return 0;
}
