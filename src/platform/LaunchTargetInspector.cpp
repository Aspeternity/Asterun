#include "LaunchTargetInspector.hpp"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <wrl/client.h>

#include <algorithm>
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

    [[nodiscard]] HRESULT Result()
        const noexcept {
        return result_;
    }

private:
    HRESULT result_{};
};

[[nodiscard]] ExecutableInspection
InspectExecutableDetailed(
    const std::filesystem::path& path) {

    ExecutableInspection result;

    std::error_code ec;
    result.fileExists =
        std::filesystem::is_regular_file(
            path,
            ec);

    std::ifstream input(
        path,
        std::ios::binary);

    if (!input) {
        result.stage =
            ExecutableInspectionStage::
                OpenFailed;
        return result;
    }

    IMAGE_DOS_HEADER dos{};
    input.read(
        reinterpret_cast<char*>(&dos),
        sizeof(dos));

    if (!input) {
        result.stage =
            ExecutableInspectionStage::
                DosHeaderReadFailed;
        return result;
    }

    if (dos.e_magic !=
        IMAGE_DOS_SIGNATURE) {
        result.stage =
            ExecutableInspectionStage::
                InvalidDosHeader;
        return result;
    }

    if (dos.e_lfanew <= 0) {
        result.stage =
            ExecutableInspectionStage::
                InvalidPeOffset;
        return result;
    }

    input.seekg(
        dos.e_lfanew,
        std::ios::beg);

    if (!input) {
        result.stage =
            ExecutableInspectionStage::
                SeekFailed;
        return result;
    }

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

    if (!input) {
        result.stage =
            ExecutableInspectionStage::
                NtHeaderReadFailed;
        return result;
    }

    if (signature !=
        IMAGE_NT_SIGNATURE) {
        result.stage =
            ExecutableInspectionStage::
                InvalidPeSignature;
        return result;
    }

    result.optionalMagic = magic;

    input.seekg(
        -static_cast<std::streamoff>(
            sizeof(magic)),
        std::ios::cur);

    if (!input) {
        result.stage =
            ExecutableInspectionStage::
                SeekFailed;
        return result;
    }

    WORD subsystem = 0;

    if (magic ==
        IMAGE_NT_OPTIONAL_HDR32_MAGIC) {
        IMAGE_OPTIONAL_HEADER32 header{};
        input.read(
            reinterpret_cast<char*>(
                &header),
            sizeof(header));

        if (!input) {
            result.stage =
                ExecutableInspectionStage::
                    OptionalHeaderReadFailed;
            return result;
        }

        subsystem =
            header.Subsystem;
    } else if (
        magic ==
        IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
        IMAGE_OPTIONAL_HEADER64 header{};
        input.read(
            reinterpret_cast<char*>(
                &header),
            sizeof(header));

        if (!input) {
            result.stage =
                ExecutableInspectionStage::
                    OptionalHeaderReadFailed;
            return result;
        }

        subsystem =
            header.Subsystem;
    } else {
        result.stage =
            ExecutableInspectionStage::
                UnsupportedOptionalMagic;
        return result;
    }

    result.subsystem = subsystem;

    if (subsystem ==
        IMAGE_SUBSYSTEM_WINDOWS_GUI) {
        result.legacyKind =
            LaunchTargetKind::
                GuiExecutable;
        result.currentKind =
            result.legacyKind;
        result.stage =
            ExecutableInspectionStage::
                GuiExecutable;
        return result;
    }

    if (subsystem ==
        IMAGE_SUBSYSTEM_WINDOWS_CUI) {
        result.legacyKind =
            LaunchTargetKind::
                ConsoleExecutable;
        result.currentKind =
            result.legacyKind;
        result.stage =
            ExecutableInspectionStage::
                ConsoleExecutable;
        return result;
    }

    result.stage =
        ExecutableInspectionStage::
            UnsupportedSubsystem;

    return result;
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

const char*
ExecutableInspectionStageName(
    ExecutableInspectionStage stage) noexcept {

    switch (stage) {
    case ExecutableInspectionStage::NotRun:
        return "not-run";
    case ExecutableInspectionStage::OpenFailed:
        return "open-failed";
    case ExecutableInspectionStage::DosHeaderReadFailed:
        return "dos-header-read-failed";
    case ExecutableInspectionStage::InvalidDosHeader:
        return "invalid-dos-header";
    case ExecutableInspectionStage::InvalidPeOffset:
        return "invalid-pe-offset";
    case ExecutableInspectionStage::SeekFailed:
        return "seek-failed";
    case ExecutableInspectionStage::NtHeaderReadFailed:
        return "nt-header-read-failed";
    case ExecutableInspectionStage::InvalidPeSignature:
        return "invalid-pe-signature";
    case ExecutableInspectionStage::UnsupportedOptionalMagic:
        return "unsupported-optional-magic";
    case ExecutableInspectionStage::OptionalHeaderReadFailed:
        return "optional-header-read-failed";
    case ExecutableInspectionStage::GuiExecutable:
        return "gui-executable";
    case ExecutableInspectionStage::ConsoleExecutable:
        return "console-executable";
    case ExecutableInspectionStage::UnsupportedSubsystem:
        return "unsupported-subsystem";
    }

    return "not-run";
}

const char*
ShellLinkInspectionStageName(
    ShellLinkInspectionStage stage) noexcept {

    switch (stage) {
    case ShellLinkInspectionStage::ComUnavailable:
        return "com-unavailable";
    case ShellLinkInspectionStage::CreateInstanceFailed:
        return "create-instance-failed";
    case ShellLinkInspectionStage::PersistInterfaceFailed:
        return "persist-interface-failed";
    case ShellLinkInspectionStage::LoadFailed:
        return "load-failed";
    case ShellLinkInspectionStage::TargetResolutionFailed:
        return "target-resolution-failed";
    case ShellLinkInspectionStage::Resolved:
        return "resolved";
    }

    return "target-resolution-failed";
}

LaunchTargetInspection
InspectLaunchTargetDetailed(
    std::wstring_view target) {

    LaunchTargetInspection result;
    result.target =
        std::wstring(target);

    result.inferredKind =
        InferTextTargetKind(target);

    if (result.inferredKind !=
        LaunchTargetKind::Unknown) {
        result.finalKind =
            result.inferredKind;
        return result;
    }

    const std::filesystem::path path(
        target);

    const std::wstring extension =
        LowerExtension(target);

    if (extension != L".exe") {
        return result;
    }

    result.executable =
        InspectExecutableDetailed(path);

    result.executable.currentKind =
        result.executable.legacyKind;

    if (result.executable.legacyKind ==
        LaunchTargetKind::Unknown) {
        DWORD binaryType = 0;

        if (GetBinaryTypeW(
                path.c_str(),
                &binaryType)) {
            result.executable
                .getBinaryTypeSucceeded =
                true;
            result.executable.binaryType =
                binaryType;
            result.executable.currentKind =
                LaunchTargetKind::
                    ExecutableUnknown;
            result.executable.fallbackUsed =
                true;
        } else if (
            result.executable.fileExists) {
            result.executable.currentKind =
                LaunchTargetKind::
                    ExecutableUnknown;
            result.executable.fallbackUsed =
                true;
        }
    }

    result.finalKind =
        result.executable.currentKind;

    return result;
}

LaunchTargetKind InspectLaunchTarget(
    std::wstring_view target) {

    return InspectLaunchTargetDetailed(
        target).finalKind;
}

ShellLinkInspection
InspectShellLinkDetailed(
    const std::filesystem::path& path) {

    ShellLinkInspection inspection;

    ComApartment apartment;

    if (!apartment.Available()) {
        inspection.stage =
            ShellLinkInspectionStage::
                ComUnavailable;
        inspection.nativeResult =
            static_cast<long>(
                apartment.Result());
        return inspection;
    }

    ComPtr<IShellLinkW> shellLink;

    const HRESULT createResult =
        CoCreateInstance(
            CLSID_ShellLink,
            nullptr,
            CLSCTX_INPROC_SERVER,
            IID_PPV_ARGS(
                &shellLink));

    if (FAILED(createResult) ||
        !shellLink) {
        inspection.stage =
            ShellLinkInspectionStage::
                CreateInstanceFailed;
        inspection.nativeResult =
            static_cast<long>(
                createResult);
        return inspection;
    }

    ComPtr<IPersistFile> persist;

    const HRESULT persistResult =
        shellLink.As(&persist);

    if (FAILED(persistResult) ||
        !persist) {
        inspection.stage =
            ShellLinkInspectionStage::
                PersistInterfaceFailed;
        inspection.nativeResult =
            static_cast<long>(
                persistResult);
        return inspection;
    }

    const HRESULT loadResult =
        persist->Load(
            path.c_str(),
            STGM_READ);

    if (FAILED(loadResult)) {
        inspection.stage =
            ShellLinkInspectionStage::
                LoadFailed;
        inspection.nativeResult =
            static_cast<long>(
                loadResult);
        return inspection;
    }

    std::vector<wchar_t> buffer(
        32768,
        L'\0');

    WIN32_FIND_DATAW findData{};

    std::wstring target;

    const HRESULT getPathResult =
        shellLink->GetPath(
            buffer.data(),
            static_cast<int>(
                buffer.size()),
            &findData,
            0);

    if (SUCCEEDED(getPathResult) &&
        buffer.front() != L'\0') {
        target.assign(
            buffer.data());
    }

    HRESULT idListResult = S_OK;

    if (target.empty()) {
        PIDLIST_ABSOLUTE pidl = nullptr;

        idListResult =
            shellLink->GetIDList(
                &pidl);

        if (SUCCEEDED(idListResult) &&
            pidl != nullptr) {

            PWSTR raw = nullptr;

            const HRESULT nameResult =
                SHGetNameFromIDList(
                    pidl,
                    SIGDN_DESKTOPABSOLUTEPARSING,
                    &raw);

            if (SUCCEEDED(nameResult) &&
                raw != nullptr) {
                target.assign(raw);
                CoTaskMemFree(raw);
            } else {
                idListResult =
                    nameResult;
            }

            CoTaskMemFree(pidl);
        }
    }

    if (target.empty()) {
        inspection.stage =
            ShellLinkInspectionStage::
                TargetResolutionFailed;
        inspection.nativeResult =
            static_cast<long>(
                FAILED(getPathResult)
                    ? getPathResult
                    : idListResult);
        return inspection;
    }

    ShortcutTarget shortcut;
    shortcut.target =
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
        shortcut.arguments.assign(
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
        shortcut.workingDirectory.assign(
            buffer.data());
    }

    inspection.targetInspection =
        InspectLaunchTargetDetailed(
            shortcut.target);

    shortcut.targetKind =
        inspection.targetInspection
            .finalKind;

    inspection.shortcut =
        std::move(shortcut);
    inspection.stage =
        ShellLinkInspectionStage::
            Resolved;
    inspection.nativeResult =
        static_cast<long>(
            getPathResult);

    return inspection;
}

std::optional<ShortcutTarget>
InspectShellLink(
    const std::filesystem::path& path) {

    auto inspection =
        InspectShellLinkDetailed(path);

    return std::move(
        inspection.shortcut);
}

} // namespace altrun::win
