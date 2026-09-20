#include "core/UserCommandStore.hpp"

#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

using namespace altrun;

namespace {

std::string ReadAll(
    const std::filesystem::path& path) {
    std::ifstream input(
        path,
        std::ios::binary);
    return std::string(
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>());
}

const Command* Find(
    const UserCommandStore& store,
    std::wstring_view id) {
    for (const auto& command :
         store.Commands()) {
        if (command.id == id) {
            return &command;
        }
    }
    return nullptr;
}

} // namespace

int main() {
    const auto nonce =
        std::chrono::high_resolution_clock::
            now()
            .time_since_epoch()
            .count();

    const auto root =
        std::filesystem::temp_directory_path() /
        ("altrun-path-update-" +
         std::to_string(nonce));

    std::filesystem::create_directories(root);

    const auto jsonPath =
        root / "commands.json";

    UserCommandStore store(jsonPath);
    store.Load();

    assert(store.Commands().size() >= 2);

    const std::wstring firstId =
        store.Commands()[0].id;
    const std::wstring secondId =
        store.Commands()[1].id;

    std::vector<UserCommandPathUpdate>
        updates;

    UserCommandPathUpdate first;
    first.id = firstId;
    first.target =
        LR"(..\Tools\Notepad\notepad.exe)";
    first.workingDirectory =
        LR"(..\Tools\Notepad)";
    first.icon =
        LR"(..\Icons\Notepad.ico)";
    updates.push_back(first);

    UserCommandPathUpdate second;
    second.id = secondId;
    second.workingDirectory =
        L"%LOCALAPPDATA%";
    updates.push_back(second);

    assert(store.ApplyPathUpdates(updates));

    const Command* firstAfter =
        Find(store, firstId);
    const Command* secondAfter =
        Find(store, secondId);

    assert(firstAfter);
    assert(secondAfter);
    assert(
        firstAfter->target ==
        LR"(..\Tools\Notepad\notepad.exe)");
    assert(
        firstAfter->workingDirectory ==
        LR"(..\Tools\Notepad)");
    assert(
        firstAfter->icon ==
        LR"(..\Icons\Notepad.ico)");
    assert(
        secondAfter->workingDirectory ==
        L"%LOCALAPPDATA%");

    const std::string beforeFailure =
        ReadAll(jsonPath);

    std::vector<UserCommandPathUpdate>
        invalid;

    UserCommandPathUpdate validFirst;
    validFirst.id = firstId;
    validFirst.target = L"changed.exe";
    validFirst.icon =
        L"changed.ico";
    invalid.push_back(validFirst);

    UserCommandPathUpdate missing;
    missing.id = L"missing-command-id";
    missing.target = L"missing.exe";
    invalid.push_back(missing);

    assert(!store.ApplyPathUpdates(invalid));
    assert(ReadAll(jsonPath) == beforeFailure);

    firstAfter = Find(store, firstId);
    assert(firstAfter);
    assert(
        firstAfter->target ==
        LR"(..\Tools\Notepad\notepad.exe)");
    assert(
        firstAfter->icon ==
        LR"(..\Icons\Notepad.ico)");

    std::filesystem::remove_all(root);

    std::cout
        << "User command path update tests passed\n";
    return 0;
}
