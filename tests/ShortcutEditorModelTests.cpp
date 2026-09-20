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
