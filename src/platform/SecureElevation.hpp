#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

namespace altrun::win {

struct SecuredExecutable {
    std::filesystem::path executable;
    std::filesystem::path temporaryDirectory;
    HANDLE fileGuard{INVALID_HANDLE_VALUE};
    HANDLE directoryGuard{INVALID_HANDLE_VALUE};

    SecuredExecutable() = default;
    SecuredExecutable(const SecuredExecutable&) = delete;
    SecuredExecutable& operator=(const SecuredExecutable&) = delete;
    ~SecuredExecutable();

    void Reset() noexcept;
    void RemoveTemporaryNow() noexcept;

    [[nodiscard]] bool Valid() const noexcept;
    [[nodiscard]] bool IsTemporary() const noexcept;
};

[[nodiscard]] bool
GenerateSecureToken(
    std::wstring& token,
    std::uint32_t& nativeError);

[[nodiscard]] bool
LockExecutableForElevation(
    const std::filesystem::path& executable,
    SecuredExecutable& secured,
    std::uint32_t& nativeError);

[[nodiscard]] bool
CreateSecuredTemporaryExecutableCopy(
    const std::filesystem::path& source,
    SecuredExecutable& secured,
    std::uint32_t& nativeError);

[[nodiscard]] bool
LaunchSecuredExecutable(
    const SecuredExecutable& secured,
    std::wstring_view arguments,
    bool elevate,
    int showCommand,
    HANDLE& process,
    std::uint32_t& nativeError);

void ScheduleTemporaryWorkerSelfCleanup() noexcept;

} // namespace altrun::win
