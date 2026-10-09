#include "NotificationIdentity.hpp"

#include "AppIdentity.hpp"
#include "WinUtil.hpp"
#include "../ResourceIds.h"

#include <shobjidl_core.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <filesystem>
#include <vector>

namespace altrun::notification_identity {
namespace {

constexpr wchar_t kIdentityRegistryPath[] =
    L"Software\\Classes\\AppUserModelId\\Asterun";
constexpr wchar_t kNotificationIconRelativePath[] =
    L"data\\assets\\asterun-notification.ico";

[[nodiscard]] bool SameEmbeddedBytes(
    const std::filesystem::path& path,
    const std::byte* bytes,
    DWORD size) noexcept {

    HANDLE file =
        CreateFileW(
            path.c_str(),
            GENERIC_READ,
            FILE_SHARE_READ |
                FILE_SHARE_WRITE |
                FILE_SHARE_DELETE,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);

    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }

    LARGE_INTEGER length{};
    if (!GetFileSizeEx(file, &length) ||
        length.QuadPart !=
            static_cast<LONGLONG>(size)) {
        CloseHandle(file);
        return false;
    }

    std::array<std::byte, 4096> buffer{};
    DWORD offset = 0;
    bool matches = true;

    while (offset < size) {
        const DWORD requested =
            std::min<DWORD>(
                static_cast<DWORD>(
                    buffer.size()),
                size - offset);
        DWORD read = 0;

        if (!ReadFile(
                file,
                buffer.data(),
                requested,
                &read,
                nullptr) ||
            read != requested ||
            !std::equal(
                buffer.begin(),
                buffer.begin() + read,
                bytes + offset)) {
            matches = false;
            break;
        }

        offset += read;
    }

    CloseHandle(file);
    return matches;
}

[[nodiscard]] bool MaterializeNotificationIcon(
    HINSTANCE instance,
    std::filesystem::path& iconPath) noexcept {

    try {
        iconPath =
            win::ExecutableDirectory() /
            kNotificationIconRelativePath;
    } catch (...) {
        return false;
    }

    HRSRC resource =
        FindResourceW(
            instance,
            MAKEINTRESOURCEW(
                IDR_ASTERUN_NOTIFICATION_ICON),
            RT_RCDATA);

    if (!resource) {
        return false;
    }

    const DWORD size =
        SizeofResource(
            instance,
            resource);

    HGLOBAL loaded =
        LoadResource(
            instance,
            resource);

    if (!loaded || size == 0) {
        return false;
    }

    const auto* bytes =
        static_cast<const std::byte*>(
            LockResource(loaded));

    if (!bytes) {
        return false;
    }

    if (SameEmbeddedBytes(
            iconPath,
            bytes,
            size)) {
        return true;
    }

    std::error_code ec;
    std::filesystem::create_directories(
        iconPath.parent_path(),
        ec);

    if (ec) {
        return false;
    }

    std::filesystem::path temp =
        iconPath;
    temp +=
        L".tmp-" +
        std::to_wstring(
            GetCurrentProcessId());

    HANDLE file =
        CreateFileW(
            temp.c_str(),
            GENERIC_WRITE,
            0,
            nullptr,
            CREATE_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);

    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }

    DWORD written = 0;
    const bool writeOk =
        WriteFile(
            file,
            bytes,
            size,
            &written,
            nullptr) != FALSE &&
        written == size;

    if (writeOk) {
        FlushFileBuffers(file);
    }

    CloseHandle(file);

    if (!writeOk) {
        DeleteFileW(
            temp.c_str());
        return false;
    }

    if (!MoveFileExW(
            temp.c_str(),
            iconPath.c_str(),
            MOVEFILE_REPLACE_EXISTING |
                MOVEFILE_WRITE_THROUGH)) {
        DeleteFileW(
            temp.c_str());

        // If a shell reader briefly holds the previous icon, keeping a valid
        // existing file is still preferable to registering a broken IconUri.
        return GetFileAttributesW(
                   iconPath.c_str()) !=
            INVALID_FILE_ATTRIBUTES;
    }

    return true;
}

[[nodiscard]] bool SetStringValue(
    HKEY key,
    const wchar_t* name,
    const std::wstring& value) noexcept {

    return RegSetValueExW(
               key,
               name,
               0,
               REG_SZ,
               reinterpret_cast<const BYTE*>(
                   value.c_str()),
               static_cast<DWORD>(
                   (value.size() + 1) *
                   sizeof(wchar_t))) ==
        ERROR_SUCCESS;
}

[[nodiscard]] bool RegisterDescriptor(
    const std::filesystem::path& iconPath) noexcept {

    HKEY key = nullptr;
    DWORD disposition = 0;

    if (RegCreateKeyExW(
            HKEY_CURRENT_USER,
            kIdentityRegistryPath,
            0,
            nullptr,
            REG_OPTION_NON_VOLATILE,
            KEY_SET_VALUE,
            nullptr,
            &key,
            &disposition) !=
        ERROR_SUCCESS) {
        return false;
    }

    const bool ok =
        SetStringValue(
            key,
            L"DisplayName",
            L"Asterun") &&
        SetStringValue(
            key,
            L"IconUri",
            iconPath.wstring());

    RegCloseKey(key);
    return ok;
}

} // namespace

void Register(
    HINSTANCE instance) noexcept {

    if (!instance) {
        return;
    }

    std::filesystem::path iconPath;

    // Only assign the explicit AUMID after both the icon and descriptor are
    // valid. This prevents Windows from caching an identity with a blank icon.
    if (!MaterializeNotificationIcon(
            instance,
            iconPath) ||
        !RegisterDescriptor(
            iconPath)) {
        return;
    }

    (void)
        SetCurrentProcessExplicitAppUserModelID(
            app_identity::kAppUserModelId);
}

} // namespace altrun::notification_identity
