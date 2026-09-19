#include "core/EverythingIpcProtocol.hpp"
#include "platform/EverythingIpcClient.hpp"

#include <windows.h>

#include <cassert>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <span>
#include <string>
#include <thread>
#include <vector>

using namespace std::chrono_literals;
using namespace altrun;
using namespace altrun::everything_ipc;

namespace {

void AppendU32(
    std::vector<std::byte>& bytes,
    std::uint32_t value) {
    bytes.push_back(
        static_cast<std::byte>(
            value & 0xffU));
    bytes.push_back(
        static_cast<std::byte>(
            (value >> 8U) & 0xffU));
    bytes.push_back(
        static_cast<std::byte>(
            (value >> 16U) & 0xffU));
    bytes.push_back(
        static_cast<std::byte>(
            (value >> 24U) & 0xffU));
}

void AppendUtf16Field(
    std::vector<std::byte>& bytes,
    std::u16string_view text) {
    AppendU32(
        bytes,
        static_cast<std::uint32_t>(
            text.size()));

    for (const auto ch : text) {
        const auto value =
            static_cast<std::uint16_t>(
                ch);
        bytes.push_back(
            static_cast<std::byte>(
                value & 0xffU));
        bytes.push_back(
            static_cast<std::byte>(
                (value >> 8U) &
                0xffU));
    }

    bytes.push_back(std::byte{0});
    bytes.push_back(std::byte{0});
}

std::vector<std::byte> BuildReply(
    const Query2WireRequest& query) {
    std::vector<std::byte> bytes;

    AppendU32(bytes, 1);
    AppendU32(bytes, 1);
    AppendU32(bytes, 0);
    AppendU32(
        bytes,
        query.requestFlags);
    AppendU32(
        bytes,
        kSortNameAscending);

    AppendU32(bytes, 0);
    const auto dataOffsetPosition =
        bytes.size();
    AppendU32(bytes, 0);

    const auto dataOffset =
        static_cast<std::uint32_t>(
            bytes.size());
    bytes[dataOffsetPosition + 0] =
        static_cast<std::byte>(
            dataOffset & 0xffU);
    bytes[dataOffsetPosition + 1] =
        static_cast<std::byte>(
            (dataOffset >> 8U) & 0xffU);
    bytes[dataOffsetPosition + 2] =
        static_cast<std::byte>(
            (dataOffset >> 16U) & 0xffU);
    bytes[dataOffsetPosition + 3] =
        static_cast<std::byte>(
            (dataOffset >> 24U) & 0xffU);

    const auto name =
        query.search + u".txt";
    const std::u16string path =
        u"C:\\Fake";
    const auto fullPath =
        path + u"\\" + name;

    if ((query.requestFlags &
         kRequestName) != 0U) {
        AppendUtf16Field(
            bytes,
            name);
    }
    if ((query.requestFlags &
         kRequestPath) != 0U) {
        AppendUtf16Field(
            bytes,
            path);
    }
    if ((query.requestFlags &
         kRequestFullPathAndName) !=
        0U) {
        AppendUtf16Field(
            bytes,
            fullPath);
    }

    return bytes;
}

class FakeEverythingServer {
public:
    enum class Mode {
        Immediate,
        HoldFirstUntilSecond,
        NoReply,
    };

    explicit FakeEverythingServer(
        Mode mode)
        : mode_(mode),
          windowClass_(
              L"ALTRunNext.TestEverythingIpc." +
              std::to_wstring(
                  GetCurrentProcessId()) +
              L"." +
              std::to_wstring(
                  reinterpret_cast<
                      ULONG_PTR>(this))) {
        thread_ = std::jthread(
            [this](std::stop_token) {
                Run();
            });

        std::unique_lock lock(
            mutex_);
        readyCv_.wait_for(
            lock,
            2s,
            [this] {
                return ready_;
            });
        assert(ready_);
        assert(window_ != nullptr);
    }

