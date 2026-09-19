#include "PathProvider.hpp"

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
        true,
        0,
    };
    return descriptor;
}

std::vector<Command>
PathProvider::Discover() const {

    constexpr std::size_t kMaxPathApps =
        4096;

    std::vector<Command> commands;
    std::unordered_set<std::wstring>
        seenTargets;
    std::unordered_set<std::wstring>
        seenDirectories;

    std::size_t added = 0;

    for (const auto& rawDirectory :
         SplitPath(
             EnvironmentVariable(L"PATH"))) {

        if (added >= kMaxPathApps) {
            break;
        }

        std::error_code ec;

        std::filesystem::path directory(
            rawDirectory);

        const std::wstring directoryKey =
            NormalizeTarget(
                directory.lexically_normal()
                    .wstring());

        if (directoryKey.empty() ||
            !seenDirectories.insert(
                directoryKey).second ||
            !std::filesystem::is_directory(
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

            Command command;
            command.title =
                it->path().stem().wstring();
            command.keyword =
                win::CompactKeyword(
                    command.title);

            if (command.keyword.empty()) {
                command.keyword =
                    win::Lower(
                        command.title);
            }

            command.target = target;
            command.type =
                CommandType::Application;
            command.icon = L"auto";
            command.enabled = true;
            command.source =
                CommandSource::Path;
            command.basePriority = -35;
            command.id =
                L"path:" + targetKey;

            commands.push_back(
                std::move(command));
            ++added;
        }
    }

    return commands;
}

} // namespace altrun
