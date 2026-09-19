#pragma once

#include "LauncherResult.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace altrun {

struct DynamicQueryRequest {
    std::uint64_t generation{0};
    std::wstring query;
    std::size_t limit{10};
};

enum class DynamicQueryStatus {
    Success,
    Unavailable,
    Timeout,
    Error,
};

struct DynamicQueryResponse {
    std::uint64_t generation{0};
    std::string providerId;
    DynamicQueryStatus status{
        DynamicQueryStatus::Unavailable};
    std::vector<LauncherResult> results;
    std::uint32_t totalMatches{0};
    std::uint64_t latencyMicros{0};
    std::uint32_t nativeError{0};
};

class DynamicQueryProvider {
public:
    using Completion =
        std::function<void(
            DynamicQueryResponse)>;

    virtual ~DynamicQueryProvider() =
        default;

    [[nodiscard]]
    virtual std::string_view
    Id() const noexcept = 0;

    [[nodiscard]]
    virtual bool
    IsAvailable() const noexcept = 0;

    virtual void QueryAsync(
        DynamicQueryRequest request,
        Completion completion) = 0;
};

} // namespace altrun
