#include "LauncherActionPolicy.hpp"

namespace altrun {

LauncherAction
ResolveLauncherAction(
    const LauncherResult& result,
    LauncherExecutionIntent intent,
    bool explorerContextAvailable) {
    LauncherAction action =
        result.action;

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
