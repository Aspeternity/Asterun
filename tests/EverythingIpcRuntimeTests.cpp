#include "core/EverythingIpcProtocol.hpp"
#include "core/EverythingProvider.hpp"
#include "platform/EverythingIpcClient.hpp"

#include <windows.h>

#include <cassert>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>
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

struct FakeReplyItem {
    std::uint32_t flags{0};
    std::u16string name;
    std::u16string path;
    std::u16string fullPath;
};

void WriteU32At(
    std::vector<std::byte>& bytes,
    std::size_t offset,
    std::uint32_t value) {
    assert(offset + 4U <= bytes.size());
    bytes[offset + 0] =
        static_cast<std::byte>(
            value & 0xffU);
    bytes[offset + 1] =
        static_cast<std::byte>(
            (value >> 8U) & 0xffU);
    bytes[offset + 2] =
        static_cast<std::byte>(
            (value >> 16U) & 0xffU);
    bytes[offset + 3] =
        static_cast<std::byte>(
            (value >> 24U) & 0xffU);
}

std::vector<std::byte> BuildReplyItems(
    const Query2WireRequest& query,
    const std::vector<FakeReplyItem>& items,
    std::uint32_t totalItems = 0) {
    std::vector<std::byte> bytes;

    const auto count =
        static_cast<std::uint32_t>(
            items.size());
    if (totalItems == 0) {
        totalItems = count;
    }

    AppendU32(bytes, totalItems);
    AppendU32(bytes, count);
    AppendU32(bytes, 0);
    AppendU32(
        bytes,
        query.requestFlags);
    AppendU32(
        bytes,
        kSortNameAscending);

    const auto tableOffset =
        bytes.size();

    for (const auto& item : items) {
        AppendU32(
            bytes,
            item.flags);
        AppendU32(bytes, 0);
    }

    for (std::size_t i = 0;
         i < items.size();
         ++i) {
        const auto dataOffset =
            static_cast<std::uint32_t>(
                bytes.size());

        WriteU32At(
            bytes,
            tableOffset +
                i * sizeof(Item2Header) +
                4U,
            dataOffset);

        const auto& item = items[i];

        if ((query.requestFlags &
             kRequestName) != 0U) {
            AppendUtf16Field(
                bytes,
                item.name);
        }
        if ((query.requestFlags &
             kRequestPath) != 0U) {
            AppendUtf16Field(
                bytes,
                item.path);
        }
        if ((query.requestFlags &
             kRequestFullPathAndName) !=
            0U) {
            AppendUtf16Field(
                bytes,
                item.fullPath);
        }
    }

    return bytes;
}

