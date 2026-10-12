#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace altrun::win {

// Restricts the process default DLL search path to System32 for the rest of
// the process (SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_SYSTEM32)).
//
// Scope, so callers do not over-claim what this protects:
// - It applies to DLLs loaded at run time without an explicit search flag,
//   and to the dependencies the loader resolves for them.
// - It does NOT affect static imports, which are resolved before the entry
//   point runs; those are governed by the /DEPENDENTLOADFLAG link option.
// - Known DLLs, API sets, side-by-side manifest redirection, modules that are
//   already loaded, full-path loads and LoadLibraryEx calls that pass their
//   own LOAD_LIBRARY_SEARCH_* flags are not changed by it.
//
// Used only by processes that perform privileged or file-replacing work
// (Update.exe, Uninstall.exe and Asterun.exe's managed Everything service
// commands). The normal Asterun.exe process does not call it because it loads
// third-party Shell extensions in-process, whose dependencies would also be
// restricted.
//
// Returns ERROR_SUCCESS, or the Win32 error that made the call fail; callers
// must not continue with the protected operation on failure.
[[nodiscard]] inline DWORD RestrictDefaultDllSearchToSystem32() noexcept {
    if (SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_SYSTEM32)) {
        return ERROR_SUCCESS;
    }

    const DWORD error = GetLastError();
    return error != ERROR_SUCCESS ? error : ERROR_GEN_FAILURE;
}

} // namespace altrun::win
