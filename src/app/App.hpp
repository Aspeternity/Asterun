#pragma once

#include "../core/CommandStore.hpp"
#include "../core/SearchEngine.hpp"
#include "../core/UsageStore.hpp"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <filesystem>
#include <memory>
#include <string_view>
#include <vector>

namespace altrun {

class LauncherWindow;

class App {
public:
    explicit App(HINSTANCE instance);
    ~App();

    int Run();
    void ReloadCommands();

    [[nodiscard]] std::vector<SearchResult> Search(std::wstring_view query, std::size_t limit) const;
    [[nodiscard]] const Command& GetCommand(std::size_t index) const;
    bool ExecuteCommand(std::size_t index);

private:
    HINSTANCE instance_{};
    std::filesystem::path baseDirectory_;
    CommandStore commandStore_;
    UsageStore usageStore_;
    SearchEngine searchEngine_;
    std::unique_ptr<LauncherWindow> window_;
};

} // namespace altrun
