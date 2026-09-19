#include "core/SearchEngine.hpp"

#include <cassert>
#include <iostream>

using namespace altrun;

int main() {
    std::vector<Command> commands{
        {L"1", L"chrome", L"Google Chrome", L"chrome.exe", L"", L"", CommandSource::Custom, 100},
        {L"2", L"code", L"Visual Studio Code", L"code.exe", L"", L"", CommandSource::Custom, 100},
        {L"3", L"calc", L"Calculator", L"calc.exe", L"", L"", CommandSource::Custom, 100},
    };

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

    usage[L"3"] = UsageStat{42, 4102444800LL};
    auto frequent = engine.Search(commands, usage, L"", 10);
    assert(!frequent.empty());
    assert(frequent.front().commandIndex == 2);

    std::cout << "SearchEngine tests passed\n";
    return 0;
}
