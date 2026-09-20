#include "core/EverythingBootstrapPolicy.hpp"

#include <cassert>
#include <iostream>

using namespace altrun;

int main() {
    {
        const auto x64 =
            ManagedEverythingPackage(
                EverythingPackageArchitecture::
                    X64);

        assert(
            x64.version ==
            L"1.4.1.1032");
        assert(
            x64.fileName ==
            L"Everything-1.4.1.1032.x64.zip");
        assert(
            x64.downloadUrl ==
            L"https://www.voidtools.com/Everything-1.4.1.1032.x64.zip");
        assert(
            x64.checksumManifestUrl ==
            L"https://www.voidtools.com/Everything-1.4.1.1032.sha256");
        assert(
            x64.fileName.find(L"Lite") ==
            std::wstring::npos);

        const auto names =
            ManagedEverythingArchiveNames(
                x64);

        assert(
            names.downloadFileName ==
            L"Everything-1.4.1.1032.x64.zip.download");
        assert(
            names.downloadFileName.ends_with(
                L".download"));
        assert(
            names.verifiedZipFileName ==
            L"Everything-1.4.1.1032.x64.zip");
        assert(
            names.verifiedZipFileName.ends_with(
                L".zip"));
        assert(
            !names.verifiedZipFileName
                 .ends_with(
                     L".download"));
    }

    {
        const auto arm64 =
            ManagedEverythingPackage(
                EverythingPackageArchitecture::
                    Arm64);

        assert(
            arm64.fileName ==
            L"Everything-1.4.1.1032.ARM64.zip");
        assert(
            arm64.fileName.find(L"Lite") ==
            std::wstring::npos);
    }

    {
        const std::string x64Hash(
            64,
            'a');
        const std::string armHash(
            64,
            'B');

        const std::string manifest =
            x64Hash +
            "  Everything-1.4.1.1032.x64.zip\r\n" +
            armHash +
            " *Everything-1.4.1.1032.ARM64.zip\n";

        const auto x64 =
            FindSha256ForFile(
                manifest,
                "Everything-1.4.1.1032.x64.zip");
        assert(x64.has_value());
        assert(*x64 == x64Hash);

        const auto arm =
            FindSha256ForFile(
                manifest,
                "everything-1.4.1.1032.arm64.zip");
        assert(arm.has_value());
        assert(
            *arm ==
            std::string(
                64,
                'b'));

        assert(
            !FindSha256ForFile(
                 manifest,
                 "Everything-1.4.1.1032.x64.Lite.zip")
                 .has_value());
    }

    assert(
        IsSha256Hex(
            "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"));
    assert(
        !IsSha256Hex(
            "xyz"));
    assert(
        !IsSha256Hex(
            "0123456789abcdef"));

    std::cout
        << "Everything bootstrap policy tests passed\n";
    return 0;
}
