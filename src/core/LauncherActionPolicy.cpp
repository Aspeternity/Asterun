#include "LauncherActionPolicy.hpp"

namespace altrun {

ActionEvaluation
EvaluateLauncherAction(
    const LauncherResult& result,
    LauncherExecutionIntent intent,
    bool explorerContextAvailable,
    bool fileDialogContextAvailable,
    bool totalCommanderContextAvailable) {
    ActionEvaluation evaluation;
    evaluation.action = result.action;

    if (intent == LauncherExecutionIntent::CopySelectedText) {
        evaluation.action.kind = LauncherActionKind::CopyText;
        evaluation.action.commandIndex = static_cast<std::size_t>(-1);

        if (!result.action.payload.empty() &&
            result.action.kind != LauncherActionKind::ExecuteCommand) {
            evaluation.action.payload = result.action.payload;
        } else if (!result.target.empty()) {
            evaluation.action.payload = result.target;
        } else {
            evaluation.action.payload.clear();
        }

        if (evaluation.action.payload.empty()) {
            evaluation.available = false;
            evaluation.reason = ActionUnavailableReason::NoCopyableTarget;
        }
        return evaluation;
    }

    if (intent == LauncherExecutionIntent::Default &&
        fileDialogContextAvailable &&
        result.kind == ResultKind::Folder) {
        evaluation.action.kind = LauncherActionKind::NavigateFileDialog;
        evaluation.action.payload =
            result.target.empty() ? result.action.payload : result.target;
        if (evaluation.action.payload.empty()) {
            evaluation.available = false;
            evaluation.reason = ActionUnavailableReason::InvalidActionTarget;
        }
        return evaluation;
    }

    if (intent == LauncherExecutionIntent::NavigateCurrentFileManager) {
        if (result.kind != ResultKind::Folder) {
            evaluation.available = false;
            evaluation.reason = ActionUnavailableReason::ResultNotFolder;
            return evaluation;
        }

        if (totalCommanderContextAvailable) {
            evaluation.action.kind = LauncherActionKind::NavigateTotalCommander;
            evaluation.action.payload =
                result.target.empty() ? result.action.payload : result.target;
            if (evaluation.action.payload.empty()) {
                evaluation.available = false;
                evaluation.reason = ActionUnavailableReason::InvalidActionTarget;
            }
            return evaluation;
        }

        if (explorerContextAvailable) {
            evaluation.action.kind = LauncherActionKind::NavigateExplorer;
            evaluation.action.payload =
                result.target.empty() ? result.action.payload : result.target;
            if (evaluation.action.payload.empty()) {
                evaluation.available = false;
                evaluation.reason = ActionUnavailableReason::InvalidActionTarget;
            }
            return evaluation;
        }

        evaluation.available = false;
        evaluation.reason = ActionUnavailableReason::NoSupportedFileManager;
        return evaluation;
    }

    if (evaluation.action.kind == LauncherActionKind::ExecuteCommand) {
        if (evaluation.action.commandIndex == static_cast<std::size_t>(-1)) {
            evaluation.available = false;
            evaluation.reason = ActionUnavailableReason::InvalidActionTarget;
        }
        return evaluation;
    }

    if (evaluation.action.payload.empty() && result.target.empty()) {
        evaluation.available = false;
        evaluation.reason = ActionUnavailableReason::InvalidActionTarget;
    }

    return evaluation;
}

LauncherAction
ResolveLauncherAction(
    const LauncherResult& result,
    LauncherExecutionIntent intent,
    bool explorerContextAvailable,
    bool fileDialogContextAvailable,
    bool totalCommanderContextAvailable) {
    return EvaluateLauncherAction(
               result,
               intent,
               explorerContextAvailable,
               fileDialogContextAvailable,
               totalCommanderContextAvailable)
        .action;
}

} // namespace altrun
