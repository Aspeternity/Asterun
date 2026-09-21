#include "ContextActions.hpp"

#include "ShortcutEditorModel.hpp"

#include <cwctype>
#include <filesystem>
#include <string>

namespace altrun {
namespace {

[[nodiscard]] std::wstring
TrimWide(std::wstring_view value) {
    std::size_t first = 0;
    std::size_t last = value.size();

    while (first < last &&
           std::iswspace(value[first])) {
        ++first;
    }

    while (last > first &&
           std::iswspace(value[last - 1])) {
        --last;
    }

    return std::wstring(
        value.substr(first, last - first));
}

[[nodiscard]] bool
HasNonFilesystemScheme(
    std::wstring_view value) {
    const auto colon =
        value.find(L':');

    if (colon == std::wstring_view::npos) {
        return false;
    }

    if (colon == 1 &&
        std::iswalpha(value[0])) {
        return false;
    }

    if (colon == 0 ||
        !std::iswalpha(value[0])) {
        return false;
    }

    for (std::size_t index = 1;
         index < colon;
         ++index) {
        const wchar_t c = value[index];

        if (!std::iswalnum(c) &&
            c != L'+' &&
            c != L'-' &&
            c != L'.') {
            return false;
        }
    }

    return true;
}

[[nodiscard]] bool
CanRunElevated(
    const LauncherResult& result) {
    if (result.kind ==
            ResultKind::Folder ||
        result.kind ==
            ResultKind::Action) {
        return false;
    }

    const std::wstring value =
        TrimWide(result.target);

    if (value.empty() ||
        HasNonFilesystemScheme(value)) {
        return false;
    }

    std::wstring extension =
        std::filesystem::path(value)
            .extension()
            .wstring();

    for (auto& ch : extension) {
        ch = static_cast<wchar_t>(
            std::towlower(ch));
    }

    return extension == L".exe" ||
        extension == L".com" ||
        extension == L".bat" ||
        extension == L".cmd" ||
        extension == L".msi" ||
        extension == L".lnk";
}

[[nodiscard]] bool
PrimaryActionAvailable(
    const LauncherResult& result) {
    if (result.action.kind ==
        LauncherActionKind::
            ExecuteCommand) {
        return result.action.commandIndex !=
            static_cast<std::size_t>(-1);
    }

    return !result.action.payload.empty() ||
        !result.target.empty();
}

} // namespace

bool CanRevealTargetInExplorer(
    std::wstring_view target) {
    const std::wstring value =
        TrimWide(target);

    if (value.empty() ||
        HasNonFilesystemScheme(value)) {
        return false;
    }

    if (value.starts_with(L"\\") ||
        value.starts_with(L"//") ||
        value.starts_with(L".\\") ||
        value.starts_with(L"./") ||
        value.starts_with(L"..\\") ||
        value.starts_with(L"../")) {
        return true;
    }

    if (value.size() >= 2 &&
        std::iswalpha(value[0]) &&
        value[1] == L':') {
        return true;
    }

    if (value.find(L'\\') !=
            std::wstring::npos ||
        value.find(L'/') !=
            std::wstring::npos) {
        return true;
    }

    const auto dot =
        value.find_last_of(L'.');

    return dot != std::wstring::npos &&
        dot > 0 &&
        dot + 1 < value.size();
}

LauncherContextActions
EvaluateLauncherContextActions(
    const LauncherResult& result,
    bool currentFileManagerAvailable) {
    LauncherContextActions actions;

    actions.primary =
        PrimaryActionAvailable(result);
    actions.runAsAdministrator =
        actions.primary &&
        CanRunElevated(result);

    const bool userShortcut =
        result.kind ==
        ResultKind::UserCommand;

    actions.editShortcut =
        userShortcut;
    actions.deleteShortcut =
        userShortcut;

    actions.addAsShortcut =
        !userShortcut &&
        !result.target.empty() &&
        (result.kind ==
             ResultKind::Application ||
         result.kind ==
             ResultKind::File ||
         result.kind ==
             ResultKind::Folder);

    actions.navigateCurrentFileManager =
        currentFileManagerAvailable &&
        result.kind ==
            ResultKind::Folder;

    actions.locateInExplorer =
        result.kind !=
            ResultKind::Folder &&
        CanRevealTargetInExplorer(
            result.target);

    if (result.action.kind ==
        LauncherActionKind::
            ExecuteCommand) {
        actions.copyTarget =
            !result.target.empty();
    } else {
        actions.copyTarget =
            !result.action.payload.empty() ||
            !result.target.empty();
    }

    return actions;
}

Command ShortcutSeedFromLauncherResult(
    const LauncherResult& result) {
    Command command;

    command.keyword.clear();

    if (result.kind ==
            ResultKind::Application &&
        !result.subtitle.empty()) {
        command.title =
            result.subtitle;
    } else {
        command.title =
            result.title;
    }

    command.target =
        result.target;

    command.type =
        result.kind ==
                ResultKind::Folder
            ? CommandType::Folder
            : InferShortcutCommandType(
                  result.target);

    command.icon = L"auto";
    command.enabled = true;
    command.runAsAdmin = false;
    command.pinned = false;
    command.source =
        CommandSource::User;
    command.basePriority = 120;

    return command;
}

} // namespace altrun
