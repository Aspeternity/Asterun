#pragma once

#include <guiddef.h>

namespace altrun::app_identity {

inline constexpr wchar_t kAppUserModelId[] =
    L"Asterun";

inline constexpr GUID kTrayIconGuid{
    0x1f25fb11,
    0xdcd3,
    0x417c,
    {0xa6, 0xd9, 0x5d, 0x85, 0x91, 0x42, 0x2e, 0xa8},
};

} // namespace altrun::app_identity
