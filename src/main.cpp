#include "app/App.hpp"
#include "core/ConfigIO.hpp"
#include "core/LaunchCandidate.hpp"
#include "core/LaunchSurface.hpp"
#include "core/TextCodec.hpp"
#include "Version.hpp"
#include "platform/LaunchTargetInspector.hpp"
#include "platform/WinUtil.hpp"

#include <objbase.h>
#include <shellapi.h>

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace {

struct StartupArguments {
    bool repairManagedEverything{
        false};
    bool setManagedEverythingService{
        false};
    bool managedEverythingServiceEnabled{
        false};
    std::wstring diagnosticShortcutPath;
    std::wstring updateHealthEvent;
    std::vector<std::wstring>
        shortcutPaths;
    bool valid{true};
};

StartupArguments ParseArguments() {
    StartupArguments result;

    int argc = 0;
    LPWSTR* argv =
        CommandLineToArgvW(
            GetCommandLineW(),
            &argc);

    if (!argv) {
        result.valid = false;
        return result;
    }

    if (argc == 1) {
        LocalFree(argv);
        return result;
    }

    if (argc >= 3 &&
        std::wstring_view(argv[1]) ==
            L"--add-shortcut") {
        result.shortcutPaths.reserve(
            static_cast<std::size_t>(
                argc - 2));

        for (int index = 2;
             index < argc;
             ++index) {
            if (argv[index] &&
                *argv[index] != L'\0') {
                result.shortcutPaths
                    .emplace_back(
                        argv[index]);
            }
        }

        result.valid =
            !result.shortcutPaths.empty();
        LocalFree(argv);
        return result;
    }

    if (argc == 3 &&
        std::wstring_view(argv[1]) ==
            L"--diagnose-shortcut" &&
        argv[2] &&
        *argv[2] != L'\0') {
        result.diagnosticShortcutPath =
            argv[2];
        LocalFree(argv);
        return result;
    }

    if (argc == 2 &&
        std::wstring_view(argv[1]) ==
            L"--repair-managed-everything-service") {
        result.repairManagedEverything =
            true;
        LocalFree(argv);
        return result;
    }

    if (argc == 3 &&
        std::wstring_view(argv[1]) ==
            L"--set-managed-everything-service") {
        const std::wstring_view value(
            argv[2]);

        if (value == L"enabled") {
            result.setManagedEverythingService =
                true;
            result.managedEverythingServiceEnabled =
                true;
            LocalFree(argv);
            return result;
        }

        if (value == L"disabled") {
            result.setManagedEverythingService =
                true;
            result.managedEverythingServiceEnabled =
                false;
            LocalFree(argv);
            return result;
        }

        result.valid = false;
        LocalFree(argv);
        return result;
    }

    if (argc == 3 &&
        std::wstring_view(argv[1]) ==
            L"--post-update-health-event" &&
        argv[2] &&
        *argv[2] != L'\0') {
        result.updateHealthEvent =
            argv[2];
        LocalFree(argv);
        return result;
    }

    result.valid = false;
    LocalFree(argv);
    return result;
}


const char* LaunchTargetKindName(
    altrun::LaunchTargetKind kind) noexcept {

    switch (kind) {
    case altrun::LaunchTargetKind::Unknown:
        return "unknown";
    case altrun::LaunchTargetKind::ExecutableUnknown:
        return "executable-unknown";
    case altrun::LaunchTargetKind::GuiExecutable:
        return "gui-executable";
    case altrun::LaunchTargetKind::ConsoleExecutable:
        return "console-executable";
    case altrun::LaunchTargetKind::CommandScript:
        return "command-script";
    case altrun::LaunchTargetKind::ShellApplication:
        return "shell-application";
    case altrun::LaunchTargetKind::SystemControl:
        return "system-control";
    case altrun::LaunchTargetKind::Document:
        return "document";
    case altrun::LaunchTargetKind::WebUri:
        return "web-uri";
    }

    return "unknown";
}

int RunShortcutDiagnostic(
    const std::filesystem::path& shortcutPath) {

    const auto inspection =
        altrun::win::
            InspectShellLinkDetailed(
                shortcutPath);

    nlohmann::json root;
    root["version"] =
        std::string(altrun::kVersion);
    root["shortcutPath"] =
        altrun::text::ToUtf8(
            shortcutPath.wstring());
    root["shellLink"]["stage"] =
        altrun::win::
            ShellLinkInspectionStageName(
                inspection.stage);
    root["shellLink"]["nativeResult"] =
        inspection.nativeResult;

    if (inspection.shortcut) {
        const auto& shortcut =
            *inspection.shortcut;

        root["shellLink"]["target"] =
            altrun::text::ToUtf8(
                shortcut.target);
        root["shellLink"]["arguments"] =
            altrun::text::ToUtf8(
                shortcut.arguments);
        root["shellLink"]["workingDirectory"] =
            altrun::text::ToUtf8(
                shortcut.workingDirectory);

        const auto& target =
            inspection.targetInspection;

        const auto legacyKind =
            target.inferredKind !=
                    altrun::LaunchTargetKind::Unknown
                ? target.inferredKind
                : target.executable.legacyKind;

        root["targetInspection"]["inferredKind"] =
            LaunchTargetKindName(
                target.inferredKind);
        root["targetInspection"]["legacyAlpha516Kind"] =
            LaunchTargetKindName(
                legacyKind);
        root["targetInspection"]["currentKind"] =
            LaunchTargetKindName(
                target.finalKind);

        root["targetInspection"]["executable"]["stage"] =
            altrun::win::
                ExecutableInspectionStageName(
                    target.executable.stage);
        root["targetInspection"]["executable"]["fileExists"] =
            target.executable.fileExists;
        root["targetInspection"]["executable"]["optionalMagic"] =
            target.executable.optionalMagic;
        root["targetInspection"]["executable"]["subsystem"] =
            target.executable.subsystem;
        root["targetInspection"]["executable"]["fallbackUsed"] =
            target.executable.fallbackUsed;
        root["targetInspection"]["executable"]["getBinaryTypeSucceeded"] =
            target.executable
                .getBinaryTypeSucceeded;
        root["targetInspection"]["executable"]["binaryType"] =
            target.executable.binaryType;

        const std::wstring title =
            shortcutPath.stem().wstring();

        const auto surface =
            altrun::ClassifyApplicationSurface(
                title,
                shortcut.target);

        const auto legacyAdmission =
            altrun::EvaluateLaunchCandidate({
                altrun::LaunchCandidateSource::
                    StartMenu,
                title,
                shortcut.target,
                surface,
                legacyKind,
                true,
            });

        const auto currentAdmission =
            altrun::EvaluateLaunchCandidate({
                altrun::LaunchCandidateSource::
                    StartMenu,
                title,
                shortcut.target,
                surface,
                target.finalKind,
                true,
            });

        root["admission"]["surface"] =
            altrun::LaunchSurfaceName(
                surface);
        root["admission"]["alpha516"]["admit"] =
            legacyAdmission.admit;
        root["admission"]["alpha516"]["reason"] =
            altrun::LaunchAdmissionReasonName(
                legacyAdmission.reason);
        root["admission"]["current"]["admit"] =
            currentAdmission.admit;
        root["admission"]["current"]["reason"] =
            altrun::LaunchAdmissionReasonName(
                currentAdmission.reason);
    }

    const auto output =
        altrun::win::ExecutableDirectory() /
        "data" /
        "launch-target-diagnostic.json";

    if (!altrun::config::SaveJsonAtomic(
            output,
            root)) {
        return ERROR_WRITE_FAULT;
    }

    MessageBoxW(
        nullptr,
        output.c_str(),
        L"ALTRun Next shortcut diagnostic",
        MB_OK | MB_ICONINFORMATION);

    return 0;
}

} // namespace

