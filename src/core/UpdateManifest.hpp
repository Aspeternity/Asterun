#pragma once

#include "UpdatePolicy.hpp"

#include <optional>
#include <string_view>

namespace altrun {

[[nodiscard]] std::optional<UpdateManifest>
ParseUpdateManifest(
    std::string_view jsonText);

} // namespace altrun
