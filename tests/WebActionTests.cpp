#include "core/ProviderIds.hpp"
#include "core/WebAction.hpp"

#include <cassert>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

using namespace altrun;

namespace {

Command SearchCommand(
    std::wstring keyword,
    std::vector<std::wstring> aliases,
    std::wstring target) {
    Command command;
    command.id = L"search-command";
    command.keyword = std::move(keyword);
    command.aliases = std::move(aliases);
    command.title = L"Google Search";
    command.type = CommandType::Url;
    command.target = std::move(target);
    command.enabled = true;
    command.source = CommandSource::User;
    return command;
}

} // namespace

int main() {
    {
        const auto results = BuildWebActionResults(
            {}, L"https://example.com/a?b=1", 10);
        assert(results.size() == 1);
        assert(results[0].providerId == providers::kBuiltinWeb);
        assert(results[0].kind == ResultKind::Action);
        assert(results[0].action.kind == LauncherActionKind::OpenUrl);
        assert(results[0].target == L"https://example.com/a?b=1");
        assert(results[0].action.payload == results[0].target);
    }

    {
        const auto results = BuildWebActionResults(
            {}, L"  WWW.Example.com/path  ", 10);
        assert(results.size() == 1);
        assert(results[0].target == L"https://WWW.Example.com/path");
    }

    {
        auto search =
            SearchCommand(
                L"g",
                {L"google"},
                L"https://www.google.com/search?q={query}");
        search.icon =
            L"C:\\Icons\\google.ico";

        const std::vector<Command> commands{
            search,
        };

        const auto results = BuildWebActionResults(
            commands, L"g ALTRun Next", 10);
        assert(results.size() == 1);
        assert(results[0].action.commandIndex == 0);
        assert(
            results[0].iconSource ==
            L"C:\\Icons\\google.ico");
        assert(
            results[0].target ==
            L"https://www.google.com/search?q=ALTRun%20Next");

        const auto alias = BuildWebActionResults(
            commands, L"GOOGLE \u5FC3\u810F MRI", 10);
        assert(alias.size() == 1);
        assert(
            alias[0].target ==
            L"https://www.google.com/search?q=%E5%BF%83%E8%84%8F%20MRI");

        const auto emptyQuery = BuildWebActionResults(
            commands, L"g", 10);
        assert(emptyQuery.size() == 1);
        assert(
            emptyQuery[0].target ==
            L"https://www.google.com/search?q=");
    }

    {
        auto explicitRuntime =
            SearchCommand(
                L"g",
                {},
                L"https://www.google.com/search?q={query}");
        explicitRuntime.runtimeInputMode =
            RuntimeInputMode::UrlEncoded;

        const std::vector<Command> commands{
            explicitRuntime,
        };

        // Schema-2 runtime-input shortcuts are owned by RuntimeInput,
        // not the legacy {query} WebAction compatibility path.
        assert(BuildWebActionResults(
            commands,
            L"g ALTRun Next",
            10).empty());
    }

    {
        const std::vector<Command> commands{
            SearchCommand(
                L"gh",
                {L"github"},
                L"https://github.com"),
        };
        assert(BuildWebActionResults(
            commands, L"gh ALTRunNext", 10).empty());
    }

    {
        const std::vector<Command> commands{
            SearchCommand(
                L"x",
                {},
                L"file:///tmp/{query}"),
        };
        assert(BuildWebActionResults(
            commands, L"x test", 10).empty());
    }

    assert(BuildWebActionResults(
        {}, L"not a url", 10).empty());

    std::cout << "Smart web action tests passed\n";
    return 0;
}
