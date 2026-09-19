#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>

#include <string>
#include <string_view>

namespace altrun::win {

enum class WindowsContextKind {
    None,
    Explorer,
    FileDialog,
};

struct WindowsContextSnapshot {
    WindowsContextKind kind{
        WindowsContextKind::None};
    HWND foregroundWindow{};
    HWND explorerBrowserWindow{};
    HWND explorerViewWindow{};
    HWND fileDialogWindow{};
    DWORD fileDialogProcessId{};
    // Optional diagnostic/source path. Shell namespace locations such as
    // Home, This PC and Quick access legitimately have no filesystem path.
    std::wstring explorerFolder;

    [[nodiscard]] bool
    HasExplorer() const noexcept {
        return kind ==
                WindowsContextKind::
                    Explorer &&
            explorerBrowserWindow !=
                nullptr &&
            explorerViewWindow !=
                nullptr;
    }

    [[nodiscard]] bool
    HasFileDialog() const noexcept {
        return kind ==
                WindowsContextKind::
                    FileDialog &&
            fileDialogWindow !=
                nullptr &&
            fileDialogProcessId != 0;
    }
};

[[nodiscard]] WindowsContextSnapshot
CaptureWindowsContext(
    HWND foregroundWindow);

[[nodiscard]] bool
NavigateExplorerToFolder(
    const WindowsContextSnapshot& context,
    std::wstring_view folderPath);

[[nodiscard]] bool
NavigateFileDialogToFolder(
    const WindowsContextSnapshot& context,
    std::wstring_view folderPath);

} // namespace altrun::win
