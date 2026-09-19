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
};

enum class LauncherActionKind {
    ExecuteCommand,
    OpenFile,
    OpenFolder,
};

struct LauncherAction {
    LauncherActionKind kind{
        LauncherActionKind::ExecuteCommand};
    std::size_t commandIndex{
        static_cast<std::size_t>(-1)};
};

struct LauncherResult {
    std::wstring id;
    std::string providerId;
    ResultKind kind{
        ResultKind::Application};
    std::wstring title;
    std::wstring subtitle;
    std::wstring target;
    std::wstring detail;
    int score{0};
    LauncherAction action;
};

[[nodiscard]] bool SameLauncherTarget(
    const LauncherResult& left,
    const LauncherResult& right);

} // namespace altrun
