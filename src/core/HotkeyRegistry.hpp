#pragma once

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace altrun {

namespace hotkey_actions {
inline constexpr std::string_view kActivate = "launcher.activate";
inline constexpr std::string_view kActivateSecondary = "launcher.activateSecondary";
inline constexpr std::string_view kOpenSettings = "launcher.openSettings";
inline constexpr std::string_view kOpenShortcutManager = "launcher.openShortcutManager";
inline constexpr std::string_view kExitApplication = "launcher.exitApplication";
inline constexpr std::string_view kNavigateCurrentFileManager = "result.navigateCurrentFileManager";
inline constexpr std::string_view kCopySelectedTarget = "result.copySelectedTarget";
}

enum class HotkeyScope {
    Global,
    Launcher,
};

struct HotkeyBinding {
    bool enabled{true};
    std::vector<std::string> modifiers;
    std::string key;
};

struct HotkeyActionDescriptor {
    std::string id;
    HotkeyScope scope{HotkeyScope::Launcher};
    bool required{false};
    bool requireModifier{false};
    HotkeyBinding defaultBinding;
};

using HotkeyBindingMap =
    std::map<std::string, HotkeyBinding>;

[[nodiscard]] const std::vector<HotkeyActionDescriptor>&
HotkeyActionRegistry();

[[nodiscard]] const HotkeyActionDescriptor*
FindHotkeyAction(std::string_view actionId);

[[nodiscard]] HotkeyBindingMap
DefaultHotkeyBindings();

[[nodiscard]] HotkeyBinding
EffectiveHotkeyBinding(
    const HotkeyBindingMap& bindings,
    std::string_view actionId);

void CanonicalizeHotkeyBinding(
    HotkeyBinding& binding);

[[nodiscard]] bool SameHotkeyChord(
    const HotkeyBinding& left,
    const HotkeyBinding& right);

[[nodiscard]] bool ValidateHotkeyBinding(
    std::string_view actionId,
    const HotkeyBinding& binding);

[[nodiscard]] std::optional<std::string>
FindHotkeyConflict(
    const HotkeyBindingMap& bindings,
    std::string_view actionId,
    const HotkeyBinding& candidate);

[[nodiscard]] std::optional<std::string>
MatchHotkeyAction(
    const HotkeyBindingMap& bindings,
    HotkeyScope scope,
    std::string_view key,
    bool ctrl,
    bool alt,
    bool shift,
    bool win);

} // namespace altrun
