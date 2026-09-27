#include "platform/EverythingBootstrapper.hpp"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

using namespace altrun::win;

namespace {

constexpr wchar_t kEverythingClass[] =
    L"EVERYTHING_TASKBAR_NOTIFICATION";

std::filesystem::path TestRoot() {
    std::filesystem::path root =
        std::filesystem::temp_directory_path();
    root /=
        L"ALTRunNext-EverythingLifecycle-" +
        std::to_wstring(
            GetCurrentProcessId());
    return root;
}

LRESULT CALLBACK WindowProc(
    HWND hwnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam) {
    return DefWindowProcW(
        hwnd,
        message,
        wParam,
        lParam);
}

} // namespace

int main() {
    const auto root = TestRoot();

    std::error_code ec;
    std::filesystem::remove_all(
        root,
        ec);

    assert(!IsManagedEverythingRunning(root));

    {
        const auto result =
            StopManagedEverything(root);

        assert(
            result.status ==
            ManagedEverythingStopStatus::
                NotInstalled);
        assert(result.nativeError == 0);
    }

    const auto managed =
        ManagedEverythingExecutable(root);

    assert(
        IsManagedEverythingServiceExecutable(
            root,
            managed));
    assert(
        !IsManagedEverythingServiceExecutable(
            root,
            root /
                L"external" /
                L"Everything.exe"));

    std::filesystem::create_directories(
        managed.parent_path());

    {
        std::ofstream file(
            managed,
            std::ios::binary |
                std::ios::trunc);
        assert(file);
        file << "not-an-executable";
    }

    {
        const auto result =
            StopManagedEverything(root);

        assert(
            result.status ==
            ManagedEverythingStopStatus::
                NotRunning);
        assert(result.nativeError == 0);
    }

    // Versioned managed copies remain ALTRun-owned, and the newest installed
    // stable version becomes the active managed executable.
    const auto managedDirectoryName =
        managed.parent_path()
            .filename()
            .wstring();
    const std::wstring pinnedVersion =
        L"1.4.1.1032";
    assert(
        managedDirectoryName.starts_with(
            pinnedVersion));
    const std::wstring architectureSuffix =
        managedDirectoryName.substr(
            pinnedVersion.size());

    const auto newerManaged =
        root /
        L"tools" /
        L"Everything" /
        (L"1.5.0.1500" +
         architectureSuffix) /
        L"Everything.exe";

    std::filesystem::create_directories(
        newerManaged.parent_path());

    {
        std::ofstream file(
            newerManaged,
            std::ios::binary |
                std::ios::trunc);
        assert(file);
        file << "newer-not-an-executable";
    }

    assert(
        IsManagedEverythingServiceExecutable(
            root,
            newerManaged));
    assert(
        ManagedEverythingExecutable(
            root) ==
        newerManaged);
    assert(
        !IsManagedEverythingServiceExecutable(
            root,
            root /
                L"tools" /
                L"Everything" /
                L"not-a-version-x64" /
                L"Everything.exe"));

    WNDCLASSEXW windowClass{};
    windowClass.cbSize =
        sizeof(windowClass);
    windowClass.lpfnWndProc =
        WindowProc;
    windowClass.hInstance =
        GetModuleHandleW(nullptr);
    windowClass.lpszClassName =
        kEverythingClass;

    const ATOM atom =
        RegisterClassExW(
            &windowClass);

    HWND fakeEverything = nullptr;

    if (atom != 0) {
        fakeEverything =
            CreateWindowExW(
                0,
                kEverythingClass,
                L"",
                WS_OVERLAPPED,
                0,
                0,
                100,
                100,
                nullptr,
                nullptr,
                windowClass.hInstance,
                nullptr);

        assert(fakeEverything != nullptr);

        // Even with ALTRun-owned managed binaries present on disk, an IPC
        // window owned by another executable must not make managed runtime
        // controls visible.
        assert(!IsManagedEverythingRunning(root));

        const auto result =
            StopManagedEverything(root);

        // The IPC-looking window belongs to this test executable, not the
        // managed Everything path. The lifecycle code must not issue -exit.
        assert(
            result.status ==
            ManagedEverythingStopStatus::
                NotRunning);
        assert(result.nativeError == 0);

        DestroyWindow(fakeEverything);
        UnregisterClassW(
            kEverythingClass,
            windowClass.hInstance);
    } else {
        const DWORD error =
            GetLastError();

        // A real Everything instance can already own this class on a shared
        // runner. It is still external, so the same ownership guard applies.
        assert(
            error ==
            ERROR_CLASS_ALREADY_EXISTS);

        const auto result =
            StopManagedEverything(root);

        assert(
            result.status ==
            ManagedEverythingStopStatus::
                NotRunning);
        assert(result.nativeError == 0);
    }

    std::filesystem::remove_all(
        root,
        ec);

    std::cout
        << "Managed Everything lifecycle tests passed\n";
    return 0;
}
