#include "UiTypography.hpp"

namespace altrun::ui {

namespace {

constexpr int
    kClassicLauncherPrimaryLogicalHeight96 = -16;
constexpr int
    kClassicLauncherAuxiliaryLogicalHeight96 = -13;
constexpr int
    kModernLauncherBodyPointSize = 10;
constexpr int
    kModernLauncherAuxiliaryPointSize = 9;
constexpr int
    kModernLauncherTitlePointSize = 10;
constexpr int
    kModernLauncherSearchLogicalHeight96 = -14;

[[nodiscard]] const wchar_t*
ApplicationFace(
    Language language) noexcept {

    return language == Language::ZhCN
        ? L"Microsoft YaHei UI"
        : L"Segoe UI";
}

[[nodiscard]] int
ApplicationPointSize(
    UiFontRole role) noexcept {

    switch (role) {
    case UiFontRole::SectionTitle:
        return 11;
    case UiFontRole::PageTitle:
        return 18;
    case UiFontRole::AppTitle:
        return 22;
    case UiFontRole::LauncherTitle:
        return 10;
    case UiFontRole::LauncherSearch:
        return 11;
    case UiFontRole::Body:
    case UiFontRole::BodySemibold:
    case UiFontRole::LauncherAuxiliary:
    default:
        return 10;
    }
}

[[nodiscard]] int
RoleWeight(
    UiFontRole role) noexcept {

    switch (role) {
    case UiFontRole::BodySemibold:
    case UiFontRole::SectionTitle:
    case UiFontRole::PageTitle:
    case UiFontRole::AppTitle:
        return FW_SEMIBOLD;
    case UiFontRole::LauncherTitle:
        return FW_BOLD;
    case UiFontRole::Body:
    case UiFontRole::LauncherAuxiliary:
    default:
        return FW_NORMAL;
    }
}

} // namespace

UiFontSpec ApplicationFontSpec(
    Language language,
    UiFontRole role) noexcept {

    return {
        ApplicationFace(language),
        ApplicationPointSize(role),
        0,
        RoleWeight(role),
        DEFAULT_CHARSET,
        CLEARTYPE_QUALITY,
    };
}

UiFontSpec LauncherFontSpec(
    UiStyle style,
    Language language,
    UiFontRole role) noexcept {

    const bool modern =
        style == UiStyle::ModernCompact;

    if (!modern) {
        return {
            L"SimSun",
            0,
            role == UiFontRole::LauncherAuxiliary
                ? kClassicLauncherAuxiliaryLogicalHeight96
                : kClassicLauncherPrimaryLogicalHeight96,
            FW_NORMAL,
            ANSI_CHARSET,
            DEFAULT_QUALITY,
        };
    }

    if (role == UiFontRole::LauncherSearch) {
        // Use one DPI-scaled logical height for the native EDIT. Keeping one
        // Segoe UI HFONT lets Windows font linking handle Han glyphs without
        // changing the whole control's font when mixed Latin/CJK text appears.
        return {
            L"Segoe UI",
            0,
            kModernLauncherSearchLogicalHeight96,
            FW_NORMAL,
            DEFAULT_CHARSET,
            CLEARTYPE_NATURAL_QUALITY,
        };
    }

    const int pointSize =
        role == UiFontRole::LauncherTitle
            ? kModernLauncherTitlePointSize
            : role == UiFontRole::LauncherAuxiliary
                ? kModernLauncherAuxiliaryPointSize
                : kModernLauncherBodyPointSize;

    return {
        L"Segoe UI",
        pointSize,
        0,
        RoleWeight(role),
        DEFAULT_CHARSET,
        CLEARTYPE_NATURAL_QUALITY,
    };
}

HFONT CreateFontHandle(
    const UiFontSpec& spec,
    UINT dpi) {

    if (dpi == 0) {
        dpi = 96;
    }

    const int height =
        spec.logicalHeight96 != 0
            ? MulDiv(
                  spec.logicalHeight96,
                  static_cast<int>(dpi),
                  96)
            : -MulDiv(
                  spec.pointSize,
                  static_cast<int>(dpi),
                  72);

    return CreateFontW(
        height,
        0,
        0,
        0,
        spec.weight,
        FALSE,
        FALSE,
        FALSE,
        spec.charset,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        spec.quality,
        DEFAULT_PITCH | FF_DONTCARE,
        spec.face);
}

} // namespace altrun::ui
