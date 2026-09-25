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
        auto evidence =
            BaseEvidence(
                L"Acme Task Scheduler 2026",
                L"C:/Program Files/Acme/Tools/Tool.exe");

        evidence.executable.productName =
            L"Acme Studio";

        const auto decision =
            ClassifyApplicationRole(
                evidence);

        assert(
            decision.role ==
            ApplicationRole::
                SuiteUtility);
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
                L"task scheduler"));
    }

    {
        auto evidence =
            BaseEvidence(
                L"Acme Sync",
                L"C:/Program Files/Acme/Tools/Sync.exe");

        const auto decision =
            ClassifyApplicationRole(
                evidence);

        // A weak utility word is not enough to suppress a standalone entry.
        assert(
            decision.role ==
            ApplicationRole::
                SuiteUtility);
        assert(
            decision.confidence ==
            RoleConfidence::Low);
        assert(
            decision.visibility ==
            CatalogVisibility::Normal);
        assert(
            HasToken(
                decision.distinctiveTokens,
                L"sync"));

        auto asyncEvidence =
            BaseEvidence(
                L"Acme Async Studio",
                L"C:/Program Files/Acme/Async.exe");

        const auto asyncDecision =
            ClassifyApplicationRole(
                asyncEvidence);

        // Weak "sync" matching is token-aware; "async" must not trigger it.
        assert(
            asyncDecision.role !=
            ApplicationRole::
                SuiteUtility);
    }

    {
        auto evidence =
            BaseEvidence(
                L"Fabrikam X9 2026",
                L"C:/Program Files/Fabrikam/X9.exe");

        evidence.executable.productName =
            L"Fabrikam Suite";
        evidence.executable.fileDescription =
            L"Diagnostic Utility";
        evidence.executable.internalName =
            L"Problem Reporter";

        const auto decision =
            ClassifyApplicationRole(
                evidence);

        assert(
            decision.role ==
            ApplicationRole::
                DiagnosticTool);
        assert(
            decision.confidence ==
            RoleConfidence::High);
        assert(
            decision.visibility ==
            CatalogVisibility::
                StrongMatchOnly);
        // Opaque metadata-driven roles retain only the family-stripped entry
        // name so a user can still explicitly reach the tool.
        assert(
            HasToken(
                decision.distinctiveTokens,
                L"x9"));
        assert(
            !HasToken(
                decision.distinctiveTokens,
                L"fabrikam"));
    }

    {
        // Alternate variants can be related by activation identity or title
        // even when Start Menu grouping differs.
        Command primary;
        primary.source =
            CommandSource::StartMenu;
        primary.title =
            L"Contoso Studio 2026";
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
            L"family:contosostudio|menu:c:\\programdata\\microsoft\\windows\\start menu\\programs\\contoso studio 2026";

        Command crossGroupQuick;
        crossGroupQuick.source =
            CommandSource::StartMenu;
        crossGroupQuick.title =
            L"Contoso Studio 2026 Quick Launch";
        crossGroupQuick.target =
            primary.target;
        crossGroupQuick.arguments =
            L"--quick";
        crossGroupQuick.canonicalIdentity =
            primary.canonicalIdentity +
            L"|args:--quick";
        crossGroupQuick.applicationRole =
            ApplicationRole::
                AlternateLaunch;
        crossGroupQuick.roleConfidence =
            RoleConfidence::Low;
        crossGroupQuick.catalogVisibility =
            CatalogVisibility::Normal;
        crossGroupQuick.catalogGroupKey =
            L"family:contosotools|menu:c:\\programdata\\microsoft\\windows\\start menu\\programs\\contoso tools 2026";
        crossGroupQuick.distinctiveTokens = {
            L"contoso",
            L"quick",
        };

        Command crossGroupSafe;
        crossGroupSafe.source =
            CommandSource::StartMenu;
        crossGroupSafe.title =
            L"Contoso Studio 2026 Safe Mode";
        crossGroupSafe.target =
            L"C:\\Program Files\\Contoso\\Tools\\Safe.exe";
        crossGroupSafe.canonicalIdentity =
            L"file:c:\\program files\\contoso\\tools\\safe.exe";
        crossGroupSafe.applicationRole =
            ApplicationRole::
                AlternateLaunch;
        crossGroupSafe.roleConfidence =
            RoleConfidence::Low;
        crossGroupSafe.catalogVisibility =
            CatalogVisibility::Normal;
        crossGroupSafe.catalogGroupKey =
            L"family:contososafe|menu:c:\\programdata\\microsoft\\windows\\start menu\\programs\\contoso safe";
        crossGroupSafe.distinctiveTokens = {
            L"contoso",
            L"safe",
        };

        Command isolatedQuick;
        isolatedQuick.source =
            CommandSource::StartMenu;
        isolatedQuick.title =
            L"Fabrikam Quick Launch";
        isolatedQuick.target =
            L"C:\\Program Files\\Fabrikam\\Quick.exe";
        isolatedQuick.canonicalIdentity =
            L"file:c:\\program files\\fabrikam\\quick.exe";
        isolatedQuick.applicationRole =
            ApplicationRole::
                AlternateLaunch;
        isolatedQuick.roleConfidence =
            RoleConfidence::Low;
        isolatedQuick.catalogVisibility =
            CatalogVisibility::Normal;
        isolatedQuick.catalogGroupKey =
            L"family:fabrikam|root:c:\\program files\\fabrikam";

        Command userQuick =
            crossGroupQuick;
        userQuick.source =
            CommandSource::User;
        userQuick.title =
            L"My Quick Contoso";
        userQuick.applicationRole =
            ApplicationRole::
                PrimaryApplication;
        userQuick.roleConfidence =
            RoleConfidence::High;
        userQuick.catalogVisibility =
            CatalogVisibility::Normal;

        std::vector<Command*> commands{
            &primary,
            &crossGroupQuick,
            &crossGroupSafe,
            &isolatedQuick,
            &userQuick,
        };

        CalibrateCatalogRoleContext(
            commands);

        assert(
            crossGroupQuick.applicationRole ==
            ApplicationRole::
                AlternateLaunch);
        assert(
            crossGroupQuick.roleConfidence ==
            RoleConfidence::High);
        assert(
            crossGroupQuick.catalogVisibility ==
            CatalogVisibility::
                StrongMatchOnly);
        assert(
            HasToken(
                crossGroupQuick
                    .distinctiveTokens,
                L"quick launch"));

        assert(
            crossGroupSafe.applicationRole ==
            ApplicationRole::
                AlternateLaunch);
        assert(
            crossGroupSafe.roleConfidence ==
            RoleConfidence::Medium);
        assert(
            crossGroupSafe.catalogVisibility ==
            CatalogVisibility::
                StrongMatchOnly);
        assert(
            HasToken(
                crossGroupSafe
                    .distinctiveTokens,
                L"safe mode"));

        // Phrase alone remains insufficient.
        assert(
            isolatedQuick.roleConfidence ==
            RoleConfidence::Low);
        assert(
            isolatedQuick.catalogVisibility ==
            CatalogVisibility::Normal);

        // User shortcuts remain explicit authority.
        assert(
            userQuick.applicationRole ==
            ApplicationRole::
                PrimaryApplication);
        assert(
            userQuick.catalogVisibility ==
            CatalogVisibility::Normal);
    }

    {
        const std::wstring group =
            L"family:contosostudio|root:c:\\program files\\contoso\\studio";

        Command primary;
        primary.source =
            CommandSource::StartMenu;
        primary.title =
            L"Contoso Studio 2026";
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

        Command composer;
        composer.source =
            CommandSource::StartMenu;
        composer.title =
            L"Contoso Studio Composer 2026";
        composer.target =
            L"C:\\Program Files\\Contoso\\Studio\\Composer.exe";
        composer.canonicalIdentity =
            L"file:c:\\program files\\contoso\\studio\\composer.exe";
        composer.applicationRole =
            ApplicationRole::
                CompanionApplication;
        composer.roleConfidence =
            RoleConfidence::Medium;
        composer.catalogVisibility =
            CatalogVisibility::Normal;
        composer.catalogGroupKey =
            group;

        Command composerSync;
        composerSync.source =
            CommandSource::StartMenu;
        composerSync.title =
            L"Contoso Studio Composer Sync 2026";
        composerSync.target =
            L"C:\\Program Files\\Contoso\\Studio\\ComposerSync.exe";
        composerSync.canonicalIdentity =
            L"file:c:\\program files\\contoso\\studio\\composersync.exe";
        composerSync.applicationRole =
            ApplicationRole::
                CompanionApplication;
        composerSync.roleConfidence =
            RoleConfidence::Medium;
        composerSync.catalogVisibility =
            CatalogVisibility::Normal;
        composerSync.catalogGroupKey =
            group;
        composerSync.distinctiveTokens = {
            L"composer",
            L"sync",
        };

        Command taskScheduler;
        taskScheduler.source =
            CommandSource::StartMenu;
        taskScheduler.title =
            L"Contoso Studio Task Scheduler 2026";
        taskScheduler.target =
            L"C:\\Program Files\\Contoso\\Studio\\Scheduler.exe";
        taskScheduler.canonicalIdentity =
            L"file:c:\\program files\\contoso\\studio\\scheduler.exe";
        taskScheduler.applicationRole =
            ApplicationRole::
                PrimaryApplication;
        taskScheduler.roleConfidence =
            RoleConfidence::High;
        taskScheduler.catalogVisibility =
            CatalogVisibility::Normal;
        taskScheduler.catalogGroupKey =
            group;
        taskScheduler.distinctiveTokens = {
            L"contoso",
            L"scheduler",
        };

        std::vector<Command*> commands{
            &primary,
            &composer,
            &composerSync,
            &taskScheduler,
        };

        CalibrateCatalogRoleContext(
            commands);

        assert(
            composer.applicationRole ==
            ApplicationRole::
                CompanionApplication);
        assert(
            composer.catalogVisibility ==
            CatalogVisibility::Normal);

        assert(
            composerSync.applicationRole ==
            ApplicationRole::
                SuiteUtility);
        assert(
            composerSync.roleConfidence ==
            RoleConfidence::Medium);
        assert(
            composerSync.catalogVisibility ==
            CatalogVisibility::
                StrongMatchOnly);
        assert(
            composerSync
                .distinctiveTokens
                .size() == 1);
        assert(
            composerSync
                .distinctiveTokens[0] ==
            L"sync");

        assert(
            taskScheduler.applicationRole ==
            ApplicationRole::
                SuiteUtility);
        assert(
            taskScheduler.roleConfidence ==
            RoleConfidence::Medium);
        assert(
            taskScheduler.catalogVisibility ==
            CatalogVisibility::
                StrongMatchOnly);
        assert(
            HasToken(
                taskScheduler
                    .distinctiveTokens,
                L"task scheduler"));
    }

    {
        auto standaloneNetwork =
            BaseEvidence(
                L"Acme Network Monitor 2026",
                L"C:/Program Files/Acme/Tools/Tool.exe");

        const auto standaloneDecision =
            ClassifyApplicationRole(
                standaloneNetwork);

        // Residual management/monitoring phrases are intentionally weak:
        // a standalone application with this name stays visible.
        assert(
            standaloneDecision.role ==
            ApplicationRole::
                SuiteUtility);
        assert(
            standaloneDecision.confidence ==
            RoleConfidence::Low);
        assert(
            standaloneDecision.visibility ==
            CatalogVisibility::Normal);

        auto serviceManager =
            BaseEvidence(
                L"Acme Service Manager 2026",
                L"C:/Program Files/Acme/Tools/Tool.exe");

        const auto serviceManagerDecision =
            ClassifyApplicationRole(
                serviceManager);

        assert(
            serviceManagerDecision.role ==
            ApplicationRole::
                SuiteUtility);
        assert(
            serviceManagerDecision.confidence ==
            RoleConfidence::Low);
        assert(
            serviceManagerDecision.visibility ==
            CatalogVisibility::Normal);

        auto serviceHost =
            BaseEvidence(
                L"Acme Service Host",
                L"C:/Program Files/Acme/Service.exe");

        const auto serviceHostDecision =
            ClassifyApplicationRole(
                serviceHost);

        // The user-facing service-manager exception must not weaken true
        // service/background component detection.
        assert(
            serviceHostDecision.role ==
            ApplicationRole::
                ServiceComponent);
    }

    {
        const std::wstring group =
            L"family:fabrikamstudio|root:c:\\program files\\fabrikam\\studio";

        Command primary;
        primary.source =
            CommandSource::StartMenu;
        primary.title =
            L"Fabrikam Studio 2026";
        primary.target =
            L"C:\\Program Files\\Fabrikam\\Studio\\Studio.exe";
        primary.canonicalIdentity =
            L"file:c:\\program files\\fabrikam\\studio\\studio.exe";
        primary.applicationRole =
            ApplicationRole::
                PrimaryApplication;
        primary.roleConfidence =
            RoleConfidence::High;
        primary.catalogVisibility =
            CatalogVisibility::Normal;
        primary.catalogGroupKey =
            group;

        Command networkMonitor;
        networkMonitor.source =
            CommandSource::StartMenu;
        networkMonitor.title =
            L"Fabrikam Studio Network Monitor 2026";
        networkMonitor.target =
            L"C:\\Program Files\\Fabrikam\\Studio\\NetworkMonitor.exe";
        networkMonitor.canonicalIdentity =
            L"file:c:\\program files\\fabrikam\\studio\\networkmonitor.exe";
        networkMonitor.applicationRole =
            ApplicationRole::
                BackgroundComponent;
        networkMonitor.roleConfidence =
            RoleConfidence::High;
        networkMonitor.catalogVisibility =
            CatalogVisibility::Hidden;
        networkMonitor.catalogGroupKey =
            group;
        networkMonitor.distinctiveTokens = {
            L"network",
            L"monitor",
        };

        Command licenseManager;
        licenseManager.source =
            CommandSource::StartMenu;
        licenseManager.title =
            L"Fabrikam Studio License Manager 2026";
        licenseManager.target =
            L"C:\\Program Files\\Fabrikam\\Studio\\License.exe";
        licenseManager.canonicalIdentity =
            L"file:c:\\program files\\fabrikam\\studio\\license.exe";
        licenseManager.applicationRole =
            ApplicationRole::
                CompanionApplication;
        licenseManager.roleConfidence =
            RoleConfidence::Medium;
        licenseManager.catalogVisibility =
            CatalogVisibility::Normal;
        licenseManager.catalogGroupKey =
            group;
        licenseManager.distinctiveTokens = {
            L"license",
            L"manager",
        };

        Command serviceManager;
        serviceManager.source =
            CommandSource::StartMenu;
        serviceManager.title =
            L"Fabrikam Studio Service Manager 2026";
        serviceManager.target =
            L"C:\\Program Files\\Fabrikam\\Studio\\Service.exe";
        serviceManager.canonicalIdentity =
            L"file:c:\\program files\\fabrikam\\studio\\service.exe";
        serviceManager.applicationRole =
            ApplicationRole::
                ServiceComponent;
        serviceManager.roleConfidence =
            RoleConfidence::High;
        serviceManager.catalogVisibility =
            CatalogVisibility::Hidden;
        serviceManager.catalogGroupKey =
            group;
        serviceManager.distinctiveTokens = {
            L"service",
            L"manager",
        };

        Command networkDesigner;
        networkDesigner.source =
            CommandSource::StartMenu;
        networkDesigner.title =
            L"Fabrikam Studio Network Designer 2026";
        networkDesigner.target =
            L"C:\\Program Files\\Fabrikam\\Studio\\Designer.exe";
        networkDesigner.canonicalIdentity =
            L"file:c:\\program files\\fabrikam\\studio\\designer.exe";
        networkDesigner.applicationRole =
            ApplicationRole::
                CompanionApplication;
        networkDesigner.roleConfidence =
            RoleConfidence::Medium;
        networkDesigner.catalogVisibility =
            CatalogVisibility::Normal;
        networkDesigner.catalogGroupKey =
            group;
        networkDesigner.distinctiveTokens = {
            L"network",
            L"designer",
        };

        std::vector<Command*> commands{
            &primary,
            &networkMonitor,
            &licenseManager,
            &serviceManager,
            &networkDesigner,
        };

        CalibrateCatalogRoleContext(
            commands);

        for (const Command* utility :
             std::vector<const Command*>{
                 &networkMonitor,
                 &licenseManager,
                 &serviceManager}) {
            assert(
                utility->applicationRole ==
                ApplicationRole::
                    SuiteUtility);
            assert(
                utility->roleConfidence ==
                RoleConfidence::Medium);
            assert(
                utility->catalogVisibility ==
                CatalogVisibility::
                    StrongMatchOnly);
        }

        assert(
            HasToken(
                networkMonitor
                    .distinctiveTokens,
                L"network monitor"));
        assert(
            HasToken(
                networkMonitor
                    .distinctiveTokens,
                L"monitor"));
        assert(
            HasToken(
                licenseManager
                    .distinctiveTokens,
                L"license manager"));
        assert(
            HasToken(
                serviceManager
                    .distinctiveTokens,
                L"service manager"));

        // Nearby words are not enough; an independent network-oriented
        // companion remains a normal application.
        assert(
            networkDesigner.applicationRole ==
            ApplicationRole::
                CompanionApplication);
        assert(
            networkDesigner.catalogVisibility ==
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
            ParseApplicationRole(
                ApplicationRoleName(
                    ApplicationRole::
                        SuiteUtility)) ==
            ApplicationRole::
                SuiteUtility);
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
