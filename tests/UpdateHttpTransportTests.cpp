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
#include <miniz.h>

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
    bool failRedirect{}, failTimeout{}, failOption{}, failCallback{}, entered{}, closeEntered{}, releaseFinal{};
    bool holdApi{}, releaseApi{};
    bool delayedClose{}, finalSent{};
    unsigned opens{}, redirects{};
    std::vector<DWORD> statuses{200};
    std::string payload{"data"};
    std::vector<std::wstring> objects;
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
        std::unique_lock lock(fixture->mutex);
        fixture->entered = true;
        fixture->changed.notify_all();
        if (fixture->holdApi)
            fixture->changed.wait(lock, [] { return fixture->releaseApi; });
    } else fixture->Emit(status, info, bytes);
    return TRUE;
}
HINTERNET WINAPI TestOpen(LPCWSTR, DWORD, LPCWSTR, LPCWSTR, DWORD flags) {
    assert(flags == WINHTTP_FLAG_ASYNC);
    fixture->owner = std::this_thread::get_id();
    return sessionHandle;
}
BOOL WINAPI TestTimeouts(HINTERNET h, int resolve, int connect, int send, int receive) {
    assert(h == sessionHandle && resolve == 5000 && connect == 5000 && send == 15000 && receive == 15000);
    if (fixture->failTimeout) { SetLastError(ERROR_INVALID_PARAMETER); return FALSE; }
    return TRUE;
}
HINTERNET WINAPI TestConnect(HINTERNET h, LPCWSTR, INTERNET_PORT, DWORD) {
    assert(h == sessionHandle); return connectionHandle;
}
HINTERNET WINAPI TestRequest(HINTERNET h, LPCWSTR, LPCWSTR object, LPCWSTR, LPCWSTR, LPCWSTR*, DWORD flags) {
    assert(h == connectionHandle && flags == WINHTTP_FLAG_SECURE);
    ++fixture->opens; fixture->consumed = 0; fixture->finalSent = false;
    fixture->callback = nullptr; fixture->context = 0;
    fixture->objects.emplace_back(object); return requestHandle;
}
BOOL WINAPI TestOption(HINTERNET h, DWORD option, LPVOID value, DWORD bytes) {
    assert(h == requestHandle);
    if (option == WINHTTP_OPTION_REDIRECT_POLICY) {
        assert(*static_cast<DWORD*>(value) == WINHTTP_OPTION_REDIRECT_POLICY_DISALLOW_HTTPS_TO_HTTP);
        ++fixture->redirects;
        if (fixture->failRedirect) { SetLastError(ERROR_INVALID_PARAMETER); return FALSE; }
        return TRUE;
    }
    assert(option == WINHTTP_OPTION_CONTEXT_VALUE && bytes == sizeof(DWORD_PTR));
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
    *static_cast<DWORD*>(value) = (query & ~WINHTTP_QUERY_FLAG_NUMBER) == WINHTTP_QUERY_STATUS_CODE ? fixture->statuses[std::min<std::size_t>(fixture->opens - 1, fixture->statuses.size() - 1)] : static_cast<DWORD>(fixture->payload.size());
    return TRUE;
}
BOOL WINAPI TestRead(HINTERNET h, LPVOID buffer, DWORD bytes, LPDWORD output) {
    ApiCall guard; assert(h == requestHandle && !output);
    assert(bytes == 16 * 1024 || bytes == 64 * 1024);
    const DWORD count = static_cast<DWORD>(std::min<std::size_t>(bytes, fixture->payload.size() - fixture->consumed));
    fixture->readBuffer = static_cast<char*>(buffer); fixture->readSize = count;
    std::memcpy(buffer, fixture->payload.data() + fixture->consumed, count);
    fixture->consumed += count;
    return Complete(WINHTTP_CALLBACK_STATUS_READ_COMPLETE, buffer, count);
}
BOOL WINAPI TestClose(HINTERNET h) {
    assert(fixture->owner == std::this_thread::get_id() && fixture->activeCalls == 0);
    if (h == sessionHandle) { ++fixture->sessionCloses; return TRUE; }
    if (h == connectionHandle) {
        assert(!fixture->callback || fixture->finalSent);
        ++fixture->connectionCloses; return TRUE;
    }
    assert(h == requestHandle && ++fixture->requestCloses == fixture->opens);
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
                std::memset(fixture->readBuffer, 'x', fixture->readSize);
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
#define WinHttpReadData TestRead
#define WinHttpCloseHandle TestClose
#include "../src/platform/UpdateManager.cpp"
#undef WinHttpCloseHandle
#undef WinHttpReadData
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
        ("Asterun-update-transport-" + std::to_string(GetCurrentProcessId()));
    altrun::win::UpdateSnapshot snapshot;
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
    assert(f.sessionCloses == (request ? f.opens : 1));
    if (request) assert(f.connectionCloses == f.opens && f.requestCloses == f.opens);
    if (f.callback) assert(f.finalCallbacks == f.opens && f.finalSent);
}
}
namespace {
void UpdateSemantics() {
    namespace fs = std::filesystem;
    using namespace altrun;
    using namespace altrun::win;
    const auto root = fs::temp_directory_path() / ("Asterun-update-http-" + std::to_string(GetCurrentProcessId()));
    fs::remove_all(root); fs::create_directories(root);
    const std::string hash(64, 'a');
    const nlohmann::json asset{{"name", "Asterun-x64.zip"}, {"sha256", hash}};
    const std::string manifestText = nlohmann::json{{"schemaVersion", 1}, {"version", "9.0.0"},
        {"commit", std::string(40, 'a')}, {"assets", {{"x64", asset}, {"ARM64", asset}}}}.dump();
    auto manifest = *ParseUpdateManifest(manifestText);
    { // Multiple fixed-size completions, including a short final chunk and EOF.
        Fixture f; fixture = &f; f.payload.assign(80 * 1024 + 7, 'x');
        std::string output; std::uint32_t error{};
        assert(DownloadText(L"https://example.invalid/manifest", output, error, {}));
        assert(output == f.payload); CheckClosed(f);
    }
    { // The manifest size limit remains enforced without retry.
        Fixture f; fixture = &f; f.payload.assign(256 * 1024 + 1, 'x');
        std::string output; std::uint32_t error{};
        assert(!DownloadTextWithRetry(L"https://example.invalid/manifest", output, error, {}));
        assert(error == ERROR_FILE_TOO_LARGE && f.opens == 1); CheckClosed(f);
    }
    for (auto channel : {UpdateChannel::Stable, UpdateChannel::Development}) {
        Fixture f; fixture = &f; f.payload = manifestText;
        for (int i = 0; i < 4; ++i) {
            auto result = CheckForUpdate(root, "1.0.1", channel, {}, {});
            assert(result.snapshot.stage == UpdateStage::Available && result.manifest);
            assert(result.manifest->version == "9.0.0");
        }
        assert(f.opens == 4 && f.redirects == 4); CheckClosed(f);
    }
    { // Existing retry budget: initial attempt + 250ms + 750ms, then success.
        Fixture f; fixture = &f; f.payload = manifestText; f.statuses = {503, 503, 200};
        auto result = CheckForUpdate(root, "1.0.1", UpdateChannel::Development, {}, {});
        assert(result.snapshot.stage == UpdateStage::Available && f.opens == 3); CheckClosed(f);
    }
    for (DWORD status : {403UL, 503UL}) {
        fs::remove_all(root / "update");
        Fixture f; fixture = &f; f.statuses = {status};
        auto result = CheckForUpdate(root, "1.0.1", UpdateChannel::Development, {}, {});
        assert(result.snapshot.failure == UpdateFailure::ManifestDownloadFailed);
        assert(result.snapshot.nativeError == status && f.opens == (status == 503 ? 3U : 1U));
        assert(!fs::exists(root / "update/update-state.json")); CheckClosed(f);
    }
    { // Stable-only 404 legacy release metadata fallback remains intact.
        Fixture f; fixture = &f; f.statuses = {404, 200}; f.payload = R"({"tag_name":"v1.0.1"})";
        auto result = CheckForUpdate(root, "1.0.1", UpdateChannel::Stable, {}, {});
        assert(result.snapshot.stage == UpdateStage::UpToDate && f.opens == 2);
        assert(f.objects.back() == L"/repos/Aspeternity/Asterun/releases/latest"); CheckClosed(f);
    }
    { // Invalid manifest remains a parse failure, never a retry.
        Fixture f; fixture = &f;
        auto result = CheckForUpdate(root, "1.0.1", UpdateChannel::Development, {}, {});
        assert(result.snapshot.failure == UpdateFailure::ManifestInvalid && f.opens == 1); CheckClosed(f);
    }
    for (bool package : {false, true}) for (DWORD phase : {
            WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE, WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE,
            WINHTTP_CALLBACK_STATUS_READ_COMPLETE}) {
        Fixture f; fixture = &f; f.blocked = phase; f.payload = manifestText;
        std::stop_source stop;
        std::jthread worker([&] {
            auto snapshot = package ? PrepareUpdate(root, UpdateChannel::Development, manifest, "1.0.1", {}, stop.get_token()).snapshot
                : CheckForUpdate(root, "1.0.1", UpdateChannel::Development, {}, stop.get_token()).snapshot;
            assert(snapshot.failure == UpdateFailure::Cancelled && snapshot.nativeError == ERROR_CANCELLED);
        });
        f.Wait(&Fixture::entered); stop.request_stop(); worker.join(); CheckClosed(f);
        assert(!fs::exists(root / "update/downloads/Asterun-x64.zip.download"));
    }
    for (DWORD failure : {WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE, WINHTTP_CALLBACK_STATUS_READ_COMPLETE}) {
        Fixture f; fixture = &f; f.failStart = failure;
        auto result = PrepareUpdate(root, UpdateChannel::Development, manifest, "1.0.1", {}, {});
        assert(result.snapshot.failure == UpdateFailure::AssetDownloadFailed && f.opens == 1);
        assert(!fs::exists(root / "update/downloads/Asterun-x64.zip.download")); CheckClosed(f);
    }
    { // A transient package HTTP failure is NOT retried.
        Fixture f; fixture = &f; f.statuses = {503};
        auto result = PrepareUpdate(root, UpdateChannel::Development, manifest, "1.0.1", {}, {});
        assert(result.snapshot.failure == UpdateFailure::AssetDownloadFailed && f.opens == 1); CheckClosed(f);
    }
    { // Download -> real SHA256 -> verified archive -> extraction -> install handoff state.
        mz_zip_archive zip{}; assert(mz_zip_writer_init_heap(&zip, 0, 0));
        for (const char* name : {"Asterun.exe", "Update.exe", "Uninstall.exe"})
            assert(mz_zip_writer_add_mem(&zip, name, "test", 4, MZ_DEFAULT_COMPRESSION));
        assert(mz_zip_writer_add_mem(&zip, "VERSION", "9.0.0", 5, MZ_DEFAULT_COMPRESSION));
        void* bytes{}; size_t length{};
        assert(mz_zip_writer_finalize_heap_archive(&zip, &bytes, &length)); mz_zip_writer_end(&zip);
        Fixture f; fixture = &f; f.payload.assign(static_cast<char*>(bytes), length); mz_free(bytes);
        const auto archive = root / "source.zip";
        { std::ofstream output(archive, std::ios::binary); output.write(f.payload.data(), f.payload.size()); }
        std::uint32_t error{}; const auto digest = Sha256File(archive, error); assert(digest);
        manifest.x64.sha256 = manifest.arm64.sha256 = *digest;
        std::vector<UpdateStage> stages;
        auto result = PrepareUpdate(root, UpdateChannel::Development, manifest, "1.0.1",
            [&](const UpdateSnapshot& s) { if (stages.empty() || stages.back() != s.stage) stages.push_back(s.stage); }, {});
        assert(result.snapshot.stage == UpdateStage::ReadyToInstall);
        assert((stages == std::vector{UpdateStage::Downloading, UpdateStage::Verifying,
            UpdateStage::Extracting, UpdateStage::ReadyToInstall}));
        assert(result.snapshot.assetSha256 == *digest && fs::exists(result.snapshot.verifiedArchive));
        assert(fs::exists(result.snapshot.stagingDirectory / "Asterun.exe"));
        assert(!fs::exists(root / "update/downloads/Asterun-x64.zip.download")); CheckClosed(f);
    }
    fs::remove_all(root);
}
}

