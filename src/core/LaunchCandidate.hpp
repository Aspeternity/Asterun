#pragma once

#include "LaunchSurface.hpp"

#include <string_view>

namespace altrun {

enum class LaunchCandidateSource {
    StartMenu,
    AppsFolder,
    AppPaths,
    Path,
};

enum class LaunchTargetKind {
    Unknown,
    GuiExecutable,
    ConsoleExecutable,
    CommandScript,
    ShellApplication,
    SystemControl,
    Document,
    WebUri,
};

struct LaunchCandidate {
    LaunchCandidateSource source{
        LaunchCandidateSource::StartMenu};
    std::wstring_view title;
    std::wstring_view target;
    LaunchSurfaceClass surface{
        LaunchSurfaceClass::
            PrimaryApplication};
    LaunchTargetKind targetKind{
        LaunchTargetKind::Unknown};
    bool targetResolved{false};
};

struct LaunchAdmission {
    bool admit{false};
    LaunchSurfaceClass surface{
        LaunchSurfaceClass::
            PrimaryApplication};
};

[[nodiscard]] bool
IsDocumentationLikeTitle(
    std::wstring_view title);

[[nodiscard]] bool
IsMaintenanceLikeTitle(
    std::wstring_view title);

[[nodiscard]] bool
LooksLikeWebTarget(
    std::wstring_view target);

[[nodiscard]] bool
LooksLikeDocumentTarget(
    std::wstring_view target);

[[nodiscard]] LaunchTargetKind
InferTextTargetKind(
    std::wstring_view target);

[[nodiscard]] LaunchAdmission
EvaluateLaunchCandidate(
    const LaunchCandidate& candidate);

} // namespace altrun
