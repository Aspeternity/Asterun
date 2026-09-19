#include "ProviderRegistry.hpp"

#include "AppPathsProvider.hpp"
#include "PackagedAppProvider.hpp"
#include "PathProvider.hpp"
#include "StartMenuProvider.hpp"
#include "TextCodec.hpp"

#include <algorithm>
#include <exception>
#include <utility>

namespace altrun {

ProviderRegistry::ProviderRegistry() {
    providers_.push_back(
        std::make_unique<
            StartMenuProvider>());
    providers_.push_back(
        std::make_unique<
            PackagedAppProvider>());
    providers_.push_back(
        std::make_unique<
            AppPathsProvider>());
    providers_.push_back(
        std::make_unique<
            PathProvider>());

    std::stable_sort(
        providers_.begin(),
        providers_.end(),
        [](const auto& left,
           const auto& right) {
            return left->Descriptor().priority >
                right->Descriptor().priority;
        });
}

std::vector<ProviderDescriptor>
ProviderRegistry::Descriptors() const {
    std::vector<ProviderDescriptor>
        descriptors;

    descriptors.reserve(
        providers_.size());

    for (const auto& provider :
         providers_) {
        descriptors.push_back(
            provider->Descriptor());
    }

    return descriptors;
}

std::vector<ProviderDiscoveryResult>
ProviderRegistry::Discover(
    const ProviderEnableMap& enabled) const {

    std::vector<ProviderDiscoveryResult>
        results;

    for (const auto& provider :
         providers_) {
        const auto& descriptor =
            provider->Descriptor();

        if (!providers::IsEnabled(
                enabled,
                descriptor.id,
                descriptor.defaultEnabled)) {
            continue;
        }

        ProviderDiscoveryResult result;
        result.id = descriptor.id;

        try {
            result.commands =
                provider->Discover();
            result.success = true;
        } catch (const std::exception& error) {
            result.error =
                text::FromUtf8(
                    error.what());
        } catch (...) {
            result.error =
                L"Unknown provider discovery error.";
        }

        results.push_back(
            std::move(result));
    }

    return results;
}

} // namespace altrun
