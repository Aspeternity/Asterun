#include "WinUtil.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <algorithm>
#include <array>
#include <cwctype>
#include <optional>
#include <stdexcept>


namespace {

bool IsAsciiAlpha(wchar_t value) {
    return (value >= L'A' && value <= L'Z') ||
           (value >= L'a' && value <= L'z');
}

bool IsSchemeChar(wchar_t value) {
    return IsAsciiAlpha(value) ||
           (value >= L'0' && value <= L'9') ||
           value == L'+' ||
           value == L'-' ||
           value == L'.';
}

bool HasUriScheme(std::wstring_view text) {
    if (text.size() >= 2 &&
        IsAsciiAlpha(text[0]) &&
        text[1] == L':') {
        return false;
    }

    const auto colon = text.find(L':');
    if (colon == std::wstring_view::npos ||
        colon == 0) {
        return false;
    }

    if (!IsAsciiAlpha(text[0])) {
        return false;
    }

    for (std::size_t index = 1;
         index < colon;
         ++index) {
        if (!IsSchemeChar(text[index])) {
            return false;
        }
    }

    return true;
}

bool LooksLikeRelativePath(
    std::wstring_view text,
    bool bareRelativeIsPath) {
    if (text.empty()) {
        return false;
    }

    if (text.starts_with(L".\\") ||
        text.starts_with(L"./") ||
        text.starts_with(L"..\\") ||
        text.starts_with(L"../")) {
        return true;
    }

    if (text.find(L'\\') !=
            std::wstring_view::npos ||
        text.find(L'/') !=
            std::wstring_view::npos) {
        return true;
    }

    return bareRelativeIsPath;
}

std::wstring LowerPath(
    std::wstring_view value) {
    std::wstring out(value);
    std::transform(
        out.begin(),
        out.end(),
        out.begin(),
        [](wchar_t c) {
            if (c == L'/') {
                return L'\\';
            }
            return static_cast<wchar_t>(
                std::towlower(c));
        });
    return out;
}

std::wstring PathString(
    const std::filesystem::path& path) {
    return path.lexically_normal().wstring();
}

bool SamePathText(
    std::wstring_view left,
    std::wstring_view right) {
    return LowerPath(left) ==
        LowerPath(right);
}

bool HasPathPrefix(
    std::wstring_view path,
    std::wstring_view root) {
    const std::wstring loweredPath =
        LowerPath(path);
    std::wstring loweredRoot =
        LowerPath(root);

    while (loweredRoot.size() > 3 &&
           !loweredRoot.empty() &&
           loweredRoot.back() == L'\\') {
        loweredRoot.pop_back();
    }

    if (loweredPath.size() <
        loweredRoot.size()) {
        return false;
    }

    if (loweredPath.compare(
            0,
            loweredRoot.size(),
            loweredRoot) != 0) {
        return false;
    }

    return loweredPath.size() ==
            loweredRoot.size() ||
        loweredPath[
            loweredRoot.size()] ==
            L'\\';
}

std::size_t LeadingParentCount(
    const std::filesystem::path& relative) {
    std::size_t count = 0;

    for (const auto& part : relative) {
        if (part == L"..") {
            ++count;
            continue;
        }
        break;
    }

    return count;
}

std::optional<std::wstring>
EnvironmentPortablePath(
    const std::filesystem::path& absolutePath) {
    struct Variable {
        const wchar_t* name;
    };

    constexpr std::array<Variable, 9>
        variables{{
            {L"LOCALAPPDATA"},
            {L"APPDATA"},
            {L"USERPROFILE"},
            {L"ProgramData"},
            {L"ProgramFiles"},
            {L"ProgramFiles(x86)"},
            {L"ProgramW6432"},
            {L"WINDIR"},
            {L"SystemRoot"},
        }};

    const std::wstring absolute =
        PathString(absolutePath);

    std::optional<std::wstring> best;
    std::size_t bestRootLength = 0;

    for (const auto& variable :
         variables) {
        std::wstring token =
            L"%" +
            std::wstring(variable.name) +
            L"%";

        const std::wstring expanded =
            altrun::win::ExpandEnvironment(
                token);

        if (expanded.empty() ||
            expanded == token) {
            continue;
        }

        const std::wstring root =
            PathString(
                std::filesystem::path(
                    expanded));

        if (!HasPathPrefix(
                absolute,
                root) ||
            root.size() <=
                bestRootLength) {
            continue;
        }

        std::wstring remainder =
            absolute.substr(
                root.size());

        if (!remainder.empty() &&
            remainder.front() != L'\\') {
            remainder.insert(
                remainder.begin(),
                L'\\');
        }

        best =
            token +
            remainder;
        bestRootLength =
            root.size();
    }

    return best;
}

std::optional<std::wstring>
RelativePortablePath(
    const std::filesystem::path& absolutePath,
    const std::filesystem::path& baseDirectory) {
    const auto relative =
        absolutePath
            .lexically_normal()
            .lexically_relative(
                baseDirectory
                    .lexically_normal());

    if (relative.empty() ||
        relative.is_absolute()) {
        return std::nullopt;
    }

    // Keep relative conversion intentionally local to the portable tree.
    // One parent hop covers the common "ALTRunNext + sibling Tools"
    // layout without turning machine-specific system paths into fragile
    // chains such as ..\\..\\Windows.
    if (LeadingParentCount(relative) > 1) {
        return std::nullopt;
    }

    std::wstring value =
        relative.wstring();

    if (value.empty()) {
        value = L".";
    } else if (!value.starts_with(L".") &&
               relative.begin() !=
                   relative.end()) {
        value =
            L".\\" +
            value;
    }

    return value;
}

bool ExistsNoThrow(
    const std::filesystem::path& path) {
    std::error_code ec;
    const bool exists =
        std::filesystem::exists(
            path,
            ec);
    return !ec && exists;
}

} // namespace

