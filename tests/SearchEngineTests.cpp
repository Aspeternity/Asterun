#include "core/SearchEngine.hpp"

#include <algorithm>
#include <cassert>
#include <filesystem>
#include <iostream>

using namespace altrun;

namespace {

Command MakeCommand(
    std::wstring id,
    std::wstring keyword,
    std::wstring title,
    std::wstring target,
    int sortOrder) {

    Command command;
    command.id = std::move(id);
    command.keyword = std::move(keyword);
    command.title = std::move(title);
    command.target = std::move(target);
    command.source = CommandSource::User;
    command.basePriority = 100;
    command.sortOrder = sortOrder;
    return command;
}

bool ContainsCommand(
    const std::vector<SearchResult>& results,
    std::size_t commandIndex) {

    return std::any_of(
        results.begin(),
        results.end(),
        [&](const SearchResult& result) {
            return result.commandIndex ==
                   commandIndex;
        });
}

} // namespace

int main(int argc, char** argv) {
    std::filesystem::path executableDirectory =
        std::filesystem::current_path();

    if (argc > 0 && argv[0] && *argv[0]) {
        std::error_code ec;
        const auto executable =
            std::filesystem::absolute(
                argv[0],
                ec);

        if (!ec && executable.has_parent_path()) {
            executableDirectory =
                executable.parent_path();
        }
    }

    std::vector<Command> commands{
        MakeCommand(L"1", L"chrome", L"Google Chrome", L"chrome.exe", 0),
        MakeCommand(L"2", L"code", L"Visual Studio Code", L"code.exe", 1),
        MakeCommand(L"3", L"calc", L"Calculator", L"calc.exe", 2),
        MakeCommand(L"4", L"微信", L"微信", L"wechat.exe", 3),
        MakeCommand(L"5", L"网易云音乐", L"网易云音乐", L"cloudmusic.exe", 4),
        MakeCommand(L"6", L"计算器", L"计算器", L"calc-cn.exe", 5),
        MakeCommand(L"7", L"重庆银行", L"重庆银行", L"cqbank.exe", 6),
        MakeCommand(L"8", L"wx", L"WX Tool", L"wx-tool.exe", 7),
        MakeCommand(L"9", L"terminal", L"Windows Terminal", L"wt.exe", 8),
        MakeCommand(L"10", L"wechatdev", L"微信 DevTools", L"wechat-dev.exe", 9),
    };

    commands[1].aliases = {L"vscode", L"vs"};

    SearchEngine engine(
        executableDirectory / "dict");

    assert(!engine.PinyinLoaded());
    assert(engine.PinyinAvailable());
    assert(engine.PinyinCacheEntryCount() == 0);

    UsageMap usage;

    SearchEngine pinyinDisabledEngine(
        executableDirectory / "dict");

    auto disabledPinyin =
        pinyinDisabledEngine.Search(
            commands,
            usage,
            L"weixin",
            10,
            false,
            false);

    assert(disabledPinyin.empty());
    assert(!pinyinDisabledEngine.PinyinLoaded());
    assert(
        pinyinDisabledEngine
            .PinyinCacheEntryCount() == 0);

    auto weixin = engine.Search(commands, usage, L"weixin", 10);
    assert(!weixin.empty());
    assert(weixin.front().commandIndex == 3);
    assert(engine.PinyinLoaded());
    assert(engine.PinyinAvailable());
    assert(engine.PinyinCacheEntryCount() > 0);

    engine.ReleasePinyinResources();
    assert(!engine.PinyinLoaded());
    assert(engine.PinyinAvailable());
    assert(engine.PinyinCacheEntryCount() == 0);

    auto weixinReload =
        engine.Search(
            commands,
            usage,
            L"weixin",
            10);
    assert(!weixinReload.empty());
    assert(weixinReload.front().commandIndex == 3);
    assert(engine.PinyinLoaded());

    auto exact = engine.Search(commands, usage, L"chrome", 10);
    assert(!exact.empty());
    assert(exact.front().commandIndex == 0);

    auto prefix = engine.Search(commands, usage, L"cal", 10);
    assert(!prefix.empty());
    assert(prefix.front().commandIndex == 2);

    auto fuzzy = engine.Search(commands, usage, L"vsc", 10);
    assert(!fuzzy.empty());
    assert(fuzzy.front().commandIndex == 1);

    auto alias = engine.Search(commands, usage, L"vscode", 10);
    assert(!alias.empty());
    assert(alias.front().commandIndex == 1);

    auto chinese = engine.Search(commands, usage, L"微信", 10);
    assert(!chinese.empty());
    assert(chinese.front().commandIndex == 3);

    auto wx = engine.Search(commands, usage, L"wx", 10);
    assert(!wx.empty());
    // A real primary keyword remains stronger than a derived pinyin initial.
    assert(wx.front().commandIndex == 7);
    assert(ContainsCommand(wx, 3));

    auto neteaseInitials = engine.Search(commands, usage, L"wyy", 10);
    assert(!neteaseInitials.empty());
    assert(neteaseInitials.front().commandIndex == 4);

    auto neteaseFull = engine.Search(commands, usage, L"wangyiyun", 10);
    assert(!neteaseFull.empty());
    assert(neteaseFull.front().commandIndex == 4);

    auto calculatorInitials = engine.Search(commands, usage, L"jsq", 10);
    assert(!calculatorInitials.empty());
    assert(calculatorInitials.front().commandIndex == 5);

    auto polyphonic = engine.Search(commands, usage, L"chongqing", 10);
    assert(!polyphonic.empty());
    assert(polyphonic.front().commandIndex == 6);

    auto hybridPinyin = engine.Search(commands, usage, L"wangyy", 10);
    assert(!hybridPinyin.empty());
    assert(hybridPinyin.front().commandIndex == 4);

    auto spacedPinyin = engine.Search(commands, usage, L"wei x", 10);
    assert(!spacedPinyin.empty());
    assert(spacedPinyin.front().commandIndex == 3);

    auto multiWord = engine.Search(commands, usage, L"visual code", 10);
    assert(!multiWord.empty());
    assert(multiWord.front().commandIndex == 1);

    auto englishInitials = engine.Search(commands, usage, L"wt", 10);
    assert(!englishInitials.empty());
    // "WX Tool" also has the valid initials "wt", so this regression checks
    // that Windows Terminal remains discoverable instead of imposing an
    // arbitrary winner on an intentionally ambiguous initials collision.
    assert(ContainsCommand(englishInitials, 8));

    auto mixedInitials = engine.Search(commands, usage, L"wxdt", 10);
    assert(!mixedInitials.empty());
    assert(mixedInitials.front().commandIndex == 9);

    auto impossibleMultiWord = engine.Search(commands, usage, L"visual music", 10);
    assert(impossibleMultiWord.empty());

    // Short queries must not admit arbitrary subsequence or target-path noise.
    auto shortSubsequence =
        engine.Search(
            commands,
            usage,
            L"cd",
            10);
    assert(shortSubsequence.empty());

    auto tightFuzzy =
        engine.Search(
            commands,
            usage,
            L"cde",
            10);
    assert(!tightFuzzy.empty());
    assert(
        tightFuzzy.front()
            .commandIndex == 1);

    auto explicitTargetPath =
        engine.Search(
            commands,
            usage,
            L"cloudmusic.exe",
            10);
    assert(!explicitTargetPath.empty());
    assert(
        explicitTargetPath.front()
            .commandIndex == 4);

    {
        std::vector<Command>
            hygieneCommands{
                MakeCommand(
                    L"h1",
                    L"cs2",
                    L"Counter-Strike 2",
                    L"C:\\Games\\cs2.exe",
                    0),
                MakeCommand(
                    L"h2",
                    L"componentservices",
                    L"Component Services",
                    L"C:\\ProgramData\\Microsoft\\Windows\\Start Menu\\Programs\\Windows Tools\\Component Services.lnk",
                    1),
                MakeCommand(
                    L"h3",
                    L"solidworks",
                    L"SOLIDWORKS",
                    L"C:\\ProgramData\\Microsoft\\Windows\\Start Menu\\Programs\\SOLIDWORKS\\SOLIDWORKS.lnk",
                    2),
            };

        hygieneCommands[1].source =
            CommandSource::StartMenu;
        hygieneCommands[1].surfaceClass =
            LaunchSurfaceClass::
                SystemUtility;
        hygieneCommands[1].basePriority = 0;
        hygieneCommands[2].source =
            CommandSource::StartMenu;
        hygieneCommands[2].surfaceClass =
            LaunchSurfaceClass::
                PrimaryApplication;
        hygieneCommands[2].basePriority = 0;

        const auto cs =
            engine.Search(
                hygieneCommands,
                usage,
                L"cs",
                10);

        assert(!cs.empty());
        assert(
            cs.front()
                .commandIndex == 0);
        assert(ContainsCommand(cs, 1));
        assert(!ContainsCommand(cs, 2));
    }

    {
        std::vector<Command>
            defenderCommands{
                MakeCommand(
                    L"d1",
                    L"windowsdefenderfirewall",
                    L"Windows Defender Firewall",
                    L"C:\\ProgramData\\Microsoft\\Windows\\Start Menu\\Programs\\Windows Tools\\Windows Defender Firewall.lnk",
                    0),
            };

        defenderCommands[0].source =
            CommandSource::StartMenu;
        defenderCommands[0].surfaceClass =
            LaunchSurfaceClass::
                SystemUtility;
        defenderCommands[0].basePriority = 0;

        const auto df =
            engine.Search(
                defenderCommands,
                usage,
                L"df",
                10);

        assert(df.empty());
    }


    // Launch-surface admission happens before ranking. A single-character
    // query may reach real primary apps, but not system/CLI/auxiliary noise.
    {
        std::vector<Command> surfaceCommands{
            MakeCommand(
                L"s1",
                L"hyperapp",
                L"Hyper App",
                L"hyper.exe",
                0),
            MakeCommand(
                L"s2",
                L"helpcenter",
                L"Help Center",
                L"helpcenter.exe",
                1),
            MakeCommand(
                L"s3",
                L"hosttool",
                L"Host Tool",
                L"hosttool.exe",
                2),
            MakeCommand(
                L"s4",
                L"git",
                L"git",
                L"git.exe",
                3),
        };

        surfaceCommands[0].source =
            CommandSource::StartMenu;
        surfaceCommands[0].surfaceClass =
            LaunchSurfaceClass::
                PrimaryApplication;

        surfaceCommands[1].source =
            CommandSource::StartMenu;
        surfaceCommands[1].surfaceClass =
            LaunchSurfaceClass::
                SystemUtility;

        surfaceCommands[2].source =
            CommandSource::PackagedApp;
        surfaceCommands[2].surfaceClass =
            LaunchSurfaceClass::Auxiliary;

        surfaceCommands[3].source =
            CommandSource::Path;
        surfaceCommands[3].surfaceClass =
            LaunchSurfaceClass::
                CommandLineTool;

        const auto singleH =
            engine.Search(
                surfaceCommands,
                usage,
                L"h",
                10);

        assert(singleH.size() == 1);
        assert(
            singleH.front()
                .commandIndex == 0);

        const auto explicitHelp =
            engine.Search(
                surfaceCommands,
                usage,
                L"help",
                10);

        assert(ContainsCommand(
            explicitHelp,
            1));

        const auto cliShort =
            engine.Search(
                surfaceCommands,
                usage,
                L"g",
                10);

        assert(!ContainsCommand(
            cliShort,
            3));

        const auto cliExplicit =
            engine.Search(
                surfaceCommands,
                usage,
                L"git",
                10);

        assert(ContainsCommand(
            cliExplicit,
            3));
    }

    // Catalog visibility is a query-admission policy, not another
    // product-specific ranking score. Shared family-name searches keep
    // normal apps while StrongMatchOnly entries require distinctive intent.
    {
        std::vector<Command>
            catalogCommands{
                MakeCommand(
                    L"c1",
                    L"contosostudio",
                    L"Contoso Studio",
                    L"ContosoStudio.exe",
                    0),
                MakeCommand(
                    L"c2",
                    L"contosostudioencoder",
                    L"Contoso Studio Encoder",
                    L"Encoder.exe",
                    1),
                MakeCommand(
                    L"c3",
                    L"contosostudioperformancetest",
                    L"Contoso Studio Performance Test",
                    L"Benchmark.exe",
                    2),
                MakeCommand(
                    L"c4",
                    L"contosostudiosettings",
                    L"Contoso Studio Settings",
                    L"Config.exe",
                    3),
                MakeCommand(
                    L"c5",
                    L"contosostudiodownloadmanager",
                    L"Contoso Studio Download Manager",
                    L"Downloader.exe",
                    4),
                MakeCommand(
                    L"c6",
                    L"contosostudioupdater",
                    L"Contoso Studio Updater",
                    L"Updater.exe",
                    5),
                MakeCommand(
                    L"c7",
                    L"standalonediagnostics",
                    L"Standalone Diagnostics",
                    L"Diagnostics.exe",
                    6),
                MakeCommand(
                    L"c8",
                    L"myhiddenutility",
                    L"My Hidden Utility",
                    L"MyHiddenUtility.exe",
                    7),
            };

        for (std::size_t index = 0;
             index < 7;
             ++index) {
            catalogCommands[index].source =
                CommandSource::StartMenu;
            catalogCommands[index]
                .surfaceClass =
                LaunchSurfaceClass::
                    PrimaryApplication;
            catalogCommands[index]
                .basePriority = 0;
        }

        catalogCommands[0]
            .applicationRole =
            ApplicationRole::
                PrimaryApplication;
        catalogCommands[0]
            .catalogVisibility =
            CatalogVisibility::Normal;

        catalogCommands[1]
            .applicationRole =
            ApplicationRole::
                CompanionApplication;
        catalogCommands[1]
            .catalogVisibility =
            CatalogVisibility::Normal;

        catalogCommands[2]
            .applicationRole =
            ApplicationRole::BenchmarkTool;
        catalogCommands[2]
            .catalogVisibility =
            CatalogVisibility::
                StrongMatchOnly;
        catalogCommands[2]
            .distinctiveTokens = {
                L"performance",
                L"test",
            };

        catalogCommands[3]
            .applicationRole =
            ApplicationRole::
                ConfigurationTool;
        catalogCommands[3]
            .catalogVisibility =
            CatalogVisibility::
                StrongMatchOnly;
        catalogCommands[3]
            .distinctiveTokens = {
                L"settings",
            };

        catalogCommands[4]
            .applicationRole =
            ApplicationRole::Downloader;
        catalogCommands[4]
            .catalogVisibility =
            CatalogVisibility::
                StrongMatchOnly;
        catalogCommands[4]
            .distinctiveTokens = {
                L"download",
                L"manager",
            };

        catalogCommands[5]
            .applicationRole =
            ApplicationRole::Updater;
        catalogCommands[5]
            .catalogVisibility =
            CatalogVisibility::Hidden;
        catalogCommands[5]
            .distinctiveTokens = {
                L"updater",
            };

        // StrongMatchOnly remains explicitly reachable when grouping could
        // not derive distinctive tokens but the complete entry itself is
        // typed exactly.
        catalogCommands[6]
            .applicationRole =
            ApplicationRole::DiagnosticTool;
        catalogCommands[6]
            .catalogVisibility =
            CatalogVisibility::
                StrongMatchOnly;
        catalogCommands[6]
            .distinctiveTokens.clear();

        // User-authored commands are authoritative even if stale/generated
        // metadata ever carries a restrictive visibility value.
        catalogCommands[7]
            .catalogVisibility =
            CatalogVisibility::Hidden;

        const auto family =
            engine.Search(
                catalogCommands,
                usage,
                L"contoso studio",
                20);

        assert(ContainsCommand(
            family,
            0));
        assert(ContainsCommand(
            family,
            1));
        assert(!ContainsCommand(
            family,
            2));
        assert(!ContainsCommand(
            family,
            3));
        assert(!ContainsCommand(
            family,
            4));
        assert(!ContainsCommand(
            family,
            5));

        const auto performance =
            engine.Search(
                catalogCommands,
                usage,
                L"performance",
                20);
        assert(ContainsCommand(
            performance,
            2));

        const auto performancePrefix =
            engine.Search(
                catalogCommands,
                usage,
                L"perf",
                20);
        assert(ContainsCommand(
            performancePrefix,
            2));

        const auto familyPerformance =
            engine.Search(
                catalogCommands,
                usage,
                L"contoso studio performance",
                20);
        assert(ContainsCommand(
            familyPerformance,
            2));

        const auto settings =
            engine.Search(
                catalogCommands,
                usage,
                L"settings",
                20);
        assert(ContainsCommand(
            settings,
            3));

        const auto familySettings =
            engine.Search(
                catalogCommands,
                usage,
                L"contoso studio settings",
                20);
        assert(ContainsCommand(
            familySettings,
            3));

        const auto oneLetter =
            engine.Search(
                catalogCommands,
                usage,
                L"s",
                20);
        assert(!ContainsCommand(
            oneLetter,
            3));

        const auto wildcardSettings =
            engine.Search(
                catalogCommands,
                usage,
                L"*settings*",
                20,
                true);
        assert(ContainsCommand(
            wildcardSettings,
            3));

        const auto hiddenExact =
            engine.Search(
                catalogCommands,
                usage,
                L"Contoso Studio Updater",
                20);
        assert(!ContainsCommand(
            hiddenExact,
            5));

        const auto hiddenWildcard =
            engine.Search(
                catalogCommands,
                usage,
                L"*Updater*",
                20,
                true);
        assert(!ContainsCommand(
            hiddenWildcard,
            5));

        const auto exactFallback =
            engine.Search(
                catalogCommands,
                usage,
                L"Standalone Diagnostics",
                20);
        assert(ContainsCommand(
            exactFallback,
            6));

        const auto userAuthority =
            engine.Search(
                catalogCommands,
                usage,
                L"My Hidden Utility",
                20);
        assert(ContainsCommand(
            userAuthority,
            7));
    }

    auto wildcardDisabled =
        engine.Search(
            commands,
            usage,
            L"calc*",
            10,
            false);
    assert(wildcardDisabled.empty());

    auto wildcardKeyword =
        engine.Search(
            commands,
            usage,
            L"calc*",
            10,
            true);
    assert(!wildcardKeyword.empty());
    assert(
        wildcardKeyword.front()
            .commandIndex == 2);

    auto wildcardAlias =
        engine.Search(
            commands,
            usage,
            L"?scode",
            10,
            true);
    assert(!wildcardAlias.empty());
    assert(
        wildcardAlias.front()
            .commandIndex == 1);

    auto wildcardTitle =
        engine.Search(
            commands,
            usage,
            L"windows*terminal",
            10,
            true);
    assert(!wildcardTitle.empty());
    assert(
        wildcardTitle.front()
            .commandIndex == 8);

    auto wildcardTarget =
        engine.Search(
            commands,
            usage,
            L"*cloudmusic.exe",
            10,
            true);
    assert(!wildcardTarget.empty());
    assert(
        wildcardTarget.front()
            .commandIndex == 4);

    usage[L"3"] = UsageStat{42, 4102444800LL};
    auto frequent = engine.Search(commands, usage, L"", 10);
    assert(!frequent.empty());
    assert(frequent.front().commandIndex == 2);

    usage.clear();
    commands[0].pinned = true;
    auto pinned = engine.Search(commands, usage, L"", 10);
    assert(!pinned.empty());
    assert(pinned.front().commandIndex == 0);

    // A missing dictionary must never break the original search path.
    SearchEngine fallback;
    assert(!fallback.PinyinLoaded());
    assert(!fallback.PinyinAvailable());
    auto fallbackExact = fallback.Search(commands, usage, L"chrome", 10);
    assert(!fallback.PinyinLoaded());
    assert(!fallback.PinyinAvailable());
    assert(!fallbackExact.empty());
    assert(fallbackExact.front().commandIndex == 0);

    std::cout << "SearchEngine + Pinyin tests passed\n";
    return 0;
}
