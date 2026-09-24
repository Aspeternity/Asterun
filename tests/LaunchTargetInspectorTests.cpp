#include "platform/LaunchTargetInspector.hpp"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shobjidl.h>
#include <wrl/client.h>

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

using Microsoft::WRL::ComPtr;
using namespace altrun;

namespace {

std::filesystem::path CurrentExecutable() {
    std::vector<wchar_t> buffer(
        32768,
        L'\0');

    const DWORD length =
        GetModuleFileNameW(
            nullptr,
            buffer.data(),
            static_cast<DWORD>(
                buffer.size()));

    assert(length > 0);
    assert(length < buffer.size());

    return std::filesystem::path(
        std::wstring(
            buffer.data(),
            length));
}

void CreateShortcut(
    const std::filesystem::path& shortcut,
    const std::filesystem::path& target) {

    ComPtr<IShellLinkW> shellLink;

    assert(SUCCEEDED(
        CoCreateInstance(
            CLSID_ShellLink,
            nullptr,
            CLSCTX_INPROC_SERVER,
            IID_PPV_ARGS(
                &shellLink))));

    assert(SUCCEEDED(
        shellLink->SetPath(
            target.c_str())));

    ComPtr<IPersistFile> persist;
    assert(SUCCEEDED(
        shellLink.As(&persist)));

    assert(SUCCEEDED(
        persist->Save(
            shortcut.c_str(),
            TRUE)));
}

} // namespace

int wmain() {
    const HRESULT com =
        CoInitializeEx(
            nullptr,
            COINIT_APARTMENTTHREADED);

    assert(
        SUCCEEDED(com) ||
        com == RPC_E_CHANGED_MODE);

    const auto root =
        std::filesystem::temp_directory_path() /
        "ALTRunNext-launch-target-inspector";

    std::error_code ec;
    std::filesystem::remove_all(
        root,
        ec);
    ec.clear();
    std::filesystem::create_directories(
        root,
        ec);
    assert(!ec);

    const auto executable =
        CurrentExecutable();

    assert(
        win::InspectLaunchTarget(
            executable.wstring()) ==
        LaunchTargetKind::
            ConsoleExecutable);

    const auto doc =
        root /
        "whats-new.chm";

    {
        std::ofstream output(
            doc,
            std::ios::binary);
        output << "help";
    }

    const auto appLink =
        root /
        "Application.lnk";
    const auto docLink =
        root /
        "What's New.lnk";

    CreateShortcut(
        appLink,
        executable);
    CreateShortcut(
        docLink,
        doc);

    const auto app =
        win::InspectShellLink(
            appLink);
    const auto help =
        win::InspectShellLink(
            docLink);

    assert(app.has_value());
    assert(help.has_value());

    assert(
        app->targetKind ==
        LaunchTargetKind::
            ConsoleExecutable);

    assert(
        help->targetKind ==
        LaunchTargetKind::
            Document);

    assert(
        std::filesystem::path(
            help->target)
            .filename() ==
        doc.filename());

    std::filesystem::remove_all(
        root,
        ec);

    if (SUCCEEDED(com)) {
        CoUninitialize();
    }

    std::cout
        << "Windows launch-target inspector tests passed\n";
    return 0;
}
