#pragma once

#include "../core/EverythingQuery.hpp"

#include <windows.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

namespace altrun {

struct EverythingIpcClientOptions {
    std::wstring everythingWindowClass{
        L"EVERYTHING_TASKBAR_NOTIFICATION"};
    std::chrono::milliseconds debounce{70};
    std::chrono::milliseconds sendTimeout{250};
    std::chrono::milliseconds replyTimeout{1000};
};

class EverythingIpcClient {
public:
    using Completion =
        std::function<void(EverythingQueryResult)>;

    explicit EverythingIpcClient(
        EverythingIpcClientOptions options = {});
    ~EverythingIpcClient();

    EverythingIpcClient(
        const EverythingIpcClient&) = delete;
    EverythingIpcClient& operator=(
        const EverythingIpcClient&) = delete;

    void QueryAsync(
        EverythingQueryRequest request,
        Completion completion);

    [[nodiscard]] bool
    IsAvailable() const noexcept;

    [[nodiscard]]
    EverythingIpcStatusSnapshot
    Status() const;

private:
    struct PendingQuery {
        EverythingQueryRequest request;
        Completion completion;
    };

    struct InFlightQuery {
        std::uint64_t generation{0};
        std::uint32_t replyToken{0};
        Completion completion;
        std::chrono::steady_clock::time_point
            started;
    };

    static constexpr UINT
        kQueryQueuedMessage =
            WM_APP + 0x351;
    static constexpr UINT_PTR
        kDebounceTimerId = 0x351;
    static constexpr UINT_PTR
        kReplyTimerId = 0x352;

    static LRESULT CALLBACK WindowProc(
        HWND hwnd,
        UINT message,
        WPARAM wParam,
        LPARAM lParam);

    void WorkerMain(
        std::stop_token stopToken);
    LRESULT HandleWindowMessage(
        HWND hwnd,
        UINT message,
        WPARAM wParam,
        LPARAM lParam);
    void SchedulePendingQuery(
        HWND hwnd);
    void SendPendingQuery(
        HWND hwnd);
    void HandleReply(
        HWND hwnd,
        std::uint32_t replyToken,
        const void* data,
        std::size_t size);
    void HandleReplyTimeout(
        HWND hwnd);

    void CompletePending(
        PendingQuery pending,
        EverythingQueryStatus status,
        std::uint32_t nativeError = 0);
    void CompleteInFlight(
        EverythingQueryStatus status,
        std::uint32_t nativeError = 0);
    void UpdateStatus(
        EverythingAvailability availability,
        EverythingQueryStatus status,
        std::chrono::microseconds latency,
        std::uint32_t resultCount);

    [[nodiscard]] static std::u16string
    ToUtf16(
        std::wstring_view text);
    [[nodiscard]] static std::wstring
    ToWide(
        std::u16string_view text);
    [[nodiscard]] static std::wstring
    JoinPath(
        std::wstring_view parent,
        std::wstring_view name);

    EverythingIpcClientOptions options_;
    std::wstring replyWindowClass_;
    std::jthread worker_;
    std::atomic<HWND>
        workerWindow_{nullptr};
    std::atomic<DWORD>
        workerThreadId_{0};
    std::atomic<std::uint64_t>
        latestGeneration_{0};

    mutable std::mutex pendingMutex_;
    std::optional<PendingQuery>
        pending_;
    std::optional<InFlightQuery>
        inFlight_;
    std::uint32_t
        nextReplyToken_{0xA5100000U};

    mutable std::mutex statusMutex_;
    EverythingIpcStatusSnapshot status_;
};

} // namespace altrun
