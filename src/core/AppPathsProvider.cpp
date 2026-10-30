#include "AppPathsProvider.hpp"

#include "LaunchCandidate.hpp"
#include "LaunchRole.hpp"
#include "ProviderFingerprint.hpp"
#include "ProviderIds.hpp"
#include "../platform/LaunchTargetInspector.hpp"
#include "../platform/RunByNameInspector.hpp"
#include "../platform/WinUtil.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shlobj.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace altrun {

namespace {

constexpr wchar_t kAppPathsKey[] =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\App Paths";

constexpr wchar_t kUninstallKey[] =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall";

constexpr std::wstring_view kPackageStore =
    L"\\program files\\windowsapps\\";

std::wstring StripQuotes(
    std::wstring value) {

    value = win::Trim(value);

    if (value.size() >= 2 &&
        value.front() == L'"' &&
        value.back() == L'"') {
        value =
            value.substr(
                1,
                value.size() - 2);
    }

    return value;
}

std::wstring RegistryString(
    HKEY key,
    const wchar_t* valueName) {

    DWORD type = 0;
    DWORD bytes = 0;

    if (RegQueryValueExW(
            key,
            valueName,
            nullptr,
            &type,
            nullptr,
            &bytes) != ERROR_SUCCESS ||
        bytes == 0 ||
        (type != REG_SZ &&
         type != REG_EXPAND_SZ)) {
        return {};
    }

    std::wstring value(
        bytes / sizeof(wchar_t),
        L'\0');

    if (RegQueryValueExW(
            key,
            valueName,
            nullptr,
            &type,
            reinterpret_cast<BYTE*>(
                value.data()),
            &bytes) != ERROR_SUCCESS) {
        return {};
    }

    while (!value.empty() &&
           value.back() == L'\0') {
        value.pop_back();
    }

    if (type == REG_EXPAND_SZ) {
        value =
            win::ExpandEnvironment(
                value);
    }

    return StripQuotes(
        std::move(value));
}

std::wstring NormalizeTarget(
    std::wstring_view target) {

    std::wstring normalized =
        win::Lower(
            win::Trim(target));

    std::replace(
        normalized.begin(),
        normalized.end(),
        L'/',
        L'\\');

    return normalized;
}

std::wstring MakeId(
    std::wstring_view target) {

    return L"apppath:" +
        NormalizeTarget(target);
}

std::wstring FriendlyStem(
    std::wstring_view value) {

    const std::filesystem::path path(
        value);

    std::wstring stem =
        path.stem().wstring();

    if (stem.empty()) {
        stem =
            path.filename().wstring();
    }

    return stem;
}

bool AddDiscovered(
    std::vector<Command>& output,
    std::unordered_set<std::wstring>& seenTargets,
    Command command) {

    const std::wstring targetKey =
        NormalizeTarget(
            command.target);

    if (targetKey.empty() ||
        !seenTargets.insert(
            targetKey).second) {
        return false;
    }

    if (command.keyword.empty()) {
        command.keyword =
            win::CompactKeyword(
                command.title);
    }

    if (command.keyword.empty()) {
        command.keyword =
            win::Lower(
                command.title);
    }

    output.push_back(
        std::move(command));

    return true;
}

std::wstring NormalizeRoot(
    std::wstring_view value) {

    std::wstring root =
        NormalizeTarget(value);

    while (root.size() > 3 &&
           root.back() == L'\\') {
        root.pop_back();
    }

    return root;
}

bool PathWithin(
    std::wstring_view path,
    std::wstring_view root) {

    return !root.empty() &&
        path.size() > root.size() &&
        path.starts_with(root) &&
        path[root.size()] == L'\\';
}

std::wstring KnownFolderRoot(
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

    std::wstring root =
        NormalizeRoot(path);
    CoTaskMemFree(path);
    return root;
}

// Everything discovery needs to judge where a run-by-name entry comes from,
// gathered once per refresh rather than per entry.
struct RunByNameContext {
    std::filesystem::path aliasDirectory;
    std::vector<std::wstring> sharedRoots;
    std::vector<std::wstring> broadRoots;
    std::vector<std::wstring> installRoots;
    std::unordered_map<
        std::wstring,
        std::vector<std::wstring>>
        packageServers;
};

// A drive root, or a folder that holds many products, is not one product.
bool IsBroadRoot(
    const RunByNameContext& context,
    std::wstring_view root) {

    return root.size() <= 3 ||
        std::find(
            context.broadRoots.begin(),
            context.broadRoots.end(),
            root) !=
            context.broadRoots.end();
}

void CollectInstallRoots(
    HKEY root,
    REGSAM view,
    RunByNameContext& context) {

    HKEY uninstall{};

    if (RegOpenKeyExW(
            root,
            kUninstallKey,
            0,
            KEY_READ | view,
            &uninstall) != ERROR_SUCCESS) {
        return;
    }

    for (DWORD index = 0;; ++index) {
        std::array<wchar_t, 512>
            nameBuffer{};

        DWORD nameLength =
            static_cast<DWORD>(
                nameBuffer.size());

        const LSTATUS status =
            RegEnumKeyExW(
                uninstall,
                index,
                nameBuffer.data(),
                &nameLength,
                nullptr,
                nullptr,
                nullptr,
                nullptr);

        if (status ==
            ERROR_NO_MORE_ITEMS) {
            break;
        }

        if (status != ERROR_SUCCESS) {
            continue;
        }

        HKEY productKey{};

        if (RegOpenKeyExW(
                uninstall,
                nameBuffer.data(),
                0,
                KEY_READ | view,
                &productKey) != ERROR_SUCCESS) {
            continue;
        }

        const std::wstring location =
            NormalizeRoot(
                RegistryString(
                    productKey,
                    L"InstallLocation"));

        RegCloseKey(productKey);

        if (location.size() < 3 ||
            location[1] != L':' ||
            IsBroadRoot(
                context,
                location)) {
            continue;
        }

        context.installRoots.push_back(
            location);
    }

    RegCloseKey(uninstall);
}

RunByNameContext MakeRunByNameContext() {

    RunByNameContext context;

    context.aliasDirectory =
        win::AppExecutionAliasDirectory();

    for (const KNOWNFOLDERID* id : {
             &FOLDERID_ProgramFilesCommon,
             &FOLDERID_ProgramFilesCommonX86,
             &FOLDERID_ProgramFilesCommonX64}) {
        std::wstring root =
            KnownFolderRoot(*id);

        if (!root.empty()) {
            context.sharedRoots.push_back(
                std::move(root));
        }
    }

    context.broadRoots =
        context.sharedRoots;

    for (const KNOWNFOLDERID* id : {
             &FOLDERID_ProgramFiles,
             &FOLDERID_ProgramFilesX86,
             &FOLDERID_ProgramFilesX64,
             &FOLDERID_UserProgramFiles,
             &FOLDERID_Windows,
             &FOLDERID_System,
             &FOLDERID_SystemX86,
             &FOLDERID_LocalAppData,
             &FOLDERID_RoamingAppData,
             &FOLDERID_ProgramData,
             &FOLDERID_Profile}) {
        std::wstring root =
            KnownFolderRoot(*id);

        if (!root.empty()) {
            context.broadRoots.push_back(
                std::move(root));
        }
    }

    CollectInstallRoots(
        HKEY_LOCAL_MACHINE,
        KEY_WOW64_64KEY,
        context);
    CollectInstallRoots(
        HKEY_LOCAL_MACHINE,
        KEY_WOW64_32KEY,
        context);
    CollectInstallRoots(
        HKEY_CURRENT_USER,
        KEY_WOW64_64KEY,
        context);

    std::sort(
        context.installRoots.begin(),
        context.installRoots.end());
    context.installRoots.erase(
        std::unique(
            context.installRoots.begin(),
            context.installRoots.end()),
        context.installRoots.end());

    return context;
}

// "c:\program files\windowsapps\<package full name>" for a packaged EXE.
std::wstring PackageRoot(
    std::wstring_view normalizedTarget) {

    if (normalizedTarget.size() < 3 ||
        normalizedTarget[1] != L':' ||
        !normalizedTarget.substr(2)
             .starts_with(kPackageStore)) {
        return {};
    }

    const std::size_t begin =
        2 + kPackageStore.size();
    const std::size_t end =
        normalizedTarget.find(
            L'\\',
            begin);

    if (end == std::wstring_view::npos ||
        end == begin) {
        return {};
    }

    return std::wstring(
        normalizedTarget.substr(0, end));
}

RunByNameEvidence CollectRunByNameEvidence(
    RunByNameContext& context,
    std::wstring_view alias,
    std::wstring_view target) {

    RunByNameEvidence evidence;

    const std::wstring normalized =
        NormalizeTarget(target);
    const std::wstring packageRoot =
        PackageRoot(normalized);

    if (!packageRoot.empty()) {
        // MSIX mirrors its execution aliases into App Paths. The per-user
        // alias stub names the exact application that owns this one.
        if (!context.aliasDirectory.empty()) {
            const auto link =
                win::ReadAppExecutionAlias(
                    context.aliasDirectory /
                    std::wstring(alias));

            if (link &&
                NormalizeTarget(
                    link->target) ==
                    normalized) {
                evidence.packagedAppId =
                    link->appUserModelId;
            }
        }

        auto [servers, inserted] =
            context.packageServers
                .try_emplace(packageRoot);

        if (inserted) {
            servers->second =
                win::PackageServerExecutables(
                    std::filesystem::path(
                        packageRoot));
        }

        const std::wstring relative =
            normalized.substr(
                packageRoot.size() + 1);

        evidence.packageServer =
            std::find(
                servers->second.begin(),
                servers->second.end(),
                relative) !=
            servers->second.end();

        return evidence;
    }

    evidence.osComponent =
        win::IsOwnedByTrustedInstaller(
            std::filesystem::path(target));

    evidence.sharedComponent =
        std::any_of(
            context.sharedRoots.begin(),
            context.sharedRoots.end(),
            [&](const std::wstring& root) {
                return PathWithin(
                    normalized,
                    root);
            });

    // The deepest registered install location wins; otherwise the EXE's
    // own folder stands in for the product.
    for (const auto& root :
         context.installRoots) {
        if (PathWithin(normalized, root) &&
            root.size() >
                evidence.productRoot.size()) {
            evidence.productRoot = root;
        }
    }

    if (evidence.productRoot.empty()) {
        std::wstring folder =
            NormalizeRoot(
                std::filesystem::path(
                    normalized)
                    .parent_path()
                    .wstring());

        if (!IsBroadRoot(
                context,
                folder)) {
            evidence.productRoot =
                std::move(folder);
        }
    }

    return evidence;
}

void EnumerateAppPathsKey(
    HKEY root,
    REGSAM view,
    std::vector<Command>& output,
    std::unordered_set<std::wstring>& seenTargets,
    ProviderAdmissionDiagnostics&
        diagnostics,
    RunByNameContext& context,
    std::stop_token stop) {

    HKEY appPaths{};

    if (RegOpenKeyExW(
            root,
            kAppPathsKey,
            0,
            KEY_READ | view,
            &appPaths) != ERROR_SUCCESS) {
        return;
    }

    DWORD index = 0;

    for (;;) {
        if (stop.stop_requested()) break;
        std::array<wchar_t, 512>
            nameBuffer{};

        DWORD nameLength =
            static_cast<DWORD>(
                nameBuffer.size());

        const LSTATUS status =
            RegEnumKeyExW(
                appPaths,
                index,
                nameBuffer.data(),
                &nameLength,
                nullptr,
                nullptr,
                nullptr,
                nullptr);

        if (status ==
            ERROR_NO_MORE_ITEMS) {
            break;
        }

        ++index;

        if (status != ERROR_SUCCESS ||
            nameLength == 0) {
            continue;
        }

        std::wstring subkeyName(
            nameBuffer.data(),
            nameLength);

        HKEY appKey{};

        if (RegOpenKeyExW(
                appPaths,
                subkeyName.c_str(),
                0,
                KEY_READ | view,
                &appKey) != ERROR_SUCCESS) {
            continue;
        }

        const std::wstring target =
            RegistryString(
                appKey,
                nullptr);

        RegCloseKey(appKey);

        if (target.empty()) {
            continue;
        }

        std::wstring title =
            FriendlyStem(
                subkeyName);

        if (title.empty()) {
            title =
                FriendlyStem(
                    target);
        }

        std::error_code targetError;
        if (!std::filesystem::is_regular_file(
                std::filesystem::path(target),
                targetError)) {
            LaunchAdmission rejected;
            rejected.admit = false;
            rejected.surface =
                LaunchSurfaceClass::
                    PrimaryApplication;
            rejected.reason =
                LaunchAdmissionReason::
                    TargetMissing;

            diagnostics.Record(
                title,
                target,
                target,
                LaunchTargetKind::Unknown,
                LaunchSurfaceClass::
                    PrimaryApplication,
                true,
                rejected);

            // App Paths entries are not guaranteed to be cleaned up when an
            // application is moved or uninstalled.
            continue;
        }



        const LaunchTargetKind targetKind =
            win::InspectLaunchTarget(
                target);

        const LaunchSurfaceClass
            initialSurface =
                ClassifyApplicationSurface(
                    title,
                    target);

        const LaunchAdmission admission =
            EvaluateLaunchCandidate({
                LaunchCandidateSource::
                    AppPaths,
                title,
                target,
                initialSurface,
                targetKind,
                true,
            });

        diagnostics.Record(
            title,
            target,
            target,
            targetKind,
            initialSurface,
            true,
            admission);

        if (!admission.admit) {
            continue;
        }

        LaunchEvidence evidence;
        evidence.source =
            LaunchCandidateSource::AppPaths;
        evidence.displayTitle = title;
        evidence.resolvedTarget = target;
        evidence.installRootHint =
            std::filesystem::path(
                target)
                .parent_path();
        evidence.targetKind =
            targetKind;
        evidence.executable =
            win::InspectExecutableMetadata(
                target);

        const ApplicationRoleDecision role =
            ClassifyApplicationRole(
                evidence);

        Command command;
        command.title =
            std::move(title);
        command.keyword =
            win::CompactKeyword(
                command.title);
        command.target = target;
        command.activationKind =
            LaunchActivationKind::
                ShellItem;
        command.canonicalIdentity =
            BuildCanonicalLaunchIdentity(
                command.activationKind,
                command.target);
        command.type =
            CommandType::Application;
        command.enabled = true;
        command.source =
            CommandSource::AppPaths;
        command.surfaceClass =
            admission.surface;
        command.applicationRole =
            role.role;
        command.roleConfidence =
            role.confidence;
        command.catalogVisibility =
            role.visibility;
        command.catalogGroupKey =
            role.catalogGroupKey;
        command.distinctiveTokens =
            role.distinctiveTokens;
        command.runByName =
            CollectRunByNameEvidence(
                context,
                subkeyName,
                command.target);
        command.basePriority = 0;
        command.id =
            MakeId(command.target);

        AddDiscovered(
            output,
            seenTargets,
            std::move(command));
    }

    RegCloseKey(appPaths);
}

void FingerprintAppPathsKey(
    HKEY root,
    REGSAM view,
    std::vector<std::uint64_t>& items) {

    HKEY appPaths{};

    if (RegOpenKeyExW(
            root,
            kAppPathsKey,
            0,
            KEY_READ | view,
            &appPaths) != ERROR_SUCCESS) {
        return;
    }

    DWORD index = 0;

    for (;;) {
        std::array<wchar_t, 512>
            nameBuffer{};

        DWORD nameLength =
            static_cast<DWORD>(
                nameBuffer.size());

        FILETIME lastWrite{};

        const LSTATUS status =
            RegEnumKeyExW(
                appPaths,
                index,
                nameBuffer.data(),
                &nameLength,
                nullptr,
                nullptr,
                nullptr,
                &lastWrite);

        if (status ==
            ERROR_NO_MORE_ITEMS) {
            break;
        }

        ++index;

        if (status != ERROR_SUCCESS ||
            nameLength == 0) {
            continue;
        }

        std::wstring name(
            nameBuffer.data(),
            nameLength);

        std::uint64_t itemHash =
            fingerprint::kOffset;

        fingerprint::Mix(
            itemHash,
            name);

        const std::uint64_t timeValue =
            (static_cast<std::uint64_t>(
                 lastWrite.dwHighDateTime)
             << 32u) |
            lastWrite.dwLowDateTime;

        fingerprint::Mix(
            itemHash,
            timeValue);

        HKEY appKey{};

        if (RegOpenKeyExW(
                appPaths,
                name.c_str(),
                0,
                KEY_READ | view,
                &appKey) == ERROR_SUCCESS) {

            fingerprint::Mix(
                itemHash,
                RegistryString(
                    appKey,
                    nullptr));

            RegCloseKey(appKey);
        }

        items.push_back(
            itemHash);
    }

    RegCloseKey(appPaths);
}

} // namespace

