#include "core/ClassicBehavior.hpp"
#include "core/RelevancePolicy.hpp"

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

    // A URI target's tail is not a file name continuation.
    {
        Command game;
        game.keyword = L"wuye";
        game.title = L"午夜轮班";
        game.target = L"steam://rungameid/3722330";
        assert(!classic_behavior::HasStrongCommandContinuation(
            std::vector<Command>{game}, L"3"));
        Command netflix;
        netflix.keyword = L"netflix";
        netflix.title = L"Netflix";
        netflix.target = L"4DF9E0F8.Netflix_mcm4njqhnhss8!Netflix.App";
        assert(!classic_behavior::HasStrongCommandContinuation(
            std::vector<Command>{netflix}, L"4"));
        // A real file name still counts: 7-Zip keeps "7" as text.
        Command zip;
        zip.title = L"File Manager";
        zip.target = L"C:\\Program Files\\7-Zip\\7zFM.exe";
        assert(classic_behavior::HasStrongCommandContinuation(
            std::vector<Command>{zip}, L"7"));
    }

    // Empty query: a bare digit is text (115, 4399, 7zip); Ctrl/Alt+digit
    // launches from the numbered list.
    context.strongContinuation = false;
    context.queryEmpty = true;
    assert(
        classic_behavior::
            DecideNumericQuickLaunch(
                context) ==
        NumericQuickLaunchDecision::
            Text);
    context.controlDown = true;
    assert(
        classic_behavior::
            DecideNumericQuickLaunch(
                context) ==
        NumericQuickLaunchDecision::
            ExecuteNow);
    context.controlDown = false;

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
    // A file row's subtitle is its location, not its name.
    result.kind = ResultKind::File;
    result.title = L"Other";
    result.subtitle = L"v2 storage";
    result.target = L"D:\\v2\\other.exe";
    assert(!classic_behavior::HasStrongResultContinuation(std::vector<LauncherResult>{result}, L"v2"));

    // App rows: the digit continues a word inside the display name.
    LauncherResult office;
    office.kind = ResultKind::Application;
    office.title = L"microsoftoffice365";
    office.subtitle = L"Microsoft Office 365";
    office.target = L"C:\\Office\\office.lnk";
    assert(classic_behavior::HasStrongResultContinuation(
        std::vector<LauncherResult>{office}, L"office3"));
    assert(!classic_behavior::HasStrongResultContinuation(
        std::vector<LauncherResult>{office}, L"office4"));
    LauncherResult photoshop;
    photoshop.title = L"adobephotoshop2024";
    photoshop.subtitle = L"Adobe Photoshop 2024";
    assert(classic_behavior::HasStrongResultContinuation(
        std::vector<LauncherResult>{photoshop}, L"photoshop2"));

    // A digit after a space, or while typing an argument for a web search /
    // runtime-input action, is text; Ctrl/Alt+digit still launches.
    {
        classic_behavior::NumericQuickLaunchContext typing{};
        typing.enabled = true;
        typing.queryEmpty = false;
        typing.resultAvailable = true;
        assert(classic_behavior::DecideNumericQuickLaunch(typing) ==
               NumericQuickLaunchDecision::DeferExecute);
        typing.afterSeparator = true;
        assert(classic_behavior::DecideNumericQuickLaunch(typing) ==
               NumericQuickLaunchDecision::Text);
        typing.afterSeparator = false;
        typing.typingArgument = true;
        assert(classic_behavior::DecideNumericQuickLaunch(typing) ==
               NumericQuickLaunchDecision::Text);
        typing.controlDown = true;
        assert(classic_behavior::DecideNumericQuickLaunch(typing) ==
               NumericQuickLaunchDecision::ExecuteNow);
    }

    LauncherResult web;
    web.providerId = "builtin.web";
    web.id = L"builtin.web:search:g";
    assert(classic_behavior::ConsumesTypedText(web));
    LauncherResult runtime;
    runtime.id = L"runtime.input:user-1";
    runtime.providerId = "user.commands";
    assert(classic_behavior::ConsumesTypedText(runtime));
    assert(!classic_behavior::ConsumesTypedText(office));

    // Single-result auto execution: only clear matches, never actions that
    // are built from the text still being typed.
    {
        using classic_behavior::SingleResultAutoExecutable;
        LauncherResult app = office;
        for (auto kind : {relevance::MatchKind::Exact,
                          relevance::MatchKind::Prefix,
                          relevance::MatchKind::Initials,
                          relevance::MatchKind::BoundaryPrefix,
                          relevance::MatchKind::HybridPinyin}) {
            app.relevanceMatch.kind = kind;
            app.relevanceMatch.score = 800;
            assert(SingleResultAutoExecutable(app));
        }
        for (auto kind : {relevance::MatchKind::Substring,
                          relevance::MatchKind::TightFuzzy,
                          relevance::MatchKind::Fuzzy,
                          relevance::MatchKind::Wildcard,
                          relevance::MatchKind::None}) {
            app.relevanceMatch.kind = kind;
            assert(!SingleResultAutoExecutable(app));
        }

        LauncherResult openUrl = web;
        openUrl.relevanceMatch = {relevance::MatchKind::Exact,
                                  relevance::MatchField::Target, 1200, false};
        assert(!SingleResultAutoExecutable(openUrl));
        LauncherResult argument = runtime;
        argument.relevanceMatch = {relevance::MatchKind::Exact,
                                   relevance::MatchField::Keyword, 1320, false};
        assert(!SingleResultAutoExecutable(argument));
    }
    assert(classic_behavior::BuildFilenameContinuationQuery(L"v2") == LR"(nopath:regex:"^[\s_-]*v[\s_-]*2")");
    assert(classic_behavior::BuildFilenameContinuationQuery(L"c++2").find(L"c[\\s_-]*\\+") != std::wstring::npos);
    assert(classic_behavior::BuildFilenameContinuationQuery(L"ext:exe").empty());
    assert(classic_behavior::BuildFilenameContinuationQuery(L"C:\\Apps").empty());
    assert(classic_behavior::BuildFilenameContinuationQuery(L"   ").empty());

    std::cout
        << "Classic behavior tests passed\n";
    return 0;
}
