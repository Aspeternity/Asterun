#pragma once

#include <filesystem>
#include <string_view>

namespace altrun::win {

[[nodiscard]] bool
RevealInExplorer(
    std::wstring_view target,
    const std::filesystem::path& baseDirectory,
    bool bareRelativeIsPath = false);

} // namespace altrun::win
