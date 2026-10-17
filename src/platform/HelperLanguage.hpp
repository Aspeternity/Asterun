#pragma once

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

// Update.exe and Uninstall.exe speak the language chosen in Asterun's own
// settings, not merely the Windows display language. They run elevated, so
// the settings file is scanned for the one value they need instead of being
// handed to a full JSON parser.
namespace altrun::helper_language {

// The appearance language written by Asterun ("zh-CN" / "en-US"), using the
// launcher's own rule: "en" or "en-US" is English, anything else Chinese.
// nullopt when the text holds no "language" string value.
[[nodiscard]] inline std::optional<bool>
ChineseFromSettingsText(std::string_view text) {
    constexpr std::string_view kKey = "\"language\"";
    constexpr std::size_t kMaximumValue = 16;

    for (std::size_t at = text.find(kKey);
         at != std::string_view::npos;
         at = text.find(kKey, at + 1)) {
        std::size_t i = at + kKey.size();
        const auto skipSpace = [&] {
            while (i < text.size() &&
                   (text[i] == ' ' || text[i] == '\t' ||
                    text[i] == '\r' || text[i] == '\n')) {
                ++i;
            }
        };

        skipSpace();
        if (i >= text.size() || text[i] != ':') {
            continue;
        }
        ++i;
        skipSpace();
        if (i >= text.size() || text[i] != '"') {
            continue;
        }

        const std::size_t begin = i + 1;
        const std::size_t end = text.find('"', begin);
        if (end == std::string_view::npos ||
            end - begin > kMaximumValue) {
            continue;
        }

        std::string value(text.substr(begin, end - begin));
        std::transform(
            value.begin(), value.end(), value.begin(),
            [](char c) {
                return c >= 'A' && c <= 'Z'
                    ? static_cast<char>(c - 'A' + 'a')
                    : c;
            });
        return !(value == "en" || value == "en-us");
    }

    return std::nullopt;
}

#ifdef _WIN32
// Chinese when the installation's settings say so; when they cannot be read
// (first run, deleted data), follow the Windows display language instead.
[[nodiscard]] inline bool
PreferChinese(const std::filesystem::path& install) {
    constexpr std::streamsize kMaximumSettingsBytes = 256 * 1024;

    if (!install.empty()) {
        std::ifstream input(
            install / L"data" / L"settings.json",
            std::ios::binary);

        if (input) {
            std::string text(
                static_cast<std::size_t>(kMaximumSettingsBytes), '\0');
            input.read(text.data(), kMaximumSettingsBytes);
            text.resize(static_cast<std::size_t>(input.gcount()));

            if (!text.empty()) {
                // A readable settings file without a language uses the
                // launcher's default, Chinese.
                return ChineseFromSettingsText(text).value_or(true);
            }
        }
    }

    return PRIMARYLANGID(GetUserDefaultUILanguage()) == LANG_CHINESE;
}
#endif

} // namespace altrun::helper_language
