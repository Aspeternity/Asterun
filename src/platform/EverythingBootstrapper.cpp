#include "EverythingBootstrapper.hpp"

#include "../core/EverythingBootstrapPolicy.hpp"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <bcrypt.h>
#include <shlobj.h>
#include <shldisp.h>
#include <winhttp.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace altrun::win {
namespace {

constexpr wchar_t kEverythingWindowClass[] =
    L"EVERYTHING_TASKBAR_NOTIFICATION";

struct InternetHandle {
    HINTERNET value{nullptr};

    ~InternetHandle() {
        if (value) {
            WinHttpCloseHandle(value);
        }
    }

    InternetHandle() = default;
    InternetHandle(const InternetHandle&) = delete;
    InternetHandle& operator=(
        const InternetHandle&) = delete;
};

struct RegistryKey {
    HKEY value{nullptr};

    ~RegistryKey() {
        if (value) {
            RegCloseKey(value);
        }
    }

    RegistryKey() = default;
    RegistryKey(const RegistryKey&) = delete;
    RegistryKey& operator=(
        const RegistryKey&) = delete;
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

[[nodiscard]] EverythingPackageArchitecture
CurrentArchitecture() {
#if defined(_M_ARM64) || defined(__aarch64__)
    return EverythingPackageArchitecture::
        Arm64;
#else
    return EverythingPackageArchitecture::
        X64;
#endif
}

[[nodiscard]] std::wstring
VersionDirectoryName() {
    const auto spec =
        ManagedEverythingPackage(
            CurrentArchitecture());

    return spec.version +
        (CurrentArchitecture() ==
                 EverythingPackageArchitecture::
                     Arm64
             ? L"-ARM64"
             : L"-x64");
}

void Report(
    EverythingBootstrapSnapshot& snapshot,
    EverythingBootstrapStage stage,
    const EverythingBootstrapProgress&
        progress) {
    snapshot.stage = stage;

    if (progress) {
        progress(snapshot);
    }
}

[[nodiscard]] std::wstring
EnvironmentVariable(
    const wchar_t* name) {
    const DWORD required =
        GetEnvironmentVariableW(
            name,
            nullptr,
            0);

    if (required == 0) {
        return {};
    }

    std::wstring value(
        required,
        L'\0');

    const DWORD written =
        GetEnvironmentVariableW(
            name,
            value.data(),
            required);

    if (written == 0 ||
        written >= required) {
        return {};
    }

    value.resize(written);
    return value;
}

[[nodiscard]] bool
FileExists(
    const std::filesystem::path& path) {
    std::error_code ec;
    return std::filesystem::is_regular_file(
               path,
               ec) &&
        !ec;
}

[[nodiscard]] std::wstring
LowerPath(
    const std::filesystem::path& path) {
    std::wstring value =
        path.lexically_normal().wstring();

    std::transform(
        value.begin(),
        value.end(),
        value.begin(),
        [](wchar_t c) {
            if (c == L'/') {
                return L'\\';
            }
            return static_cast<wchar_t>(
                std::towlower(c));
        });

    return value;
}

struct ExistingCandidate {
    EverythingBootstrapSource source{
        EverythingBootstrapSource::None};
    std::filesystem::path path;
};

void AddCandidate(
    std::vector<ExistingCandidate>& candidates,
    EverythingBootstrapSource source,
    std::filesystem::path path) {
    if (path.empty() ||
        !FileExists(path)) {
        return;
    }

    const std::wstring key =
        LowerPath(path);

    const bool duplicate =
        std::any_of(
            candidates.begin(),
            candidates.end(),
            [&](const ExistingCandidate& item) {
                return LowerPath(item.path) ==
                    key;
            });

    if (!duplicate) {
        candidates.push_back({
            source,
            std::move(path),
        });
    }
}

[[nodiscard]] std::wstring
ReadRegistryDefault(
    HKEY root,
    REGSAM view) {
    RegistryKey key;

    if (RegOpenKeyExW(
            root,
            L"Software\\Microsoft\\Windows\\CurrentVersion\\App Paths\\Everything.exe",
            0,
            KEY_READ | view,
            &key.value) != ERROR_SUCCESS) {
        return {};
    }

    DWORD type = 0;
    DWORD bytes = 0;

    if (RegQueryValueExW(
            key.value,
            nullptr,
            nullptr,
            &type,
            nullptr,
            &bytes) != ERROR_SUCCESS ||
        bytes == 0 ||
        (type != REG_SZ &&
         type != REG_EXPAND_SZ)) {
        return {};
    }

    std::wstring value(
        bytes / sizeof(wchar_t),
        L'\0');

    if (RegQueryValueExW(
            key.value,
            nullptr,
            nullptr,
            &type,
            reinterpret_cast<BYTE*>(
                value.data()),
            &bytes) != ERROR_SUCCESS) {
        return {};
    }

    while (!value.empty() &&
           value.back() == L'\0') {
        value.pop_back();
    }

    if (type == REG_EXPAND_SZ) {
        const DWORD needed =
            ExpandEnvironmentStringsW(
                value.c_str(),
                nullptr,
                0);

        if (needed > 0) {
            std::wstring expanded(
                needed,
                L'\0');

            if (ExpandEnvironmentStringsW(
                    value.c_str(),
                    expanded.data(),
                    needed) > 0) {
                if (!expanded.empty() &&
                    expanded.back() ==
                        L'\0') {
                    expanded.pop_back();
                }
                value =
                    std::move(expanded);
            }
        }
    }

    if (value.size() >= 2 &&
        value.front() == L'"' &&
        value.back() == L'"') {
        value =
            value.substr(
                1,
                value.size() - 2);
    }

    return value;
}

[[nodiscard]]
std::vector<ExistingCandidate>
FindExistingCandidates(
    const std::filesystem::path&
        dataDirectory) {
    std::vector<ExistingCandidate>
        candidates;

    AddCandidate(
        candidates,
        EverythingBootstrapSource::
            Managed,
        ManagedEverythingExecutable(
            dataDirectory));

    for (const auto view :
         std::array<REGSAM, 2>{
             KEY_WOW64_64KEY,
             KEY_WOW64_32KEY}) {
        for (const auto root :
             std::array<HKEY, 2>{
                 HKEY_CURRENT_USER,
                 HKEY_LOCAL_MACHINE}) {
            const std::wstring value =
                ReadRegistryDefault(
                    root,
                    view);

            if (!value.empty()) {
                AddCandidate(
                    candidates,
                    EverythingBootstrapSource::
                        Registry,
                    value);
            }
        }
    }

    for (const auto* variable :
         std::array<const wchar_t*, 3>{
             L"ProgramFiles",
             L"ProgramFiles(x86)",
             L"ProgramW6432"}) {
        const std::wstring root =
            EnvironmentVariable(
                variable);

        if (!root.empty()) {
            AddCandidate(
                candidates,
                EverythingBootstrapSource::
                    ProgramFiles,
                std::filesystem::path(
                    root) /
                    L"Everything" /
                    L"Everything.exe");
        }
    }

    std::array<wchar_t, 32768>
        found{};

    const DWORD length =
        SearchPathW(
            nullptr,
            L"Everything.exe",
            nullptr,
            static_cast<DWORD>(
                found.size()),
            found.data(),
            nullptr);

    if (length > 0 &&
        length < found.size()) {
        AddCandidate(
            candidates,
            EverythingBootstrapSource::Path,
            std::wstring(
                found.data(),
                length));
    }

    return candidates;
}

[[nodiscard]] bool
LaunchEverything(
    const std::filesystem::path& executable,
    std::uint32_t& nativeError) {
    std::wstring command =
        L"\"" +
        executable.wstring() +
        L"\" -startup -first-instance";

    std::vector<wchar_t> mutableCommand(
        command.begin(),
        command.end());
    mutableCommand.push_back(L'\0');

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};

