#pragma once

#include "../core/LauncherResult.hpp"

#include <windows.h>

#include <cstddef>
#include <string>
#include <unordered_map>
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
    void RefreshResults(
        bool allowImmediateExecution = false);
    void ApplyDynamicResults(
        std::uint64_t generation,
        std::vector<LauncherResult> results);
    void ApplyAppearance();
    void ApplyLanguage();
    void ApplyGeneralSettings();
    void ApplyResultIconPreference();
    void Toggle();

    [[nodiscard]] bool
    IsVisible() const noexcept {
        return hwnd_ != nullptr &&
            IsWindowVisible(hwnd_);
    }

private:
    struct ThemePalette {
        COLORREF windowBackground{};
        COLORREF controlBackground{};
        COLORREF accentBackground{};
        COLORREF text{};
        COLORREF mutedText{};
        COLORREF keyword{};
        COLORREF selectionBackground{};
        COLORREF selectionText{};
        COLORREF separator{};
        COLORREF frame{};
    };

    static constexpr UINT kTrayMessage = WM_APP + 17;
    static constexpr UINT kMenuShow = 40001;
    static constexpr UINT kMenuReload = 40002;
    static constexpr UINT kMenuSettings = 40003;
    static constexpr UINT kMenuShortcuts = 40004;
    static constexpr UINT kMenuAbout = 40005;
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
    void PaintClassicTitleBar(HDC dc, const RECT& client);
    void PaintClassicLogo(HDC dc, int x, int y);
    void PaintClassicClose(HDC dc, const RECT& rect);
    void UpdateHint();
    void UpdatePreview();
    [[nodiscard]] HICON ResultIcon(
        const LauncherResult& result);
    void ClearResultIconCache();
    void RebuildVisibleResults(
        bool allowImmediateExecution);
    void ExecuteSelection(
        LauncherExecutionIntent intent =
            LauncherExecutionIntent::Default);
    void ExecuteResultAt(
        std::size_t resultIndex,
        LauncherExecutionIntent intent =
            LauncherExecutionIntent::Default);
    [[nodiscard]] int QuickLaunchIndexForKey(
        WPARAM key) const;
    [[nodiscard]] std::wstring ResultNumberLabel(
        std::size_t resultIndex) const;
    void MoveSelection(int delta);
    void AddTrayIcon();
    void RemoveTrayIcon();
    void ShowTrayMenu(POINT point);

    [[nodiscard]] bool IsModern() const;
    [[nodiscard]] RECT ClassicCloseRect() const;
    [[nodiscard]] std::wstring CurrentQuery() const;
    [[nodiscard]] ThemePalette CurrentPalette() const;
    [[nodiscard]] int DpiScale(int value) const;

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
    HFONT titleFont_{};
    HBRUSH windowBrush_{};
    HBRUSH controlBrush_{};
    HBRUSH accentBrush_{};
    HBRUSH bottomBrush_{};
    std::unordered_map<std::wstring, HICON>
        resultIconCache_;
    bool trayIconAdded_{false};
    bool imeComposing_{false};
    bool dynamicQueryPending_{false};
    bool immediateExecutionPending_{false};
    UINT dpi_{96};
    int widthLogical_{420};
    int rowHeightLogical_{16};
    std::size_t maxResults_{10};
    std::wstring titleText_{L"[ALTRun]"};
    std::uint64_t searchGeneration_{0};
    std::vector<LauncherResult>
        staticResults_;
    std::vector<LauncherResult>
        dynamicResults_;
    std::vector<LauncherResult> results_;
};

} // namespace altrun
