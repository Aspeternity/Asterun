#include "platform/Hotkey.hpp"

#include <windows.h>

#include <cassert>
#include <iostream>
#include <string>
#include <vector>

using namespace altrun;

int main() {
    assert(
        hotkey::KeyFromName(
            "pause") ==
        VK_PAUSE);
    assert(
        hotkey::KeyFromName(
            "BREAK") ==
        VK_PAUSE);
    assert(
        hotkey::KeyName(
            VK_PAUSE) ==
        "pause");
    assert(
        hotkey::KeyDisplayName(
            VK_PAUSE) ==
        L"Pause");

    assert(
        hotkey::KeyFromName(
            "space") ==
        VK_SPACE);
    assert(
        hotkey::KeyFromName(
            "F24") ==
        VK_F24);
    assert(
        hotkey::KeyName(
            VK_F24) ==
        "f24");

    const UINT noModifiers =
        hotkey::ModifiersFromNames(
            {},
            false);

    assert(noModifiers == 0);

    const UINT modifiers =
        hotkey::ModifiersFromNames(
            {"ALT",
             "Control",
             "shift",
             "Windows"},
            false);

    assert(
        modifiers ==
        (MOD_ALT |
         MOD_CONTROL |
         MOD_SHIFT |
         MOD_WIN));

    const UINT withNoRepeat =
        hotkey::ModifiersFromNames(
            {"alt"});

    assert(
        (withNoRepeat &
         MOD_ALT) != 0);

    assert(
        (withNoRepeat &
         MOD_NOREPEAT) != 0);

    std::cout
        << "Hotkey codec tests passed\n";

    return 0;
}
