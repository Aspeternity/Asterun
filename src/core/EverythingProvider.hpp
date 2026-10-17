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
    // Broad query and name-prefix pass run concurrently on separate clients
    // (each client keeps one query in flight).
    EverythingIpcClient client_;
    EverythingIpcClient prefixClient_;
};

} // namespace altrun
