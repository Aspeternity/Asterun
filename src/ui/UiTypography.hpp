#pragma once

#include "../core/Settings.hpp"

#include <windows.h>

namespace altrun::ui {

enum class UiFontRole {
    Body,
    BodySemibold,
    SectionTitle,
    PageTitle,
    AppTitle,
    LauncherTitle,
};

struct UiFontSpec {
    const wchar_t* face{};
    int pointSize{};
    int weight{};
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

[[nodiscard]] HFONT
CreateFontHandle(
    const UiFontSpec& spec,
    UINT dpi);

} // namespace altrun::ui
