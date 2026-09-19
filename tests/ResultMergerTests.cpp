#include "core/ProviderIds.hpp"
#include "core/ResultMerger.hpp"
#include "core/ResultRanking.hpp"

#include <algorithm>
#include <cassert>
#include <iostream>
#include <utility>
#include <vector>

using namespace altrun;

namespace {

LauncherResult MakeResult(
    std::wstring id,
    std::string provider,
    ResultKind kind,
    std::wstring title,
    std::wstring target,
    int score) {
    LauncherResult result;
    result.id = std::move(id);
    result.providerId =
        std::move(provider);
    result.kind = kind;
    result.title = std::move(title);
    result.target = std::move(target);
    result.score = score;
    return result;
}

} // namespace

int main() {
    const std::vector<LauncherResult>
        staticResults{
            MakeResult(
                L"user-1",
                "user.commands",
                ResultKind::UserCommand,
                L"paper",
                L"C:\\Tools\\paper.exe",
                1260),
            MakeResult(
                L"app-1",
                std::string(
                    providers::kStartMenu),
                ResultKind::Application,
                L"Paper Viewer",
                L"C:\\Apps\\viewer.exe",
                610),
            MakeResult(
                L"app-2",
                std::string(
                    providers::kPath),
                ResultKind::Application,
                L"Calculator",
                L"C:\\Windows\\System32\\calc.exe",
                560),
        };

    LauncherResult exactFile =
        MakeResult(
            L"everything.filesystem:D:\\Research\\paper.docx",
            std::string(
                providers::
                    kEverythingFilesystem),
            ResultKind::File,
            L"paper.docx",
            L"D:\\Research\\paper.docx",
            0);
    exactFile.subtitle =
        L"D:\\Research";
    exactFile.score =
        ScoreDynamicResultText(
            exactFile,
            L"paper");

    const std::vector<LauncherResult>
        dynamicResults{
            exactFile,
            MakeResult(
                L"everything.filesystem:C:\\windows\\system32\\CALC.EXE",
                std::string(
                    providers::
                        kEverythingFilesystem),
                ResultKind::File,
                L"calc.exe",
                L"C:\\windows\\system32\\CALC.EXE",
                1100),
        };

    const auto merged =
        MergeLauncherResultsRanked(
            staticResults,
            dynamicResults,
            10);

    assert(merged.size() == 4);

    // Explicit user commands remain authoritative, while an exact file-name
    // match is allowed to outrank fuzzy application results.
    assert(
        merged[0].kind ==
        ResultKind::UserCommand);
    assert(
        merged[1].kind ==
        ResultKind::File);
    assert(
        merged[1].title ==
        L"paper.docx");
    assert(
        merged[2].title ==
        L"Paper Viewer");

    // The Everything copy of calc.exe is removed because a static command
    // already owns the same target (case-insensitive).
    assert(
        std::none_of(
            merged.begin(),
            merged.end(),
            [](const LauncherResult&
                    result) {
                return result.id.starts_with(
                    L"everything.filesystem:C:\\windows");
            }));

    const auto limited =
        MergeLauncherResultsRanked(
            staticResults,
            dynamicResults,
            2);

    assert(limited.size() == 2);
    assert(
        limited[0].kind ==
        ResultKind::UserCommand);
    assert(
        limited[1].kind ==
        ResultKind::File);

    // With identical match quality, type/provider weights provide stable,
    // conservative tie breaking rather than dominating match quality.
    const std::vector<LauncherResult>
        ties{
            MakeResult(
                L"folder",
                std::string(
                    providers::
                        kEverythingFilesystem),
                ResultKind::Folder,
                L"Docs",
                L"D:\\Docs",
                800),
            MakeResult(
                L"file",
                std::string(
                    providers::
                        kEverythingFilesystem),
                ResultKind::File,
                L"Docs.txt",
                L"D:\\Docs.txt",
                800),
            MakeResult(
                L"start",
                std::string(
                    providers::kStartMenu),
                ResultKind::Application,
                L"Docs App",
                L"C:\\Apps\\docs.exe",
                800),
        };

    const auto rankedTies =
        MergeLauncherResultsRanked(
            {},
            ties,
            10);

    assert(
        rankedTies[0].kind ==
        ResultKind::Application);
    assert(
        rankedTies[1].kind ==
        ResultKind::Folder);
    assert(
        rankedTies[2].kind ==
        ResultKind::File);

    std::cout
        << "Unified launcher result merger tests passed\n";
    return 0;
}
