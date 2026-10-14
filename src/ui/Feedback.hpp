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
// For one-line, single-action notices where TaskDialog's native minimum width
// creates excessive blank space; does not affect normal warning/confirm dialogs.
int ShowCompactNotice(HWND owner, const wchar_t* text, const wchar_t* title,
                      bool chinese = true);
bool ConfirmShortcutDeletion(HWND owner, std::wstring_view name, bool chinese);

// Product-level native TaskDialog flows shared by Settings and Uninstall.
bool ConfirmEverythingSetup(HWND owner, bool chinese);

enum class UninstallDataChoice {
    Cancel,
    Preserve,
    Delete,
};

[[nodiscard]] UninstallDataChoice
ChooseUninstallData(HWND owner, bool chinese);

bool ConfirmPermanentUserDataDeletion(
    HWND owner,
    bool chinese);

} // namespace altrun::ui
