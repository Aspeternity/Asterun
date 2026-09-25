#pragma once

#include "LaunchCandidate.hpp"

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace altrun {

enum class ApplicationRole {
    Unknown,
    PrimaryApplication,
    CompanionApplication,
    AlternateLaunch,
    UserTool,
    ConfigurationTool,
    DiagnosticTool,
    BenchmarkTool,
    Installer,
    Uninstaller,
    Updater,
    Downloader,
    RepairTool,
    Documentation,
    ProductInfo,
    BackgroundComponent,
    ServiceComponent,
    InternalComponent,
};

enum class RoleConfidence {
    Low,
    Medium,
    High,
};

enum class CatalogVisibility {
    Normal,
    StrongMatchOnly,
    Hidden,
};

struct ExecutableMetadata {
    std::wstring fileDescription;
    std::wstring productName;
    std::wstring companyName;
    std::wstring originalFilename;
    std::wstring internalName;

    [[nodiscard]] bool Empty() const noexcept {
        return fileDescription.empty() &&
            productName.empty() &&
            companyName.empty() &&
            originalFilename.empty() &&
            internalName.empty();
    }
};

struct LaunchEvidence {
    LaunchCandidateSource source{
        LaunchCandidateSource::StartMenu};
    std::wstring displayTitle;
    std::wstring resolvedTarget;
    std::wstring arguments;
    std::filesystem::path shortcutPath;
    std::filesystem::path startMenuFolder;
    std::filesystem::path installRootHint;
    LaunchTargetKind targetKind{
        LaunchTargetKind::Unknown};
    ExecutableMetadata executable;
    PackagedVisibilityEvidence
        packagedVisibility;
};

struct ApplicationRoleDecision {
    ApplicationRole role{
        ApplicationRole::Unknown};
    RoleConfidence confidence{
        RoleConfidence::Low};
    CatalogVisibility visibility{
        CatalogVisibility::Normal};
    std::wstring catalogGroupKey;
    std::vector<std::wstring>
        distinctiveTokens;
};

[[nodiscard]] const char*
ApplicationRoleName(
    ApplicationRole role) noexcept;

[[nodiscard]] ApplicationRole
ParseApplicationRole(
    std::string_view value,
    ApplicationRole fallback =
        ApplicationRole::Unknown) noexcept;

[[nodiscard]] const char*
RoleConfidenceName(
    RoleConfidence confidence) noexcept;

[[nodiscard]] RoleConfidence
ParseRoleConfidence(
    std::string_view value,
    RoleConfidence fallback =
        RoleConfidence::Low) noexcept;

[[nodiscard]] const char*
CatalogVisibilityName(
    CatalogVisibility visibility) noexcept;

[[nodiscard]] CatalogVisibility
ParseCatalogVisibility(
    std::string_view value,
    CatalogVisibility fallback =
        CatalogVisibility::Normal) noexcept;

[[nodiscard]] CatalogVisibility
CatalogVisibilityForRole(
    ApplicationRole role,
    RoleConfidence confidence) noexcept;

[[nodiscard]] std::wstring
BuildCatalogGroupKey(
    const LaunchEvidence& evidence);

[[nodiscard]]
std::vector<std::wstring>
BuildDistinctiveTokens(
    const LaunchEvidence& evidence);

[[nodiscard]] ApplicationRoleDecision
ClassifyApplicationRole(
    const LaunchEvidence& evidence);

struct Command;

// Applies cross-entry catalog context after provider discovery/cache loading.
// This never performs I/O and is intended for catalog publication, not query.
void CalibrateCatalogRoleContext(
    std::vector<Command*>& commands);

} // namespace altrun
