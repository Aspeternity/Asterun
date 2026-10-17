#pragma once

#include "DynamicQueryProvider.hpp"
#include "ClassicBehavior.hpp"
#include "../platform/EverythingIpcClient.hpp"

namespace altrun {

class EverythingProvider final
    : public DynamicQueryProvider {
public:
    explicit EverythingProvider(
        EverythingIpcClientOptions
            options = {});

    [[nodiscard]]
    std::string_view
    Id() const noexcept override;

    [[nodiscard]]
    bool
    IsAvailable() const noexcept override;

    [[nodiscard]]
    EverythingIpcStatusSnapshot
    Status() const;

    void QueryAsync(
        DynamicQueryRequest request,
        Completion completion) override;


private:
    void QueryBroad(DynamicQueryRequest request, Completion completion,
                    std::vector<LauncherResult> prefixes = {});
    EverythingIpcClient client_;
    // Separate generation/queue: a hidden probe cannot cancel the visible query.
};

} // namespace altrun
