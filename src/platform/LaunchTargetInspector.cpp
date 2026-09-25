#include "LaunchTargetInspector.hpp"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shlobj.h>
#include <winver.h>
#include <shobjidl.h>
#include <wrl/client.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cwchar>
#include <cwctype>
#include <fstream>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace altrun::win {
namespace {

using Microsoft::WRL::ComPtr;

class ComApartment final {
public:
    ComApartment()
        : result_(
              CoInitializeEx(
                  nullptr,
                  COINIT_APARTMENTTHREADED)) {}

    ~ComApartment() {
        if (SUCCEEDED(result_)) {
            CoUninitialize();
        }
    }

    [[nodiscard]] bool Available()
        const noexcept {
        return
            SUCCEEDED(result_) ||
            result_ ==
                RPC_E_CHANGED_MODE;
    }

private:
    HRESULT result_{};
};

[[nodiscard]] LaunchTargetKind
InspectExecutable(
    const std::filesystem::path& path) {

    std::ifstream input(
        path,
        std::ios::binary);

    if (!input) {
        return LaunchTargetKind::Unknown;
    }

    IMAGE_DOS_HEADER dos{};
    input.read(
        reinterpret_cast<char*>(&dos),
        sizeof(dos));

    if (!input ||
        dos.e_magic !=
            IMAGE_DOS_SIGNATURE ||
        dos.e_lfanew <= 0) {
        return LaunchTargetKind::Unknown;
    }

    input.seekg(
        dos.e_lfanew,
        std::ios::beg);

    DWORD signature = 0;
    IMAGE_FILE_HEADER fileHeader{};
    WORD magic = 0;

    input.read(
        reinterpret_cast<char*>(
            &signature),
        sizeof(signature));

    input.read(
        reinterpret_cast<char*>(
            &fileHeader),
        sizeof(fileHeader));

    input.read(
        reinterpret_cast<char*>(
            &magic),
        sizeof(magic));

    if (!input ||
        signature !=
            IMAGE_NT_SIGNATURE) {
        return LaunchTargetKind::Unknown;
    }

    input.seekg(
        -static_cast<std::streamoff>(
            sizeof(magic)),
        std::ios::cur);

    WORD subsystem = 0;

    if (magic ==
        IMAGE_NT_OPTIONAL_HDR32_MAGIC) {
        IMAGE_OPTIONAL_HEADER32 header{};
        input.read(
            reinterpret_cast<char*>(
                &header),
            sizeof(header));
        if (input) {
            subsystem =
                header.Subsystem;
        }
    } else if (
        magic ==
        IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
        IMAGE_OPTIONAL_HEADER64 header{};
        input.read(
            reinterpret_cast<char*>(
                &header),
            sizeof(header));
        if (input) {
            subsystem =
                header.Subsystem;
        }
    } else {
        return LaunchTargetKind::Unknown;
    }

    if (subsystem ==
        IMAGE_SUBSYSTEM_WINDOWS_GUI) {
        return LaunchTargetKind::
            GuiExecutable;
    }

    if (subsystem ==
        IMAGE_SUBSYSTEM_WINDOWS_CUI) {
        return LaunchTargetKind::
            ConsoleExecutable;
    }

    return LaunchTargetKind::Unknown;
}

[[nodiscard]] std::wstring
LowerExtension(
    std::wstring_view target) {

    std::filesystem::path path(target);
    std::wstring extension =
        path.extension().wstring();

    std::transform(
        extension.begin(),
        extension.end(),
        extension.begin(),
        [](wchar_t ch) {
            return static_cast<wchar_t>(
                std::towlower(ch));
        });

    return extension;
}

[[nodiscard]] std::wstring
MetadataCacheKey(
    const std::filesystem::path& path) {

    std::wstring key =
        path.lexically_normal().wstring();

    std::transform(
        key.begin(),
        key.end(),
        key.begin(),
        [](wchar_t ch) {
            return ch == L'/'
                ? L'\\'
                : static_cast<wchar_t>(
                      std::towlower(ch));
        });

    return key;
}

[[nodiscard]] std::wstring
LowerTrimmed(
    std::wstring_view value) {

    std::size_t first = 0;
    std::size_t last = value.size();

    while (first < last &&
           std::iswspace(value[first])) {
        ++first;
    }

    while (last > first &&
           std::iswspace(value[last - 1])) {
        --last;
    }

    std::wstring result(
        value.substr(
            first,
            last - first));

    std::transform(
        result.begin(),
        result.end(),
        result.begin(),
        [](wchar_t ch) {
            return static_cast<wchar_t>(
                std::towlower(ch));
        });

    return result;
}

[[nodiscard]] bool
IsShellNamespaceActivation(
    std::wstring_view value) {

    const std::wstring lower =
        LowerTrimmed(value);

    return
        lower.find(L"shell:::{") !=
            std::wstring::npos ||
        lower.find(L"::{") !=
            std::wstring::npos ||
        lower.find(
            L"shell:controlpanelfolder") !=
            std::wstring::npos;
}

[[nodiscard]] std::wstring
WindowsDirectoryKey() {

    std::vector<wchar_t> buffer(
        32768,
        L'\0');

    const UINT length =
        GetWindowsDirectoryW(
            buffer.data(),
            static_cast<UINT>(
                buffer.size()));

    if (length == 0 ||
        length >= buffer.size()) {
        return {};
    }

    return MetadataCacheKey(
        std::filesystem::path(
            std::wstring(
                buffer.data(),
                length)));
}

[[nodiscard]] bool
IsTrustedWindowsExecutable(
    std::wstring_view target,
    std::wstring_view fileName) {

    if (target.empty()) {
        return false;
    }

    const std::filesystem::path
        path(target);

    std::wstring leaf =
        path.filename().wstring();

    std::transform(
        leaf.begin(),
        leaf.end(),
        leaf.begin(),
        [](wchar_t ch) {
            return static_cast<wchar_t>(
                std::towlower(ch));
        });

    std::wstring expected(fileName);

    std::transform(
        expected.begin(),
        expected.end(),
        expected.begin(),
        [](wchar_t ch) {
            return static_cast<wchar_t>(
                std::towlower(ch));
        });

    if (leaf != expected) {
        return false;
    }

    static const std::wstring
        windowsDirectory =
            WindowsDirectoryKey();

    if (windowsDirectory.empty()) {
        return false;
    }

    const std::wstring key =
        MetadataCacheKey(path);

    const std::wstring windowsRoot =
        windowsDirectory +
        L"\\" +
        expected;

    const std::wstring system32 =
        windowsDirectory +
        L"\\system32\\" +
        expected;

    const std::wstring syswow64 =
        windowsDirectory +
        L"\\syswow64\\" +
        expected;

    return
        key == windowsRoot ||
        key == system32 ||
        key == syswow64;
}

struct MetadataCacheEntry {
    std::uintmax_t fileSize{0};
    std::filesystem::file_time_type
        writeTime{};
    ExecutableMetadata metadata;
};

std::mutex gMetadataCacheMutex;
std::unordered_map<
    std::wstring,
    MetadataCacheEntry>
    gMetadataCache;

struct LangAndCodePage {
    WORD language;
    WORD codePage;
};

[[nodiscard]] std::wstring
VersionString(
    const std::vector<std::byte>& data,
    WORD language,
    WORD codePage,
    std::wstring_view key) {

    wchar_t path[128]{};

    if (swprintf_s(
            path,
            128,
            L"\\StringFileInfo\\%04x%04x\\%.*s",
            language,
            codePage,
            static_cast<int>(
                key.size()),
            key.data()) < 0) {
        return {};
    }

    void* raw = nullptr;
    UINT length = 0;

    if (!VerQueryValueW(
            data.data(),
            path,
            &raw,
            &length) ||
        raw == nullptr ||
        length == 0) {
        return {};
    }

    const auto* text =
        static_cast<const wchar_t*>(
            raw);

    std::wstring value(
        text,
        text + length);

    while (!value.empty() &&
           value.back() == L'\0') {
        value.pop_back();
    }

    return value;
}

[[nodiscard]] ExecutableMetadata
ReadExecutableMetadata(
    const std::filesystem::path& path) {

    ExecutableMetadata metadata;

    DWORD ignored = 0;
    const DWORD size =
        GetFileVersionInfoSizeW(
            path.c_str(),
            &ignored);

    if (size == 0) {
        return metadata;
    }

    std::vector<std::byte> data(
        size);

    if (!GetFileVersionInfoW(
            path.c_str(),
            0,
            size,
            data.data())) {
        return metadata;
    }

    std::vector<LangAndCodePage>
        translations;

    void* rawTranslations = nullptr;
    UINT translationBytes = 0;

    if (VerQueryValueW(
            data.data(),
            L"\\VarFileInfo\\Translation",
            &rawTranslations,
            &translationBytes) &&
        rawTranslations != nullptr &&
        translationBytes >=
            sizeof(LangAndCodePage)) {

        const auto count =
            translationBytes /
            sizeof(LangAndCodePage);

        const auto* values =
            static_cast<
                const LangAndCodePage*>(
                    rawTranslations);

        translations.assign(
            values,
            values + count);
    }

    if (translations.empty()) {
        translations.push_back({
            0x0409,
            0x04B0,
        });
        translations.push_back({
            0x0409,
            0x04E4,
        });
    }

    const auto query =
        [&](std::wstring_view key) {
            for (const auto& translation :
                 translations) {
                std::wstring value =
                    VersionString(
                        data,
                        translation.language,
                        translation.codePage,
                        key);

                if (!value.empty()) {
                    return value;
                }
            }

            return std::wstring{};
        };

    metadata.fileDescription =
        query(L"FileDescription");
    metadata.productName =
        query(L"ProductName");
    metadata.companyName =
        query(L"CompanyName");
    metadata.originalFilename =
        query(L"OriginalFilename");
    metadata.internalName =
        query(L"InternalName");

    return metadata;
}

} // namespace

