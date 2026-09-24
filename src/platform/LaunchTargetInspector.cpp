#include "LaunchTargetInspector.hpp"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <wrl/client.h>

#include <algorithm>
#include <array>
#include <cwctype>
#include <fstream>
#include <string>
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
        const auto inspected =
            InspectExecutable(path);

        if (inspected !=
            LaunchTargetKind::Unknown) {
            return inspected;
        }

        DWORD binaryType = 0;

        if (GetBinaryTypeW(
                path.c_str(),
                &binaryType)) {
            return LaunchTargetKind::
                ExecutableUnknown;
        }

        std::error_code ec;

        if (std::filesystem::
                is_regular_file(
                    path,
                    ec)) {
            return LaunchTargetKind::
                ExecutableUnknown;
        }
    }

    return LaunchTargetKind::Unknown;
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

    if (target.empty()) {
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
                target.assign(raw);
                CoTaskMemFree(raw);
            }

            CoTaskMemFree(pidl);
        }
    }

    if (target.empty()) {
        return std::nullopt;
    }

    ShortcutTarget result;
    result.target =
        std::move(target);

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

} // namespace altrun::win
