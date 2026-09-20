#pragma once

#include <windows.h>

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
        kIdClose = 52107;
    static constexpr UINT
        kIdList = 52120;

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
    void Layout();

    [[nodiscard]] std::wstring
    SelectedId() const;
    [[nodiscard]] const Command*
    SelectedCommand() const;

    void AddShortcut();
    void EditSelected();
    void DeleteSelected();
    void TestSelected();
    void ConvertPaths();

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
    HWND close_{};
    HWND list_{};
    HFONT font_{};
    UINT dpi_{96};
    std::vector<std::wstring>
        visibleIds_;
};

} // namespace altrun
