#pragma once

#include <string>
#include <string_view>

namespace altrun {

// RFC 3986 percent-encoding of the UTF-8 bytes of `value`: unreserved
// characters (A-Z a-z 0-9 - . _ ~) stay as is, everything else becomes %XX.
// Unpaired surrogates encode as U+FFFD. Shared by web-search templates and
// URL-encoded shortcut runtime input.
[[nodiscard]] std::wstring PercentEncodeUtf8(
    std::wstring_view value);

} // namespace altrun