LaunchTargetKind InspectLaunchTarget(
    std::wstring_view target) {

    const auto inferred =
        InferTextTargetKind(target);

    if (inferred !=
        LaunchTargetKind::Unknown) {
        return inferred;
    }

    const std::filesystem::path path(
        target);

    const std::wstring extension =
        LowerExtension(target);

    if (extension == L".exe") {
        return InspectExecutable(path);
    }

    return LaunchTargetKind::Unknown;
}

ExecutableMetadata
InspectExecutableMetadata(
    std::wstring_view target) {

    const std::filesystem::path path(
        target);

    if (LowerExtension(target) !=
        L".exe") {
        return {};
    }

    std::error_code ec;

    if (!std::filesystem::
            is_regular_file(
                path,
                ec)) {
        return {};
    }

    const auto size =
        std::filesystem::file_size(
            path,
            ec);

    if (ec) {
        return {};
    }

    const auto writeTime =
        std::filesystem::last_write_time(
            path,
            ec);

    if (ec) {
        return {};
    }

    const std::wstring key =
        MetadataCacheKey(path);

    {
        std::scoped_lock lock(
            gMetadataCacheMutex);

        const auto it =
            gMetadataCache.find(key);

        if (it !=
                gMetadataCache.end() &&
            it->second.fileSize ==
                size &&
            it->second.writeTime ==
                writeTime) {
            return it->second.metadata;
        }
    }

    ExecutableMetadata metadata =
        ReadExecutableMetadata(path);

    {
        std::scoped_lock lock(
            gMetadataCacheMutex);

        gMetadataCache[key] = {
            size,
            writeTime,
            metadata,
        };
    }

    return metadata;
}

