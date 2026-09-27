#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace altrun {

enum class EverythingPackageArchitecture {
    X64,
    Arm64,
};

struct EverythingPackageSpec {
    std::wstring version;
    std::wstring fileName;
    std::wstring downloadUrl;
    std::wstring checksumManifestUrl;
};

struct EverythingArchiveNames {
    std::wstring downloadFileName;
    std::wstring verifiedZipFileName;
};

[[nodiscard]] std::wstring_view
PinnedManagedEverythingVersion() noexcept;

[[nodiscard]] std::wstring_view
EverythingStableUpdateMetadataUrl() noexcept;

[[nodiscard]] EverythingPackageSpec
ManagedEverythingPackage(
    EverythingPackageArchitecture architecture,
    std::wstring_view version = {});

[[nodiscard]] std::optional<std::wstring>
ParseEverythingStableUpdateVersion(
    std::string_view updateIni);

[[nodiscard]] int
CompareEverythingVersions(
    std::wstring_view left,
    std::wstring_view right) noexcept;

[[nodiscard]] EverythingArchiveNames
ManagedEverythingArchiveNames(
    const EverythingPackageSpec& package);

[[nodiscard]] bool
IsSha256Hex(
    std::string_view value);

[[nodiscard]] std::optional<std::string>
FindSha256ForFile(
    std::string_view manifest,
    std::string_view fileName);

[[nodiscard]] std::string
ApplyManagedEverythingIniPolicy(
    std::string_view existing,
    bool showTrayIcon = false);

[[nodiscard]] std::wstring
ExtractEverythingServiceExecutable(
    std::wstring_view binaryPath);

} // namespace altrun
