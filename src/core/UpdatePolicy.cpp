#include "UpdatePolicy.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <string>
#include <vector>

namespace altrun {
namespace {

struct ParsedVersion {
    std::uint64_t major{0};
    std::uint64_t minor{0};
    std::uint64_t patch{0};
    std::vector<std::string>
        prerelease;
};

[[nodiscard]] bool
ParseUnsigned(
    std::string_view value,
    std::uint64_t& output) {
    if (value.empty()) {
        return false;
    }

    const auto result =
        std::from_chars(
            value.data(),
            value.data() +
                value.size(),
            output);

    return result.ec ==
               std::errc{} &&
        result.ptr ==
            value.data() +
                value.size();
}

[[nodiscard]]
std::optional<ParsedVersion>
ParseVersion(
    std::string_view value) {
    if (!value.empty() &&
        (value.front() == 'v' ||
         value.front() == 'V')) {
        value.remove_prefix(1);
    }

    ParsedVersion result;

    const auto dash =
        value.find('-');
    const auto main =
        value.substr(
            0,
            dash);
    const auto prerelease =
        dash ==
                std::string_view::npos
            ? std::string_view{}
            : value.substr(
                  dash + 1);

    const auto firstDot =
        main.find('.');
    const auto secondDot =
        firstDot ==
                std::string_view::npos
            ? std::string_view::npos
            : main.find(
                  '.',
                  firstDot + 1);

    if (firstDot ==
            std::string_view::npos ||
        secondDot ==
            std::string_view::npos ||
        main.find(
            '.',
            secondDot + 1) !=
            std::string_view::npos) {
        return std::nullopt;
    }

    if (!ParseUnsigned(
            main.substr(
                0,
                firstDot),
            result.major) ||
        !ParseUnsigned(
            main.substr(
                firstDot + 1,
                secondDot -
                    firstDot - 1),
            result.minor) ||
        !ParseUnsigned(
            main.substr(
                secondDot + 1),
            result.patch)) {
        return std::nullopt;
    }

    if (dash !=
        std::string_view::npos) {
        if (prerelease.empty()) {
            return std::nullopt;
        }

        std::size_t offset = 0;

        while (offset <=
               prerelease.size()) {
            const auto dot =
                prerelease.find(
                    '.',
                    offset);
            const auto part =
                dot ==
                        std::string_view::npos
                    ? prerelease.substr(
                          offset)
                    : prerelease.substr(
                          offset,
                          dot - offset);

            if (part.empty() ||
                !std::all_of(
                    part.begin(),
                    part.end(),
                    [](unsigned char c) {
                        return
                            std::isalnum(c) ||
                            c == '-';
                    })) {
                return std::nullopt;
            }

            result.prerelease
                .emplace_back(part);

            if (dot ==
                std::string_view::npos) {
                break;
            }

            offset = dot + 1;
        }
    }

    return result;
}

[[nodiscard]] bool
NumericIdentifier(
    std::string_view value,
    std::uint64_t& number) {
    return ParseUnsigned(
        value,
        number);
}

[[nodiscard]] int
CompareParsed(
    const ParsedVersion& left,
    const ParsedVersion& right) {
    const auto compareNumber =
        [](std::uint64_t a,
           std::uint64_t b) {
            return a < b
                ? -1
                : (a > b ? 1 : 0);
        };

    if (const int value =
            compareNumber(
                left.major,
                right.major);
        value != 0) {
        return value;
    }

    if (const int value =
            compareNumber(
                left.minor,
                right.minor);
        value != 0) {
        return value;
    }

    if (const int value =
            compareNumber(
                left.patch,
                right.patch);
        value != 0) {
        return value;
    }

    if (left.prerelease.empty() &&
        right.prerelease.empty()) {
        return 0;
    }

    if (left.prerelease.empty()) {
        return 1;
    }

    if (right.prerelease.empty()) {
        return -1;
    }

    const std::size_t count =
        std::min(
            left.prerelease.size(),
            right.prerelease.size());

    for (std::size_t i = 0;
         i < count;
         ++i) {
        const auto& a =
            left.prerelease[i];
        const auto& b =
            right.prerelease[i];

        if (a == b) {
            continue;
        }

        std::uint64_t aNumber = 0;
        std::uint64_t bNumber = 0;
        const bool aNumeric =
            NumericIdentifier(
                a,
                aNumber);
        const bool bNumeric =
            NumericIdentifier(
                b,
                bNumber);

        if (aNumeric &&
            bNumeric) {
            return aNumber <
                           bNumber
                ? -1
                : 1;
        }

        if (aNumeric !=
            bNumeric) {
            return aNumeric
                ? -1
                : 1;
        }

        return a < b
            ? -1
            : 1;
    }

    if (left.prerelease.size() ==
        right.prerelease.size()) {
        return 0;
    }

    return left.prerelease.size() <
                   right.prerelease.size()
        ? -1
        : 1;
}

[[nodiscard]] std::wstring
WideAscii(
    std::string_view value) {
    std::wstring result;
    result.reserve(value.size());

    for (const char c : value) {
        result.push_back(
            static_cast<unsigned char>(
                c));
    }

    return result;
}

} // namespace

const char*
UpdateChannelName(
    UpdateChannel channel) {
    return channel ==
            UpdateChannel::
                Development
        ? "development"
        : "stable";
}

UpdateChannel
DefaultUpdateChannelForVersion(
    std::string_view) {
    // Prerelease updates are always opt-in. First-run and reset defaults
    // stay on the stable channel regardless of the current build label.
    return UpdateChannel::Stable;
}

std::optional<int>
CompareVersions(
    std::string_view left,
    std::string_view right) {
    const auto a =
        ParseVersion(left);
    const auto b =
        ParseVersion(right);

    if (!a || !b) {
        return std::nullopt;
    }

    return CompareParsed(
        *a,
        *b);
}

bool IsUpdateVersionNewer(
    std::string_view current,
    std::string_view candidate) {
    const auto comparison =
        CompareVersions(
            candidate,
            current);

    return comparison &&
        *comparison > 0;
}

std::wstring
UpdateManifestUrl(
    UpdateChannel channel) {
    if (channel ==
        UpdateChannel::
            Development) {
        return
            L"https://github.com/Aspeternity/ALTRunNext/releases/download/dev-latest/update-manifest.json";
    }

    return
        L"https://github.com/Aspeternity/ALTRunNext/releases/latest/download/update-manifest.json";
}

std::wstring
UpdateAssetUrl(
    UpdateChannel channel,
    std::string_view assetName) {
    if (!IsSafeUpdateAssetName(
            assetName)) {
        return {};
    }

    std::wstring result =
        channel ==
                UpdateChannel::
                    Development
            ? L"https://github.com/Aspeternity/ALTRunNext/releases/download/dev-latest/"
            : L"https://github.com/Aspeternity/ALTRunNext/releases/latest/download/";

    result += WideAscii(
        assetName);

    return result;
}

bool IsSafeUpdateAssetName(
    std::string_view assetName) {
    if (assetName.empty() ||
        assetName.size() > 128 ||
        assetName.find('/') !=
            std::string_view::npos ||
        assetName.find('\\') !=
            std::string_view::npos ||
        assetName.find("..") !=
            std::string_view::npos) {
        return false;
    }

    return std::all_of(
        assetName.begin(),
        assetName.end(),
        [](unsigned char c) {
            return
                std::isalnum(c) ||
                c == '-' ||
                c == '_' ||
                c == '.';
        });
}

bool IsSha256HexString(
    std::string_view value) {
    return value.size() == 64 &&
        std::all_of(
            value.begin(),
            value.end(),
            [](unsigned char c) {
                return
                    std::isxdigit(c) !=
                    0;
            });
}

} // namespace altrun
