#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace altrun {

enum class ResultKind {
    UserCommand,
    Application,
    File,
    Folder,
    Action,
};

enum class LauncherActionKind {
    ExecuteCommand,
    OpenFile,
    OpenFolder,
    OpenUrl,
    NavigateExplorer,
    NavigateFileDialog,
    NavigateTotalCommander,
    CopyText,
};

enum class LauncherExecutionIntent {
    Default,
    NavigateCurrentFileManager,
    // Compatibility alias for the alpha.2 public/internal contract.
    NavigateCurrentExplorer =
        NavigateCurrentFileManager,
    CopySelectedText,
};

struct LauncherAction {
    LauncherActionKind kind{
        LauncherActionKind::ExecuteCommand};
    std::size_t commandIndex{
        static_cast<std::size_t>(-1)};
    // Execution payload is separate from presentation metadata so future
    // smart actions can carry action-specific data without abusing target.
    std::wstring payload;
};

struct LauncherResult {
    std::wstring id;
    std::string providerId;
    ResultKind kind{
        ResultKind::Application};
    std::wstring title;
    std::wstring subtitle;
    std::wstring target;
    // Empty means derive an icon from target. A user command may provide an
    // explicit icon file/executable/shortcut path.
    std::wstring iconSource;
    std::wstring detail;
    int score{0};
    LauncherAction action;
};

[[nodiscard]] bool SameLauncherTarget(
    const LauncherResult& left,
    const LauncherResult& right);

} // namespace altrun
