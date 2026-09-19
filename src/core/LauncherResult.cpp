#include "LauncherResult.hpp"

#include <cwctype>
#include <string_view>

namespace altrun {
namespace {

[[nodiscard]] bool EqualInsensitive(
    std::wstring_view left,
    std::wstring_view right) {
    if (left.size() != right.size()) {
        return false;
    }

    for (std::size_t i = 0;
         i < left.size();
         ++i) {
        if (std::towlower(left[i]) !=
            std::towlower(right[i])) {
            return false;
        }
    }

    return true;
}

} // namespace

bool SameLauncherTarget(
    const LauncherResult& left,
    const LauncherResult& right) {
    return !left.target.empty() &&
        !right.target.empty() &&
        EqualInsensitive(
            left.target,
            right.target);
}

} // namespace altrun
