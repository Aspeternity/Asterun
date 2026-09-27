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
    int basePriority{0};
};

} // namespace altrun
