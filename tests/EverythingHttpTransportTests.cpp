#include <windows.h>
#include <winhttp.h>
#include <atomic>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <mutex>
#include <stop_token>
#include <thread>

using namespace std::chrono_literals;
namespace {
int sessionTag, connectionTag, requestTag;
const auto sessionHandle = static_cast<HINTERNET>(&sessionTag);
const auto connectionHandle = static_cast<HINTERNET>(&connectionTag);
const auto requestHandle = static_cast<HINTERNET>(&requestTag);
struct Fixture {
    std::mutex mutex, emitMutex;
    std::condition_variable changed;
    WINHTTP_STATUS_CALLBACK callback{};
    DWORD_PTR context{};
    DWORD blocked{}, failStart{}, asyncError{};
    bool failTimeout{}, failOption{}, failCallback{}, entered{}, closeEntered{}, releaseFinal{};
    bool delayedClose{}, finalSent{};
    unsigned sessionCloses{}, connectionCloses{}, requestCloses{}, finalCallbacks{}, activeCalls{};
    unsigned consumed{};
    char* readBuffer{};
    DWORD readSize{};
    std::thread::id owner;
    std::jthread late;
    void Wait(bool Fixture::*flag) {
        std::unique_lock lock(mutex);
        assert(changed.wait_for(lock, 3s, [&] { return this->*flag; }));
    }
    void Emit(DWORD status, void* info = nullptr, DWORD bytes = 0) {
        std::scoped_lock lock(emitMutex);
        if (finalSent || !callback) return;
        if (status == WINHTTP_CALLBACK_STATUS_HANDLE_CLOSING) {
            finalSent = true;
            ++finalCallbacks;
        }
        callback(requestHandle, context, status, info, bytes);
    }
};
Fixture* fixture;
struct ApiCall {
    ApiCall() { ++fixture->activeCalls; assert(fixture->owner == std::this_thread::get_id()); }
    ~ApiCall() { --fixture->activeCalls; }
};
BOOL Complete(DWORD status, void* info = nullptr, DWORD bytes = 0) {
    if (fixture->failStart == status) {
        SetLastError(ERROR_WINHTTP_CONNECTION_ERROR);
        return FALSE;
    }
    if (fixture->asyncError == status) {
        WINHTTP_ASYNC_RESULT error{0, ERROR_WINHTTP_TIMEOUT};
        fixture->Emit(WINHTTP_CALLBACK_STATUS_REQUEST_ERROR, &error, sizeof(error));
    } else if (fixture->blocked == status && !fixture->entered) {
        std::scoped_lock lock(fixture->mutex);
        fixture->entered = true;
        fixture->changed.notify_all();
    } else fixture->Emit(status, info, bytes);
    return TRUE;
}
HINTERNET WINAPI TestOpen(LPCWSTR, DWORD, LPCWSTR, LPCWSTR, DWORD flags) {
    assert(flags == WINHTTP_FLAG_ASYNC);
    fixture->owner = std::this_thread::get_id();
    return sessionHandle;
}
BOOL WINAPI TestTimeouts(HINTERNET h, int resolve, int connect, int send, int receive) {
    assert(h == sessionHandle && resolve == 5000 && connect == 5000 && send == 10000 && receive == 10000);
    if (fixture->failTimeout) { SetLastError(ERROR_INVALID_PARAMETER); return FALSE; }
    return TRUE;
}
HINTERNET WINAPI TestConnect(HINTERNET h, LPCWSTR, INTERNET_PORT, DWORD) {
    assert(h == sessionHandle); return connectionHandle;
}
HINTERNET WINAPI TestRequest(HINTERNET h, LPCWSTR, LPCWSTR, LPCWSTR, LPCWSTR, LPCWSTR*, DWORD flags) {
    assert(h == connectionHandle && flags == WINHTTP_FLAG_SECURE); return requestHandle;
}
BOOL WINAPI TestOption(HINTERNET h, DWORD option, LPVOID value, DWORD bytes) {
    assert(h == requestHandle && option == WINHTTP_OPTION_CONTEXT_VALUE && bytes == sizeof(DWORD_PTR));
    if (fixture->failOption) { SetLastError(ERROR_INVALID_PARAMETER); return FALSE; }
    fixture->context = *static_cast<DWORD_PTR*>(value); return TRUE;
}
WINHTTP_STATUS_CALLBACK WINAPI TestCallback(HINTERNET h, WINHTTP_STATUS_CALLBACK callback, DWORD flags, DWORD_PTR) {
    assert(h == requestHandle && (flags & WINHTTP_CALLBACK_FLAG_HANDLES));
    if (fixture->failCallback) { SetLastError(ERROR_INVALID_PARAMETER); return WINHTTP_INVALID_STATUS_CALLBACK; }
    fixture->callback = callback; return nullptr;
}
BOOL WINAPI TestSend(HINTERNET h, LPCWSTR, DWORD, LPVOID, DWORD, DWORD, DWORD_PTR context) {
    ApiCall guard; assert(h == requestHandle && context == fixture->context);
    return Complete(WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE);
}
BOOL WINAPI TestReceive(HINTERNET h, LPVOID) {
    ApiCall guard; assert(h == requestHandle);
    return Complete(WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE);
}
BOOL WINAPI TestHeaders(HINTERNET h, DWORD query, LPCWSTR, LPVOID value, LPDWORD, LPDWORD) {
    ApiCall guard; assert(h == requestHandle);
    *static_cast<DWORD*>(value) = (query & ~WINHTTP_QUERY_FLAG_NUMBER) == WINHTTP_QUERY_STATUS_CODE ? 200 : 4;
    return TRUE;
}
BOOL WINAPI TestAvailable(HINTERNET h, LPDWORD output) {
    ApiCall guard; assert(h == requestHandle && !output);
    DWORD bytes = fixture->consumed ? 0 : 4;
    return Complete(WINHTTP_CALLBACK_STATUS_DATA_AVAILABLE, &bytes, sizeof(bytes));
}
BOOL WINAPI TestRead(HINTERNET h, LPVOID buffer, DWORD bytes, LPDWORD output) {
    ApiCall guard; assert(h == requestHandle && !output && bytes == 4);
    fixture->readBuffer = static_cast<char*>(buffer); fixture->readSize = bytes;
    std::memcpy(buffer, "data", 4); fixture->consumed = 4;
    return Complete(WINHTTP_CALLBACK_STATUS_READ_COMPLETE, buffer, 4);
}
BOOL WINAPI TestClose(HINTERNET h) {
    assert(fixture->owner == std::this_thread::get_id() && fixture->activeCalls == 0);
    if (h == sessionHandle) { ++fixture->sessionCloses; return TRUE; }
    if (h == connectionHandle) {
        assert(!fixture->callback || fixture->finalSent);
        ++fixture->connectionCloses; return TRUE;
    }
    assert(h == requestHandle && ++fixture->requestCloses == 1);
    {
        std::scoped_lock lock(fixture->mutex);
        fixture->closeEntered = true; fixture->changed.notify_all();
    }
    if (fixture->delayedClose) {
        fixture->late = std::jthread([] {
            {
                std::unique_lock lock(fixture->mutex);
                fixture->changed.wait(lock, [] { return fixture->releaseFinal; });
            }
            // Emulate a late write/completion AFTER CloseHandle returned.
            if (fixture->readBuffer) {
                std::memcpy(fixture->readBuffer, "late", fixture->readSize);
                fixture->Emit(WINHTTP_CALLBACK_STATUS_READ_COMPLETE, fixture->readBuffer, fixture->readSize);
            }
            WINHTTP_ASYNC_RESULT error{0, ERROR_WINHTTP_OPERATION_CANCELLED};
            fixture->Emit(WINHTTP_CALLBACK_STATUS_REQUEST_ERROR, &error, sizeof(error));
            fixture->Emit(WINHTTP_CALLBACK_STATUS_HANDLE_CLOSING);
        });
    } else fixture->Emit(WINHTTP_CALLBACK_STATUS_HANDLE_CLOSING);
    return TRUE;
}
}
#define WinHttpOpen TestOpen
#define WinHttpSetTimeouts TestTimeouts
#define WinHttpConnect TestConnect
#define WinHttpOpenRequest TestRequest
#define WinHttpSetOption TestOption
#define WinHttpSetStatusCallback TestCallback
#define WinHttpSendRequest TestSend
#define WinHttpReceiveResponse TestReceive
#define WinHttpQueryHeaders TestHeaders
#define WinHttpQueryDataAvailable TestAvailable
#define WinHttpReadData TestRead
#define WinHttpCloseHandle TestClose
#include "../src/platform/EverythingBootstrapper.cpp"
#undef WinHttpCloseHandle
#undef WinHttpReadData
#undef WinHttpQueryDataAvailable
#undef WinHttpQueryHeaders
#undef WinHttpReceiveResponse
#undef WinHttpSendRequest
#undef WinHttpSetStatusCallback
#undef WinHttpSetOption
#undef WinHttpOpenRequest
#undef WinHttpConnect
#undef WinHttpSetTimeouts
#undef WinHttpOpen

