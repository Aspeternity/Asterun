#pragma once

#include <windows.h>
#include <commctrl.h>

#include <optional>
#include <string>
#include <vector>

namespace altrun {

class App;

class ShortcutPathConverterDialog {
public:
    [[nodiscard]] static bool Show(
        App& app,
        HINSTANCE instance,
        HWND owner);

private:
    enum class Mode {
        Portable,
        Absolute,
    };

    enum class Field {
        Target,
        WorkingDirectory,
        Icon,
    };

    struct Row {
        std::wstring commandId;
        Field field{Field::Target};
        std::wstring current;
        std::wstring converted;
        std::wstring resolved;
        bool exists{false};
        bool selected{false};
    };

    ShortcutPathConverterDialog(
        App& app,
        HINSTANCE instance,
        HWND owner);
    ~ShortcutPathConverterDialog();

    bool Create();
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
    void CloseWindow();
    void ApplyLanguage();
    void Layout();
    void Scan(
        std::optional<std::size_t>
            appliedFieldCount = std::nullopt);
    void ApplySelected();
    void UpdateSelectionState(
        std::optional<std::size_t>
            appliedFieldCount = std::nullopt);
    void UpdateColumnWidths(
        int resizedColumn = -1);
    [[nodiscard]] bool
    HandleHeaderNotification(
        LPARAM lParam,
        LRESULT& result);
    [[nodiscard]] std::size_t
    SelectedFieldCount() const;
    void DrawModeCard(
        const DRAWITEMSTRUCT& draw) const;
    void DrawActionButton(
        const DRAWITEMSTRUCT& draw) const;
    void InsertGroupHeader(
        std::wstring_view title);
    void InsertPreviewRow(
        Row row);
    [[nodiscard]] bool
    IsGroupHeaderItem(
        int itemIndex) const;
    [[nodiscard]] std::optional<std::size_t>
    RowIndexForListItem(
        int itemIndex) const;
    LRESULT HandleListCustomDraw(
        NMLVCUSTOMDRAW* draw);
    [[nodiscard]] const wchar_t*
    FieldLabel(
        Field field) const;
    [[nodiscard]] bool
    GetResultFieldInteractionRect(
        int itemIndex,
        RECT& interactionRect) const;
    [[nodiscard]] bool
    GetResultFieldLayout(
        int itemIndex,
        RECT& checkboxRect,
        RECT& textRect) const;
    void DrawResultFieldCell(
        HDC dc,
        int itemIndex) const;
    bool ToggleResultRowSelection(
        int itemIndex);

    [[nodiscard]] const wchar_t* T(
        const wchar_t* zh,
        const wchar_t* en) const;

    [[nodiscard]] int Scale(
        int value) const;

    App& app_;
    HINSTANCE instance_{};
    HWND owner_{};
    HWND hwnd_{};

    HWND modeTitle_{};
    HWND portable_{};
    HWND absolute_{};
    HWND rescan_{};
    HWND rule_{};
    HWND list_{};
    HWND status_{};
    HWND apply_{};

    HFONT font_{};
    HFONT groupFont_{};
    UINT dpi_{96};
    Mode mode_{Mode::Portable};
    bool changed_{false};
    bool closed_{false};
    bool customColumnWidths_{false};
    bool adjustingColumnWidths_{false};
    std::size_t convertibleShortcutCount_{0};
    std::vector<Row> rows_;
};

} // namespace altrun
