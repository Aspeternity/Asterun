#include "LaunchCandidate.hpp"

#include <algorithm>
#include <cwctype>
#include <initializer_list>
#include <string>

namespace altrun {
namespace {

[[nodiscard]] std::wstring Lower(
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

[[nodiscard]] std::wstring Compact(
    std::wstring_view value) {

    std::wstring result;
    result.reserve(value.size());

    for (const wchar_t ch : value) {
        if (std::iswalnum(ch) ||
            ch >= 0x4E00) {
            result.push_back(
                static_cast<wchar_t>(
                    std::towlower(ch)));
        }
    }

    return result;
}

[[nodiscard]] bool ContainsAny(
    std::wstring_view value,
    std::initializer_list<
        std::wstring_view> needles) {

    return std::any_of(
        needles.begin(),
        needles.end(),
        [&](std::wstring_view needle) {
            return value.find(needle) !=
                std::wstring_view::npos;
        });
}

[[nodiscard]] bool EndsWithAny(
    std::wstring_view value,
    std::initializer_list<
        std::wstring_view> suffixes) {

    return std::any_of(
        suffixes.begin(),
        suffixes.end(),
        [&](std::wstring_view suffix) {
            return value.ends_with(
                suffix);
        });
}

[[nodiscard]] bool StartsWithAny(
    std::wstring_view value,
    std::initializer_list<
        std::wstring_view> prefixes) {

    return std::any_of(
        prefixes.begin(),
        prefixes.end(),
        [&](std::wstring_view prefix) {
            return value.starts_with(
                prefix);
        });
}

[[nodiscard]] std::wstring TargetLeafStem(
    std::wstring_view target) {

    const auto slash =
        target.find_last_of(L"\\/");

    std::wstring_view leaf =
        slash == std::wstring_view::npos
            ? target
            : target.substr(slash + 1);

    const auto query =
        leaf.find_first_of(L"?#");

    if (query != std::wstring_view::npos) {
        leaf = leaf.substr(0, query);
    }

    const auto dot =
        leaf.find_last_of(L'.');

    if (dot != std::wstring_view::npos &&
        dot != 0) {
        leaf = leaf.substr(0, dot);
    }

    return std::wstring(leaf);
}

[[nodiscard]] bool HasUriScheme(
    std::wstring_view target) {

    const auto colon =
        target.find(L':');

    if (colon ==
            std::wstring_view::npos ||
        colon == 0) {
        return false;
    }

    if (colon == 1 &&
        std::iswalpha(target[0])) {
        return false;
    }

    if (!std::iswalpha(target[0])) {
        return false;
    }

    for (std::size_t index = 1;
         index < colon;
         ++index) {

        const wchar_t ch =
            target[index];

        if (!std::iswalnum(ch) &&
            ch != L'+' &&
            ch != L'-' &&
            ch != L'.') {
            return false;
        }
    }

    return true;
}

[[nodiscard]] bool IsExecutableKind(
    LaunchTargetKind kind) noexcept {

    return
        kind ==
            LaunchTargetKind::
                GuiExecutable ||
        kind ==
            LaunchTargetKind::
                ConsoleExecutable ||
        kind ==
            LaunchTargetKind::
                CommandScript;
}

[[nodiscard]] LaunchSurfaceClass
NormalizeSurface(
    LaunchSurfaceClass surface,
    LaunchTargetKind targetKind) {

    if (targetKind ==
        LaunchTargetKind::
            SystemControl) {
        return LaunchSurfaceClass::
            SystemUtility;
    }

    if ((targetKind ==
             LaunchTargetKind::
                 ConsoleExecutable ||
         targetKind ==
             LaunchTargetKind::
                 CommandScript) &&
        surface ==
            LaunchSurfaceClass::
                PrimaryApplication) {
        return LaunchSurfaceClass::
            CommandLineTool;
    }

    return surface;
}

} // namespace

bool IsDocumentationLikeTitle(
    std::wstring_view title) {

    const std::wstring lower =
        Lower(title);
    const std::wstring compact =
        Compact(title);

    return
        ContainsAny(
            lower,
            {
                L"documentation",
                L"release notes",
                L"what's new",
                L"whats new",
                L"new features",
                L"readme",
                L"manual",
                L"user guide",
                L"getting started",
                L"online help",
                L"website",
                L"support page",
                L"文档",
                L"帮助",
                L"手册",
                L"说明书",
                L"使用说明",
                L"版本说明",
                L"发行说明",
                L"发布说明",
                L"更新说明",
                L"更新日志",
                L"新功能",
                L"新特性",
                L"最新版本",
            }) ||
        EndsWithAny(
            compact,
            {
                L"help",
                L"manual",
                L"documentation",
                L"docs",
                L"readme",
                L"releasenotes",
                L"whatsnew",
                L"newfeatures",
                L"website",
                L"support",
            });
}

bool IsMaintenanceLikeTitle(
    std::wstring_view title) {

    const std::wstring lower =
        Lower(title);
    const std::wstring compact =
        Compact(title);

    return
        ContainsAny(
            lower,
            {
                L"uninstall",
                L"uninstaller",
                L"repair",
                L"modify installation",
                L"update helper",
                L"update service",
                L"setup wizard",
                L"卸载",
                L"修复",
                L"更新程序",
                L"升级程序",
            }) ||
        StartsWithAny(
            compact,
            {
                L"unins",
                L"uninst",
                L"setup",
            }) ||
        EndsWithAny(
            compact,
            {
                L"uninstall",
                L"uninstaller",
                L"installer",
                L"repair",
                L"updater",
                L"updatehelper",
                L"updateservice",
            });
}

bool LooksLikeWebTarget(
    std::wstring_view target) {

    const std::wstring lower =
        Lower(target);

    return
        lower.starts_with(L"http://") ||
        lower.starts_with(L"https://") ||
        lower.starts_with(L"ftp://") ||
        lower.starts_with(L"mailto:") ||
        lower.starts_with(L"www.");
}

bool LooksLikeDocumentTarget(
    std::wstring_view target) {

    const std::wstring lower =
        Lower(target);

    return EndsWithAny(
        lower,
        {
            L".chm",
            L".hlp",
            L".html",
            L".htm",
            L".pdf",
            L".txt",
            L".rtf",
            L".md",
            L".url",
        });
}

LaunchTargetKind InferTextTargetKind(
    std::wstring_view target) {

    const std::wstring lower =
        Lower(target);

    if (LooksLikeWebTarget(lower)) {
        return LaunchTargetKind::
            WebUri;
    }

    if (LooksLikeDocumentTarget(lower)) {
        return LaunchTargetKind::
            Document;
    }

    if (lower.ends_with(L".msc") ||
        lower.ends_with(L".cpl")) {
        return LaunchTargetKind::
            SystemControl;
    }

    if (lower.ends_with(L".bat") ||
        lower.ends_with(L".cmd") ||
        lower.ends_with(L".ps1")) {
        return LaunchTargetKind::
            CommandScript;
    }

    if (lower.ends_with(L".com")) {
        return LaunchTargetKind::
            ConsoleExecutable;
    }

    if (lower.starts_with(L"shell:") ||
        lower.starts_with(L"::{") ||
        lower.find(L"!") !=
            std::wstring::npos) {
        return LaunchTargetKind::
            ShellApplication;
    }

    if (HasUriScheme(lower)) {
        // Non-web activation URIs are valid application/system activation
        // targets (for example ms-settings:). They are not documents.
        return LaunchTargetKind::
            ShellApplication;
    }

    return LaunchTargetKind::Unknown;
}

LaunchAdmission EvaluateLaunchCandidate(
    const LaunchCandidate& candidate) {

    LaunchAdmission decision;
    decision.surface =
        NormalizeSurface(
            candidate.surface,
            candidate.targetKind);

    if (candidate.title.empty() ||
        candidate.target.empty()) {
        return decision;
    }

    const std::wstring targetLeaf =
        TargetLeafStem(
            candidate.target);

    const LaunchSurfaceClass targetRole =
        ClassifyApplicationSurface(
            targetLeaf,
            candidate.target);

    if (IsDocumentationLikeTitle(
            candidate.title) ||
        IsMaintenanceLikeTitle(
            candidate.title) ||
        IsDocumentationLikeTitle(
            targetLeaf) ||
        IsMaintenanceLikeTitle(
            targetLeaf) ||
        candidate.surface ==
            LaunchSurfaceClass::
                Auxiliary ||
        candidate.surface ==
            LaunchSurfaceClass::
                Maintenance ||
        targetRole ==
            LaunchSurfaceClass::
                Auxiliary ||
        targetRole ==
            LaunchSurfaceClass::
                Maintenance ||
        candidate.targetKind ==
            LaunchTargetKind::
                Document ||
        candidate.targetKind ==
            LaunchTargetKind::
                WebUri) {
        return decision;
    }

    switch (candidate.source) {
    case LaunchCandidateSource::StartMenu:
        decision.admit =
            IsExecutableKind(
                candidate.targetKind) ||
            candidate.targetKind ==
                LaunchTargetKind::
                    ShellApplication ||
            candidate.targetKind ==
                LaunchTargetKind::
                    SystemControl;
        break;

    case LaunchCandidateSource::AppsFolder:
        decision.admit =
            candidate.targetKind ==
                LaunchTargetKind::
                    ShellApplication ||
            candidate.targetKind ==
                LaunchTargetKind::
                    SystemControl ||
            IsExecutableKind(
                candidate.targetKind);
        break;

    case LaunchCandidateSource::AppPaths:
        decision.admit =
            candidate.targetKind ==
                LaunchTargetKind::
                    GuiExecutable ||
            candidate.targetKind ==
                LaunchTargetKind::
                    ConsoleExecutable;
        break;

    case LaunchCandidateSource::Path:
        decision.admit =
            IsExecutableKind(
                candidate.targetKind);
        decision.surface =
            LaunchSurfaceClass::
                CommandLineTool;
        break;
    }

    return decision;
}

} // namespace altrun