std::optional<ShortcutTarget>
InspectShellLink(
    const std::filesystem::path& path) {

    ComApartment apartment;

    if (!apartment.Available()) {
        return std::nullopt;
    }

    ComPtr<IShellLinkW> shellLink;

    if (FAILED(
            CoCreateInstance(
                CLSID_ShellLink,
                nullptr,
                CLSCTX_INPROC_SERVER,
                IID_PPV_ARGS(
                    &shellLink))) ||
        !shellLink) {
        return std::nullopt;
    }

    ComPtr<IPersistFile> persist;

    if (FAILED(
            shellLink.As(&persist)) ||
        !persist ||
        FAILED(
            persist->Load(
                path.c_str(),
                STGM_READ))) {
        return std::nullopt;
    }

    std::vector<wchar_t> buffer(
        32768,
        L'\0');

    WIN32_FIND_DATAW findData{};

    std::wstring target;

    if (SUCCEEDED(
            shellLink->GetPath(
                buffer.data(),
                static_cast<int>(
                    buffer.size()),
                &findData,
                0)) &&
        buffer.front() != L'\0') {
        target.assign(
            buffer.data());
    }

    // Preserve the Shell PIDL parsing identity even when GetPath() also
    // returns a broker executable. Windows Start Menu links can route through
    // control.exe / explorer.exe while the PIDL carries the actual namespace.
    std::wstring shellParsingName;
    PIDLIST_ABSOLUTE pidl = nullptr;

    if (SUCCEEDED(
            shellLink->GetIDList(
                &pidl)) &&
        pidl != nullptr) {

        PWSTR raw = nullptr;

        if (SUCCEEDED(
                SHGetNameFromIDList(
                    pidl,
                    SIGDN_DESKTOPABSOLUTEPARSING,
                    &raw)) &&
            raw != nullptr) {
            shellParsingName.assign(raw);
            CoTaskMemFree(raw);
        }

        CoTaskMemFree(pidl);
    }

    if (target.empty()) {
        target = shellParsingName;
    }

    if (target.empty()) {
        return std::nullopt;
    }

    ShortcutTarget result;
    result.target =
        std::move(target);
    result.shellParsingName =
        std::move(shellParsingName);

    std::fill(
        buffer.begin(),
        buffer.end(),
        L'\0');

    if (SUCCEEDED(
            shellLink->GetArguments(
                buffer.data(),
                static_cast<int>(
                    buffer.size()))) &&
        buffer.front() != L'\0') {
        result.arguments.assign(
            buffer.data());
    }

    std::fill(
        buffer.begin(),
        buffer.end(),
        L'\0');

    if (SUCCEEDED(
            shellLink->GetWorkingDirectory(
                buffer.data(),
                static_cast<int>(
                    buffer.size()))) &&
        buffer.front() != L'\0') {
        result.workingDirectory.assign(
            buffer.data());
    }

    result.targetKind =
        InspectLaunchTarget(
            result.target);

    return result;
}

