#pragma once

#include "LauncherResult.hpp"

namespace altrun {

enum class ActionUnavailableReason {
    None,
    ResultNotFolder,
    NoSupportedFileManager,
    NoCopyableTarget,
    InvalidActionTarget,
};

struct ActionEvaluation {
    LauncherAction action;
    bool available{true};
    ActionUnavailableReason reason{
        ActionUnavailableReason::None};
};

[[nodiscard]] ActionEvaluation
EvaluateLauncherAction(
    const LauncherResult& result,
    LauncherExecutionIntent intent,
    bool explorerContextAvailable,
    bool fileDialogContextAvailable = false,
    bool totalCommanderContextAvailable = false);

[[nodiscard]] LauncherAction
ResolveLauncherAction(
    const LauncherResult& result,
    LauncherExecutionIntent intent,
    bool explorerContextAvailable,
    bool fileDialogContextAvailable = false,
    bool totalCommanderContextAvailable = false);

} // namespace altrun
