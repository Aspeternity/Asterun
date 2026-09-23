#include "UpdateManager.hpp"

#include "../core/ConfigIO.hpp"
#include "../core/UpdateManifest.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <bcrypt.h>
#include <shellapi.h>
#include <shldisp.h>
#include <winhttp.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include <nlohmann/json.hpp>

namespace altrun::win {
namespace {

constexpr std::uint64_t
    kMaximumManifestBytes =
        256ULL * 1024ULL;
constexpr std::uint64_t
    kMaximumPackageBytes =
        128ULL * 1024ULL * 1024ULL;

constexpr wchar_t
    kStableLatestReleaseMetadataUrl[] =
        L"https://api.github.com/repos/Aspeternity/ALTRunNext/releases/latest";

struct InternetHandle {
    std::atomic<HINTERNET>
        value{nullptr};

    ~InternetHandle() {
        Close();
    }

    InternetHandle() = default;
    InternetHandle(
        const InternetHandle&) = delete;
    InternetHandle& operator=(
        const InternetHandle&) = delete;

    void Reset(
        HINTERNET next) noexcept {
        const HINTERNET previous =
            value.exchange(
                next,
                std::memory_order_acq_rel);

        if (previous) {
            WinHttpCloseHandle(
                previous);
        }
    }

    [[nodiscard]] HINTERNET
    Get() const noexcept {
        return value.load(
            std::memory_order_acquire);
    }

    void Close() noexcept {
        const HINTERNET handle =
            value.exchange(
                nullptr,
                std::memory_order_acq_rel);

        if (handle) {
            WinHttpCloseHandle(
                handle);
        }
    }
};

template <typename T>
struct ComPtr {
    T* value{nullptr};
    ~ComPtr() {
        if (value) {
            value->Release();
        }
    }
    ComPtr() = default;
    ComPtr(const ComPtr&) = delete;
    ComPtr& operator=(
        const ComPtr&) = delete;

    [[nodiscard]] T** Put() {
        if (value) {
            value->Release();
            value = nullptr;
        }
        return &value;
    }

    [[nodiscard]] T* Get() const {
        return value;
    }
};

struct ComApartment {
    HRESULT result{
        CoInitializeEx(
            nullptr,
            COINIT_APARTMENTTHREADED)};

    ~ComApartment() {
        if (SUCCEEDED(result)) {
            CoUninitialize();
        }
    }

