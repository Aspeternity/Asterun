#pragma once

#include "../core/FeedbackPolicy.hpp"
#include <windows.h>
#include <string_view>

namespace altrun::ui {

// UI-thread-only. Disabled until the application's persisted settings are loaded.
void SetFeedbackEnabled(bool enabled);
void PlayFeedback(FeedbackCue cue);

// Native, silent dialogs; optional failure audio is owned by PlayFeedback.
int ShowMessage(HWND owner, const wchar_t* text, const wchar_t* title, UINT flags);
bool ConfirmShortcutDeletion(HWND owner, std::wstring_view name, bool chinese);

} // namespace altrun::ui
