#include <windows.h>

#include <cassert>
#include <iostream>

namespace {

constexpr int kFirstId = 0x6A31;
constexpr int kSecondId = 0x6A32;
constexpr int kModifiedId = 0x6A33;

} // namespace

int main() {
    SetLastError(ERROR_SUCCESS);

    const BOOL first =
        RegisterHotKey(
            nullptr,
            kFirstId,
            0,
            VK_F24);

    assert(first);

    SetLastError(ERROR_SUCCESS);

    const BOOL duplicate =
        RegisterHotKey(
            nullptr,
            kSecondId,
            0,
            VK_F24);

    assert(!duplicate);

    assert(
        UnregisterHotKey(
            nullptr,
            kFirstId));

    const BOOL retry =
        RegisterHotKey(
            nullptr,
            kSecondId,
            0,
            VK_F24);

    assert(retry);

    assert(
        UnregisterHotKey(
            nullptr,
            kSecondId));

    const BOOL modified =
        RegisterHotKey(
            nullptr,
            kModifiedId,
            MOD_CONTROL |
                MOD_ALT |
                MOD_NOREPEAT,
            VK_F23);

    assert(modified);

    assert(
        UnregisterHotKey(
            nullptr,
            kModifiedId));

    std::cout
        << "Windows hotkey runtime tests passed\n";

    return 0;
}
