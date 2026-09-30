#pragma once

#include <windows.h>
#include <winhttp.h>

#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <stop_token>
#include <utility>
#include <vector>

namespace altrun::win::update_http {

// Private to the UpdateManager transport. All methods and all WinHTTP API calls
// belong to the downloading thread. Callbacks only publish state; they never
// close a handle or call back into UpdateManager/App/UI.
class Request {
    struct State {
        std::mutex mutex;
        std::condition_variable changed;
        DWORD expected{};
        DWORD error{};
        DWORD bytes{};
        bool completed{};
        bool cancelled{};
        bool closed{};
        std::vector<char> buffer;
    };

public:
    Request() = default;
    Request(const Request&) = delete;
    Request& operator=(const Request&) = delete;
    ~Request() { Close(); }

    // Takes ownership even if callback setup fails. No I/O has started yet.
    bool Attach(HINTERNET handle, std::uint32_t& error) {
        handle_ = handle;
        if (!handle_) {
            error = GetLastError();
            return false;
        }
        DWORD_PTR context = reinterpret_cast<DWORD_PTR>(state_.get());
        if (!WinHttpSetOption(handle_, WINHTTP_OPTION_CONTEXT_VALUE,
                &context, sizeof(context))) {
            error = GetLastError();
            return false;
        }
        if (WinHttpSetStatusCallback(handle_, Callback,
                WINHTTP_CALLBACK_FLAG_ALL_COMPLETIONS | WINHTTP_CALLBACK_FLAG_HANDLES,
                0) == WINHTTP_INVALID_STATUS_CALLBACK) {
            error = GetLastError();
            return false;
        }
        callbacksInstalled_ = true;
        return true;
    }

    bool Send(std::stop_token stop, std::uint32_t& error) {
        return Run(WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE, stop, error, [&] {
            return WinHttpSendRequest(handle_, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                WINHTTP_NO_REQUEST_DATA, 0, 0,
                reinterpret_cast<DWORD_PTR>(state_.get()));
        });
    }

    bool Receive(std::stop_token stop, std::uint32_t& error) {
        return Run(WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE, stop, error,
            [&] { return WinHttpReceiveResponse(handle_, nullptr); });
    }

    bool Read(DWORD bytes, DWORD& read, std::stop_token stop, std::uint32_t& error) {
        state_->buffer.resize(bytes);
        if (!Run(WINHTTP_CALLBACK_STATUS_READ_COMPLETE, stop, error, [&] {
                return WinHttpReadData(handle_, state_->buffer.data(), bytes, nullptr);
            })) return false;
        read = state_->bytes;
        return true;
    }

    const char* Data() const noexcept { return state_->buffer.data(); }

    // Sole close path: only the owning thread calls it, after the initiating
    // WinHTTP API has returned. Idempotent for failure/cancellation/destruction.
    void Close() noexcept {
        const HINTERNET handle = std::exchange(handle_, nullptr);
        if (!handle) return;
        WinHttpCloseHandle(handle);
        if (callbacksInstalled_) {
            std::unique_lock lock(state_->mutex);
            state_->changed.wait(lock, [&] { return state_->closed; });
        }
        // The heap context AND buffer remain owned until the final callback.
        // Connection/session owners outlive this Request's destructor.
    }

private:
    template<class Start>
    bool Run(DWORD expected, std::stop_token stop, std::uint32_t& error, Start start) {
        {
            std::scoped_lock lock(state_->mutex);
            state_->expected = expected;
            state_->completed = false;
            state_->error = 0;
            state_->bytes = 0;
        }
        std::stop_callback cancelled{stop, [state = state_] {
            std::scoped_lock lock(state->mutex);
            state->cancelled = true;
            state->changed.notify_all();
        }};
        if (stop.stop_requested() || !handle_) {
            error = stop.stop_requested() ? ERROR_CANCELLED : ERROR_INVALID_HANDLE;
            Close();
            return false;
        }
        // Do not hold the state mutex across WinHTTP calls: even an async
        // session can invoke its completion callback inline before returning.
        if (!start()) {
            error = stop.stop_requested() ? ERROR_CANCELLED : GetLastError();
            Close();
            return false;
        }
        {
            std::unique_lock lock(state_->mutex);
            state_->changed.wait(lock, [&] {
                return state_->completed || state_->cancelled;
            });
            // Cancellation observed at this completion boundary wins. A stop
            // after a completed operation is observed by the next operation.
            error = state_->cancelled ? ERROR_CANCELLED : state_->error;
        }
        if (error) {
            Close();
            return false;
        }
        return true;
    }

    static void CALLBACK Callback(HINTERNET, DWORD_PTR context, DWORD status,
        void* information, DWORD length) noexcept {
        if (!context) return;
        // Request owns this heap state until HANDLE_CLOSING is acknowledged.
        // No callback captures a Request/UpdateManager pointer or stack storage.
        auto& state = *reinterpret_cast<State*>(context);
        std::scoped_lock lock(state.mutex);
        if (status == WINHTTP_CALLBACK_STATUS_HANDLE_CLOSING) {
            state.closed = true;
        } else if (status == WINHTTP_CALLBACK_STATUS_REQUEST_ERROR) {
            state.error = static_cast<WINHTTP_ASYNC_RESULT*>(information)->dwError;
            state.completed = true;
        } else if (status == state.expected) {
            if (status == WINHTTP_CALLBACK_STATUS_READ_COMPLETE)
                state.bytes = length;
            state.completed = true;
        }
        // Notify under lock; after unlocking the callback never touches state
        // again, including on the final notification that releases the owner.
        state.changed.notify_all();
    }

    std::shared_ptr<State> state_{std::make_shared<State>()};
    HINTERNET handle_{};
    bool callbacksInstalled_{};
};

} // namespace altrun::win::update_http
