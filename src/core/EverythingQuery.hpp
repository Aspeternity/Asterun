#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

namespace altrun {

enum class EverythingItemKind {
    File,
    Folder,
};

enum class EverythingQueryStatus {
    Success,
    Unavailable,
    SendTimeout,
    ReplyTimeout,
    ProtocolError,
    Cancelled,
};

enum class EverythingAvailability {
    Unknown,
    Available,
    Unavailable,
};

struct EverythingQueryRequest {
    std::uint64_t generation{0};
    std::wstring query;
    std::uint32_t limit{32};
};

struct EverythingIpcItem {
    EverythingItemKind kind{EverythingItemKind::File};
    std::wstring name;
    std::wstring parentPath;
    std::wstring fullPath;
};

struct EverythingQueryResult {
    std::uint64_t generation{0};
    EverythingQueryStatus status{EverythingQueryStatus::Unavailable};
    std::vector<EverythingIpcItem> items;
    std::uint32_t totalMatches{0};
    std::chrono::microseconds latency{0};
    std::uint32_t nativeError{0};
};

struct EverythingIpcStatusSnapshot {
    EverythingAvailability availability{
        EverythingAvailability::Unknown};
    bool hasQuery{false};
    EverythingQueryStatus lastStatus{
        EverythingQueryStatus::Unavailable};
    std::chrono::microseconds lastLatency{0};
    std::uint32_t lastResultCount{0};
    std::uint32_t lastTotalMatches{0};
    std::uint32_t lastNativeError{0};
};

} // namespace altrun
