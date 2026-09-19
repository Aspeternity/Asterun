#pragma once

#include <string>
#include <vector>

namespace altrun {

enum class CommandSource {
    User,
    StartMenu,
};

enum class CommandType {
    Application,
    Url,
    Folder,
    CommandLine,
};

struct Command {
    std::wstring id;
    std::wstring keyword;
    std::vector<std::wstring> aliases;
    std::wstring title;
    CommandType type{CommandType::Application};
    std::wstring target;
    std::wstring arguments;
    std::wstring workingDirectory;
    std::wstring icon{L"auto"};
    bool enabled{true};
    bool runAsAdmin{false};
    bool pinned{false};
    int sortOrder{0};
    std::vector<std::wstring> legacyIds;
    CommandSource source{CommandSource::User};
    int basePriority{0};
};

} // namespace altrun
