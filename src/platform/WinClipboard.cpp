#include "WinClipboard.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>

#include <cstring>

namespace altrun::win {

bool SetClipboardUnicodeText(
    std::wstring_view text) {
    if (text.empty()) {
        SetLastError(
            ERROR_INVALID_DATA);
        return false;
    }

    bool opened = false;

    for (int attempt = 0;
         attempt < 8;
         ++attempt) {
        if (OpenClipboard(nullptr)) {
            opened = true;
            break;
        }

        Sleep(5);
    }

    if (!opened) {
        SetLastError(
            ERROR_BUSY);
        return false;
    }

    if (!EmptyClipboard()) {
        const DWORD error =
            GetLastError();
        CloseClipboard();
        SetLastError(error);
        return false;
    }

    const SIZE_T bytes =
        (text.size() + 1) *
        sizeof(wchar_t);

    HGLOBAL memory =
        GlobalAlloc(
            GMEM_MOVEABLE,
            bytes);

    if (!memory) {
        const DWORD error =
            GetLastError();
        CloseClipboard();
        SetLastError(error);
        return false;
    }

    void* buffer =
        GlobalLock(memory);

    if (!buffer) {
        const DWORD error =
            GetLastError();
        GlobalFree(memory);
        CloseClipboard();
        SetLastError(error);
        return false;
    }

    std::memcpy(
        buffer,
        text.data(),
        text.size() *
            sizeof(wchar_t));

    static_cast<wchar_t*>(
        buffer)[text.size()] =
        L'\0';

    GlobalUnlock(memory);

    if (!SetClipboardData(
            CF_UNICODETEXT,
            memory)) {
        const DWORD error =
            GetLastError();
        GlobalFree(memory);
        CloseClipboard();
        SetLastError(error);
        return false;
    }

    // Ownership of memory transfers to the system after SetClipboardData.
    CloseClipboard();
    SetLastError(ERROR_SUCCESS);
    return true;
}

} // namespace altrun::win
