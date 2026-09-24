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
            "0.8.0-alpha.2.14"));
    assert(
        IsUpdateVersionNewer(
            "0.8.0-alpha.2.14",
            "0.8.0-alpha.3"));
    assert(
        IsUpdateVersionNewer(
            "0.8.0-alpha.3",
            "0.8.0-alpha.3.1"));
    assert(
        IsUpdateVersionNewer(
            "0.8.0-alpha.3.1",
            "0.8.0-alpha.3.2"));
    assert(
        IsUpdateVersionNewer(
            "0.8.0-alpha.3.2",
            "0.8.0-alpha.3.3"));
    assert(
        IsUpdateVersionNewer(
            "0.8.0-alpha.3.3",
            "0.8.0-alpha.3.4"));
    assert(
        IsUpdateVersionNewer(
            "0.8.0-alpha.3.4",
            "0.8.0-alpha.3.5"));
    assert(
        IsUpdateVersionNewer(
            "0.8.0-alpha.3.5",
            "0.8.0-alpha.3.6"));
    assert(
        IsUpdateVersionNewer(
            "0.8.0-alpha.3.6",
            "0.8.0-alpha.3.7"));
    assert(
        IsUpdateVersionNewer(
            "0.8.0-alpha.3.7",
            "0.8.0-alpha.3.8"));
    assert(
        IsUpdateVersionNewer(
            "0.8.0-alpha.3.8",
            "0.8.0-alpha.3.9"));
    assert(
        IsUpdateVersionNewer(
            "0.8.0-alpha.3.9",
            "0.8.0-alpha.3.10"));
    assert(
        IsUpdateVersionNewer(
            "0.8.0-alpha.3.10",
            "0.8.0-alpha.3.11"));
    assert(
        IsUpdateVersionNewer(
            "0.8.0-alpha.3.11",
            "0.8.0-alpha.3.12"));
    assert(
        IsUpdateVersionNewer(
            "0.8.0-alpha.3.12",
            "0.8.0-alpha.3.13"));
    assert(
        IsUpdateVersionNewer(
            "0.8.0-alpha.3.22",
            "0.8.0-alpha.3.23"));
    assert(
        IsUpdateVersionNewer(
            "0.8.0-alpha.3.23",
            "0.8.0-alpha.3.24"));
    assert(
        IsUpdateVersionNewer(
            "0.8.0-alpha.3.24",
            "0.8.0-alpha.3.25"));
    assert(
        IsUpdateVersionNewer(
            "0.8.0-alpha.3.25",
            "0.8.0-alpha.3.26"));
    assert(
        IsUpdateVersionNewer(
            "0.8.0-alpha.3.26",
            "0.8.0-alpha.3.27"));
    assert(
        IsUpdateVersionNewer(
            "0.8.0-alpha.3.27",
            "0.8.0-alpha.3.28"));
    assert(
        IsUpdateVersionNewer(
            "0.8.0-alpha.3.28",
            "0.8.0-alpha.3.29"));
    assert(
        IsUpdateVersionNewer(
            "0.8.0-alpha.3.29",
            "0.8.0-alpha.3.30"));
    assert(
        IsUpdateVersionNewer(
            "0.8.0-alpha.3.30",
            "0.8.0-alpha.3.31"));
    assert(
        IsUpdateVersionNewer(
            "0.8.0-alpha.3.31",
            "0.8.0-alpha.3.32"));
    assert(
        IsUpdateVersionNewer(
            "0.8.0-alpha.3.32",
            "0.8.0-alpha.3.33"));
    assert(
        IsUpdateVersionNewer(
            "0.8.0-alpha.3.33",
            "0.8.0-alpha.3.34"));
    assert(
        IsUpdateVersionNewer(
            "0.8.0-alpha.3.34",
            "0.8.0-alpha.3.35"));
    assert(
        IsUpdateVersionNewer(
            "0.8.0-alpha.3.35",
            "0.8.0-alpha.3.36"));
    assert(
        IsUpdateVersionNewer(
            "0.8.0-alpha.3.36",
            "0.8.0-alpha.3.37"));
    assert(
        IsUpdateVersionNewer(
            "0.8.0-alpha.3.37",
            "0.8.0-alpha.3.38"));
    assert(
        IsUpdateVersionNewer(
            "0.8.0-alpha.3.38",
            "0.8.0-alpha.3.39"));
    assert(
        IsUpdateVersionNewer(
            "0.8.0-alpha.3.39",
            "0.8.0-alpha.3.40"));
    assert(
        IsUpdateVersionNewer(
            "0.8.0-alpha.3.40",
            "0.8.0-alpha.4.1"));
    assert(
        IsUpdateVersionNewer(
            "0.8.0-alpha.4.1",
            "0.8.0-alpha.4.2"));
    assert(
        !IsUpdateVersionNewer(
            "0.8.0-alpha.4.2",
            "0.8.0-alpha.4.1"));
    assert(
        IsUpdateVersionNewer(
            "0.8.0-alpha.4.2",
            "0.8.0-alpha.4.3"));
    assert(
        !IsUpdateVersionNewer(
            "0.8.0-alpha.4.3",
            "0.8.0-alpha.4.2"));
    assert(
        IsUpdateVersionNewer(
            "0.8.0-alpha.4.3",
            "0.8.0-alpha.4.4"));
    assert(
        !IsUpdateVersionNewer(
            "0.8.0-alpha.4.4",
            "0.8.0-alpha.4.3"));
    assert(
        IsUpdateVersionNewer(
            "0.8.0-alpha.4.4",
            "0.8.0-alpha.4.5"));
    assert(
        !IsUpdateVersionNewer(
            "0.8.0-alpha.4.5",
            "0.8.0-alpha.4.4"));
    assert(
        IsUpdateVersionNewer(
            "0.8.0-alpha.4.5",
            "0.8.0-alpha.4.6"));
    assert(
        !IsUpdateVersionNewer(
            "0.8.0-alpha.4.6",
            "0.8.0-alpha.4.5"));
    assert(
        IsUpdateVersionNewer(
            "0.8.0-alpha.4.6",
            "0.8.0-alpha.4.7"));
    assert(
        !IsUpdateVersionNewer(
            "0.8.0-alpha.4.7",
            "0.8.0-alpha.4.6"));
    assert(
        IsUpdateVersionNewer(
            "0.8.0-alpha.4.7",
            "0.8.0-alpha.4.8"));
    assert(
        !IsUpdateVersionNewer(
            "0.8.0-alpha.4.8",
            "0.8.0-alpha.4.7"));
    assert(
        IsUpdateVersionNewer(
            "0.8.0-alpha.4.8",
            "0.8.0-alpha.4.9"));
    assert(
        !IsUpdateVersionNewer(
            "0.8.0-alpha.4.9",
            "0.8.0-alpha.4.8"));

    assert(
        IsUpdateVersionNewer(
            "0.8.0-alpha.4.9",
            "0.8.0-alpha.5.1"));
    assert(
        !IsUpdateVersionNewer(
            "0.8.0-alpha.5.1",
            "0.8.0-alpha.4.9"));

    assert(
        IsUpdateVersionNewer(
            "0.8.0-alpha.5.1",
            "0.8.0-alpha.5.2"));
    assert(
        !IsUpdateVersionNewer(
            "0.8.0-alpha.5.2",
            "0.8.0-alpha.5.1"));

    assert(
        IsUpdateVersionNewer(
            "0.8.0-alpha.5.2",
            "0.8.0-alpha.5.3"));
    assert(
        !IsUpdateVersionNewer(
            "0.8.0-alpha.5.3",
            "0.8.0-alpha.5.2"));

    assert(
        IsUpdateVersionNewer(
            "0.8.0-alpha.5.3",
            "0.8.0-alpha.5.4"));
    assert(
        !IsUpdateVersionNewer(
            "0.8.0-alpha.5.4",
            "0.8.0-alpha.5.3"));

    assert(
        IsUpdateVersionNewer(
            "0.8.0-alpha.5.4",
            "0.8.0-alpha.5.5"));
    assert(
        !IsUpdateVersionNewer(
            "0.8.0-alpha.5.5",
            "0.8.0-alpha.5.4"));

    assert(
        IsUpdateVersionNewer(
            "0.8.0-alpha.5.5",
            "0.8.0-alpha.5.6"));
    assert(
        !IsUpdateVersionNewer(
            "0.8.0-alpha.5.6",
            "0.8.0-alpha.5.5"));

    assert(
        IsUpdateVersionNewer(
            "0.8.0-alpha.5.6",
            "0.8.0-alpha.5.7"));
    assert(
        !IsUpdateVersionNewer(
            "0.8.0-alpha.5.7",
            "0.8.0-alpha.5.6"));

    assert(
        IsUpdateVersionNewer(
            "0.8.0-alpha.5.7",
            "0.8.0-alpha.5.8"));
    assert(
        !IsUpdateVersionNewer(
            "0.8.0-alpha.5.8",
            "0.8.0-alpha.5.7"));
    assert(
        IsUpdateVersionNewer(
            "0.8.0-alpha.5.8",
            "0.8.0-alpha.5.9"));
    assert(
        !IsUpdateVersionNewer(
            "0.8.0-alpha.5.9",
            "0.8.0-alpha.5.8"));
    assert(
        IsUpdateVersionNewer(
            "0.8.0-alpha.5.9",
            "0.8.0-alpha.5.10"));
    assert(
        !IsUpdateVersionNewer(
            "0.8.0-alpha.5.10",
            "0.8.0-alpha.5.9"));
    assert(
        IsUpdateVersionNewer(
            "0.8.0-alpha.5.10",
            "0.8.0-alpha.5.11"));
    assert(
        !IsUpdateVersionNewer(
            "0.8.0-alpha.5.11",
            "0.8.0-alpha.5.10"));
    assert(
        IsUpdateVersionNewer(
            "0.8.0-alpha.5.11",
            "0.8.0-alpha.5.12"));
    assert(
        !IsUpdateVersionNewer(
            "0.8.0-alpha.5.12",
            "0.8.0-alpha.5.11"));
    assert(
        IsUpdateVersionNewer(
            "0.8.0-alpha.5.12",
            "0.8.0-alpha.5.13"));
    assert(
        !IsUpdateVersionNewer(
            "0.8.0-alpha.5.13",
            "0.8.0-alpha.5.12"));
    assert(
        IsUpdateVersionNewer(
            "0.8.0-alpha.5.13",
            "0.8.0-alpha.5.14"));
    assert(
        !IsUpdateVersionNewer(
            "0.8.0-alpha.5.14",
            "0.8.0-alpha.5.13"));
    assert(
        IsUpdateVersionNewer(
            "0.8.0-alpha.5.14",
            "0.8.0-alpha.5.15"));
    assert(
        !IsUpdateVersionNewer(
            "0.8.0-alpha.5.15",
            "0.8.0-alpha.5.14"));
    assert(
        IsUpdateVersionNewer(
            "0.8.0-alpha.5.15",
            "0.8.0-alpha.5.16"));
    assert(
        !IsUpdateVersionNewer(
            "0.8.0-alpha.5.16",
            "0.8.0-alpha.5.15"));
    assert(
        IsUpdateVersionNewer(
            "0.8.0-alpha.5.16",
            "0.8.0-alpha.5.17"));
    assert(
        !IsUpdateVersionNewer(
            "0.8.0-alpha.5.17",
            "0.8.0-alpha.5.16"));
    assert(
        IsUpdateVersionNewer(
            "0.8.0-alpha.5.17",
            "0.8.0-alpha.5.18"));
    assert(
        !IsUpdateVersionNewer(
            "0.8.0-alpha.5.18",
            "0.8.0-alpha.5.17"));
    assert(
        IsUpdateVersionNewer(
            "0.8.0-alpha.5.18",
            "0.8.0-alpha.5.19"));
    assert(
        !IsUpdateVersionNewer(
            "0.8.0-alpha.5.19",
            "0.8.0-alpha.5.18"));
    assert(
        IsUpdateVersionNewer(
            "0.8.0-alpha.5.19",
            "0.8.0-alpha.5.20"));
    assert(
        !IsUpdateVersionNewer(
            "0.8.0-alpha.5.20",
            "0.8.0-alpha.5.19"));
    assert(
        IsUpdateVersionNewer(
            "0.8.0-alpha.5.20",
            "0.8.0-alpha.5.21"));
    assert(
        !IsUpdateVersionNewer(
            "0.8.0-alpha.5.21",
            "0.8.0-alpha.5.20"));
    assert(
        IsUpdateVersionNewer(
            "0.8.0-alpha.5.21",
            "0.8.0-alpha.5.22"));
    assert(
        !IsUpdateVersionNewer(
            "0.8.0-alpha.5.22",
            "0.8.0-alpha.5.21"));
    assert(
        IsUpdateVersionNewer(
            "0.8.0-alpha.5.22",
            "0.8.0-alpha.5.23"));
    assert(
        !IsUpdateVersionNewer(
            "0.8.0-alpha.5.23",
            "0.8.0-alpha.5.22"));

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
            "0.8.0-alpha.4.1") ==
        UpdateChannel::Stable);
    assert(
        DefaultUpdateChannelForVersion(
            "0.8.0-alpha.4.2") ==
        UpdateChannel::Stable);
    assert(
        DefaultUpdateChannelForVersion(
            "0.8.0-alpha.4.3") ==
        UpdateChannel::Stable);
    assert(
        DefaultUpdateChannelForVersion(
            "0.8.0-alpha.4.4") ==
        UpdateChannel::Stable);
    assert(
        DefaultUpdateChannelForVersion(
            "0.8.0-alpha.4.5") ==
        UpdateChannel::Stable);
    assert(
        DefaultUpdateChannelForVersion(
            "0.8.0-alpha.4.6") ==
        UpdateChannel::Stable);
    assert(
        DefaultUpdateChannelForVersion(
            "0.8.0-alpha.4.7") ==
        UpdateChannel::Stable);
    assert(
        DefaultUpdateChannelForVersion(
            "0.8.0-alpha.4.8") ==
        UpdateChannel::Stable);
    assert(
        DefaultUpdateChannelForVersion(
            "0.8.0-alpha.4.9") ==
        UpdateChannel::Stable);

    assert(
        DefaultUpdateChannelForVersion(
            "0.8.0-alpha.5.1") ==
        UpdateChannel::Stable);

    assert(
        DefaultUpdateChannelForVersion(
            "0.8.0-alpha.5.2") ==
        UpdateChannel::Stable);
    assert(
        DefaultUpdateChannelForVersion(
            "0.8.0-alpha.5.3") ==
        UpdateChannel::Stable);
    assert(
        DefaultUpdateChannelForVersion(
            "0.8.0-alpha.5.4") ==
        UpdateChannel::Stable);
    assert(
        DefaultUpdateChannelForVersion(
            "0.8.0-alpha.5.5") ==
        UpdateChannel::Stable);
    assert(
        DefaultUpdateChannelForVersion(
            "0.8.0-alpha.5.6") ==
        UpdateChannel::Stable);
    assert(
        DefaultUpdateChannelForVersion(
            "0.8.0-alpha.5.7") ==
        UpdateChannel::Stable);
    assert(
        DefaultUpdateChannelForVersion(
            "0.8.0-alpha.5.8") ==
        UpdateChannel::Stable);
    assert(
        DefaultUpdateChannelForVersion(
            "0.8.0-alpha.5.9") ==
        UpdateChannel::Stable);
    assert(
        DefaultUpdateChannelForVersion(
            "0.8.0-alpha.5.10") ==
        UpdateChannel::Stable);
    assert(
        DefaultUpdateChannelForVersion(
            "0.8.0-alpha.5.11") ==
        UpdateChannel::Stable);
    assert(
        DefaultUpdateChannelForVersion(
            "0.8.0-alpha.5.12") ==
        UpdateChannel::Stable);
    assert(
        DefaultUpdateChannelForVersion(
            "0.8.0-alpha.5.13") ==
        UpdateChannel::Stable);
    assert(
        DefaultUpdateChannelForVersion(
            "0.8.0-alpha.5.14") ==
        UpdateChannel::Stable);
    assert(
        DefaultUpdateChannelForVersion(
            "0.8.0-alpha.5.15") ==
        UpdateChannel::Stable);
    assert(
        DefaultUpdateChannelForVersion(
            "0.8.0-alpha.5.16") ==
        UpdateChannel::Stable);
    assert(
        DefaultUpdateChannelForVersion(
            "0.8.0-alpha.5.17") ==
        UpdateChannel::Stable);
    assert(
        DefaultUpdateChannelForVersion(
            "0.8.0-alpha.5.18") ==
        UpdateChannel::Stable);
    assert(
        DefaultUpdateChannelForVersion(
            "0.8.0-alpha.5.19") ==
        UpdateChannel::Stable);
    assert(
        DefaultUpdateChannelForVersion(
            "0.8.0-alpha.5.20") ==
        UpdateChannel::Stable);
    assert(
        DefaultUpdateChannelForVersion(
            "0.8.0-alpha.5.21") ==
        UpdateChannel::Stable);
    assert(
        DefaultUpdateChannelForVersion(
            "0.8.0-alpha.5.22") ==
        UpdateChannel::Stable);
    assert(
        DefaultUpdateChannelForVersion(
            "0.8.0-alpha.5.23") ==
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
