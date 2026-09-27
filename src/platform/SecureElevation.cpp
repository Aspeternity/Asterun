#include "SecureElevation.hpp"

#include <bcrypt.h>
#include <shellapi.h>

#include <algorithm>
#include <cstddef>
#include <array>
#include <filesystem>
#include <string>
#include <vector>

namespace altrun::win {
namespace {

[[nodiscard]] bool
ValidHandle(HANDLE handle) noexcept {
    return handle != nullptr &&
        handle != INVALID_HANDLE_VALUE;
}

void CloseGuard(HANDLE& handle) noexcept {
    if (ValidHandle(handle)) {
        CloseHandle(handle);
    }
    handle = INVALID_HANDLE_VALUE;
}

[[nodiscard]] std::filesystem::path
TemporaryRoot(
    std::uint32_t& nativeError) {
    std::array<wchar_t, 32768> buffer{};

    const DWORD length =
        GetTempPathW(
            static_cast<DWORD>(
                buffer.size()),
            buffer.data());

    if (length == 0 ||
        length >= buffer.size()) {
        nativeError =
            length == 0
                ? static_cast<std::uint32_t>(
                      GetLastError())
                : ERROR_INSUFFICIENT_BUFFER;
        return {};
    }

    nativeError = 0;
    return std::filesystem::path(
        std::wstring(
            buffer.data(),
            length));
}

[[nodiscard]] bool
PlainHandle(
    HANDLE handle,
    bool directory) {
    FILE_ATTRIBUTE_TAG_INFO info{};

    if (!GetFileInformationByHandleEx(
            handle,
            FileAttributeTagInfo,
            &info,
            sizeof(info))) {
        return false;
    }

    if ((info.FileAttributes &
         FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
        SetLastError(
            ERROR_REPARSE_TAG_INVALID);
        return false;
    }

    const bool isDirectory =
        (info.FileAttributes &
         FILE_ATTRIBUTE_DIRECTORY) != 0;

    if (isDirectory != directory) {
        SetLastError(
            ERROR_INVALID_DATA);
        return false;
    }

    return true;
}

[[nodiscard]] bool
OpenGuards(
    const std::filesystem::path& executable,
    SecuredExecutable& secured,
    std::uint32_t& nativeError) {
    const auto directory =
        executable.parent_path();

    if (directory.empty()) {
        nativeError =
            ERROR_INVALID_PARAMETER;
        return false;
    }

    HANDLE directoryGuard =
        CreateFileW(
            directory.c_str(),
            FILE_READ_ATTRIBUTES,
            FILE_SHARE_READ |
                FILE_SHARE_WRITE,
            nullptr,
            OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS |
                FILE_FLAG_OPEN_REPARSE_POINT,
            nullptr);

    if (!ValidHandle(
            directoryGuard)) {
        nativeError =
            static_cast<std::uint32_t>(
                GetLastError());
        return false;
    }

    if (!PlainHandle(
            directoryGuard,
            true)) {
        nativeError =
            static_cast<std::uint32_t>(
                GetLastError());
        CloseHandle(
            directoryGuard);
        return false;
    }

    HANDLE fileGuard =
        CreateFileW(
            executable.c_str(),
            GENERIC_READ,
            FILE_SHARE_READ,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL |
                FILE_FLAG_OPEN_REPARSE_POINT,
            nullptr);

    if (!ValidHandle(
            fileGuard)) {
        nativeError =
            static_cast<std::uint32_t>(
                GetLastError());
        CloseHandle(
            directoryGuard);
        return false;
    }

    if (!PlainHandle(
            fileGuard,
            false)) {
        nativeError =
            static_cast<std::uint32_t>(
                GetLastError());
        CloseHandle(
            fileGuard);
        CloseHandle(
            directoryGuard);
        return false;
    }

    secured.Reset();
    secured.executable = executable;
    secured.fileGuard =
        fileGuard;
    secured.directoryGuard =
        directoryGuard;
    nativeError = 0;
    return true;
}

[[nodiscard]] bool
CopyFileExclusive(
    const std::filesystem::path& source,
    const std::filesystem::path& destination,
    std::uint32_t& nativeError) {
    HANDLE input =
        CreateFileW(
            source.c_str(),
            GENERIC_READ,
            FILE_SHARE_READ,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL |
                FILE_FLAG_OPEN_REPARSE_POINT,
            nullptr);

    if (!ValidHandle(input)) {
        nativeError =
            static_cast<std::uint32_t>(
                GetLastError());
        return false;
    }

    if (!PlainHandle(
            input,
            false)) {
        nativeError =
            static_cast<std::uint32_t>(
                GetLastError());
        CloseHandle(input);
        return false;
    }

    HANDLE output =
        CreateFileW(
            destination.c_str(),
            GENERIC_WRITE,
            0,
            nullptr,
            CREATE_NEW,
            FILE_ATTRIBUTE_TEMPORARY |
                FILE_ATTRIBUTE_NOT_CONTENT_INDEXED,
            nullptr);

    if (!ValidHandle(output)) {
        nativeError =
            static_cast<std::uint32_t>(
                GetLastError());
        CloseHandle(input);
        return false;
    }

    std::array<std::byte, 64 * 1024>
        buffer{};
    bool success = true;

    for (;;) {
        DWORD read = 0;

        if (!ReadFile(
                input,
                buffer.data(),
                static_cast<DWORD>(
                    buffer.size()),
                &read,
                nullptr)) {
            nativeError =
                static_cast<std::uint32_t>(
                    GetLastError());
            success = false;
            break;
        }

        if (read == 0) {
            break;
        }

        DWORD offset = 0;

        while (offset < read) {
            DWORD written = 0;

            if (!WriteFile(
                    output,
                    buffer.data() +
                        offset,
                    read - offset,
                    &written,
                    nullptr) ||
                written == 0) {
                nativeError =
                    static_cast<std::uint32_t>(
                        GetLastError());
                success = false;
                break;
            }

            offset += written;
        }

        if (!success) {
            break;
        }
    }

    if (success &&
        !FlushFileBuffers(output)) {
        nativeError =
            static_cast<std::uint32_t>(
                GetLastError());
        success = false;
    }

    CloseHandle(output);
    CloseHandle(input);

    if (!success) {
        DeleteFileW(
            destination.c_str());
        return false;
    }

    nativeError = 0;
    return true;
}

[[nodiscard]] bool
StartsWith(
    std::wstring_view value,
    std::wstring_view prefix) {
    return value.size() >=
               prefix.size() &&
        value.substr(
            0,
            prefix.size()) ==
            prefix;
}

} // namespace

SecuredExecutable::~SecuredExecutable() {
    Reset();
}

void SecuredExecutable::Reset() noexcept {
    CloseGuard(fileGuard);
    CloseGuard(directoryGuard);
    executable.clear();
    temporaryDirectory.clear();
}

void
SecuredExecutable::RemoveTemporaryNow()
    noexcept {
    const auto file = executable;
    const auto directory =
        temporaryDirectory;

    Reset();

    if (!file.empty()) {
        DeleteFileW(file.c_str());
    }

    if (!directory.empty()) {
        RemoveDirectoryW(
            directory.c_str());
    }
}

bool SecuredExecutable::Valid() const
    noexcept {
    return
        !executable.empty() &&
        ValidHandle(fileGuard) &&
        ValidHandle(directoryGuard);
}

bool SecuredExecutable::IsTemporary() const
    noexcept {
    return
        !temporaryDirectory.empty();
}

bool GenerateSecureToken(
    std::wstring& token,
    std::uint32_t& nativeError) {
    std::array<unsigned char, 16>
        random{};

    const NTSTATUS status =
        BCryptGenRandom(
            nullptr,
            random.data(),
            static_cast<ULONG>(
                random.size()),
            BCRYPT_USE_SYSTEM_PREFERRED_RNG);

    if (status < 0) {
        nativeError =
            ERROR_GEN_FAILURE;
        token.clear();
        return false;
    }

    constexpr wchar_t hex[] =
        L"0123456789abcdef";

    token.clear();
    token.reserve(
        random.size() * 2);

    for (const auto byte : random) {
        token.push_back(
            hex[(byte >> 4) & 0x0f]);
        token.push_back(
            hex[byte & 0x0f]);
    }

    nativeError = 0;
    return true;
}

bool LockExecutableForElevation(
    const std::filesystem::path& executable,
    SecuredExecutable& secured,
    std::uint32_t& nativeError) {
    return OpenGuards(
        executable,
        secured,
        nativeError);
}

bool CreateSecuredTemporaryExecutableCopy(
    const std::filesystem::path& source,
    SecuredExecutable& secured,
    std::uint32_t& nativeError) {
    secured.RemoveTemporaryNow();

    const auto root =
        TemporaryRoot(
            nativeError);

    if (root.empty()) {
        return false;
    }

    for (int attempt = 0;
         attempt < 16;
         ++attempt) {
        std::wstring token;

        if (!GenerateSecureToken(
                token,
                nativeError)) {
            return false;
        }

        const auto directory =
            root /
            (L"ALTRunNext-Elevated." +
             token);

        if (!CreateDirectoryW(
                directory.c_str(),
                nullptr)) {
            const DWORD error =
                GetLastError();

            if (error ==
                ERROR_ALREADY_EXISTS) {
                continue;
            }

            nativeError =
                static_cast<std::uint32_t>(
                    error);
            return false;
        }

        HANDLE directoryGuard =
            CreateFileW(
                directory.c_str(),
                FILE_READ_ATTRIBUTES,
                FILE_SHARE_READ |
                    FILE_SHARE_WRITE,
                nullptr,
                OPEN_EXISTING,
                FILE_FLAG_BACKUP_SEMANTICS |
                    FILE_FLAG_OPEN_REPARSE_POINT,
                nullptr);

        if (!ValidHandle(
                directoryGuard) ||
            !PlainHandle(
                directoryGuard,
                true)) {
            nativeError =
                static_cast<std::uint32_t>(
                    GetLastError());

            if (ValidHandle(
                    directoryGuard)) {
                CloseHandle(
                    directoryGuard);
            }

            RemoveDirectoryW(
                directory.c_str());
            return false;
        }

        const auto destination =
            directory /
            source.filename();

        if (!CopyFileExclusive(
                source,
                destination,
                nativeError)) {
            CloseHandle(
                directoryGuard);
            RemoveDirectoryW(
                directory.c_str());
            return false;
        }

        HANDLE fileGuard =
            CreateFileW(
                destination.c_str(),
                GENERIC_READ,
                FILE_SHARE_READ,
                nullptr,
                OPEN_EXISTING,
                FILE_ATTRIBUTE_NORMAL |
                    FILE_FLAG_OPEN_REPARSE_POINT,
                nullptr);

        if (!ValidHandle(
                fileGuard) ||
            !PlainHandle(
                fileGuard,
                false)) {
            nativeError =
                static_cast<std::uint32_t>(
                    GetLastError());

            if (ValidHandle(
                    fileGuard)) {
                CloseHandle(
                    fileGuard);
            }

            CloseHandle(
                directoryGuard);
            DeleteFileW(
                destination.c_str());
            RemoveDirectoryW(
                directory.c_str());
            return false;
        }

        secured.Reset();
        secured.executable =
            destination;
        secured.temporaryDirectory =
            directory;
        secured.fileGuard =
            fileGuard;
        secured.directoryGuard =
            directoryGuard;
        nativeError = 0;
        return true;
    }

    nativeError =
        ERROR_ALREADY_EXISTS;
    return false;
}

bool LaunchSecuredExecutable(
    const SecuredExecutable& secured,
    std::wstring_view arguments,
    bool elevate,
    int showCommand,
    HANDLE& process,
    std::uint32_t& nativeError) {
    process = nullptr;

    if (!secured.Valid()) {
        nativeError =
            ERROR_INVALID_HANDLE;
        return false;
    }

    std::wstring argumentCopy(
        arguments);
    const std::wstring directory =
        secured.executable
            .parent_path()
            .wstring();

    SHELLEXECUTEINFOW info{};
    info.cbSize = sizeof(info);
    info.fMask =
        SEE_MASK_NOCLOSEPROCESS |
        SEE_MASK_NOASYNC |
        SEE_MASK_FLAG_NO_UI;
    info.hwnd = nullptr;
    info.lpVerb =
        elevate
            ? L"runas"
            : L"open";
    info.lpFile =
        secured.executable.c_str();
    info.lpParameters =
        argumentCopy.empty()
            ? nullptr
            : argumentCopy.c_str();
    info.lpDirectory =
        directory.empty()
            ? nullptr
            : directory.c_str();
    info.nShow = showCommand;

    if (!ShellExecuteExW(&info)) {
        nativeError =
            static_cast<std::uint32_t>(
                GetLastError());
        return false;
    }

    if (!info.hProcess) {
        nativeError =
            ERROR_INVALID_HANDLE;
        return false;
    }

    process = info.hProcess;
    nativeError = 0;
    return true;
}

void ScheduleTemporaryWorkerSelfCleanup()
    noexcept {
    std::array<wchar_t, 32768>
        buffer{};

    const DWORD length =
        GetModuleFileNameW(
            nullptr,
            buffer.data(),
            static_cast<DWORD>(
                buffer.size()));

    if (length == 0 ||
        length >= buffer.size()) {
        return;
    }

    const std::filesystem::path
        executable(
            std::wstring(
                buffer.data(),
                length));
    const auto directory =
        executable.parent_path();

    if (directory.empty() ||
        !StartsWith(
            directory.filename()
                .wstring(),
            L"ALTRunNext-Elevated.")) {
        return;
    }

    MoveFileExW(
        executable.c_str(),
        nullptr,
        MOVEFILE_DELAY_UNTIL_REBOOT);
    MoveFileExW(
        directory.c_str(),
        nullptr,
        MOVEFILE_DELAY_UNTIL_REBOOT);
}

} // namespace altrun::win
