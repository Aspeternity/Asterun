#include "EverythingIpcClient.hpp"

#include "../core/EverythingIpcProtocol.hpp"

#include <algorithm>
#include <limits>
#include <span>
#include <utility>

namespace altrun {
namespace {

[[nodiscard]] UINT TimerDelay(
    std::chrono::milliseconds value) {
    const auto count =
        std::clamp<std::int64_t>(
            value.count(),
            1,
            static_cast<std::int64_t>(
                std::numeric_limits<UINT>::max()));
    return static_cast<UINT>(count);
}

[[nodiscard]]
EverythingAvailability AvailabilityFor(
    EverythingQueryStatus status) {
    switch (status) {
    case EverythingQueryStatus::Success:
    case EverythingQueryStatus::ReplyTimeout:
    case EverythingQueryStatus::ProtocolError:
        return EverythingAvailability::Available;
    case EverythingQueryStatus::Unavailable:
    case EverythingQueryStatus::SendTimeout:
    case EverythingQueryStatus::Cancelled:
        return EverythingAvailability::Unavailable;
    }
    return EverythingAvailability::Unknown;
}

} // namespace

EverythingIpcClient::EverythingIpcClient(
    EverythingIpcClientOptions options)
    : options_(std::move(options)) {
    replyWindowClass_ =
        L"ALTRunNext.EverythingIpcReply." +
        std::to_wstring(
            GetCurrentProcessId());

    worker_ = std::jthread(
        [this](
            std::stop_token stopToken) {
            WorkerMain(stopToken);
        });
}

EverythingIpcClient::~EverythingIpcClient() {
    worker_.request_stop();

    if (const auto hwnd =
            workerWindow_.load()) {
        PostMessageW(
            hwnd,
            WM_CLOSE,
            0,
            0);
    } else if (
        const auto threadId =
            workerThreadId_.load()) {
        PostThreadMessageW(
            threadId,
            WM_QUIT,
            0,
            0);
    }

    if (worker_.joinable()) {
        worker_.join();
    }
}

void EverythingIpcClient::QueryAsync(
    EverythingQueryRequest request,
    Completion completion) {
    latestGeneration_.store(
        request.generation);

    {
        std::scoped_lock lock(
            pendingMutex_);
        pending_ = PendingQuery{
            .request = std::move(request),
            .completion =
                std::move(completion),
        };
    }

    if (const auto hwnd =
            workerWindow_.load()) {
        PostMessageW(
            hwnd,
            kQueryQueuedMessage,
            0,
            0);
    }
}

bool EverythingIpcClient::IsAvailable()
    const noexcept {
    return FindWindowW(
        options_.everythingWindowClass
            .c_str(),
        nullptr) != nullptr;
}

EverythingIpcStatusSnapshot
EverythingIpcClient::Status() const {
    std::scoped_lock lock(statusMutex_);
    return status_;
}

LRESULT CALLBACK
EverythingIpcClient::WindowProc(
    HWND hwnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam) {
    EverythingIpcClient* self = nullptr;

    if (message == WM_NCCREATE) {
        const auto* create =
            reinterpret_cast<
                const CREATESTRUCTW*>(
                    lParam);
        self =
            static_cast<
                EverythingIpcClient*>(
                    create->lpCreateParams);
        SetWindowLongPtrW(
            hwnd,
            GWLP_USERDATA,
            reinterpret_cast<LONG_PTR>(
                self));
    } else {
        self =
            reinterpret_cast<
                EverythingIpcClient*>(
                GetWindowLongPtrW(
                    hwnd,
                    GWLP_USERDATA));
    }

    if (self) {
        return self->HandleWindowMessage(
            hwnd,
            message,
            wParam,
            lParam);
    }

    return DefWindowProcW(
        hwnd,
        message,
        wParam,
        lParam);
}

void EverythingIpcClient::WorkerMain(
    std::stop_token stopToken) {
    workerThreadId_.store(
        GetCurrentThreadId());

    MSG queueMessage{};
    PeekMessageW(
        &queueMessage,
        nullptr,
        WM_USER,
        WM_USER,
        PM_NOREMOVE);

    const auto instance =
        GetModuleHandleW(nullptr);
    WNDCLASSEXW windowClass{};
    windowClass.cbSize =
        sizeof(windowClass);
    windowClass.hInstance = instance;
    windowClass.lpfnWndProc = WindowProc;
    windowClass.lpszClassName =
        replyWindowClass_.c_str();

    if (!RegisterClassExW(
            &windowClass) &&
        GetLastError() !=
            ERROR_CLASS_ALREADY_EXISTS) {
        UpdateStatus(
            EverythingAvailability::
                Unavailable,
            EverythingQueryStatus::
                Unavailable,
            {},
            0);
        return;
    }

    const auto hwnd =
        CreateWindowExW(
            0,
            replyWindowClass_.c_str(),
            L"",
            0,
            0,
            0,
            0,
            0,
            HWND_MESSAGE,
            nullptr,
            instance,
            this);

    if (!hwnd) {
        UpdateStatus(
            EverythingAvailability::
                Unavailable,
            EverythingQueryStatus::
                Unavailable,
            {},
            0);
        return;
    }

    workerWindow_.store(hwnd);

    {
        std::scoped_lock lock(
            pendingMutex_);
        if (pending_) {
            PostMessageW(
                hwnd,
                kQueryQueuedMessage,
                0,
                0);
        }
    }

    MSG message{};
    while (!stopToken.stop_requested()) {
        const auto result =
            GetMessageW(
                &message,
                nullptr,
                0,
                0);
        if (result <= 0) {
            break;
        }

        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    workerWindow_.store(nullptr);
    if (IsWindow(hwnd)) {
        DestroyWindow(hwnd);
    }
    workerThreadId_.store(0);
}

LRESULT
EverythingIpcClient::HandleWindowMessage(
    HWND hwnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam) {
    switch (message) {
    case kQueryQueuedMessage:
        SchedulePendingQuery(hwnd);
        return 0;

    case WM_TIMER:
        if (wParam ==
            kDebounceTimerId) {
            KillTimer(
                hwnd,
                kDebounceTimerId);
            SendPendingQuery(hwnd);
            return 0;
        }
        if (wParam ==
            kReplyTimerId) {
            KillTimer(
                hwnd,
                kReplyTimerId);
            HandleReplyTimeout(hwnd);
            return 0;
        }
        break;

    case WM_COPYDATA: {
        const auto* copyData =
            reinterpret_cast<
                const COPYDATASTRUCT*>(
                    lParam);
        if (!copyData ||
            !copyData->lpData ||
            copyData->cbData == 0) {
            return FALSE;
        }

        HandleReply(
            hwnd,
            static_cast<std::uint32_t>(
                copyData->dwData),
            copyData->lpData,
            copyData->cbData);
        return TRUE;
    }

    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    default:
        break;
    }

    return DefWindowProcW(
        hwnd,
        message,
        wParam,
        lParam);
}

void
EverythingIpcClient::SchedulePendingQuery(
    HWND hwnd) {
    KillTimer(
        hwnd,
        kDebounceTimerId);
    SetTimer(
        hwnd,
        kDebounceTimerId,
        TimerDelay(options_.debounce),
        nullptr);
}

void
EverythingIpcClient::SendPendingQuery(
    HWND hwnd) {
    std::optional<PendingQuery>
        pending;

    {
        std::scoped_lock lock(
            pendingMutex_);
        if (pending_) {
            pending =
                std::move(pending_);
            pending_.reset();
        }
    }

    if (!pending ||
        pending->request.generation !=
            latestGeneration_.load()) {
        return;
    }

    const auto everythingWindow =
        FindWindowW(
            options_.everythingWindowClass
                .c_str(),
            nullptr);
    if (!everythingWindow) {
        CompletePending(
            std::move(*pending),
            EverythingQueryStatus::
                Unavailable,
            ERROR_FILE_NOT_FOUND);
        return;
    }

    if (inFlight_) {
        KillTimer(
            hwnd,
            kReplyTimerId);
        inFlight_.reset();
    }

    auto token =
        ++nextReplyToken_;
    if (token == 0U) {
        token =
            ++nextReplyToken_;
    }

    everything_ipc::
        Query2WireRequest wireRequest;
    wireRequest.replyHwnd =
        static_cast<std::uint32_t>(
            reinterpret_cast<ULONG_PTR>(
                hwnd));
    wireRequest.replyToken = token;
    wireRequest.maxResults =
        pending->request.limit;
    wireRequest.search =
        ToUtf16(
            pending->request.query);

    auto payload =
        everything_ipc::EncodeQuery2(
            wireRequest);

    inFlight_ = InFlightQuery{
        .generation =
            pending->request.generation,
        .replyToken = token,
        .completion =
            std::move(
                pending->completion),
        .started =
            std::chrono::
                steady_clock::now(),
    };

    COPYDATASTRUCT copyData{};
    copyData.dwData =
        everything_ipc::
            kCopyDataQuery2W;
    copyData.cbData =
        static_cast<DWORD>(
            payload.size());
    copyData.lpData =
        payload.data();

    DWORD_PTR messageResult = 0;
    SetLastError(ERROR_SUCCESS);
    const auto sendSucceeded =
        SendMessageTimeoutW(
            everythingWindow,
            WM_COPYDATA,
            reinterpret_cast<WPARAM>(
                hwnd),
            reinterpret_cast<LPARAM>(
                &copyData),
            SMTO_ABORTIFHUNG |
                SMTO_BLOCK,
            TimerDelay(
                options_.sendTimeout),
            &messageResult);

    if (!sendSucceeded) {
        const auto error =
            GetLastError();
        CompleteInFlight(
            error == ERROR_TIMEOUT
                ? EverythingQueryStatus::
                    SendTimeout
                : EverythingQueryStatus::
                    Unavailable,
            error);
        return;
    }

    if (messageResult == FALSE) {
        CompleteInFlight(
            EverythingQueryStatus::
                Unavailable,
            ERROR_NOT_SUPPORTED);
        return;
    }

    if (inFlight_ &&
        inFlight_->replyToken ==
            token) {
        SetTimer(
            hwnd,
            kReplyTimerId,
            TimerDelay(
                options_.replyTimeout),
            nullptr);
    }
}

void EverythingIpcClient::HandleReply(
    HWND hwnd,
    std::uint32_t replyToken,
    const void* data,
    std::size_t size) {
    if (!inFlight_ ||
        inFlight_->replyToken !=
            replyToken) {
        return;
    }

    KillTimer(
        hwnd,
        kReplyTimerId);

    if (inFlight_->generation !=
        latestGeneration_.load()) {
        inFlight_.reset();
        return;
    }

    const auto bytes =
        std::span(
            reinterpret_cast<
                const std::byte*>(
                    data),
            size);

    auto parsed =
        everything_ipc::ParseList2(
            bytes);
    if (!parsed) {
        CompleteInFlight(
            EverythingQueryStatus::
                ProtocolError,
            ERROR_INVALID_DATA);
        return;
    }

    const auto generation =
        inFlight_->generation;
    const auto started =
        inFlight_->started;
    auto completion =
        std::move(
            inFlight_->completion);
    inFlight_.reset();

    EverythingQueryResult result;
    result.generation = generation;
    result.status =
        EverythingQueryStatus::
            Success;
    result.totalMatches =
        parsed.value->totalItems;
    result.latency =
        std::chrono::duration_cast<
            std::chrono::microseconds>(
            std::chrono::
                steady_clock::now() -
            started);

    result.items.reserve(
        parsed.value->items.size());

    for (const auto& parsedItem :
         parsed.value->items) {
        EverythingIpcItem item;
        item.kind =
            parsedItem.folder
                ? EverythingItemKind::
                    Folder
                : EverythingItemKind::
                    File;
        item.name =
            ToWide(parsedItem.name);
        item.parentPath =
            ToWide(parsedItem.path);
        item.fullPath =
            ToWide(
                parsedItem.fullPath);

        if (item.fullPath.empty()) {
            item.fullPath =
                JoinPath(
                    item.parentPath,
                    item.name);
        }

        if (item.name.empty() &&
            !item.fullPath.empty()) {
            const auto separator =
                item.fullPath
                    .find_last_of(
                        L"\\/");
            item.name =
                separator ==
                    std::wstring::npos
                    ? item.fullPath
                    : item.fullPath.substr(
                        separator + 1U);

            if (item.name.empty()) {
                item.name =
                    item.fullPath;
            }
        }

        if (item.parentPath.empty() &&
            !item.fullPath.empty()) {
            const auto separator =
                item.fullPath
                    .find_last_of(
                        L"\\/");
            if (separator !=
                std::wstring::npos) {
                item.parentPath =
                    item.fullPath.substr(
                        0,
                        separator);
            }
        }

        result.items.push_back(
            std::move(item));
    }

    UpdateStatus(
        EverythingAvailability::
            Available,
        result.status,
        result.latency,
        static_cast<std::uint32_t>(
            result.items.size()));

    if (generation ==
            latestGeneration_.load() &&
        completion) {
        completion(
            std::move(result));
    }
}

void
EverythingIpcClient::HandleReplyTimeout(
    HWND) {
    if (inFlight_) {
        CompleteInFlight(
            EverythingQueryStatus::
                ReplyTimeout,
            ERROR_TIMEOUT);
    }
}

void
EverythingIpcClient::CompletePending(
    PendingQuery pending,
    EverythingQueryStatus status,
    std::uint32_t nativeError) {
    EverythingQueryResult result;
    result.generation =
        pending.request.generation;
    result.status = status;
    result.nativeError =
        nativeError;

    UpdateStatus(
        AvailabilityFor(status),
        status,
        result.latency,
        0);

    if (result.generation ==
            latestGeneration_.load() &&
        pending.completion) {
        pending.completion(
            std::move(result));
    }
}

void
EverythingIpcClient::CompleteInFlight(
    EverythingQueryStatus status,
    std::uint32_t nativeError) {
    if (!inFlight_) {
        return;
    }

    const auto generation =
        inFlight_->generation;
    auto completion =
        std::move(
            inFlight_->completion);
    const auto latency =
        std::chrono::duration_cast<
            std::chrono::microseconds>(
            std::chrono::
                steady_clock::now() -
            inFlight_->started);
    inFlight_.reset();

    if (generation !=
        latestGeneration_.load()) {
        return;
    }

    EverythingQueryResult result;
    result.generation = generation;
    result.status = status;
    result.latency = latency;
    result.nativeError =
        nativeError;

    UpdateStatus(
        AvailabilityFor(status),
        status,
        latency,
        0);

    if (completion) {
        completion(
            std::move(result));
    }
}

void EverythingIpcClient::UpdateStatus(
    EverythingAvailability availability,
    EverythingQueryStatus status,
    std::chrono::microseconds latency,
    std::uint32_t resultCount) {
    std::scoped_lock lock(
        statusMutex_);
    status_.availability =
        availability;
    status_.lastStatus = status;
    status_.lastLatency = latency;
    status_.lastResultCount =
        resultCount;
}

std::u16string
EverythingIpcClient::ToUtf16(
    std::wstring_view text) {
    static_assert(
        sizeof(wchar_t) ==
        sizeof(char16_t));

    std::u16string result;
    result.reserve(text.size());
    for (const auto ch : text) {
        result.push_back(
            static_cast<char16_t>(
                ch));
    }
    return result;
}

std::wstring
EverythingIpcClient::ToWide(
    std::u16string_view text) {
    static_assert(
        sizeof(wchar_t) ==
        sizeof(char16_t));

    std::wstring result;
    result.reserve(text.size());
    for (const auto ch : text) {
        result.push_back(
            static_cast<wchar_t>(
                ch));
    }
    return result;
}

std::wstring
EverythingIpcClient::JoinPath(
    std::wstring_view parent,
    std::wstring_view name) {
    if (parent.empty()) {
        return std::wstring(name);
    }
    if (name.empty()) {
        return std::wstring(parent);
    }

    std::wstring result(parent);
    const auto last =
        result.back();
    if (last != L'\\' &&
        last != L'/') {
        result.push_back(L'\\');
    }
    result.append(name);
    return result;
}

} // namespace altrun
