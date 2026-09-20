#include "core/LauncherActionPolicy.hpp"

#include <cassert>
#include <iostream>

using namespace altrun;

int main() {
    LauncherResult folder;
    folder.kind = ResultKind::Folder;
    folder.target = L"D:\\Research\\CKD";
    folder.action.kind = LauncherActionKind::OpenFolder;
    folder.action.payload = folder.target;

    {
        const auto evaluation = EvaluateLauncherAction(
            folder, LauncherExecutionIntent::Default, true, false, false);
        assert(evaluation.available);
        assert(evaluation.reason == ActionUnavailableReason::None);
        assert(evaluation.action.kind == LauncherActionKind::OpenFolder);
    }

    {
        const auto evaluation = EvaluateLauncherAction(
            folder, LauncherExecutionIntent::Default, false, true, false);
        assert(evaluation.available);
        assert(evaluation.action.kind == LauncherActionKind::NavigateFileDialog);
    }

    {
        const auto evaluation = EvaluateLauncherAction(
            folder,
            LauncherExecutionIntent::NavigateCurrentFileManager,
            true, false, false);
        assert(evaluation.available);
        assert(evaluation.action.kind == LauncherActionKind::NavigateExplorer);
    }

    {
        const auto evaluation = EvaluateLauncherAction(
            folder,
            LauncherExecutionIntent::NavigateCurrentFileManager,
            false, false, true);
        assert(evaluation.available);
        assert(evaluation.action.kind == LauncherActionKind::NavigateTotalCommander);
    }

    {
        const auto evaluation = EvaluateLauncherAction(
            folder,
            LauncherExecutionIntent::NavigateCurrentFileManager,
            false, false, false);
        assert(!evaluation.available);
        assert(evaluation.reason == ActionUnavailableReason::NoSupportedFileManager);
        assert(evaluation.action.kind == LauncherActionKind::OpenFolder);
    }

    {
        LauncherResult file;
        file.kind = ResultKind::File;
        file.target = L"D:\\Research\\CKD\\notes.txt";
        file.action.kind = LauncherActionKind::OpenFile;
        file.action.payload = file.target;

        const auto evaluation = EvaluateLauncherAction(
            file,
            LauncherExecutionIntent::NavigateCurrentFileManager,
            true, false, false);
        assert(!evaluation.available);
        assert(evaluation.reason == ActionUnavailableReason::ResultNotFolder);
        assert(evaluation.action.kind == LauncherActionKind::OpenFile);
    }

    {
        const auto evaluation = EvaluateLauncherAction(
            folder,
            LauncherExecutionIntent::CopySelectedText,
            true, false, false);
        assert(evaluation.available);
        assert(evaluation.action.kind == LauncherActionKind::CopyText);
        assert(evaluation.action.commandIndex == static_cast<std::size_t>(-1));
        assert(evaluation.action.payload == folder.target);
    }

    {
        LauncherResult web;
        web.kind = ResultKind::Action;
        web.target = L"https://example.com";
        web.action.kind = LauncherActionKind::OpenUrl;
        web.action.payload = L"https://example.com/?q=test";

        const auto action = ResolveLauncherAction(
            web,
            LauncherExecutionIntent::CopySelectedText,
            false, false, false);
        assert(action.kind == LauncherActionKind::CopyText);
        assert(action.payload == web.action.payload);
    }

    {
        LauncherResult userCommand;
        userCommand.kind = ResultKind::UserCommand;
        userCommand.target = L"powershell.exe";
        userCommand.action.kind = LauncherActionKind::ExecuteCommand;
        userCommand.action.commandIndex = 3;

        const auto action = ResolveLauncherAction(
            userCommand,
            LauncherExecutionIntent::CopySelectedText,
            false, false, false);
        assert(action.kind == LauncherActionKind::CopyText);
        assert(action.payload == userCommand.target);
        assert(action.commandIndex == static_cast<std::size_t>(-1));
    }

    {
        LauncherResult empty;
        empty.action.kind = LauncherActionKind::ExecuteCommand;
        empty.action.commandIndex = 4;

        const auto evaluation = EvaluateLauncherAction(
            empty,
            LauncherExecutionIntent::CopySelectedText,
            false, false, false);
        assert(!evaluation.available);
        assert(evaluation.reason == ActionUnavailableReason::NoCopyableTarget);
        assert(evaluation.action.kind == LauncherActionKind::CopyText);
        assert(evaluation.action.payload.empty());
    }

    {
        LauncherResult invalid;
        invalid.action.kind = LauncherActionKind::OpenFile;

        const auto evaluation = EvaluateLauncherAction(
            invalid,
            LauncherExecutionIntent::Default,
            false, false, false);
        assert(!evaluation.available);
        assert(evaluation.reason == ActionUnavailableReason::InvalidActionTarget);
    }

    std::cout << "Launcher action policy tests passed\n";
    return 0;
}
