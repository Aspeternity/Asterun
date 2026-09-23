#pragma once

#include "../core/LauncherResult.hpp"
#include "../core/ResultIconPipeline.hpp"
#include "UiMetrics.hpp"
#include "UiTheme.hpp"

#include <windows.h>

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <filesystem>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
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
    static constexpr UINT kTrayMessage = WM_APP + 17;
    static constexpr UINT kIconReadyMessage = WM_APP + 18;
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

    struct ResultIconCacheEntry {
        HICON icon{};
        std::uint64_t lastUse{0};
    };

    struct ResultIconPending {
        std::uint64_t searchGeneration{0};
        std::uint64_t iconEpoch{0};
    };

    struct ResultIconJob {
        ResultIconRequestStamp stamp;
        HWND targetWindow{};
        std::wstring cacheKey;
        std::wstring source;
        std::filesystem::path baseDirectory;
    };

    struct ResultIconCompletion {
        ResultIconRequestStamp stamp;
        std::wstring cacheKey;
        HICON icon{};
    };

    [[nodiscard]] HICON ResultIcon(
        const LauncherResult& result);
    [[nodiscard]] int
    ResultIconPixelSize() const;
    void QueueResultIcon(
        const LauncherResult& result,
        std::wstring cacheKey,
        int pixelSize);
    void EnsureResultIconWorker();
    void ResultIconWorkerLoop();
    void HandleResultIconCompletions();
    void CancelPendingResultIconRequests();
    void ClearResultIconCache();
    void TrimResultIconCache();
    void InvalidateResultRowsForIconKey(
        std::wstring_view cacheKey);
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
    HWND hint_{};
    HWND list_{};
    HWND preview_{};
    WNDPROC oldEditProc_{};
    HFONT normalFont_{};
    HFONT auxiliaryFont_{};
    HFONT boldFont_{};
    HFONT titleFont_{};
    HBRUSH windowBrush_{};
    HBRUSH controlBrush_{};
    HBRUSH accentBrush_{};
    HBRUSH bottomBrush_{};
    HBITMAP classicShortcutBitmap_{};
    HBITMAP classicCloseBitmap_{};
    std::unordered_map<
        std::wstring,
        ResultIconCacheEntry>
        resultIconCache_;
    std::unordered_map<
        std::wstring,
        ResultIconPending>
        pendingResultIcons_;
    std::deque<ResultIconJob>
        resultIconJobs_;
    std::deque<ResultIconCompletion>
        resultIconCompletions_;
    std::mutex resultIconWorkerMutex_;
    std::condition_variable
        resultIconWorkerCv_;
    std::thread resultIconWorker_;
    bool resultIconWorkerStop_{false};
    std::uint64_t resultIconEpoch_{0};
    std::uint64_t resultIconCacheTick_{0};
    bool trayIconAdded_{false};
    bool firstRevealPending_{true};
    bool imeComposing_{false};
    bool contextActionModalActive_{false};
    bool dynamicQueryPending_{false};
    bool immediateExecutionPending_{false};
    UINT dpi_{96};
    int widthLogical_{
        ui::kClassicLauncherMetrics.widthLogical};
    int rowHeightLogical_{
        ui::kClassicLauncherMetrics.rowHeightLogical};
    std::size_t maxResults_{
        ui::kClassicLauncherMetrics.maxResults};
    std::wstring titleText_{L"[ALTRun]"};
    std::uint64_t searchGeneration_{0};
    std::vector<LauncherResult>
        staticResults_;
    std::vector<LauncherResult>
        dynamicResults_;
    std::vector<LauncherResult> results_;
};

} // namespace altrun
