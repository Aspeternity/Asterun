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

    using ProbeCompletion = std::function<void(std::uint64_t, classic_behavior::ContinuationEvidence)>;
    void ProbeContinuation(std::uint64_t generation, std::wstring query, ProbeCompletion completion);

private:
    void QueryBroad(DynamicQueryRequest request, Completion completion,
                    std::vector<LauncherResult> prefixes = {});
    EverythingIpcClient client_;
    // Separate generation/queue: a hidden probe cannot cancel the visible query.
    EverythingIpcClient probeClient_;
};

} // namespace altrun
