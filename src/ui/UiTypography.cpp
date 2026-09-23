#include "UiTypography.hpp"

namespace altrun::ui {

namespace {

constexpr int
    kClassicLauncherPrimaryPointSize = 12;
constexpr int
    kClassicLauncherAuxiliaryPointSize = 10;
constexpr int
    kModernLauncherBodyPointSize = 10;
constexpr int
    kModernLauncherTitlePointSize = 10;

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
        RoleWeight(role),
        CLEARTYPE_QUALITY,
    };
}

UiFontSpec LauncherFontSpec(
    UiStyle style,
    Language language,
    UiFontRole role) noexcept {

    const bool modern =
        style == UiStyle::ModernCompact;

    const wchar_t* face =
        modern
            ? ApplicationFace(language)
            : language == Language::ZhCN
                ? L"SimSun"
                : L"Tahoma";

    const int pointSize =
        modern
            ? role == UiFontRole::LauncherTitle
                ? kModernLauncherTitlePointSize
                : kModernLauncherBodyPointSize
            : role == UiFontRole::LauncherAuxiliary
                ? kClassicLauncherAuxiliaryPointSize
                : kClassicLauncherPrimaryPointSize;

    const int weight =
        !modern &&
        role == UiFontRole::LauncherTitle
            ? FW_NORMAL
            : RoleWeight(role);

    const DWORD quality =
        modern
            ? CLEARTYPE_QUALITY
            : DEFAULT_QUALITY;

    return {
        face,
        pointSize,
        weight,
        quality,
    };
}

HFONT CreateFontHandle(
    const UiFontSpec& spec,
    UINT dpi) {

    if (dpi == 0) {
        dpi = 96;
    }

    return CreateFontW(
        -MulDiv(
            spec.pointSize,
            static_cast<int>(dpi),
            72),
        0,
        0,
        0,
        spec.weight,
        FALSE,
        FALSE,
        FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        spec.quality,
        DEFAULT_PITCH | FF_DONTCARE,
        spec.face);
}

} // namespace altrun::ui
