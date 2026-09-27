#pragma once

#include <guiddef.h>

namespace altrun::app_identity {

inline constexpr wchar_t kAppUserModelId[] =
    L"Aspeternity.ALTRunNext";

inline constexpr GUID kTrayIconGuid{
    0x8a395c23,
    0x15dc,
    0x516a,
    {0xb9, 0x3c, 0xd9, 0x37, 0xde, 0xbb, 0xd4, 0xa2},
};

} // namespace altrun::app_identity
