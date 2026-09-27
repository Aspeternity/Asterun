#include "SecureArchive.hpp"

#include <bcrypt.h>
#include <shellapi.h>
#include <shldisp.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <cstddef>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>

namespace altrun::win {
namespace {

[[nodiscard]] bool
ValidHandle(HANDLE handle) noexcept {
    return handle != nullptr &&
        handle != INVALID_HANDLE_VALUE;
}

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
        const DWORD attributes =
            GetFileAttributesW(
                it->path().c_str());

        if (attributes ==
                INVALID_FILE_ATTRIBUTES ||
            (attributes &
             FILE_ATTRIBUTE_REPARSE_POINT) !=
                0) {
            ec =
                std::make_error_code(
                    std::errc::
                        too_many_symbolic_link_levels);
            break;
        }

        if (it->is_regular_file(ec) &&
            !ec) {
            stats.bytes +=
                it->file_size(ec);

            if (!ec) {
                ++stats.files;
            }
        }

        if (ec) {
            break;
        }
    }

    if (ec) {
        return {};
    }

    return stats;
}

[[nodiscard]] bool
PlainFileHandle(
    HANDLE handle) {
    FILE_ATTRIBUTE_TAG_INFO info{};

    if (!GetFileInformationByHandleEx(
            handle,
            FileAttributeTagInfo,
            &info,
            sizeof(info))) {
        return false;
    }

    if ((info.FileAttributes &
         (FILE_ATTRIBUTE_DIRECTORY |
          FILE_ATTRIBUTE_REPARSE_POINT)) !=
        0) {
        SetLastError(
            ERROR_REPARSE_TAG_INVALID);
        return false;
    }

    return true;
}

[[nodiscard]] bool
ValidSha256(
    std::string_view value) {
    return value.size() == 64 &&
        std::all_of(
            value.begin(),
            value.end(),
            [](unsigned char ch) {
                return std::isxdigit(ch) !=
                    0;
            });
}

[[nodiscard]] bool
HashHandleSha256(
    HANDLE file,
    std::string& result,
    std::uint32_t& nativeError) {
    LARGE_INTEGER zero{};
    if (!SetFilePointerEx(
            file,
            zero,
            nullptr,
            FILE_BEGIN)) {
        nativeError =
            static_cast<std::uint32_t>(
                GetLastError());
        return false;
    }

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

    if (BCryptOpenAlgorithmProvider(
            &algorithm,
            BCRYPT_SHA256_ALGORITHM,
            nullptr,
            0) != 0 ||
        BCryptGetProperty(
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
        return false;
    }

    std::vector<UCHAR>
        object(objectLength);

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
        return false;
    }

    std::array<UCHAR, 64 * 1024>
        buffer{};

    for (;;) {
        DWORD read = 0;

        if (!ReadFile(
                file,
                buffer.data(),
                static_cast<DWORD>(
                    buffer.size()),
                &read,
                nullptr)) {
            cleanup();
            nativeError =
                static_cast<std::uint32_t>(
                    GetLastError());
            return false;
        }

        if (read == 0) {
            break;
        }

        if (BCryptHashData(
                hash,
                buffer.data(),
                read,
                0) != 0) {
            cleanup();
            nativeError =
                ERROR_INVALID_DATA;
            return false;
        }
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
        return false;
    }

    cleanup();

    constexpr char hex[] =
        "0123456789abcdef";
    result.clear();
    result.reserve(64);

    for (const auto byte : digest) {
        result.push_back(
            hex[(byte >> 4) & 0x0f]);
        result.push_back(
            hex[byte & 0x0f]);
    }

    nativeError = 0;
    return true;
}

} // namespace

LockedVerifiedFile::~LockedVerifiedFile() {
    Reset();
}

void LockedVerifiedFile::Reset()
    noexcept {
    if (ValidHandle(handle)) {
        CloseHandle(handle);
    }

    handle =
        INVALID_HANDLE_VALUE;

    for (HANDLE guard :
         pathGuards) {
        if (ValidHandle(guard)) {
            CloseHandle(guard);
        }
    }

    pathGuards.clear();
}

bool LockedVerifiedFile::Valid() const
    noexcept {
    return ValidHandle(handle);
}

