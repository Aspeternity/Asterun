#include "AppPathsProvider.hpp"

#include "ProviderFingerprint.hpp"
#include "ProviderIds.hpp"
#include "../platform/WinUtil.hpp"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

namespace altrun {

namespace {

constexpr wchar_t kAppPathsKey[] =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\App Paths";

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
    std::wstring_view target) {

    return L"apppath:" +
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
        NormalizeTarget(
            command.target);

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

    output.push_back(
        std::move(command));

    return true;
}

void EnumerateAppPathsKey(
    HKEY root,
    REGSAM view,
    std::vector<Command>& output,
    std::unordered_set<std::wstring>& seenTargets) {

    HKEY appPaths{};

    if (RegOpenKeyExW(
            root,
            kAppPathsKey,
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

        if (status ==
            ERROR_NO_MORE_ITEMS) {
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
        command.target = target;
        command.type =
            CommandType::Application;
        command.icon = L"auto";
        command.enabled = true;
        command.source =
            CommandSource::AppPaths;
        command.basePriority = -10;
        command.id =
            MakeId(command.target);

        AddDiscovered(
            output,
            seenTargets,
            std::move(command));
    }

    RegCloseKey(appPaths);
}

void FingerprintAppPathsKey(
    HKEY root,
    REGSAM view,
    std::vector<std::uint64_t>& items) {

    HKEY appPaths{};

    if (RegOpenKeyExW(
            root,
            kAppPathsKey,
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

        FILETIME lastWrite{};

        const LSTATUS status =
            RegEnumKeyExW(
                appPaths,
                index,
                nameBuffer.data(),
                &nameLength,
                nullptr,
                nullptr,
                nullptr,
                &lastWrite);

        if (status ==
            ERROR_NO_MORE_ITEMS) {
            break;
        }

        ++index;

        if (status != ERROR_SUCCESS ||
            nameLength == 0) {
            continue;
        }

        std::wstring name(
            nameBuffer.data(),
            nameLength);

        std::uint64_t itemHash =
            fingerprint::kOffset;

        fingerprint::Mix(
            itemHash,
            name);

        const std::uint64_t timeValue =
            (static_cast<std::uint64_t>(
                 lastWrite.dwHighDateTime)
             << 32u) |
            lastWrite.dwLowDateTime;

        fingerprint::Mix(
            itemHash,
            timeValue);

        HKEY appKey{};

        if (RegOpenKeyExW(
                appPaths,
                name.c_str(),
                0,
                KEY_READ | view,
                &appKey) == ERROR_SUCCESS) {

            fingerprint::Mix(
                itemHash,
                RegistryString(
                    appKey,
                    nullptr));

            RegCloseKey(appKey);
        }

        items.push_back(
            itemHash);
    }

    RegCloseKey(appPaths);
}

} // namespace

const ProviderDescriptor&
AppPathsProvider::Descriptor() const noexcept {
    static const ProviderDescriptor descriptor{
        std::string(providers::kAppPaths),
        L"App Paths",
        true,
        20,
    };
    return descriptor;
}

std::vector<Command>
AppPathsProvider::Discover() const {

    std::vector<Command> commands;
    std::unordered_set<std::wstring>
        seenTargets;

    constexpr std::array<REGSAM, 2>
        views{
            KEY_WOW64_64KEY,
            KEY_WOW64_32KEY,
        };

    for (const REGSAM view : views) {
        EnumerateAppPathsKey(
            HKEY_CURRENT_USER,
            view,
            commands,
            seenTargets);

        EnumerateAppPathsKey(
            HKEY_LOCAL_MACHINE,
            view,
            commands,
            seenTargets);
    }

    return commands;
}

std::uint64_t
AppPathsProvider::ChangeToken() const {
    std::vector<std::uint64_t> items;

    constexpr std::array<REGSAM, 2>
        views{
            KEY_WOW64_64KEY,
            KEY_WOW64_32KEY,
        };

    for (const REGSAM view : views) {
        FingerprintAppPathsKey(
            HKEY_CURRENT_USER,
            view,
            items);

        FingerprintAppPathsKey(
            HKEY_LOCAL_MACHINE,
            view,
            items);
    }

    std::sort(
        items.begin(),
        items.end());

    std::uint64_t hash =
        fingerprint::kOffset;

    fingerprint::Mix(
        hash,
        static_cast<std::uint64_t>(
            items.size()));

    for (const auto item : items) {
        fingerprint::Mix(
            hash,
            item);
    }

    return hash;
}

} // namespace altrun
