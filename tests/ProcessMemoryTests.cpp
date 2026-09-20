#include "platform/ProcessMemory.hpp"

#include <cassert>
#include <cstdint>
#include <iostream>

int main() {
    const auto snapshot =
        altrun::win::
            QueryCurrentProcessMemory();

    assert(snapshot.available);
    assert(snapshot.workingSetBytes > 0);
    assert(snapshot.peakWorkingSetBytes >=
           snapshot.workingSetBytes);
    assert(snapshot.privateBytes > 0);

    std::cout
        << "Process memory diagnostics passed: "
        << snapshot.workingSetBytes
        << " working-set bytes, "
        << snapshot.privateBytes
        << " private bytes\n";

    return 0;
}
