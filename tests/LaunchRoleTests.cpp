#include "core/Command.hpp"
#include "core/LaunchRole.hpp"

#include <algorithm>
#include <cassert>
#include <iostream>
#include <string>
#include <vector>

using namespace altrun;

namespace {

bool HasToken(
    const std::vector<std::wstring>& tokens,
    std::wstring_view token) {
    return std::find(
               tokens.begin(),
               tokens.end(),
               token) !=
        tokens.end();
}

LaunchEvidence BaseEvidence(
    std::wstring title,
    std::wstring target =
        L"C:/Program Files/Contoso/Studio/App.exe") {

    LaunchEvidence evidence;
    evidence.source =
        LaunchCandidateSource::StartMenu;
    evidence.displayTitle =
        std::move(title);
    evidence.resolvedTarget =
        std::move(target);
    evidence.installRootHint =
        std::filesystem::path(
            evidence.resolvedTarget)
            .parent_path();
    evidence.targetKind =
        LaunchTargetKind::GuiExecutable;
    return evidence;
}

} // namespace

int main() {
    {
        auto evidence =
            BaseEvidence(
                L"Contoso Studio 2025");

        evidence.executable.productName =
            L"Contoso Studio";
        evidence.executable.fileDescription =
            L"Contoso Studio";
        evidence.executable.companyName =
            L"Contoso Software";

        const auto decision =
            ClassifyApplicationRole(
                evidence);

        assert(
            decision.role ==
            ApplicationRole::
                PrimaryApplication);
        assert(
            decision.confidence ==
            RoleConfidence::High);
        assert(
            decision.visibility ==
            CatalogVisibility::Normal);
        assert(
            !decision.catalogGroupKey
                 .empty());
        assert(
            decision.distinctiveTokens
                .empty());
    }

    {
        auto evidence =
            BaseEvidence(
                L"Contoso Studio Encoder",
                L"C:/Program Files/Contoso/Studio/Encoder.exe");

        evidence.executable.productName =
            L"Contoso Studio";
        evidence.executable.fileDescription =
            L"Contoso Studio Encoder";
        evidence.executable.companyName =
            L"Contoso Software";

        const auto decision =
            ClassifyApplicationRole(
                evidence);

        assert(
            decision.role ==
            ApplicationRole::
                CompanionApplication);
        assert(
            decision.visibility ==
            CatalogVisibility::Normal);
        assert(
            HasToken(
                decision.distinctiveTokens,
                L"encoder"));
    }

    {
        auto evidence =
            BaseEvidence(
                L"Contoso Studio Performance Test",
                L"C:/Program Files/Contoso/Studio/Bench.exe");

        evidence.executable.productName =
            L"Contoso Studio";
        evidence.executable.fileDescription =
            L"Contoso Performance Benchmark";
        evidence.executable.originalFilename =
            L"ContosoBenchmark.exe";

        const auto decision =
            ClassifyApplicationRole(
                evidence);

        assert(
            decision.role ==
            ApplicationRole::BenchmarkTool);
        assert(
            decision.confidence ==
            RoleConfidence::High);
        assert(
            decision.visibility ==
            CatalogVisibility::
                StrongMatchOnly);
        assert(
            HasToken(
                decision.distinctiveTokens,
                L"performance"));
        assert(
            HasToken(
                decision.distinctiveTokens,
                L"test"));
    }

    {
        auto evidence =
            BaseEvidence(
                L"Contoso Studio Settings",
                L"C:/Program Files/Contoso/Studio/Config.exe");

        evidence.executable.productName =
            L"Contoso Studio";
        evidence.executable.fileDescription =
            L"Contoso Configuration Utility";

        const auto decision =
            ClassifyApplicationRole(
                evidence);

        assert(
            decision.role ==
            ApplicationRole::
                ConfigurationTool);
        assert(
            decision.confidence ==
            RoleConfidence::High);
        assert(
            decision.visibility ==
            CatalogVisibility::
                StrongMatchOnly);
    }

    {
        auto evidence =
            BaseEvidence(
                L"Contoso Studio Updater",
                L"C:/Program Files/Contoso/Studio/Updater.exe");

        evidence.executable.productName =
            L"Contoso Studio";
        evidence.executable.originalFilename =
            L"ContosoUpdater.exe";

        const auto decision =
            ClassifyApplicationRole(
                evidence);

        assert(
            decision.role ==
            ApplicationRole::Updater);
        assert(
            decision.confidence ==
            RoleConfidence::High);
        assert(
            decision.visibility ==
            CatalogVisibility::Hidden);
    }

    {
        auto evidence =
            BaseEvidence(
                L"Contoso Update Service",
                L"C:/Program Files/Contoso/Studio/UpdateService.exe");

        evidence.executable.productName =
            L"Contoso Studio";
        evidence.executable.fileDescription =
            L"Contoso Update Service";
        evidence.executable.originalFilename =
            L"ContosoUpdateService.exe";

        const auto decision =
            ClassifyApplicationRole(
                evidence);

        assert(
            decision.role ==
            ApplicationRole::
                ServiceComponent);
        assert(
            decision.confidence ==
            RoleConfidence::High);
        assert(
            decision.visibility ==
            CatalogVisibility::Hidden);
    }

    {
        LaunchEvidence evidence;
        evidence.source =
            LaunchCandidateSource::AppsFolder;
        evidence.displayTitle =
            L"Contoso Shell Surface";
        evidence.resolvedTarget =
            L"Contoso.Package_abc!Internal";
        evidence.targetKind =
            LaunchTargetKind::
                ShellApplication;
        evidence.packagedVisibility.system =
            true;
        evidence.packagedVisibility
            .preventPinning = true;

        const auto decision =
            ClassifyApplicationRole(
                evidence);

        assert(
            decision.role ==
            ApplicationRole::
                InternalComponent);
        assert(
            decision.confidence ==
            RoleConfidence::High);
        assert(
            decision.visibility ==
            CatalogVisibility::Hidden);
    }

    {
        LaunchEvidence evidence;
        evidence.source =
            LaunchCandidateSource::Path;
        evidence.displayTitle =
            L"contoso-cli";
        evidence.resolvedTarget =
            L"C:/Tools/contoso-cli.exe";
        evidence.installRootHint =
            L"C:/Tools";
        evidence.targetKind =
            LaunchTargetKind::
                ConsoleExecutable;

        const auto decision =
            ClassifyApplicationRole(
                evidence);

        assert(
            decision.role ==
            ApplicationRole::UserTool);
        assert(
            decision.visibility ==
            CatalogVisibility::Normal);
    }

    {
        auto evidence =
            BaseEvidence(
                L"Mystery Application",
                L"C:/Apps/Mystery.exe");

        evidence.executable.companyName =
            L"Contoso Software";

        const auto decision =
            ClassifyApplicationRole(
                evidence);

        assert(
            decision.role ==
            ApplicationRole::Unknown);
        assert(
            decision.confidence ==
            RoleConfidence::Low);
        assert(
            decision.visibility ==
            CatalogVisibility::Normal);
        // CompanyName alone must never manufacture a suite group.
        assert(
            decision.catalogGroupKey
                .empty());
    }

    {
        auto primary =
            BaseEvidence(
                L"Fabrikam Studio",
                L"C:/Program Files/Fabrikam/Studio/Studio.exe");
        primary.executable.productName =
            L"Fabrikam Studio";

        auto companion =
            BaseEvidence(
                L"Fabrikam Studio Encoder",
                L"C:/Program Files/Fabrikam/Studio/Encoder.exe");
        companion.executable.productName =
            L"Fabrikam Studio";

        assert(
            BuildCatalogGroupKey(
                primary) ==
            BuildCatalogGroupKey(
                companion));
        assert(
            !BuildCatalogGroupKey(
                 primary)
                 .empty());
    }

    {
        auto evidence =
            BaseEvidence(
                L"Acme Performance Test 2026",
                L"C:/Program Files/Acme/App.exe");

        const auto decision =
            ClassifyApplicationRole(
                evidence);

        assert(
            decision.role ==
            ApplicationRole::BenchmarkTool);
        assert(
            decision.confidence ==
            RoleConfidence::Medium);
        assert(
            decision.visibility ==
            CatalogVisibility::
                StrongMatchOnly);
        assert(
            HasToken(
                decision.distinctiveTokens,
                L"performance test"));
    }

    {
        auto evidence =
            BaseEvidence(
                L"Acme 设置向导 2026",
                L"C:/Program Files/Acme/App.exe");

        const auto decision =
            ClassifyApplicationRole(
                evidence);

        assert(
            decision.role ==
            ApplicationRole::
                ConfigurationTool);
        assert(
            decision.confidence ==
            RoleConfidence::Medium);
        assert(
            decision.visibility ==
            CatalogVisibility::
                StrongMatchOnly);
        assert(
            HasToken(
                decision.distinctiveTokens,
                L"设置向导"));
    }

    {
        auto evidence =
            BaseEvidence(
                L"Acme Settings",
                L"C:/Program Files/Acme/App.exe");

        const auto decision =
            ClassifyApplicationRole(
                evidence);

        // A generic single role word remains conservative without catalog
        // corroboration.
        assert(
            decision.role ==
            ApplicationRole::
                ConfigurationTool);
        assert(
            decision.confidence ==
            RoleConfidence::Low);
        assert(
            decision.visibility ==
            CatalogVisibility::Normal);
    }

    {
        const std::wstring group =
            L"product:contosostudio|root:c:\\program files\\contoso\\studio";

        Command primary;
        primary.source =
            CommandSource::StartMenu;
        primary.title =
            L"Contoso Studio";
        primary.target =
            L"C:\\Program Files\\Contoso\\Studio\\Studio.exe";
        primary.canonicalIdentity =
            L"file:c:\\program files\\contoso\\studio\\studio.exe";
        primary.applicationRole =
            ApplicationRole::
                PrimaryApplication;
        primary.roleConfidence =
            RoleConfidence::High;
        primary.catalogVisibility =
            CatalogVisibility::Normal;
        primary.catalogGroupKey =
            group;

        Command settings;
        settings.source =
            CommandSource::StartMenu;
        settings.title =
            L"Contoso Studio Settings";
        settings.target =
            L"C:\\Program Files\\Contoso\\Studio\\Config\\Settings.exe";
        settings.canonicalIdentity =
            L"file:c:\\program files\\contoso\\studio\\config\\settings.exe";
        settings.applicationRole =
            ApplicationRole::
                CompanionApplication;
        settings.roleConfidence =
            RoleConfidence::Low;
        settings.catalogVisibility =
            CatalogVisibility::Normal;
        settings.catalogGroupKey =
            L"product:contosostudio|root:c:\\program files\\contoso\\studio\\config";

        Command benchmark;
        benchmark.source =
            CommandSource::StartMenu;
        benchmark.title =
            L"Contoso Studio Benchmark";
        benchmark.target =
            L"C:\\Program Files\\Contoso\\Studio\\Tools\\Bench.exe";
        benchmark.canonicalIdentity =
            L"file:c:\\program files\\contoso\\studio\\tools\\bench.exe";
        benchmark.applicationRole =
            ApplicationRole::
                BenchmarkTool;
        benchmark.roleConfidence =
            RoleConfidence::Low;
        benchmark.catalogVisibility =
            CatalogVisibility::Normal;
        benchmark.catalogGroupKey =
            L"product:contosostudio|root:c:\\program files\\contoso\\studio\\tools";

        Command diagnostics;
        diagnostics.source =
            CommandSource::StartMenu;
        diagnostics.title =
            L"Contoso Studio Diagnostics";
        diagnostics.target =
            L"C:\\Program Files\\Contoso\\Studio\\Diag.exe";
        diagnostics.canonicalIdentity =
            L"file:c:\\program files\\contoso\\studio\\diag.exe";
        diagnostics.applicationRole =
            ApplicationRole::
                CompanionApplication;
        diagnostics.roleConfidence =
            RoleConfidence::Low;
        diagnostics.catalogVisibility =
            CatalogVisibility::Normal;
        diagnostics.catalogGroupKey =
            group;

        Command downloader;
        downloader.source =
            CommandSource::StartMenu;
        downloader.title =
            L"Contoso Studio Download Manager";
        downloader.target =
            L"C:\\Program Files\\Contoso\\Studio\\Download.exe";
        downloader.canonicalIdentity =
            L"file:c:\\program files\\contoso\\studio\\download.exe";
        downloader.applicationRole =
            ApplicationRole::
                CompanionApplication;
        downloader.roleConfidence =
            RoleConfidence::Low;
        downloader.catalogVisibility =
            CatalogVisibility::Normal;
        downloader.catalogGroupKey =
            group;

        Command editor;
        editor.source =
            CommandSource::StartMenu;
        editor.title =
            L"Contoso Studio Editor";
        editor.target =
            L"C:\\Program Files\\Contoso\\Studio\\Editor.exe";
        editor.canonicalIdentity =
            L"file:c:\\program files\\contoso\\studio\\editor.exe";
        editor.applicationRole =
            ApplicationRole::
                CompanionApplication;
        editor.roleConfidence =
            RoleConfidence::Medium;
        editor.catalogVisibility =
            CatalogVisibility::Normal;
        editor.catalogGroupKey =
            group;

        Command renderer;
        renderer.source =
            CommandSource::StartMenu;
        renderer.title =
            L"Contoso Studio Renderer";
        renderer.target =
            L"C:\\Program Files\\Contoso\\Studio\\Render\\Renderer.exe";
        renderer.canonicalIdentity =
            L"file:c:\\program files\\contoso\\studio\\render\\renderer.exe";
        renderer.applicationRole =
            ApplicationRole::
                CompanionApplication;
        renderer.roleConfidence =
            RoleConfidence::Medium;
        renderer.catalogVisibility =
            CatalogVisibility::Normal;
        renderer.catalogGroupKey =
            L"product:contosostudio|root:c:\\program files\\contoso\\studio\\render";

        Command quickLaunch;
        quickLaunch.source =
            CommandSource::StartMenu;
        quickLaunch.title =
            L"Contoso Studio Quick Launch";
        quickLaunch.target =
            primary.target;
        quickLaunch.arguments =
            L"--quick";
        quickLaunch.canonicalIdentity =
            primary.canonicalIdentity +
            L"|args:--quick";
        quickLaunch.applicationRole =
            ApplicationRole::
                CompanionApplication;
        quickLaunch.roleConfidence =
            RoleConfidence::Low;
        quickLaunch.catalogVisibility =
            CatalogVisibility::Normal;
        quickLaunch.catalogGroupKey =
            group;

        Command safeMode;
        safeMode.source =
            CommandSource::StartMenu;
        safeMode.title =
            L"Contoso Studio Safe Mode";
        safeMode.target =
            L"C:\\Program Files\\Contoso\\Studio\\Safe.exe";
        safeMode.canonicalIdentity =
            L"file:c:\\program files\\contoso\\studio\\safe.exe";
        safeMode.applicationRole =
            ApplicationRole::
                CompanionApplication;
        safeMode.roleConfidence =
            RoleConfidence::Low;
        safeMode.catalogVisibility =
            CatalogVisibility::Normal;
        safeMode.catalogGroupKey =
            group;

        Command userShortcut;
        userShortcut.source =
            CommandSource::User;
        userShortcut.title =
            L"My Contoso Settings";
        userShortcut.applicationRole =
            ApplicationRole::
                ConfigurationTool;
        userShortcut.roleConfidence =
            RoleConfidence::Low;
        userShortcut.catalogVisibility =
            CatalogVisibility::Normal;
        userShortcut.catalogGroupKey =
            group;

        std::vector<Command*> commands{
            &primary,
            &settings,
            &benchmark,
            &diagnostics,
            &downloader,
            &editor,
            &renderer,
            &quickLaunch,
            &safeMode,
            &userShortcut,
        };

        CalibrateCatalogRoleContext(
            commands);

        for (const Command* auxiliary :
             std::vector<const Command*>{
                 &settings,
                 &benchmark,
                 &diagnostics,
                 &downloader}) {
            assert(
                auxiliary->roleConfidence ==
                RoleConfidence::Medium);
            assert(
                auxiliary
                    ->catalogVisibility ==
                CatalogVisibility::
                    StrongMatchOnly);
        }

        assert(
            settings.applicationRole ==
            ApplicationRole::
                ConfigurationTool);
        assert(
            benchmark.applicationRole ==
            ApplicationRole::
                BenchmarkTool);
        assert(
            diagnostics.applicationRole ==
            ApplicationRole::
                DiagnosticTool);
        assert(
            downloader.applicationRole ==
            ApplicationRole::
                Downloader);

        // Independent applications in the same suite remain normal
        // companions; catalog grouping is evidence, never a one-app filter.
        assert(
            editor.applicationRole ==
            ApplicationRole::
                CompanionApplication);
        assert(
            editor.catalogVisibility ==
            CatalogVisibility::Normal);
        assert(
            renderer.applicationRole ==
            ApplicationRole::
                CompanionApplication);
        assert(
            renderer.catalogVisibility ==
            CatalogVisibility::Normal);

        assert(
            quickLaunch.applicationRole ==
            ApplicationRole::
                AlternateLaunch);
        assert(
            quickLaunch.roleConfidence ==
            RoleConfidence::High);
        assert(
            quickLaunch.catalogVisibility ==
            CatalogVisibility::
                StrongMatchOnly);

        assert(
            safeMode.applicationRole ==
            ApplicationRole::
                AlternateLaunch);
        assert(
            safeMode.roleConfidence ==
            RoleConfidence::Medium);
        assert(
            safeMode.catalogVisibility ==
            CatalogVisibility::
                StrongMatchOnly);

        // User-authored entries are explicit intent and are never rewritten
        // by automatic catalog context.
        assert(
            userShortcut.applicationRole ==
            ApplicationRole::
                ConfigurationTool);
        assert(
            userShortcut.roleConfidence ==
            RoleConfidence::Low);
        assert(
            userShortcut.catalogVisibility ==
            CatalogVisibility::Normal);
    }

    {
        assert(
            ParseApplicationRole(
                ApplicationRoleName(
                    ApplicationRole::
                        DiagnosticTool)) ==
            ApplicationRole::
                DiagnosticTool);
        assert(
            ParseApplicationRole(
                ApplicationRoleName(
                    ApplicationRole::
                        AlternateLaunch)) ==
            ApplicationRole::
                AlternateLaunch);
        assert(
            ParseRoleConfidence(
                RoleConfidenceName(
                    RoleConfidence::High)) ==
            RoleConfidence::High);
        assert(
            ParseCatalogVisibility(
                CatalogVisibilityName(
                    CatalogVisibility::
                        StrongMatchOnly)) ==
            CatalogVisibility::
                StrongMatchOnly);
    }

    std::cout
        << "Launch role evidence tests passed\n";
    return 0;
}
