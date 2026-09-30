#include "../platform/WindowsCommandLine.hpp"
#include "UpdateManager.hpp"
#include "UpdateHttpRequest.hpp"

#include "../core/ConfigIO.hpp"
#include "../core/ArchiveExtractor.hpp"
#include "../core/UpdateManifest.hpp"
#include "SecureElevation.hpp"
#include "SecureArchive.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
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
        L"https://api.github.com/repos/Aspeternity/Asterun/releases/latest";

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
IsRetryableUpdateCheckError(
    std::uint32_t error) {
    return
        IsTransientUpdateHttpStatus(
            error) ||
        IsNetworkTimeout(error);
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
    update_http::Request request;
};

[[nodiscard]] bool
OpenRequest(
    std::wstring_view url,
    HttpRequest& handles,
    std::uint64_t& contentLength,
    std::uint32_t& nativeError,
    std::stop_token stopToken) {
    if (stopToken.stop_requested()) {
        nativeError = ERROR_CANCELLED;
        return false;
    }

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
            L"Asterun Update/1.0",
            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
            WINHTTP_NO_PROXY_NAME,
            WINHTTP_NO_PROXY_BYPASS,
            WINHTTP_FLAG_ASYNC);

    handles.session.Reset(
        session);

    if (!session) {
        nativeError =
            static_cast<std::uint32_t>(
                GetLastError());
        return false;
    }

    // Per-operation bounds plus the App-level absolute check watchdog prevent
    // a pathological network request from remaining Checking forever.
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

    if (!handles.request.Attach(request, nativeError)) {
        return false;
    }

    DWORD redirectPolicy =
        WINHTTP_OPTION_REDIRECT_POLICY_DISALLOW_HTTPS_TO_HTTP;

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

    if (!handles.request.Send(stopToken, nativeError) ||
        !handles.request.Receive(stopToken, nativeError)) {
        return false;
    }

    if (stopToken.stop_requested()) {
        nativeError = ERROR_CANCELLED;
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

    if (stopToken.stop_requested()) {
        nativeError =
            ERROR_CANCELLED;
        return false;
    }

    output.clear();

    // Preserve fixed-size direct reads. The request owns the buffer until
    // HANDLE_CLOSING, including when cancellation precedes a late callback.
    constexpr DWORD kReadBytes = 16 * 1024;

    for (;;) {
        if (stopToken.stop_requested()) {
            nativeError =
                ERROR_CANCELLED;
            return false;
        }

        DWORD read = 0;

        if (!handles.request.Read(kReadBytes, read, stopToken, nativeError)) {
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
            handles.request.Data(),
            static_cast<std::size_t>(
                read));
    }

    nativeError = 0;
    return !output.empty();
}

[[nodiscard]] bool
WaitForUpdateRetryDelay(
    std::stop_token stopToken,
    std::chrono::milliseconds delay) {
    constexpr auto kSlice =
        std::chrono::milliseconds{50};

    while (delay.count() > 0) {
        if (stopToken.stop_requested()) {
            return false;
        }

        const auto slice =
            std::min(delay, kSlice);
        std::this_thread::sleep_for(
            slice);
        delay -= slice;
    }

    return !stopToken.stop_requested();
}

