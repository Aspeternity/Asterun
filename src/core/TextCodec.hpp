#pragma once

#include <string>
#include <string_view>

namespace altrun::text {

[[nodiscard]] std::string ToUtf8(std::wstring_view value);
[[nodiscard]] std::wstring FromUtf8(std::string_view value);

} // namespace altrun::text