    ~FakeEverythingServer() {
        if (window_) {
            PostMessageW(
                window_,
                WM_CLOSE,
                0,
                0);
        }

        if (thread_.joinable()) {
            thread_.join();
        }
    }

    [[nodiscard]]
    const std::wstring&
    WindowClass() const noexcept {
        return windowClass_;
    }

    [[nodiscard]]
    std::size_t ReceivedCount() const {
        std::scoped_lock lock(
            mutex_);
        return received_.size();
    }

    [[nodiscard]]
    std::u16string LastSearch() const {
        std::scoped_lock lock(
            mutex_);
        return received_.empty()
            ? std::u16string{}
            : received_.back()
                .query.search;
    }

    bool WaitForReceived(
        std::size_t count) {
        std::unique_lock lock(
            mutex_);
        return receivedCv_.wait_for(
            lock,
            2s,
            [this, count] {
                return received_.size() >=
                    count;
            });
    }

private:
    struct ReceivedQuery {
        Query2WireRequest query;
    };

    static constexpr UINT
        kSendReplyMessage =
            WM_APP + 0x461;

    static LRESULT CALLBACK
    WindowProc(
        HWND hwnd,
        UINT message,
        WPARAM wParam,
        LPARAM lParam) {
        FakeEverythingServer* self =
            nullptr;

        if (message == WM_NCCREATE) {
            const auto* create =
                reinterpret_cast<
                    const CREATESTRUCTW*>(
                        lParam);
            self =
                static_cast<
                    FakeEverythingServer*>(
                        create->
                            lpCreateParams);
            SetWindowLongPtrW(
                hwnd,
                GWLP_USERDATA,
                reinterpret_cast<
                    LONG_PTR>(self));
        } else {
            self =
                reinterpret_cast<
                    FakeEverythingServer*>(
                    GetWindowLongPtrW(
                        hwnd,
                        GWLP_USERDATA));
        }

        return self
            ? self->Handle(
                hwnd,
                message,
                wParam,
                lParam)
            : DefWindowProcW(
                hwnd,
                message,
                wParam,
                lParam);
    }

    LRESULT Handle(
        HWND hwnd,
        UINT message,
        WPARAM wParam,
        LPARAM lParam) {
        if (message == WM_COPYDATA) {
            const auto* copyData =
                reinterpret_cast<
                    const COPYDATASTRUCT*>(
                        lParam);
            if (!copyData ||
                copyData->dwData !=
                    kCopyDataQuery2W ||
                !copyData->lpData) {
                return FALSE;
            }

            const auto parsed =
                ParseQuery2(
                    std::span(
                        reinterpret_cast<
                            const std::byte*>(
                            copyData->
                                lpData),
                        copyData->
                            cbData));
            if (!parsed ||
                parsed.value->
                    requestFlags !=
                    kDefaultRequestFlags) {
                return FALSE;
            }

            std::size_t index = 0;
            {
                std::scoped_lock lock(
                    mutex_);
                received_.push_back(
                    ReceivedQuery{
                        *parsed.value});
                index =
                    received_.size() -
                    1U;
            }
            receivedCv_.notify_all();

            if (mode_ ==
                Mode::Immediate) {
                PostMessageW(
                    hwnd,
                    kSendReplyMessage,
                    index,
                    0);
            } else if (
                mode_ ==
                    Mode::
                        HoldFirstUntilSecond &&
                index == 1U) {
                PostMessageW(
                    hwnd,
                    kSendReplyMessage,
                    1,
                    0);
                PostMessageW(
                    hwnd,
                    kSendReplyMessage,
                    0,
                    0);
            }

            return TRUE;
        }

        if (message == WM_CLOSE) {
            DestroyWindow(hwnd);
            return 0;
        }

        if (message == WM_DESTROY) {
            PostQuitMessage(0);
            return 0;
        }

        return DefWindowProcW(
            hwnd,
            message,
            wParam,
            lParam);
    }