std::vector<std::byte> BuildReply(
    const Query2WireRequest& query) {
    if (query.search == uR"(nopath:regex:"^[\s_-]*v[\s_-]*2")") {
        return BuildReplyItems(query, {{.name = u"v2rayN.exe", .path = u"D:\\Portable",
            .fullPath = u"D:\\Portable\\v2rayN.exe"}});
    }
    if (query.search.starts_with(u"nopath:regex:")) {
        // The probe fixture supports a confirmed empty set and a truncated unknown set.
        if (query.search.find(u"[\\s_-]*x[\\s_-]*9") != std::u16string::npos)
            return BuildReplyItems(query, {}, 9000);
        return BuildReplyItems(query, {});
    }
    if (query.search == u"folder") {
        return BuildReplyItems(
            query,
            {{
                .flags = kItemFolder,
                .name = u"Folder",
                .path = u"C:\\Fake",
                .fullPath =
                    u"C:\\Fake\\Folder",
            }});
    }

    if (query.search == u"drive-root") {
        return BuildReplyItems(
            query,
            {{
                .flags =
                    kItemFolder |
                    kItemDriveOrRoot,
                .name = u"C:",
                .path = u"",
                .fullPath = u"C:\\",
            }});
    }

    if (query.search == u"unc") {
        return BuildReplyItems(
            query,
            {{
                .name = u"报告.txt",
                .path =
                    u"\\\\server\\share\\资料",
                .fullPath =
                    u"\\\\server\\share\\资料\\报告.txt",
            }});
    }

    if (query.search == u"longpath") {
        std::u16string path =
            u"\\\\?\\C:\\";
        path.append(
            320,
            u'a');
        const std::u16string name =
            u"deep-result.txt";
        return BuildReplyItems(
            query,
            {{
                .name = name,
                .path = path,
                .fullPath =
                    path + u"\\" + name,
            }});
    }

    if (query.search == u"v2") {
        const auto count =
            std::min<std::uint32_t>(
                query.maxResults,
                240U);
        std::vector<FakeReplyItem>
            items;
        items.reserve(count);

        for (std::uint32_t i = 0;
             i < count;
             ++i) {
            FakeReplyItem item;

            if (i == 2000U) { // outside even the bounded 1000-row broad pool
                item.name =
                    u"v2rayN.exe";
                item.path =
                    u"D:\\v2rayN-windows-64";
                item.fullPath =
                    u"D:\\v2rayN-windows-64\\v2rayN.exe";
            } else {
                const auto suffix =
                    std::to_wstring(i);
                std::u16string number;
                number.reserve(
                    suffix.size());

                for (const wchar_t ch :
                     suffix) {
                    number.push_back(
                        static_cast<
                            char16_t>(ch));
                }

                item.name =
                    u"noise-v2-" +
                    number +
                    u".txt";
                item.path =
                    u"C:\\Noise";
                item.fullPath =
                    item.path +
                    u"\\" +
                    item.name;
            }

            items.push_back(
                std::move(item));
        }

        return BuildReplyItems(
            query,
            items,
            10000U);
    }

    if (query.search == u"many") {
        const auto count =
            std::min<std::uint32_t>(
                query.maxResults,
                256U);
        std::vector<FakeReplyItem>
            items;
        items.reserve(count);

        for (std::uint32_t i = 0;
             i < count;
             ++i) {
            const auto suffix =
                std::to_wstring(i);
            std::u16string number;
            number.reserve(
                suffix.size());
            for (const wchar_t ch :
                 suffix) {
                number.push_back(
                    static_cast<
                        char16_t>(ch));
            }

            FakeReplyItem item;
            item.name =
                u"item-" + number +
                u".txt";
            item.path =
                u"C:\\Many";
            item.fullPath =
                item.path +
                u"\\" +
                item.name;
            items.push_back(
                std::move(item));
        }

        return BuildReplyItems(
            query,
            items,
            500000U);
    }

    if (query.search == u"overlimit") {
        const auto count =
            query.maxResults + 1U;
        std::vector<FakeReplyItem>
            items;
        items.reserve(count);

        for (std::uint32_t i = 0;
             i < count;
             ++i) {
            FakeReplyItem item;
            item.name = u"x.txt";
            item.path = u"C:\\Overflow";
            item.fullPath =
                u"C:\\Overflow\\x.txt";
            items.push_back(
                std::move(item));
        }

        return BuildReplyItems(
            query,
            items);
    }

    const auto name =
        query.search + u".txt";
    return BuildReplyItems(
        query,
        {{
            .name = name,
            .path = u"C:\\Fake",
            .fullPath =
                u"C:\\Fake\\" + name,
        }});
}

class FakeEverythingServer {
public:
    enum class Mode {
        Immediate,
        HoldFirstUntilSecond,
        NoReply,
        WrongSender,
    };

