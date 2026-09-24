#pragma once

#include "Command.hpp"
#include "LauncherResult.hpp"

#include <string_view>

namespace altrun {

struct LauncherContextActions {
    bool primary{false};
    bool runAsAdministrator{false};
    bool navigateCurrentFileManager{false};
    bool addAsShortcut{false};
    bool editShortcut{false};
    bool deleteShortcut{false};
    bool locateInExplorer{false};
    bool copyTarget{false};
};

[[nodiscard]] bool
CanRevealTargetInExplorer(
    std::wstring_view target);

[[nodiscard]] LauncherContextActions
EvaluateLauncherContextActions(
    const LauncherResult& result,
    bool currentFileManagerAvailable);

[[nodiscard]] Command
ShortcutSeedFromLauncherResult(
    const LauncherResult& result);

[[nodiscard]] Command
ShortcutSeedFromFileSystemPath(
    std::wstring_view path);

} // namespace altrun
