#pragma once

#include <windows.h>

namespace altrun::instance_ipc {

inline constexpr wchar_t
    kLauncherWindowClass[] =
        L"ALTRunNext.Launcher";

inline constexpr ULONG_PTR
    kAddShortcutCopyData =
        0xA17A0001;

} // namespace altrun::instance_ipc
