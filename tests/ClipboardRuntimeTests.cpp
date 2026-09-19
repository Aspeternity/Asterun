#include "platform/WinClipboard.hpp"

#include <windows.h>

#include <cassert>
#include <iostream>
#include <string>

using namespace altrun::win;

int main() {
    const std::wstring expected =
        L"ALTRun Next \u526a\u8d34\u677f "
        L"D:\\Folder With Spaces";

    assert(
        SetClipboardUnicodeText(
            expected));

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

    assert(opened);

    HANDLE handle =
        GetClipboardData(
            CF_UNICODETEXT);

    assert(handle != nullptr);

    const auto* text =
        static_cast<const wchar_t*>(
            GlobalLock(handle));

    assert(text != nullptr);
    assert(std::wstring(text) == expected);

    GlobalUnlock(handle);
    CloseClipboard();

    std::cout
        << "Clipboard runtime tests passed\n";
    return 0;
}
