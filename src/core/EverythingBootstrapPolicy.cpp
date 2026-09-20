#include "EverythingBootstrapPolicy.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <string>
#include <vector>

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

EverythingArchiveNames
ManagedEverythingArchiveNames(
    const EverythingPackageSpec& package) {
    EverythingArchiveNames names;
    names.downloadFileName =
        package.fileName +
        L".download";
    names.verifiedZipFileName =
        package.fileName;
    return names;
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

std::string
ApplyManagedEverythingIniPolicy(
    std::string_view existing) {
    struct RequiredValue {
        std::string_view key;
        std::string_view value;
    };

    constexpr std::array<
        RequiredValue,
        6>
        required{{
            {"app_data", "0"},
            {"run_as_admin", "0"},
            {"run_in_background", "1"},
            {"show_tray_icon", "0"},
            {"check_for_updates_on_startup", "0"},
            {"ipc", "1"},
        }};

    auto lowerTrim =
        [](std::string_view value) {
            value = TrimAscii(value);
            return LowerAscii(value);
        };

    std::vector<std::string> lines;
    std::size_t offset = 0;

    while (offset <= existing.size()) {
        const auto newline =
            existing.find('\n', offset);
        std::string_view line =
            newline ==
                    std::string_view::npos
                ? existing.substr(offset)
                : existing.substr(
                      offset,
                      newline - offset);

        if (!line.empty() &&
            line.back() == '\r') {
            line.remove_suffix(1);
        }

        lines.emplace_back(line);

        if (newline ==
            std::string_view::npos) {
            break;
        }

        offset = newline + 1;
    }

    if (existing.empty()) {
        lines.clear();
    } else if (
        !lines.empty() &&
        lines.back().empty() &&
        existing.back() == '\n') {
        lines.pop_back();
    }

    bool sectionFound = false;
    bool inEverything = false;
    std::array<bool, required.size()>
        seen{};

    std::vector<std::string> output;
    output.reserve(
        lines.size() +
        required.size() + 2);

    const auto appendMissing =
        [&]() {
            for (std::size_t i = 0;
                 i < required.size();
                 ++i) {
                if (!seen[i]) {
                    output.push_back(
                        std::string(
                            required[i].key) +
                        "=" +
                        std::string(
                            required[i].value));
                    seen[i] = true;
                }
            }
        };

    for (const auto& original : lines) {
        std::string_view line =
            TrimAscii(original);

        if (line.size() >= 2 &&
            line.front() == '[' &&
            line.back() == ']') {
            if (inEverything) {
                appendMissing();
            }

            const auto sectionName =
                lowerTrim(
                    line.substr(
                        1,
                        line.size() - 2));

            inEverything =
                sectionName ==
                "everything";

            if (inEverything) {
                sectionFound = true;
                seen.fill(false);
            }

            output.push_back(original);
            continue;
        }

        bool replaced = false;

        if (inEverything) {
            const auto equals =
                line.find('=');

            if (equals !=
                std::string_view::npos) {
                const auto key =
                    lowerTrim(
                        line.substr(
                            0,
                            equals));

                for (std::size_t i = 0;
                     i < required.size();
                     ++i) {
                    if (key ==
                        required[i].key) {
                        if (!seen[i]) {
                            output.push_back(
                                std::string(
                                    required[i].key) +
                                "=" +
                                std::string(
                                    required[i].value));
                            seen[i] = true;
                        }
                        replaced = true;
                        break;
                    }
                }
            }
        }

        if (!replaced) {
            output.push_back(original);
        }
    }

    if (inEverything) {
        appendMissing();
    }

    if (!sectionFound) {
        if (!output.empty() &&
            !output.back().empty()) {
            output.emplace_back();
        }

        output.emplace_back(
            "[Everything]");
        seen.fill(false);
        appendMissing();
    }

    std::string result;

    for (const auto& line : output) {
        result += line;
        result += "\r\n";
    }

    return result;
}



std::wstring
ExtractEverythingServiceExecutable(
    std::wstring_view binaryPath) {
    while (!binaryPath.empty() &&
           (binaryPath.front() == L' ' ||
            binaryPath.front() == L'\t' ||
            binaryPath.front() == L'\r' ||
            binaryPath.front() == L'\n')) {
        binaryPath.remove_prefix(1);
    }

    if (binaryPath.empty()) {
        return {};
    }

    if (binaryPath.front() == L'"') {
        binaryPath.remove_prefix(1);

        const auto closing =
            binaryPath.find(L'"');

        if (closing ==
            std::wstring_view::npos) {
            return {};
        }

        return std::wstring(
            binaryPath.substr(
                0,
                closing));
    }

    std::wstring lowered(binaryPath);
    std::transform(
        lowered.begin(),
        lowered.end(),
        lowered.begin(),
        [](wchar_t c) {
            if (c >= L'A' &&
                c <= L'Z') {
                return static_cast<wchar_t>(
                    c - L'A' + L'a');
            }

            return c;
        });

    const auto exe =
        lowered.rfind(L".exe");

    if (exe ==
        std::wstring::npos) {
        const auto separator =
            binaryPath.find_first_of(
                L" \t");

        return std::wstring(
            binaryPath.substr(
                0,
                separator));
    }

    return std::wstring(
        binaryPath.substr(
            0,
            exe + 4));
}

} // namespace altrun
