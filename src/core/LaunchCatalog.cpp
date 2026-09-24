#include "LaunchCatalog.hpp"

#include <algorithm>
#include <cwctype>
#include <string>

namespace altrun {
namespace {

[[nodiscard]] std::wstring Trim(
    std::wstring_view value) {

    std::size_t first = 0;
    std::size_t last = value.size();

    while (first < last &&
           std::iswspace(value[first])) {
        ++first;
    }

    while (last > first &&
           std::iswspace(value[last - 1])) {
        --last;
    }

    return std::wstring(
        value.substr(
            first,
            last - first));
}

[[nodiscard]] std::wstring
NormalizeTarget(
    std::wstring_view value) {

    std::wstring result =
        Trim(value);

    std::transform(
        result.begin(),
        result.end(),
        result.begin(),
        [](wchar_t ch) {
            return static_cast<wchar_t>(
                std::towlower(ch));
        });

    std::replace(
        result.begin(),
        result.end(),
        L'/',
        L'\\');

    return result;
}

[[nodiscard]] bool HasUriScheme(
    std::wstring_view target) noexcept {

    const auto colon =
        target.find(L':');

    if (colon ==
            std::wstring_view::npos ||
        colon == 0) {
        return false;
    }

    if (colon == 1 &&
        std::iswalpha(target[0])) {
        return false;
    }

    if (!std::iswalpha(target[0])) {
        return false;
    }

    for (std::size_t i = 1;
         i < colon;
         ++i) {
        const wchar_t ch =
            target[i];

        if (!std::iswalnum(ch) &&
            ch != L'+' &&
            ch != L'-' &&
            ch != L'.') {
            return false;
        }
    }

    return true;
}

} // namespace

const char* LaunchActivationKindName(
    LaunchActivationKind kind) noexcept {

    switch (kind) {
    case LaunchActivationKind::
        ShellItem:
        return "shell-execute";
    case LaunchActivationKind::
        PackagedApplication:
        return "packaged-application";
    }

    return "shell-execute";
}

LaunchActivationKind
ParseLaunchActivationKind(
    std::string_view value,
    LaunchActivationKind fallback) noexcept {

    if (value ==
        "packaged-application") {
        return LaunchActivationKind::
            PackagedApplication;
    }

    if (value == "shell-execute") {
        return LaunchActivationKind::
            ShellItem;
    }

    return fallback;
}

bool IsStrongInternalPackagedEntry(
    const PackagedVisibilityEvidence&
        evidence) noexcept {

    if (evidence.hidden) {
        return true;
    }

    // Windows exposes several shell activation surfaces through AppsFolder.
    // Treat an entry as internal only when Windows itself supplies more than
    // one strong "not a normal app-list entry" signal. A single prevent-pin
    // flag is not enough because legitimate utilities can use it; pairing it
    // with the Shell SYSTEM attribute is a stronger structural signal.
    return evidence.preventPinning &&
        evidence.system;
}

bool IsPackagedApplicationId(
    std::wstring_view target) noexcept {

    if (target.empty() ||
        HasUriScheme(target)) {
        return false;
    }

    const auto bang =
        target.find(L'!');

    return
        bang != std::wstring_view::npos &&
        bang > 0 &&
        bang + 1 < target.size() &&
        target.find_first_of(
            L"\\/") ==
            std::wstring_view::npos;
}

LaunchActivationKind
ActivationKindForCatalogTarget(
    std::wstring_view target) noexcept {

    return IsPackagedApplicationId(
               target)
        ? LaunchActivationKind::
              PackagedApplication
        : LaunchActivationKind::
              ShellItem;
}

std::wstring BuildCanonicalLaunchIdentity(
    LaunchActivationKind activation,
    std::wstring_view resolvedTarget,
    std::wstring_view arguments) {

    const std::wstring target =
        NormalizeTarget(
            resolvedTarget);

    if (target.empty()) {
        return {};
    }

    std::wstring identity;

    if (activation ==
        LaunchActivationKind::
            PackagedApplication) {
        identity = L"aumid:";
        identity += target;
        return identity;
    }

    if (HasUriScheme(target)) {
        identity = L"shell:";
    } else {
        identity = L"file:";
    }

    identity += target;

    const std::wstring normalizedArgs =
        Trim(arguments);

    if (!normalizedArgs.empty()) {
        identity += L"|args:";
        identity += normalizedArgs;
    }

    return identity;
}

} // namespace altrun
