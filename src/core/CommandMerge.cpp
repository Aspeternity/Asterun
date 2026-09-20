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

    for (const Command* command :
         candidates) {

        const bool duplicate =
            std::any_of(
                result.commands.begin(),
                result.commands.end(),
                [&](const Command& existing) {
                    return IsDuplicateOf(
                        *command,
                        existing);
                });

        if (duplicate) {
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