int main() {
    constexpr DWORD phases[]{WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE,
        WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE,
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
            assert(error == static_cast<std::uint32_t>(asynchronous ? ERROR_WINHTTP_TIMEOUT : ERROR_WINHTTP_CONNECTION_ERROR));
            CheckClosed(f);
        }
    }
    for (DWORD phase : phases) {
        Fixture f; fixture = &f; f.blocked = phase; f.holdApi = true;
        std::stop_source stop;
        std::jthread worker([&] {
            std::uint32_t error{};
            assert(!Download(false, stop.get_token(), error) && error == ERROR_CANCELLED);
        });
        f.Wait(&Fixture::entered);
        stop.request_stop(); // must not close while the initiating API is still on stack
        {
            std::scoped_lock lock(f.mutex);
            assert(!f.closeEntered);
            f.releaseApi = true; f.changed.notify_all();
        }
        worker.join(); CheckClosed(f);
    }
    for (int setup = 0; setup < 4; ++setup) {
        Fixture f; fixture = &f;
        f.failTimeout = setup == 0; f.failOption = setup == 1; f.failCallback = setup == 2; f.failRedirect = setup == 3;
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
        Fixture f; fixture = &f; f.blocked = phases[cycle % 3];
        std::stop_source stop;
        std::jthread worker([&] {
            std::uint32_t error{};
            const bool ok = Download(false, stop.get_token(), error);
            assert(ok || error == ERROR_CANCELLED);
        });
        f.Wait(&Fixture::entered);
        std::jthread completion([&] {
            if (f.blocked == WINHTTP_CALLBACK_STATUS_READ_COMPLETE)
                f.Emit(f.blocked, f.readBuffer, f.readSize);
            else f.Emit(f.blocked);
        });
        // Independent watchdog and exit callers may race each other AND completion.
        std::jthread watchdog([&] { stop.request_stop(); });
        stop.request_stop(); watchdog.join(); completion.join(); worker.join(); CheckClosed(f);
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
    UpdateSemantics();
    std::cout << "Update transport: cancellation, late callbacks, setup errors and 64 completion races passed\n";
}
