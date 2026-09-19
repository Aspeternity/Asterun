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
        assert(
            action.payload ==
            folder.target);
    }

    {
        const auto action =
            ResolveLauncherAction(
                folder,
                LauncherExecutionIntent::
                    NavigateCurrentFileManager,
                false,
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
                true,
                false);

        assert(
            action.kind ==
            LauncherActionKind::
                NavigateFileDialog);
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
                    NavigateCurrentFileManager,
                false,
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
