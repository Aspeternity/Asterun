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

namespace {

bool IsSelected(
    std::string_view id,
    const std::vector<std::string>& selectedIds) {

    if (selectedIds.empty()) {
        return true;
    }

    return std::find(
               selectedIds.begin(),
               selectedIds.end(),
               id) !=
        selectedIds.end();
}

} // namespace

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
            return left->Descriptor()
                       .priority >
                right->Descriptor()
                    .priority;
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
    const ProviderEnableMap& enabled,
    const std::vector<std::string>&
        selectedIds) const {

    std::vector<ProviderDiscoveryResult>
        results;

    for (const auto& provider :
         providers_) {

        const auto& descriptor =
            provider->Descriptor();

        if (!IsSelected(
                descriptor.id,
                selectedIds) ||
            !providers::IsEnabled(
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

std::vector<ProviderChangeToken>
ProviderRegistry::ChangeTokens(
    const ProviderEnableMap& enabled) const {

    std::vector<ProviderChangeToken>
        tokens;

    tokens.reserve(
        providers_.size());

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

        ProviderChangeToken token;
        token.id =
            descriptor.id;

        try {
            token.token =
                provider->ChangeToken();
            token.success = true;
        } catch (...) {
            token.success = false;
        }

        tokens.push_back(
            std::move(token));
    }

    return tokens;
}

} // namespace altrun
