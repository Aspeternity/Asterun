#pragma once

#include "Settings.hpp"

#include <string_view>

namespace altrun {

enum class TextId {
    CreateWindowFailed,
    UnableToLaunch,
    UnableToCopy,
    CopyTextAction,
    HotkeyBusy,
    SearchPlaceholder,
    ClassicHint,
    CommandPrefix,
    TrayShow,
    TrayReload,
    TrayAppearance,
    TrayClassic,
    TrayModern,
    TrayLanguage,
    TrayChinese,
    TrayEnglish,
    TrayExit,
};

[[nodiscard]] std::wstring_view LocalizedText(TextId id, Language language);

} // namespace altrun
