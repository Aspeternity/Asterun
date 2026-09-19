#include "core/ClipboardAction.hpp"

#include <cassert>
#include <iostream>

using namespace altrun;

int main() {
    {
        const auto results =
            BuildClipboardActionResults(
                L"copy hello world",
                10,
                L"Copy text");

        assert(results.size() == 1);
        assert(
            results[0].providerId ==
            "builtin.clipboard");
        assert(
            results[0].kind ==
            ResultKind::Action);
        assert(
            results[0].action.kind ==
            LauncherActionKind::
                CopyText);
        assert(
            results[0].action.payload ==
            L"hello world");
        assert(
            results[0].target ==
            L"hello world");
    }

    {
        const auto results =
            BuildClipboardActionResults(
                L"CLIP \u4e2d\u6587 text",
                10,
                L"Copy text");

        assert(results.size() == 1);
        assert(
            results[0].action.payload ==
            L"\u4e2d\u6587 text");
    }

    {
        const auto results =
            BuildClipboardActionResults(
                L"\u590d\u5236  D:\\Folder With Spaces ",
                10,
                L"Copy text");

        assert(results.size() == 1);
        assert(
            results[0].action.payload ==
            L"D:\\Folder With Spaces");
    }

    {
        assert(
            BuildClipboardActionResults(
                L"copy",
                10,
                L"Copy text")
                .empty());

        assert(
            BuildClipboardActionResults(
                L"copyright notice",
                10,
                L"Copy text")
                .empty());

        assert(
            BuildClipboardActionResults(
                L"copy text",
                0,
                L"Copy text")
                .empty());
    }

    std::cout
        << "Clipboard action tests passed\n";
    return 0;
}
