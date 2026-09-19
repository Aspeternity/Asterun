#include "LauncherActionPolicy.hpp"

namespace altrun {

LauncherAction
ResolveLauncherAction(
    const LauncherResult& result,
    LauncherExecutionIntent intent,
    bool explorerContextAvailable,
    bool fileDialogContextAvailable) {
    LauncherAction action =
        result.action;

    // A file dialog is itself the current navigation surface. Normal
    // execution (Enter, double-click, numeric quick launch, opt-in
    // single-result execution) should move that dialog rather than opening a
    // separate Explorer window. Ctrl+Enter deliberately remains the explicit
    // Explorer-navigation intent and is not consumed here while Ctrl is still
    // physically held.
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
                NavigateCurrentExplorer &&
        explorerContextAvailable &&
        result.kind ==
            ResultKind::Folder) {
        action.kind =
            LauncherActionKind::
                NavigateExplorer;
        action.payload =
            result.target.empty()
                ? result.action.payload
                : result.target;
    }

    return action;
}

} // namespace altrun
