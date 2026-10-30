#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace altrun::win {

// One per-user MSIX execution alias (an IO_REPARSE_TAG_APPEXECLINK stub in
// %LOCALAPPDATA%\Microsoft\WindowsApps): the shell's own record of which
// packaged application a run-by-name alias belongs to.
struct AppExecutionAliasLink {
    std::wstring packageFamily;
    std::wstring appUserModelId;
    std::wstring target;
};

[[nodiscard]] std::filesystem::path
AppExecutionAliasDirectory();

[[nodiscard]] std::optional<
    AppExecutionAliasLink>
ReadAppExecutionAlias(
    const std::filesystem::path& stub);

// Package-relative executables (lower case, backslash separated) that the
// package manifest declares as COM or service servers.
[[nodiscard]] std::vector<std::wstring>
PackageServerExecutables(
    const std::filesystem::path& packageRoot);

// Files serviced by Windows itself are owned by NT SERVICE\TrustedInstaller.
[[nodiscard]] bool
IsOwnedByTrustedInstaller(
    const std::filesystem::path& path);

} // namespace altrun::win
