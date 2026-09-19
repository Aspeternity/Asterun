#pragma once

#include <string_view>

namespace altrun::win {

[[nodiscard]] bool
SetClipboardUnicodeText(
    std::wstring_view text);

} // namespace altrun::win
