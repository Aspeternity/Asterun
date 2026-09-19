#pragma once

#include "DynamicQueryProvider.hpp"
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

    void QueryAsync(
        DynamicQueryRequest request,
        Completion completion) override;

private:
    EverythingIpcClient client_;
};

} // namespace altrun
