#pragma once

#include <cstdint>

namespace altrun::win {

struct ProcessMemorySnapshot {
    bool available{false};
    std::uint64_t workingSetBytes{0};
    std::uint64_t peakWorkingSetBytes{0};
    std::uint64_t privateBytes{0};
};

[[nodiscard]] ProcessMemorySnapshot
QueryCurrentProcessMemory() noexcept;

} // namespace altrun::win
