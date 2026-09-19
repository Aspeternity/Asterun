#include "PackagedAppProvider.hpp"

#include "ProviderFingerprint.hpp"
#include "ProviderIds.hpp"
#include "../platform/WinUtil.hpp"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <knownfolders.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <wrl/client.h>

#include <algorithm>
#include <cstdint>
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
        L'\');

    return normalized;
}

std::vector<ShellApp>
EnumerateAppsFolder() {

    std::vector<ShellApp> apps;

    const HRESULT comResult =
        CoInitializeEx(
            nullptr,
            COINIT_APARTMENTTHREADED);

    ComPtr<IShellItem> appsFolder;

    if (SUCCEEDED(
            SHGetKnownFolderItem(
                FOLDERID_AppsFolder,
                KF_FLAG_DEFAULT,
                nullptr,
                IID_PPV_ARGS(
                    &appsFolder)))) {

        ComPtr<IEnumShellItems> enumerator;

        if (SUCCEEDED(
                appsFolder->BindToHandler(
                    nullptr,
                    BHID_EnumItems,
                    IID_PPV_ARGS(
                        &enumerator)))) {

            for (;;) {
                ComPtr<IShellItem> item;
                ULONG fetched = 0;

                const HRESULT next =
                    enumerator->Next(
                        1,
                        item.GetAddressOf(),
                        &fetched);

                if (next != S_OK ||
                    fetched != 1 ||
                    !item) {
                    break;
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

                apps.push_back({
                    std::move(title),
                    std::move(target),
                });
            }
        }
    }

    if (SUCCEEDED(comResult)) {
        CoUninitialize();
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

    std::vector<Command> commands;
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
        command.type =
            CommandType::Application;
        command.icon = L"auto";
        command.enabled = true;
        command.source =
            CommandSource::PackagedApp;
        command.basePriority = -5;
        command.id =
            L"packaged:" +
            targetKey;

        commands.push_back(
            std::move(command));
    }

    return commands;
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
