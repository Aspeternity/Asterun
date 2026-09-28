#pragma once

#include <filesystem>
#include <stop_token>
#include <system_error>

namespace altrun {
// Synchronous extraction: success means every member passed CRC and size
// checks and every output was closed. No detached Shell copy operation.
[[nodiscard]] bool ExtractArchive(
    const std::filesystem::path& archive,
    const std::filesystem::path& destination,
    std::error_code& error,
    std::stop_token stop = {});
}
