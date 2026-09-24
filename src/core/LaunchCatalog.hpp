#pragma once

#include <string>
#include <string_view>

namespace altrun {

enum class LaunchActivationKind {
    ShellItem,
    PackagedApplication,
};

[[nodiscard]] const char*
LaunchActivationKindName(
    LaunchActivationKind kind) noexcept;

[[nodiscard]] LaunchActivationKind
ParseLaunchActivationKind(
    std::string_view value,
    LaunchActivationKind fallback =
        LaunchActivationKind::
            ShellItem) noexcept;

struct PackagedVisibilityEvidence {
    bool hidden{false};
    bool system{false};
    bool preventPinning{false};
};

[[nodiscard]] bool
IsStrongInternalPackagedEntry(
    const PackagedVisibilityEvidence&
        evidence) noexcept;

[[nodiscard]] bool
IsPackagedApplicationId(
    std::wstring_view target) noexcept;

[[nodiscard]] LaunchActivationKind
ActivationKindForCatalogTarget(
    std::wstring_view target) noexcept;

[[nodiscard]] std::wstring
BuildCanonicalLaunchIdentity(
    LaunchActivationKind activation,
    std::wstring_view resolvedTarget,
    std::wstring_view arguments = {});

} // namespace altrun
