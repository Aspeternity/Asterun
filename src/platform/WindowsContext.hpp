#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <string>
#include <string_view>

namespace altrun::win {

enum class WindowsContextKind {
    None,
    Explorer,
};

struct WindowsContextSnapshot {
    WindowsContextKind kind{
        WindowsContextKind::None};
    HWND foregroundWindow{};
    HWND explorerBrowserWindow{};
    HWND explorerViewWindow{};
    std::wstring explorerFolder;

    [[nodiscard]] bool
    HasExplorer() const noexcept {
        return kind ==
                WindowsContextKind::
                    Explorer &&
            explorerBrowserWindow !=
                nullptr &&
            explorerViewWindow !=
                nullptr &&
            !explorerFolder.empty();
    }
};

[[nodiscard]] WindowsContextSnapshot
CaptureWindowsContext(
    HWND foregroundWindow);

[[nodiscard]] bool
NavigateExplorerToFolder(
    const WindowsContextSnapshot& context,
    std::wstring_view folderPath);

} // namespace altrun::win
