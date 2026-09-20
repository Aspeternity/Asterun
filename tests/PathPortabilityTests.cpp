#include "platform/WinUtil.hpp"

#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

using namespace altrun;

namespace {

bool EndsWithInsensitive(
    std::wstring value,
    std::wstring suffix) {
    auto lower = [](std::wstring text) {
        for (auto& c : text) {
            if (c >= L'A' && c <= L'Z') {
                c = static_cast<wchar_t>(
                    c - L'A' + L'a');
            }
            if (c == L'/') {
                c = L'\\';
            }
        }
        return text;
    };

    value = lower(std::move(value));
    suffix = lower(std::move(suffix));

    return value.size() >= suffix.size() &&
        value.compare(
            value.size() - suffix.size(),
            suffix.size(),
            suffix) == 0;
}

} // namespace

int main() {
    const std::filesystem::path base =
        LR"(C:\Portable\ALTRunNext)";

    assert(
        win::ResolvePortablePath(
            L"notepad.exe",
            base,
            false) ==
        L"notepad.exe");

    assert(
        win::ResolvePortablePath(
            LR"(..\Tools\Demo\demo.exe)",
            base,
            false) ==
        LR"(C:\Portable\Tools\Demo\demo.exe)");

    assert(
        win::ResolvePortablePath(
            L"data",
            base,
            true) ==
        LR"(C:\Portable\ALTRunNext\data)");

    assert(win::IsUncPath(
        LR"(\\server\share\tool.exe)"));

    assert(
        !win::MakePortablePath(
            LR"(\\server\share\tool.exe)",
            base,
            false));

    assert(
        !win::ExpandPortablePath(
            LR"(\\server\share\tool.exe)",
            base,
            false));

    assert(
        !win::MakePortablePath(
            L"https://example.com",
            base,
            false));

    const auto local =
        win::MakePortablePath(
            LR"(C:\Portable\Tools\Demo\demo.exe)",
            base,
            false);

    assert(local);
    assert(
        local->converted ==
        LR"(..\Tools\Demo\demo.exe)");

    const auto expandedLocal =
        win::ExpandPortablePath(
            local->converted,
            base,
            false);

    assert(expandedLocal);
    assert(
        expandedLocal->converted ==
        LR"(C:\Portable\Tools\Demo\demo.exe)");

    const std::wstring windir =
        win::ExpandEnvironment(
            L"%WINDIR%");

    if (!windir.empty() &&
        windir != L"%WINDIR%") {
        const std::wstring systemTarget =
            (std::filesystem::path(windir) /
             L"System32" /
             L"cmd.exe")
                .lexically_normal()
                .wstring();

        const auto portableSystem =
            win::MakePortablePath(
                systemTarget,
                LR"(D:\Portable\ALTRunNext)",
                false);

        assert(portableSystem);
        assert(
            portableSystem->converted
                .find(L"%WINDIR%") == 0 ||
            portableSystem->converted
                .find(L"%SystemRoot%") == 0);

        const auto expandedSystem =
            win::ExpandPortablePath(
                portableSystem->converted,
                base,
                false);

        assert(expandedSystem);
        assert(
            EndsWithInsensitive(
                expandedSystem->converted,
                LR"(\System32\cmd.exe)"));
    }

    std::cout
        << "Path portability tests passed\n";
    return 0;
}