    explicit FakeEverythingServer(
        Mode mode,
        std::wstring windowClass = {})
        : mode_(mode),
          windowClass_(
              windowClass.empty()
                  ? L"ALTRunNext.TestEverythingIpc." +
                        std::to_wstring(
                            GetCurrentProcessId()) +
                        L"." +
                        std::to_wstring(
                            reinterpret_cast<
                                ULONG_PTR>(this))
                  : std::move(windowClass)) {
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

    [[nodiscard]]
    std::uint32_t LastMaxResults()
        const {
        std::scoped_lock lock(
            mutex_);
        return received_.empty()
            ? 0U
            : received_.back()
                .query.maxResults;
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
                    Mode::Immediate ||
                mode_ ==
                    Mode::WrongSender) {
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
                mode_ ==
                        Mode::WrongSender
                    ? GetDesktopWindow()
                    : window_),
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
    options.discoverNamedInstances =
        false;
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

        const auto status =
            client.Status();
        assert(status.hasQuery);
        assert(
            status.availability ==
            EverythingAvailability::
                Unavailable);
        assert(
            status.lastStatus ==
            EverythingQueryStatus::
                Unavailable);
        assert(
            status.lastNativeError ==
            ERROR_FILE_NOT_FOUND);
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
        assert(status.hasQuery);
        assert(
            status.lastResultCount ==
            1);
        assert(
            status.lastTotalMatches ==
            1);
        assert(
            status.lastNativeError ==
            0);
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

        const auto status =
            client.Status();
        assert(status.hasQuery);
        assert(
            status.lastStatus ==
            EverythingQueryStatus::
                ReplyTimeout);
        assert(
            status.lastNativeError ==
            ERROR_TIMEOUT);
    }

    {
        const std::wstring windowClass =
            L"ALTRunNext.TestEverythingIpc.Recovery." +
            std::to_wstring(
                GetCurrentProcessId());

        EverythingIpcClient client(
            OptionsFor(windowClass));
        ResultCollector collector;

        client.QueryAsync(
            {
                .generation = 351,
                .query = L"offline",
                .limit = 8,
            },
            collector.Callback());

        assert(collector.WaitFor(1));
        assert(
            collector.Snapshot()[0].status ==
            EverythingQueryStatus::
                Unavailable);

        {
            FakeEverythingServer server(
                FakeEverythingServer::
                    Mode::Immediate,
                windowClass);

            client.QueryAsync(
                {
                    .generation = 352,
                    .query = L"recovered",
                    .limit = 8,
                },
                collector.Callback());

            assert(collector.WaitFor(2));
            const auto results =
                collector.Snapshot();

            assert(
                results.back().status ==
                EverythingQueryStatus::
                    Success);
            assert(
                results.back().items[0].name ==
                L"recovered.txt");

            const auto status =
                client.Status();
            assert(status.hasQuery);
            assert(
                status.availability ==
                EverythingAvailability::
                    Available);
            assert(
                status.lastStatus ==
                EverythingQueryStatus::
                    Success);
        }

        assert(!client.IsAvailable());
    }

    {
        const std::wstring baseClass =
            L"ALTRunNext.TestEverythingIpc.Named." +
            std::to_wstring(
                GetCurrentProcessId());
        const std::wstring namedClass =
            baseClass +
            L"_(1.5b)";

        FakeEverythingServer server(
            FakeEverythingServer::
                Mode::Immediate,
            namedClass);

        auto options =
            OptionsFor(baseClass);
        options.discoverNamedInstances =
            true;

        EverythingIpcClient client(
            options);
        ResultCollector collector;

        assert(client.IsAvailable());

        auto status =
            client.Status();
        assert(
            status.availability ==
            EverythingAvailability::
                Available);
        assert(
            status.namedInstanceFallback);
        assert(
            !status.ambiguousNamedInstances);
        assert(
            status.matchingWindowCount ==
            1);
        assert(
            status.ipcWindowClass ==
            namedClass);

        client.QueryAsync(
            {
                .generation = 360,
                .query = L"named",
                .limit = 8,
            },
            collector.Callback());

        assert(collector.WaitFor(1));
        assert(
            collector.Snapshot()
                .back()
                .status ==
            EverythingQueryStatus::
                Success);
    }