namespace altrun::win {

std::wstring Utf8ToWide(std::string_view text) {
    if (text.empty()) return {};
    const int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), nullptr, 0);
    if (size <= 0) return {};
    std::wstring out(static_cast<std::size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), out.data(), size);
    return out;
}

std::string WideToUtf8(std::wstring_view text) {
    if (text.empty()) return {};
    const int size = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    if (size <= 0) return {};
    std::string out(static_cast<std::size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), out.data(), size, nullptr, nullptr);
    return out;
}

std::filesystem::path ExecutableDirectory() {
    std::wstring buffer(32768, L'\0');
    const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (length == 0 || length >= buffer.size()) {
        throw std::runtime_error("GetModuleFileNameW failed");
    }
    buffer.resize(length);
    return std::filesystem::path(buffer).parent_path();
}

std::wstring ExpandEnvironment(std::wstring_view text) {
    if (text.empty()) return {};
    const DWORD required = ExpandEnvironmentStringsW(std::wstring(text).c_str(), nullptr, 0);
    if (required == 0) return std::wstring(text);
    std::wstring out(required, L'\0');
    const DWORD written = ExpandEnvironmentStringsW(std::wstring(text).c_str(), out.data(), required);
    if (written == 0) return std::wstring(text);
    if (!out.empty() && out.back() == L'\0') out.pop_back();
    return out;
}


bool IsUncPath(
    std::wstring_view text) {
    const std::wstring value =
        Trim(text);

    return value.starts_with(L"\\\\") ||
        value.starts_with(L"//");
}

std::wstring ResolvePortablePath(
    std::wstring_view text,
    const std::filesystem::path& baseDirectory,
    bool bareRelativeIsPath) {
    const std::wstring trimmed =
        Trim(text);

    if (trimmed.empty()) {
        return {};
    }

    const std::wstring expanded =
        ExpandEnvironment(trimmed);

    if (expanded.empty() ||
        HasUriScheme(expanded) ||
        IsUncPath(expanded)) {
        return expanded;
    }

    const std::filesystem::path path(
        expanded);

    if (path.is_absolute()) {
        return PathString(path);
    }

    if (!LooksLikeRelativePath(
            expanded,
            bareRelativeIsPath)) {
        return expanded;
    }

    return PathString(
        baseDirectory /
        path);
}

