#include "StartMenuProvider.hpp"

#include "LaunchCandidate.hpp"
#include "LaunchRole.hpp"
#include "ProviderFingerprint.hpp"
#include "ProviderIds.hpp"
#include "../platform/LaunchTargetInspector.hpp"
#include "../platform/WinUtil.hpp"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shlobj.h>

#include <algorithm>
#include <cstdint>
#include <cwctype>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>

namespace altrun {

namespace {

std::filesystem::path KnownFolder(
    REFKNOWNFOLDERID id) {

    PWSTR path = nullptr;

    if (FAILED(SHGetKnownFolderPath(
            id,
            KF_FLAG_DEFAULT,
            nullptr,
            &path)) ||
        path == nullptr) {
        return {};
    }

    std::filesystem::path result(path);
    CoTaskMemFree(path);
    return result;
}

std::wstring MakeId(
    std::wstring_view keyword,
    std::wstring_view target) {

    return L"start:" +
        win::Lower(keyword) +
        L":" +
        win::Lower(target);
}

bool IsStartMenuEntry(
    const std::filesystem::path& path) {

    const auto extension =
        win::Lower(
            path.extension().wstring());

    return extension == L".lnk" ||
           extension == L".exe";
}

std::wstring LowerPathText(
    const std::filesystem::path& path) {

    std::wstring value =
        win::Lower(
            path.lexically_normal()
                .wstring());

    std::replace(
        value.begin(),
        value.end(),
        L'/',
        L'\\');

    return value;
}

bool PathWithin(
    const std::filesystem::path& path,
    const std::filesystem::path& root) {

    if (root.empty()) {
        return false;
    }

    const std::wstring value =
        LowerPathText(path);
    std::wstring prefix =
        LowerPathText(root);

    if (value.empty() ||
        prefix.empty()) {
        return false;
    }

    while (prefix.size() > 3 &&
           prefix.back() == L'\\') {
        prefix.pop_back();
    }

    return value == prefix ||
        (value.size() > prefix.size() &&
         value.compare(
             0,
             prefix.size(),
             prefix) == 0 &&
         value[prefix.size()] == L'\\');
}

bool HasPathComponent(
    const std::filesystem::path& path,
    std::wstring_view component) {

    const std::wstring wrapped =
        L"\\" +
        LowerPathText(
            path.parent_path()) +
        L"\\";

    const std::wstring needle =
        L"\\" +
        win::Lower(component) +
        L"\\";

    return wrapped.find(needle) !=
        std::wstring::npos;
}

bool IsAdministrativeEntry(
    const std::filesystem::path& path) {

    static const std::filesystem::path
        userAdmin =
            KnownFolder(
                FOLDERID_AdminTools);
    static const std::filesystem::path
        commonAdmin =
            KnownFolder(
                FOLDERID_CommonAdminTools);

    return
        PathWithin(
            path,
            userAdmin) ||
        PathWithin(
            path,
            commonAdmin) ||
        HasPathComponent(
            path,
            L"Windows Tools") ||
        HasPathComponent(
            path,
            L"Administrative Tools") ||
        HasPathComponent(
            path,
            L"System Tools");
}

bool IsDeveloperEntry(
    const std::filesystem::path& path) {

    return
        HasPathComponent(
            path,
            L"Developer Tools") ||
        HasPathComponent(
            path,
            L"Visual Studio Tools") ||
        HasPathComponent(
            path,
            L"Windows Kits") ||
        HasPathComponent(
            path,
            L"SDK");
}

struct StartMenuInspection {
    std::wstring resolvedTarget;
    std::wstring arguments;
    LaunchTargetKind targetKind{
        LaunchTargetKind::Unknown};
    LaunchSurfaceClass surface{
        LaunchSurfaceClass::
            PrimaryApplication};
    bool targetResolved{false};
    LaunchAdmission admission;
    ApplicationRoleDecision role;
};

StartMenuInspection
InspectStartMenuEntry(
    const std::filesystem::path& path) {

    StartMenuInspection result;

    const std::wstring title =
        path.stem().wstring();

    const std::wstring extension =
        win::Lower(
            path.extension().wstring());

    if (extension == L".lnk") {
        const auto shortcut =
            win::InspectShellLink(path);

        if (!shortcut) {
            result.admission.admit = false;
            result.admission.surface =
                LaunchSurfaceClass::
                    PrimaryApplication;
            result.admission.reason =
                LaunchAdmissionReason::
                    TargetResolutionFailed;
            return result;
        }

        result.resolvedTarget =
            shortcut->target;
        result.arguments =
            shortcut->arguments;
        result.targetKind =
            shortcut->targetKind;
        result.targetResolved = true;
    } else {
        result.resolvedTarget =
            path.wstring();
        result.targetKind =
            win::InspectLaunchTarget(
                result.resolvedTarget);
        result.targetResolved = true;
    }

    result.surface =
        ClassifyApplicationSurface(
            title,
            result.resolvedTarget);

    // A root-level Start Menu shortcut can resolve into Windows Tools /
    // Administrative Tools / Developer Tools even when the shortcut itself
    // is not stored inside that folder. Classify from both sides of the
    // shortcut relationship so the cached launch surface reflects the real
    // destination instead of only the publication location.
    const std::filesystem::path
        resolvedTargetPath(
            result.resolvedTarget);

    if (IsAdministrativeEntry(path) ||
        IsAdministrativeEntry(
            resolvedTargetPath)) {
        result.surface =
            LaunchSurfaceClass::
                SystemUtility;
    } else if (
        IsDeveloperEntry(path) ||
        IsDeveloperEntry(
            resolvedTargetPath)) {
        result.surface =
            LaunchSurfaceClass::
                DeveloperTool;
    }

    result.admission =
        EvaluateLaunchCandidate({
            LaunchCandidateSource::
                StartMenu,
            title,
            result.resolvedTarget,
            result.surface,
            result.targetKind,
            result.targetResolved,
            result.arguments,
        });

    if (result.admission.admit) {
        LaunchEvidence evidence;
        evidence.source =
            LaunchCandidateSource::
                StartMenu;
        evidence.displayTitle = title;
        evidence.resolvedTarget =
            result.resolvedTarget;
        evidence.arguments =
            result.arguments;
        evidence.shortcutPath = path;
        evidence.startMenuFolder =
            path.parent_path();
        evidence.installRootHint =
            std::filesystem::path(
                result.resolvedTarget)
                .parent_path();
        evidence.targetKind =
            result.targetKind;
        evidence.executable =
            win::InspectExecutableMetadata(
                result.resolvedTarget);

        result.role =
            ClassifyApplicationRole(
                evidence);
    }

    return result;
}

} // namespace

const ProviderDescriptor&
StartMenuProvider::Descriptor() const noexcept {
    static const ProviderDescriptor descriptor{
        std::string(providers::kStartMenu),
        L"Start Menu",
        true,
        40,
    };
    return descriptor;
}

std::vector<Command>
StartMenuProvider::Discover() const {
    return DiscoverDetailed().commands;
}

ProviderDiscoveryPayload
StartMenuProvider::DiscoverDetailed() const {
    ProviderDiscoveryPayload payload;

    ScanPath(
        KnownFolder(FOLDERID_StartMenu),
        payload.commands,
        payload.admission);
    ScanPath(
        KnownFolder(FOLDERID_CommonStartMenu),
        payload.commands,
        payload.admission);

    return payload;
}

std::uint64_t
StartMenuProvider::ChangeToken() const {
    std::vector<std::uint64_t> items;

    FingerprintPath(
        KnownFolder(FOLDERID_StartMenu),
        items);
    FingerprintPath(
        KnownFolder(FOLDERID_CommonStartMenu),
        items);

    std::sort(
        items.begin(),
        items.end());

    std::uint64_t hash =
        fingerprint::kOffset;

    fingerprint::Mix(
        hash,
        static_cast<std::uint64_t>(
            items.size()));

    for (const auto item : items) {
        fingerprint::Mix(
            hash,
            item);
    }

    return hash;
}

void StartMenuProvider::ScanPath(
    const std::filesystem::path& root,
    std::vector<Command>& output,
    ProviderAdmissionDiagnostics&
        diagnostics) const {

    if (root.empty() ||
        !std::filesystem::exists(root)) {
        return;
    }

    std::error_code ec;

    for (std::filesystem::recursive_directory_iterator it(
             root,
             std::filesystem::directory_options::
                 skip_permission_denied,
             ec),
         end;
         it != end;
         it.increment(ec)) {

        if (ec) {
            ec.clear();
            continue;
        }

        if (!it->is_regular_file(ec) ||
            !IsStartMenuEntry(
                it->path())) {
            continue;
        }

        const std::wstring title =
            it->path().stem().wstring();

        const auto inspection =
            InspectStartMenuEntry(
                it->path());

        diagnostics.Record(
            title,
            it->path().wstring(),
            inspection.resolvedTarget,
            inspection.targetKind,
            inspection.surface,
            inspection.targetResolved,
            inspection.admission);

        if (!inspection.admission.admit) {
            continue;
        }

        Command command;
        command.title = title;
        command.keyword =
            win::CompactKeyword(
                command.title);
        command.target =
            it->path().wstring();
        command.activationKind =
            LaunchActivationKind::
                ShellItem;
        command.canonicalIdentity =
            BuildCanonicalLaunchIdentity(
                command.activationKind,
                inspection.resolvedTarget,
                inspection.arguments);
        command.type =
            CommandType::Application;
        command.icon = L"auto";
        command.enabled = true;
        command.source =
            CommandSource::StartMenu;
        command.surfaceClass =
            inspection.admission.surface;
        command.applicationRole =
            inspection.role.role;
        command.roleConfidence =
            inspection.role.confidence;
        command.catalogVisibility =
            inspection.role.visibility;
        command.catalogGroupKey =
            inspection.role.catalogGroupKey;
        command.distinctiveTokens =
            inspection.role.distinctiveTokens;
        command.basePriority = 0;

        if (command.keyword.empty()) {
            command.keyword =
                win::Lower(
                    command.title);
        }

        command.id =
            MakeId(
                command.keyword,
                command.target);

        output.push_back(
            std::move(command));
    }
}

void StartMenuProvider::FingerprintPath(
    const std::filesystem::path& root,
    std::vector<std::uint64_t>& items) const {

    if (root.empty()) {
        return;
    }

    std::error_code ec;

    if (!std::filesystem::is_directory(
            root,
            ec)) {
        return;
    }

    for (std::filesystem::recursive_directory_iterator it(
             root,
             std::filesystem::directory_options::
                 skip_permission_denied,
             ec),
         end;
         it != end;
         it.increment(ec)) {

        if (ec) {
            ec.clear();
            continue;
        }

        if (!it->is_regular_file(ec) ||
            !IsStartMenuEntry(
                it->path())) {
            continue;
        }

        std::uint64_t itemHash =
            fingerprint::kOffset;

        fingerprint::Mix(
            itemHash,
            it->path().wstring());

        const auto writeTime =
            it->last_write_time(ec);

        if (!ec) {
            fingerprint::Mix(
                itemHash,
                static_cast<std::uint64_t>(
                    writeTime
                        .time_since_epoch()
                        .count()));
        } else {
            ec.clear();
        }

        const auto size =
            it->file_size(ec);

        if (!ec) {
            fingerprint::Mix(
                itemHash,
                static_cast<std::uint64_t>(
                    size));
        } else {
            ec.clear();
        }

        items.push_back(
            itemHash);
    }
}

} // namespace altrun
