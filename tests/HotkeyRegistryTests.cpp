#include "core/HotkeyRegistry.hpp"

#include <cassert>
#include <iostream>
#include <vector>

using namespace altrun;

int main() {
    const auto defaults=DefaultHotkeyBindings();
    assert(defaults.size()==7);

    assert(
        MatchHotkeyAction(
            defaults,
            HotkeyScope::Global,
            "s",
            false,true,false,false) ==
        std::optional<std::string>(
            std::string(
                hotkey_actions::
                    kOpenShortcutManager)));

    assert(
        !EffectiveHotkeyBinding(
             defaults,
             hotkey_actions::
                 kExitApplication)
             .enabled);

    assert(
        MatchHotkeyAction(
            defaults,
            HotkeyScope::Launcher,
            "enter",
            true,false,false,false) ==
        std::optional<std::string>(
            std::string(
                hotkey_actions::
                    kNavigateCurrentFileManager)));

    assert(
        !MatchHotkeyAction(
            defaults,
            HotkeyScope::Launcher,
            "enter",
            false,false,false,false));

    assert(
        FindHotkeyConflict(
            defaults,
            hotkey_actions::kOpenSettings,
            {true,{"ctrl","shift"},"c"}) ==
        std::optional<std::string>(
            std::string(
                hotkey_actions::
                    kCopySelectedTarget)));

    assert(
        !ValidateHotkeyBinding(
            hotkey_actions::kOpenSettings,
            {true,{},"escape"}));

    assert(
        !ValidateHotkeyBinding(
            hotkey_actions::kActivate,
            {true,{},"space"}));

    assert(
        !ValidateHotkeyBinding(
            hotkey_actions::
                kOpenShortcutManager,
            {true,{},"s"}));

    assert(
        ValidateHotkeyBinding(
            hotkey_actions::
                kOpenShortcutManager,
            {true,{"alt"},"s"}));

    assert(
        !ValidateHotkeyBinding(
            hotkey_actions::kActivate,
            {false,{"alt"},"space"}));

    assert(
        !ValidateHotkeyBinding(
            hotkey_actions::
                kCopySelectedTarget,
            {true,{"ctrl"},"not-a-key"}));

    assert(
        !ValidateHotkeyBinding(
            hotkey_actions::
                kCopySelectedTarget,
            {true,{},"c"}));

    assert(
        !ValidateHotkeyBinding(
            hotkey_actions::
                kOpenSettings,
            {true,{},"left"}));

    assert(
        ValidateHotkeyBinding(
            hotkey_actions::
                kOpenSettings,
            {true,{},"f8"}));

    HotkeyBinding custom{
        true,
        {"SHIFT","Control","ctrl"},
        "C"};

    CanonicalizeHotkeyBinding(custom);

    assert(
        custom.modifiers ==
        (std::vector<std::string>{
            "ctrl","shift"}));
    assert(custom.key=="c");

    std::cout
        << "Hotkey registry tests passed\n";
    return 0;
}
