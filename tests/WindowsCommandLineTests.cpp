#include "platform/WindowsCommandLine.hpp"

#include <cassert>

int main() {
    using altrun::win::QuoteWindowsArgument;
    assert(QuoteWindowsArgument(L"") == L"\"\"");
    assert(QuoteWindowsArgument(L"C:\\Program Files\\App.exe") ==
           L"\"C:\\Program Files\\App.exe\"");
    assert(QuoteWindowsArgument(L"a\"b") == L"\"a\\\"b\"");
    assert(QuoteWindowsArgument(L"C:\\path\\") == L"\"C:\\path\\\\\"");
}
