#include "../src/core/ShortcutEditorModel.hpp"

#include <cassert>
#include <filesystem>

using namespace altrun;

int main() {
    {
        const auto parsed =
            ParseShortcutKeywords(
                L"v2rayN, vpn，proxy; VPN");

        assert(parsed.primary == L"v2rayN");
        assert(parsed.aliases.size() == 2);
        assert(parsed.aliases[0] == L"vpn");
        assert(parsed.aliases[1] == L"proxy");

        assert(
            FormatShortcutKeywords(
                parsed.primary,
                parsed.aliases) ==
            L"v2rayN, vpn, proxy");
    }

    {
        Command chrome;
        chrome.id = L"chrome-id";
        chrome.keyword = L"chrome";
        chrome.aliases = {L"browser", L"web"};
        chrome.title = L"Google Chrome";
        chrome.target = L"C:\\Apps\\Chrome\\chrome.exe";

        Command edge;
        edge.id = L"edge-id";
        edge.keyword = L"edge";
        edge.aliases = {L"BROWSER"};
        edge.title = L"Microsoft Edge";
        edge.target = L"C:\\Apps\\Edge\\msedge.exe";

        const std::vector<Command> existing{
            chrome,
        };

        const auto conflict =
            FindShortcutKeywordConflict(
                edge,
                existing);

        assert(conflict.has_value());
        assert(conflict->token == L"BROWSER");
        assert(conflict->existingId == L"chrome-id");
        assert(conflict->existingTitle == L"Google Chrome");

        assert(
            !FindShortcutKeywordConflict(
                 chrome,
                 existing,
                 L"chrome-id")
                 .has_value());

        assert(ShortcutMatchesFilter(chrome, L"browser"));
        assert(ShortcutMatchesFilter(chrome, L"CHROME"));
        assert(ShortcutMatchesFilter(chrome, L"google"));
        assert(ShortcutMatchesFilter(chrome, L"Apps\\Chrome"));
        assert(!ShortcutMatchesFilter(chrome, L"firefox"));
        assert(ShortcutMatchesFilter(chrome, L""));
    }

    assert(
        InferShortcutCommandType(
            L"https://www.google.com/search?q=test") ==
        CommandType::Url);

    assert(
        InferShortcutCommandType(
            L"steam://run/730") ==
        CommandType::Url);

    assert(
        InferShortcutCommandType(
            L"C:\\Tools\\backup.cmd") ==
        CommandType::CommandLine);

    assert(
        InferShortcutCommandType(
            L"C:\\Tools\\app.exe") ==
        CommandType::Application);

    assert(
        InferShortcutCommandType(
            L"C:\\Temp\\") ==
        CommandType::Folder);

    const auto temp =
        std::filesystem::temp_directory_path();
    assert(
        InferShortcutCommandType(
            temp.wstring()) ==
        CommandType::Folder);

    assert(
        SuggestShortcutTitle(
            L"C:\\Tools\\v2rayN\\v2rayN.exe",
            CommandType::Application) ==
        L"v2rayN");

    assert(
        SuggestShortcutTitle(
            L"https://www.google.com/search?q=test",
            CommandType::Url) ==
        L"google.com");

    assert(
        SuggestShortcutTitle(
            L"",
            CommandType::Application,
            L"fallback") ==
        L"fallback");

    assert(
        DefaultShortcutWorkingDirectory(
            CommandType::Application,
            L"C:\\Tools\\v2rayN\\v2rayN.exe") ==
        L"C:\\Tools\\v2rayN");

    assert(
        DefaultShortcutWorkingDirectory(
            CommandType::CommandLine,
            L"C:\\Scripts\\backup.cmd") ==
        L"C:\\Scripts");

    assert(
        DefaultShortcutWorkingDirectory(
            CommandType::Url,
            L"https://example.com") ==
        L"");

    assert(
        DefaultShortcutWorkingDirectory(
            CommandType::Application,
            L"notepad.exe") ==
        L"");

    return 0;
}
