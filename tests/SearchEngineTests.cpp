#include "core/SearchEngine.hpp"

#include <cassert>
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

} // namespace

int main() {
    std::vector<Command> commands{
        MakeCommand(L"1", L"chrome", L"Google Chrome", L"chrome.exe", 0),
        MakeCommand(L"2", L"code", L"Visual Studio Code", L"code.exe", 1),
        MakeCommand(L"3", L"calc", L"Calculator", L"calc.exe", 2),
    };

    commands[1].aliases = {L"vscode", L"vs"};

    SearchEngine engine;
    UsageMap usage;

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

    usage[L"3"] = UsageStat{42, 4102444800LL};
    auto frequent = engine.Search(commands, usage, L"", 10);
    assert(!frequent.empty());
    assert(frequent.front().commandIndex == 2);

    usage.clear();
    commands[0].pinned = true;
    auto pinned = engine.Search(commands, usage, L"", 10);
    assert(!pinned.empty());
    assert(pinned.front().commandIndex == 0);

    std::cout << "SearchEngine tests passed\n";
    return 0;
}
