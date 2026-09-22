#pragma once

#include "../core/Command.hpp"

#include <windows.h>

#include <string>
#include <string_view>

namespace altrun {

class App;

class ShortcutEditorDialog {
public:
    [[nodiscard]] static bool Show(
        App& app,
        HINSTANCE instance,
        HWND owner,
        std::wstring_view commandId = {});

    [[nodiscard]] static bool ShowNew(
        App& app,
        HINSTANCE instance,
        HWND owner,
        const Command& seed);

private:
    ShortcutEditorDialog(
        App& app,
        HINSTANCE instance,
        HWND owner);
    ~ShortcutEditorDialog();

    bool Create(
        std::wstring_view commandId,
        const Command* seed = nullptr);
    bool RunModal();

    static LRESULT CALLBACK WindowProc(
        HWND hwnd,
        UINT message,
        WPARAM wParam,
        LPARAM lParam);

    LRESULT HandleMessage(
        UINT message,
        WPARAM wParam,
        LPARAM lParam);

    void CreateControls();
    void ApplyLanguage();
    void UpdateWindowTitle();
    void Layout();
    void ResizeForContent();
    void RefreshDynamicLayout();
    void UpdateAdvancedVisibility();
    void UpdateRuntimeTestVisibility();
    void ToggleAdvanced();
    void UpdateTypeState();
    void UpdateRuntimeInputHint();
    void MaybeAutoFillName();
    void SetNameText(
        std::wstring_view text,
        bool automatic);

    void LoadCommand(
        std::wstring_view commandId);
    void BeginNew(
        const Command* seed = nullptr);
    bool Save();
    void Test();
    void BrowseTargetFile();
    void BrowseTargetFolder();
    void BrowseWorkingDirectory();
    void BrowseIcon();
    void ResetIcon();

    [[nodiscard]] Command
    CollectCommand() const;

    [[nodiscard]] CommandType
    SelectedType() const;

    [[nodiscard]] RuntimeInputMode
    SelectedRuntimeInputMode() const;

    [[nodiscard]] std::wstring
    ControlText(
        HWND control) const;

    [[nodiscard]] const wchar_t* T(
        const wchar_t* zh,
        const wchar_t* en) const;

    [[nodiscard]] int Scale(
        int value) const;

    App& app_;
    HINSTANCE instance_{};
    HWND owner_{};
    HWND hwnd_{};

    HWND keywordLabel_{};
    HWND keyword_{};
    HWND keywordHint_{};
    HWND nameLabel_{};
    HWND name_{};
    HWND targetLabel_{};
    HWND target_{};
    HWND browseFile_{};
    HWND browseFolder_{};
    HWND typeLabel_{};
    HWND type_{};
    HWND typeHint_{};
    HWND runtimeInputLabel_{};
    HWND runtimeInput_{};
    HWND runtimeInputHint_{};
    HWND testInputLabel_{};
    HWND testInput_{};
    HWND advancedToggle_{};
    HWND argumentsLabel_{};
    HWND arguments_{};
    HWND workdirLabel_{};
    HWND workdir_{};
    HWND browseWorkdir_{};
    HWND iconLabel_{};
    HWND icon_{};
    HWND browseIcon_{};
    HWND resetIcon_{};
    HWND admin_{};
    HWND test_{};
    HWND save_{};
    HWND cancel_{};

    HFONT font_{};
    UINT dpi_{96};
    bool changed_{false};
    bool closed_{false};
    bool advancedExpanded_{false};
    bool nameAuto_{true};
    bool suppressNameChange_{false};
    std::wstring commandId_;
};

} // namespace altrun
