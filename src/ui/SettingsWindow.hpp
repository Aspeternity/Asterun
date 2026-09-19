#pragma once

#include <windows.h>

#include <string>
#include <vector>

namespace altrun {

class App;

class SettingsWindow {
public:
    SettingsWindow(App& app, HINSTANCE instance);
    ~SettingsWindow();

    bool Create();
    void Show();
    void ApplyLanguage();
    void RefreshFromSettings();

private:
    enum class Page {
        General,
        Appearance,
        About,
    };

    static constexpr int kSidebarWidthLogical = 190;

    static constexpr UINT kIdNavGeneral = 51001;
    static constexpr UINT kIdNavAppearance = 51002;
    static constexpr UINT kIdNavAbout = 51003;

    static constexpr UINT kIdHideAfterLaunch = 51101;
    static constexpr UINT kIdClearQueryOnShow = 51102;
    static constexpr UINT kIdHideOnFocusLost = 51103;
    static constexpr UINT kIdShowTrayIcon = 51104;
    static constexpr UINT kIdPopupMonitor = 51105;

    static constexpr UINT kIdUiStyle = 51201;
    static constexpr UINT kIdLanguage = 51202;

    static constexpr UINT kIdOpenDataFolder = 51301;
    static constexpr UINT kIdOpenGitHub = 51302;

    static LRESULT CALLBACK WindowProc(
        HWND hwnd,
        UINT message,
        WPARAM wParam,
        LPARAM lParam);

    LRESULT HandleMessage(UINT message, WPARAM wParam, LPARAM lParam);

    void CreateControls();
    void CreateGeneralPage();
    void CreateAppearancePage();
    void CreateAboutPage();
    void ApplyFonts();
    void Layout();
    void CenterOnCurrentMonitor();
    void ShowPage(Page page);
    void UpdateNavLabels();
    void UpdatePageHeader();
    void ToggleGeneralSetting(UINT id);
    void ApplyMonitorControl();
    void ApplyAppearanceControls();
    void DrawGeneralToggle(const DRAWITEMSTRUCT& item);

    HWND CreateStatic(
        const wchar_t* text,
        DWORD style = SS_LEFT,
        DWORD exStyle = 0);

    HWND CreateButton(
        const wchar_t* text,
        UINT id,
        DWORD style = BS_PUSHBUTTON | BS_FLAT);

    HWND CreateCheckboxRow(
        const wchar_t* text,
        UINT id);

    [[nodiscard]] bool ToggleChecked(UINT id) const;
    [[nodiscard]] RECT BehaviorCardRect() const;
    [[nodiscard]] RECT MonitorCardRect() const;

    int Scale(int value) const;
    const wchar_t* T(const wchar_t* zh, const wchar_t* en) const;

    App& app_;
    HINSTANCE instance_{};
    HWND hwnd_{};

    HWND navGeneral_{};
    HWND navAppearance_{};
    HWND navAbout_{};
    HWND pageTitle_{};
    HWND pageDescription_{};

    HWND generalBehaviorTitle_{};
    HWND hideAfterLaunch_{};
    HWND clearQueryOnShow_{};
    HWND hideOnFocusLost_{};
    HWND showTrayIcon_{};
    HWND popupSectionTitle_{};
    HWND popupMonitorLabel_{};
    HWND popupMonitorDescription_{};
    HWND popupMonitor_{};
    HWND generalNote_{};

    HWND uiStyleLabel_{};
    HWND uiStyle_{};
    HWND languageLabel_{};
    HWND language_{};
    HWND appearanceNote_{};

    HWND aboutName_{};
    HWND aboutVersion_{};
    HWND aboutDescription_{};
    HWND dataPathLabel_{};
    HWND dataPath_{};
    HWND openDataFolder_{};
    HWND openGitHub_{};

    HFONT normalFont_{};
    HFONT titleFont_{};
    HFONT appNameFont_{};
    HFONT sectionFont_{};
    HBRUSH backgroundBrush_{};
    HBRUSH sidebarBrush_{};
    HBRUSH cardBrush_{};

    UINT dpi_{96};
    Page page_{Page::General};
    bool syncing_{false};

    std::vector<HWND> generalControls_;
    std::vector<HWND> appearanceControls_;
    std::vector<HWND> aboutControls_;
};

} // namespace altrun
