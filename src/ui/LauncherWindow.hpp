#pragma once

#include "../core/SearchEngine.hpp"

#include <windows.h>

#include <cstddef>
#include <vector>

namespace altrun {

class App;

class LauncherWindow {
public:
    LauncherWindow(App& app, HINSTANCE instance);
    ~LauncherWindow();

    bool Create();
    void Show();
    void Hide();
    void RefreshResults();
    void ApplyAppearance();
    void ApplyLanguage();

private:
    struct ThemePalette {
        COLORREF windowBackground{};
        COLORREF controlBackground{};
        COLORREF text{};
        COLORREF mutedText{};
        COLORREF keyword{};
        COLORREF selectionBackground{};
        COLORREF selectionText{};
        COLORREF separator{};
        COLORREF frame{};
    };

    static constexpr UINT kHotkeyId = 0xA171;
    static constexpr UINT kTrayMessage = WM_APP + 17;
    static constexpr UINT kMenuShow = 40001;
    static constexpr UINT kMenuReload = 40002;
    static constexpr UINT kMenuThemeClassic = 40010;
    static constexpr UINT kMenuThemeModern = 40011;
    static constexpr UINT kMenuLangZh = 40020;
    static constexpr UINT kMenuLangEn = 40021;
    static constexpr UINT kMenuExit = 40030;

    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK EditProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

    LRESULT HandleMessage(UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT HandleEditMessage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

    void CreateChildren();
    void ApplyFonts();
    void RecreateBrushes();
    void UpdateControlFrames();
    void UpdateWindowChrome();
    void Layout();
    void Reposition();
    void PaintWindowBackground(HDC dc);
    void UpdateHint();
    void UpdatePreview();
    void ExecuteSelection();
    void MoveSelection(int delta);
    void AddTrayIcon();
    void RemoveTrayIcon();
    void ShowTrayMenu(POINT point);
    std::wstring CurrentQuery() const;
    ThemePalette CurrentPalette() const;
    int DpiScale(int value) const;

    App& app_;
    HINSTANCE instance_{};
    HWND hwnd_{};
    HWND edit_{};
    HWND hint_{};
    HWND list_{};
    HWND preview_{};
    WNDPROC oldEditProc_{};
    HFONT normalFont_{};
    HFONT boldFont_{};
    HBRUSH windowBrush_{};
    HBRUSH controlBrush_{};
    UINT dpi_{96};
    int widthLogical_{500};
    int rowHeightLogical_{22};
    std::size_t maxResults_{10};
    std::size_t hintCycle_{0};
    std::vector<SearchResult> results_;
};

} // namespace altrun
