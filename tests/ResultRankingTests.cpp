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
    // Learned acronym order must survive the unified result merge, and the
    // comparator must stay transitive across three candidates and ties.
    {
        std::vector<LauncherResult> initials;
        for (const int score : {930, 808, 809}) {
            for (const int usage : {0, 16, 24, 32}) {
                LauncherResult result;
                result.kind = ResultKind::Application;
                result.surfaceClass = LaunchSurfaceClass::PrimaryApplication;
                result.relevanceMatch = {relevance::MatchKind::Initials,
                    relevance::MatchField::Title, score, false};
                result.usageScore = usage;
                initials.push_back(result);
            }
        }
        assert(BetterLauncherResult(initials[5], initials[0]));
        for (const auto& a : initials) {
            assert(!BetterLauncherResult(a, a));
            for (const auto& b : initials) {
                if (BetterLauncherResult(a, b)) {
                    assert(!BetterLauncherResult(b, a));
                    for (const auto& c : initials) {
                        if (BetterLauncherResult(b, c)) {
                            assert(BetterLauncherResult(a, c));
                        }
                    }
                }
            }
        }
    }

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

    LauncherResult versioned =
        Result(
            ResultKind::File,
            std::string(
                providers::
                    kEverythingFilesystem),
            L"v2rayN.exe",
            L"D:\\v2rayN-windows-64",
            L"D:\\v2rayN-windows-64\\v2rayN.exe");

    assert(RankDynamicResultText(
        versioned,
        L"v2"));
    assert(
        versioned.relevanceMatch.kind ==
        relevance::MatchKind::Prefix);

    assert(BetterLauncherResult(
        exact,
        stem));
    assert(BetterLauncherResult(
        stem,
        contains));

    // Two-character CJK queries must not inherit the short-ASCII
    // strong-match gate. Everything may return a filename where the query is
    // a middle substring; the launcher must preserve that valid result.
    const auto chineseFolder =
        Result(
            ResultKind::Folder,
            std::string(
                providers::
                    kEverythingFilesystem),
            L"系统男主",
            L"C:\\Media",
            L"C:\\Media\\系统男主");

    LauncherResult chinesePrefix =
        chineseFolder;
    LauncherResult chineseSubstring =
        chineseFolder;

    assert(RankDynamicResultText(
        chinesePrefix,
        L"系统"));
    assert(RankDynamicResultText(
        chineseSubstring,
        L"男主"));

    // Match classification around CJK boundaries can vary with the host
    // C library's wide-character classification. Exercise the actual policy
    // directly so the regression is specifically about short non-ASCII
    // substring admission, while the short-ASCII noise gate stays intact.
    const relevance::Match cjkSubstringMatch{
        relevance::MatchKind::Substring,
        relevance::MatchField::Title,
        688,
        false,
    };
    assert(relevance::
        AdmitLaunchSurface(
            LaunchSurfaceClass::
                FilesystemItem,
            L"男主",
            cjkSubstringMatch));
    assert(!relevance::
        AdmitLaunchSurface(
            LaunchSurfaceClass::
                FilesystemItem,
            L"he",
            cjkSubstringMatch));

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

    LauncherResult shorter = app;
    shorter.relevanceMatch.score = 876;
    LauncherResult familiar = app;
    familiar.relevanceMatch.score = 866;
    familiar.usageScore = 16;
    assert(BetterLauncherResult(familiar, shorter));
    familiar.usageScore = 0;
    assert(BetterLauncherResult(shorter, familiar));

    familiar.usageScore = 100000;
    familiar.relevanceMatch.score = 820;
    assert(BetterLauncherResult(shorter, familiar));

    familiar.relevanceMatch.kind = relevance::MatchKind::Substring;
    familiar.relevanceMatch.score = 950;
    assert(BetterLauncherResult(shorter, familiar));

    familiar.relevanceMatch = shorter.relevanceMatch;
    familiar.relevanceMatch.field = relevance::MatchField::Subtitle;
    assert(BetterLauncherResult(shorter, familiar));

    assert(
        ProviderRankWeight(
            providers::kStartMenu) >
        ProviderRankWeight(
            providers::kPath));

    std::cout
        << "Unified result ranking tests passed\n";
    return 0;
}