namespace {
bool Download(bool file, std::stop_token token, std::uint32_t& error) {
    if (!file) {
        std::string text;
        bool ok = altrun::win::DownloadText(L"https://example.invalid/test", text, error, token);
        if (ok) assert(text == "data");
        return ok;
    }
    auto path = std::filesystem::temp_directory_path() /
        ("Asterun-transport-" + std::to_string(GetCurrentProcessId()));
    altrun::win::EverythingBootstrapSnapshot snapshot;
    bool ok = altrun::win::DownloadFile(L"https://example.invalid/test", path, snapshot, {}, error, token);
    if (ok) {
        std::ifstream input(path, std::ios::binary);
        std::string bytes{std::istreambuf_iterator<char>(input), {}};
        assert(bytes == "data" && snapshot.downloadedBytes == 4);
    }
    std::filesystem::remove(path);
    return ok;
}
void CheckClosed(const Fixture& f, bool request = true) {
    assert(f.sessionCloses == 1);
    if (request) assert(f.connectionCloses == 1 && f.requestCloses == 1);
    if (f.callback) assert(f.finalCallbacks == 1 && f.finalSent);
}
}
int main() {
    constexpr DWORD phases[]{WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE,
        WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE, WINHTTP_CALLBACK_STATUS_DATA_AVAILABLE,
        WINHTTP_CALLBACK_STATUS_READ_COMPLETE};
    for (bool file : {false, true}) {
        { // Inline callbacks, full output, then a stop AFTER request destruction.
            Fixture f; fixture = &f; std::stop_source stop; std::uint32_t error{};
            assert(Download(file, stop.get_token(), error) && error == 0);
            stop.request_stop(); CheckClosed(f);
        }
        for (DWORD phase : phases) {
            Fixture f; fixture = &f; f.blocked = phase; f.delayedClose = true;
            std::stop_source stop; std::promise<void> done; auto finished = done.get_future();
            std::jthread worker([&] {
                std::uint32_t error{};
                assert(!Download(file, stop.get_token(), error) && error == ERROR_CANCELLED);
                done.set_value();
            });
            f.Wait(&Fixture::entered);
            stop.request_stop(); f.Wait(&Fixture::closeEntered);
            // The context/buffer cannot be freed just because CloseHandle returned.
            assert(finished.wait_for(30ms) == std::future_status::timeout);
            { std::scoped_lock lock(f.mutex); f.releaseFinal = true; f.changed.notify_all(); }
            assert(finished.wait_for(2s) == std::future_status::ready);
            worker.join(); f.late.join(); CheckClosed(f);
        }
        for (DWORD phase : phases) for (bool asynchronous : {false, true}) {
            Fixture f; fixture = &f;
            (asynchronous ? f.asyncError : f.failStart) = phase;
            std::uint32_t error{};
            assert(!Download(file, {}, error));
            assert(error == (asynchronous ? ERROR_WINHTTP_TIMEOUT : ERROR_WINHTTP_CONNECTION_ERROR));
            CheckClosed(f);
        }
    }
    for (int setup = 0; setup < 3; ++setup) {
        Fixture f; fixture = &f;
        f.failTimeout = setup == 0; f.failOption = setup == 1; f.failCallback = setup == 2;
        std::uint32_t error{};
        assert(!Download(false, {}, error) && error == ERROR_INVALID_PARAMETER);
        CheckClosed(f, setup != 0);
        if (setup == 0) assert(f.requestCloses == 0 && f.connectionCloses == 0);
    }
    { // Already-stopped requests never start network work.
        Fixture f; fixture = &f; std::stop_source stop; stop.request_stop();
        std::uint32_t error{};
        assert(!Download(false, stop.get_token(), error) && error == ERROR_CANCELLED);
        assert(f.sessionCloses == 0 && f.requestCloses == 0);
    }
    for (int cycle = 0; cycle < 64; ++cycle) {
        Fixture f; fixture = &f; f.blocked = phases[cycle % 4];
        std::stop_source stop;
        std::jthread worker([&] {
            std::uint32_t error{};
            const bool ok = Download(false, stop.get_token(), error);
            assert(ok || error == ERROR_CANCELLED);
        });
        f.Wait(&Fixture::entered);
        std::jthread completion([&] {
            DWORD available = 4;
            if (f.blocked == WINHTTP_CALLBACK_STATUS_DATA_AVAILABLE)
                f.Emit(f.blocked, &available, sizeof(available));
            else if (f.blocked == WINHTTP_CALLBACK_STATUS_READ_COMPLETE)
                f.Emit(f.blocked, f.readBuffer, f.readSize);
            else f.Emit(f.blocked);
        });
        stop.request_stop(); completion.join(); worker.join(); CheckClosed(f);
    }
    { // jthread destructor models the application's request_stop + join exit.
        Fixture f; fixture = &f; f.blocked = WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE;
        auto worker = std::make_unique<std::jthread>([](std::stop_token token) {
            std::uint32_t error{};
            assert(!Download(true, token, error) && error == ERROR_CANCELLED);
        });
        f.Wait(&Fixture::entered);
        const auto started = std::chrono::steady_clock::now();
        worker.reset();
        assert(std::chrono::steady_clock::now() - started < 2s);
        CheckClosed(f);
    }
    std::cout << "Everything transport: cancellation, late callbacks, setup errors and 64 completion races passed\n";
}
