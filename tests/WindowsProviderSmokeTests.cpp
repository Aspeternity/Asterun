#include "core/ProviderIds.hpp"
#include "core/ProviderRegistry.hpp"

#include <cassert>
#include <iostream>
#include <string>
#include <unordered_set>
#include <vector>

using namespace altrun;

int main() {
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
