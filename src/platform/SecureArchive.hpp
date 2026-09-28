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
#include <optional>
#include <string>
#include <stop_token>
#include <string_view>
#include <vector>

namespace altrun::win {

[[nodiscard]] std::optional<std::string>
Sha256File(const std::filesystem::path& path, std::uint32_t& nativeError);

struct LockedVerifiedFile {
    HANDLE handle{INVALID_HANDLE_VALUE};
    std::vector<HANDLE> pathGuards;

    LockedVerifiedFile() = default;
    LockedVerifiedFile(
        const LockedVerifiedFile&) = delete;
    LockedVerifiedFile& operator=(
        const LockedVerifiedFile&) = delete;
    ~LockedVerifiedFile();

    void Reset() noexcept;
    [[nodiscard]] bool Valid() const noexcept;
};

[[nodiscard]] bool
LockAndVerifySha256(
    const std::filesystem::path& path,
    std::string_view expectedSha256,
    LockedVerifiedFile& locked,
    std::uint32_t& nativeError);

[[nodiscard]] bool
ExtractZipVerifiedSecure(
    const std::filesystem::path& archive,
    const std::filesystem::path& destination,
    std::uint32_t& nativeError,
    std::stop_token stopToken = {});

} // namespace altrun::win
