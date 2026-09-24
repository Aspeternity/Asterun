#include "PackagedAppProvider.hpp"

#include "LaunchCandidate.hpp"
#include "ProviderFingerprint.hpp"
#include "ProviderIds.hpp"
#include "../platform/LaunchTargetInspector.hpp"
#include "../platform/WinUtil.hpp"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <knownfolders.h>
#include <propkey.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <wrl/client.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

namespace altrun {

namespace {

using Microsoft::WRL::ComPtr;

struct ShellApp {
    std::wstring title;
    std::wstring target;
    PackagedVisibilityEvidence
        visibility;
};

class ComApartment final {
public:
    ComApartment()
        : result_(
              CoInitializeEx(
                  nullptr,
                  COINIT_APARTMENTTHREADED)) {

        if (FAILED(result_) &&
            result_ !=
                RPC_E_CHANGED_MODE) {
            throw std::runtime_error(
                "Unable to initialize COM for AppsFolder discovery.");
        }
    }

    ~ComApartment() {
        if (SUCCEEDED(result_)) {
            CoUninitialize();
        }
    }

private:
    HRESULT result_{};
};

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

std::vector<ShellApp>
EnumerateAppsFolder() {

    ComApartment apartment;
    std::vector<ShellApp> apps;

    ComPtr<IShellItem> appsFolder;

    const HRESULT folderResult =
        SHGetKnownFolderItem(
            FOLDERID_AppsFolder,
            KF_FLAG_DEFAULT,
            nullptr,
            IID_PPV_ARGS(
                &appsFolder));

    if (FAILED(folderResult) ||
        !appsFolder) {
        throw std::runtime_error(
            "Unable to open the Windows AppsFolder.");
    }

    ComPtr<IEnumShellItems> enumerator;

    const HRESULT bindResult =
        appsFolder->BindToHandler(
            nullptr,
            BHID_EnumItems,
            IID_PPV_ARGS(
                &enumerator));

    if (FAILED(bindResult) ||
        !enumerator) {
        throw std::runtime_error(
            "Unable to enumerate the Windows AppsFolder.");
    }

    for (;;) {
        ComPtr<IShellItem> item;
        ULONG fetched = 0;

        const HRESULT next =
            enumerator->Next(
                1,
                item.GetAddressOf(),
                &fetched);

        if (next == S_FALSE ||
            fetched == 0) {
            break;
        }

        if (FAILED(next)) {
            throw std::runtime_error(
                "Windows AppsFolder enumeration failed.");
        }

        if (next != S_OK ||
            fetched != 1 ||
            !item) {
            continue;
        }

        PWSTR rawTitle = nullptr;

        if (FAILED(
                item->GetDisplayName(
                    SIGDN_NORMALDISPLAY,
                    &rawTitle)) ||
            rawTitle == nullptr) {
            continue;
        }

        std::wstring title(
            rawTitle);

        CoTaskMemFree(
            rawTitle);

        if (title.empty()) {
            continue;
        }

        PWSTR rawTarget = nullptr;

        if (FAILED(
                item->GetDisplayName(
                    SIGDN_DESKTOPABSOLUTEPARSING,
                    &rawTarget)) ||
            rawTarget == nullptr) {
            continue;
        }

        std::wstring target(
            rawTarget);

        CoTaskMemFree(
            rawTarget);

        if (target.empty()) {
            continue;
        }

        PackagedVisibilityEvidence
            visibility;

        SFGAOF attributes{};

        if (SUCCEEDED(
                item->GetAttributes(
                    static_cast<SFGAOF>(
                        SFGAO_HIDDEN |
                        SFGAO_SYSTEM),
                    &attributes))) {
            visibility.hidden =
                (attributes &
                 SFGAO_HIDDEN) != 0;
            visibility.system =
                (attributes &
                 SFGAO_SYSTEM) != 0;
        }

        ComPtr<IShellItem2> item2;

        if (SUCCEEDED(
                item.As(&item2)) &&
            item2) {
            BOOL value = FALSE;

            if (SUCCEEDED(
                    item2->GetBool(
                        PKEY_AppUserModel_PreventPinning,
                        &value))) {
                visibility.preventPinning =
                    value != FALSE;
            }

        }

        apps.push_back({
            std::move(title),
            std::move(target),
            visibility,
        });
    }

    return apps;
}

} // namespace

const ProviderDescriptor&
PackagedAppProvider::Descriptor() const noexcept {
    static const ProviderDescriptor descriptor{
        std::string(providers::kPackaged),
        L"Windows Apps",
        true,
        30,
    };
    return descriptor;
}

std::vector<Command>
PackagedAppProvider::Discover() const {
    return DiscoverDetailed().commands;
}

ProviderDiscoveryPayload
PackagedAppProvider::DiscoverDetailed() const {

    ProviderDiscoveryPayload payload;
    auto& commands = payload.commands;
    auto& diagnostics =
        payload.admission;

    std::unordered_set<std::wstring>
        seenTargets;

    for (auto app :
         EnumerateAppsFolder()) {

        const std::wstring targetKey =
            NormalizeTarget(
                app.target);

        if (targetKey.empty() ||
            !seenTargets.insert(
                targetKey).second) {
            continue;
        }

        LaunchTargetKind targetKind =
            InferTextTargetKind(
                app.target);

        if (targetKind ==
            LaunchTargetKind::Unknown) {
            std::error_code targetError;

            if (std::filesystem::is_regular_file(
                    std::filesystem::path(
                        app.target),
                    targetError)) {
                targetKind =
                    win::InspectLaunchTarget(
                        app.target);
            }
        }

        const LaunchSurfaceClass
            initialSurface =
                ClassifyApplicationSurface(
                    app.title,
                    app.target);

        const LaunchAdmission admission =
            EvaluateLaunchCandidate({
                LaunchCandidateSource::
                    AppsFolder,
                app.title,
                app.target,
                initialSurface,
                targetKind,
                true,
                {},
                app.visibility,
            });

        diagnostics.Record(
            app.title,
            app.target,
            app.target,
            targetKind,
            initialSurface,
            true,
            admission);

        if (!admission.admit) {
            continue;
        }

        Command command;
        command.title =
            std::move(app.title);
        command.keyword =
            win::CompactKeyword(
                command.title);

        if (command.keyword.empty()) {
            command.keyword =
                win::Lower(
                    command.title);
        }

        command.target =
            std::move(app.target);
        command.activationKind =
            ActivationKindForCatalogTarget(
                command.target);
        command.canonicalIdentity =
            BuildCanonicalLaunchIdentity(
                command.activationKind,
                command.target);
        command.type =
            CommandType::Application;
        command.icon = L"auto";
        command.enabled = true;
        command.source =
            CommandSource::PackagedApp;
        command.surfaceClass =
            admission.surface;
        command.basePriority = 0;
        command.id =
            L"packaged:" +
            targetKey;

        commands.push_back(
            std::move(command));
    }

    return payload;
}

std::uint64_t
PackagedAppProvider::ChangeToken() const {

    std::vector<std::uint64_t> items;

    for (const auto& app :
         EnumerateAppsFolder()) {

        std::uint64_t itemHash =
            fingerprint::kOffset;

        fingerprint::Mix(
            itemHash,
            app.title);

        fingerprint::Mix(
            itemHash,
            NormalizeTarget(
                app.target));

        items.push_back(
            itemHash);
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

    for (const auto item :
         items) {
        fingerprint::Mix(
            hash,
            item);
    }

    return hash;
}

} // namespace altrun
