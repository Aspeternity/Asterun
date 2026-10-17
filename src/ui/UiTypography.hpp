#pragma once

#include "../core/Settings.hpp"

#include <windows.h>

namespace altrun::ui {

enum class UiFontRole {
    Body,
    BodySemibold,
    SettingsNavigation,
    ShortcutBody,
    ShortcutHint,
    SectionTitle,
    PageTitle,
    AppTitle,
    LauncherTitle,
    LauncherSearch,
    LauncherAuxiliary,
};

struct UiFontSpec {
    const wchar_t* face{};
    int pointSize{};
    int logicalHeight96{};
    int weight{};
    BYTE charset{DEFAULT_CHARSET};
    DWORD quality{CLEARTYPE_QUALITY};
    // The text is a fixed set of short labels (Settings navigation), not
    // arbitrary input, so it may use em sizes HintSafeFontHeight avoids for
    // free text and prefer the largest one YaHei still grid-fits.
    bool fixedLabels{false};
};

[[nodiscard]] UiFontSpec
ApplicationFontSpec(
    Language language,
    UiFontRole role) noexcept;

[[nodiscard]] UiFontSpec
LauncherFontSpec(
    UiStyle style,
    Language language,
    UiFontRole role) noexcept;

// Adjust a negative (em) GDI height away from pixel sizes where the face's
// TrueType hinting visibly shrinks individual Han glyphs.
[[nodiscard]] int
HintSafeFontHeight(
    const wchar_t* face,
    int height) noexcept;

// The GDI em height (negative) CreateFontHandle realizes for `spec` at `dpi`.
[[nodiscard]] int
FontPixelHeight(
    const UiFontSpec& spec,
    UINT dpi) noexcept;

[[nodiscard]] HFONT
CreateFontHandle(
    const UiFontSpec& spec,
    UINT dpi);

} // namespace altrun::ui
