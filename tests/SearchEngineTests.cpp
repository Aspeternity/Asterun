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
