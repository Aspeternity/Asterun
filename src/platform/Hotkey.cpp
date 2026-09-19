#include "Hotkey.hpp"

#include <algorithm>
#include <cctype>

namespace altrun::hotkey {

namespace {

std::string LowerAscii(std::string value) {
    std::transform(
        value.begin(),
        value.end(),
        value.begin(),
        [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
    return value;
}

} // namespace

UINT ModifiersFromNames(
    const std::vector<std::string>& modifiers,
    bool includeNoRepeat) {

    UINT value =
        includeNoRepeat ? MOD_NOREPEAT : 0;

    for (auto modifier : modifiers) {
        modifier = LowerAscii(std::move(modifier));

        if (modifier == "alt") {
            value |= MOD_ALT;
        } else if (
            modifier == "ctrl" ||
            modifier == "control") {
            value |= MOD_CONTROL;
        } else if (modifier == "shift") {
            value |= MOD_SHIFT;
        } else if (
            modifier == "win" ||
            modifier == "windows") {
            value |= MOD_WIN;
        }
    }

    return value;
}

UINT KeyFromName(std::string_view keyView) {
    std::string key =
        LowerAscii(std::string(keyView));

    if (key.size() == 1) {
        const unsigned char c =
            static_cast<unsigned char>(key[0]);

        if (std::isalpha(c)) {
            return static_cast<UINT>(
                std::toupper(c));
        }

        if (std::isdigit(c)) {
            return static_cast<UINT>(c);
        }
    }

    if (key == "space") return VK_SPACE;
    if (key == "pause" || key == "break") return VK_PAUSE;
    if (key == "tab") return VK_TAB;
    if (key == "enter" || key == "return") return VK_RETURN;
    if (key == "escape" || key == "esc") return VK_ESCAPE;
    if (key == "home") return VK_HOME;
    if (key == "end") return VK_END;
    if (key == "insert" || key == "ins") return VK_INSERT;
    if (key == "delete" || key == "del") return VK_DELETE;
    if (key == "pageup" || key == "pgup") return VK_PRIOR;
    if (key == "pagedown" || key == "pgdn") return VK_NEXT;
    if (key == "up") return VK_UP;
    if (key == "down") return VK_DOWN;
    if (key == "left") return VK_LEFT;
    if (key == "right") return VK_RIGHT;

    if (key.size() >= 2 && key[0] == 'f') {
        try {
            const int number = std::stoi(key.substr(1));
            if (number >= 1 && number <= 24) {
                return static_cast<UINT>(
                    VK_F1 + number - 1);
            }
        } catch (...) {
        }
    }

    return 0;
}

std::string KeyName(UINT virtualKey) {
    if (virtualKey >= 'A' && virtualKey <= 'Z') {
        return std::string(
            1,
            static_cast<char>(
                std::tolower(
                    static_cast<unsigned char>(
                        virtualKey))));
    }

    if (virtualKey >= '0' && virtualKey <= '9') {
        return std::string(
            1,
            static_cast<char>(virtualKey));
    }

    if (virtualKey >= VK_F1 && virtualKey <= VK_F24) {
        return "f" +
            std::to_string(
                virtualKey - VK_F1 + 1);
    }

    switch (virtualKey) {
    case VK_SPACE: return "space";
    case VK_PAUSE: return "pause";
    case VK_TAB: return "tab";
    case VK_RETURN: return "enter";
    case VK_ESCAPE: return "escape";
    case VK_HOME: return "home";
    case VK_END: return "end";
    case VK_INSERT: return "insert";
    case VK_DELETE: return "delete";
    case VK_PRIOR: return "pageup";
    case VK_NEXT: return "pagedown";
    case VK_UP: return "up";
    case VK_DOWN: return "down";
    case VK_LEFT: return "left";
    case VK_RIGHT: return "right";
    default: return {};
    }
}

std::wstring KeyDisplayName(UINT virtualKey) {
    if (virtualKey >= 'A' && virtualKey <= 'Z') {
        return std::wstring(
            1,
            static_cast<wchar_t>(virtualKey));
    }

    if (virtualKey >= '0' && virtualKey <= '9') {
        return std::wstring(
            1,
            static_cast<wchar_t>(virtualKey));
    }

    if (virtualKey >= VK_F1 && virtualKey <= VK_F24) {
        return L"F" +
            std::to_wstring(
                virtualKey - VK_F1 + 1);
    }

    switch (virtualKey) {
    case VK_SPACE: return L"Space";
    case VK_PAUSE: return L"Pause";
    case VK_TAB: return L"Tab";
    case VK_RETURN: return L"Enter";
    case VK_ESCAPE: return L"Esc";
    case VK_HOME: return L"Home";
    case VK_END: return L"End";
    case VK_INSERT: return L"Insert";
    case VK_DELETE: return L"Delete";
    case VK_PRIOR: return L"Page Up";
    case VK_NEXT: return L"Page Down";
    case VK_UP: return L"Up";
    case VK_DOWN: return L"Down";
    case VK_LEFT: return L"Left";
    case VK_RIGHT: return L"Right";
    default: return L"Unknown";
    }
}

} // namespace altrun::hotkey
