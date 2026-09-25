#include "CommandMerge.hpp"

#include <algorithm>
#include <cwctype>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace altrun {

namespace {

bool IsUser(
    CommandSource source) noexcept {

    return source ==
        CommandSource::User;
}

int SourcePriority(
    CommandSource source) noexcept {

    switch (source) {
    case CommandSource::User:
        return 100;
    case CommandSource::StartMenu:
        return 40;
    case CommandSource::PackagedApp:
        return 30;
    case CommandSource::AppPaths:
        return 20;
    case CommandSource::Path:
        return 0;
    }

    return 0;
}

std::wstring Trim(
    std::wstring_view value) {

    std::size_t first = 0;
    std::size_t last = value.size();

    while (first < last &&
           std::iswspace(
               value[first])) {
        ++first;
    }

    while (last > first &&
           std::iswspace(
               value[last - 1])) {
        --last;
    }

    return std::wstring(
        value.substr(
            first,
            last - first));
}

std::wstring Lower(
    std::wstring_view value) {

    std::wstring result(value);

    std::transform(
        result.begin(),
        result.end(),
        result.begin(),
        [](wchar_t ch) {
            return static_cast<wchar_t>(
                std::towlower(ch));
        });

    return result;
}

std::wstring Compact(
    std::wstring_view value) {

    std::wstring result;
    result.reserve(value.size());

    for (wchar_t ch : value) {
        if (std::iswalnum(ch) ||
            ch >= 0x4E00) {
            result.push_back(
                static_cast<wchar_t>(
                    std::towlower(ch)));
        }
    }

    return result;
}

std::wstring NormalizeTarget(
    std::wstring_view value) {

    std::wstring result =
        Lower(Trim(value));

    std::replace(
        result.begin(),
        result.end(),
        L'/',
        L'\\');

    return result;
}

std::wstring NameKey(
    const Command& command) {

    std::wstring key =
        Compact(command.title);

    if (key.empty()) {
        key =
            Compact(command.keyword);
    }

    return key;
}

bool IsDuplicateOf(
    const Command& incoming,
    const Command& existing) {

    const std::wstring incomingTarget =
        NormalizeTarget(
            incoming.target);

    if (!incomingTarget.empty() &&
        incomingTarget ==
            NormalizeTarget(
                existing.target)) {
        return true;
    }

    if (IsUser(existing.source) ||
        IsUser(incoming.source)) {
        return false;
    }

    if (!incoming.canonicalIdentity.empty() &&
        !existing.canonicalIdentity.empty() &&
        incoming.canonicalIdentity ==
            existing.canonicalIdentity) {
        return true;
    }

    const std::wstring incomingName =
        NameKey(incoming);

    if (incomingName.empty() ||
        incomingName !=
            NameKey(existing)) {
        return false;
    }

    return Lower(incoming.keyword) ==
        Lower(existing.keyword);
}

// An App Paths executable inside a registered MSIX package is not the
// application's shell activation identity. Associate it only when the
// enabled packaged provider publishes the corresponding package family;
// unrelated Win32 executables remain independent even under WindowsApps.
std::wstring PackagedFamilyForAppPaths(
    const Command& command) {
    if (command.source != CommandSource::AppPaths) {
        return {};
    }

    const std::wstring path = NormalizeTarget(command.target);
    if (path.size() < 4 || path[1] != L':') {
        return {};
    }

    constexpr std::wstring_view prefix =
        L"\\program files\\windowsapps\\";
    if (!std::wstring_view(path).substr(2).starts_with(prefix)) {
        return {};
    }

    const std::size_t begin = 2 + prefix.size();
    const std::size_t end = path.find(L'\\', begin);
    if (end == std::wstring::npos || end == begin ||
        !path.ends_with(L".exe")) {
        return {};
    }

    const std::wstring_view folder(path.data() + begin, end - begin);
    const std::size_t version = folder.find(L'_');
    const std::size_t publisher = folder.rfind(L"__");
    if (version == std::wstring_view::npos ||
        publisher == std::wstring_view::npos ||
        version == 0 || publisher <= version + 1 ||
        publisher + 2 == folder.size() ||
        !std::iswdigit(folder[version + 1])) {
        return {};
    }

    // Package ownership alone is insufficient: a package may expose a
    // distinct command-line companion. Require the EXE stem to be a suffix
    // of the package name before replacing its launch surface.
    const std::wstring_view packageName = folder.substr(0, version);
    const std::size_t leaf = path.find_last_of(L'\\');
    const std::wstring_view executable(path.data() + leaf + 1,
                                       path.size() - leaf - 5);
    if (executable.size() < 4 ||
        !packageName.ends_with(executable)) {
        return {};
    }

    return std::wstring(packageName) + L"_" +
        std::wstring(folder.substr(publisher + 2));
}

bool HasPackagedFamily(
    const std::vector<const Command*>& candidates,
    std::wstring_view family) {
    const std::wstring prefix = L"aumid:" + std::wstring(family) + L"!";
    return std::any_of(candidates.begin(), candidates.end(),
                       [&](const Command* candidate) {
                           return candidate->source == CommandSource::PackagedApp &&
                               Lower(candidate->canonicalIdentity).starts_with(prefix);
                       });
}

void IncrementAccepted(
    CommandMergeStats& stats,
    CommandSource source) {

    switch (source) {
    case CommandSource::User:
        ++stats.acceptedUser;
        break;
    case CommandSource::StartMenu:
        ++stats.acceptedStartMenu;
        break;
    case CommandSource::PackagedApp:
        ++stats.acceptedPackaged;
        break;
    case CommandSource::AppPaths:
        ++stats.acceptedAppPaths;
        break;
    case CommandSource::Path:
        ++stats.acceptedPath;
        break;
    }
}

void IncrementSuppressed(
    CommandMergeStats& stats,
    CommandSource source) {

    switch (source) {
    case CommandSource::User:
        break;
    case CommandSource::StartMenu:
        ++stats.suppressedStartMenu;
        break;
    case CommandSource::PackagedApp:
        ++stats.suppressedPackaged;
        break;
    case CommandSource::AppPaths:
        ++stats.suppressedAppPaths;
        break;
    case CommandSource::Path:
        ++stats.suppressedPath;
        break;
    }
}

} // namespace

