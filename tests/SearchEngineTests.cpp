#include "core/LaunchRole.hpp"
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

    // Long-running provider refreshes must not make the derived pinyin cache
    // grow forever. Use a deliberately tiny capacity so eviction is cheap to
    // exercise in the normal core test suite.
    {
        constexpr std::size_t
            kTestCapacity = 8;

        PinyinSearch boundedPinyin(
            executableDirectory / "dict",
            kTestCapacity);

        for (std::size_t index = 0;
             index < kTestCapacity + 6;
             ++index) {
            const std::wstring text =
                L"测试应用" +
                std::to_wstring(
                    index);

            assert(
                boundedPinyin.FormsFor(
                    text) != nullptr);
            assert(
                boundedPinyin
                    .CacheEntryCount() <=
                kTestCapacity);
        }

        assert(
            boundedPinyin
                .CacheEntryCount() ==
            kTestCapacity);

        boundedPinyin.Unload();
        assert(
            boundedPinyin
                .CacheEntryCount() == 0);
    }

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

    // Short ASCII precision: 1-2 characters may use exact, whole-field
    // prefix and initials, but must not recall arbitrary later word
    // boundaries. Three-character queries restore BoundaryPrefix behavior.
    {
        std::vector<Command>
            shortPrecision{
                MakeCommand(
                    L"sp1",
                    L"adorneditor",
                    L"Adorn Editor",
                    L"adorn.exe",
                    0),
                MakeCommand(
                    L"sp2",
                    L"administrativetools",
                    L"Administrative Tools",
                    L"admin-tools.lnk",
                    1),
                MakeCommand(
                    L"sp3",
                    L"xboxappadminserver",
                    L"Xbox App Admin Server",
                    L"xbox-admin.exe",
                    2),
                MakeCommand(
                    L"sp4",
                    L"securityadvancedfirewall",
                    L"Security Advanced Firewall",
                    L"security.exe",
                    3),
                MakeCommand(
                    L"sp5",
                    L"installadditionaltool",
                    L"Install Additional Tool",
                    L"install-tool.exe",
                    4),
                MakeCommand(
                    L"sp6",
                    L"odbcdatasources",
                    L"ODBC Data Sources",
                    L"odbc.exe",
                    5),
                MakeCommand(
                    L"sp7",
                    L"solitairecasualgames",
                    L"Solitaire Casual Games",
                    L"solitaire.exe",
                    6),
                MakeCommand(
                    L"sp8",
                    L"utility",
                    L"Utility",
                    L"utility.exe",
                    7),
            };

        for (auto& command :
             shortPrecision) {
            command.source =
                CommandSource::StartMenu;
            command.surfaceClass =
                LaunchSurfaceClass::
                    PrimaryApplication;
            command.basePriority = 0;
        }

        shortPrecision[1].surfaceClass =
            LaunchSurfaceClass::
                SystemUtility;
        shortPrecision[7].source =
            CommandSource::User;
        shortPrecision[7].aliases = {
            L"ad",
        };

        const auto ad =
            engine.Search(
                shortPrecision,
                usage,
                L"ad",
                20);

        assert(
            ContainsCommand(
                ad,
                0));
        assert(
            ContainsCommand(
                ad,
                1));
        assert(
            !ContainsCommand(
                ad,
                2));
        assert(
            !ContainsCommand(
                ad,
                3));
        assert(
            !ContainsCommand(
                ad,
                4));
        assert(
            ContainsCommand(
                ad,
                7));

        // Explicit user alias remains authoritative even under short-query
        // tightening.
        assert(
            ad.front()
                .commandIndex == 7);

        const auto so =
            engine.Search(
                shortPrecision,
                usage,
                L"so",
                20);

        assert(
            !ContainsCommand(
                so,
                5));
        assert(
            ContainsCommand(
                so,
                6));

        // Boundary-prefix recall resumes at three ASCII characters.
        const auto adminBoundary =
            engine.Search(
                shortPrecision,
                usage,
                L"adm",
                20);
        assert(
            ContainsCommand(
                adminBoundary,
                2));

        const auto sourcesBoundary =
            engine.Search(
                shortPrecision,
                usage,
                L"sou",
                20);
        assert(
            ContainsCommand(
                sourcesBoundary,
                5));
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
                MakeCommand(
                    L"c9",
                    L"contosostudioq7",
                    L"Contoso Studio Q7",
                    L"Q7.exe",
                    8),
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

        const std::wstring catalogGroup =
            L"family:contosostudio|menu:c:\\programdata\\microsoft\\windows\\start menu\\programs\\contoso studio";

        for (std::size_t index = 0;
             index < 7;
             ++index) {
            catalogCommands[index]
                .catalogGroupKey =
                    catalogGroup;
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
        // Deliberately retain a shared-family token to model a provider/cache
        // boundary leak. StrongMatchOnly admission must still require the
        // entry-owned intent rather than treating "co"/"contoso" as explicit.
        catalogCommands[2]
            .distinctiveTokens = {
                L"contoso",
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

        catalogCommands[8].source =
            CommandSource::StartMenu;
        catalogCommands[8]
            .surfaceClass =
            LaunchSurfaceClass::
                PrimaryApplication;
        catalogCommands[8]
            .applicationRole =
            ApplicationRole::
                SuiteUtility;
        catalogCommands[8]
            .catalogVisibility =
            CatalogVisibility::
                StrongMatchOnly;
        catalogCommands[8]
            .catalogGroupKey =
                catalogGroup;
        catalogCommands[8]
            .distinctiveTokens = {
                L"q7",
            };

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
        assert(!ContainsCommand(
            family,
            8));

        const auto familyPrefix =
            engine.Search(
                catalogCommands,
                usage,
                L"co",
                20);
        assert(ContainsCommand(
            familyPrefix,
            0));
        assert(ContainsCommand(
            familyPrefix,
            1));
        assert(!ContainsCommand(
            familyPrefix,
            2));

        const auto familyWord =
            engine.Search(
                catalogCommands,
                usage,
                L"contoso",
                20);
        assert(ContainsCommand(
            familyWord,
            0));
        assert(ContainsCommand(
            familyWord,
            1));
        assert(!ContainsCommand(
            familyWord,
            2));

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

        const auto opaqueExact =
            engine.Search(
                catalogCommands,
                usage,
                L"q7",
                20);
        assert(ContainsCommand(
            opaqueExact,
            8));

        const auto familyOpaque =
            engine.Search(
                catalogCommands,
                usage,
                L"contoso studio q7",
                20);
        assert(ContainsCommand(
            familyOpaque,
            8));

        const auto opaqueOneLetter =
            engine.Search(
                catalogCommands,
                usage,
                L"q",
                20);
        assert(!ContainsCommand(
            opaqueOneLetter,
            8));

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

    // A StrongMatchOnly entry whose Windows display title joins family,
    // version and CJK role text into one token must not let a family prefix
    // masquerade as distinctive intent.
    {
        LaunchEvidence evidence;
        evidence.source =
            LaunchCandidateSource::
                StartMenu;
        evidence.displayTitle =
            L"Contoso性能测试2025";
        evidence.resolvedTarget =
            L"C:\\Program Files\\Contoso\\Bench.exe";
        evidence.installRootHint =
            L"C:\\Program Files\\Contoso";
        evidence.targetKind =
            LaunchTargetKind::
                GuiExecutable;
        evidence.executable.productName =
            L"Contoso 2025";

        const auto decision =
            ClassifyApplicationRole(
                evidence);

        Command compactCjk =
            MakeCommand(
                L"compact-cjk",
                L"contoso性能测试2025",
                L"Contoso性能测试2025",
                L"C:\\Program Files\\Contoso\\Bench.exe",
                0);

        compactCjk.source =
            CommandSource::StartMenu;
        compactCjk.surfaceClass =
            LaunchSurfaceClass::
                PrimaryApplication;
        compactCjk.basePriority = 0;
        compactCjk.applicationRole =
            decision.role;
        compactCjk.roleConfidence =
            decision.confidence;
        compactCjk.catalogVisibility =
            decision.visibility;
        compactCjk.distinctiveTokens =
            decision.distinctiveTokens;

        std::vector<Command>
            compactCommands{
                compactCjk,
            };

        const auto familyPrefix =
            engine.Search(
                compactCommands,
                usage,
                L"co",
                10);

        assert(
            familyPrefix.empty());

        const auto rolePrefix =
            engine.Search(
                compactCommands,
                usage,
                L"性能",
                10);

        assert(
            ContainsCommand(
                rolePrefix,
                0));

        const auto exactCompact =
            engine.Search(
                compactCommands,
                usage,
                L"Contoso性能测试2025",
                10);

        assert(
            ContainsCommand(
                exactCompact,
                0));
    }

    // End-to-end suite regression: helper EXEs may report unrelated ProductName
    // values while Windows exposes them under one Start Menu suite folder.
    // Family-prefix search must keep real companion apps but suppress explicit
    // auxiliary/alternate entries until their own intent is typed.
    {
        const std::filesystem::path menuFolder =
            L"C:/ProgramData/Microsoft/Windows/Start Menu/Programs/Contoso Studio 2026";

        const auto makeEvidence =
            [&](std::wstring title,
                std::wstring target,
                std::wstring productName) {
                LaunchEvidence evidence;
                evidence.source =
                    LaunchCandidateSource::
                        StartMenu;
                evidence.displayTitle =
                    std::move(title);
                evidence.resolvedTarget =
                    std::move(target);
                evidence.startMenuFolder =
                    menuFolder;
                evidence.installRootHint =
                    std::filesystem::path(
                        evidence.resolvedTarget)
                        .parent_path();
                evidence.targetKind =
                    LaunchTargetKind::
                        GuiExecutable;
                evidence.executable
                    .productName =
                    std::move(productName);
                return evidence;
            };

        auto primaryEvidence =
            makeEvidence(
                L"Contoso Studio 2026",
                L"C:/Program Files/Contoso/Studio/Studio.exe",
                L"Contoso Main Application 2026");
        auto benchmarkEvidence =
            makeEvidence(
                L"Contoso Studio 性能测试 2026",
                L"C:/Program Files/Contoso/Tools/Bench.exe",
                L"Contoso Benchmark Utility");
        auto settingsEvidence =
            makeEvidence(
                L"Contoso Studio 设置向导 2026",
                L"C:/Program Files/Contoso/Setup/Config.exe",
                L"Contoso Configuration Manager");
        auto quickEvidence =
            makeEvidence(
                L"Contoso Studio 2026 快速启动",
                L"C:/Program Files/Contoso/Studio/Studio.exe",
                L"Contoso Launcher");
        auto composerEvidence =
            makeEvidence(
                L"Contoso Studio Composer 2026",
                L"C:/Program Files/Contoso/Composer/Composer.exe",
                L"Contoso Composer");

        const auto primaryRole =
            ClassifyApplicationRole(
                primaryEvidence);
        const auto benchmarkRole =
            ClassifyApplicationRole(
                benchmarkEvidence);
        const auto settingsRole =
            ClassifyApplicationRole(
                settingsEvidence);
        const auto quickRole =
            ClassifyApplicationRole(
                quickEvidence);
        const auto composerRole =
            ClassifyApplicationRole(
                composerEvidence);

        auto makeCatalogCommand =
            [&](std::wstring id,
                const LaunchEvidence& evidence,
                const ApplicationRoleDecision&
                    role,
                int order) {
                Command command =
                    MakeCommand(
                        std::move(id),
                        evidence.displayTitle,
                        evidence.displayTitle,
                        evidence.resolvedTarget,
                        order);

                command.source =
                    CommandSource::
                        StartMenu;
                command.surfaceClass =
                    LaunchSurfaceClass::
                        PrimaryApplication;
                command.basePriority = 0;
                command.applicationRole =
                    role.role;
                command.roleConfidence =
                    role.confidence;
                command.catalogVisibility =
                    role.visibility;
                command.catalogGroupKey =
                    role.catalogGroupKey;
                command.distinctiveTokens =
                    role.distinctiveTokens;
                command.canonicalIdentity =
                    L"file:" +
                    relevance::Normalize(
                        evidence.resolvedTarget);
                return command;
            };

        std::vector<Command> suite{
            makeCatalogCommand(
                L"suite-primary",
                primaryEvidence,
                primaryRole,
                0),
            makeCatalogCommand(
                L"suite-benchmark",
                benchmarkEvidence,
                benchmarkRole,
                1),
            makeCatalogCommand(
                L"suite-settings",
                settingsEvidence,
                settingsRole,
                2),
            makeCatalogCommand(
                L"suite-quick",
                quickEvidence,
                quickRole,
                3),
            makeCatalogCommand(
                L"suite-composer",
                composerEvidence,
                composerRole,
                4),
        };

        suite[3].arguments =
            L"--quick";
        suite[3].canonicalIdentity =
            suite[0].canonicalIdentity +
            L"|args:--quick";

        std::vector<Command*> views;

        for (auto& command : suite) {
            views.push_back(&command);
        }

        CalibrateCatalogRoleContext(
            views);

        const auto familyPrefix =
            engine.Search(
                suite,
                usage,
                L"co",
                20);

        assert(
            ContainsCommand(
                familyPrefix,
                0));
        assert(
            !ContainsCommand(
                familyPrefix,
                1));
        assert(
            !ContainsCommand(
                familyPrefix,
                2));
        assert(
            !ContainsCommand(
                familyPrefix,
                3));
        assert(
            ContainsCommand(
                familyPrefix,
                4));

        const auto benchmarkIntent =
            engine.Search(
                suite,
                usage,
                L"性能",
                20);
        assert(
            ContainsCommand(
                benchmarkIntent,
                1));

        const auto settingsIntent =
            engine.Search(
                suite,
                usage,
                L"设置",
                20);
        assert(
            ContainsCommand(
                settingsIntent,
                2));

        const auto quickIntent =
            engine.Search(
                suite,
                usage,
                L"快速",
                20);
        assert(
            ContainsCommand(
                quickIntent,
                3));

        const auto quickExact =
            engine.Search(
                suite,
                usage,
                L"Contoso Studio 2026 快速启动",
                20);
        assert(
            ContainsCommand(
                quickExact,
                3));
    }

    {
        // End-to-end role completion regression: a family query keeps the
        // primary and independent companion, while weak suite utilities and
        // cross-group alternate variants require their own explicit intent.
        const std::wstring group =
            L"family:contosostudio|root:c:\\program files\\contoso\\studio";

        Command primary =
            MakeCommand(
                L"role-primary",
                L"contosostudio2026",
                L"Contoso Studio 2026",
                L"C:\\Program Files\\Contoso\\Studio\\Studio.exe",
                0);
        primary.source =
            CommandSource::StartMenu;
        primary.applicationRole =
            ApplicationRole::
                PrimaryApplication;
        primary.roleConfidence =
            RoleConfidence::High;
        primary.catalogVisibility =
            CatalogVisibility::Normal;
        primary.catalogGroupKey =
            group;
        primary.canonicalIdentity =
            L"file:c:\\program files\\contoso\\studio\\studio.exe";

        Command composer =
            MakeCommand(
                L"role-composer",
                L"contosostudiocomposer2026",
                L"Contoso Studio Composer 2026",
                L"C:\\Program Files\\Contoso\\Studio\\Composer.exe",
                1);
        composer.source =
            CommandSource::StartMenu;
        composer.applicationRole =
            ApplicationRole::
                CompanionApplication;
        composer.roleConfidence =
            RoleConfidence::Medium;
        composer.catalogVisibility =
            CatalogVisibility::Normal;
        composer.catalogGroupKey =
            group;
        composer.distinctiveTokens = {
            L"composer",
        };
        composer.canonicalIdentity =
            L"file:c:\\program files\\contoso\\studio\\composer.exe";

        Command composerSync =
            MakeCommand(
                L"role-sync",
                L"contosostudiocomposersync2026",
                L"Contoso Studio Composer Sync 2026",
                L"C:\\Program Files\\Contoso\\Studio\\ComposerSync.exe",
                2);
        composerSync.source =
            CommandSource::StartMenu;
        composerSync.applicationRole =
            ApplicationRole::
                CompanionApplication;
        composerSync.roleConfidence =
            RoleConfidence::Medium;
        composerSync.catalogVisibility =
            CatalogVisibility::Normal;
        composerSync.catalogGroupKey =
            group;
        composerSync.distinctiveTokens = {
            L"composer",
            L"sync",
        };
        composerSync.canonicalIdentity =
            L"file:c:\\program files\\contoso\\studio\\composersync.exe";

        Command taskScheduler =
            MakeCommand(
                L"role-scheduler",
                L"contosostudiotaskscheduler2026",
                L"Contoso Studio Task Scheduler 2026",
                L"C:\\Program Files\\Contoso\\Studio\\Scheduler.exe",
                3);
        taskScheduler.source =
            CommandSource::StartMenu;
        taskScheduler.applicationRole =
            ApplicationRole::
                PrimaryApplication;
        taskScheduler.roleConfidence =
            RoleConfidence::High;
        taskScheduler.catalogVisibility =
            CatalogVisibility::Normal;
        taskScheduler.catalogGroupKey =
            group;
        taskScheduler.distinctiveTokens = {
            L"contoso",
            L"scheduler",
        };
        taskScheduler.canonicalIdentity =
            L"file:c:\\program files\\contoso\\studio\\scheduler.exe";

        Command quick =
            MakeCommand(
                L"role-quick",
                L"contosostudio2026quicklaunch",
                L"Contoso Studio 2026 Quick Launch",
                primary.target,
                4);
        quick.source =
            CommandSource::StartMenu;
        quick.applicationRole =
            ApplicationRole::
                AlternateLaunch;
        quick.roleConfidence =
            RoleConfidence::Low;
        quick.catalogVisibility =
            CatalogVisibility::Normal;
        quick.catalogGroupKey =
            L"family:contosotools|root:c:\\program files\\contoso\\tools";
        quick.arguments =
            L"--quick";
        quick.canonicalIdentity =
            primary.canonicalIdentity +
            L"|args:--quick";
        quick.distinctiveTokens = {
            L"contoso",
            L"quick",
        };

        std::vector<Command> roleCommands{
            primary,
            composer,
            composerSync,
            taskScheduler,
            quick,
        };

        std::vector<Command*> roleViews;
        for (auto& command :
             roleCommands) {
            roleViews.push_back(
                &command);
        }

        CalibrateCatalogRoleContext(
            roleViews);

        assert(
            roleCommands[2]
                .applicationRole ==
            ApplicationRole::
                SuiteUtility);
        assert(
            roleCommands[3]
                .applicationRole ==
            ApplicationRole::
                SuiteUtility);
        assert(
            roleCommands[4]
                .applicationRole ==
            ApplicationRole::
                AlternateLaunch);

        const auto family =
            engine.Search(
                roleCommands,
                usage,
                L"co",
                20);

        assert(
            ContainsCommand(
                family,
                0));
        assert(
            ContainsCommand(
                family,
                1));
        assert(
            !ContainsCommand(
                family,
                2));
        assert(
            !ContainsCommand(
                family,
                3));
        assert(
            !ContainsCommand(
                family,
                4));

        const auto composerOnly =
            engine.Search(
                roleCommands,
                usage,
                L"composer",
                20);

        assert(
            ContainsCommand(
                composerOnly,
                1));
        assert(
            !ContainsCommand(
                composerOnly,
                2));

        const auto syncIntent =
            engine.Search(
                roleCommands,
                usage,
                L"sync",
                20);
        assert(
            ContainsCommand(
                syncIntent,
                2));

        const auto schedulerIntent =
            engine.Search(
                roleCommands,
                usage,
                L"task",
                20);
        assert(
            ContainsCommand(
                schedulerIntent,
                3));

        const auto quickIntent =
            engine.Search(
                roleCommands,
                usage,
                L"quick",
                20);
        assert(
            ContainsCommand(
                quickIntent,
                4));
    }

    {
        // Suite topology is an admission concern, not a query heuristic.
        // A family query keeps independent companions, while a structurally
        // corroborated child requires its own delta intent.
        const std::wstring group =
            L"family:acmestudio|root:c:\\program files\\acme\\studio";

        auto makeSuiteCommand =
            [&](std::wstring id,
                std::wstring title,
                std::wstring targetStem,
                std::vector<std::wstring>
                    tokens,
                int order) {
                Command command =
                    MakeCommand(
                        std::move(id),
                        relevance::Normalize(
                            title),
                        title,
                        L"C:\\Program Files\\Acme\\Studio\\" +
                            targetStem +
                            L".exe",
                        order);

                command.source =
                    CommandSource::StartMenu;
                command.applicationRole =
                    ApplicationRole::
                        CompanionApplication;
                command.roleConfidence =
                    RoleConfidence::Medium;
                command.catalogVisibility =
                    CatalogVisibility::Normal;
                command.catalogGroupKey =
                    group;
                command.distinctiveTokens =
                    std::move(tokens);
                command.canonicalIdentity =
                    L"file:c:\\program files\\acme\\studio\\" +
                    targetStem +
                    L".exe";
                return command;
            };

        Command primary =
            MakeCommand(
                L"topology-primary",
                L"acmestudio2026",
                L"Acme Studio 2026",
                L"C:\\Program Files\\Acme\\Studio\\Studio.exe",
                0);
        primary.source =
            CommandSource::StartMenu;
        primary.applicationRole =
            ApplicationRole::
                PrimaryApplication;
        primary.roleConfidence =
            RoleConfidence::High;
        primary.catalogVisibility =
            CatalogVisibility::Normal;
        primary.catalogGroupKey =
            group;
        primary.canonicalIdentity =
            L"file:c:\\program files\\acme\\studio\\studio.exe";

        std::vector<Command> topology{
            primary,
            makeSuiteCommand(
                L"topology-composer",
                L"Acme Studio Composer 2026",
                L"Composer",
                {L"composer"},
                1),
            makeSuiteCommand(
                L"topology-composer-player",
                L"Acme Studio Composer Player 2026",
                L"ComposerPlayer",
                {L"composer", L"player"},
                2),
            makeSuiteCommand(
                L"topology-visualize",
                L"Acme Studio Visualize 2026",
                L"Visualize",
                {L"visualize"},
                3),
            makeSuiteCommand(
                L"topology-visualize-boost",
                L"Acme Studio Visualize Boost 2026",
                L"VisualizeBoost",
                {L"visualize", L"boost"},
                4),
            makeSuiteCommand(
                L"topology-routing",
                L"Acme Studio Routing 2026",
                L"Routing",
                {L"routing"},
                5),
            makeSuiteCommand(
                L"topology-title-only",
                L"Acme Studio Composer Pro 2026",
                L"IndependentPro",
                {L"composer", L"pro"},
                6),
        };

        topology[3].target =
            L"C:\\Program Files\\Acme\\Visualize\\VisualizeApp.exe";
        topology[3].canonicalIdentity =
            L"file:c:\\program files\\acme\\visualize\\visualizeapp.exe";
        topology[3].distinctiveTokens = {
            L"metadataalias",
        };

        topology[4].target =
            L"C:\\Program Files\\Acme\\Visualize Boost\\BoostWorker.exe";
        topology[4].canonicalIdentity =
            L"file:c:\\program files\\acme\\visualize boost\\boostworker.exe";
        topology[4].distinctiveTokens = {
            L"metadataalias",
        };

        std::vector<Command*> views;
        for (auto& command : topology) {
            views.push_back(&command);
        }

        CalibrateCatalogRoleContext(
            views);

        assert(
            topology[2].applicationRole ==
            ApplicationRole::
                SuiteSubordinate);
        assert(
            topology[4].applicationRole ==
            ApplicationRole::
                SuiteSubordinate);
        assert(
            topology[4]
                .distinctiveTokens
                .size() == 1);
        assert(
            topology[4]
                .distinctiveTokens[0] ==
            L"boost");
        assert(
            topology[5].applicationRole ==
            ApplicationRole::
                CompanionApplication);
        assert(
            topology[6].applicationRole ==
            ApplicationRole::
                CompanionApplication);

        const auto family =
            engine.Search(
                topology,
                usage,
                L"ac",
                20);

        assert(ContainsCommand(family, 0));
        assert(ContainsCommand(family, 1));
        assert(!ContainsCommand(family, 2));
        assert(ContainsCommand(family, 3));
        assert(!ContainsCommand(family, 4));
        assert(ContainsCommand(family, 5));
        assert(ContainsCommand(family, 6));

        const auto composer =
            engine.Search(
                topology,
                usage,
                L"composer",
                20);

        assert(ContainsCommand(composer, 1));
        assert(!ContainsCommand(composer, 2));

        const auto player =
            engine.Search(
                topology,
                usage,
                L"player",
                20);

        assert(ContainsCommand(player, 2));

        const auto boost =
            engine.Search(
                topology,
                usage,
                L"boost",
                20);

        assert(ContainsCommand(boost, 4));

        const auto exactChild =
            engine.Search(
                topology,
                usage,
                L"Acme Studio Composer Player 2026",
                20);

        assert(ContainsCommand(exactChild, 2));
    }

    {
        // Utility-container evidence must affect admission, not lexical
        // matching. Family queries hide corroborated sidecars/management
        // surfaces while explicit child identity still re-admits them.
        const std::wstring mainGroup =
            L"family:acmestudio|menu:c:\\programdata\\microsoft\\windows\\start menu\\programs\\acme studio 2026";
        const std::wstring toolsGroup =
            L"family:acmestudio|menu:c:\\programdata\\microsoft\\windows\\start menu\\programs\\acme studio tools 2026";

        auto makeUtilityCommand =
            [&](std::wstring id,
                std::wstring title,
                std::wstring target,
                std::vector<std::wstring>
                    tokens,
                int order) {
                Command command =
                    MakeCommand(
                        std::move(id),
                        relevance::Normalize(
                            title),
                        title,
                        target,
                        order);

                command.source =
                    CommandSource::StartMenu;
                command.applicationRole =
                    ApplicationRole::
                        PrimaryApplication;
                command.roleConfidence =
                    RoleConfidence::Medium;
                command.catalogVisibility =
                    CatalogVisibility::Normal;
                command.catalogGroupKey =
                    toolsGroup;
                command.distinctiveTokens =
                    std::move(tokens);
                command.canonicalIdentity =
                    L"file:" +
                    relevance::Normalize(
                        target);
                return command;
            };

        Command primary =
            MakeCommand(
                L"utility-primary",
                L"acmestudio2026",
                L"Acme Studio 2026",
                L"C:\\Program Files\\Acme\\Studio\\Studio.exe",
                0);
        primary.source =
            CommandSource::StartMenu;
        primary.applicationRole =
            ApplicationRole::
                PrimaryApplication;
        primary.roleConfidence =
            RoleConfidence::High;
        primary.catalogVisibility =
            CatalogVisibility::Normal;
        primary.catalogGroupKey =
            mainGroup;
        primary.canonicalIdentity =
            L"file:c:\\program files\\acme\\studio\\studio.exe";

        std::vector<Command> utility{
            primary,
            makeUtilityCommand(
                L"utility-inspector",
                L"Acme Studio Inspector 2026",
                L"C:\\Program Files\\Acme\\Studio\\Inspector.exe",
                {L"inspector"},
                1),
            makeUtilityCommand(
                L"utility-library-manager",
                L"Acme Studio Routing Library Manager 2026",
                L"C:\\Program Files\\Acme\\Studio\\Managers\\Library.exe",
                {L"routing", L"library", L"manager"},
                2),
            makeUtilityCommand(
                L"utility-opaque",
                L"Acme Studio Q7 2026",
                L"C:\\Program Files\\Acme\\Studio\\Support\\Q7.exe",
                {L"q7"},
                3),
            makeUtilityCommand(
                L"utility-distant-opaque",
                L"Acme Studio Z5 2026",
                L"C:\\Program Files\\Acme\\Independent\\Z5.exe",
                {L"z5"},
                4),
            makeUtilityCommand(
                L"utility-treehouse",
                L"Acme Studio Treehouse 2026",
                L"C:\\Program Files\\Acme\\Studio\\Treehouse\\Treehouse.exe",
                {L"treehouse"},
                5),
        };

        // MakeCanonical-style normalization in the test helper does not model
        // Windows path separators, so use explicit canonical identities.
        utility[1].canonicalIdentity =
            L"file:c:\\program files\\acme\\studio\\inspector.exe";
        utility[2].canonicalIdentity =
            L"file:c:\\program files\\acme\\studio\\managers\\library.exe";
        utility[3].canonicalIdentity =
            L"file:c:\\program files\\acme\\studio\\support\\q7.exe";
        utility[4].canonicalIdentity =
            L"file:c:\\program files\\acme\\independent\\z5.exe";
        utility[5].canonicalIdentity =
            L"file:c:\\program files\\acme\\studio\\treehouse\\treehouse.exe";

        std::vector<Command*> views;
        for (auto& command : utility) {
            views.push_back(&command);
        }

        CalibrateCatalogRoleContext(
            views);

        assert(
            utility[1].applicationRole ==
            ApplicationRole::
                SuiteUtility);
        assert(
            utility[2].applicationRole ==
            ApplicationRole::
                SuiteUtility);
        assert(
            utility[3].applicationRole ==
            ApplicationRole::
                SuiteUtility);
        assert(
            utility[4].applicationRole ==
            ApplicationRole::
                PrimaryApplication);
        assert(
            utility[5].applicationRole ==
            ApplicationRole::
                PrimaryApplication);

        const auto family =
            engine.Search(
                utility,
                usage,
                L"ac",
                20);

        assert(ContainsCommand(family, 0));
        assert(!ContainsCommand(family, 1));
        assert(!ContainsCommand(family, 2));
        assert(!ContainsCommand(family, 3));
        assert(ContainsCommand(family, 4));
        assert(ContainsCommand(family, 5));

        const auto inspector =
            engine.Search(
                utility,
                usage,
                L"inspector",
                20);

        assert(ContainsCommand(inspector, 1));

        const auto routing =
            engine.Search(
                utility,
                usage,
                L"routing",
                20);

        assert(ContainsCommand(routing, 2));

        const auto opaque =
            engine.Search(
                utility,
                usage,
                L"q7",
                20);

        assert(ContainsCommand(opaque, 3));

        const auto distantOpaque =
            engine.Search(
                utility,
                usage,
                L"z5",
                20);

        assert(ContainsCommand(distantOpaque, 4));

        const auto treehouse =
            engine.Search(
                utility,
                usage,
                L"treehouse",
                20);

        assert(ContainsCommand(treehouse, 5));
    }

    {
        const std::wstring group =
            L"family:fabrikamstudio|root:c:\\program files\\fabrikam\\studio";

        auto makeRoleCommand =
            [&](std::wstring id,
                std::wstring title,
                ApplicationRole role,
                RoleConfidence confidence,
                CatalogVisibility visibility,
                int order) {
                Command command =
                    MakeCommand(
                        std::move(id),
                        relevance::Normalize(
                            title),
                        title,
                        L"C:\\Program Files\\Fabrikam\\Studio\\Tool.exe",
                        order);

                command.source =
                    CommandSource::StartMenu;
                command.applicationRole =
                    role;
                command.roleConfidence =
                    confidence;
                command.catalogVisibility =
                    visibility;
                command.catalogGroupKey =
                    group;
                command.canonicalIdentity =
                    L"file:c:\\program files\\fabrikam\\studio\\" +
                    std::to_wstring(order) +
                    L".exe";
                return command;
            };

        std::vector<Command> residual{
            makeRoleCommand(
                L"residual-primary",
                L"Fabrikam Studio 2026",
                ApplicationRole::
                    PrimaryApplication,
                RoleConfidence::High,
                CatalogVisibility::Normal,
                0),
            makeRoleCommand(
                L"residual-network-monitor",
                L"Fabrikam Studio Network Monitor 2026",
                ApplicationRole::
                    BackgroundComponent,
                RoleConfidence::High,
                CatalogVisibility::Hidden,
                1),
            makeRoleCommand(
                L"residual-license-manager",
                L"Fabrikam Studio License Manager 2026",
                ApplicationRole::
                    CompanionApplication,
                RoleConfidence::Medium,
                CatalogVisibility::Normal,
                2),
            makeRoleCommand(
                L"residual-service-manager",
                L"Fabrikam Studio Service Manager 2026",
                ApplicationRole::
                    ServiceComponent,
                RoleConfidence::High,
                CatalogVisibility::Hidden,
                3),
            makeRoleCommand(
                L"residual-network-designer",
                L"Fabrikam Studio Network Designer 2026",
                ApplicationRole::
                    CompanionApplication,
                RoleConfidence::Medium,
                CatalogVisibility::Normal,
                4),
        };

        residual[1].distinctiveTokens = {
            L"network",
            L"monitor",
        };
        residual[2].distinctiveTokens = {
            L"license",
            L"manager",
        };
        residual[3].distinctiveTokens = {
            L"service",
            L"manager",
        };
        residual[4].distinctiveTokens = {
            L"network",
            L"designer",
        };

        std::vector<Command*> views;
        for (auto& command :
             residual) {
            views.push_back(
                &command);
        }

        CalibrateCatalogRoleContext(
            views);

        const auto family =
            engine.Search(
                residual,
                usage,
                L"fa",
                20);

        assert(
            ContainsCommand(
                family,
                0));
        assert(
            !ContainsCommand(
                family,
                1));
        assert(
            !ContainsCommand(
                family,
                2));
        assert(
            !ContainsCommand(
                family,
                3));
        assert(
            ContainsCommand(
                family,
                4));

        const auto monitorIntent =
            engine.Search(
                residual,
                usage,
                L"monitor",
                20);
        assert(
            ContainsCommand(
                monitorIntent,
                1));

        const auto licenseIntent =
            engine.Search(
                residual,
                usage,
                L"license",
                20);
        assert(
            ContainsCommand(
                licenseIntent,
                2));

        const auto serviceIntent =
            engine.Search(
                residual,
                usage,
                L"service",
                20);
        assert(
            ContainsCommand(
                serviceIntent,
                3));
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

    // Exact derived initials and a longer acronym are both learned choices.
    // Their old 122-point difference permanently defeated the 32-point cap.
    {
        std::vector<Command> initials{
            MakeCommand(L"new", L"teamspeak", L"TeamSpeak", L"new.exe", 0),
            MakeCommand(L"classic", L"teamspeak3client", L"TeamSpeak 3 Client", L"classic.exe", 1),
            MakeCommand(L"other", L"taskservicecontrol", L"Task Service Control", L"other.exe", 2),
        };
        for (auto& item : initials) {
            item.source = CommandSource::StartMenu;
            item.surfaceClass = LaunchSurfaceClass::PrimaryApplication;
            item.basePriority = 0;
        }
        UsageMap learned;
        assert(engine.Search(initials, learned, L"ts", 10, false, false)
                   .front().commandIndex == 0);
        learned[L"classic"] = UsageStat{1, 1, {{L"ts", 1}}};
        assert(engine.Search(initials, learned, L"ts", 10, false, false)
                   .front().commandIndex == 0);
        learned[L"classic"] = UsageStat{2, 1, {{L"ts", 2}}};
        assert(engine.Search(initials, learned, L"ts", 10, false, false)
                   .front().commandIndex == 1);
        // Learning ts neither steals team nor outranks an actual exact name.
        assert(engine.Search(initials, learned, L"team", 10, false, false)
                   .front().commandIndex == 0);
        assert(engine.Search(initials, learned, L"teamspeak", 10, false, false)
                   .front().commandIndex == 0);
        auto explicitName = MakeCommand(L"exact", L"ts", L"TS", L"exact.exe", 3);
        explicitName.source = CommandSource::StartMenu;
        explicitName.surfaceClass = LaunchSurfaceClass::PrimaryApplication;
        initials.push_back(explicitName);
        learned[L"classic"].queryLaunches[L"ts"] = 100000;
        assert(engine.Search(initials, learned, L"ts", 10, false, false)
                   .front().commandIndex == 3);
    }

    // Short family prefixes can match an unrelated name as strongly as a
    // suite companion. Repeated real launches may reorder comparable prefix
    // matches, while one accidental launch and stronger intent cannot.
    {
        std::vector<Command> habits{
            MakeCommand(L"game", L"acmesolitaire", L"Acme Solitaire",
                        L"Solitaire.exe", 0),
            MakeCommand(L"design", L"acmestudiodesigner",
                        L"Acme Studio Designer", L"Designer.exe", 1),
            MakeCommand(L"exact", L"ac", L"AC", L"Exact.exe", 2),
        };
        for (auto& item : habits) {
            item.source = CommandSource::StartMenu;
            item.surfaceClass = LaunchSurfaceClass::PrimaryApplication;
            item.basePriority = 0;
        }

        UsageMap habitUsage;
        const auto cold = engine.Search(habits, habitUsage, L"ac", 10,
                                        false, false);
        assert(cold.front().commandIndex == 2);
        assert(cold[1].commandIndex == 0);

        habitUsage[L"design"] = UsageStat{1, 4102444800LL,
                                           {{L"ac", 1}}};
        const auto oneLaunch = engine.Search(habits, habitUsage, L"ac", 10,
                                             false, false);
        assert(oneLaunch[1].commandIndex == 0);

        habitUsage[L"design"] = UsageStat{2, 1,
                                           {{L"ac", 2}}};
        const auto repeated = engine.Search(habits, habitUsage, L"ac", 10,
                                            false, false);
        assert(repeated.front().commandIndex == 2);
        assert(repeated[1].commandIndex == 1);

        habitUsage[L"design"] = UsageStat{100000, 1,
                                           {{L"other", 100000}}};
        const auto unrelated = engine.Search(habits, habitUsage, L"ac", 10,
                                             false, false);
        assert(unrelated[1].commandIndex == 0);
        habitUsage[L"design"].queryLaunches[L"ac"] = 100000;
        const auto capped = engine.Search(habits, habitUsage, L"ac", 10,
                                          false, false);
        assert(capped.front().commandIndex == 2);
    }

    {
        std::vector<Command> shortcuts{
            MakeCommand(L"a", L"steam", L"Steam", L"Steam.exe", 0),
            MakeCommand(L"b", L"spotify", L"Spotify", L"Spotify.exe", 1),
        };
        for (auto& item : shortcuts) {
            item.source = CommandSource::StartMenu;
            item.surfaceClass = LaunchSurfaceClass::PrimaryApplication;
            item.basePriority = 0;
        }
        UsageMap contextual;
        contextual[L"b"] = UsageStat{2, 1, {{L"s", 2}}};
        auto shortQuery = engine.Search(shortcuts, contextual, L"s", 10,
                                        false, false);
        assert(shortQuery.front().commandIndex == 1);

        // Launching the other application via "st" cannot steal "s".
        contextual[L"a"] = UsageStat{100, 1, {{L"st", 100}}};
        shortQuery = engine.Search(shortcuts, contextual, L"s", 10,
                                   false, false);
        assert(shortQuery.front().commandIndex == 1);
        const auto narrow = engine.Search(shortcuts, contextual, L"st", 10,
                                          false, false);
        assert(narrow.front().commandIndex == 0);
    }

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
