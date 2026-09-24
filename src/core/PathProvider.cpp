#include "PathProvider.hpp"

#include "LaunchCandidate.hpp"
#include "ProviderFingerprint.hpp"
#include "ProviderIds.hpp"
#include "../platform/WinUtil.hpp"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

namespace altrun {

namespace {

std::wstring StripQuotes(
    std::wstring value) {

    value = win::Trim(value);

    if (value.size() >= 2 &&
        value.front() == L'"' &&
        value.back() == L'"') {
        value =
            value.substr(
                1,
                value.size() - 2);
    }

    return value;
}

std::wstring EnvironmentVariable(
    const wchar_t* name) {

    const DWORD required =
        GetEnvironmentVariableW(
            name,
            nullptr,
            0);

    if (required == 0) {
        return {};
    }

    std::wstring value(
        required,
        L'\0');

    const DWORD written =
        GetEnvironmentVariableW(
            name,
            value.data(),
            required);

    if (written == 0 ||
        written >= required) {
        return {};
    }

    value.resize(written);
    return value;
}

std::wstring RegistryPathValue(
    HKEY root,
    const wchar_t* subkey) {

    HKEY key{};

    if (RegOpenKeyExW(
            root,
            subkey,
            0,
            KEY_READ,
            &key) != ERROR_SUCCESS) {
        return {};
    }

    DWORD type = 0;
    DWORD bytes = 0;

    if (RegQueryValueExW(
            key,
            L"Path",
            nullptr,
            &type,
            nullptr,
            &bytes) != ERROR_SUCCESS ||
        bytes == 0 ||
        (type != REG_SZ &&
         type != REG_EXPAND_SZ)) {
        RegCloseKey(key);
        return {};
    }

    std::wstring value(
        bytes / sizeof(wchar_t),
        L'\0');

    if (RegQueryValueExW(
            key,
            L"Path",
            nullptr,
            &type,
            reinterpret_cast<BYTE*>(
                value.data()),
            &bytes) != ERROR_SUCCESS) {
        RegCloseKey(key);
        return {};
    }

    RegCloseKey(key);

    while (!value.empty() &&
           value.back() == L'\0') {
        value.pop_back();
    }

    if (type == REG_EXPAND_SZ) {
        value =
            win::ExpandEnvironment(
                value);
    }

    return value;
}

std::vector<std::wstring>
PathSources() {
    std::vector<std::wstring> values;

    const std::wstring processPath =
        EnvironmentVariable(L"PATH");

    if (!processPath.empty()) {
        values.push_back(
            processPath);
    }

    const std::wstring machinePath =
        RegistryPathValue(
            HKEY_LOCAL_MACHINE,
            L"SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Environment");

    if (!machinePath.empty()) {
        values.push_back(
            machinePath);
    }

    const std::wstring userPath =
        RegistryPathValue(
            HKEY_CURRENT_USER,
            L"Environment");

    if (!userPath.empty()) {
        values.push_back(
            userPath);
    }

    return values;
}

std::vector<std::wstring> SplitPath(
    std::wstring_view value) {

    std::vector<std::wstring> entries;
    std::size_t start = 0;

    while (start <= value.size()) {
        const auto separator =
            value.find(L';', start);

        std::wstring part =
            separator ==
                    std::wstring_view::npos
                ? std::wstring(
                      value.substr(start))
                : std::wstring(
                      value.substr(
                          start,
                          separator - start));

        part = StripQuotes(
            win::ExpandEnvironment(
                win::Trim(part)));

        if (!part.empty()) {
            entries.push_back(
                std::move(part));
        }

        if (separator ==
            std::wstring_view::npos) {
            break;
        }

        start = separator + 1;
    }

    return entries;
}

std::wstring NormalizeTarget(
    std::wstring_view target) {

    std::wstring normalized =
        win::Lower(
            win::Trim(target));

    std::replace(
        normalized.begin(),
        normalized.end(),
        L'/',
        L'\\');

    return normalized;
}

std::vector<std::filesystem::path>
PathDirectories() {

    std::vector<std::pair<
        std::wstring,
        std::filesystem::path>>
        keyed;

    std::unordered_set<std::wstring>
        seen;

    for (const auto& source :
         PathSources()) {
        for (const auto& rawDirectory :
             SplitPath(source)) {

            std::filesystem::path directory(
                rawDirectory);

            const std::wstring key =
                NormalizeTarget(
                    directory
                        .lexically_normal()
                        .wstring());

            if (key.empty() ||
                !seen.insert(key).second) {
                continue;
            }

            keyed.emplace_back(
                key,
                std::move(directory));
        }
    }

    std::sort(
        keyed.begin(),
        keyed.end(),
        [](const auto& left,
           const auto& right) {
            return left.first <
                right.first;
        });

    std::vector<std::filesystem::path>
        directories;

    directories.reserve(
        keyed.size());

    for (auto& [key, path] :
         keyed) {
        directories.push_back(
            std::move(path));
    }

    return directories;
}

bool IsPathExecutable(
    const std::filesystem::path& path) {

    const std::wstring extension =
        win::Lower(
            path.extension().wstring());

    return extension == L".exe" ||
           extension == L".com" ||
           extension == L".bat" ||
           extension == L".cmd";
}

} // namespace

const ProviderDescriptor&
PathProvider::Descriptor() const noexcept {
    static const ProviderDescriptor descriptor{
        std::string(providers::kPath),
        L"PATH",
        false,
        0,
    };
    return descriptor;
}

std::vector<Command>
PathProvider::Discover() const {
    return DiscoverDetailed().commands;
}

ProviderDiscoveryPayload
PathProvider::DiscoverDetailed() const {

    constexpr std::size_t kMaxPathApps =
        4096;

    ProviderDiscoveryPayload payload;
    auto& commands = payload.commands;
    auto& diagnostics =
        payload.admission;

    std::unordered_set<std::wstring>
        seenTargets;

    std::size_t added = 0;

    for (const auto& directory :
         PathDirectories()) {

        if (added >= kMaxPathApps) {
            break;
        }

        std::error_code ec;

        if (!std::filesystem::is_directory(
                directory,
                ec)) {
            continue;
        }

        for (std::filesystem::directory_iterator it(
                 directory,
                 std::filesystem::directory_options::
                     skip_permission_denied,
                 ec),
             end;
             it != end && added < kMaxPathApps;
             it.increment(ec)) {

            if (ec) {
                ec.clear();
                continue;
            }

            if (!it->is_regular_file(ec) ||
                !IsPathExecutable(
                    it->path())) {
                continue;
            }

            const std::wstring target =
                it->path().wstring();

            const std::wstring targetKey =
                NormalizeTarget(target);

            if (targetKey.empty() ||
                !seenTargets.insert(
                    targetKey).second) {
                continue;
            }

            const std::wstring extension =
                win::Lower(
                    it->path()
                        .extension()
                        .wstring());

            LaunchTargetKind targetKind =
                LaunchTargetKind::
                    ConsoleExecutable;

            if (extension == L".bat" ||
                extension == L".cmd") {
                targetKind =
                    LaunchTargetKind::
                        CommandScript;
            }

            const std::wstring title =
                it->path().stem().wstring();

            const LaunchSurfaceClass
                initialSurface =
                    ClassifyApplicationSurface(
                        title,
                        target);

            const LaunchAdmission admission =
                EvaluateLaunchCandidate({
                    LaunchCandidateSource::Path,
                    title,
                    target,
                    initialSurface,
                    targetKind,
                    true,
                });

            diagnostics.Record(
                title,
                target,
                target,
                targetKind,
                initialSurface,
                true,
                admission);

            if (!admission.admit) {
                continue;
            }

            Command command;
            command.title = title;
            command.keyword =
                win::CompactKeyword(
                    command.title);

            if (command.keyword.empty()) {
                command.keyword =
                    win::Lower(
                        command.title);
            }

            command.target = target;
            command.activationKind =
                LaunchActivationKind::
                    ShellItem;
            command.canonicalIdentity =
                BuildCanonicalLaunchIdentity(
                    command.activationKind,
                    command.target);
            command.type =
                CommandType::Application;
            command.icon = L"auto";
            command.enabled = true;
            command.source =
                CommandSource::Path;
            command.surfaceClass =
                admission.surface;
            command.basePriority = 0;
            command.id =
                L"path:" + targetKey;

            commands.push_back(
                std::move(command));
            ++added;
        }
    }

    return payload;
}

std::uint64_t
PathProvider::ChangeToken() const {

    std::uint64_t hash =
        fingerprint::kOffset;

    for (const auto& source :
         PathSources()) {
        fingerprint::Mix(
            hash,
            source);
    }

    for (const auto& directory :
         PathDirectories()) {
        const std::wstring key =
            NormalizeTarget(
                directory
                    .lexically_normal()
                    .wstring());

        fingerprint::Mix(
            hash,
            key);

        std::error_code ec;
        const auto writeTime =
            std::filesystem::last_write_time(
                directory,
                ec);

        if (!ec) {
            fingerprint::Mix(
                hash,
                static_cast<std::uint64_t>(
                    writeTime
                        .time_since_epoch()
                        .count()));
        }
    }

    return hash;
}

} // namespace altrun
