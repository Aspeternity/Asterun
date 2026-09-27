#pragma once

#include "../core/LauncherResult.hpp"
#include "../core/ClassicBehavior.hpp"
#include "UiMetrics.hpp"
#include "UiTheme.hpp"

#include <windows.h>

#include <array>
#include <cstddef>
#include <deque>
#include <string>
#include <string_view>
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
        bool allowImmediateExecution = false,
        bool preserveSelection = true);
    void ApplyNumericContinuation(std::uint64_t token, classic_behavior::ContinuationEvidence evidence);
    void ApplyDynamicResults(
        std::uint64_t generation,
        std::vector<LauncherResult> results);
    void ApplyAppearance();
    void ApplyLanguage();
    void ApplyGeneralSettings();
    void Toggle();
    void ShowStartupNotification(
        std::wstring_view activationHotkey);
    void QueueNewShortcutForPath(
        std::wstring path);

    [[nodiscard]] bool
    IsVisible() const noexcept {
        return hwnd_ != nullptr &&
            IsWindowVisible(hwnd_);
    }

private:
    friend struct NumericIntentRuntimeFixture;
    static constexpr UINT kTrayMessage = WM_APP + 17;
    static constexpr UINT kShortcutIpcMessage = WM_APP + 19;
    static constexpr UINT_PTR
        kNumericIntentTimerId = 0xA176;
    static constexpr UINT kMenuShow = 40001;
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
    void PaintClassicBackground(HDC dc, const RECT& client);
    void PaintClassicTitleBar(HDC dc, const RECT& client);
    void PaintClassicLogo(HDC dc, int x, int y);
    void PaintClassicClose(HDC dc, const RECT& rect);
    void UpdatePreview();

    void RebuildVisibleResults(
        bool allowImmediateExecution,
        bool preserveSelection,
        bool queryEmpty);
    void ExecuteSelection(
        LauncherExecutionIntent intent =
            LauncherExecutionIntent::Default);
    void ExecuteResultAt(
        std::size_t resultIndex,
        LauncherExecutionIntent intent =
            LauncherExecutionIntent::Default);
    void ExecuteResultSnapshot(
        const LauncherResult& result,
        LauncherExecutionIntent intent =
            LauncherExecutionIntent::Default,
        const std::wstring* snapshotQuery = nullptr);
    [[nodiscard]] int QuickLaunchIndexForKey(
        WPARAM key) const;
    [[nodiscard]] int NumericDigitForKey(
        WPARAM key) const noexcept;
    [[nodiscard]] bool
    HasStrongNumericContinuation(
        wchar_t digit) const;
    [[nodiscard]] bool
    HasRecentTextInput() const noexcept;
    void QueuePendingNumericIntent(
        UINT virtualKey,
        wchar_t digit,
        const LauncherResult& result);
    void CommitPendingNumericIntentAsText();
    void ExecutePendingNumericIntent();
    void CancelPendingNumericIntent();
    void ConsumeNumericKey(
        UINT virtualKey,
        wchar_t digit) noexcept;
    [[nodiscard]] std::wstring ResultNumberLabel(
        std::size_t resultIndex) const;
    void MoveSelection(int delta);
    void AddTrayIcon(
        bool force = false);
    void RemoveTrayIcon();
    void ProcessPendingShortcutPaths();
    void ShowNewShortcutForPath(
        std::wstring_view path);
    void ShowTrayMenu(POINT point);
    void ShowResultContextMenu(
        POINT point);

    [[nodiscard]] bool IsModern() const;
    [[nodiscard]] RECT ClassicCloseRect() const;
    [[nodiscard]] std::wstring CurrentQuery() const;
    [[nodiscard]] const ui::UiPalette& CurrentPalette() const;
    [[nodiscard]] int DpiScale(int value) const;

    App& app_;
    HINSTANCE instance_{};
    HWND hwnd_{};
    HWND edit_{};
    HWND list_{};
    HWND preview_{};
    HWND classicPreview_{};
    WNDPROC oldEditProc_{};
    HFONT normalFont_{};
    HFONT auxiliaryFont_{};
    HFONT boldFont_{};
    HFONT titleFont_{};
    HBRUSH windowBrush_{};
    HBRUSH controlBrush_{};
    HBRUSH accentBrush_{};
    HBRUSH bottomBrush_{};
    std::array<
        HBITMAP,
        ui::kClassicGlyphAssetPixelSizes.size()>
        classicShortcutBitmaps_{};
    std::array<
        HBITMAP,
        ui::kClassicGlyphAssetPixelSizes.size()>
        classicCloseBitmaps_{};
    HBITMAP classicBackgroundBitmap_{};
    HDC classicBitmapDc_{};
    SIZE classicBackgroundSize_{};
    std::deque<std::wstring>
        pendingShortcutPaths_;
    bool trayIconAdded_{false};
    bool notificationOnlyTrayIcon_{false};
    UINT taskbarCreatedMessage_{0};
    bool firstRevealPending_{true};
    bool imeComposing_{false};
    bool contextActionModalActive_{false};
    bool dynamicQueryPending_{false};
    bool immediateExecutionPending_{false};
    bool numericTextCommitInProgress_{false};
    std::uint64_t lastTextInputTick_{0};
    UINT consumedNumericVirtualKey_{0};
    wchar_t consumedNumericChar_{0};

    struct PendingNumericIntent {
        bool active{false};
        UINT virtualKey{0};
        wchar_t digit{0};
        LauncherResult result{};
        std::wstring query;
        std::uint64_t token{0};
        std::uint64_t started{0};
        DWORD selection{0};
        bool probeRequired{false};
        classic_behavior::ContinuationEvidence evidence{classic_behavior::ContinuationEvidence::Unknown};
    };

    PendingNumericIntent
        pendingNumericIntent_{};

    std::uint64_t numericIntentToken_{0};

    UINT dpi_{96};
    ui::ClassicLauncherDpiMetrics
        classicDpiMetrics_{
            ui::ClassicLauncherMetricsForDpi(
                96)};
    int widthLogical_{
        ui::kClassicLauncherMetrics.widthLogical};
    int rowHeightLogical_{
        ui::kClassicLauncherMetrics.rowHeightLogical};
    std::size_t maxResults_{
        ui::kClassicLauncherMetrics.maxResults};
    std::wstring titleText_{};
    std::wstring previewText_{};
    std::uint64_t searchGeneration_{0};
    std::vector<LauncherResult>
        staticResults_;
    std::vector<LauncherResult>
        dynamicResults_;
    std::vector<LauncherResult> results_;
};

} // namespace altrun
