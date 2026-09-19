#pragma once

#include <string>

namespace altrun {

enum class CommandSource {
    Custom,
    StartMenu,
};

struct Command {
    std::wstring id;
    std::wstring keyword;
    std::wstring title;
    std::wstring target;
    std::wstring arguments;
    std::wstring workingDirectory;
    CommandSource source{CommandSource::Custom};
    int basePriority{0};
};

} // namespace altrun
