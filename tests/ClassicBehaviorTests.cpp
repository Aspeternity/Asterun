#include "core/ClassicBehavior.hpp"

#include <cassert>
#include <iostream>
#include <vector>

using namespace altrun;

int main() {
    using classic_behavior::
        NumericQuickLaunchContext;
    using classic_behavior::
        NumericQuickLaunchDecision;

    assert(
        classic_behavior::
            QuickLaunchIndexForDigit(
                1,
                "one-to-zero") == 0);
    assert(
        classic_behavior::
            QuickLaunchIndexForDigit(
                0,
                "one-to-zero") == 9);

    assert(
        classic_behavior::
            WrappedSelectionIndex(
                9,
                1,
                10,
                true) == 0);
    assert(
        classic_behavior::
            WrappedSelectionIndex(
                0,
                -1,
                10,
                true) == 9);
    assert(
        classic_behavior::
            WrappedSelectionIndex(
                9,
                1,
                10,
                false) == 9);

    NumericQuickLaunchContext context{};
    context.enabled = true;
    context.resultAvailable = true;
    context.queryEmpty = false;

    assert(
        classic_behavior::
            DecideNumericQuickLaunch(
                context) ==
        NumericQuickLaunchDecision::
            DeferExecute);

    context.recentTextInput = true;
    assert(
        classic_behavior::
            DecideNumericQuickLaunch(
                context) ==
        NumericQuickLaunchDecision::
            Text);

    context.recentTextInput = false;
    context.strongContinuation = true;
    assert(
        classic_behavior::
            DecideNumericQuickLaunch(
                context) ==
        NumericQuickLaunchDecision::
            Text);

    context.strongContinuation = false;
    context.queryEmpty = true;
    assert(
        classic_behavior::
            DecideNumericQuickLaunch(
                context) ==
        NumericQuickLaunchDecision::
            Text);

    context.queryEmpty = false;
    context.controlDown = true;
    assert(
        classic_behavior::
            DecideNumericQuickLaunch(
                context) ==
        NumericQuickLaunchDecision::
            ExecuteNow);

    context.controlDown = false;
    context.altDown = true;
    assert(
        classic_behavior::
            DecideNumericQuickLaunch(
                context) ==
        NumericQuickLaunchDecision::
            ExecuteNow);

    context.altDown = false;
    context.shiftDown = true;
    assert(
        classic_behavior::
            DecideNumericQuickLaunch(
                context) ==
        NumericQuickLaunchDecision::
            Text);

    context.shiftDown = false;
    context.imeComposing = true;
    assert(
        classic_behavior::
            DecideNumericQuickLaunch(
                context) ==
        NumericQuickLaunchDecision::
            Text);

    context.imeComposing = false;
    context.resultAvailable = false;
    assert(
        classic_behavior::
            DecideNumericQuickLaunch(
                context) ==
        NumericQuickLaunchDecision::
            Text);

    Command v2ray;
    v2ray.keyword = L"v2ray";
    v2ray.title = L"V2RayN";
    v2ray.target =
        L"C:\\Apps\\v2rayN.exe";

    Command sevenZip;
    sevenZip.title = L"7-Zip";
    sevenZip.target =
        L"C:\\Program Files\\7-Zip\\7zFM.exe";

    Command alias;
    alias.title = L"Password Manager";
    alias.aliases = {
        L"1password",
    };

    const std::vector<Command>
        commands{
            v2ray,
            sevenZip,
            alias,
        };

    assert(
        classic_behavior::
            HasStrongCommandContinuation(
                commands,
                L"v2"));
    assert(
        classic_behavior::
            HasStrongCommandContinuation(
                commands,
                L"7z"));
    assert(
        classic_behavior::
            HasStrongCommandContinuation(
                commands,
                L"1p"));
    assert(
        !classic_behavior::
            HasStrongCommandContinuation(
                commands,
                L"v3"));

    LauncherResult result;
    result.title = L"cs2";
    result.subtitle =
        L"Counter-Strike 2";
    result.target =
        L"D:\\Games\\cs2.exe";

    const std::vector<LauncherResult>
        results{
            result,
        };

    assert(
        classic_behavior::
            HasStrongResultContinuation(
                results,
                L"cs2"));
    assert(
        !classic_behavior::
            HasStrongResultContinuation(
                results,
                L"cs3"));

    assert(
        classic_behavior::
            kNumericIntentGraceMs < 100);
    assert(
        classic_behavior::
            kNumericTypingWindowMs >= 300);

    using classic_behavior::ContinuationEvidence;
    using classic_behavior::PendingNumericDecision;
    using classic_behavior::ResolvePendingNumericIntent;
    assert(ResolvePendingNumericIntent(ContinuationEvidence::Unknown, true, 90) == PendingNumericDecision::Wait);
    assert(ResolvePendingNumericIntent(ContinuationEvidence::Unknown, true, 240) == PendingNumericDecision::Text);
    assert(ResolvePendingNumericIntent(ContinuationEvidence::Present, true, 1) == PendingNumericDecision::Text);
    assert(ResolvePendingNumericIntent(ContinuationEvidence::Absent, true, 89) == PendingNumericDecision::Wait);
    assert(ResolvePendingNumericIntent(ContinuationEvidence::Absent, true, 90) == PendingNumericDecision::Execute);
    assert(ResolvePendingNumericIntent(ContinuationEvidence::Unknown, false, 90) == PendingNumericDecision::Execute);
    context.resultAvailable = true;
    context.editingText = true;
    assert(classic_behavior::DecideNumericQuickLaunch(context) == NumericQuickLaunchDecision::Text);
    assert(classic_behavior::StrongNameContinuation(L" V-2rayN", L"v2"));
    assert(!classic_behavior::StrongNameContinuation(L"noise-v2.txt", L"v2"));
    result.title = L"Other";
    result.subtitle = L"v2 storage";
    result.target = L"D:\\v2\\other.exe";
    assert(!classic_behavior::HasStrongResultContinuation(std::vector<LauncherResult>{result}, L"v2"));
    assert(classic_behavior::BuildFilenameContinuationQuery(L"v2") == LR"(nopath:regex:"^[\s_-]*v[\s_-]*2")");
    assert(classic_behavior::BuildFilenameContinuationQuery(L"c++2").find(L"c[\\s_-]*\\+") != std::wstring::npos);
    assert(classic_behavior::BuildFilenameContinuationQuery(L"ext:exe").empty());
    assert(classic_behavior::BuildFilenameContinuationQuery(L"C:\\Apps").empty());
    assert(classic_behavior::BuildFilenameContinuationQuery(L"   ").empty());

    std::cout
        << "Classic behavior tests passed\n";
    return 0;
}
