#include "UiTypography.hpp"

namespace altrun::ui {

namespace {

constexpr int
    kClassicLauncherBodyPointSize = 10;
constexpr int
    kModernLauncherBodyPointSize = 10;
constexpr int
    kLauncherTitlePointSize = 10;

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
        role == UiFontRole::LauncherTitle
            ? kLauncherTitlePointSize
            : modern
                ? kModernLauncherBodyPointSize
                : kClassicLauncherBodyPointSize;

    return {
        face,
        pointSize,
        RoleWeight(role),
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
        CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE,
        spec.face);
}

} // namespace altrun::ui