int WINAPI wWinMain(
    HINSTANCE instance,
    HINSTANCE,
    PWSTR,
    int) {
    const auto arguments =
        ParseArguments();

    if (!arguments.valid) {
        return ERROR_INVALID_PARAMETER;
    }

    if (arguments
            .repairManagedEverything) {
        const auto result =
            altrun::win::
                RepairManagedEverythingServicePath(
                    altrun::win::
                        ExecutableDirectory() /
                    "data");

        if (result.success) {
            return 0;
        }

        return static_cast<int>(
            result.nativeError != 0
                ? result.nativeError
                : ERROR_GEN_FAILURE);
    }

    if (arguments
            .setManagedEverythingService) {
        const auto result =
            altrun::win::
                ApplyManagedEverythingServiceEnabledPolicy(
                    altrun::win::
                        ExecutableDirectory() /
                    "data",
                    arguments
                        .managedEverythingServiceEnabled);

        if (result.success) {
            return 0;
        }

        return static_cast<int>(
            result.nativeError != 0
                ? result.nativeError
                : ERROR_GEN_FAILURE);
    }

    SetProcessDpiAwarenessContext(
        DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    const HRESULT comResult =
        CoInitializeEx(
            nullptr,
            COINIT_APARTMENTTHREADED |
                COINIT_DISABLE_OLE1DDE);

    if (!arguments
             .diagnosticShortcutPath
             .empty()) {
        const int diagnosticResult =
            RunShortcutDiagnostic(
                arguments
                    .diagnosticShortcutPath);

        if (SUCCEEDED(comResult)) {
            CoUninitialize();
        }

        return diagnosticResult;
    }

    altrun::App app(
        instance,
        arguments.updateHealthEvent,
        arguments.shortcutPaths);
    const int result = app.Run();

    if (SUCCEEDED(comResult)) {
        CoUninitialize();
    }

    return result;
}
