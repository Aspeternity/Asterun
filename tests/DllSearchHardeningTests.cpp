// Behaviour of RestrictDefaultDllSearchToSystem32 (Update.exe, Uninstall.exe
// and Asterun.exe's managed Everything service commands call it first).
//
// A copy of a System32 DLL placed beside this test executable under a unique
// name (never a Known DLL) is found by a plain LoadLibraryW before the call
// and is no longer found after it, while System32 itself is still searched.
// The copies are removed again at the end.

#include "platform/DllSearchHardening.hpp"

#include <cassert>
#include <filesystem>
#include <iostream>
#include <string>
#include <system_error>

namespace {

std::filesystem::path ExecutableDirectory() {
    std::wstring buffer(32768, L'\0');
    const DWORD length = GetModuleFileNameW(
        nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    assert(length > 0 && length < buffer.size());
    buffer.resize(length);
    return std::filesystem::path(buffer).parent_path();
}

std::filesystem::path System32File(const wchar_t* name) {
    std::wstring buffer(MAX_PATH, L'\0');
    const UINT length = GetSystemDirectoryW(
        buffer.data(), static_cast<UINT>(buffer.size()));
    assert(length > 0 && length < buffer.size());
    buffer.resize(length);
    return std::filesystem::path(buffer) / name;
}

struct ProbeCopies {
    std::filesystem::path first;
    std::filesystem::path second;

    ~ProbeCopies() {
        std::error_code ec;
        std::filesystem::remove(first, ec);
        std::filesystem::remove(second, ec);
    }
};

} // namespace

int main() {
    const auto directory = ExecutableDirectory();
    const auto source = System32File(L"version.dll");
    assert(std::filesystem::exists(source));

    ProbeCopies probes{
        directory / L"asterun_dll_search_probe_a.dll",
        directory / L"asterun_dll_search_probe_b.dll",
    };
    std::filesystem::copy_file(
        source, probes.first, std::filesystem::copy_options::overwrite_existing);
    std::filesystem::copy_file(
        source, probes.second, std::filesystem::copy_options::overwrite_existing);

    // Control: with the standard search order the application directory is
    // searched, so a bare module name resolves to the copy beside the EXE.
    HMODULE before = LoadLibraryW(L"asterun_dll_search_probe_a.dll");
    assert(before != nullptr);
    FreeLibrary(before);

    const DWORD result = altrun::win::RestrictDefaultDllSearchToSystem32();
    assert(result == ERROR_SUCCESS);

    // After the call the application directory is no longer searched.
    SetLastError(ERROR_SUCCESS);
    HMODULE after = LoadLibraryW(L"asterun_dll_search_probe_b.dll");
    const DWORD afterError = GetLastError();
    if (after != nullptr) {
        FreeLibrary(after);
    }
    assert(after == nullptr);
    assert(afterError == ERROR_MOD_NOT_FOUND);

    // System32 is still searched for bare module names.
    HMODULE system = LoadLibraryW(L"mpr.dll");
    assert(system != nullptr);
    FreeLibrary(system);

    // The restriction is idempotent for a second caller in the same process.
    assert(altrun::win::RestrictDefaultDllSearchToSystem32() == ERROR_SUCCESS);

    std::cout << "DLL search hardening tests passed\n";
    return 0;
}