std::optional<LaunchSurfaceClass>
ClassifyShellActivationSurface(
    std::wstring_view target,
    std::wstring_view arguments,
    std::wstring_view shellParsingName) {

    const std::wstring lowerTarget =
        LowerTrimmed(target);

    const std::wstring lowerArguments =
        LowerTrimmed(arguments);

    // URI-native Windows Settings links are shell-owned system surfaces.
    if (lowerTarget.starts_with(
            L"ms-settings:") ||
        lowerArguments.starts_with(
            L"ms-settings:")) {
        return LaunchSurfaceClass::
            SystemUtility;
    }

    // These are Windows-owned brokers, not product-name heuristics. Restrict
    // executable recognition to the Windows directory so an unrelated app
    // named control.exe/mmc.exe cannot inherit SystemUtility semantics.
    if (IsTrustedWindowsExecutable(
            target,
            L"control.exe") ||
        IsTrustedWindowsExecutable(
            target,
            L"mmc.exe")) {
        return LaunchSurfaceClass::
            SystemUtility;
    }

    if (IsTrustedWindowsExecutable(
            target,
            L"rundll32.exe") &&
        (lowerArguments.find(
             L"control_rundll") !=
             std::wstring::npos ||
         lowerArguments.find(
             L".cpl") !=
             std::wstring::npos)) {
        return LaunchSurfaceClass::
            SystemUtility;
    }

    // A direct namespace target has no ordinary filesystem application
    // identity. Explorer is treated similarly only when its arguments/PIDL
    // actually name a Shell namespace; normal Explorer folder launches stay
    // outside this rule.
    if (IsShellNamespaceActivation(
            target)) {
        return LaunchSurfaceClass::
            SystemUtility;
    }

    if (IsTrustedWindowsExecutable(
            target,
            L"explorer.exe") &&
        (IsShellNamespaceActivation(
             arguments) ||
         IsShellNamespaceActivation(
             shellParsingName))) {
        return LaunchSurfaceClass::
            SystemUtility;
    }

    return std::nullopt;
}

} // namespace altrun::win
