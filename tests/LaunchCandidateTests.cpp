#include "core/LaunchCandidate.hpp"
#include "core/LaunchSurface.hpp"

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
                L"关于 Java",
                L"C:\\Program Files\\Java\\javacpl.exe",
                LaunchSurfaceClass::
                    PrimaryApplication,
                LaunchTargetKind::
                    GuiExecutable,
                true,
            });

        assert(!decision.admit);
        assert(
            decision.reason ==
            LaunchAdmissionReason::
                ProductInfo);
    }

    {
        PackagedVisibilityEvidence
            visibility;
        visibility.preventPinning = true;
        visibility.system = true;

        LaunchCandidate candidate{
            LaunchCandidateSource::
                AppsFolder,
            L"Internal Action",
            L"Vendor.Package_abc!InternalAction",
            LaunchSurfaceClass::
                PrimaryApplication,
            LaunchTargetKind::
                ShellApplication,
            true,
        };
        candidate.packagedVisibility =
            visibility;

        const auto decision =
            EvaluateLaunchCandidate(
                candidate);

        assert(!decision.admit);
        assert(
            decision.reason ==
            LaunchAdmissionReason::
                InternalComponent);
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
    // Repair tools are admitted on the restrictive Maintenance surface and
    // left to role classification; uninstallers and updaters stay rejected.
    {
        const auto repair =
            EvaluateLaunchCandidate({
                LaunchCandidateSource::StartMenu,
                L"DirectX 修复工具",
                L"C:\\Tools\\DirectX Repair\\DirectX Repair.exe",
                ClassifyApplicationSurface(
                    L"DirectX 修复工具",
                    L"C:\\Tools\\DirectX Repair\\DirectX Repair.exe"),
                LaunchTargetKind::GuiExecutable,
                true,
            });
        assert(repair.admit);
        assert(repair.surface ==
               LaunchSurfaceClass::Maintenance);

        const auto uninstall =
            EvaluateLaunchCandidate({
                LaunchCandidateSource::StartMenu,
                L"Uninstall DirectX Repair",
                L"C:\\Tools\\DirectX Repair\\unins000.exe",
                LaunchSurfaceClass::PrimaryApplication,
                LaunchTargetKind::GuiExecutable,
                true,
            });
        assert(!uninstall.admit);
        assert(uninstall.reason ==
               LaunchAdmissionReason::Maintenance);
    }
    // System tools get the program name they are commonly launched by.
    assert(SystemToolAlias(L"System Configuration",
                           L"C:\\WINDOWS\\system32\\msconfig.exe",
                           L"") == L"msconfig");
    assert(SystemToolAlias(L"Task Scheduler",
                           L"C:\\WINDOWS\\system32\\taskschd.msc",
                           L"/s") == L"taskschd");
    assert(SystemToolAlias(L"Registry Editor",
                           L"C:\\WINDOWS\\regedit.exe",
                           L"") == L"regedit");
    // Hosts take the tool from their arguments.
    assert(SystemToolAlias(L"Event Viewer",
                           L"C:\\WINDOWS\\system32\\mmc.exe",
                           L"\"C:\\WINDOWS\\system32\\eventvwr.msc\" /s") ==
           L"eventvwr");
    assert(SystemToolAlias(L"Programs and Features",
                           L"C:\\WINDOWS\\system32\\control.exe",
                           L"appwiz.cpl") == L"appwiz");
    // Nothing new, too short, or not a program.
    assert(SystemToolAlias(L"services",
                           L"C:\\WINDOWS\\system32\\services.msc",
                           L"").empty());
    assert(SystemToolAlias(L"Windows Defender Firewall with Advanced Security",
                           L"C:\\WINDOWS\\system32\\WF.msc",
                           L"").empty());
    assert(SystemToolAlias(L"Control Panel",
                           L"C:\\WINDOWS\\system32\\control.exe",
                           L"").empty());
    assert(SystemToolAlias(L"Notes",
                           L"C:\\Docs\\notes.txt",
                           L"").empty());

    assert(IsRepairLikeTitle(L"Stellar Repair for Photo"));
    assert(!IsMaintenanceLikeTitle(L"DirectX 修复工具"));

    assert(IsMaintenanceLikeTitle(
        L"Uninstall Product"));
    assert(IsMaintenanceLikeTitle(
        L"产品更新程序"));
    assert(IsProductInfoLikeTitle(
        L"About Java"));
    assert(IsProductInfoLikeTitle(
        L"关于 Java"));
    assert(!IsProductInfoLikeTitle(
        L"AboutTime"));

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
