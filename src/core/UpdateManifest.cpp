#include "UpdateManifest.hpp"

#include <algorithm>
#include <cctype>
#include <string>

#include <nlohmann/json.hpp>

namespace altrun {
namespace {

[[nodiscard]] std::string
LowerAscii(
    std::string value) {
    std::transform(
        value.begin(),
        value.end(),
        value.begin(),
        [](unsigned char c) {
            return static_cast<char>(
                std::tolower(c));
        });
    return value;
}

[[nodiscard]] bool
IsGitSha(
    std::string_view value) {
    return value.size() == 40 &&
        std::all_of(
            value.begin(),
            value.end(),
            [](unsigned char c) {
                return std::isxdigit(c) !=
                    0;
            });
}

[[nodiscard]] bool
ReadAsset(
    const nlohmann::json& root,
    const char* key,
    UpdateAsset& asset) {
    if (!root.contains("assets") ||
        !root["assets"].is_object() ||
        !root["assets"].contains(key) ||
        !root["assets"][key]
             .is_object()) {
        return false;
    }

    const auto& item =
        root["assets"][key];

    if (!item.contains("name") ||
        !item["name"].is_string() ||
        !item.contains("sha256") ||
        !item["sha256"]
             .is_string()) {
        return false;
    }

    asset.name =
        item["name"].get<
            std::string>();
    asset.sha256 =
        LowerAscii(
            item["sha256"].get<
                std::string>());

    return IsSafeUpdateAssetName(
               asset.name) &&
        IsSha256HexString(
            asset.sha256);
}

} // namespace

std::optional<UpdateManifest>
ParseUpdateManifest(
    std::string_view jsonText) {
    const auto root =
        nlohmann::json::parse(
            jsonText,
            nullptr,
            false);

    if (root.is_discarded() ||
        !root.is_object()) {
        return std::nullopt;
    }

    UpdateManifest manifest;

    try {
        manifest.schemaVersion =
            root.value(
                "schemaVersion",
                0);

        if (manifest.schemaVersion !=
            1 ||
            !root.contains("version") ||
            !root["version"].is_string() ||
            !root.contains("commit") ||
            !root["commit"].is_string()) {
            return std::nullopt;
        }

        manifest.version =
            root["version"].get<
                std::string>();
        manifest.commit =
            root["commit"].get<
                std::string>();
        manifest.prerelease =
            root.value(
                "prerelease",
                false);

        if (!CompareVersions(
                manifest.version,
                manifest.version) ||
            !IsGitSha(
                manifest.commit) ||
            !ReadAsset(
                root,
                "x64",
                manifest.x64) ||
            !ReadAsset(
                root,
                "ARM64",
                manifest.arm64)) {
            return std::nullopt;
        }

        return manifest;
    } catch (...) {
        return std::nullopt;
    }
}

} // namespace altrun
