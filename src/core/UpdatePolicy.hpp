#pragma once

#include <compare>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace altrun {

enum class UpdateChannel {
    Stable,
    Development,
};

struct UpdateAsset {
    std::string name;
    std::string sha256;
};

struct UpdateManifest {
    int schemaVersion{0};
    std::string version;
    std::string commit;
    bool prerelease{false};
    UpdateAsset x64;
    UpdateAsset arm64;
};

[[nodiscard]] const char*
UpdateChannelName(
    UpdateChannel channel);

[[nodiscard]] UpdateChannel
DefaultUpdateChannelForVersion(
    std::string_view version);

[[nodiscard]] std::optional<int>
CompareVersions(
    std::string_view left,
    std::string_view right);

[[nodiscard]] bool
IsUpdateVersionNewer(
    std::string_view current,
    std::string_view candidate);

[[nodiscard]] bool
IsTransientUpdateHttpStatus(
    std::uint32_t status);

// HTTP failures share the native-error slot with Win32 errors. They are
// encoded like winerror.h HTTP_E_STATUS_* values so a status such as 404 can
// never be mistaken for the Win32 error with the same number.
[[nodiscard]] std::uint32_t
UpdateHttpStatusError(
    std::uint32_t status);

// The HTTP status carried by `error`, or 0 when it is not an HTTP failure.
[[nodiscard]] std::uint32_t
UpdateHttpStatusFromError(
    std::uint32_t error);

[[nodiscard]] std::wstring
UpdateManifestUrl(
    UpdateChannel channel);

[[nodiscard]] std::wstring
UpdateAssetUrl(
    UpdateChannel channel,
    std::string_view assetName);

[[nodiscard]] bool
IsSafeUpdateAssetName(
    std::string_view assetName);

[[nodiscard]] bool
IsSha256HexString(
    std::string_view value);

} // namespace altrun
