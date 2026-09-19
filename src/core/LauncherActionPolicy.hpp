#pragma once

#include "LauncherResult.hpp"

namespace altrun {

[[nodiscard]] LauncherAction
ResolveLauncherAction(
    const LauncherResult& result,
    LauncherExecutionIntent intent,
    bool explorerContextAvailable,
    bool fileDialogContextAvailable = false,
    bool totalCommanderContextAvailable = false);

} // namespace altrun