    void SendReply(
        std::size_t index) {
        ReceivedQuery received;
        {
            std::scoped_lock lock(
                mutex_);
            assert(
                index <
                received_.size());
            received =
                received_[index];
        }

        auto payload =
            BuildReply(
                received.query);

        COPYDATASTRUCT copyData{};
        copyData.dwData =
            received.query.replyToken;
        copyData.cbData =
            static_cast<DWORD>(
                payload.size());
        copyData.lpData =
            payload.data();

        const auto replyWindow =
            reinterpret_cast<HWND>(
                static_cast<ULONG_PTR>(
                    received.query
                        .replyHwnd));

        SendMessageW(
            replyWindow,
            WM_COPYDATA,
            reinterpret_cast<WPARAM>(
                window_),
            reinterpret_cast<LPARAM>(
                &copyData));
    }

    void Run() {
        const auto instance =
            GetModuleHandleW(nullptr);

        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.hInstance = instance;
        wc.lpfnWndProc =
            WindowProc;
        wc.lpszClassName =
            windowClass_.c_str();

        const auto registered =
            RegisterClassExW(&wc);
        assert(
            registered != 0 ||
            GetLastError() ==
                ERROR_CLASS_ALREADY_EXISTS);

        window_ =
            CreateWindowExW(
                0,
                windowClass_.c_str(),
                L"",
                0,
                0,
                0,
                0,
                0,
                nullptr,
                nullptr,
                instance,
                this);

        {
            std::scoped_lock lock(
                mutex_);
            ready_ =
                window_ != nullptr;
        }
        readyCv_.notify_all();

        MSG message{};
        while (GetMessageW(
                   &message,
                   nullptr,
                   0,
                   0) > 0) {
            if (message.message ==
                kSendReplyMessage) {
                SendReply(
                    static_cast<
                        std::size_t>(
                        message.wParam));
                continue;
            }

            TranslateMessage(
                &message);
            DispatchMessageW(
                &message);
        }

        window_ = nullptr;
    }

    Mode mode_;
    std::wstring windowClass_;
    std::jthread thread_;
    HWND window_{nullptr};

    mutable std::mutex mutex_;
    std::condition_variable
        readyCv_;
    std::condition_variable
        receivedCv_;
    bool ready_{false};
    std::vector<ReceivedQuery>
        received_;
};

class ResultCollector {
public:
    EverythingIpcClient::Completion
    Callback() {
        return
            [this](
                EverythingQueryResult
                    result) {
                {
                    std::scoped_lock lock(
                        mutex_);
                    results_.push_back(
                        std::move(
                            result));
                }
                cv_.notify_all();
            };
    }

    bool WaitFor(
        std::size_t count,
        std::chrono::milliseconds
            timeout = 2s) {
        std::unique_lock lock(
            mutex_);
        return cv_.wait_for(
            lock,
            timeout,
            [this, count] {
                return results_.size() >=
                    count;
            });
    }

    [[nodiscard]]
    std::vector<
        EverythingQueryResult>
    Snapshot() const {
        std::scoped_lock lock(
            mutex_);
        return results_;
    }

private:
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::vector<
        EverythingQueryResult>
        results_;
};

EverythingIpcClientOptions
OptionsFor(
    const std::wstring& windowClass) {
    EverythingIpcClientOptions
        options;
    options.everythingWindowClass =
        windowClass;
    options.debounce = 10ms;
    options.sendTimeout = 200ms;
    options.replyTimeout = 250ms;
    return options;
}

} // namespace

