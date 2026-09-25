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
                L"performance test"));
        assert(
            std::none_of(
                decision
                    .distinctiveTokens
                    .begin(),
                decision
                    .distinctiveTokens
                    .end(),
                [](const std::wstring&
                       token) {
                    return token.starts_with(
                        L"contoso");
                }));
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
                L"Contoso性能测试2025",
                L"C:/Program Files/Contoso/App.exe");

        evidence.executable.productName =
            L"Contoso 2025";

        const auto decision =
            ClassifyApplicationRole(
                evidence);

        assert(
            decision.role ==
            ApplicationRole::
                BenchmarkTool);
        assert(
            decision.visibility ==
            CatalogVisibility::
                StrongMatchOnly);
        assert(
            HasToken(
                decision.distinctiveTokens,
                L"性能测试"));
        assert(
            std::none_of(
                decision
                    .distinctiveTokens
                    .begin(),
                decision
                    .distinctiveTokens
                    .end(),
                [](const std::wstring&
                       token) {
                    return token.starts_with(
                        L"contoso");
                }));
    }

    {
        auto evidence =
            BaseEvidence(
                L"FabrikamStudioPerformanceTest2026",
                L"C:/Program Files/Fabrikam/Studio.exe");

        evidence.executable.productName =
            L"Fabrikam Studio 2026";

        const auto tokens =
            BuildDistinctiveTokens(
                evidence);

        assert(
            std::none_of(
                tokens.begin(),
                tokens.end(),
                [](const std::wstring&
                       token) {
                    return token.starts_with(
                        L"fabrikam");
                }));
        assert(
            HasToken(
                tokens,
                L"performancetest"));
    }

    {
        auto evidence =
            BaseEvidence(
                L"Acme2025快速启动",
                L"C:/Program Files/Acme/App.exe");

        evidence.executable.productName =
            L"Acme";

        const auto tokens =
            BuildDistinctiveTokens(
                evidence);

        assert(
            std::none_of(
                tokens.begin(),
                tokens.end(),
                [](const std::wstring&
                       token) {
                    return token.starts_with(
                        L"acme");
                }));
        assert(
            HasToken(
                tokens,
                L"快速启动"));
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
        // A suite helper can have a completely different EXE ProductName.
        // Start Menu suite context must still isolate the shared family and
        // group all entries without product-specific knowledge.
        const std::filesystem::path menuFolder =
            L"C:/ProgramData/Microsoft/Windows/Start Menu/Programs/Contoso Studio 2026";

        auto primary =
            BaseEvidence(
                L"Contoso Studio 2026",
                L"C:/Program Files/Contoso/Studio/Studio.exe");
        primary.startMenuFolder =
            menuFolder;
        primary.executable.productName =
            L"Contoso Main Application 2026";

        auto benchmark =
            BaseEvidence(
                L"Contoso Studio 性能测试 2026",
                L"C:/Program Files/Contoso/Tools/Bench.exe");
        benchmark.startMenuFolder =
            menuFolder;
        benchmark.executable.productName =
            L"Contoso Benchmark Utility";

        auto settings =
            BaseEvidence(
                L"Contoso Studio 设置向导 2026",
                L"C:/Program Files/Contoso/Setup/Config.exe");
        settings.startMenuFolder =
            menuFolder;
        settings.executable.productName =
            L"Contoso Configuration Manager";

        auto composer =
            BaseEvidence(
                L"Contoso Studio Composer 2026",
                L"C:/Program Files/Contoso/Composer/Composer.exe");
        composer.startMenuFolder =
            menuFolder;
        composer.executable.productName =
            L"Contoso Composer";

        auto quick =
            BaseEvidence(
                L"Contoso Studio 2026 快速启动",
                L"C:/Program Files/Contoso/Studio/Studio.exe");
        quick.startMenuFolder =
            menuFolder;
        quick.executable.productName =
            L"Contoso Launcher";

        const auto primaryDecision =
            ClassifyApplicationRole(
                primary);
        const auto benchmarkDecision =
            ClassifyApplicationRole(
                benchmark);
        const auto settingsDecision =
            ClassifyApplicationRole(
                settings);
        const auto composerDecision =
            ClassifyApplicationRole(
                composer);
        const auto quickDecision =
            ClassifyApplicationRole(
                quick);

        assert(
            primaryDecision.role ==
            ApplicationRole::
                PrimaryApplication);
        assert(
            primaryDecision.confidence !=
            RoleConfidence::Low);

        assert(
            benchmarkDecision.role ==
            ApplicationRole::
                BenchmarkTool);
        assert(
            benchmarkDecision.confidence ==
            RoleConfidence::Medium);
        assert(
            benchmarkDecision.visibility ==
            CatalogVisibility::
                StrongMatchOnly);
        assert(
            benchmarkDecision
                .distinctiveTokens
                .size() == 1);
        assert(
            benchmarkDecision
                .distinctiveTokens[0] ==
            L"性能测试");

        assert(
            settingsDecision.role ==
            ApplicationRole::
                ConfigurationTool);
        assert(
            settingsDecision.confidence ==
            RoleConfidence::Medium);
        assert(
            settingsDecision.visibility ==
            CatalogVisibility::
                StrongMatchOnly);
        assert(
            HasToken(
                settingsDecision
                    .distinctiveTokens,
                L"设置向导"));

        assert(
            composerDecision.role ==
            ApplicationRole::
                CompanionApplication);
        assert(
            composerDecision.visibility ==
            CatalogVisibility::Normal);
        assert(
            HasToken(
                composerDecision
                    .distinctiveTokens,
                L"composer"));

        assert(
            primaryDecision.catalogGroupKey ==
            benchmarkDecision.catalogGroupKey);
        assert(
            primaryDecision.catalogGroupKey ==
            settingsDecision.catalogGroupKey);
        assert(
            primaryDecision.catalogGroupKey ==
            composerDecision.catalogGroupKey);
        assert(
            primaryDecision.catalogGroupKey ==
            quickDecision.catalogGroupKey);
        assert(
            primaryDecision.catalogGroupKey
                .starts_with(
                    L"family:contosostudio|menu:"));

        // AlternateLaunch remains contextual: the phrase alone does not
        // suppress it before a primary sibling is known.
        assert(
            quickDecision.visibility ==
            CatalogVisibility::Normal);

        Command primaryCommand;
        primaryCommand.source =
            CommandSource::StartMenu;
        primaryCommand.title =
            primary.displayTitle;
        primaryCommand.target =
            primary.resolvedTarget;
        primaryCommand.canonicalIdentity =
            L"file:c:\\program files\\contoso\\studio\\studio.exe";
        primaryCommand.applicationRole =
            primaryDecision.role;
        primaryCommand.roleConfidence =
            primaryDecision.confidence;
        primaryCommand.catalogVisibility =
            primaryDecision.visibility;
        primaryCommand.catalogGroupKey =
            primaryDecision.catalogGroupKey;
        primaryCommand.distinctiveTokens =
            primaryDecision.distinctiveTokens;

        Command quickCommand;
        quickCommand.source =
            CommandSource::StartMenu;
        quickCommand.title =
            quick.displayTitle;
        quickCommand.target =
            primaryCommand.target;
        quickCommand.arguments =
            L"--quick";
        quickCommand.canonicalIdentity =
            primaryCommand.canonicalIdentity +
            L"|args:--quick";
        quickCommand.applicationRole =
            quickDecision.role;
        quickCommand.roleConfidence =
            quickDecision.confidence;
        quickCommand.catalogVisibility =
            quickDecision.visibility;
        quickCommand.catalogGroupKey =
            quickDecision.catalogGroupKey;
        quickCommand.distinctiveTokens =
            quickDecision.distinctiveTokens;

        Command composerCommand;
        composerCommand.source =
            CommandSource::StartMenu;
        composerCommand.title =
            composer.displayTitle;
        composerCommand.target =
            composer.resolvedTarget;
        composerCommand.canonicalIdentity =
            L"file:c:\\program files\\contoso\\composer\\composer.exe";
        composerCommand.applicationRole =
            composerDecision.role;
        composerCommand.roleConfidence =
            composerDecision.confidence;
        composerCommand.catalogVisibility =
            composerDecision.visibility;
        composerCommand.catalogGroupKey =
            composerDecision.catalogGroupKey;
        composerCommand.distinctiveTokens =
            composerDecision.distinctiveTokens;

        std::vector<Command*> contextual{
            &primaryCommand,
            &quickCommand,
            &composerCommand,
        };

        CalibrateCatalogRoleContext(
            contextual);

        assert(
            quickCommand.applicationRole ==
            ApplicationRole::
                AlternateLaunch);
        assert(
            quickCommand.catalogVisibility ==
            CatalogVisibility::
                StrongMatchOnly);
        assert(
            HasToken(
                quickCommand
                    .distinctiveTokens,
                L"快速启动"));
        assert(
            std::none_of(
                quickCommand
                    .distinctiveTokens
                    .begin(),
                quickCommand
                    .distinctiveTokens
                    .end(),
                [](const std::wstring&
                       token) {
                    return token.starts_with(
                        L"contoso");
                }));

        assert(
            composerCommand.applicationRole ==
            ApplicationRole::
                CompanionApplication);
        assert(
            composerCommand.catalogVisibility ==
            CatalogVisibility::Normal);
    }

    {
        // High-information title evidence must repair a stale/over-generic
        // Primary role even without any catalog group.
        Command stalePrimary;
        stalePrimary.source =
            CommandSource::StartMenu;
        stalePrimary.title =
            L"Acme Performance Test 2026";
        stalePrimary.applicationRole =
            ApplicationRole::
                PrimaryApplication;
        stalePrimary.roleConfidence =
            RoleConfidence::High;
        stalePrimary.catalogVisibility =
            CatalogVisibility::Normal;
        stalePrimary.distinctiveTokens = {
            L"acmeperformancetest2026",
        };

        std::vector<Command*> commands{
            &stalePrimary,
        };

        CalibrateCatalogRoleContext(
            commands);

        assert(
            stalePrimary.applicationRole ==
            ApplicationRole::
                BenchmarkTool);
        assert(
            stalePrimary.roleConfidence ==
            RoleConfidence::Medium);
        assert(
            stalePrimary.catalogVisibility ==
            CatalogVisibility::
                StrongMatchOnly);
        assert(
            HasToken(
                stalePrimary
                    .distinctiveTokens,
                L"performance test"));
        assert(
            !HasToken(
                stalePrimary
                    .distinctiveTokens,
                L"acmeperformancetest2026"));
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