    [[nodiscard]] bool Ready() const {
        return SUCCEEDED(result) ||
            result == RPC_E_CHANGED_MODE;
    }
};

void Report(
    UpdateSnapshot& snapshot,
    UpdateStage stage,
    const UpdateProgress& progress) {
    snapshot.stage = stage;

    if (progress) {
        progress(snapshot);
    }
}

[[nodiscard]] UpdateSnapshot
Fail(
    UpdateSnapshot snapshot,
    UpdateFailure failure,
    std::uint32_t nativeError,
    const UpdateProgress& progress) {
    snapshot.stage =
        UpdateStage::Failed;
    snapshot.failure = failure;
    snapshot.nativeError =
        nativeError;
    snapshot.running = false;

    if (progress) {
        progress(snapshot);
    }

    return snapshot;
}

[[nodiscard]] bool
IsNetworkTimeout(
    std::uint32_t error) {
    return
        error == ERROR_TIMEOUT ||
        error == ERROR_WINHTTP_TIMEOUT;
}

[[nodiscard]] bool
CrackHttpsUrl(
    std::wstring_view url,
    std::wstring& host,
    INTERNET_PORT& port,
    std::wstring& object) {
    std::wstring copy(url);

    URL_COMPONENTSW parts{};
    parts.dwStructSize =
        sizeof(parts);
    parts.dwSchemeLength =
        static_cast<DWORD>(-1);
    parts.dwHostNameLength =
        static_cast<DWORD>(-1);
    parts.dwUrlPathLength =
        static_cast<DWORD>(-1);
    parts.dwExtraInfoLength =
        static_cast<DWORD>(-1);

    if (!WinHttpCrackUrl(
            copy.c_str(),
            0,
            0,
            &parts) ||
        parts.nScheme !=
            INTERNET_SCHEME_HTTPS ||
        !parts.lpszHostName ||
        parts.dwHostNameLength == 0) {
        return false;
    }

    host.assign(
        parts.lpszHostName,
        parts.dwHostNameLength);

    object.assign(
        parts.lpszUrlPath
            ? parts.lpszUrlPath
            : L"/",
        parts.dwUrlPathLength);

    if (parts.lpszExtraInfo &&
        parts.dwExtraInfoLength > 0) {
        object.append(
            parts.lpszExtraInfo,
            parts.dwExtraInfoLength);
    }

    if (object.empty()) {
        object = L"/";
    }

    port = parts.nPort;
    return true;
}

struct HttpRequest {
    InternetHandle session;
    InternetHandle connection;
    InternetHandle request;
};

[[nodiscard]] bool
OpenRequest(
    std::wstring_view url,
    HttpRequest& handles,
    std::uint64_t& contentLength,
    std::uint32_t& nativeError,
    std::stop_token stopToken) {
    std::wstring host;
    std::wstring object;
    INTERNET_PORT port = 0;

    if (!CrackHttpsUrl(
            url,
            host,
            port,
            object)) {
        nativeError =
            ERROR_INVALID_PARAMETER;
        return false;
    }

    const HINTERNET session =
        WinHttpOpen(
            L"ALTRunNext Update/0.8",
            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
            WINHTTP_NO_PROXY_NAME,
            WINHTTP_NO_PROXY_BYPASS,
            0);

    handles.session.Reset(
        session);

    if (!session) {
        nativeError =
            static_cast<std::uint32_t>(
                GetLastError());
        return false;
    }

    // Per-operation bounds plus the App-level absolute check watchdog prevent
    // a pathological synchronous request from remaining Checking forever.
    if (!WinHttpSetTimeouts(
            session,
            5000,
            5000,
            15000,
            15000)) {
        nativeError =
            static_cast<std::uint32_t>(
                GetLastError());
        return false;
    }

    const HINTERNET connection =
        WinHttpConnect(
            session,
            host.c_str(),
            port,
            0);

    handles.connection.Reset(
        connection);

    if (!connection) {
        nativeError =
            static_cast<std::uint32_t>(
                GetLastError());
        return false;
    }

    const HINTERNET request =
        WinHttpOpenRequest(
            connection,
            L"GET",
            object.c_str(),
            nullptr,
            WINHTTP_NO_REFERER,
            WINHTTP_DEFAULT_ACCEPT_TYPES,
            WINHTTP_FLAG_SECURE);

    handles.request.Reset(
        request);

    if (!request) {
        nativeError =
            static_cast<std::uint32_t>(
                GetLastError());
        return false;
    }

    DWORD redirectPolicy =
        WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;

    if (!WinHttpSetOption(
            request,
            WINHTTP_OPTION_REDIRECT_POLICY,
            &redirectPolicy,
            sizeof(redirectPolicy))) {
        nativeError =
            static_cast<std::uint32_t>(
                GetLastError());
        return false;
    }

    // request_stop() can now abort a blocked synchronous WinHTTP operation.
    std::stop_callback cancelOnStop{
        stopToken,
        [&handles]() {
            handles.request.Close();
        }};

    if (stopToken.stop_requested()) {
        nativeError =
            ERROR_CANCELLED;
        return false;
    }

    if (!WinHttpSendRequest(
            request,
            WINHTTP_NO_ADDITIONAL_HEADERS,
            0,
            WINHTTP_NO_REQUEST_DATA,
            0,
            0,
            0)) {
        nativeError =
            stopToken.stop_requested()
                ? ERROR_CANCELLED
                : static_cast<
                      std::uint32_t>(
                      GetLastError());
        return false;
    }

    if (!WinHttpReceiveResponse(
            request,
            nullptr)) {
        nativeError =
            stopToken.stop_requested()
                ? ERROR_CANCELLED
                : static_cast<
                      std::uint32_t>(
                      GetLastError());
        return false;
    }

    if (stopToken.stop_requested()) {
        nativeError =
            ERROR_CANCELLED;
        return false;
    }

    DWORD status = 0;
    DWORD statusBytes =
        sizeof(status);

    if (!WinHttpQueryHeaders(
            request,
            WINHTTP_QUERY_STATUS_CODE |
                WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX,
            &status,
            &statusBytes,
            WINHTTP_NO_HEADER_INDEX)) {
        nativeError =
            stopToken.stop_requested()
                ? ERROR_CANCELLED
                : static_cast<
                      std::uint32_t>(
                      GetLastError());
        return false;
    }

    if (status < 200 ||
        status >= 300) {
        nativeError = status;
        return false;
    }

    DWORD length = 0;
    DWORD lengthBytes =
        sizeof(length);

    if (WinHttpQueryHeaders(
            request,
            WINHTTP_QUERY_CONTENT_LENGTH |
                WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX,
            &length,
            &lengthBytes,
            WINHTTP_NO_HEADER_INDEX)) {
        contentLength = length;
    } else {
        contentLength = 0;
    }

    if (stopToken.stop_requested()) {
        nativeError =
            ERROR_CANCELLED;
        return false;
    }

    nativeError = 0;
    return true;
}

[[nodiscard]] bool
DownloadText(
    std::wstring_view url,
    std::string& output,
    std::uint32_t& nativeError,
    std::stop_token stopToken) {
    HttpRequest handles;
    std::uint64_t contentLength = 0;

    if (!OpenRequest(
            url,
            handles,
            contentLength,
            nativeError,
            stopToken) ||
        contentLength >
            kMaximumManifestBytes) {
        if (contentLength >
            kMaximumManifestBytes) {
            nativeError =
                ERROR_FILE_TOO_LARGE;
        }
        return false;
    }

    std::stop_callback cancelOnStop{
        stopToken,
        [&handles]() {
            handles.request.Close();
        }};

    if (stopToken.stop_requested()) {
        nativeError =
            ERROR_CANCELLED;
        return false;
    }

    output.clear();

    // Fixed-size direct reads stay under the receive timeout and remain
    // interruptible through the request handle above.
    std::array<char, 16 * 1024>
        buffer{};

    for (;;) {
        if (stopToken.stop_requested()) {
            nativeError =
                ERROR_CANCELLED;
            return false;
        }

        const HINTERNET request =
            handles.request.Get();

        if (!request) {
            nativeError =
                stopToken.stop_requested()
                    ? ERROR_CANCELLED
                    : ERROR_INVALID_HANDLE;
            return false;
        }

        DWORD read = 0;

        if (!WinHttpReadData(
                request,
                buffer.data(),
                static_cast<DWORD>(
                    buffer.size()),
                &read)) {
            nativeError =
                stopToken.stop_requested()
                    ? ERROR_CANCELLED
                    : static_cast<
                          std::uint32_t>(
                          GetLastError());
            return false;
        }

        if (read == 0) {
            break;
        }

        if (output.size() +
                read >
            kMaximumManifestBytes) {
            nativeError =
                ERROR_FILE_TOO_LARGE;
            return false;
        }

        output.append(
            buffer.data(),
            static_cast<std::size_t>(
                read));
    }

    nativeError = 0;
    return !output.empty();
}

[[nodiscard]]
std::optional<std::string>
ParseLatestStableReleaseVersion(
    std::string_view text) {
    try {
        const auto value =
            nlohmann::json::parse(text);

        if (!value.is_object() ||
            !value.contains("tag_name") ||
            !value["tag_name"].is_string()) {
            return std::nullopt;
        }

        std::string version =
            value["tag_name"]
                .get<std::string>();

        if (!version.empty() &&
            (version.front() == 'v' ||
             version.front() == 'V')) {
            version.erase(
                version.begin());
        }

        if (!CompareVersions(
                version,
                version)) {
            return std::nullopt;
        }

        return version;
    } catch (...) {
        return std::nullopt;
    }
}

[[nodiscard]] bool
DownloadFile(
    std::wstring_view url,
    const std::filesystem::path& path,
    UpdateSnapshot& snapshot,
    const UpdateProgress& progress,
    std::uint32_t& nativeError,
    std::stop_token stopToken) {
    HttpRequest handles;
    std::uint64_t contentLength = 0;

    if (!OpenRequest(
            url,
            handles,
            contentLength,
            nativeError,
            stopToken) ||
        contentLength >
            kMaximumPackageBytes) {
        if (contentLength >
            kMaximumPackageBytes) {
            nativeError =
                ERROR_FILE_TOO_LARGE;
        }
        return false;
    }

    std::stop_callback cancelOnStop{
        stopToken,
        [&handles]() {
            handles.request.Close();
        }};

    if (stopToken.stop_requested()) {
        nativeError =
            ERROR_CANCELLED;
        return false;
    }

    std::ofstream output(
        path,
        std::ios::binary |
            std::ios::trunc);

    if (!output) {
        nativeError =
            ERROR_OPEN_FAILED;
        return false;
    }

    snapshot.downloadedBytes = 0;
    snapshot.totalBytes =
        contentLength;
    Report(
        snapshot,
        UpdateStage::Downloading,
        progress);

    std::array<char, 64 * 1024>
        buffer{};

    for (;;) {
        if (stopToken.stop_requested()) {
            nativeError =
                ERROR_CANCELLED;
            return false;
        }

        const HINTERNET request =
            handles.request.Get();

        if (!request) {
            nativeError =
                stopToken.stop_requested()
                    ? ERROR_CANCELLED
                    : ERROR_INVALID_HANDLE;
            return false;
        }

        DWORD read = 0;

        if (!WinHttpReadData(
                request,
                buffer.data(),
                static_cast<DWORD>(
                    buffer.size()),
                &read)) {
            nativeError =
                stopToken.stop_requested()
                    ? ERROR_CANCELLED
                    : static_cast<
                          std::uint32_t>(
                          GetLastError());
            return false;
        }

        if (read == 0) {
            break;
        }

        if (snapshot.downloadedBytes +
                read >
            kMaximumPackageBytes) {
            nativeError =
                ERROR_FILE_TOO_LARGE;
            return false;
        }

        output.write(
            buffer.data(),
            static_cast<
                std::streamsize>(
                read));

        if (!output) {
            nativeError =
                ERROR_WRITE_FAULT;
            return false;
        }

        snapshot.downloadedBytes +=
            read;

        if (progress) {
            progress(snapshot);
        }
    }

    output.flush();

    if (!output) {
        nativeError =
            ERROR_WRITE_FAULT;
        return false;
    }

    nativeError = 0;
    return snapshot.downloadedBytes >
        0;
}

[[nodiscard]]
std::optional<std::string>
Sha256File(
    const std::filesystem::path& path,
    std::uint32_t& nativeError) {
    BCRYPT_ALG_HANDLE algorithm =
        nullptr;
    BCRYPT_HASH_HANDLE hash =
        nullptr;

    DWORD objectLength = 0;
    DWORD objectLengthBytes =
        sizeof(objectLength);
    DWORD hashLength = 0;
    DWORD hashLengthBytes =
        sizeof(hashLength);

    if (BCryptOpenAlgorithmProvider(
            &algorithm,
            BCRYPT_SHA256_ALGORITHM,
            nullptr,
            0) != 0) {
        nativeError =
            ERROR_INVALID_FUNCTION;
        return std::nullopt;
    }

    const auto cleanup =
        [&]() {
            if (hash) {
                BCryptDestroyHash(hash);
                hash = nullptr;
            }
            if (algorithm) {
                BCryptCloseAlgorithmProvider(
                    algorithm,
                    0);
                algorithm = nullptr;
            }
        };

    if (BCryptGetProperty(
            algorithm,
            BCRYPT_OBJECT_LENGTH,
            reinterpret_cast<PUCHAR>(
                &objectLength),
            sizeof(objectLength),
            &objectLengthBytes,
            0) != 0 ||
        BCryptGetProperty(
            algorithm,
            BCRYPT_HASH_LENGTH,
            reinterpret_cast<PUCHAR>(
                &hashLength),
            sizeof(hashLength),
            &hashLengthBytes,
            0) != 0 ||
        hashLength != 32) {
        cleanup();
        nativeError =
            ERROR_INVALID_DATA;
        return std::nullopt;
    }

    std::vector<UCHAR> object(
        objectLength);

    if (BCryptCreateHash(
            algorithm,
            &hash,
            object.data(),
            objectLength,
            nullptr,
            0,
            0) != 0) {
        cleanup();
        nativeError =
            ERROR_INVALID_FUNCTION;
        return std::nullopt;
    }

    std::ifstream input(
        path,
        std::ios::binary);

    if (!input) {
        cleanup();
        nativeError =
            ERROR_OPEN_FAILED;
        return std::nullopt;
    }

    std::array<char, 64 * 1024>
        buffer{};

    while (input) {
        input.read(
            buffer.data(),
            static_cast<
                std::streamsize>(
                buffer.size()));

        const auto count =
            input.gcount();

        if (count > 0 &&
            BCryptHashData(
                hash,
                reinterpret_cast<PUCHAR>(
                    buffer.data()),
                static_cast<ULONG>(
                    count),
                0) != 0) {
            cleanup();
            nativeError =
                ERROR_INVALID_DATA;
            return std::nullopt;
        }
    }

    if (!input.eof()) {
        cleanup();
        nativeError =
            ERROR_READ_FAULT;
        return std::nullopt;
    }

    std::array<UCHAR, 32>
        digest{};

    if (BCryptFinishHash(
            hash,
            digest.data(),
            static_cast<ULONG>(
                digest.size()),
            0) != 0) {
        cleanup();
        nativeError =
            ERROR_INVALID_DATA;
        return std::nullopt;
    }

    cleanup();

    constexpr char hex[] =
        "0123456789abcdef";
    std::string result;
    result.reserve(64);

    for (const auto byte : digest) {
        result.push_back(
            hex[(byte >> 4) & 0x0f]);
        result.push_back(
            hex[byte & 0x0f]);
    }

    nativeError = 0;
    return result;
}

[[nodiscard]] bool
ReadFileText(
    const std::filesystem::path& path,
    std::string& value) {
    std::ifstream input(
        path,
        std::ios::binary);

    if (!input) {
        return false;
    }

    value.assign(
        std::istreambuf_iterator<char>(
            input),
        std::istreambuf_iterator<char>());

    while (!value.empty() &&
           (value.back() == '\r' ||
            value.back() == '\n' ||
            value.back() == ' ' ||
            value.back() == '\t')) {
        value.pop_back();
    }

    return true;
}

struct TreeStats {
    std::uintmax_t bytes{0};
    std::uint64_t files{0};