bool LockAndVerifySha256(
    const std::filesystem::path& path,
    std::string_view expectedSha256,
    LockedVerifiedFile& locked,
    std::uint32_t& nativeError) {
    locked.Reset();

    if (!path.is_absolute() ||
        !ValidSha256(
            expectedSha256)) {
        nativeError =
            ERROR_INVALID_PARAMETER;
        return false;
    }

    std::vector<HANDLE>
        pathGuards;
    std::filesystem::path current =
        path.parent_path();

    while (!current.empty()) {
        HANDLE guard =
            CreateFileW(
                current.c_str(),
                FILE_READ_ATTRIBUTES,
                FILE_SHARE_READ,
                nullptr,
                OPEN_EXISTING,
                FILE_FLAG_BACKUP_SEMANTICS |
                    FILE_FLAG_OPEN_REPARSE_POINT,
                nullptr);

        if (!ValidHandle(guard)) {
            nativeError =
                static_cast<std::uint32_t>(
                    GetLastError());

            for (HANDLE opened :
                 pathGuards) {
                CloseHandle(opened);
            }

            return false;
        }

        pathGuards.push_back(
            guard);

        const auto parent =
            current.parent_path();

        if (parent.empty() ||
            parent == current) {
            break;
        }

        current = parent;
    }

    HANDLE handle =
        CreateFileW(
            path.c_str(),
            GENERIC_READ,
            FILE_SHARE_READ,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL |
                FILE_FLAG_OPEN_REPARSE_POINT |
                FILE_FLAG_SEQUENTIAL_SCAN,
            nullptr);

    if (!ValidHandle(handle)) {
        nativeError =
            static_cast<std::uint32_t>(
                GetLastError());

        for (HANDLE opened :
             pathGuards) {
            CloseHandle(opened);
        }

        return false;
    }

    locked.handle = handle;
    locked.pathGuards =
        std::move(pathGuards);

    if (!PlainFileHandle(
            locked.handle)) {
        nativeError =
            static_cast<std::uint32_t>(
                GetLastError());
        locked.Reset();
        return false;
    }

    std::string actual;

    if (!HashHandleSha256(
            locked.handle,
            actual,
            nativeError)) {
        locked.Reset();
        return false;
    }

    std::string expected(
        expectedSha256);

    std::transform(
        expected.begin(),
        expected.end(),
        expected.begin(),
        [](unsigned char ch) {
            return static_cast<char>(
                std::tolower(ch));
        });

    if (actual != expected) {
        nativeError =
            ERROR_CRC;
        locked.Reset();
        return false;
    }

    nativeError = 0;
    return true;
}

bool ExtractZipWithShellSecure(
    const std::filesystem::path& archive,
    const std::filesystem::path& destination,
    std::uint32_t& nativeError,
    std::stop_token stopToken) {
    std::error_code ec;

    if (!archive.is_absolute() ||
        !destination.is_absolute()) {
        nativeError =
            ERROR_INVALID_PARAMETER;
        return false;
    }

    const DWORD destinationAttributes =
        GetFileAttributesW(
            destination.c_str());

    if (destinationAttributes !=
            INVALID_FILE_ATTRIBUTES &&
        (destinationAttributes &
         FILE_ATTRIBUTE_REPARSE_POINT) !=
            0) {
        nativeError =
            ERROR_REPARSE_TAG_INVALID;
        return false;
    }

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
                FAILED(hr)
                    ? hr
                    : E_FAIL);
        return false;
    }

    VARIANT archiveVariant;
    VariantInit(&archiveVariant);
    archiveVariant.vt =
        VT_BSTR;
    archiveVariant.bstrVal =
        SysAllocString(
            archive.c_str());

    VARIANT destinationVariant;
    VariantInit(&destinationVariant);
    destinationVariant.vt =
        VT_BSTR;
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
        std::chrono::steady_clock::
            now() +
        std::chrono::seconds(30);

    while (!stopToken
                .stop_requested() &&
           std::chrono::steady_clock::
               now() < deadline) {
        const TreeStats current =
            DirectoryStats(destination);

        std::error_code validate;
        const bool essentials =
            current.files > 0 &&
            std::filesystem::
                is_regular_file(
                    destination /
                        L"ALTRunNext.exe",
                    validate) &&
            !validate &&
            std::filesystem::
                is_regular_file(
                    destination /
                        L"Update.exe",
                    validate) &&
            !validate &&
            std::filesystem::
                is_regular_file(
                    destination /
                        L"Uninstall.exe",
                    validate) &&
            !validate &&
            std::filesystem::
                is_regular_file(
                    destination /
                        L"VERSION",
                    validate) &&
            !validate &&
            std::filesystem::
                is_regular_file(
                    destination /
                        L"README.md",
                    validate) &&
            !validate &&
            std::filesystem::
                is_regular_file(
                    destination /
                        L"third_party" /
                        L"cpp-pinyin-LICENSE.txt",
                    validate) &&
            !validate &&
            std::filesystem::
                is_directory(
                    destination /
                        L"dict" /
                        L"mandarin",
                    validate) &&
            !validate;

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

} // namespace altrun::win
