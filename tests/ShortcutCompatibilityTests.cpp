#include "core/TextCodec.hpp"
#include "core/UserCommandStore.hpp"

#include <algorithm>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>

using namespace altrun;

namespace {

void WriteWide(
    const std::filesystem::path& path,
    const std::wstring& value) {
    std::filesystem::create_directories(
        path.parent_path());

    std::ofstream output(
        path,
        std::ios::binary |
            std::ios::trunc);
    assert(output);

    const std::string utf8 =
        text::ToUtf8(value);
    output.write(
        utf8.data(),
        static_cast<std::streamsize>(
            utf8.size()));
    assert(output.good());
}

const Command& Find(
    const UserCommandStore& store,
    std::wstring_view keyword) {
    const auto it =
        std::find_if(
            store.Commands().begin(),
            store.Commands().end(),
            [&](const Command& command) {
                return command.keyword ==
                    keyword;
            });
    assert(
        it !=
        store.Commands().end());
    return *it;
}

template <typename Assertion>
void AssertImport(
    const std::filesystem::path& root,
    std::wstring_view name,
    const std::wstring& tsv,
    std::wstring_view keyword,
    Assertion assertion) {
    const auto tsvPath =
        root /
        (std::wstring(name) + L".tsv");
    const auto jsonPath =
        root /
        (std::wstring(name) + L".json");

    WriteWide(
        tsvPath,
        tsv);

    UserCommandStore store(
        jsonPath);
    store.Load();

    std::size_t imported = 0;
    std::size_t skipped = 0;

    assert(
        store.ImportTsv(
            tsvPath,
            false,
            &imported,
            &skipped));
    assert(imported == 1);
    assert(skipped == 0);

    assertion(
        Find(
            store,
            keyword));
}

} // namespace

