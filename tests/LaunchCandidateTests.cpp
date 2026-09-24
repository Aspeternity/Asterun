#include "core/LaunchCandidate.hpp"

#include <cassert>
#include <iostream>

using namespace altrun;

int main() {
    {
        const auto decision =
            EvaluateLaunchCandidate({
                LaunchCandidateSource::
                    AppsFolder,
                L"访问 Java.com",
                L"https://java.com/",
                LaunchSurfaceClass::
                    PrimaryApplication,
                LaunchTargetKind::WebUri,
                true,
            });

        assert(!decision.admit);
        assert(
            decision.reason ==
            LaunchAdmissionReason::
                WebTarget);
    }

    {
        const auto decision =
            EvaluateLaunchCandidate({
                LaunchCandidateSource::
                    StartMenu,
                L"最新版本里有哪些新功能",
                L"C:\\Program Files\\WinRAR\\WinRAR.chm",
                LaunchSurfaceClass::
                    PrimaryApplication,
                LaunchTargetKind::Document,
                true,
            });

        assert(!decision.admit);
        assert(
            decision.reason ==
            LaunchAdmissionReason::
                Documentation);
    }

    {
        const auto decision =
            EvaluateLaunchCandidate({
                LaunchCandidateSource::
                    StartMenu,
                L"WinRAR",
                L"C:\\Program Files\\WinRAR\\WinRAR.exe",
                LaunchSurfaceClass::
                    PrimaryApplication,
                LaunchTargetKind::
                    GuiExecutable,
                true,
            });

        assert(decision.admit);
        assert(
            decision.surface ==
            LaunchSurfaceClass::
                PrimaryApplication);
    }


    {
        const auto decision =
            EvaluateLaunchCandidate({
                LaunchCandidateSource::
                    StartMenu,
                L"TeamSpeak",
                L"C:\\Users\\Asp\\AppData\\Local\\Programs\\TeamSpeak\\TeamSpeak.exe",
                LaunchSurfaceClass::
                    PrimaryApplication,
                LaunchTargetKind::
                    ExecutableUnknown,
                true,
            });

        assert(decision.admit);
        assert(
            decision.surface ==
            LaunchSurfaceClass::
                PrimaryApplication);
        assert(
            decision.reason ==
            LaunchAdmissionReason::
                Admitted);
    }

    {
        const auto decision =
            EvaluateLaunchCandidate({
                LaunchCandidateSource::
                    AppPaths,
                L"Product Updater",
                L"C:\\Apps\\Updater.exe",
                LaunchSurfaceClass::
                    Maintenance,
                LaunchTargetKind::
                    GuiExecutable,
                true,
            });

        assert(!decision.admit);
    }

    {
        const auto decision =
            EvaluateLaunchCandidate({
                LaunchCandidateSource::
                    AppPaths,
                L"BrowserNativeMessagingHost",
                L"C:\\Apps\\BrowserNativeMessagingHost.exe",
                LaunchSurfaceClass::
                    Auxiliary,
                LaunchTargetKind::
                    ConsoleExecutable,
                true,
            });

        assert(!decision.admit);
    }

    {
        const auto decision =
            EvaluateLaunchCandidate({
                LaunchCandidateSource::
                    AppPaths,
                L"Remove Product",
                L"C:\\Apps\\unins000.exe",
                LaunchSurfaceClass::
                    PrimaryApplication,
                LaunchTargetKind::
                    GuiExecutable,
                true,
            });

        assert(!decision.admit);
    }

    {
        const auto decision =
            EvaluateLaunchCandidate({
                LaunchCandidateSource::
                    AppPaths,
                L"Browser Integration",
                L"C:\\Apps\\BrowserNativeMessagingHost.exe",
                LaunchSurfaceClass::
                    PrimaryApplication,
                LaunchTargetKind::
                    ConsoleExecutable,
                true,
            });

        assert(!decision.admit);
    }

    {
        const auto decision =
            EvaluateLaunchCandidate({
                LaunchCandidateSource::
                    StartMenu,
                L"Component Services",
                L"C:\\Windows\\System32\\comexp.msc",
                LaunchSurfaceClass::
                    SystemUtility,
                LaunchTargetKind::
                    SystemControl,
                true,
            });

        assert(decision.admit);
        assert(
            decision.surface ==
            LaunchSurfaceClass::
                SystemUtility);
    }

    {
        const auto decision =
            EvaluateLaunchCandidate({
                LaunchCandidateSource::
                    AppPaths,
                L"git",
                L"C:\\Tools\\git.exe",
                LaunchSurfaceClass::
                    PrimaryApplication,
                LaunchTargetKind::
                    ConsoleExecutable,
                true,
            });

        assert(decision.admit);
        assert(
            decision.surface ==
            LaunchSurfaceClass::
                CommandLineTool);
    }

    {
        const auto decision =
            EvaluateLaunchCandidate({
                LaunchCandidateSource::
                    Path,
                L"ffmpeg",
                L"C:\\Tools\\ffmpeg.exe",
                LaunchSurfaceClass::
                    PrimaryApplication,
                LaunchTargetKind::
                    ConsoleExecutable,
                true,
            });

        assert(decision.admit);
        assert(
            decision.surface ==
            LaunchSurfaceClass::
                CommandLineTool);
    }

    assert(IsDocumentationLikeTitle(
        L"What's New"));
    assert(IsDocumentationLikeTitle(
        L"最新版本里有哪些新功能"));
    assert(IsDocumentationLikeTitle(
        L"控制台 RAR 中文手册"));
    assert(IsMaintenanceLikeTitle(
        L"Uninstall Product"));
    assert(IsMaintenanceLikeTitle(
        L"产品更新程序"));

    assert(
        InferTextTargetKind(
            L"https://java.com/") ==
        LaunchTargetKind::WebUri);
    assert(
        InferTextTargetKind(
            L"C:\\Docs\\manual.chm") ==
        LaunchTargetKind::Document);
    assert(
        InferTextTargetKind(
            L"Microsoft.WindowsCalculator_8wekyb3d8bbwe!App") ==
        LaunchTargetKind::ShellApplication);
    assert(
        InferTextTargetKind(
            L"ms-settings:display") ==
        LaunchTargetKind::ShellApplication);

    std::cout
        << "Launch candidate admission tests passed\n";
    return 0;
}
