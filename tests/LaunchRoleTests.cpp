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
        L"C:\\Program Files\\Contoso\\Studio\\App.exe") {

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
                L"C:\\Program Files\\Contoso\\Studio\\Encoder.exe");

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
                L"C:\\Program Files\\Contoso\\Studio\\Bench.exe");

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
                L"C:\\Program Files\\Contoso\\Studio\\Config.exe");

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
                L"C:\\Program Files\\Contoso\\Studio\\Updater.exe");

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
                L"C:\\Program Files\\Contoso\\Studio\\UpdateService.exe");

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
            L"C:\\Tools\\contoso-cli.exe";
        evidence.installRootHint =
            L"C:\\Tools";
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
                L"C:\\Apps\\Mystery.exe");

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
                L"C:\\Program Files\\Fabrikam\\Studio\\Studio.exe");
        primary.executable.productName =
            L"Fabrikam Studio";

        auto companion =
            BaseEvidence(
                L"Fabrikam Studio Encoder",
                L"C:\\Program Files\\Fabrikam\\Studio\\Encoder.exe");
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
        assert(
            ParseApplicationRole(
                ApplicationRoleName(
                    ApplicationRole::
                        DiagnosticTool)) ==
            ApplicationRole::
                DiagnosticTool);
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