    friend bool operator==(
        const TreeStats&,
        const TreeStats&) = default;
};

[[nodiscard]] TreeStats
DirectoryStats(
    const std::filesystem::path& root) {
    TreeStats stats;
    std::error_code ec;

    if (!std::filesystem::exists(
            root,
            ec) ||
        ec) {
        return stats;
    }

    for (std::filesystem::
             recursive_directory_iterator
             it(root, ec),
         end;
         !ec && it != end;
         it.increment(ec)) {
        if (it->is_regular_file(ec) &&
            !ec) {
            stats.bytes +=
                it->file_size(ec);
            if (!ec) {
                ++stats.files;
            }
        }
        ec.clear();
    }

    return stats;
}

[[nodiscard]] bool
ExtractZipWithShell(
    const std::filesystem::path& archive,
    const std::filesystem::path& destination,
    std::uint32_t& nativeError,
    std::stop_token stopToken) {
    std::error_code ec;
    std::filesystem::create_directories(
        destination,
        ec);

    if (ec) {
        nativeError =
            static_cast<std::uint32_t>(
                ec.value());
        return false;
    }

    ComApartment apartment;

    if (!apartment.Ready()) {
        nativeError =
            static_cast<std::uint32_t>(
                apartment.result);
        return false;
    }

    ComPtr<IShellDispatch> shell;

    HRESULT hr =
        CoCreateInstance(
            CLSID_Shell,
            nullptr,
            CLSCTX_INPROC_SERVER,
            IID_IShellDispatch,
            reinterpret_cast<void**>(
                shell.Put()));

    if (FAILED(hr) ||
        !shell.Get()) {
        nativeError =
            static_cast<std::uint32_t>(
                hr);
        return false;
    }

    VARIANT archiveVariant;
    VariantInit(&archiveVariant);
    archiveVariant.vt = VT_BSTR;
    archiveVariant.bstrVal =
        SysAllocString(
            archive.c_str());

    VARIANT destinationVariant;
    VariantInit(&destinationVariant);
    destinationVariant.vt = VT_BSTR;
    destinationVariant.bstrVal =
        SysAllocString(
            destination.c_str());

    if (!archiveVariant.bstrVal ||
        !destinationVariant.bstrVal) {
        VariantClear(
            &archiveVariant);
        VariantClear(
            &destinationVariant);
        nativeError =
            ERROR_NOT_ENOUGH_MEMORY;
        return false;
    }

    ComPtr<Folder> source;
    ComPtr<Folder> destinationFolder;

    hr = shell.Get()->NameSpace(
        archiveVariant,
        source.Put());

    if (SUCCEEDED(hr)) {
        hr = shell.Get()->NameSpace(
            destinationVariant,
            destinationFolder.Put());
    }

    VariantClear(
        &archiveVariant);
    VariantClear(
        &destinationVariant);

    if (FAILED(hr) ||
        !source.Get() ||
        !destinationFolder.Get()) {
        nativeError =
            static_cast<std::uint32_t>(
                FAILED(hr)
                    ? hr
                    : E_FAIL);
        return false;
    }

    ComPtr<FolderItems> items;

    hr = source.Get()->Items(
        items.Put());

    if (FAILED(hr) ||
        !items.Get()) {
        nativeError =
            static_cast<std::uint32_t>(
                FAILED(hr)
                    ? hr
                    : E_FAIL);
        return false;
    }

    VARIANT itemVariant;
    VariantInit(&itemVariant);
    itemVariant.vt =
        VT_DISPATCH;
    itemVariant.pdispVal =
        items.Get();
    itemVariant.pdispVal->AddRef();

    VARIANT options;
    VariantInit(&options);
    options.vt = VT_I4;
    options.lVal =
        FOF_SILENT |
        FOF_NOCONFIRMATION |
        FOF_NOERRORUI |
        FOF_NOCONFIRMMKDIR;

    hr = destinationFolder.Get()->
        CopyHere(
            itemVariant,
            options);

    VariantClear(
        &itemVariant);

    if (FAILED(hr)) {
        nativeError =
            static_cast<std::uint32_t>(
                hr);
        return false;
    }

    TreeStats previous;
    int stableSamples = 0;
    const auto deadline =
        std::chrono::steady_clock::now() +
        std::chrono::seconds(30);

    while (!stopToken.stop_requested() &&
           std::chrono::steady_clock::now() <
               deadline) {
        const TreeStats current =
            DirectoryStats(destination);

        const bool essentials =
            current.files > 0 &&
            std::filesystem::is_regular_file(
                destination /
                    L"ALTRunNext.exe",
                ec) &&
            !ec &&
            std::filesystem::is_regular_file(
                destination /
                    L"Update.exe",
                ec) &&
            !ec &&
            std::filesystem::is_regular_file(
                destination /
                    L"Uninstall.exe",
                ec) &&
            !ec &&
            std::filesystem::is_regular_file(
                destination /
                    L"VERSION",
                ec) &&
            !ec &&
            std::filesystem::is_regular_file(
                destination /
                    L"README.md",
                ec) &&
            !ec &&
            std::filesystem::is_regular_file(
                destination /
                    L"third_party" /
                    L"cpp-pinyin-LICENSE.txt",
                ec) &&
            !ec &&
            std::filesystem::is_directory(
                destination /
                    L"dict" /
                    L"mandarin",
                ec) &&
            !ec;

        if (essentials &&
            current == previous) {
            ++stableSamples;
        } else {
            stableSamples = 0;
        }

        previous = current;

        if (essentials &&
            stableSamples >= 10) {
            nativeError = 0;
            return true;
        }

        std::this_thread::sleep_for(
            std::chrono::milliseconds(
                200));
    }

    nativeError =
        stopToken.stop_requested()
            ? ERROR_CANCELLED
            : ERROR_TIMEOUT;
    return false;
}

[[nodiscard]] std::filesystem::path
UpdateRoot(
    const std::filesystem::path& dataDirectory) {
    return dataDirectory /
        L"update";
}

[[nodiscard]] std::filesystem::path
UpdateStatePath(
    const std::filesystem::path& dataDirectory) {
    return UpdateRoot(
               dataDirectory) /
        L"update-state.json";
}

void MarkChecked(
    const std::filesystem::path& dataDirectory) {
    const auto now =
        std::chrono::system_clock::
            to_time_t(
                std::chrono::
                    system_clock::now());

    const nlohmann::json value = {
        {"schemaVersion", 1},
        {"lastCheckUnix",
         static_cast<std::int64_t>(
             now)},
    };

    (void)config::SaveJsonAtomic(
        UpdateStatePath(
            dataDirectory),
        value);
}

[[nodiscard]] std::wstring
QuoteArgument(
    std::wstring_view value) {
    std::wstring result = L"\"";

    std::size_t backslashes = 0;

    for (const wchar_t c : value) {
        if (c == L'\\') {
            ++backslashes;
            continue;
        }

        if (c == L'\"') {
            result.append(
                backslashes * 2 + 1,
                L'\\');
            result.push_back(L'\"');
            backslashes = 0;
            continue;
        }

        result.append(
            backslashes,
            L'\\');
        backslashes = 0;
        result.push_back(c);
    }

    result.append(
        backslashes * 2,
        L'\\');
    result.push_back(L'\"');
    return result;
}

[[nodiscard]] bool
DirectoryWritable(
    const std::filesystem::path& directory) {
    std::error_code ec;
    const auto probe =
        directory /
        (L".altrun-update-write-" +
         std::to_wstring(
             GetCurrentProcessId()) +
         L".tmp");

    {
        std::ofstream output(
            probe,
            std::ios::binary |
                std::ios::trunc);
        if (!output) {
            return false;
        }
        output << "probe";
    }

    std::filesystem::remove(
        probe,
        ec);
    return !ec;
}

} // namespace

bool UpdateAutoCheckDue(
    const std::filesystem::path& dataDirectory,
    std::int64_t nowUnixSeconds,
    std::int64_t intervalSeconds) {
    const auto state =
        config::LoadJsonWithBackup(
            UpdateStatePath(
                dataDirectory));

    if (!state ||
        !state->contains(
            "lastCheckUnix") ||
        !(*state)["lastCheckUnix"]
             .is_number_integer()) {
        return true;
    }

    const auto last =
        (*state)["lastCheckUnix"]
            .get<std::int64_t>();

    if (last <= 0 ||
        nowUnixSeconds < last) {
        return true;
    }

    return nowUnixSeconds - last >=
        intervalSeconds;
}

UpdateCheckResult
CheckForUpdate(
    const std::filesystem::path& dataDirectory,
    std::string_view currentVersion,
    UpdateChannel channel,
    UpdateProgress progress,
    std::stop_token stopToken) {
    UpdateCheckResult result;
    auto& snapshot =
        result.snapshot;

    snapshot.currentVersion =
        std::string(currentVersion);
    snapshot.running = true;

    Report(
        snapshot,
        UpdateStage::Checking,
        progress);

    std::string text;
    std::uint32_t nativeError = 0;

    if (!DownloadText(
            UpdateManifestUrl(
                channel),
            text,
            nativeError,
            stopToken)) {
        // Stable v0.6.0 predates the native updater and has no
        // update-manifest.json. Treat that legacy release as release metadata
        // rather than surfacing HTTP 404 as a system error.
        if (channel ==
                UpdateChannel::Stable &&
            nativeError == 404 &&
            !stopToken.stop_requested()) {
            std::string releaseText;
            std::uint32_t releaseError = 0;

            if (DownloadText(
                    kStableLatestReleaseMetadataUrl,
                    releaseText,
                    releaseError,
                    stopToken)) {
                const auto stableVersion =
                    ParseLatestStableReleaseVersion(
                        releaseText);

                if (!stableVersion) {
                    snapshot =
                        Fail(
                            snapshot,
                            UpdateFailure::
                                ManifestInvalid,
                            ERROR_INVALID_DATA,
                            progress);
                    return result;
                }

                const auto comparison =
                    CompareVersions(
                        *stableVersion,
                        currentVersion);

                if (!comparison) {
                    snapshot =
                        Fail(
                            snapshot,
                            UpdateFailure::
                                ManifestInvalid,
                            ERROR_INVALID_DATA,
                            progress);
                    return result;
                }

                MarkChecked(
                    dataDirectory);

                snapshot.availableVersion =
                    *stableVersion;
                snapshot.nativeError = 0;
                snapshot.running = false;

                if (*comparison > 0) {
                    snapshot =
                        Fail(
                            snapshot,
                            UpdateFailure::
                                StableManifestUnavailable,
                            0,
                            progress);
                    return result;
                }

                snapshot.stage =
                    *comparison < 0
                        ? UpdateStage::
                              ChannelNotNewer
                        : UpdateStage::
                              UpToDate;
                snapshot.failure =
                    UpdateFailure::None;

                if (progress) {
                    progress(snapshot);
                }

                return result;
            }

            nativeError =
                releaseError;
        }

        // A transient GitHub/release handoff failure must not suppress update
        // checks for the next 24 hours. Only a completed manifest fetch counts
        // as an automatic check for throttle purposes.
        snapshot =
            Fail(
                snapshot,
                stopToken.stop_requested()
                    ? UpdateFailure::
                          Cancelled
                    : IsNetworkTimeout(
                          nativeError)
                        ? UpdateFailure::
                              CheckTimedOut
                        : UpdateFailure::
                              ManifestDownloadFailed,
                nativeError,
                progress);
        return result;
    }

    if (stopToken.stop_requested()) {
        snapshot =
            Fail(
                snapshot,
                UpdateFailure::Cancelled,
                ERROR_CANCELLED,
                progress);
        return result;
    }

    MarkChecked(dataDirectory);

    const auto manifest =
        ParseUpdateManifest(text);

    if (!manifest) {
        snapshot =
            Fail(
                snapshot,
                UpdateFailure::
                    ManifestInvalid,
                ERROR_INVALID_DATA,
                progress);
        return result;
    }

    snapshot.availableVersion =
        manifest->version;
    result.manifest =
        *manifest;

    if (!IsUpdateVersionNewer(
            currentVersion,
            manifest->version)) {
        snapshot.stage =
            UpdateStage::UpToDate;
        snapshot.failure =
            UpdateFailure::None;
        snapshot.running = false;

        if (progress) {
            progress(snapshot);
        }

        return result;
    }

    snapshot.stage =
        UpdateStage::Available;
    snapshot.failure =
        UpdateFailure::None;
    snapshot.running = false;

    if (progress) {
        progress(snapshot);
    }

    return result;
}

UpdatePrepareResult
PrepareUpdate(
    const std::filesystem::path& dataDirectory,
    UpdateChannel channel,
    const UpdateManifest& manifest,
    std::string_view currentVersion,
    UpdateProgress progress,
    std::stop_token stopToken) {
    UpdatePrepareResult result;
    auto& snapshot =
        result.snapshot;

    snapshot.currentVersion =
        std::string(currentVersion);
    snapshot.availableVersion =
        manifest.version;
    snapshot.running = true;

#if defined(_M_ARM64) || defined(__aarch64__)
    const UpdateAsset& asset =
        manifest.arm64;
#else
    const UpdateAsset& asset =
        manifest.x64;
#endif

    if (!IsSafeUpdateAssetName(
            asset.name) ||
        !IsSha256HexString(
            asset.sha256)) {
        snapshot =
            Fail(
                snapshot,
                UpdateFailure::
                    UnsupportedArchitecture,
                ERROR_INVALID_DATA,
                progress);
        return result;
    }

    const auto root =
        UpdateRoot(dataDirectory);
    const auto downloads =
        root /
        L"downloads";
    const auto stagingRoot =
        root /
        L"staging";

    std::error_code ec;
    std::filesystem::create_directories(
        downloads,
        ec);

    if (!ec) {
        std::filesystem::create_directories(
            stagingRoot,
            ec);
    }

    if (ec) {
        snapshot =
            Fail(
                snapshot,
                UpdateFailure::
                    AssetDownloadFailed,
                static_cast<std::uint32_t>(
                    ec.value()),
                progress);
        return result;
    }

    std::wstring assetName;
    assetName.reserve(
        asset.name.size());

    for (const char c : asset.name) {
        assetName.push_back(
            static_cast<unsigned char>(
                c));
    }

    const auto downloadArchive =
        downloads /
        (assetName +
         L".download");
    const auto verifiedArchive =
        downloads /
        assetName;
    const auto stagingDirectory =
        stagingRoot /
        std::filesystem::path(
            std::wstring(
                manifest.version.begin(),
                manifest.version.end()));

    std::filesystem::remove(
        downloadArchive,
        ec);
    ec.clear();
    std::filesystem::remove(
        verifiedArchive,
        ec);
    ec.clear();
    std::filesystem::remove_all(
        stagingDirectory,
        ec);
    ec.clear();

    const std::wstring url =
        UpdateAssetUrl(
            channel,
            asset.name);

    std::uint32_t nativeError = 0;

    if (url.empty() ||
        !DownloadFile(
            url,
            downloadArchive,
            snapshot,
            progress,
            nativeError,
            stopToken)) {
        std::filesystem::remove(
            downloadArchive,
            ec);

        snapshot =
            Fail(
                snapshot,
                stopToken.stop_requested()
                    ? UpdateFailure::
                          Cancelled
                    : UpdateFailure::
                          AssetDownloadFailed,
                nativeError,
                progress);
        return result;
    }

    Report(
        snapshot,
        UpdateStage::Verifying,
        progress);

    const auto hash =
        Sha256File(
            downloadArchive,
            nativeError);

    if (!hash) {
        std::filesystem::remove(
            downloadArchive,
            ec);
        snapshot =
            Fail(
                snapshot,
                UpdateFailure::
                    AssetHashFailed,
                nativeError,
                progress);
        return result;
    }

    if (*hash != asset.sha256) {
        std::filesystem::remove(
            downloadArchive,
            ec);
        snapshot =
            Fail(
                snapshot,
                UpdateFailure::
                    AssetHashMismatch,
                ERROR_CRC,
                progress);
        return result;
    }

    if (!MoveFileExW(
            downloadArchive.c_str(),
            verifiedArchive.c_str(),
            MOVEFILE_REPLACE_EXISTING |
                MOVEFILE_WRITE_THROUGH)) {
        nativeError =
            static_cast<std::uint32_t>(
                GetLastError());
        std::filesystem::remove(
            downloadArchive,
            ec);
        snapshot =
            Fail(
                snapshot,
                UpdateFailure::
                    ExtractionFailed,
                nativeError,
                progress);
        return result;
    }

    Report(
        snapshot,
        UpdateStage::Extracting,
        progress);

    if (!ExtractZipWithShell(
            verifiedArchive,
            stagingDirectory,
            nativeError,
            stopToken)) {
        std::filesystem::remove(
            verifiedArchive,
            ec);
        std::filesystem::remove_all(
            stagingDirectory,
            ec);

        snapshot =
            Fail(
                snapshot,
                stopToken.stop_requested()
                    ? UpdateFailure::
                          Cancelled
                    : UpdateFailure::
                          ExtractionFailed,
                nativeError,
                progress);
        return result;
    }

    std::filesystem::remove(
        verifiedArchive,
        ec);

    std::string stagedVersion;

    if (!ReadFileText(
            stagingDirectory /
                L"VERSION",
            stagedVersion) ||
        stagedVersion !=
            manifest.version ||
        !std::filesystem::is_regular_file(
            stagingDirectory /
                L"ALTRunNext.exe",
            ec) ||
        ec ||
        !std::filesystem::is_regular_file(
            stagingDirectory /
                L"Update.exe",
            ec) ||
        ec ||
        !std::filesystem::is_regular_file(
            stagingDirectory /
                L"Uninstall.exe",
            ec) ||
        ec) {
        snapshot =
            Fail(
                snapshot,
                UpdateFailure::
                    StagedPackageInvalid,
                ERROR_INVALID_DATA,
                progress);
        return result;
    }

    snapshot.stagingDirectory =
        stagingDirectory;
    snapshot.stage =
        UpdateStage::ReadyToInstall;
    snapshot.failure =
        UpdateFailure::None;
    snapshot.running = false;
    snapshot.nativeError = 0;

    if (progress) {
        progress(snapshot);
    }

    return result;
}

bool LaunchPreparedUpdate(
    const std::filesystem::path& baseDirectory,
    const std::filesystem::path& dataDirectory,
    const UpdateSnapshot& snapshot,
    std::string_view currentVersion,
    std::uint32_t parentProcessId,
    std::uint32_t& nativeError) {
    if (snapshot.stage !=
            UpdateStage::
                ReadyToInstall ||
        snapshot.stagingDirectory.empty() ||
        snapshot.availableVersion.empty()) {
        nativeError =
            ERROR_INVALID_DATA;
        return false;
    }

    const auto updater =
        baseDirectory /
        L"Update.exe";

    std::error_code ec;

    if (!std::filesystem::is_regular_file(
            updater,
            ec) ||
        ec) {
        nativeError =
            ERROR_FILE_NOT_FOUND;
        return false;
    }

    std::array<wchar_t, MAX_PATH>
        tempBuffer{};
    const DWORD tempLength =
        GetTempPathW(
            static_cast<DWORD>(
                tempBuffer.size()),
            tempBuffer.data());

    if (tempLength == 0 ||
        tempLength >=
            tempBuffer.size()) {
        nativeError =
            static_cast<std::uint32_t>(
                GetLastError());
        return false;
    }

    const auto tick =
        GetTickCount64();
    const auto tempUpdater =
        std::filesystem::path(
            tempBuffer.data()) /
        (L"ALTRunNext-Update." +
         std::to_wstring(
             parentProcessId) +
         L"." +
         std::to_wstring(tick) +
         L".exe");

    std::filesystem::copy_file(
        updater,
        tempUpdater,
        std::filesystem::
            copy_options::
                overwrite_existing,
        ec);

    if (ec) {
        nativeError =
            static_cast<std::uint32_t>(
                ec.value());
        return false;
    }

    const auto backupDirectory =
        UpdateRoot(dataDirectory) /
        L"backup" /
        std::filesystem::path(
            std::wstring(
                currentVersion.begin(),
                currentVersion.end()));

    const std::wstring healthEvent =
        L"Local\\Aspeternity.ALTRunNext.UpdateHealth." +
        std::to_wstring(
            parentProcessId) +
        L"." +
        std::to_wstring(tick);

    std::wstring arguments =
        L"--apply --parent-pid " +
        std::to_wstring(
            parentProcessId) +
        L" --source " +
        QuoteArgument(
            snapshot.stagingDirectory
                .wstring()) +
        L" --install " +
        QuoteArgument(
            baseDirectory.wstring()) +
        L" --backup " +
        QuoteArgument(
            backupDirectory.wstring()) +
        L" --version " +
        QuoteArgument(
            std::wstring(
                snapshot.availableVersion
                    .begin(),
                snapshot.availableVersion
                    .end())) +
        L" --health-event " +
        QuoteArgument(healthEvent);

    const bool elevate =
        !DirectoryWritable(
            baseDirectory);

    SHELLEXECUTEINFOW info{};
    info.cbSize = sizeof(info);
    info.fMask =
        SEE_MASK_NOCLOSEPROCESS |
        SEE_MASK_NOASYNC;
    info.hwnd = nullptr;
    info.lpVerb =
        elevate
            ? L"runas"
            : L"open";
    info.lpFile =
        tempUpdater.c_str();
    info.lpParameters =
        arguments.c_str();
    info.lpDirectory =
        tempUpdater
            .parent_path()
            .c_str();
    info.nShow = SW_HIDE;

    if (!ShellExecuteExW(&info)) {
        nativeError =
            static_cast<std::uint32_t>(
                GetLastError());
        std::filesystem::remove(
            tempUpdater,
            ec);
        return false;
    }

    if (info.hProcess) {
        CloseHandle(info.hProcess);
    }

    nativeError = 0;
    return true;
}

} // namespace altrun::win