const ProviderDescriptor&
AppPathsProvider::Descriptor() const noexcept {
    static const ProviderDescriptor descriptor{
        std::string(providers::kAppPaths),
        L"App Paths",
        true,
        20,
    };
    return descriptor;
}

std::vector<Command>
AppPathsProvider::Discover() const {
    return DiscoverDetailed().commands;
}

ProviderDiscoveryPayload
AppPathsProvider::DiscoverDetailed(std::stop_token stop) const {

    ProviderDiscoveryPayload payload;
    auto& commands = payload.commands;
    auto& diagnostics =
        payload.admission;

    std::unordered_set<std::wstring>
        seenTargets;

    constexpr std::array<REGSAM, 2>
        views{
            KEY_WOW64_64KEY,
            KEY_WOW64_32KEY,
        };

    RunByNameContext context =
        MakeRunByNameContext();

    for (const REGSAM view : views) {
        if (stop.stop_requested()) break;
        EnumerateAppPathsKey(
            HKEY_CURRENT_USER,
            view,
            commands,
            seenTargets,
            diagnostics,
            context,
            stop);
        if (stop.stop_requested()) break;

        EnumerateAppPathsKey(
            HKEY_LOCAL_MACHINE,
            view,
            commands,
            seenTargets,
            diagnostics,
            context,
            stop);
    }

    return payload;
}

std::uint64_t
AppPathsProvider::ChangeToken() const {
    std::vector<std::uint64_t> items;

    constexpr std::array<REGSAM, 2>
        views{
            KEY_WOW64_64KEY,
            KEY_WOW64_32KEY,
        };

    for (const REGSAM view : views) {
        FingerprintAppPathsKey(
            HKEY_CURRENT_USER,
            view,
            items);

        FingerprintAppPathsKey(
            HKEY_LOCAL_MACHINE,
            view,
            items);
    }

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

} // namespace altrun
