#include "SecureArchive.hpp"
#include "../core/ArchiveExtractor.hpp"

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

std::optional<std::string> Sha256File(
    const std::filesystem::path& path, std::uint32_t& nativeError) {
    const HANDLE input = CreateFileW(path.c_str(), GENERIC_READ,
        FILE_SHARE_READ, nullptr, OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
    if (!ValidHandle(input)) {
        nativeError = GetLastError();
        return std::nullopt;
    }
    FILE_ATTRIBUTE_TAG_INFO info{};
    if (!GetFileInformationByHandleEx(input, FileAttributeTagInfo,
            &info, sizeof(info))) {
        nativeError = GetLastError();
        CloseHandle(input);
        return std::nullopt;
    }
    if ((info.FileAttributes & (FILE_ATTRIBUTE_REPARSE_POINT |
                                FILE_ATTRIBUTE_DIRECTORY)) != 0) {
        nativeError = ERROR_REPARSE_TAG_INVALID;
        CloseHandle(input);
        return std::nullopt;
    }
    std::string digest;
    const bool hashed = HashHandleSha256(input, digest, nativeError);
    CloseHandle(input);
    if (!hashed) return std::nullopt;
    return digest;
}

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

bool ExtractZipVerifiedSecure(
    const std::filesystem::path& archive,
    const std::filesystem::path& destination,
    std::uint32_t& nativeError,
    std::stop_token stopToken) {
    std::error_code error;
    if (!altrun::ExtractArchive(archive, destination, error, stopToken)) {
        nativeError = stopToken.stop_requested() ? ERROR_CANCELLED : ERROR_INVALID_DATA;
        return false;
    }
    nativeError = 0;
    return true;
}

} // namespace altrun::win
