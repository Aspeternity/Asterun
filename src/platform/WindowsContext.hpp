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
    TotalCommander,
};

struct WindowsContextSnapshot {
    WindowsContextKind kind{
        WindowsContextKind::None};
    HWND foregroundWindow{};
    HWND explorerBrowserWindow{};
    HWND explorerViewWindow{};
    HWND fileDialogWindow{};
    DWORD fileDialogProcessId{};
    HWND totalCommanderWindow{};
    DWORD totalCommanderProcessId{};
    int totalCommanderActivePanel{0};
    std::wstring totalCommanderFolder;
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

    [[nodiscard]] bool
    HasTotalCommander() const noexcept {
        return kind ==
                WindowsContextKind::
                    TotalCommander &&
            totalCommanderWindow !=
                nullptr &&
            totalCommanderProcessId != 0 &&
            (totalCommanderActivePanel == 1 ||
             totalCommanderActivePanel == 2);
    }

    [[nodiscard]] std::wstring_view
    CurrentFilesystemFolder() const noexcept {
        if (HasExplorer() &&
            !explorerFolder.empty()) {
            return explorerFolder;
        }

        if (HasTotalCommander() &&
            !totalCommanderFolder.empty()) {
            return totalCommanderFolder;
        }

        return {};
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

[[nodiscard]] bool
NavigateTotalCommanderToFolder(
    const WindowsContextSnapshot& context,
    std::wstring_view folderPath);

} // namespace altrun::win
