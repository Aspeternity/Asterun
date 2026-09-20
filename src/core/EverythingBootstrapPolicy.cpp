#include "EverythingBootstrapPolicy.hpp"

#include <algorithm>
#include <cctype>
#include <string>

namespace altrun {
namespace {

constexpr std::wstring_view kVersion =
    L"1.4.1.1032";
constexpr std::wstring_view kBaseUrl =
    L"https://www.voidtools.com/";

[[nodiscard]] std::string
LowerAscii(
    std::string_view value) {
    std::string result(value);

    std::transform(
        result.begin(),
        result.end(),
        result.begin(),
        [](unsigned char c) {
            return static_cast<char>(
                std::tolower(c));
        });

    return result;
}

[[nodiscard]] std::string_view
TrimAscii(
    std::string_view value) {
    while (!value.empty() &&
           std::isspace(
               static_cast<unsigned char>(
                   value.front()))) {
        value.remove_prefix(1);
    }

    while (!value.empty() &&
           std::isspace(
               static_cast<unsigned char>(
                   value.back()))) {
        value.remove_suffix(1);
    }

    return value;
}

} // namespace

EverythingPackageSpec
ManagedEverythingPackage(
    EverythingPackageArchitecture architecture) {
    EverythingPackageSpec spec;
    spec.version =
        std::wstring(kVersion);

    spec.fileName =
        L"Everything-" +
        spec.version +
        (architecture ==
                 EverythingPackageArchitecture::
                     Arm64
             ? L".ARM64.zip"
             : L".x64.zip");

    spec.downloadUrl =
        std::wstring(kBaseUrl) +
        spec.fileName;

    spec.checksumManifestUrl =
        std::wstring(kBaseUrl) +
        L"Everything-" +
        spec.version +
        L".sha256";

    return spec;
}

bool IsSha256Hex(
    std::string_view value) {
    if (value.size() != 64) {
        return false;
    }

    return std::all_of(
        value.begin(),
        value.end(),
        [](unsigned char c) {
            return std::isxdigit(c) != 0;
        });
}

std::optional<std::string>
FindSha256ForFile(
    std::string_view manifest,
    std::string_view fileName) {
    const std::string wanted =
        LowerAscii(fileName);

    std::size_t offset = 0;

    while (offset <= manifest.size()) {
        const auto newline =
            manifest.find('\n', offset);

        std::string_view line =
            newline ==
                    std::string_view::npos
                ? manifest.substr(offset)
                : manifest.substr(
                      offset,
                      newline - offset);

        if (!line.empty() &&
            line.back() == '\r') {
            line.remove_suffix(1);
        }

        line = TrimAscii(line);

        if (!line.empty()) {
            const auto separator =
                line.find_first_of(
                    " \t");

            if (separator !=
                std::string_view::npos) {
                std::string_view hash =
                    line.substr(
                        0,
                        separator);
                std::string_view name =
                    TrimAscii(
                        line.substr(
                            separator));

                if (!name.empty() &&
                    name.front() == '*') {
                    name.remove_prefix(1);
                    name = TrimAscii(name);
                }

                if (IsSha256Hex(hash) &&
                    LowerAscii(name) ==
                        wanted) {
                    return LowerAscii(hash);
                }
            }
        }

        if (newline ==
            std::string_view::npos) {
            break;
        }

        offset = newline + 1;
    }

    return std::nullopt;
}

} // namespace altrun
