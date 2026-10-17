#include "UiTypography.hpp"

#include <initializer_list>

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

int CALLBACK MarkInstalledFont(
    const LOGFONTW*, const TEXTMETRICW*, DWORD, LPARAM context) {
    *reinterpret_cast<bool*>(context) = true;
    return 0;
}

// Resolve both application and UI display-face priorities with one GDI
// enumeration path. Keep their existing preference order and no-DC fallback
// intact; this consolidates the old unsuccessful font-family experiment
// without changing the fonts realized on a normal Windows installation.
[[nodiscard]] const wchar_t* ResolveInstalledFace(
    std::initializer_list<const wchar_t*> priorities,
    const wchar_t* unavailableDcFallback,
    const wchar_t* noMatchFallback) noexcept {
    HDC dc = GetDC(nullptr);
    if (!dc) return unavailableDcFallback;

    const wchar_t* selected = noMatchFallback;
    for (const wchar_t* candidate : priorities) {
        LOGFONTW query{};
        query.lfCharSet = DEFAULT_CHARSET;
        lstrcpynW(query.lfFaceName, candidate, LF_FACESIZE);
        bool found = false;
        EnumFontFamiliesExW(dc, &query, MarkInstalledFont,
            reinterpret_cast<LPARAM>(&found), 0);
        if (found) {
            selected = candidate;
            break;
        }
    }
    ReleaseDC(nullptr, dc);
    return selected;
}

[[nodiscard]] const wchar_t* PreferredChineseUiFace() noexcept {
    // Mixed Latin and Han in Modern search, plus Settings navigation.
    // Cached once: no font probing in paint/activation paths.
    static const wchar_t* const face = ResolveInstalledFace(
        {L"Microsoft YaHei UI", L"Microsoft YaHei"},
        L"Segoe UI", L"Segoe UI");
    return face;
}

[[nodiscard]] const wchar_t* ApplicationFace(
    Language language) noexcept {
    if (language != Language::ZhCN) return L"Segoe UI";

    // Preserve the accepted Settings right-pane family separately from
    // compact Shortcut form/UI typography and navigation.
    static const wchar_t* const face = ResolveInstalledFace(
        {L"Microsoft YaHei", L"Microsoft YaHei UI"},
        L"Microsoft YaHei UI", L"Segoe UI");
    return face;
}

[[nodiscard]] int
ApplicationPointSize(
    Language language,
    UiFontRole role) noexcept {

    switch (role) {
    case UiFontRole::SettingsNavigation:
        return language == Language::ZhCN ? 11 : 10;
    case UiFontRole::ShortcutHint:
        return 9;
    case UiFontRole::ShortcutBody:
        // Only shortcut editor/manager use this compact CJK UI face.
        return 10;
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
    case UiFontRole::SettingsNavigation:
    case UiFontRole::ShortcutBody:
    case UiFontRole::ShortcutHint:
    case UiFontRole::LauncherAuxiliary:
    default:
        return FW_NORMAL;
    }
}

[[nodiscard]] bool IsYaHeiFace(const wchar_t* face) noexcept {
    return face &&
        (lstrcmpiW(face, L"Microsoft YaHei") == 0 ||
         lstrcmpiW(face, L"Microsoft YaHei UI") == 0);
}

} // namespace

int HintSafeFontHeight(
    const wchar_t* face,
    int height) noexcept {

    if (height >= 0 || !IsYaHeiFace(face)) {
        return height;
    }

    // Under GDI hinting, YaHei renders e.g. 索/级/号/存 2-3px shorter than
    // their neighbours at these em sizes (20px is 10pt at 150% DPI). Move
    // to the nearest size that keeps glyph heights uniform.
    switch (-height) {
    case 16:
        return -15;
    case 20:
        return -19;
    case 21:
        return -22;
    default:
        return height;
    }
}

UiFontSpec ApplicationFontSpec(
    Language language,
    UiFontRole role) noexcept {

    return {
        (role == UiFontRole::SettingsNavigation ||
         role == UiFontRole::ShortcutBody ||
         role == UiFontRole::ShortcutHint) &&
                language == Language::ZhCN
            ? PreferredChineseUiFace()
            : ApplicationFace(language),
        ApplicationPointSize(language, role),
        0,
        RoleWeight(role),
        DEFAULT_CHARSET,
        CLEARTYPE_QUALITY,
        role == UiFontRole::SettingsNavigation,
    };
}

// The launcher uses the same CJK-capable faces for every UI language, so the
// language parameter is accepted for symmetry with ApplicationFontSpec but unused.
UiFontSpec LauncherFontSpec(
    UiStyle style,
    Language,
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
        // CJK-capable HFONT avoids changing the control's font as mixed
        // Latin/CJK text appears; Windows font linking remains the fallback.
        return {
            PreferredChineseUiFace(),
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
        PreferredChineseUiFace(),
        pointSize,
        0,
        role == UiFontRole::BodySemibold ? FW_NORMAL : RoleWeight(role),
        DEFAULT_CHARSET,
        CLEARTYPE_NATURAL_QUALITY,
    };
}

int FontPixelHeight(
    const UiFontSpec& spec,
    UINT dpi) noexcept {

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

    // YaHei grid-fits stems only up to a 21px em; from 22px GDI merely
    // smooths outlines, so horizontal strokes straddle two pixel rows and
    // read as blurry (e.g. 11pt navigation at 150%). The glyphs that 21px
    // shrinks (级/号/存/置) are not among the fixed labels.
    constexpr int kYaHeiLargestGridFittedHeight = 21;

    if (spec.fixedLabels &&
        height == -(kYaHeiLargestGridFittedHeight + 1) &&
        IsYaHeiFace(spec.face)) {
        return -kYaHeiLargestGridFittedHeight;
    }

    return HintSafeFontHeight(
        spec.face,
        height);
}

HFONT CreateFontHandle(
    const UiFontSpec& spec,
    UINT dpi) {

    const int height =
        FontPixelHeight(
            spec,
            dpi);

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