[[nodiscard]] bool
DownloadTextWithRetry(
    std::wstring_view url,
    std::string& output,
    std::uint32_t& nativeError,
    std::stop_token stopToken) {
    constexpr std::array<
        std::chrono::milliseconds,
        2>
        kRetryDelays{
            std::chrono::milliseconds{250},
            std::chrono::milliseconds{750},
        };

    for (std::size_t attempt = 0;; ++attempt) {
        if (DownloadText(
                url,
                output,
                nativeError,
                stopToken)) {
            return true;
        }

        if (stopToken.stop_requested() ||
            !IsRetryableUpdateCheckError(
                nativeError) ||
            attempt >=
                kRetryDelays.size()) {
            return false;
        }

        if (!WaitForUpdateRetryDelay(
                stopToken,
                kRetryDelays[attempt])) {
            nativeError =
                ERROR_CANCELLED;
            return false;
        }
    }
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

    constexpr DWORD kReadBytes = 64 * 1024;

    for (;;) {
        if (stopToken.stop_requested()) {
            nativeError =
                ERROR_CANCELLED;
            return false;
        }

        DWORD read = 0;

        if (!handles.request.Read(kReadBytes, read, stopToken, nativeError)) {
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
            handles.request.Data(),
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

[[nodiscard]] bool
ExtractZipVerified(const std::filesystem::path& archive,
    const std::filesystem::path& destination, std::uint32_t& nativeError,
    std::stop_token stopToken) {
    std::error_code error;
    if (!altrun::ExtractArchive(archive, destination, error, stopToken)) {
        nativeError = stopToken.stop_requested() ? ERROR_CANCELLED : ERROR_INVALID_DATA;
        return false;
    }
    nativeError = 0;
    return true;
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
QuoteArgument(std::wstring_view value) {
    return QuoteWindowsArgument(value);
}
[[nodiscard]] bool
DirectoryWritable(
    const std::filesystem::path& directory) {
    std::error_code ec;
    const auto probe =
        directory /
        (L".asterun-update-write-" +
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

    if (!DownloadTextWithRetry(
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

            if (DownloadTextWithRetry(
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

    if (!ExtractZipVerified(
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

    // Keep the verified archive through the install handoff. A protected
    // install re-opens it under a no-write/no-delete guard, verifies the
    // manifest SHA-256 again after UAC, and re-extracts into a protected
    // staging directory before any elevated file replacement occurs.
    std::string stagedVersion;

    if (!ReadFileText(
            stagingDirectory /
                L"VERSION",
            stagedVersion) ||
        stagedVersion !=
            manifest.version ||
        !std::filesystem::is_regular_file(
            stagingDirectory /
                L"Asterun.exe",
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
        std::filesystem::remove(
            verifiedArchive,
            ec);
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
    snapshot.verifiedArchive =
        verifiedArchive;
    snapshot.assetSha256 =
        asset.sha256;
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
        snapshot.verifiedArchive.empty() ||
        !IsSha256HexString(
            snapshot.assetSha256) ||
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

    const auto backupDirectory =
        UpdateRoot(dataDirectory) /
        L"backup" /
        std::filesystem::path(
            std::wstring(
                currentVersion.begin(),
                currentVersion.end()));

    const bool elevate =
        !DirectoryWritable(
            baseDirectory);

    std::wstring healthToken;

    if (!GenerateSecureToken(
            healthToken,
            nativeError)) {
        return false;
    }

    const std::wstring healthEvent =
        L"Local\\Aspeternity.Asterun.UpdateHealth." +
        healthToken;

    std::wstring arguments =
        L"--apply --parent-pid " +
        std::to_wstring(
            parentProcessId) +
        L" --source " +
        QuoteArgument(
            snapshot.stagingDirectory
                .wstring()) +
        L" --archive " +
        QuoteArgument(
            snapshot.verifiedArchive
                .wstring()) +
        L" --sha256 " +
        QuoteArgument(
            std::wstring(
                snapshot.assetSha256.begin(),
                snapshot.assetSha256.end())) +
        L" --secure-reextract " +
        (elevate ? L"1" : L"0") +
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

    SecuredExecutable securedUpdater;

    if (!CreateSecuredTemporaryExecutableCopy(
            updater,
            securedUpdater,
            nativeError)) {
        return false;
    }

    HANDLE process = nullptr;

    if (!LaunchSecuredExecutable(
            securedUpdater,
            arguments,
            elevate,
            SW_HIDE,
            process,
            nativeError)) {
        securedUpdater
            .RemoveTemporaryNow();
        return false;
    }

    if (process) {
        CloseHandle(process);
    }

    // Keep the random worker in place after process creation. Update.exe
    // schedules its own executable and dedicated temporary directory for
    // deletion once the transaction is complete.
    securedUpdater.Reset();

    nativeError = 0;
    return true;
}

} // namespace altrun::win
