#include "WindowsAppProvider.hpp"

#include "../platform/WinUtil.hpp"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <knownfolders.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <wrl/client.h>

#include <algorithm>
#include <array>
#include <cwctype>
#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

namespace altrun {

namespace {

using Microsoft::WRL::ComPtr;

std::wstring StripQuotes(std::wstring value) {
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

std::wstring RegistryString(
    HKEY key,
    const wchar_t* valueName) {

    DWORD type = 0;
    DWORD bytes = 0;

    if (RegQueryValueExW(
            key,
            valueName,
            nullptr,
            &type,
            nullptr,
            &bytes) != ERROR_SUCCESS ||
        bytes == 0 ||
        (type != REG_SZ &&
         type != REG_EXPAND_SZ)) {
        return {};
    }

    std::wstring value(
        bytes / sizeof(wchar_t),
        L'\0');

    if (RegQueryValueExW(
            key,
            valueName,
            nullptr,
            &type,
            reinterpret_cast<BYTE*>(
                value.data()),
            &bytes) != ERROR_SUCCESS) {
        return {};
    }

    while (!value.empty() &&
           value.back() == L'\0') {
        value.pop_back();
    }

    if (type == REG_EXPAND_SZ) {
        value =
            win::ExpandEnvironment(
                value);
    }

    return StripQuotes(
        std::move(value));
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

std::wstring MakeId(
    std::wstring_view provider,
    std::wstring_view target) {

    return std::wstring(provider) +
        L":" +
        NormalizeTarget(target);
}

std::wstring FriendlyStem(
    std::wstring_view value) {

    const std::filesystem::path path(
        value);

    std::wstring stem =
        path.stem().wstring();

    if (stem.empty()) {
        stem =
            path.filename().wstring();
    }

    return stem;
}

bool AddDiscovered(
    std::vector<Command>& output,
    std::unordered_set<std::wstring>& seenTargets,
    Command command) {

    const std::wstring targetKey =
        NormalizeTarget(command.target);

    if (targetKey.empty() ||
        !seenTargets.insert(
            targetKey).second) {
        return false;
    }

    if (command.keyword.empty()) {
        command.keyword =
            win::CompactKeyword(
                command.title);
    }

    if (command.keyword.empty()) {
        command.keyword =
            win::Lower(
                command.title);
    }

    if (command.id.empty()) {
        command.id =
            MakeId(
                L"windows",
                command.target);
    }

    output.push_back(
        std::move(command));

    return true;
}

void EnumerateAppPathsKey(
    HKEY root,
    REGSAM view,
    std::vector<Command>& output,
    std::unordered_set<std::wstring>& seenTargets) {

    constexpr wchar_t kKeyPath[] =
        L"Software\\Microsoft\\Windows\\CurrentVersion\\App Paths";

    HKEY appPaths{};

    if (RegOpenKeyExW(
            root,
            kKeyPath,
            0,
            KEY_READ | view,
            &appPaths) != ERROR_SUCCESS) {
        return;
    }

    DWORD index = 0;

    for (;;) {
        std::array<wchar_t, 512>
            nameBuffer{};

        DWORD nameLength =
            static_cast<DWORD>(
                nameBuffer.size());

        const LSTATUS status =
            RegEnumKeyExW(
                appPaths,
                index,
                nameBuffer.data(),
                &nameLength,
                nullptr,
                nullptr,
                nullptr,
                nullptr);

        if (status == ERROR_NO_MORE_ITEMS) {
            break;
        }

        ++index;

        if (status != ERROR_SUCCESS ||
            nameLength == 0) {
            continue;
        }

        std::wstring subkeyName(
            nameBuffer.data(),
            nameLength);

        HKEY appKey{};

        if (RegOpenKeyExW(
                appPaths,
                subkeyName.c_str(),
                0,
                KEY_READ | view,
                &appKey) != ERROR_SUCCESS) {
            continue;
        }

        const std::wstring target =
            RegistryString(
                appKey,
                nullptr);

        RegCloseKey(appKey);

        if (target.empty()) {
            continue;
        }

        std::wstring title =
            FriendlyStem(
                subkeyName);

        if (title.empty()) {
            title =
                FriendlyStem(
                    target);
        }

        Command command;
        command.title =
            std::move(title);
        command.keyword =
            win::CompactKeyword(
                command.title);
        command.target =
            target;
        command.type =
            CommandType::Application;
        command.icon = L"auto";
        command.enabled = true;
        command.source =
            CommandSource::AppPaths;
        command.basePriority = -10;
        command.id =
            MakeId(
                L"apppath",
                command.target);

        AddDiscovered(
            output,
            seenTargets,
            std::move(command));
    }

    RegCloseKey(appPaths);
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

std::vector<Command>
WindowsAppProvider::Discover() const {

    std::vector<Command> commands;
    std::unordered_set<std::wstring>
        seenTargets;

    // AppsFolder is COM-backed. Discovery can now run on a worker thread,
    // so initialize an apartment on whichever thread calls this provider.
    const HRESULT comResult =
        CoInitializeEx(
            nullptr,
            COINIT_APARTMENTTHREADED);

    // Prefer richer shell entries first, then registry aliases, then raw
    // PATH executables. CommandStore separately places Start Menu above this
    // provider, so automatic-source ordering follows the search priorities.
    DiscoverPackagedApps(
        commands,
        seenTargets);

    if (SUCCEEDED(comResult)) {
        CoUninitialize();
    }

    DiscoverAppPaths(
        commands,
        seenTargets);

    DiscoverPathExecutables(
        commands,
        seenTargets);

    return commands;
}

void WindowsAppProvider::DiscoverAppPaths(
    std::vector<Command>& output,
    std::unordered_set<std::wstring>& seenTargets) const {

    constexpr std::array<REGSAM, 2>
        views{
            KEY_WOW64_64KEY,
            KEY_WOW64_32KEY,
        };

    for (const REGSAM view : views) {
        EnumerateAppPathsKey(
            HKEY_CURRENT_USER,
            view,
            output,
            seenTargets);

        EnumerateAppPathsKey(
            HKEY_LOCAL_MACHINE,
            view,
            output,
            seenTargets);
    }
}

void WindowsAppProvider::DiscoverPathExecutables(
    std::vector<Command>& output,
    std::unordered_set<std::wstring>& seenTargets) const {

    constexpr std::size_t kMaxPathApps =
        4096;

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

            Command command;
            command.title =
                it->path().stem().wstring();
            command.keyword =
                win::CompactKeyword(
                    command.title);
            command.target =
                it->path().wstring();
            command.type =
                CommandType::Application;
            command.icon = L"auto";
            command.enabled = true;
            command.source =
                CommandSource::Path;
            command.basePriority = -35;
            command.id =
                MakeId(
                    L"path",
                    command.target);

            if (AddDiscovered(
                    output,
                    seenTargets,
                    std::move(command))) {
                ++added;
            }
        }
    }
}

void WindowsAppProvider::DiscoverPackagedApps(
    std::vector<Command>& output,
    std::unordered_set<std::wstring>& seenTargets) const {

    ComPtr<IShellItem> appsFolder;

    if (FAILED(
            SHGetKnownFolderItem(
                FOLDERID_AppsFolder,
                KF_FLAG_DEFAULT,
                nullptr,
                IID_PPV_ARGS(
                    &appsFolder)))) {
        return;
    }

    ComPtr<IEnumShellItems> enumerator;

    if (FAILED(
            appsFolder->BindToHandler(
                nullptr,
                BHID_EnumItems,
                IID_PPV_ARGS(
                    &enumerator)))) {
        return;
    }

    for (;;) {
        ComPtr<IShellItem> item;
        ULONG fetched = 0;

        const HRESULT next =
            enumerator->Next(
                1,
                item.GetAddressOf(),
                &fetched);

        if (next != S_OK ||
            fetched != 1 ||
            !item) {
            break;
        }

        PWSTR rawTitle = nullptr;

        if (FAILED(
                item->GetDisplayName(
                    SIGDN_NORMALDISPLAY,
                    &rawTitle)) ||
            rawTitle == nullptr) {
            continue;
        }

        std::wstring title(
            rawTitle);

        CoTaskMemFree(
            rawTitle);

        if (title.empty()) {
            continue;
        }

        PWSTR rawTarget = nullptr;

        if (FAILED(
                item->GetDisplayName(
                    SIGDN_DESKTOPABSOLUTEPARSING,
                    &rawTarget)) ||
            rawTarget == nullptr) {
            continue;
        }

        std::wstring target(
            rawTarget);

        CoTaskMemFree(
            rawTarget);

        if (target.empty()) {
            continue;
        }

        Command command;
        command.title =
            std::move(title);
        command.keyword =
            win::CompactKeyword(
                command.title);
        command.target =
            std::move(target);
        command.type =
            CommandType::Application;
        command.icon = L"auto";
        command.enabled = true;
        command.source =
            CommandSource::PackagedApp;
        command.basePriority = -5;
        command.id =
            MakeId(
                L"packaged",
                command.target);

        AddDiscovered(
            output,
            seenTargets,
            std::move(command));
    }
}

} // namespace altrun
