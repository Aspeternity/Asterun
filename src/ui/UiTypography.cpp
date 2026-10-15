#include "UiTypography.hpp"

#include <initializer_list>
#include <string>

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

int CALLBACK FoundModernFont(
    const LOGFONTW*, const TEXTMETRICW*, DWORD, LPARAM context) {
    *reinterpret_cast<bool*>(context) = true;
    return 0;
}

[[nodiscard]] const wchar_t* ModernLauncherFace() noexcept {
    // Select once, including for English UI: result names can mix Latin and Han.
    // Enumeration owns no fonts and the temporary screen DC is always released.
    static const wchar_t* const face = []() -> const wchar_t* {
        HDC dc = GetDC(nullptr);
        if (!dc) return L"Segoe UI";
        const wchar_t* selected = L"Segoe UI";
        for (const wchar_t* candidate : {L"Microsoft YaHei UI", L"Microsoft YaHei"}) {
            LOGFONTW query{};
            query.lfCharSet = DEFAULT_CHARSET;
            lstrcpynW(query.lfFaceName, candidate, LF_FACESIZE);
            bool found = false;
            EnumFontFamiliesExW(dc, &query, FoundModernFont,
                reinterpret_cast<LPARAM>(&found), 0);
            if (found) {
                selected = candidate;
                break;
            }
        }
        ReleaseDC(nullptr, dc);
        return selected;
    }();
    return face;
}

