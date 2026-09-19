#include "App.hpp"

#include "../platform/WinUtil.hpp"
#include "../ui/LauncherWindow.hpp"

#include <shellapi.h>

namespace altrun {

App::App(HINSTANCE instance)
    : instance_(instance),
      baseDirectory_(win::ExecutableDirectory()),
      commandStore_(baseDirectory_),
      usageStore_(baseDirectory_ / "usage.tsv") {}

App::~App() = default;

int App::Run() {
    commandStore_.Reload();
    usageStore_.Load();

    window_ = std::make_unique<LauncherWindow>(*this, instance_);
    if (!window_->Create()) {
        MessageBoxW(nullptr, L"Unable to create ALTRun Next launcher window.", L"ALTRun Next", MB_ICONERROR | MB_OK);
        return 1;
    }

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return static_cast<int>(msg.wParam);
}

void App::ReloadCommands() {
    commandStore_.Reload();
    if (window_) window_->RefreshResults();
}

std::vector<SearchResult> App::Search(std::wstring_view query, std::size_t limit) const {
    return searchEngine_.Search(commandStore_.Commands(), usageStore_.Data(), query, limit);
}

const Command& App::GetCommand(std::size_t index) const {
    return commandStore_.Commands().at(index);
}

bool App::ExecuteCommand(std::size_t index) {
    const auto& command = commandStore_.Commands().at(index);

    const std::wstring target = win::ExpandEnvironment(command.target);
    const std::wstring args = win::ExpandEnvironment(command.arguments);
    const std::wstring cwd = win::ExpandEnvironment(command.workingDirectory);

    SHELLEXECUTEINFOW info{};
    info.cbSize = sizeof(info);
    info.fMask = SEE_MASK_NOASYNC | SEE_MASK_FLAG_NO_UI;
    info.hwnd = nullptr;
    info.lpFile = target.c_str();
    info.lpParameters = args.empty() ? nullptr : args.c_str();
    info.lpDirectory = cwd.empty() ? nullptr : cwd.c_str();
    info.nShow = SW_SHOWNORMAL;

    if (!ShellExecuteExW(&info)) {
        const DWORD error = GetLastError();
        std::wstring message = L"Unable to launch:\n" + target + L"\n\n" + win::FormatWin32Error(error);
        MessageBoxW(nullptr, message.c_str(), L"ALTRun Next", MB_ICONERROR | MB_OK);
        return false;
    }

    usageStore_.Record(command.id);
    return true;
}

} // namespace altrun
