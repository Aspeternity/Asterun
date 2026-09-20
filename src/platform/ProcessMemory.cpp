#include "ProcessMemory.hpp"

#include <windows.h>
#include <psapi.h>

namespace altrun::win {

ProcessMemorySnapshot
QueryCurrentProcessMemory() noexcept {
    PROCESS_MEMORY_COUNTERS_EX counters{};
    counters.cb =
        static_cast<DWORD>(
            sizeof(counters));

    ProcessMemorySnapshot snapshot;

    if (!GetProcessMemoryInfo(
            GetCurrentProcess(),
            reinterpret_cast<
                PROCESS_MEMORY_COUNTERS*>(
                    &counters),
            static_cast<DWORD>(
                sizeof(counters)))) {
        return snapshot;
    }

    snapshot.available = true;
    snapshot.workingSetBytes =
        static_cast<std::uint64_t>(
            counters.WorkingSetSize);
    snapshot.peakWorkingSetBytes =
        static_cast<std::uint64_t>(
            counters.PeakWorkingSetSize);
    snapshot.privateBytes =
        static_cast<std::uint64_t>(
            counters.PrivateUsage);

    return snapshot;
}

} // namespace altrun::win
