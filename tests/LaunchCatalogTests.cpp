#include "core/LaunchCatalog.hpp"

#include <cassert>
#include <iostream>

using namespace altrun;

int main() {
    assert(
        IsPackagedApplicationId(
            L"Microsoft.WindowsAlarms_8wekyb3d8bbwe!App"));
    assert(
        !IsPackagedApplicationId(
            L"ms-settings:display"));
    assert(
        !IsPackagedApplicationId(
            L"C:\\Apps\\Example.exe"));

    assert(
        ActivationKindForCatalogTarget(
            L"Microsoft.WindowsAlarms_8wekyb3d8bbwe!App") ==
        LaunchActivationKind::
            PackagedApplication);

    const auto startIdentity =
        BuildCanonicalLaunchIdentity(
            LaunchActivationKind::
                ShellItem,
            L"C:\\Program Files\\Google\\Chrome\\Application\\chrome.exe");

    const auto appPathIdentity =
        BuildCanonicalLaunchIdentity(
            LaunchActivationKind::
                ShellItem,
            L"c:/program files/google/chrome/application/CHROME.EXE");

    assert(!startIdentity.empty());
    assert(
        startIdentity ==
        appPathIdentity);

    const auto normalIdentity =
        BuildCanonicalLaunchIdentity(
            LaunchActivationKind::
                ShellItem,
            L"C:\\Apps\\Browser.exe");

    const auto specialIdentity =
        BuildCanonicalLaunchIdentity(
            LaunchActivationKind::
                ShellItem,
            L"C:\\Apps\\Browser.exe",
            L"--incognito");

    assert(
        normalIdentity !=
        specialIdentity);

    PackagedVisibilityEvidence visible;
    assert(
        !IsStrongInternalPackagedEntry(
            visible));

    PackagedVisibilityEvidence
        preventOnly;
    preventOnly.preventPinning = true;
    assert(
        !IsStrongInternalPackagedEntry(
            preventOnly));

    PackagedVisibilityEvidence
        internal;
    internal.preventPinning = true;
    internal.system = true;

    assert(
        IsStrongInternalPackagedEntry(
            internal));

    std::cout
        << "Launch catalog tests passed\n";
    return 0;
}
