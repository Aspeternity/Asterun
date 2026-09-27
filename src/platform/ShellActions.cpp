#include "ShellActions.hpp"

#include "WinUtil.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>

#include <array>
#include <filesystem>
#include <string>

namespace altrun::win {
namespace {

[[nodiscard]] bool
Exists(
    std::wstring_view path) {
    if (path.empty()) {
        return false;
    }

    return GetFileAttributesW(
               std::wstring(path).c_str()) !=
        INVALID_FILE_ATTRIBUTES;
}

[[nodiscard]] std::wstring
ResolveExistingTarget(
    std::wstring_view target,
    const std::filesystem::path&
        baseDirectory,
    bool bareRelativeIsPath) {
    std::wstring resolved =
        ResolvePortablePath(
            target,
            baseDirectory,
            bareRelativeIsPath);

    if (resolved.empty()) {
        return {};
    }

    if (Exists(resolved)) {
        return resolved;
    }

    const std::filesystem::path path(
        resolved);

    if (path.has_parent_path() ||
        path.is_absolute() ||
        resolved.find(L'\\') !=
            std::wstring::npos ||
        resolved.find(L'/') !=
            std::wstring::npos ||
        resolved.find(L':') !=
            std::wstring::npos) {
        return {};
    }

    std::array<wchar_t, 32768>
        found{};

    const auto search =
        [&](const wchar_t* extension) {
            return SearchPathW(
                nullptr,
                resolved.c_str(),
                extension,
                static_cast<DWORD>(
                    found.size()),
                found.data(),
                nullptr);
        };

    DWORD length =
        search(nullptr);

    if ((length == 0 ||
         length >= found.size()) &&
        path.extension().empty()) {
        length =
            search(L".exe");
    }

    if (length == 0 ||
        length >= found.size()) {
        return {};
    }

    std::wstring discovered(
        found.data(),
        length);

    return Exists(discovered)
        ? discovered
        : std::wstring{};
}

} // namespace

bool RevealInExplorer(
    std::wstring_view target,
    const std::filesystem::path&
        baseDirectory,
    bool bareRelativeIsPath) {
    const std::wstring resolved =
        ResolveExistingTarget(
            target,
            baseDirectory,
            bareRelativeIsPath);

    if (resolved.empty()) {
        return false;
    }

    std::wstring parameters =
        L"/select,\"";
    parameters += resolved;
    parameters += L"\"";

    const auto result =
        reinterpret_cast<INT_PTR>(
            ShellExecuteW(
                nullptr,
                L"open",
                L"explorer.exe",
                parameters.c_str(),
                nullptr,
                SW_SHOWNORMAL));

    return result > 32;
}

} // namespace altrun::win
