#pragma once

#include <windows.h>
#include <commctrl.h>

#include <array>
#include <string>
#include <string_view>
#include <vector>

namespace altrun {

class App;
struct Command;

class ShortcutManagerWindow {
public:
    ShortcutManagerWindow(
        App& app,
        HINSTANCE instance);
    ~ShortcutManagerWindow();

    bool Create();
    void Show(
        std::wstring_view preferredId = {});
    void Refresh(
        std::wstring_view preferredId = {});
    void ApplyLanguage();

private:
    static constexpr UINT
        kIdAdd = 52101;
    static constexpr UINT
        kIdEdit = 52102;
    static constexpr UINT
        kIdDelete = 52103;
    static constexpr UINT
        kIdTest = 52104;
    static constexpr UINT
        kIdPathConversion = 52105;
    static constexpr UINT
        kIdFilter = 52106;
    static constexpr UINT
        kIdList = 52120;

    static constexpr UINT_PTR
        kChildSubclassId = 1;

    static LRESULT CALLBACK WindowProc(
        HWND hwnd,
        UINT message,
        WPARAM wParam,
        LPARAM lParam);

    static LRESULT CALLBACK
    ChildSubclassProc(
        HWND hwnd,
        UINT message,
        WPARAM wParam,
        LPARAM lParam,
        UINT_PTR subclassId,
        DWORD_PTR refData);

    LRESULT HandleMessage(
        UINT message,
        WPARAM wParam,
        LPARAM lParam);

    void CreateControls();
    void Layout();
    void RecreateFonts();
    void ApplyConfiguredPlacement();
    [[nodiscard]] int
    ClampTrackedColumnWidth(
        int column,
        int proposedWidth) const;
    void UpdateColumnWidths(
        int resizedColumn = -1,
        int proposedWidth = -1);
    [[nodiscard]] bool
    HandleHeaderNotification(
        LPARAM lParam,
        LRESULT& result);
    void UpdateEmptyText();
    void ReleaseWindowResources();
    void CloseWindow();
    void ResetTransientState(
        std::wstring_view preferredId);
    void DrawActionButton(
        const DRAWITEMSTRUCT& item);
    LRESULT HandleListCustomDraw(
        NMLVCUSTOMDRAW* draw);
    [[nodiscard]] bool HandleChildKeyDown(
        HWND source,
        WPARAM key);

    [[nodiscard]] std::wstring
    SelectedId() const;
    [[nodiscard]] const Command*
    SelectedCommand() const;

    void AddShortcut();
    void EditSelected();
    void DeleteSelected();
    void TestSelected();
    void ConvertPaths();
    void ShowContextMenu(
        POINT point);
    void LocateSelected();
    void CopySelectedTarget();

    [[nodiscard]] const wchar_t* T(
        const wchar_t* zh,
        const wchar_t* en) const;

    [[nodiscard]] int Scale(
        int value) const;

    App& app_;
    HINSTANCE instance_{};
    HWND hwnd_{};
    HWND add_{};
    HWND edit_{};
    HWND delete_{};
    HWND test_{};
    HWND pathConversion_{};
    HWND filter_{};
    HWND list_{};
    HFONT font_{};
    HFONT semiboldFont_{};
    UINT dpi_{96};
    bool customColumnWidths_{false};
    bool adjustingColumnWidths_{false};
    bool columnTracking_{false};
    int trackedColumn_{-1};
    int trackedColumnWidth_{-1};
    bool suppressFilterRefresh_{false};
    std::vector<std::wstring>
        visibleIds_;
};

} // namespace altrun