    const std::wstring directory =
        executable.parent_path()
            .wstring();

    if (!CreateProcessW(
            executable.c_str(),
            mutableCommand.data(),
            nullptr,
            nullptr,
            FALSE,
            CREATE_UNICODE_ENVIRONMENT,
            nullptr,
            directory.empty()
                ? nullptr
                : directory.c_str(),
            &startup,
            &process)) {
        nativeError =
            static_cast<std::uint32_t>(
                GetLastError());
        return false;
    }

    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    nativeError = 0;
    return true;
}

struct NamedIpcSearch {
    std::uint32_t count{0};
};

BOOL CALLBACK CountNamedIpc(
    HWND hwnd,
    LPARAM lParam) {
    auto* search =
        reinterpret_cast<
            NamedIpcSearch*>(
                lParam);

    std::array<wchar_t, 512>
        className{};

    const int length =
        GetClassNameW(
            hwnd,
            className.data(),
            static_cast<int>(
                className.size()));

    if (length <= 0) {
        return TRUE;
    }

    const std::wstring_view value(
        className.data(),
        static_cast<std::size_t>(
            length));

    constexpr std::wstring_view
        prefix =
            L"EVERYTHING_TASKBAR_NOTIFICATION_(";

    if (value.starts_with(prefix) &&
        value.ends_with(L")")) {
        ++search->count;
    }

    return TRUE;
}

[[nodiscard]] bool
AnyUsableIpcEndpoint() {
    if (FindWindowW(
            kEverythingWindowClass,
            nullptr)) {
        return true;
    }

    NamedIpcSearch search;

    EnumWindows(
        CountNamedIpc,
        reinterpret_cast<LPARAM>(
            &search));

    return search.count == 1;
}

[[nodiscard]] bool
WaitForIpc(
    std::stop_token stopToken) {
    constexpr auto timeout =
        std::chrono::seconds(8);
    constexpr auto interval =
        std::chrono::milliseconds(100);

    const auto deadline =
        std::chrono::steady_clock::now() +
        timeout;

    while (!stopToken.stop_requested() &&
           std::chrono::steady_clock::now() <
               deadline) {
        if (AnyUsableIpcEndpoint()) {
            return true;
        }

        std::this_thread::sleep_for(
            interval);
    }

    return false;
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
OpenHttpRequest(
    std::wstring_view url,
    HttpRequest& handles,
    std::uint64_t& contentLength,
    std::uint32_t& nativeError) {
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

    handles.session.value =
        WinHttpOpen(
            L"ALTRunNext/0.7 Managed Everything",
            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
            WINHTTP_NO_PROXY_NAME,
            WINHTTP_NO_PROXY_BYPASS,
            0);

    if (!handles.session.value) {
        nativeError =
            static_cast<std::uint32_t>(
                GetLastError());
        return false;
    }

    WinHttpSetTimeouts(
        handles.session.value,
        5000,
        5000,
        10000,
        10000);

    handles.connection.value =
        WinHttpConnect(
            handles.session.value,
            host.c_str(),
            port,
            0);

    if (!handles.connection.value) {
        nativeError =
            static_cast<std::uint32_t>(
                GetLastError());
        return false;
    }

    handles.request.value =
        WinHttpOpenRequest(
            handles.connection.value,
            L"GET",
            object.c_str(),
            nullptr,
            WINHTTP_NO_REFERER,
            WINHTTP_DEFAULT_ACCEPT_TYPES,
            WINHTTP_FLAG_SECURE);

    if (!handles.request.value) {
        nativeError =
            static_cast<std::uint32_t>(
                GetLastError());
        return false;
    }

    if (!WinHttpSendRequest(
            handles.request.value,
            WINHTTP_NO_ADDITIONAL_HEADERS,
            0,
            WINHTTP_NO_REQUEST_DATA,
            0,
            0,
            0) ||
        !WinHttpReceiveResponse(
            handles.request.value,
            nullptr)) {
        nativeError =
            static_cast<std::uint32_t>(
                GetLastError());
        return false;
    }

    DWORD status = 0;
    DWORD statusBytes =
        sizeof(status);

    if (!WinHttpQueryHeaders(
            handles.request.value,
            WINHTTP_QUERY_STATUS_CODE |
                WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX,
            &status,
            &statusBytes,
            WINHTTP_NO_HEADER_INDEX) ||
        status < 200 ||
        status >= 300) {
        nativeError =
            status >= 400
                ? status
                : static_cast<
                      std::uint32_t>(
                      GetLastError());
        return false;
    }

    DWORD length = 0;
    DWORD lengthBytes =
        sizeof(length);

    if (WinHttpQueryHeaders(
            handles.request.value,
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

    nativeError = 0;
    return true;
}

[[nodiscard]] bool
DownloadText(
    std::wstring_view url,
    std::string& output,
    std::uint32_t& nativeError,
    std::stop_token stopToken) {
    constexpr std::uint64_t
        kMaximumBytes = 512 * 1024;

    HttpRequest handles;
    std::uint64_t contentLength = 0;

    if (!OpenHttpRequest(
            url,
            handles,
            contentLength,
            nativeError) ||
        contentLength > kMaximumBytes) {
        if (contentLength >
            kMaximumBytes) {
            nativeError =
                ERROR_FILE_TOO_LARGE;
        }
        return false;
    }

    output.clear();

    for (;;) {
        if (stopToken.stop_requested()) {
            nativeError =
                ERROR_CANCELLED;
            return false;
        }

        DWORD available = 0;

        if (!WinHttpQueryDataAvailable(
                handles.request.value,
                &available)) {
            nativeError =
                static_cast<std::uint32_t>(
                    GetLastError());
            return false;
        }

        if (available == 0) {
            break;
        }

        if (output.size() +
                available >
            kMaximumBytes) {
            nativeError =
                ERROR_FILE_TOO_LARGE;
            return false;
        }

        const std::size_t oldSize =
            output.size();

        output.resize(
            oldSize + available);

        DWORD read = 0;

        if (!WinHttpReadData(
                handles.request.value,
                output.data() +
                    oldSize,
                available,
                &read)) {
            nativeError =
                static_cast<std::uint32_t>(
                    GetLastError());
            return false;
        }

        output.resize(
            oldSize + read);
    }

    nativeError = 0;
    return !output.empty();
}

[[nodiscard]] bool
DownloadFile(
    std::wstring_view url,
    const std::filesystem::path& path,
    EverythingBootstrapSnapshot& snapshot,
    const EverythingBootstrapProgress&
        progress,
    std::uint32_t& nativeError,
    std::stop_token stopToken) {
    constexpr std::uint64_t
        kMaximumBytes =
            32ULL * 1024ULL * 1024ULL;

    HttpRequest handles;
    std::uint64_t contentLength = 0;

    if (!OpenHttpRequest(
            url,
            handles,
            contentLength,
            nativeError) ||
        contentLength > kMaximumBytes) {
        if (contentLength >
            kMaximumBytes) {
            nativeError =
                ERROR_FILE_TOO_LARGE;
        }
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

    snapshot.totalBytes =
        contentLength;
    snapshot.downloadedBytes = 0;
    Report(
        snapshot,
        EverythingBootstrapStage::
            DownloadingPackage,
        progress);

    std::vector<char> buffer;

    for (;;) {
        if (stopToken.stop_requested()) {
            nativeError =
                ERROR_CANCELLED;
            return false;
        }

        DWORD available = 0;

        if (!WinHttpQueryDataAvailable(
                handles.request.value,
                &available)) {
            nativeError =
                static_cast<std::uint32_t>(
                    GetLastError());
            return false;
        }

        if (available == 0) {
            break;
        }

        if (snapshot.downloadedBytes +
                available >
            kMaximumBytes) {
            nativeError =
                ERROR_FILE_TOO_LARGE;
            return false;
        }

        buffer.resize(available);
        DWORD read = 0;

        if (!WinHttpReadData(
                handles.request.value,
                buffer.data(),
                available,
                &read)) {
            nativeError =
                static_cast<std::uint32_t>(
                    GetLastError());
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
    return snapshot.downloadedBytes > 0;
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

    const auto closeAlgorithm =
        [&]() {
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
        closeAlgorithm();
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
        closeAlgorithm();
        nativeError =
            ERROR_INVALID_FUNCTION;
        return std::nullopt;
    }

    const auto closeHash =
        [&]() {
            if (hash) {
                BCryptDestroyHash(hash);
                hash = nullptr;
            }
        };

    std::ifstream input(
        path,
        std::ios::binary);

    if (!input) {
        closeHash();
        closeAlgorithm();
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
            closeHash();
            closeAlgorithm();
            nativeError =
                ERROR_INVALID_DATA;
            return std::nullopt;
        }
    }

    if (!input.eof()) {
        closeHash();
        closeAlgorithm();
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
        closeHash();
        closeAlgorithm();
        nativeError =
            ERROR_INVALID_DATA;
        return std::nullopt;
    }

    closeHash();
    closeAlgorithm();

    constexpr char hex[] =
        "0123456789abcdef";

    std::string value;
    value.reserve(
        digest.size() * 2);

    for (const auto byte : digest) {
        value.push_back(
            hex[(byte >> 4) & 0x0f]);
        value.push_back(
            hex[byte & 0x0f]);
    }

    nativeError = 0;
    return value;
}

[[nodiscard]] std::string
NarrowAscii(
    std::wstring_view value) {
    std::string result;
    result.reserve(value.size());

    for (const auto c : value) {
        if (c > 0x7f) {
            return {};
        }
        result.push_back(
            static_cast<char>(c));
    }

    return result;
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

    ComPtr<Folder> sourceFolder;
    ComPtr<Folder> destinationFolder;

    hr = shell.Get()->NameSpace(
        archiveVariant,
        sourceFolder.Put());

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
        !sourceFolder.Get() ||
        !destinationFolder.Get()) {
        nativeError =
            static_cast<std::uint32_t>(
                FAILED(hr)
                    ? hr
                    : E_FAIL);
        return false;
    }

    ComPtr<FolderItems> items;

    hr = sourceFolder.Get()->Items(
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

    const auto executable =
        destination /
        L"Everything.exe";

    std::uintmax_t previousSize = 0;
    int stableCount = 0;

    const auto deadline =
        std::chrono::steady_clock::now() +
        std::chrono::seconds(20);

    while (!stopToken.stop_requested() &&
           std::chrono::steady_clock::now() <
               deadline) {
        ec.clear();

        if (std::filesystem::is_regular_file(
                executable,
                ec) &&
            !ec) {
            const auto size =
                std::filesystem::file_size(
                    executable,
                    ec);

            if (!ec && size > 0) {
                if (size ==
                    previousSize) {
                    ++stableCount;
                } else {
                    previousSize =
                        size;
                    stableCount = 0;
                }

                if (stableCount >= 3) {
                    nativeError = 0;
                    return true;
                }
            }
        }

        std::this_thread::sleep_for(
            std::chrono::milliseconds(
                150));
    }

    nativeError =
        stopToken.stop_requested()
            ? ERROR_CANCELLED
            : ERROR_TIMEOUT;
    return false;
}

[[nodiscard]] EverythingBootstrapSnapshot
Fail(
    EverythingBootstrapSnapshot snapshot,
    EverythingBootstrapFailure failure,
    std::uint32_t nativeError,
    const EverythingBootstrapProgress&
        progress) {
    snapshot.stage =
        EverythingBootstrapStage::Failed;
    snapshot.failure = failure;
    snapshot.nativeError = nativeError;
    snapshot.running = false;

    if (progress) {
        progress(snapshot);
    }

    return snapshot;
}

[[nodiscard]] EverythingBootstrapSnapshot
NeedsInstall(
    EverythingBootstrapSnapshot snapshot,
    EverythingBootstrapFailure failure,
    const EverythingBootstrapProgress&
        progress) {
    snapshot.stage =
        EverythingBootstrapStage::
            NeedsInstall;
    snapshot.failure = failure;
    snapshot.running = false;

    if (progress) {
        progress(snapshot);
    }

    return snapshot;
}

} // namespace

std::filesystem::path
ManagedEverythingExecutable(
    const std::filesystem::path&
        dataDirectory) {
    return dataDirectory /
        L"tools" /
        L"Everything" /
        VersionDirectoryName() /
        L"Everything.exe";
}

bool EverythingIpcEndpointAvailable() {
    return AnyUsableIpcEndpoint();
}

EverythingBootstrapSnapshot
RunEverythingBootstrap(
    const std::filesystem::path& dataDirectory,
    bool allowDownload,
    EverythingBootstrapProgress progress,
    std::stop_token stopToken) {
    EverythingBootstrapSnapshot snapshot;
    snapshot.running = true;

    Report(
        snapshot,
        EverythingBootstrapStage::
            Discovering,
        progress);

    if (AnyUsableIpcEndpoint()) {
        snapshot.stage =
            EverythingBootstrapStage::
                Ready;
        snapshot.running = false;
        snapshot.failure =
            EverythingBootstrapFailure::
                None;

        if (progress) {
            progress(snapshot);
        }

        return snapshot;
    }

    const auto candidates =
        FindExistingCandidates(
            dataDirectory);

    if (!candidates.empty()) {
        const auto& candidate =
            candidates.front();

        snapshot.source =
            candidate.source;
        snapshot.executablePath =
            candidate.path;

        Report(
            snapshot,
            EverythingBootstrapStage::
                StartingExisting,
            progress);

        std::uint32_t launchError = 0;

        if (LaunchEverything(
                candidate.path,
                launchError)) {
            Report(
                snapshot,
                EverythingBootstrapStage::
                    WaitingForIpc,
                progress);

            if (WaitForIpc(
                    stopToken)) {
                snapshot.stage =
                    EverythingBootstrapStage::
                        Ready;
                snapshot.running = false;
                snapshot.failure =
                    EverythingBootstrapFailure::
                        None;

                if (progress) {
                    progress(snapshot);
                }

                return snapshot;
            }
        } else if (!allowDownload) {
            return Fail(
                snapshot,
                EverythingBootstrapFailure::
                    ExistingLaunchFailed,
                launchError,
                progress);
        }

        if (stopToken.stop_requested()) {
            return Fail(
                snapshot,
                EverythingBootstrapFailure::
                    Cancelled,
                ERROR_CANCELLED,
                progress);
        }

        if (!allowDownload) {
            return NeedsInstall(
                snapshot,
                EverythingBootstrapFailure::
                    IpcUnavailable,
                progress);
        }
    } else if (!allowDownload) {
        return NeedsInstall(
            snapshot,
            EverythingBootstrapFailure::
                NotFound,
            progress);
    }

    const auto spec =
        ManagedEverythingPackage(
            CurrentArchitecture());

    const auto managedDirectory =
        ManagedEverythingExecutable(
            dataDirectory)
            .parent_path();

    const auto toolsRoot =
        managedDirectory
            .parent_path();

    std::error_code ec;
    std::filesystem::create_directories(
        toolsRoot,
        ec);

    if (ec) {
        return Fail(
            snapshot,
            EverythingBootstrapFailure::
                CreateDirectoryFailed,
            static_cast<std::uint32_t>(
                ec.value()),
            progress);
    }

    Report(
        snapshot,
        EverythingBootstrapStage::
            DownloadingManifest,
        progress);

    std::string manifest;
    std::uint32_t nativeError = 0;

    if (!DownloadText(
            spec.checksumManifestUrl,
            manifest,
            nativeError,
            stopToken)) {
        return Fail(
            snapshot,
            stopToken.stop_requested()
                ? EverythingBootstrapFailure::
                    Cancelled
                : EverythingBootstrapFailure::
                    ManifestDownloadFailed,
            nativeError,
            progress);
    }

    const std::string packageName =
        NarrowAscii(
            spec.fileName);

    const auto expectedHash =
        FindSha256ForFile(
            manifest,
            packageName);

    if (!expectedHash) {
        return Fail(
            snapshot,
            EverythingBootstrapFailure::
                PackageChecksumMissing,
            ERROR_INVALID_DATA,
            progress);
    }

    const auto archive =
        toolsRoot /
        (spec.fileName +
         L".download");

    snapshot.source =
        EverythingBootstrapSource::
            Downloaded;
    snapshot.executablePath =
        ManagedEverythingExecutable(
            dataDirectory);

    if (!DownloadFile(
            spec.downloadUrl,
            archive,
            snapshot,
            progress,
            nativeError,
            stopToken)) {
        std::filesystem::remove(
            archive,
            ec);

        return Fail(
            snapshot,
            stopToken.stop_requested()
                ? EverythingBootstrapFailure::
                    Cancelled
                : EverythingBootstrapFailure::
                    PackageDownloadFailed,
            nativeError,
            progress);
    }

    Report(
        snapshot,
        EverythingBootstrapStage::
            VerifyingPackage,
        progress);

    const auto actualHash =
        Sha256File(
            archive,
            nativeError);

    if (!actualHash) {
        std::filesystem::remove(
            archive,
            ec);

        return Fail(
            snapshot,
            EverythingBootstrapFailure::
                PackageHashFailed,
            nativeError,
            progress);
    }

    if (*actualHash !=
        *expectedHash) {
        std::filesystem::remove(
            archive,
            ec);

        return Fail(
            snapshot,
            EverythingBootstrapFailure::
                PackageHashMismatch,
            ERROR_CRC,
            progress);
    }

    Report(
        snapshot,
        EverythingBootstrapStage::
            ExtractingPackage,
        progress);

    if (!ExtractZipWithShell(
            archive,
            managedDirectory,
            nativeError,
            stopToken)) {
        std::filesystem::remove(
            archive,
            ec);

        return Fail(
            snapshot,
            stopToken.stop_requested()
                ? EverythingBootstrapFailure::
                    Cancelled
                : EverythingBootstrapFailure::
                    ExtractionFailed,
            nativeError,
            progress);
    }

    std::filesystem::remove(
        archive,
        ec);

    const auto managedExecutable =
        ManagedEverythingExecutable(
            dataDirectory);

    if (!FileExists(
            managedExecutable)) {
        return Fail(
            snapshot,
            EverythingBootstrapFailure::
                ManagedExecutableMissing,
            ERROR_FILE_NOT_FOUND,
            progress);
    }

    snapshot.downloaded = true;
    snapshot.executablePath =
        managedExecutable;

    Report(
        snapshot,
        EverythingBootstrapStage::
            StartingManaged,
        progress);

    if (!LaunchEverything(
            managedExecutable,
            nativeError)) {
        return Fail(
            snapshot,
            EverythingBootstrapFailure::
                ManagedLaunchFailed,
            nativeError,
            progress);
    }

    Report(
        snapshot,
        EverythingBootstrapStage::
            WaitingForIpc,
        progress);

    if (!WaitForIpc(
            stopToken)) {
        return Fail(
            snapshot,
            stopToken.stop_requested()
                ? EverythingBootstrapFailure::
                    Cancelled
                : EverythingBootstrapFailure::
                    IpcUnavailable,
            stopToken.stop_requested()
                ? ERROR_CANCELLED
                : ERROR_TIMEOUT,
            progress);
    }

    snapshot.stage =
        EverythingBootstrapStage::Ready;
    snapshot.failure =
        EverythingBootstrapFailure::None;
    snapshot.running = false;
    snapshot.nativeError = 0;

    if (progress) {
        progress(snapshot);
    }

    return snapshot;
}

} // namespace altrun::win