std::size_t
CommandMergeStats::Accepted(
    CommandSource source) const noexcept {

    switch (source) {
    case CommandSource::User:
        return acceptedUser;
    case CommandSource::StartMenu:
        return acceptedStartMenu;
    case CommandSource::PackagedApp:
        return acceptedPackaged;
    case CommandSource::AppPaths:
        return acceptedAppPaths;
    case CommandSource::Path:
        return acceptedPath;
    }

    return 0;
}

std::size_t
CommandMergeStats::Suppressed(
    CommandSource source) const noexcept {

    switch (source) {
    case CommandSource::StartMenu:
        return suppressedStartMenu;
    case CommandSource::PackagedApp:
        return suppressedPackaged;
    case CommandSource::AppPaths:
        return suppressedAppPaths;
    case CommandSource::Path:
        return suppressedPath;
    case CommandSource::User:
        return 0;
    }

    return 0;
}

CommandMergeResult
MergeCommandViews(
    const std::vector<Command>& userCommands,
    const std::vector<const Command*>& providerCommands) {

    CommandMergeResult result;

    result.commands.reserve(
        userCommands.size() +
        providerCommands.size());

    for (const auto& command :
         userCommands) {

        if (!command.enabled) {
            continue;
        }

        // User shortcuts are intentionally not de-duplicated against one
        // another. Explicit user configuration is authoritative.
        result.commands.push_back(command);

        IncrementAccepted(
            result.stats,
            command.source);
    }

    std::vector<const Command*>
        candidates;

    candidates.reserve(
        providerCommands.size());

    for (const Command* command :
         providerCommands) {
        if (command != nullptr &&
            command->enabled &&
            !IsUser(command->source)) {
            candidates.push_back(
                command);
        }
    }

    std::stable_sort(
        candidates.begin(),
        candidates.end(),
        [](const Command* left,
           const Command* right) {
            return SourcePriority(
                       left->source) >
                SourcePriority(
                    right->source);
        });

    // Resolve Provider-vs-Provider duplicates before user shortcuts are
    // considered. This makes the Provider representative stable: adding a
    // user shortcut for the winning Start Menu .lnk must not make a lower
    // App Paths/PATH .exe for the same discovered application reappear.
    std::vector<const Command*>
        providerRepresentatives;

    providerRepresentatives.reserve(
        candidates.size());

    for (const Command* command :
         candidates) {
        const std::wstring packageFamily =
            PackagedFamilyForAppPaths(*command);
        if (!packageFamily.empty() &&
            HasPackagedFamily(candidates, packageFamily)) {
            IncrementSuppressed(result.stats, command->source);
            continue;
        }

        const bool providerDuplicate =
            std::any_of(
                providerRepresentatives.begin(),
                providerRepresentatives.end(),
                [&](const Command* existing) {
                    return existing != nullptr &&
                        IsDuplicateOf(
                            *command,
                            *existing);
                });

        if (providerDuplicate) {
            IncrementSuppressed(
                result.stats,
                command->source);
            continue;
        }

        providerRepresentatives.push_back(
            command);
    }

    // User shortcuts are authoritative only after the Provider view has been
    // canonicalized. Exact-target user shortcuts suppress the Provider
    // representative without reviving an already-suppressed lower Provider.
    for (const Command* command :
         providerRepresentatives) {
        const bool duplicateWithUser =
            std::any_of(
                result.commands.begin(),
                result.commands.end(),
                [&](const Command& existing) {
                    return IsDuplicateOf(
                        *command,
                        existing);
                });

        if (duplicateWithUser) {
            IncrementSuppressed(
                result.stats,
                command->source);
            continue;
        }

        result.commands.push_back(
            *command);

        IncrementAccepted(
            result.stats,
            command->source);
    }

    return result;
}

CommandMergeResult
MergeCommands(
    const std::vector<Command>& userCommands,
    const std::vector<Command>& providerCommands) {

    std::vector<const Command*>
        providerViews;

    providerViews.reserve(
        providerCommands.size());

    for (const auto& command :
         providerCommands) {
        providerViews.push_back(
            &command);
    }

    return MergeCommandViews(
        userCommands,
        providerViews);
}

} // namespace altrun
