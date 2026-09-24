#include "core/AppPathsProvider.hpp"
#include "core/LaunchCandidate.hpp"
#include "core/ProviderCache.hpp"
#include "core/ProviderIds.hpp"
#include "core/ProviderRegistry.hpp"
#include "core/StartMenuProvider.hpp"

#include <algorithm>
#include <cassert>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <unordered_set>
#include <vector>

using namespace altrun;

int main() {
    {
        StartMenuProvider provider;

        for (const auto& command :
             provider.Discover()) {
            std::wstring extension =
                std::filesystem::path(
                    command.target)
                    .extension()
                    .wstring();

            std::transform(
                extension.begin(),
                extension.end(),
                extension.begin(),
                [](wchar_t ch) {
                    return static_cast<wchar_t>(
                        std::towlower(ch));
                });

            assert(extension != L".url");
            assert(command.basePriority == 0);
            assert(
                command.surfaceClass !=
                    LaunchSurfaceClass::
                        UserCommand);
            assert(
                command.surfaceClass !=
                    LaunchSurfaceClass::
                        FilesystemItem);
            assert(
                command.surfaceClass !=
                    LaunchSurfaceClass::Action);
            assert(
                command.surfaceClass !=
                    LaunchSurfaceClass::
                        Auxiliary);
            assert(
                command.surfaceClass !=
                    LaunchSurfaceClass::
                        Maintenance);
        }
    }

    {
        AppPathsProvider provider;

        for (const auto& command :
             provider.Discover()) {
            std::error_code ec;
            assert(
                std::filesystem::
                    is_regular_file(
                        std::filesystem::path(
                            command.target),
                        ec));
            assert(
                command.surfaceClass !=
                    LaunchSurfaceClass::
                        UserCommand);
            assert(
                command.surfaceClass !=
                    LaunchSurfaceClass::
                        FilesystemItem);
            assert(
                command.surfaceClass !=
                    LaunchSurfaceClass::
                        Auxiliary);
            assert(
                command.surfaceClass !=
                    LaunchSurfaceClass::
                        Maintenance);
        }
    }

    {
        const auto root =
            std::filesystem::
                temp_directory_path() /
            "ALTRunNext-provider-smoke";

        std::error_code ec;
        std::filesystem::remove_all(
            root,
            ec);
        ec.clear();
        std::filesystem::create_directories(
            root,
            ec);
        assert(!ec);

        const auto cachePath =
            root /
            "provider-cache.json";
        const auto liveTarget =
            root /
            "live.exe";
        const auto missingTarget =
            root /
            "missing.exe";

        {
            std::ofstream liveFile(
                liveTarget,
                std::ios::binary);
            liveFile.put('\0');
        }

        ProviderCacheData data;
        ProviderCacheEntry entry;

        Command live;
        live.id = L"apppath:live";
        live.title = L"Live";
        live.keyword = L"live";
        live.target =
            liveTarget.wstring();
        live.source =
            CommandSource::AppPaths;
        live.enabled = true;
        entry.commands.push_back(live);

        Command stale;
        stale.id = L"apppath:stale";
        stale.title = L"Stale";
        stale.keyword = L"stale";
        stale.target =
            missingTarget.wstring();
        stale.source =
            CommandSource::AppPaths;
        stale.enabled = true;
        entry.commands.push_back(stale);

        data.emplace(
            std::string(
                providers::kAppPaths),
            std::move(entry));

        ProviderCache cache(cachePath);
        assert(cache.Save(data));

        const auto loaded =
            cache.Load();
        const auto it =
            loaded.find(
                std::string(
                    providers::kAppPaths));

        assert(it != loaded.end());
        assert(
            it->second.commands.size() ==
            1);
        assert(
            it->second.commands[0].id ==
            L"apppath:live");

        std::filesystem::remove_all(
            root,
            ec);
    }

    ProviderRegistry registry;

    const auto descriptors =
        registry.Descriptors();

    assert(descriptors.size() == 4);

    std::unordered_set<std::string>
        ids;

    for (const auto& descriptor :
         descriptors) {

        assert(!descriptor.id.empty());
        assert(!descriptor.name.empty());
        assert(ids.insert(
            descriptor.id).second);
    }

    assert(ids.contains(
        std::string(
            providers::kStartMenu)));
    assert(ids.contains(
        std::string(
            providers::kPackaged)));
    assert(ids.contains(
        std::string(
            providers::kAppPaths)));
    assert(ids.contains(
        std::string(
            providers::kPath)));

    const auto pathDescriptor =
        std::find_if(
            descriptors.begin(),
            descriptors.end(),
            [](const ProviderDescriptor& descriptor) {
                return descriptor.id ==
                    providers::kPath;
            });

    assert(pathDescriptor !=
        descriptors.end());
    assert(!pathDescriptor->defaultEnabled);

    ProviderEnableMap disabled =
        providers::DefaultEnabled();

    for (auto& [id, enabled] :
         disabled) {
        enabled = false;
    }

    assert(
        registry.Discover(
            disabled)
            .empty());

    assert(
        registry.ChangeTokens(
            disabled)
            .empty());

    for (const auto& descriptor :
         descriptors) {

        ProviderEnableMap oneEnabled =
            disabled;

        oneEnabled[descriptor.id] =
            true;

        const auto discovery =
            registry.Discover(
                oneEnabled);

        assert(discovery.size() == 1);
        assert(
            discovery[0].id ==
            descriptor.id);

        // A provider is allowed to report failure on a server/runner that
        // lacks the corresponding desktop integration. Registry must still
        // contain the failure as data rather than throwing it to the caller.
        if (!discovery[0].success) {
            std::wcout
                << L"Provider discovery reported a recoverable failure: "
                << descriptor.name
                << L"\n";
        } else {
            const auto& admission =
                discovery[0].admission;

            assert(
                admission.evaluated ==
                admission.admitted +
                    admission.rejected);

            assert(
                admission.admitted >=
                discovery[0]
                    .commands.size());

            for (const auto& sample :
                 admission.rejectedSamples) {
                assert(
                    sample.reason !=
                    LaunchAdmissionReason::
                        Admitted);
            }

            for (const auto& command :
                 discovery[0].commands) {
                assert(
                    command.surfaceClass !=
                        LaunchSurfaceClass::
                            Auxiliary);
                assert(
                    command.surfaceClass !=
                        LaunchSurfaceClass::
                            Maintenance);

                if (descriptor.id ==
                    providers::kPackaged) {
                    assert(
                        !LooksLikeWebTarget(
                            command.target));
                }
            }
        }

        const auto tokens =
            registry.ChangeTokens(
                oneEnabled);

        assert(tokens.size() == 1);
        assert(
            tokens[0].id ==
            descriptor.id);

        ProviderEnableMap allEnabled =
            providers::DefaultEnabled();

        allEnabled[descriptor.id] = true;

        const auto selected =
            registry.Discover(
                allEnabled,
                {descriptor.id});

        assert(selected.size() == 1);
        assert(
            selected[0].id ==
            descriptor.id);
    }

    std::cout
        << "Windows provider smoke tests passed\n";

    return 0;
}