    {
        const std::wstring baseClass =
            L"ALTRunNext.TestEverythingIpc.Ambiguous." +
            std::to_wstring(
                GetCurrentProcessId());

        FakeEverythingServer first(
            FakeEverythingServer::
                Mode::Immediate,
            baseClass + L"_(one)");
        FakeEverythingServer second(
            FakeEverythingServer::
                Mode::Immediate,
            baseClass + L"_(two)");

        auto options =
            OptionsFor(baseClass);
        options.discoverNamedInstances =
            true;

        EverythingIpcClient client(
            options);
        ResultCollector collector;

        assert(!client.IsAvailable());

        const auto status =
            client.Status();
        assert(
            status.availability ==
            EverythingAvailability::
                Unavailable);
        assert(
            status.ambiguousNamedInstances);
        assert(
            !status.namedInstanceFallback);
        assert(
            status.matchingWindowCount ==
            2);

        client.QueryAsync(
            {
                .generation = 361,
                .query = L"ambiguous",
                .limit = 8,
            },
            collector.Callback());

        assert(collector.WaitFor(1));
        const auto result =
            collector.Snapshot().back();
        assert(
            result.status ==
            EverythingQueryStatus::
                Unavailable);
        assert(
            result.nativeError ==
            ERROR_MORE_DATA);
        assert(
            first.ReceivedCount() ==
            0);
        assert(
            second.ReceivedCount() ==
            0);
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
                .generation = 500,
                .query = L"drive-root",
                .limit = 8,
            },
            collector.Callback());
        assert(collector.WaitFor(1));

        auto results =
            collector.Snapshot();
        assert(
            results.back().items.size() ==
            1);
        const auto root =
            results.back().items.front();
        assert(root.root);
        assert(
            root.kind ==
            EverythingItemKind::Folder);
        assert(root.name == L"C:\\");
        assert(root.parentPath.empty());
        assert(root.fullPath == L"C:\\");

        client.QueryAsync(
            {
                .generation = 501,
                .query = L"unc",
                .limit = 8,
            },
            collector.Callback());
        assert(collector.WaitFor(2));

        results = collector.Snapshot();
        const auto unc =
            results.back().items.front();
        assert(
            unc.parentPath ==
            L"\\\\server\\share\\资料");
        assert(
            unc.fullPath ==
            L"\\\\server\\share\\资料\\报告.txt");

        client.QueryAsync(
            {
                .generation = 502,
                .query = L"longpath",
                .limit = 8,
            },
            collector.Callback());
        assert(collector.WaitFor(3));

        results = collector.Snapshot();
        const auto longPath =
            results.back().items.front();
        assert(
            longPath.fullPath.size() >
            320);
        assert(
            longPath.fullPath.rfind(
                L"\\\\?\\C:\\",
                0) == 0);
        assert(
            longPath.name ==
            L"deep-result.txt");
    }

    {
        FakeEverythingServer server(
            FakeEverythingServer::
                Mode::Immediate);
        auto options =
            OptionsFor(
                server.WindowClass());
        options.debounce = 60ms;

        EverythingIpcClient client(
            options);
        ResultCollector collector;

        for (std::uint64_t i = 0;
             i < 128;
             ++i) {
            client.QueryAsync(
                {
                    .generation =
                        600 + i,
                    .query =
                        L"burst-" +
                        std::to_wstring(i),
                    .limit = 8,
                },
                collector.Callback());
        }

        assert(collector.WaitFor(1));
        assert(
            server.ReceivedCount() ==
            1);
        assert(
            server.LastSearch() ==
            u"burst-127");

        const auto results =
            collector.Snapshot();
        assert(results.size() == 1);
        assert(
            results.front().generation ==
            727);
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
                .generation = 800,
                .query = L"many",
                .limit = 256,
            },
            collector.Callback());

        assert(collector.WaitFor(1));
        const auto result =
            collector.Snapshot().back();
        assert(
            result.status ==
            EverythingQueryStatus::
                Success);
        assert(
            result.items.size() ==
            256);
        assert(
            result.totalMatches ==
            500000);
        assert(
            result.items.back()
                .name ==
            L"item-255.txt");
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
                .generation = 810,
                .query = L"overlimit",
                .limit = 8,
            },
            collector.Callback());

        assert(collector.WaitFor(1));
        const auto result =
            collector.Snapshot().back();
        assert(
            result.status ==
            EverythingQueryStatus::
                ProtocolError);
        assert(
            result.nativeError ==
            ERROR_INVALID_DATA);
    }

    {
        FakeEverythingServer server(
            FakeEverythingServer::
                Mode::Immediate);
        auto options =
            OptionsFor(
                server.WindowClass());
        options.maxReplyBytes = 64;

        EverythingIpcClient client(
            options);
        ResultCollector collector;

        client.QueryAsync(
            {
                .generation = 811,
                .query = L"oversized",
                .limit = 8,
            },
            collector.Callback());

        assert(collector.WaitFor(1));
        const auto result =
            collector.Snapshot().back();
        assert(
            result.status ==
            EverythingQueryStatus::
                ProtocolError);
        assert(
            result.nativeError ==
            ERROR_INSUFFICIENT_BUFFER);
    }

    {
        FakeEverythingServer server(
            FakeEverythingServer::
                Mode::WrongSender);
        auto options =
            OptionsFor(
                server.WindowClass());
        options.replyTimeout = 80ms;

        EverythingIpcClient client(
            options);
        ResultCollector collector;

        client.QueryAsync(
            {
                .generation = 812,
                .query = L"spoofed",
                .limit = 8,
            },
            collector.Callback());

        assert(collector.WaitFor(1));
        const auto result =
            collector.Snapshot().back();
        assert(
            result.status ==
            EverythingQueryStatus::
                ReplyTimeout);
        assert(
            result.nativeError ==
            ERROR_TIMEOUT);
    }

    {
        FakeEverythingServer server(
            FakeEverythingServer::
                Mode::Immediate);
        EverythingProvider provider(
            OptionsFor(
                server.WindowClass()));

        std::mutex mutex;
        std::condition_variable cv;
        std::optional<
            DynamicQueryResponse>
            response;

        provider.QueryAsync(
            {
                .generation = 840,
                .query = L"v2",
                .limit = 30,
            },
            [&](DynamicQueryResponse value) {
                {
                    std::scoped_lock lock(
                        mutex);
                    response =
                        std::move(value);
                }
                cv.notify_all();
            });

        {
            std::unique_lock lock(
                mutex);
            assert(
                cv.wait_for(
                    lock,
                    2s,
                    [&] {
                        return response
                            .has_value();
                    }));
        }

        assert(response);
        assert(
            server.LastMaxResults() ==
            240);
        assert(
            response->status ==
            DynamicQueryStatus::Success);
        assert(
            response->totalMatches ==
            10000);
        assert(
            response->results.size() ==
            1);
        assert(
            response->results.front()
                .title ==
            L"v2rayN.exe");
    }

    {
        FakeEverythingServer server(
            FakeEverythingServer::
                Mode::Immediate);
        EverythingProvider provider(
            OptionsFor(
                server.WindowClass()));

        std::mutex mutex;
        std::condition_variable cv;
        std::optional<
            DynamicQueryResponse>
            response;

        provider.QueryAsync(
            {
                .generation = 850,
                .query = L"many",
                .limit = 5000,
            },
            [&](DynamicQueryResponse value) {
                {
                    std::scoped_lock lock(
                        mutex);
                    response =
                        std::move(value);
                }
                cv.notify_all();
            });

        {
            std::unique_lock lock(
                mutex);
            assert(
                cv.wait_for(
                    lock,
                    2s,
                    [&] {
                        return response
                            .has_value();
                    }));
        }

        assert(response);
        assert(
            server.LastMaxResults() ==
            1000);
        assert(
            response->status ==
            DynamicQueryStatus::Success);
        assert(
            response->results.size() ==
            256);
        assert(
            response->totalMatches ==
            500000);
    }

    {
        FakeEverythingServer server(
            FakeEverythingServer::
                Mode::Immediate);
        EverythingProvider provider(
            OptionsFor(
                server.WindowClass()));

        std::mutex mutex;
        std::condition_variable cv;
        std::optional<
            DynamicQueryResponse>
            response;

        provider.QueryAsync(
            {
                .generation = 401,
                .query = L"报告",
                .limit = 8,
            },
            [&](DynamicQueryResponse value) {
                {
                    std::scoped_lock lock(
                        mutex);
                    response =
                        std::move(value);
                }
                cv.notify_all();
            });

        {
            std::unique_lock lock(
                mutex);
            const bool completed =
                cv.wait_for(
                    lock,
                    2s,
                    [&] {
                        return response
                            .has_value();
                    });
            assert(completed);
        }

        assert(response);
        assert(
            response->generation ==
            401);
        assert(
            response->providerId ==
            "everything.filesystem");
        assert(
            response->status ==
            DynamicQueryStatus::Success);
        assert(
            response->results.size() ==
            1);

        const auto& result =
            response->results.front();

        assert(
            result.kind ==
            ResultKind::File);
        assert(
            result.title ==
            L"报告.txt");
        assert(
            result.subtitle ==
            L"C:\\Fake");
        assert(
            result.target ==
            L"C:\\Fake\\报告.txt");
        assert(
            result.action.kind ==
            LauncherActionKind::
                OpenFile);
        assert(result.score > 0);
    }

    {
        FakeEverythingServer server(
            FakeEverythingServer::
                Mode::Immediate);
        EverythingProvider provider(
            OptionsFor(
                server.WindowClass()));

        std::mutex mutex;
        std::condition_variable cv;
        std::optional<
            DynamicQueryResponse>
            response;

        provider.QueryAsync(
            {
                .generation = 402,
                .query = L"folder",
                .limit = 8,
            },
            [&](DynamicQueryResponse value) {
                {
                    std::scoped_lock lock(
                        mutex);
                    response =
                        std::move(value);
                }
                cv.notify_all();
            });

        {
            std::unique_lock lock(
                mutex);
            assert(
                cv.wait_for(
                    lock,
                    2s,
                    [&] {
                        return response
                            .has_value();
                    }));
        }

        assert(response);
        assert(
            response->results.size() ==
            1);

        const auto& result =
            response->results.front();

        assert(
            result.kind ==
            ResultKind::Folder);
        assert(
            result.title ==
            L"Folder");
        assert(
            result.target ==
            L"C:\\Fake\\Folder");
        assert(
            result.action.kind ==
            LauncherActionKind::
                OpenFolder);
        assert(result.score > 0);
    }

    {
        // Independent endpoints: probing v2 must not cancel a visible folder query.
        FakeEverythingServer server(FakeEverythingServer::Mode::Immediate);
        EverythingProvider provider(OptionsFor(server.WindowClass()));
        std::mutex mutex;
        std::condition_variable cv;
        bool visible = false;
        int probes = 0;
        provider.QueryAsync({900, L"folder", 8}, [&](DynamicQueryResponse value) {
            std::scoped_lock lock(mutex);
            visible = value.generation == 900 && value.results.size() == 1;
            cv.notify_all();
        });
        using classic_behavior::ContinuationEvidence;
        for (const auto& item : std::vector<std::pair<std::wstring, ContinuationEvidence>>{
                {L"v2", ContinuationEvidence::Present},
                {L"zzz9", ContinuationEvidence::Absent},
                {L"x9", ContinuationEvidence::Unknown},
                {L"ext:exe", ContinuationEvidence::Unknown}}) {
            const int expected = probes + 1;
            provider.ProbeContinuation(static_cast<std::uint64_t>(expected), item.first,
                [&, expected, evidence = item.second](std::uint64_t token, ContinuationEvidence result) {
                    std::scoped_lock lock(mutex);
                    assert(token == static_cast<std::uint64_t>(expected));
                    assert(result == evidence);
                    ++probes;
                    cv.notify_all();
                });
            std::unique_lock lock(mutex);
            assert(cv.wait_for(lock, 2s, [&] { return probes == expected; }));
        }
        std::unique_lock lock(mutex);
        assert(cv.wait_for(lock, 2s, [&] { return visible; }));
    }
    {
        FakeEverythingServer server(FakeEverythingServer::Mode::NoReply);
        EverythingProvider provider(OptionsFor(server.WindowClass()));
        std::mutex mutex;
        std::condition_variable cv;
        bool completed = false;
        provider.ProbeContinuation(1, L"v2", [&](std::uint64_t, classic_behavior::ContinuationEvidence evidence) {
            std::scoped_lock lock(mutex);
            assert(evidence == classic_behavior::ContinuationEvidence::Unknown);
            completed = true;
            cv.notify_all();
        });
        std::unique_lock lock(mutex);
        assert(cv.wait_for(lock, 2s, [&] { return completed; }));
    }

    return 0;
}