[[nodiscard]] const wchar_t*
ApplicationFace(
    Language language) noexcept {

    if (language != Language::ZhCN) {
        return L"Segoe UI";
    }

    // The UI-specific face has different CJK outlines at compact sizes.
    // Prefer the screen-reading face when installed; do not allow GDI to
    // silently substitute an unavailable font. Resolve once per process.
    static const wchar_t* const face = []() -> const wchar_t* {
        HDC dc = GetDC(nullptr);
        if (!dc) return L"Microsoft YaHei UI";
        const wchar_t* selected = L"Segoe UI";
        for (const wchar_t* candidate :
             {L"Microsoft YaHei", L"Microsoft YaHei UI"}) {
            LOGFONTW query{};
            query.lfCharSet = DEFAULT_CHARSET;
            lstrcpynW(query.lfFaceName, candidate, LF_FACESIZE);
            bool found = false;
            EnumFontFamiliesExW(dc, &query, FoundModernFont,
                reinterpret_cast<LPARAM>(&found), 0);
            if (found) {
                selected = candidate;
                break;
            }
        }
        ReleaseDC(nullptr, dc);
        return selected;
    }();
    return face;
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
        // CJK-capable HFONT avoids changing the control's font as mixed
        // Latin/CJK text appears; Windows font linking remains the fallback.
        return {
            ModernLauncherFace(),
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
        ModernLauncherFace(),
        pointSize,
        0,
        role == UiFontRole::BodySemibold ? FW_NORMAL : RoleWeight(role),
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


// TEMPORARY UI-R02 FONT DIAGNOSIS. REMOVE THIS FUNCTION AND ALL CALLS
// BEFORE ANY READY/MERGE GATE. The log contains font/system metrics ONLY.
// Each numbered surface logs at most once per process to avoid paint I/O.
void TraceTemporaryR02Font(
    TemporaryR02FontSlot slot, HWND owner, HWND control,
    HDC paintDc) noexcept {

    constexpr const wchar_t* names[] = {
        L"settings.nav_control",
        L"settings.section_control",
        L"settings.nav_paint",
        L"manager.filter_control",
        L"manager.list_control",
        L"manager.action_paint",
        L"editor.keyword_label",
        L"editor.keyword_input",
        L"editor.advanced_paint",
    };
    constexpr unsigned slotCount =
        static_cast<unsigned>(TemporaryR02FontSlot::Count);
    static_assert(sizeof(names) / sizeof(names[0]) == slotCount);
    static volatile LONG reported[slotCount]{};

    const auto index = static_cast<unsigned>(slot);
    if (index >= slotCount ||
        InterlockedCompareExchange(&reported[index], 1, 0) != 0) {
        return;
    }

    // A paint HDC already holds the real selected font. WM_GETFONT is
    // queried only inside the owning process, never cross-process.
    const HFONT selected = paintDc
        ? static_cast<HFONT>(GetCurrentObject(paintDc, OBJ_FONT))
        : control
            ? reinterpret_cast<HFONT>(
                SendMessageW(control, WM_GETFONT, 0, 0))
            : nullptr;

    LOGFONTW request{};
    const bool hasRequest = selected &&
        GetObjectW(selected, sizeof(request), &request) ==
            sizeof(request);

    HDC dc = paintDc;
    HGDIOBJ previous = nullptr;
    bool acquired = false;
    if (!dc && owner && selected) {
        dc = GetDC(owner);
        if (dc) {
            previous = SelectObject(dc, selected);
            if (!previous || previous == HGDI_ERROR) {
                ReleaseDC(owner, dc);
                dc = nullptr;
            } else {
                acquired = true;
            }
        }
    }

    wchar_t controlClass[64]{};
    if (control) {
        GetClassNameW(control, controlClass,
            static_cast<int>(sizeof(controlClass) / sizeof(wchar_t)));
    }

    std::wstring line = L"slot=";
    line += names[index];
    line += L" window_dpi=";
    line += std::to_wstring(owner ? GetDpiForWindow(owner) : 0);
    line += L" class=";
    line += controlClass[0] ? controlClass : L"(none)";
    line += L" mode=";
    line += paintDc ? L"paint_dc" : L"WM_GETFONT";

    if (!hasRequest) {
        line += L" font=(unavailable)";
    } else {
        line += L" requested=";
        line += request.lfFaceName;
        line += L" lfHeight=";
        line += std::to_wstring(request.lfHeight);
        line += L" lfWeight=";
        line += std::to_wstring(request.lfWeight);
        line += L" lfQuality=";
        line += std::to_wstring(request.lfQuality);
        line += L" lfCharSet=";
        line += std::to_wstring(request.lfCharSet);
    }

    if (dc) {
        wchar_t actual[LF_FACESIZE]{};
        if (GetTextFaceW(dc, LF_FACESIZE, actual) > 0) {
            line += L" realized=";
            line += actual;
        }
        TEXTMETRICW tm{};
        if (GetTextMetricsW(dc, &tm)) {
            line += L" tmHeight=";
            line += std::to_wstring(tm.tmHeight);
            line += L" tmAscent=";
            line += std::to_wstring(tm.tmAscent);
            line += L" tmInternalLeading=";
            line += std::to_wstring(tm.tmInternalLeading);
            line += L" tmAveCharWidth=";
            line += std::to_wstring(tm.tmAveCharWidth);
        }
        line += L" dcDpiY=";
        line += std::to_wstring(GetDeviceCaps(dc, LOGPIXELSY));

        // Fixed, non-user-owned samples only. GGO_METRICS describes each
        // glyph's ink bounding box without recording application content.
        MAT2 transform{};
        transform.eM11.value = 1;
        transform.eM22.value = 1;
        constexpr wchar_t sample[] = {L'搜', L'索', L'新', L'词'};
        constexpr const wchar_t* sampleNames[] = {
            L"sou", L"suo", L"xin", L"ci",
        };
        for (unsigned i = 0; i < 4; ++i) {
            GLYPHMETRICS gm{};
            const DWORD result = GetGlyphOutlineW(dc,
                static_cast<UINT>(sample[i]), GGO_METRICS,
                &gm, 0, nullptr, &transform);
            line += L" glyph_";
            line += sampleNames[i];
            line += L"=";
            if (result == GDI_ERROR) {
                line += L"(unavailable)";
            } else {
                line += std::to_wstring(gm.gmBlackBoxX);
                line += L"x";
                line += std::to_wstring(gm.gmBlackBoxY);
                line += L"/advance";
                line += std::to_wstring(gm.gmCellIncX);
            }
        }
    }
    if (acquired) {
        SelectObject(dc, previous);
        ReleaseDC(owner, dc);
    }

    // A dedicated per-PID temporary UTF-8 file avoids user paths or
    // settings files and needs no changes to Asterun configuration.
    wchar_t temp[MAX_PATH + 1]{};
    const DWORD prefix = GetTempPathW(MAX_PATH, temp);
    if (!prefix || prefix >= MAX_PATH) return;
    std::wstring filename(temp, prefix);
    filename += L"Asterun-R02-font-probe-";
    filename += std::to_wstring(GetCurrentProcessId());
    filename += L".txt";

    const HANDLE file = CreateFileW(filename.c_str(),
        FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return;
    const bool newFile = GetLastError() != ERROR_ALREADY_EXISTS;
    if (newFile) {
        const unsigned char bom[]{0xEF, 0xBB, 0xBF};
        DWORD written{};
        WriteFile(file, bom, sizeof(bom), &written, nullptr);
    }

    line += L"\r\n";
    const int count = WideCharToMultiByte(CP_UTF8, 0,
        line.c_str(), static_cast<int>(line.size()),
        nullptr, 0, nullptr, nullptr);
    if (count > 0) {
        std::string utf8(static_cast<std::size_t>(count), '\0');
        if (WideCharToMultiByte(CP_UTF8, 0, line.c_str(),
                static_cast<int>(line.size()), utf8.data(), count,
                nullptr, nullptr) == count) {
            DWORD written{};
            WriteFile(file, utf8.data(), static_cast<DWORD>(count),
                &written, nullptr);
        }
    }
    CloseHandle(file);
}

} // namespace altrun::ui
