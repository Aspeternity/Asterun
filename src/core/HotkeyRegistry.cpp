#include "HotkeyRegistry.hpp"

#include <algorithm>
#include <cctype>
#include <iterator>
#include <utility>

namespace altrun {
namespace {

std::string LowerAscii(std::string value) {
    std::transform(
        value.begin(),
        value.end(),
        value.begin(),
        [](unsigned char ch) {
            return static_cast<char>(
                std::tolower(ch));
        });
    return value;
}

std::vector<std::string> CanonicalModifiers(
    const std::vector<std::string>& values) {
    bool ctrl=false, alt=false, shift=false, win=false;
    for (auto value : values) {
        value=LowerAscii(std::move(value));
        if (value=="ctrl" || value=="control") ctrl=true;
        else if (value=="alt") alt=true;
        else if (value=="shift") shift=true;
        else if (value=="win" || value=="windows") win=true;
    }
    std::vector<std::string> result;
    if (ctrl) result.push_back("ctrl");
    if (alt) result.push_back("alt");
    if (shift) result.push_back("shift");
    if (win) result.push_back("win");
    return result;
}

bool Has(
    const HotkeyBinding& binding,
    std::string_view value) {
    return std::find(
               binding.modifiers.begin(),
               binding.modifiers.end(),
               value) !=
        binding.modifiers.end();
}

bool IsSupportedKeyName(
    std::string_view key) {
    if (key.size() == 1) {
        const unsigned char ch =
            static_cast<unsigned char>(
                key.front());

        return std::isalnum(ch) != 0;
    }

    if (key.size() >= 2 &&
        key.front() == 'f') {
        try {
            const int number =
                std::stoi(
                    std::string(
                        key.substr(1)));
            if (number >= 1 &&
                number <= 24) {
                return true;
            }
        } catch (...) {
        }
    }

    static constexpr
        std::string_view names[] = {
            "space",
            "pause",
            "tab",
            "enter",
            "escape",
            "home",
            "end",
            "insert",
            "delete",
            "pageup",
            "pagedown",
            "up",
            "down",
            "left",
            "right",
        };

    return std::find(
               std::begin(names),
               std::end(names),
               key) !=
        std::end(names);
}

bool IsSafeBareLauncherActionKey(
    std::string_view key) {
    if (key == "pause") {
        return true;
    }

    if (key.size() >= 2 &&
        key.front() == 'f') {
        try {
            const int number =
                std::stoi(
                    std::string(
                        key.substr(1)));
            return number >= 1 &&
                number <= 24;
        } catch (...) {
            return false;
        }
    }

    return false;
}

bool IsReservedLauncherChord(
    const HotkeyBinding& b) {
    if (!b.enabled) return false;

    const bool ctrl=Has(b,"ctrl");
    const bool alt=Has(b,"alt");
    const bool shift=Has(b,"shift");
    const bool win=Has(b,"win");

    if (b.key=="tab" &&
        !ctrl && !alt && !win) {
        return true;
    }

    if ((b.key=="enter" ||
         b.key=="escape" ||
         b.key=="up" ||
         b.key=="down") &&
        !ctrl && !alt && !shift && !win) {
        return true;
    }

    if (b.key.size()==1 &&
        b.key[0]>='0' &&
        b.key[0]<='9' &&
        !ctrl && !alt && !shift && !win) {
        return true;
    }

    return false;
}

HotkeyBinding Stroke(
    std::string_view key,
    bool ctrl,
    bool alt,
    bool shift,
    bool win) {
    HotkeyBinding b;
    if (ctrl) b.modifiers.push_back("ctrl");
    if (alt) b.modifiers.push_back("alt");
    if (shift) b.modifiers.push_back("shift");
    if (win) b.modifiers.push_back("win");
    b.key=LowerAscii(std::string(key));
    return b;
}

} // namespace

const std::vector<HotkeyActionDescriptor>&
HotkeyActionRegistry() {
    static const std::vector<HotkeyActionDescriptor> actions{
        {std::string(hotkey_actions::kActivate),HotkeyScope::Global,true,true,{true,{"alt"},"space"}},
        {std::string(hotkey_actions::kActivateSecondary),HotkeyScope::Global,false,false,{false,{},"pause"}},
        {std::string(hotkey_actions::kOpenSettings),HotkeyScope::Launcher,false,false,{true,{},"f2"}},
        {std::string(hotkey_actions::kOpenShortcutManager),HotkeyScope::Global,false,true,{true,{"alt"},"s"}},
        {std::string(hotkey_actions::kExitApplication),HotkeyScope::Launcher,false,false,{false,{},"f12"}},
        {std::string(hotkey_actions::kNavigateCurrentFileManager),HotkeyScope::Launcher,false,false,{true,{"ctrl"},"enter"}},
        {std::string(hotkey_actions::kCopySelectedTarget),HotkeyScope::Launcher,false,false,{true,{"ctrl","shift"},"c"}},
    };
    return actions;
}

const HotkeyActionDescriptor*
FindHotkeyAction(
    std::string_view actionId) {
    const auto& actions=HotkeyActionRegistry();
    const auto it=std::find_if(
        actions.begin(),
        actions.end(),
        [&](const auto& action) {
            return action.id==actionId;
        });
    return it==actions.end()?nullptr:&*it;
}

HotkeyBindingMap DefaultHotkeyBindings() {
    HotkeyBindingMap result;
    for (const auto& action:HotkeyActionRegistry()) {
        result.emplace(
            action.id,
            action.defaultBinding);
    }
    return result;
}

HotkeyBinding EffectiveHotkeyBinding(
    const HotkeyBindingMap& bindings,
    std::string_view actionId) {
    if (const auto it=
            bindings.find(
                std::string(actionId));
        it!=bindings.end()) {
        auto result=it->second;
        CanonicalizeHotkeyBinding(result);
        return result;
    }

    if (const auto* action=
            FindHotkeyAction(actionId)) {
        return action->defaultBinding;
    }

    return {};
}

void CanonicalizeHotkeyBinding(
    HotkeyBinding& binding) {
    binding.key =
        LowerAscii(
            std::move(binding.key));

    if (binding.key == "break") {
        binding.key = "pause";
    } else if (
        binding.key == "return") {
        binding.key = "enter";
    } else if (
        binding.key == "esc") {
        binding.key = "escape";
    } else if (
        binding.key == "ins") {
        binding.key = "insert";
    } else if (
        binding.key == "del") {
        binding.key = "delete";
    } else if (
        binding.key == "pgup") {
        binding.key = "pageup";
    } else if (
        binding.key == "pgdn") {
        binding.key = "pagedown";
    }

    binding.modifiers =
        CanonicalModifiers(
            binding.modifiers);
}

bool SameHotkeyChord(
    const HotkeyBinding& left,
    const HotkeyBinding& right) {
    if (!left.enabled ||
        !right.enabled) {
        return false;
    }

    auto a=left;
    auto b=right;
    CanonicalizeHotkeyBinding(a);
    CanonicalizeHotkeyBinding(b);

    return a.key==b.key &&
        a.modifiers==b.modifiers;
}

bool ValidateHotkeyBinding(
    std::string_view actionId,
    const HotkeyBinding& source) {
    const auto* action=
        FindHotkeyAction(actionId);

    if (!action) return false;

    auto binding=source;
    CanonicalizeHotkeyBinding(binding);

    if (action->required &&
        !binding.enabled) {
        return false;
    }

    if (!binding.enabled) {
        return true;
    }

    if (binding.key.empty() ||
        !IsSupportedKeyName(
            binding.key)) {
        return false;
    }

    if (action->requireModifier &&
        binding.modifiers.empty()) {
        return false;
    }

    if (action->scope ==
            HotkeyScope::Launcher) {
        if (IsReservedLauncherChord(
                binding)) {
            return false;
        }

        // The launcher edit control owns unmodified text/editing keys.
        // Bare action bindings are therefore restricted to function-style
        // keys so customization cannot make normal query entry unusable.
        if (binding.modifiers.empty() &&
            !IsSafeBareLauncherActionKey(
                binding.key)) {
            return false;
        }
    }

    return true;
}

std::optional<std::string>
FindHotkeyConflict(
    const HotkeyBindingMap& bindings,
    std::string_view actionId,
    const HotkeyBinding& source) {
    auto candidate=source;
    CanonicalizeHotkeyBinding(candidate);

    if (!candidate.enabled) {
        return std::nullopt;
    }

    for (const auto& action:
         HotkeyActionRegistry()) {
        if (action.id==actionId) continue;

        if (SameHotkeyChord(
                candidate,
                EffectiveHotkeyBinding(
                    bindings,
                    action.id))) {
            return action.id;
        }
    }

    return std::nullopt;
}

std::optional<std::string>
MatchHotkeyAction(
    const HotkeyBindingMap& bindings,
    HotkeyScope scope,
    std::string_view key,
    bool ctrl,
    bool alt,
    bool shift,
    bool win) {
    if (key.empty()) {
        return std::nullopt;
    }

    const auto stroke=
        Stroke(
            key,
            ctrl,
            alt,
            shift,
            win);

    for (const auto& action:
         HotkeyActionRegistry()) {
        if (action.scope!=scope) {
            continue;
        }

        if (SameHotkeyChord(
                EffectiveHotkeyBinding(
                    bindings,
                    action.id),
                stroke)) {
            return action.id;
        }
    }

    return std::nullopt;
}

} // namespace altrun
