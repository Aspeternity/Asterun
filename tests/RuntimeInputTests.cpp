#include "core/RuntimeInput.hpp"

#include <cassert>
#include <iostream>
#include <vector>
#include <utility>

using namespace altrun;

namespace {

Command MakeCommand(
    CommandType type,
    RuntimeInputMode mode,
    std::wstring target,
    std::wstring arguments = {}) {
    Command command;
    command.id = L"test-command";
    command.keyword = L"g";
    command.aliases = {L"go"};
    command.title = L"Runtime input";
    command.type = type;
    command.runtimeInputMode = mode;
    command.target = std::move(target);
    command.arguments =
        std::move(arguments);
    command.enabled = true;
    command.source =
        CommandSource::User;
    return command;
}

} // namespace

int main() {
    {
        const auto encoded =
            EncodeRuntimeInput(
                RuntimeInputMode::UrlEncoded,
                L"心脏 MRI");

        assert(
            encoded ==
            L"%E5%BF%83%E8%84%8F%20MRI");
    }

    {
        auto command =
            MakeCommand(
                CommandType::Url,
                RuntimeInputMode::UrlEncoded,
                L"https://example.com/?q={input}");

        assert(
            HasRuntimeInputPlaceholder(
                command));
        assert(
            CanAcceptRuntimeInput(
                command));

        const auto resolved =
            ResolveRuntimeInput(
                command,
                L"ALTRun Next");

        assert(
            resolved.target ==
            L"https://example.com/?q=ALTRun%20Next");
    }

    {
        auto command =
            MakeCommand(
                CommandType::Url,
                RuntimeInputMode::UrlEncoded,
                L"https://example.com/?q={query}");

        const auto resolved =
            ResolveRuntimeInput(
                command,
                L"心脏");

        assert(
            resolved.target ==
            L"https://example.com/?q=%E5%BF%83%E8%84%8F");
    }

    {
        auto command =
            MakeCommand(
                CommandType::CommandLine,
                RuntimeInputMode::Raw,
                L"ping.exe",
                L"-n 1");

        assert(
            CanAcceptRuntimeInput(
                command));

        const auto resolved =
            ResolveRuntimeInput(
                command,
                L"8.8.8.8");

        assert(
            resolved.arguments ==
            L"-n 1 8.8.8.8");
    }

    {
        auto command =
            MakeCommand(
                CommandType::Application,
                RuntimeInputMode::Raw,
                L"tool.exe",
                L"--value={input}");

        const auto resolved =
            ResolveRuntimeInput(
                command,
                L"hello world");

        assert(
            resolved.arguments ==
            L"--value=hello world");
    }

    {
        auto command =
            MakeCommand(
                CommandType::Folder,
                RuntimeInputMode::Raw,
                L"C:\\Users\\{input}");

        assert(
            CanAcceptRuntimeInput(
                command));

        const auto resolved =
            ResolveRuntimeInput(
                command,
                L"Asp");

        assert(
            resolved.target ==
            L"C:\\Users\\Asp");
    }

    {
        auto command =
            MakeCommand(
                CommandType::Url,
                RuntimeInputMode::Raw,
                L"https://example.com/");

        assert(
            !CanAcceptRuntimeInput(
                command));
    }

    {
        std::vector<Command> commands{
            MakeCommand(
                CommandType::CommandLine,
                RuntimeInputMode::Raw,
                L"ping.exe"),
        };

        const auto results =
            BuildRuntimeInputActionResults(
                commands,
                L"g 8.8.8.8",
                10);

        assert(results.size() == 1);
        assert(
            results[0].action.kind ==
            LauncherActionKind::
                ExecuteCommand);
        assert(
            results[0].action
                .commandIndex == 0);
        assert(
            results[0].action.payload ==
            L"8.8.8.8");

        const auto alias =
            BuildRuntimeInputActionResults(
                commands,
                L"GO example.com",
                10);
        assert(alias.size() == 1);
        assert(
            alias[0].action.payload ==
            L"example.com");

        assert(
            BuildRuntimeInputActionResults(
                commands,
                L"g",
                10).empty());
    }

    std::cout
        << "Runtime input tests passed\n";
    return 0;
}
