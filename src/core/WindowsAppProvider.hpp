#pragma once

#include "ICommandProvider.hpp"

#include <filesystem>
#include <string>
#include <unordered_set>
#include <vector>

namespace altrun {

class WindowsAppProvider final : public ICommandProvider {
public:
    [[nodiscard]] std::vector<Command>
    Discover() const override;

private:
    void DiscoverAppPaths(
        std::vector<Command>& output,
        std::unordered_set<std::wstring>& seenTargets) const;

    void DiscoverPathExecutables(
        std::vector<Command>& output,
        std::unordered_set<std::wstring>& seenTargets) const;

    void DiscoverPackagedApps(
        std::vector<Command>& output,
        std::unordered_set<std::wstring>& seenTargets) const;
};

} // namespace altrun
