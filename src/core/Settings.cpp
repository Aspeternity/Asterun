#include "Settings.hpp"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <cwchar>

namespace altrun {

SettingsStore::SettingsStore(std::filesystem::path path)
    : path_(std::move(path)) {}

void SettingsStore::Load() {
    wchar_t ui[64]{};
    wchar_t language[64]{};

    GetPrivateProfileStringW(L"general", L"ui", L"classic", ui, static_cast<DWORD>(std::size(ui)), path_.c_str());
    GetPrivateProfileStringW(L"general", L"language", L"zh-CN", language, static_cast<DWORD>(std::size(language)), path_.c_str());

    settings_.uiStyle = (_wcsicmp(ui, L"modern") == 0 || _wcsicmp(ui, L"modern-compact") == 0)
        ? UiStyle::ModernCompact
        : UiStyle::Classic;

    settings_.language = (_wcsicmp(language, L"en-US") == 0 || _wcsicmp(language, L"en") == 0)
        ? Language::EnUS
        : Language::ZhCN;
}

void SettingsStore::Save() const {
    WritePrivateProfileStringW(
        L"general",
        L"ui",
        settings_.uiStyle == UiStyle::ModernCompact ? L"modern-compact" : L"classic",
        path_.c_str());

    WritePrivateProfileStringW(
        L"general",
        L"language",
        settings_.language == Language::EnUS ? L"en-US" : L"zh-CN",
        path_.c_str());
}

void SettingsStore::SetUiStyle(UiStyle style) {
    settings_.uiStyle = style;
    Save();
}

void SettingsStore::SetLanguage(Language language) {
    settings_.language = language;
    Save();
}

} // namespace altrun
