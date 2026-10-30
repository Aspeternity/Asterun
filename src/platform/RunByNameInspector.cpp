#include "RunByNameInspector.hpp"

#include "WinUtil.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <aclapi.h>
#include <objbase.h>
#include <sddl.h>
#include <shlobj.h>
#include <winioctl.h>
#include <wrl/client.h>
#include <xmllite.h>

#include <algorithm>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string_view>

#ifndef IO_REPARSE_TAG_APPEXECLINK
#define IO_REPARSE_TAG_APPEXECLINK (0x8000001BL)
#endif

namespace altrun::win {

namespace {

using Microsoft::WRL::ComPtr;

// NT SERVICE\TrustedInstaller is a virtual service account whose SID is
// derived from the service name and therefore identical on every machine.
constexpr wchar_t kTrustedInstallerSid[] =
    L"S-1-5-80-956008885-3418522649-1831038044-1853292631-2271478464";

class Handle final {
public:
    explicit Handle(HANDLE value) noexcept
        : value_(value) {}

    ~Handle() {
        if (valid()) {
            CloseHandle(value_);
        }
    }

    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;

    [[nodiscard]] bool valid() const noexcept {
        return value_ != nullptr &&
            value_ != INVALID_HANDLE_VALUE;
    }

    [[nodiscard]] HANDLE get() const noexcept {
        return value_;
    }

private:
    HANDLE value_;
};

std::wstring NormalizeRelative(
    std::wstring_view value) {

    std::wstring normalized =
        Lower(Trim(value));

    std::replace(
        normalized.begin(),
        normalized.end(),
        L'/',
        L'\\');

    while (!normalized.empty() &&
           normalized.front() == L'\\') {
        normalized.erase(
            normalized.begin());
    }

    return normalized;
}

ComPtr<IStream> FileStream(
    const std::filesystem::path& path) {

    std::ifstream input(
        path,
        std::ios::binary);

    if (!input) {
        return {};
    }

    const std::string bytes(
        (std::istreambuf_iterator<char>(input)),
        std::istreambuf_iterator<char>());

    if (bytes.empty()) {
        return {};
    }

    HGLOBAL memory =
        GlobalAlloc(
            GMEM_MOVEABLE,
            bytes.size());

    if (memory == nullptr) {
        return {};
    }

    void* data = GlobalLock(memory);

    if (data == nullptr) {
        GlobalFree(memory);
        return {};
    }

    std::memcpy(
        data,
        bytes.data(),
        bytes.size());
    GlobalUnlock(memory);

    ComPtr<IStream> stream;

    if (FAILED(CreateStreamOnHGlobal(
            memory,
            TRUE,
            &stream))) {
        GlobalFree(memory);
        return {};
    }

    return stream;
}

} // namespace

std::filesystem::path
AppExecutionAliasDirectory() {

    PWSTR path = nullptr;

    if (FAILED(SHGetKnownFolderPath(
            FOLDERID_LocalAppData,
            KF_FLAG_DEFAULT,
            nullptr,
            &path)) ||
        path == nullptr) {
        return {};
    }

    std::filesystem::path result(path);
    CoTaskMemFree(path);

    return result /
        L"Microsoft" /
        L"WindowsApps";
}

std::optional<AppExecutionAliasLink>
ReadAppExecutionAlias(
    const std::filesystem::path& stub) {

    const Handle file(
        CreateFileW(
            stub.c_str(),
            FILE_READ_ATTRIBUTES,
            FILE_SHARE_READ |
                FILE_SHARE_WRITE |
                FILE_SHARE_DELETE,
            nullptr,
            OPEN_EXISTING,
            FILE_FLAG_OPEN_REPARSE_POINT |
                FILE_FLAG_BACKUP_SEMANTICS,
            nullptr));

    if (!file.valid()) {
        return std::nullopt;
    }

    std::vector<BYTE> buffer(
        MAXIMUM_REPARSE_DATA_BUFFER_SIZE);
    DWORD bytes = 0;

    if (!DeviceIoControl(
            file.get(),
            FSCTL_GET_REPARSE_POINT,
            nullptr,
            0,
            buffer.data(),
            static_cast<DWORD>(
                buffer.size()),
            &bytes,
            nullptr)) {
        return std::nullopt;
    }

    // REPARSE_DATA_BUFFER header (tag, data length, reserved), then an
    // AppExecLink body: a version ULONG followed by NUL-terminated strings
    // (package family, AUMID, target executable, application type).
    constexpr std::size_t kHeader = 8;
    constexpr std::size_t kVersion = 4;

    if (bytes < kHeader + kVersion) {
        return std::nullopt;
    }

    ULONG tag = 0;
    USHORT dataLength = 0;
    std::memcpy(&tag, buffer.data(), sizeof(tag));
    std::memcpy(&dataLength, buffer.data() + 4, sizeof(dataLength));

    if (tag != IO_REPARSE_TAG_APPEXECLINK ||
        kHeader + dataLength > bytes ||
        dataLength < kVersion) {
        return std::nullopt;
    }

    const std::size_t textBytes =
        dataLength - kVersion;
    std::wstring text(
        textBytes / sizeof(wchar_t),
        L'\0');
    std::memcpy(
        text.data(),
        buffer.data() + kHeader + kVersion,
        text.size() * sizeof(wchar_t));

    std::vector<std::wstring> fields;
    std::size_t start = 0;

    while (start < text.size() &&
           fields.size() < 3) {
        std::size_t end =
            text.find(L'\0', start);

        if (end == std::wstring::npos) {
            end = text.size();
        }

        fields.emplace_back(
            text.substr(start, end - start));
        start = end + 1;
    }

    if (fields.size() < 3 ||
        fields[0].empty() ||
        fields[1].empty() ||
        fields[2].empty()) {
        return std::nullopt;
    }

    return AppExecutionAliasLink{
        std::move(fields[0]),
        std::move(fields[1]),
        std::move(fields[2]),
    };
}

std::vector<std::wstring>
PackageServerExecutables(
    const std::filesystem::path& packageRoot) {

    std::vector<std::wstring> executables;

    const ComPtr<IStream> stream =
        FileStream(
            packageRoot /
            L"AppxManifest.xml");

    if (!stream) {
        return executables;
    }

    ComPtr<IXmlReader> reader;

    if (FAILED(CreateXmlReader(
            IID_PPV_ARGS(&reader),
            nullptr)) ||
        FAILED(reader->SetProperty(
            XmlReaderProperty_DtdProcessing,
            DtdProcessing_Prohibit)) ||
        FAILED(reader->SetInput(
            stream.Get()))) {
        return executables;
    }

    XmlNodeType type{};

    while (reader->Read(&type) == S_OK) {
        if (type != XmlNodeType_Element) {
            continue;
        }

        const wchar_t* element = nullptr;

        if (FAILED(reader->GetLocalName(
                &element,
                nullptr)) ||
            element == nullptr) {
            continue;
        }

        const std::wstring_view name(element);
        const bool exeServer =
            name == L"ExeServer";

        if (!exeServer &&
            name != L"Extension") {
            continue;
        }

        std::wstring executable;
        std::wstring category;

        for (HRESULT attribute =
                 reader->MoveToFirstAttribute();
             attribute == S_OK;
             attribute =
                 reader->MoveToNextAttribute()) {
            const wchar_t* attributeName = nullptr;
            const wchar_t* value = nullptr;

            if (FAILED(reader->GetLocalName(
                    &attributeName,
                    nullptr)) ||
                FAILED(reader->GetValue(
                    &value,
                    nullptr)) ||
                attributeName == nullptr ||
                value == nullptr) {
                continue;
            }

            const std::wstring_view key(
                attributeName);

            if (key == L"Executable") {
                executable = value;
            } else if (key == L"Category") {
                category = value;
            }
        }

        if (executable.empty() ||
            (!exeServer &&
             category != L"windows.service")) {
            continue;
        }

        executables.push_back(
            NormalizeRelative(
                executable));
    }

    return executables;
}

bool IsOwnedByTrustedInstaller(
    const std::filesystem::path& path) {

    PSID owner = nullptr;
    PSECURITY_DESCRIPTOR descriptor = nullptr;

    if (GetNamedSecurityInfoW(
            path.c_str(),
            SE_FILE_OBJECT,
            OWNER_SECURITY_INFORMATION,
            &owner,
            nullptr,
            nullptr,
            nullptr,
            &descriptor) != ERROR_SUCCESS) {
        return false;
    }

    PSID trustedInstaller = nullptr;
    bool owned = false;

    if (owner != nullptr &&
        ConvertStringSidToSidW(
            kTrustedInstallerSid,
            &trustedInstaller)) {
        owned =
            EqualSid(
                owner,
                trustedInstaller) != FALSE;
        LocalFree(trustedInstaller);
    }

    LocalFree(descriptor);
    return owned;
}

} // namespace altrun::win
