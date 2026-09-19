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
    std::wstring target,
    int score = 0) {
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
    result.score = score;
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

    const auto exact =
        ScoreDynamicResultText(
            file,
            L"paper.docx");

    const auto stem =
        ScoreDynamicResultText(
            file,
            L"paper");

    const auto contains =
        ScoreDynamicResultText(
            file,
            L"aper");

    const auto path =
        ScoreDynamicResultText(
            file,
            L"research");

    assert(exact == 1100);
    assert(stem >= 1000);
    assert(stem < exact);
    assert(contains < stem);
    assert(path > 0);
    assert(path < contains);

    const auto multi =
        ScoreDynamicResultText(
            file,
            L"paper docx");
    assert(multi >= stem);

    const auto syntaxFallback =
        ScoreDynamicResultText(
            file,
            L"ext:docx");
    assert(syntaxFallback > 0);

    const auto user =
        Result(
            ResultKind::UserCommand,
            "user.commands",
            L"paper",
            L"My paper command",
            L"paper.exe",
            1000);

    const auto app =
        Result(
            ResultKind::Application,
            std::string(
                providers::kStartMenu),
            L"paper",
            L"Paper App",
            L"paper.exe",
            1000);

    const auto folder =
        Result(
            ResultKind::Folder,
            std::string(
                providers::
                    kEverythingFilesystem),
            L"paper",
            L"D:\\Research",
            L"D:\\Research\\paper",
            1000);

    const auto plainFile =
        Result(
            ResultKind::File,
            std::string(
                providers::
                    kEverythingFilesystem),
            L"paper.txt",
            L"D:\\Research",
            L"D:\\Research\\paper.txt",
            1000);

    assert(
        UnifiedRankScore(user) >
        UnifiedRankScore(app));
    assert(
        UnifiedRankScore(app) >
        UnifiedRankScore(folder));
    assert(
        UnifiedRankScore(folder) >
        UnifiedRankScore(
            plainFile));

    assert(
        ProviderRankWeight(
            providers::kStartMenu) >
        ProviderRankWeight(
            providers::kPath));

    std::cout
        << "Unified result ranking tests passed\n";
    return 0;
}
