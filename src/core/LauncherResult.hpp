#pragma once

#include "RelevancePolicy.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

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
    RunAsAdministrator,
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
    // Presentation-only text for user-configured shortcut words. Search,
    // ranking and execution continue to use Command keyword/aliases directly.
    std::wstring shortcutHint;
    std::wstring target;
    std::wstring detail;
    int score{0};
    relevance::Match relevanceMatch;
    LaunchSurfaceClass surfaceClass{
        LaunchSurfaceClass::Action};
    int usageScore{0};
    bool pinned{false};
    LauncherAction action;
};

[[nodiscard]] std::wstring
FormatUserShortcutHint(
    std::wstring_view primary,
    std::span<const std::wstring> aliases);

[[nodiscard]] bool SameLauncherTarget(
    const LauncherResult& left,
    const LauncherResult& right);

} // namespace altrun
