#pragma once

#include <guiddef.h>

namespace altrun::app_identity {

inline constexpr wchar_t kAppUserModelId[] =
    L"Asterun";

// Exists while an Asterun primary instance runs.
inline constexpr wchar_t kSingleInstanceMutexName[] =
    L"Local\\Aspeternity.Asterun.SingleInstance.v1";

// Held by Update.exe, together with the single-instance guard, while it
// replaces or restores application files.
inline constexpr wchar_t kUpdateInProgressMutexName[] =
    L"Local\\Aspeternity.Asterun.UpdateInProgress.v1";

inline constexpr GUID kTrayIconGuid{
    0x1f25fb11,
    0xdcd3,
    0x417c,
    {0xa6, 0xd9, 0x5d, 0x85, 0x91, 0x42, 0x2e, 0xa8},
};

} // namespace altrun::app_identity