int main() {
    const auto root =
        std::filesystem::
            temp_directory_path() /
        "altrun-shortcut-compatibility-test";

    std::error_code ec;
    std::filesystem::remove_all(
        root,
        ec);
    std::filesystem::create_directories(
        root);

    AssertImport(
        root,
        L"tsv-v1",
        L"# ALTRun Next commands TSV v1\n"
        L"# keyword\tname\taliases\ttype\ttarget\targuments\tworkingDirectory\tenabled\trunAsAdmin\tpinned\tsortOrder\n"
        L"v1app\t\u65e7\u683c\u5f0f\told,legacy\tapplication\tC:\\Tools\\\u5de5\u5177\\v1.exe\t--fixed\tC:\\Tools\\\u5de5\u5177\t0\t1\t1\t42\n",
        L"v1app",
        [](const Command& command) {
            assert(
                command.title ==
                L"\u65e7\u683c\u5f0f");
            assert(
                command.aliases.size() ==
                2);
            assert(
                command.aliases[0] ==
                L"old");
            assert(
                command.aliases[1] ==
                L"legacy");
            assert(
                command.target ==
                L"C:\\Tools\\\u5de5\u5177\\v1.exe");
            assert(
                command.workingDirectory ==
                L"C:\\Tools\\\u5de5\u5177");
            assert(command.enabled);
            assert(command.runAsAdmin);
            assert(!command.pinned);
            assert(
                command.sortOrder ==
                42);
            assert(
                command.runtimeInputMode ==
                RuntimeInputMode::None);
            assert(
                command.icon ==
                L"auto");
        });

    AssertImport(
        root,
        L"tsv-v2",
        L"# ALTRun Next commands TSV v2\n"
        L"# keyword\tname\taliases\ttype\ttarget\targuments\tworkingDirectory\tenabled\trunAsAdmin\tpinned\tsortOrder\truntimeInputMode\n"
        L"v2search\tV2 Search\tsearch,web\turl\thttps://example.test/?q={input}\t\t\t1\t0\t0\t50\turl-encoded\n",
        L"v2search",
        [](const Command& command) {
            assert(
                command.type ==
                CommandType::Url);
            assert(
                command.runtimeInputMode ==
                RuntimeInputMode::UrlEncoded);
            assert(
                command.icon ==
                L"auto");
            assert(command.enabled);
            assert(!command.pinned);
        });

    AssertImport(
        root,
        L"tsv-v3",
        L"# ALTRun Next commands TSV v3\n"
        L"# keyword\tname\taliases\ttype\ttarget\targuments\tworkingDirectory\tenabled\trunAsAdmin\tpinned\tsortOrder\truntimeInputMode\ticon\n"
        L"v3run\tV3 Runner\trunner,run\tcommand\t.\\tools\\runner.exe\t--name {input}\t.\\tools\t1\t1\t0\t60\traw\t.\\icons\\runner.ico\n",
        L"v3run",
        [](const Command& command) {
            assert(
                command.type ==
                CommandType::CommandLine);
            assert(
                command.target ==
                L".\\tools\\runner.exe");
            assert(
                command.arguments ==
                L"--name {input}");
            assert(
                command.workingDirectory ==
                L".\\tools");
            assert(
                command.runtimeInputMode ==
                RuntimeInputMode::Raw);
            assert(
                command.icon ==
                L".\\icons\\runner.ico");
            assert(command.runAsAdmin);
        });

    AssertImport(
        root,
        L"legacy-five-column",
        L"legacy5\tLegacy Five\tlegacy5.exe\t--old\tC:\\Legacy\n",
        L"legacy5",
        [](const Command& command) {
            assert(
                command.target ==
                L"legacy5.exe");
            assert(
                command.arguments ==
                L"--old");
            assert(
                command.workingDirectory ==
                L"C:\\Legacy");
            assert(
                command.runtimeInputMode ==
                RuntimeInputMode::None);
            assert(
                command.icon ==
                L"auto");
        });

    const auto sourcePath =
        root /
        "roundtrip-source.json";
    UserCommandStore source(
        sourcePath);
    source.Load();

    Command roundtrip;
    roundtrip.keyword =
        L"roundtrip";
    roundtrip.aliases = {
        L"round",
        L"\u5f80\u8fd4",
    };
    roundtrip.title =
        L"Round Trip \u6d4b\u8bd5";
    roundtrip.type =
        CommandType::CommandLine;
    roundtrip.target =
        L".\\bin\\tool.exe";
    roundtrip.arguments =
        L"--query {input}";
    roundtrip.workingDirectory =
        L".\\bin";
    roundtrip.runtimeInputMode =
        RuntimeInputMode::Raw;
    roundtrip.icon =
        L".\\icons\\tool.ico";
    roundtrip.runAsAdmin = true;

    assert(
        source.Create(
            roundtrip));

    const auto& sourceCommand =
        Find(
            source,
            L"roundtrip");

    const auto exportPath =
        root /
        "roundtrip-v3.tsv";
    assert(
        source.ExportTsv(
            exportPath));

    const auto destinationPath =
        root /
        "roundtrip-destination.json";
    UserCommandStore destination(
        destinationPath);
    destination.Load();

    std::size_t imported = 0;
    std::size_t skipped = 0;
    assert(
        destination.ImportTsv(
            exportPath,
            false,
            &imported,
            &skipped));
    assert(imported >= 1);

    const auto& restored =
        Find(
            destination,
            L"roundtrip");

    assert(
        restored.title ==
        sourceCommand.title);
    assert(
        restored.aliases ==
        sourceCommand.aliases);
    assert(
        restored.type ==
        sourceCommand.type);
    assert(
        restored.target ==
        sourceCommand.target);
    assert(
        restored.arguments ==
        sourceCommand.arguments);
    assert(
        restored.workingDirectory ==
        sourceCommand.workingDirectory);
    assert(
        restored.runtimeInputMode ==
        sourceCommand.runtimeInputMode);
    assert(
        restored.icon ==
        sourceCommand.icon);
    assert(
        restored.runAsAdmin ==
        sourceCommand.runAsAdmin);
    assert(restored.enabled);
    assert(!restored.pinned);
    assert(
        restored.sortOrder ==
        sourceCommand.sortOrder);

    std::filesystem::remove_all(
        root,
        ec);

    return 0;
}
