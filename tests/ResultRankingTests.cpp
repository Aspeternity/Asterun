#include "core/ProviderIds.hpp"
#include "core/ResultRanking.hpp"

#include <cassert>
#include <iostream>
#include <utility>

using namespace altrun;

namespace {

LauncherResult Result(
    ResultKind kind,
    std::string provider,
    std::wstring title,
    std::wstring subtitle,
    std::wstring target) {

    LauncherResult result;
    result.kind = kind;
    result.providerId =
        std::move(provider);
    result.title =
        std::move(title);
    result.subtitle =
        std::move(subtitle);
    result.target =
        std::move(target);
    return result;
}

} // namespace

int main() {
    const auto file =
        Result(
            ResultKind::File,
            std::string(
                providers::
                    kEverythingFilesystem),
            L"paper.docx",
            L"D:\\Research\\CKD",
            L"D:\\Research\\CKD\\paper.docx");

    LauncherResult exact = file;
    LauncherResult stem = file;
    LauncherResult contains = file;
    LauncherResult path = file;

    assert(RankDynamicResultText(
        exact,
        L"paper.docx"));
    assert(RankDynamicResultText(
        stem,
        L"paper"));
    assert(RankDynamicResultText(
        contains,
        L"aper"));
    assert(RankDynamicResultText(
        path,
        L"research"));

    assert(
        exact.relevanceMatch.kind ==
        relevance::MatchKind::Exact);
    assert(
        exact.relevanceMatch.field ==
        relevance::MatchField::Title);
    assert(
        stem.relevanceMatch.kind ==
        relevance::MatchKind::Exact);
    assert(
        stem.relevanceMatch.field ==
        relevance::MatchField::FileStem);
    assert(BetterLauncherResult(
        exact,
        stem));
    assert(BetterLauncherResult(
        stem,
        contains));

    LauncherResult multi = file;
    assert(RankDynamicResultText(
        multi,
        L"paper docx"));

    LauncherResult syntax = file;
    assert(RankDynamicResultText(
        syntax,
        L"ext:docx"));

    // Everything no longer runs a second permissive fuzzy regime.
    assert(
        ScoreDynamicResultText(
            file,
            L"p") == 0);

    const auto internal =
        Result(
            ResultKind::File,
            std::string(
                providers::
                    kEverythingFilesystem),
            L"PlatformExperienceShell.exe",
            L"C:\\Windows\\SystemApps",
            L"C:\\Windows\\SystemApps\\PlatformExperienceShell.exe");

    assert(
        ScoreDynamicResultText(
            internal,
            L"h") == 0);

    assert(!relevance::
        ShouldRunDynamicFilesystemQuery(
            L"h"));
    assert(relevance::
        ShouldRunDynamicFilesystemQuery(
            L"he"));
    assert(relevance::
        ShouldRunDynamicFilesystemQuery(
            L"ext:exe"));

    LauncherResult user;
    user.kind = ResultKind::UserCommand;
    user.providerId = "user.commands";
    user.title = L"paper";
    user.target = L"paper.exe";
    user.surfaceClass =
        LaunchSurfaceClass::UserCommand;
    user.relevanceMatch = {
        relevance::MatchKind::Prefix,
        relevance::MatchField::Keyword,
        880,
        false,
    };

    LauncherResult app;
    app.kind = ResultKind::Application;
    app.providerId =
        std::string(
            providers::kStartMenu);
    app.title = L"paper";
    app.target = L"paper.exe";
    app.surfaceClass =
        LaunchSurfaceClass::
            PrimaryApplication;
    app.relevanceMatch =
        user.relevanceMatch;

    LauncherResult auxiliary = app;
    auxiliary.target = L"helper.exe";
    auxiliary.surfaceClass =
        LaunchSurfaceClass::Auxiliary;

    assert(BetterLauncherResult(
        user,
        app));
    assert(BetterLauncherResult(
        app,
        auxiliary));

    assert(
        ProviderRankWeight(
            providers::kStartMenu) >
        ProviderRankWeight(
            providers::kPath));

    std::cout
        << "Unified result ranking tests passed\n";
    return 0;
}