std::optional<PortablePathPreview>
MakePortablePath(
    std::wstring_view text,
    const std::filesystem::path& baseDirectory,
    bool bareRelativeIsPath) {
    const std::wstring trimmed =
        Trim(text);

    if (trimmed.empty() ||
        HasUriScheme(trimmed) ||
        IsUncPath(trimmed) ||
        trimmed.find(L'%') !=
            std::wstring::npos) {
        return std::nullopt;
    }

    const std::filesystem::path original(
        trimmed);

    if (!original.is_absolute()) {
        return std::nullopt;
    }

    const std::wstring resolved =
        ResolvePortablePath(
            trimmed,
            baseDirectory,
            bareRelativeIsPath);

    const std::filesystem::path
        absolute(resolved);

    std::optional<std::wstring>
        converted =
            RelativePortablePath(
                absolute,
                baseDirectory);

    if (!converted) {
        converted =
            EnvironmentPortablePath(
                absolute);
    }

    if (!converted ||
        SamePathText(
            *converted,
            trimmed)) {
        return std::nullopt;
    }

    PortablePathPreview preview;
    preview.converted =
        *converted;
    preview.resolved =
        resolved;
    preview.exists =
        ExistsNoThrow(absolute);
    return preview;
}

std::optional<PortablePathPreview>
ExpandPortablePath(
    std::wstring_view text,
    const std::filesystem::path& baseDirectory,
    bool bareRelativeIsPath) {
    const std::wstring trimmed =
        Trim(text);

    if (trimmed.empty() ||
        HasUriScheme(trimmed) ||
        IsUncPath(trimmed)) {
        return std::nullopt;
    }

    const bool hasEnvironment =
        trimmed.find(L'%') !=
        std::wstring::npos;

    const std::filesystem::path path(
        trimmed);

    const bool relative =
        !path.is_absolute() &&
        LooksLikeRelativePath(
            trimmed,
            bareRelativeIsPath);

    if (!hasEnvironment &&
        !relative) {
        return std::nullopt;
    }

    const std::wstring resolved =
        ResolvePortablePath(
            trimmed,
            baseDirectory,
            bareRelativeIsPath);

    if (IsUncPath(resolved)) {
        return std::nullopt;
    }

    const std::filesystem::path
        absolute(resolved);

    if (!absolute.is_absolute() ||
        SamePathText(
            resolved,
            trimmed)) {
        return std::nullopt;
    }

    PortablePathPreview preview;
    preview.converted =
        resolved;
    preview.resolved =
        resolved;
    preview.exists =
        ExistsNoThrow(absolute);
    return preview;
}

std::wstring Trim(std::wstring_view text) {
    std::size_t start = 0;
    std::size_t end = text.size();
    while (start < end && std::iswspace(text[start])) ++start;
    while (end > start && std::iswspace(text[end - 1])) --end;
    return std::wstring(text.substr(start, end - start));
}

std::vector<std::wstring> SplitTabs(std::wstring_view line) {
    std::vector<std::wstring> fields;
    std::size_t start = 0;
    while (start <= line.size()) {
        const auto pos = line.find(L'\t', start);
        if (pos == std::wstring_view::npos) {
            fields.emplace_back(line.substr(start));
            break;
        }
        fields.emplace_back(line.substr(start, pos - start));
        start = pos + 1;
    }
    return fields;
}

std::wstring Lower(std::wstring_view text) {
    std::wstring out(text);
    std::transform(out.begin(), out.end(), out.begin(), [](wchar_t c) {
        return static_cast<wchar_t>(std::towlower(c));
    });
    return out;
}

std::wstring CompactKeyword(std::wstring_view text) {
    std::wstring out;
    out.reserve(text.size());
    for (wchar_t c : text) {
        if (std::iswalnum(c) || c >= 0x4E00) {
            out.push_back(static_cast<wchar_t>(std::towlower(c)));
        }
    }
    return out;
}

std::int64_t UnixTimeNow() {
    FILETIME ft{};
    GetSystemTimeAsFileTime(&ft);
    ULARGE_INTEGER value{};
    value.LowPart = ft.dwLowDateTime;
    value.HighPart = ft.dwHighDateTime;
    constexpr unsigned long long kEpochDiff100ns = 116444736000000000ULL;
    return static_cast<std::int64_t>((value.QuadPart - kEpochDiff100ns) / 10000000ULL);
}

std::wstring FormatWin32Error(unsigned long errorCode) {
    wchar_t* buffer = nullptr;
    const DWORD length = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        errorCode,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<wchar_t*>(&buffer),
        0,
        nullptr);

    std::wstring message;
    if (length != 0 && buffer != nullptr) {
        message.assign(buffer, length);
        LocalFree(buffer);
    }
    return Trim(message);
}

} // namespace altrun::win
