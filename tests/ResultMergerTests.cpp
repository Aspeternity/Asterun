#include "core/ResultMerger.hpp"

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
    std::wstring target) {
    LauncherResult result;
    result.id = std::move(id);
    result.providerId =
        std::move(provider);
    result.kind = kind;
    result.title = std::move(title);
    result.target = std::move(target);
    return result;
}

} // namespace

int main() {
    const std::vector<LauncherResult>
        staticResults{
            MakeResult(
                L"app-1",
                "windows.startmenu",
                ResultKind::Application,
                L"Calculator",
                L"C:\\Windows\\System32\\calc.exe"),
            MakeResult(
                L"app-2",
                "windows.path",
                ResultKind::Application,
                L"Git",
                L"C:\\Tools\\git.exe"),
        };

    const std::vector<LauncherResult>
        dynamicResults{
            MakeResult(
                L"everything.filesystem:C:\\Docs\\paper.docx",
                "everything.filesystem",
                ResultKind::File,
                L"paper.docx",
                L"C:\\Docs\\paper.docx"),
            MakeResult(
                L"everything.filesystem:C:\\windows\\system32\\CALC.EXE",
                "everything.filesystem",
                ResultKind::File,
                L"calc.exe",
                L"C:\\windows\\system32\\CALC.EXE"),
        };

    const auto merged =
        MergeLauncherResultsStaticFirst(
            staticResults,
            dynamicResults,
            10);

    assert(merged.size() == 3);
    assert(
        merged[0].title ==
        L"Calculator");
    assert(
        merged[1].title ==
        L"Git");
    assert(
        merged[2].title ==
        L"paper.docx");

    const auto limited =
        MergeLauncherResultsStaticFirst(
            staticResults,
            dynamicResults,
            2);
    assert(limited.size() == 2);
    assert(
        limited[0].providerId ==
        "windows.startmenu");
    assert(
        limited[1].providerId ==
        "windows.path");

    std::cout
        << "Launcher result merger tests passed\n";
    return 0;
}
