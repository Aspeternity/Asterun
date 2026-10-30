#pragma once

#include "LaunchCatalog.hpp"
#include "LaunchRole.hpp"
#include "LaunchSurface.hpp"

#include <string>
#include <vector>

namespace altrun {

enum class CommandSource {
    User,
    StartMenu,
    AppPaths,
    Path,
    PackagedApp,
};

enum class CommandType {
    Application,
    Url,
    Folder,
    CommandLine,
};

enum class RuntimeInputMode {
    None,
    Raw,
    UrlEncoded,
};

// App Paths is the shell's run-by-name table, not an application list.
// Discovery records where a run-by-name entry comes from so the merge stage
// can tell a product's own surface from one of its components.
struct RunByNameEvidence {
    // AUMID named by the per-user MSIX execution alias for this entry.
    std::wstring packagedAppId;
    // The package manifest declares this EXE as a COM or service server.
    bool packageServer{false};
    // Serviced by Windows itself (owned by TrustedInstaller).
    bool osComponent{false};
    // Installed under Common Files, the shared-components location.
    bool sharedComponent{false};
    // Product install root (Uninstall InstallLocation, else the EXE folder).
    std::wstring productRoot;
};

struct Command {
    std::wstring id;
    std::wstring keyword;
    std::vector<std::wstring> aliases;
    std::wstring title;
    CommandType type{CommandType::Application};
    std::wstring target;
    std::wstring arguments;
    std::wstring workingDirectory;
    LaunchActivationKind activationKind{
        LaunchActivationKind::
            ShellItem};
    std::wstring canonicalIdentity;
    RuntimeInputMode runtimeInputMode{
        RuntimeInputMode::None};
    bool enabled{true};
    bool runAsAdmin{false};
    bool pinned{false};
    int sortOrder{0};
    std::vector<std::wstring> legacyIds;
    CommandSource source{CommandSource::User};
    LaunchSurfaceClass surfaceClass{
        LaunchSurfaceClass::UserCommand};
    ApplicationRole applicationRole{
        ApplicationRole::Unknown};
    RoleConfidence roleConfidence{
        RoleConfidence::Low};
    CatalogVisibility catalogVisibility{
        CatalogVisibility::Normal};
    std::wstring catalogGroupKey;
    std::vector<std::wstring>
        distinctiveTokens;
    RunByNameEvidence runByName;
    int basePriority{0};
};

} // namespace altrun