int main() {
    {
        EverythingIpcClient client(
            OptionsFor(
                L"ALTRunNext."
                L"TestEverythingIpc."
                L"Missing"));
        ResultCollector collector;

        client.QueryAsync(
            {
                .generation = 1,
                .query = L"missing",
                .limit = 8,
            },
            collector.Callback());

        assert(
            collector.WaitFor(1));
        const auto results =
            collector.Snapshot();
        assert(
            results[0].status ==
            EverythingQueryStatus::
                Unavailable);
        assert(
            !client.IsAvailable());
    }

    {
        FakeEverythingServer server(
            FakeEverythingServer::
                Mode::Immediate);
        EverythingIpcClient client(
            OptionsFor(
                server.WindowClass()));
        ResultCollector collector;

        client.QueryAsync(
            {
                .generation = 2,
                .query = L"中文论文",
                .limit = 16,
            },
            collector.Callback());

        assert(
            collector.WaitFor(1));
        const auto results =
            collector.Snapshot();

        assert(results.size() == 1);
        assert(
            results[0].generation ==
            2);
        assert(
            results[0].status ==
            EverythingQueryStatus::
                Success);
        assert(
            results[0].items.size() ==
            1);
        assert(
            results[0].items[0].name ==
            L"中文论文.txt");
        assert(
            results[0].items[0]
                .parentPath ==
            L"C:\\Fake");
        assert(
            results[0].items[0]
                .fullPath ==
            L"C:\\Fake\\中文论文.txt");
        assert(client.IsAvailable());
        assert(
            server.LastSearch() ==
            u"中文论文");

        const auto status =
            client.Status();
        assert(
            status.availability ==
            EverythingAvailability::
                Available);
        assert(
            status.lastStatus ==
            EverythingQueryStatus::
                Success);
        assert(
            status.lastResultCount ==
            1);
    }

    {
        FakeEverythingServer server(
            FakeEverythingServer::
                Mode::
                    HoldFirstUntilSecond);
        auto options =
            OptionsFor(
                server.WindowClass());
        options.debounce = 5ms;
        options.replyTimeout = 500ms;

        EverythingIpcClient client(
            options);
        ResultCollector collector;

        client.QueryAsync(
            {
                .generation = 101,
                .query = L"doc",
                .limit = 8,
            },
            collector.Callback());

        assert(
            server.WaitForReceived(1));

        client.QueryAsync(
            {
                .generation = 102,
                .query = L"docker",
                .limit = 8,
            },
            collector.Callback());

        assert(
            collector.WaitFor(1));
        std::this_thread::sleep_for(
            80ms);

        const auto results =
            collector.Snapshot();
        assert(results.size() == 1);
        assert(
            results[0].generation ==
            102);
        assert(
            results[0].items[0].name ==
            L"docker.txt");
        assert(
            server.ReceivedCount() ==
            2);
    }

    {
        FakeEverythingServer server(
            FakeEverythingServer::
                Mode::Immediate);
        auto options =
            OptionsFor(
                server.WindowClass());
        options.debounce = 40ms;

        EverythingIpcClient client(
            options);
        ResultCollector collector;

        client.QueryAsync(
            {
                .generation = 201,
                .query = L"d",
                .limit = 8,
            },
            collector.Callback());
        client.QueryAsync(
            {
                .generation = 202,
                .query = L"do",
                .limit = 8,
            },
            collector.Callback());
        client.QueryAsync(
            {
                .generation = 203,
                .query = L"docker",
                .limit = 8,
            },
            collector.Callback());

        assert(
            collector.WaitFor(1));
        assert(
            server.ReceivedCount() ==
            1);
        assert(
            server.LastSearch() ==
            u"docker");

        const auto results =
            collector.Snapshot();
        assert(results.size() == 1);
        assert(
            results[0].generation ==
            203);
    }

    {
        FakeEverythingServer server(
            FakeEverythingServer::
                Mode::NoReply);
        auto options =
            OptionsFor(
                server.WindowClass());
        options.replyTimeout = 80ms;

        EverythingIpcClient client(
            options);
        ResultCollector collector;

        client.QueryAsync(
            {
                .generation = 301,
                .query = L"timeout",
                .limit = 8,
            },
            collector.Callback());

        assert(
            collector.WaitFor(1));
        const auto results =
            collector.Snapshot();
        assert(
            results[0].status ==
            EverythingQueryStatus::
                ReplyTimeout);
    }

    return 0;
}
