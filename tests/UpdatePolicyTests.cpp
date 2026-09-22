#include "core/UpdateManifest.hpp"
#include "core/UpdatePolicy.hpp"

#include <cassert>
#include <iostream>
#include <string>

using namespace altrun;

int main() {
    assert(
        IsUpdateVersionNewer(
            "0.7.0-alpha.8.4",
            "0.7.0-alpha.9"));
    assert(
        IsUpdateVersionNewer(
            "0.7.0-alpha.9",
            "0.7.0-beta.1"));
    assert(
        IsUpdateVersionNewer(
            "0.7.0-alpha.9.4",
            "0.7.0-beta.1"));
    assert(
        IsUpdateVersionNewer(
            "0.7.0-beta.1",
            "0.7.0-beta.2"));
    assert(
        IsUpdateVersionNewer(
            "0.7.0-beta.2",
            "0.7.0-beta.3"));
    assert(
        IsUpdateVersionNewer(
            "0.7.0-beta.3",
            "0.7.0-beta.4"));
    assert(
        IsUpdateVersionNewer(
            "0.7.0-beta.4",
            "0.7.0-beta.5"));
    assert(
        IsUpdateVersionNewer(
            "0.7.0-beta.5",
            "0.7.0-beta.6"));
    assert(
        IsUpdateVersionNewer(
            "0.7.0-beta.6",
            "0.7.0-beta.7"));
    assert(
        IsUpdateVersionNewer(
            "0.7.0-beta.7",
            "0.7.0-beta.8"));
    assert(
        IsUpdateVersionNewer(
            "0.7.0-beta.8",
            "0.7.0-beta.9"));
    assert(
        IsUpdateVersionNewer(
            "0.7.0-beta.9",
            "0.7.0-beta.10"));
    assert(
        IsUpdateVersionNewer(
            "0.7.0-beta.10",
            "0.7.0-beta.11"));
    assert(
        IsUpdateVersionNewer(
            "0.7.0-beta.11",
            "0.7.0-beta.12"));
    assert(
        IsUpdateVersionNewer(
            "0.7.0-beta.12",
            "0.7.0-rc.1"));
    assert(
        !IsUpdateVersionNewer(
            "0.7.0-rc.1",
            "0.7.0-beta.12"));
    assert(
        !IsUpdateVersionNewer(
            "0.7.0-beta.12",
            "0.7.0-beta.11"));
    assert(
        IsUpdateVersionNewer(
            "0.7.0-beta.1",
            "0.7.0-rc.1"));
    assert(
        IsUpdateVersionNewer(
            "0.7.0-beta.2",
            "0.7.0-rc.1"));
    assert(
        IsUpdateVersionNewer(
            "0.7.0-rc.1",
            "0.7.0"));
    assert(
        !IsUpdateVersionNewer(
            "0.7.0",
            "0.7.0-rc.99"));
    assert(
        !IsUpdateVersionNewer(
            "0.7.0-beta.1",
            "0.7.0-alpha.9.4"));
    assert(
        !IsUpdateVersionNewer(
            "0.7.0-beta.1",
            "0.6.0"));
    assert(
        IsUpdateVersionNewer(
            "0.7.0",
            "0.8.0-alpha.1"));
    assert(
        !IsUpdateVersionNewer(
            "0.8.0-alpha.1",
            "0.7.0"));
    assert(
        IsUpdateVersionNewer(
            "0.8.0-alpha.1",
            "0.8.0-alpha.2"));
    assert(
        !IsUpdateVersionNewer(
            "0.8.0-alpha.2",
            "0.8.0-alpha.1"));
    assert(
        IsUpdateVersionNewer(
            "0.8.0-alpha.2",
            "0.8.0-alpha.2.1"));
    assert(
        !IsUpdateVersionNewer(
            "0.8.0-alpha.2.1",
            "0.8.0-alpha.2"));
    assert(
        IsUpdateVersionNewer(
            "0.8.0-alpha.2.1",
            "0.8.0-alpha.2.2"));
    assert(
        !IsUpdateVersionNewer(
            "0.8.0-alpha.2.2",
            "0.8.0-alpha.2.1"));
    assert(
        IsUpdateVersionNewer(
            "0.8.0-alpha.2.2",
            "0.8.0-alpha.2.3"));
    assert(
        !IsUpdateVersionNewer(
            "0.8.0-alpha.2.3",
            "0.8.0-alpha.2.2"));
    assert(
        IsUpdateVersionNewer(
            "0.8.0-alpha.2.3",
            "0.8.0-alpha.2.4"));
    assert(
        !IsUpdateVersionNewer(
            "0.8.0-alpha.2.4",
            "0.8.0-alpha.2.3"));
    assert(
        IsUpdateVersionNewer(
            "0.8.0-alpha.2.4",
            "0.8.0-alpha.2.5"));
    assert(
        !IsUpdateVersionNewer(
            "0.8.0-alpha.2.5",
            "0.8.0-alpha.2.4"));
    assert(
        IsUpdateVersionNewer(
            "0.8.0-alpha.2.5",
            "0.8.0-alpha.2.6"));
    assert(
        !IsUpdateVersionNewer(
            "0.8.0-alpha.2.6",
            "0.8.0-alpha.2.5"));
    assert(
        IsUpdateVersionNewer(
            "0.8.0-alpha.2.6",
            "0.8.0-alpha.2.7"));
    assert(
        IsUpdateVersionNewer(
            "0.8.0-alpha.2.7",
            "0.8.0-alpha.2.8"));
    assert(
        IsUpdateVersionNewer(
            "0.8.0-alpha.2.8",
            "0.8.0-alpha.2.9"));
    assert(
        IsUpdateVersionNewer(
            "0.8.0-alpha.2.9",
            "0.8.0-alpha.2.10"));
    assert(
        IsUpdateVersionNewer(
            "0.8.0-alpha.2.10",
            "0.8.0-alpha.2.11"));
    assert(
        IsUpdateVersionNewer(
            "0.8.0-alpha.2.11",
            "0.8.0-alpha.2.12"));
    assert(
        IsUpdateVersionNewer(
            "0.8.0-alpha.2.12",
            "0.8.0-alpha.2.13"));
    assert(
        IsUpdateVersionNewer(
            "0.8.0-alpha.2.13",
            "0.8.0-alpha.3"));

    assert(
        !CompareVersions(
            "bad",
            "0.7.0"));

    assert(
        DefaultUpdateChannelForVersion(
            "0.7.0-alpha.9") ==
        UpdateChannel::Stable);
    assert(
        DefaultUpdateChannelForVersion(
            "0.7.0-beta.12") ==
        UpdateChannel::Stable);
    assert(
        DefaultUpdateChannelForVersion(
            "0.7.0-rc.1") ==
        UpdateChannel::Stable);
    assert(
        DefaultUpdateChannelForVersion(
            "0.7.0") ==
        UpdateChannel::Stable);
    assert(
        DefaultUpdateChannelForVersion(
            "0.8.0-alpha.2.13") ==
        UpdateChannel::Stable);

    assert(
        UpdateManifestUrl(
            UpdateChannel::
                Development)
            .find(L"dev-latest") !=
        std::wstring::npos);
    assert(
        UpdateManifestUrl(
            UpdateChannel::Stable)
            .find(L"/latest/") !=
        std::wstring::npos);

    assert(
        IsSafeUpdateAssetName(
            "ALTRunNext-x64.zip"));
    assert(
        !IsSafeUpdateAssetName(
            "../ALTRunNext.zip"));
    assert(
        !IsSafeUpdateAssetName(
            "folder/file.zip"));

    const std::string hash(
        64,
        'a');

    const std::string json =
        "{"
        "\"schemaVersion\":1,"
        "\"version\":\"0.7.0-beta.1\","
        "\"commit\":\"0123456789abcdef0123456789abcdef01234567\","
        "\"prerelease\":true,"
        "\"assets\":{"
        "\"x64\":{"
        "\"name\":\"ALTRunNext-x64.zip\","
        "\"sha256\":\"" +
        hash +
        "\"},"
        "\"ARM64\":{"
        "\"name\":\"ALTRunNext-ARM64.zip\","
        "\"sha256\":\"" +
        hash +
        "\"}"
        "}"
        "}";

    const auto manifest =
        ParseUpdateManifest(json);

    assert(manifest);
    assert(
        manifest->version ==
        "0.7.0-beta.1");
    assert(manifest->prerelease);
    assert(
        manifest->x64.name ==
        "ALTRunNext-x64.zip");
    assert(
        manifest->arm64.sha256 ==
        hash);

    const auto invalid =
        ParseUpdateManifest(
            "{"
            "\"schemaVersion\":1,"
            "\"version\":\"0.7.0-beta.1\","
            "\"commit\":\"0123456789abcdef0123456789abcdef01234567\","
            "\"assets\":{"
            "\"x64\":{"
            "\"name\":\"../bad.zip\","
            "\"sha256\":\"" +
            hash +
            "\"},"
            "\"ARM64\":{"
            "\"name\":\"ALTRunNext-ARM64.zip\","
            "\"sha256\":\"" +
            hash +
            "\"}"
            "}"
            "}");

    assert(!invalid);

    const std::string badCommitJson =
        "{"
        "\"schemaVersion\":1,"
        "\"version\":\"0.7.0-beta.1\","
        "\"commit\":\"short\","
        "\"assets\":{"
        "\"x64\":{"
        "\"name\":\"ALTRunNext-x64.zip\","
        "\"sha256\":\"" +
        hash +
        "\"},"
        "\"ARM64\":{"
        "\"name\":\"ALTRunNext-ARM64.zip\","
        "\"sha256\":\"" +
        hash +
        "\"}"
        "}"
        "}";

    assert(
        !ParseUpdateManifest(
            badCommitJson));

    std::cout
        << "Update policy tests passed\n";
    return 0;
}
