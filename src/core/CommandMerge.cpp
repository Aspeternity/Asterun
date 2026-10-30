#include "CommandMerge.hpp"

#include <algorithm>
#include <cwctype>
#include <string>
#include <string_view>
#include <unordered_map>
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
    const std::wstring existingTarget =
        NormalizeTarget(
            existing.target);
    const bool userBoundary =
        IsUser(existing.source) ||
        IsUser(incoming.source);

    const bool hasCanonicalPair =
        !incoming.canonicalIdentity.empty() &&
        !existing.canonicalIdentity.empty();

    // Canonical identity describes the resolved activation action including
    // arguments. Across the User boundary it is authoritative in both
    // directions: equality proves duplication, while two different strong
    // identities prove that an otherwise identical path represents a
    // different explicit launch action.
    if (hasCanonicalPair) {
        if (incoming.canonicalIdentity ==
            existing.canonicalIdentity) {
            return true;
        }

        if (userBoundary) {
            return false;
        }
    }

    if (!incomingTarget.empty() &&
        incomingTarget ==
            existingTarget) {
        if (!userBoundary) {
            return true;
        }

        // Preserve exact-target User promotion for legacy/unresolved entries,
        // but do not collapse an explicit User action that adds different
        // outer arguments to the same executable/shortcut surface.
        return Trim(incoming.arguments) ==
            Trim(existing.arguments);
    }

    // Do not fall back to title/keyword heuristics across the User boundary.
    // Different explicit user actions must remain independent unless target
    // or canonical activation identity proves they are the same launch.
    if (userBoundary) {
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

// The packaged application that an MSIX execution alias opens, when the
// Windows Apps provider publishes it.
const Command* PackagedAliasOwner(
    const Command& command,
    const std::vector<const Command*>& candidates) {
    const std::wstring& appId = command.runByName.packagedAppId;
    if (command.source != CommandSource::AppPaths || appId.empty()) {
        return nullptr;
    }

    const std::wstring identity = L"aumid:" + Lower(appId);
    const auto owner = std::find_if(
        candidates.begin(), candidates.end(), [&](const Command* candidate) {
            return candidate->source == CommandSource::PackagedApp &&
                Lower(candidate->canonicalIdentity) == identity;
        });
    return owner == candidates.end() ? nullptr : *owner;
}

// A run-by-name entry folded into the application it opens keeps its name
// as an alias, so "mspaint" or "winword" still reaches Paint or Word.
void FoldRunByNameAlias(
    const Command& alias,
    const Command& owner,
    std::vector<std::wstring>& folded) {
    if (alias.source != CommandSource::AppPaths) {
        return;
    }

    const std::wstring name = Lower(Trim(alias.title));
    const std::wstring compact = Compact(name);
    const std::wstring ownerTitle = Compact(owner.title);
    if (compact.empty() || compact == Compact(owner.keyword) ||
        ownerTitle.starts_with(compact)) {
        return;
    }

    const auto sameName = [&](const std::wstring& existing) {
        return Compact(existing) == compact;
    };
    if (std::any_of(owner.aliases.begin(), owner.aliases.end(), sameName) ||
        std::any_of(folded.begin(), folded.end(), sameName)) {
        return;
    }

    folded.push_back(name);
}

bool PathWithin(
    std::wstring_view path,
    std::wstring_view root) {
    return !root.empty() && path.size() > root.size() &&
        path.starts_with(root) && path[root.size()] == L'\\';
}

// A surface that the platform's own application lists (Start Menu or
// AppsFolder) publishes, reduced to what identifies its product.
struct ListedSurface {
    std::wstring packageFamily;
    std::wstring path;
};

ListedSurface DescribeListedSurface(
    const Command& command) {
    ListedSurface surface;
    const std::wstring identity = Lower(command.canonicalIdentity);

    if (identity.starts_with(L"aumid:")) {
        const std::size_t bang = identity.find(L'!');
        if (bang != std::wstring::npos && bang > 6) {
            surface.packageFamily = identity.substr(6, bang - 6);
        }
    } else if (identity.starts_with(L"file:")) {
        surface.path = identity.substr(5, identity.find(L"|args:") - 5);
    }

    return surface;
}

// App Paths is the shell's run-by-name table, not an application list. An
// entry that survived de-duplication is not one of the listed applications;
// it is a component when Windows services it, it lives in Common Files, its
// package declares it a server, or its product already has a listed surface.
// (A listed shortcut that runs the same EXE with arguments is a distinct
// action, so the plain EXE is not folded into it.) Components stay reachable
// by their exact name (as with Win+R) and by past use, but no longer surface
// on a prefix of a shared product word.
void ApplyRunByNamePolicy(
    std::vector<Command>& commands) {
    std::vector<ListedSurface> listed;

    for (const auto& command : commands) {
        if ((command.source == CommandSource::StartMenu ||
             command.source == CommandSource::PackagedApp) &&
            command.catalogVisibility != CatalogVisibility::Hidden) {
            listed.push_back(DescribeListedSurface(command));
        }
    }

    for (auto& command : commands) {
        if (command.source != CommandSource::AppPaths) {
            continue;
        }

        const RunByNameEvidence& evidence = command.runByName;

        if (evidence.packageServer) {
            command.catalogVisibility = CatalogVisibility::Hidden;
            continue;
        }

        bool component = false;

        if (!evidence.packagedAppId.empty()) {
            // A package with no listed application is headless: its
            // aliases (wsl, winget) are the product itself.
            const std::wstring appId = Lower(evidence.packagedAppId);
            const std::wstring family = appId.substr(0, appId.find(L'!'));
            component = std::any_of(
                listed.begin(), listed.end(), [&](const ListedSurface& surface) {
                    return surface.packageFamily == family;
                });
        } else if (evidence.osComponent || evidence.sharedComponent) {
            component = true;
        } else if (!evidence.productRoot.empty()) {
            const std::wstring root = Lower(evidence.productRoot);
            component = std::any_of(
                listed.begin(), listed.end(), [&](const ListedSurface& surface) {
                    return PathWithin(surface.path, root);
                });
        }

        if (!component ||
            command.catalogVisibility == CatalogVisibility::Hidden) {
            continue;
        }

        command.catalogVisibility = CatalogVisibility::StrongMatchOnly;
        command.distinctiveTokens.clear();
    }
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

    // Which representative a suppressed Provider entry collapsed into, and
    // the run-by-name aliases each representative gains from it.
    std::unordered_map<const Command*, const Command*>
        representativeOf;
    std::unordered_map<const Command*, std::vector<std::wstring>>
        foldedAliases;

    const auto fold =
        [&](const Command& alias,
            const Command* owner) {
            const auto resolved =
                representativeOf.find(owner);
            if (resolved != representativeOf.end()) {
                owner = resolved->second;
            }
            FoldRunByNameAlias(
                alias,
                *owner,
                foldedAliases[owner]);
        };

    for (const Command* command :
         candidates) {
        if (const Command* owner =
                PackagedAliasOwner(*command, candidates)) {
            fold(*command, owner);
            IncrementSuppressed(result.stats, command->source);
            continue;
        }

        const std::wstring packageFamily =
            command->runByName.packagedAppId.empty()
                ? PackagedFamilyForAppPaths(*command)
                : std::wstring{};
        if (!packageFamily.empty() &&
            HasPackagedFamily(candidates, packageFamily)) {
            IncrementSuppressed(result.stats, command->source);
            continue;
        }

        const auto providerDuplicate =
            std::find_if(
                providerRepresentatives.begin(),
                providerRepresentatives.end(),
                [&](const Command* existing) {
                    return existing != nullptr &&
                        IsDuplicateOf(
                            *command,
                            *existing);
                });

        if (providerDuplicate !=
            providerRepresentatives.end()) {
            representativeOf[command] =
                *providerDuplicate;
            fold(*command, *providerDuplicate);
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

        const auto folded =
            foldedAliases.find(command);
        if (folded != foldedAliases.end()) {
            auto& aliases =
                result.commands.back().aliases;
            aliases.insert(
                aliases.end(),
                folded->second.begin(),
                folded->second.end());
        }

        IncrementAccepted(
            result.stats,
            command->source);
    }

    ApplyRunByNamePolicy(
        result.commands);

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
