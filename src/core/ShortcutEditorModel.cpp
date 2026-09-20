#include "ShortcutEditorModel.hpp"

#include <algorithm>
#include <cwctype>
#include <filesystem>

namespace altrun {
namespace {

[[nodiscard]] std::wstring
TrimWide(std::wstring_view value) {
    std::size_t first = 0;
    std::size_t last = value.size();

    while (first < last &&
           std::iswspace(value[first])) {
        ++first;
    }

    while (last > first &&
           std::iswspace(value[last - 1])) {
        --last;
    }

    return std::wstring(
        value.substr(first, last - first));
}

[[nodiscard]] std::wstring
LowerWide(std::wstring_view value) {
    std::wstring result(value);
    std::transform(
        result.begin(),
        result.end(),
        result.begin(),
        [](wchar_t c) {
            return static_cast<wchar_t>(
                std::towlower(c));
        });
    return result;
}

[[nodiscard]] bool
IsKeywordSeparator(wchar_t c) {
    return c == L',' ||
        c == L';' ||
        c == L'，' ||
        c == L'；';
}

[[nodiscard]] bool
LooksLikeUri(std::wstring_view value) {
    const std::wstring lower =
        LowerWide(value);

    if (lower.starts_with(L"www.")) {
        return true;
    }

    const std::size_t colon =
        lower.find(L':');

    if (colon == std::wstring::npos ||
        colon <= 1) {
        return false;
    }

    for (std::size_t index = 0;
         index < colon;
         ++index) {
        const wchar_t c = lower[index];
        if (!std::iswalnum(c) &&
            c != L'+' &&
            c != L'-' &&
            c != L'.') {
            return false;
        }
    }

    // shell: is a Windows shell moniker rather than a web/URI shortcut.
    return lower.substr(0, colon) != L"shell";
}

[[nodiscard]] bool
IsCommandLineExtension(
    std::wstring_view lower) {
    return lower == L".bat" ||
        lower == L".cmd" ||
        lower == L".ps1" ||
        lower == L".psm1" ||
        lower == L".vbs" ||
        lower == L".wsf" ||
        lower == L".py";
}

[[nodiscard]] std::wstring
BasenamePortable(
    std::wstring_view value) {
    std::wstring trimmed =
        TrimWide(value);

    while (trimmed.size() > 1 &&
           (trimmed.back() == L'\\' ||
            trimmed.back() == L'/')) {
        trimmed.pop_back();
    }

    const std::size_t slash =
        trimmed.find_last_of(L"\\/");

    if (slash != std::wstring::npos &&
        slash + 1 < trimmed.size()) {
        return trimmed.substr(slash + 1);
    }

    return trimmed;
}

[[nodiscard]] bool
LooksLikeAbsoluteFilesystemPath(
    std::wstring_view value) {
    if (value.starts_with(L"\\\\") ||
        value.starts_with(L"/")) {
        return true;
    }

    return value.size() >= 3 &&
        std::iswalpha(value[0]) &&
        value[1] == L':' &&
        (value[2] == L'\\' ||
         value[2] == L'/');
}

} // namespace

ShortcutKeywordSet
ParseShortcutKeywords(
    std::wstring_view text) {
    ShortcutKeywordSet result;
    std::vector<std::wstring> values;
    std::wstring current;

    const auto flush = [&]() {
        std::wstring value =
            TrimWide(current);
        current.clear();

        if (value.empty()) {
            return;
        }

        const std::wstring lower =
            LowerWide(value);

        const bool duplicate =
            std::any_of(
                values.begin(),
                values.end(),
                [&](const std::wstring& existing) {
                    return LowerWide(existing) ==
                        lower;
                });

        if (!duplicate) {
            values.push_back(
                std::move(value));
        }
    };

    for (const wchar_t c : text) {
        if (IsKeywordSeparator(c)) {
            flush();
        } else {
            current.push_back(c);
        }
    }

    flush();

    if (values.empty()) {
        return result;
    }

    result.primary =
        std::move(values.front());

    for (std::size_t index = 1;
         index < values.size();
         ++index) {
        result.aliases.push_back(
            std::move(values[index]));
    }

    return result;
}

std::wstring
FormatShortcutKeywords(
    std::wstring_view primary,
    const std::vector<std::wstring>& aliases) {
    std::wstring result =
        TrimWide(primary);

    for (const auto& alias : aliases) {
        const std::wstring value =
            TrimWide(alias);
        if (value.empty()) {
            continue;
        }

        if (!result.empty()) {
            result += L", ";
        }
        result += value;
    }

    return result;
}

CommandType
InferShortcutCommandType(
    std::wstring_view target) {
    const std::wstring value =
        TrimWide(target);

    if (value.empty()) {
        return CommandType::Application;
    }

    if (LooksLikeUri(value)) {
        return CommandType::Url;
    }

    std::error_code ec;
    if (std::filesystem::is_directory(
            std::filesystem::path(value),
            ec) &&
        !ec) {
        return CommandType::Folder;
    }

    if (value.ends_with(L"\\") ||
        value.ends_with(L"/")) {
        return CommandType::Folder;
    }

    std::wstring lower =
        LowerWide(value);
    const std::size_t separator =
        lower.find_last_of(L"\\/");
    const std::size_t dot =
        lower.find_last_of(L'.');

    if (dot != std::wstring::npos &&
        (separator == std::wstring::npos ||
         dot > separator) &&
        IsCommandLineExtension(
            lower.substr(dot))) {
        return CommandType::CommandLine;
    }

    return CommandType::Application;
}

std::wstring
SuggestShortcutTitle(
    std::wstring_view target,
    CommandType type,
    std::wstring_view fallback) {
    std::wstring value =
        TrimWide(target);

    if (type == CommandType::Url &&
        !value.empty()) {
        std::wstring lower =
            LowerWide(value);

        std::size_t start = 0;
        const std::size_t scheme =
            lower.find(L"://");

        if (scheme != std::wstring::npos) {
            start = scheme + 3;
        } else {
            const std::size_t colon =
                lower.find(L':');
            if (colon != std::wstring::npos &&
                colon > 1) {
                start = colon + 1;
            }
        }

        while (start < value.size() &&
               value[start] == L'/') {
            ++start;
        }

        std::size_t end =
            value.find_first_of(
                L"/?#",
                start);

        if (end == std::wstring::npos) {
            end = value.size();
        }

        std::wstring host =
            value.substr(
                start,
                end - start);

        if (LowerWide(host)
                .starts_with(L"www.")) {
            host.erase(0, 4);
        }

        if (!host.empty()) {
            return host;
        }
    }

    std::wstring name =
        BasenamePortable(value);

    if ((type == CommandType::Application ||
         type == CommandType::CommandLine) &&
        !name.empty()) {
        const std::size_t dot =
            name.find_last_of(L'.');
        if (dot != std::wstring::npos &&
            dot > 0) {
            name.erase(dot);
        }
    }

    if (!name.empty()) {
        return name;
    }

    return TrimWide(fallback);
}

std::wstring
DefaultShortcutWorkingDirectory(
    CommandType type,
    std::wstring_view resolvedTarget) {
    if (type != CommandType::Application &&
        type != CommandType::CommandLine) {
        return {};
    }

    std::wstring target =
        TrimWide(resolvedTarget);

    if (target.size() >= 2 &&
        target.front() == L'"' &&
        target.back() == L'"') {
        target =
            target.substr(
                1,
                target.size() - 2);
    }

    if (!LooksLikeAbsoluteFilesystemPath(
            target)) {
        return {};
    }

    const std::size_t separator =
        target.find_last_of(L"\\/");

    if (separator == std::wstring::npos) {
        return {};
    }

    if (separator == 0) {
        return target.substr(0, 1);
    }

    if (separator == 2 &&
        target.size() >= 3 &&
        target[1] == L':') {
        return target.substr(0, 3);
    }

    return target.substr(0, separator);
}

} // namespace altrun
