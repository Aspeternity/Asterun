#include "LauncherActionPolicy.hpp"

namespace altrun {

LauncherAction
ResolveLauncherAction(
    const LauncherResult& result,
    LauncherExecutionIntent intent,
    bool explorerContextAvailable,
    bool fileDialogContextAvailable,
    bool totalCommanderContextAvailable) {
    LauncherAction action =
        result.action;

    if (intent ==
        LauncherExecutionIntent::
            CopySelectedText) {
        action.kind =
            LauncherActionKind::
                CopyText;
        action.commandIndex =
            static_cast<std::size_t>(-1);

        if (!result.action.payload.empty() &&
            result.action.kind !=
                LauncherActionKind::
                    ExecuteCommand) {
            action.payload =
                result.action.payload;
        } else if (!result.target.empty()) {
            action.payload =
                result.target;
        } else {
            action.payload.clear();
        }

        return action;
    }

    // A file dialog is itself the current navigation surface. Normal
    // execution moves the dialog instead of opening a separate Explorer.
    if (intent ==
            LauncherExecutionIntent::Default &&
        fileDialogContextAvailable &&
        result.kind ==
            ResultKind::Folder) {
        action.kind =
            LauncherActionKind::
                NavigateFileDialog;
        action.payload =
            result.target.empty()
                ? result.action.payload
                : result.target;
        return action;
    }

    if (intent ==
            LauncherExecutionIntent::
                NavigateCurrentFileManager &&
        result.kind ==
            ResultKind::Folder) {
        if (totalCommanderContextAvailable) {
            action.kind =
                LauncherActionKind::
                    NavigateTotalCommander;
            action.payload =
                result.target.empty()
                    ? result.action.payload
                    : result.target;
            return action;
        }

        if (explorerContextAvailable) {
            action.kind =
                LauncherActionKind::
                    NavigateExplorer;
            action.payload =
                result.target.empty()
                    ? result.action.payload
                    : result.target;
        }
    }

    return action;
}

} // namespace altrun
