#include "core/ICommandProvider.hpp"
#include "core/ProviderIndexPolicy.hpp"
#include "core/ProviderIds.hpp"

#include <cassert>
#include <iostream>
#include <string>
#include <vector>

using namespace altrun;

int main() {
    const std::vector<ProviderDescriptor>
        descriptors{
            {
                std::string(
                    providers::kStartMenu),
                L"Start Menu",
                true,
                40,
            },
            {
                std::string(
                    providers::kPackaged),
                L"Windows Apps",
                true,
                30,
            },
            {
                std::string(
                    providers::kPath),
                L"PATH",
                false,
                0,
            },
        };

    ProviderEnableMap enabled =
        providers::DefaultEnabled();

    ProviderCacheData complete;
    complete[
        std::string(
            providers::kStartMenu)] = {};
    complete[
        std::string(
            providers::kPackaged)] = {};

    assert(
        EvaluateProviderIndexState(
            descriptors,
            enabled,
            complete,
            false) ==
        ProviderIndexState::Ready);

    ProviderCacheData empty;

    assert(
        EvaluateProviderIndexState(
            descriptors,
            enabled,
            empty,
            false) ==
        ProviderIndexState::Building);
    assert(
        !ProviderIndexSearchable(
            ProviderIndexState::Building));

    ProviderCacheData incomplete;
    incomplete[
        std::string(
            providers::kStartMenu)] = {};

    assert(
        EvaluateProviderIndexState(
            descriptors,
            enabled,
            incomplete,
            false) ==
        ProviderIndexState::Building);

    assert(
        !ProviderIndexSearchable(
            ProviderIndexState::Building));

    assert(
        EvaluateProviderIndexState(
            descriptors,
            enabled,
            incomplete,
            true) ==
        ProviderIndexState::Degraded);

    assert(
        ProviderIndexSearchable(
            ProviderIndexState::Degraded));

    enabled[
        std::string(
            providers::kPackaged)] = false;

    assert(
        EvaluateProviderIndexState(
            descriptors,
            enabled,
            incomplete,
            false) ==
        ProviderIndexState::Ready);

    enabled[
        std::string(
            providers::kPath)] = true;

    assert(
        EvaluateProviderIndexState(
            descriptors,
            enabled,
            incomplete,
            false) ==
        ProviderIndexState::Building);

    ProviderAdmissionDiagnostics
        diagnostics;

    LaunchAdmission admitted;
    admitted.admit = true;
    admitted.surface =
        LaunchSurfaceClass::
            PrimaryApplication;
    admitted.reason =
        LaunchAdmissionReason::
            Admitted;

    diagnostics.Record(
        L"Calculator",
        L"Calculator.lnk",
        L"C:\\Windows\\System32\\calc.exe",
        LaunchTargetKind::
            GuiExecutable,
        LaunchSurfaceClass::
            PrimaryApplication,
        true,
        admitted);

    LaunchAdmission rejected;
    rejected.admit = false;
    rejected.surface =
        LaunchSurfaceClass::
            PrimaryApplication;
    rejected.reason =
        LaunchAdmissionReason::
            TargetResolutionFailed;

    diagnostics.Record(
        L"Broken shortcut",
        L"Broken.lnk",
        L"",
        LaunchTargetKind::Unknown,
        LaunchSurfaceClass::
            PrimaryApplication,
        false,
        rejected);

    assert(diagnostics.evaluated == 2);
    assert(diagnostics.admitted == 1);
    assert(diagnostics.rejected == 1);
    assert(
        diagnostics.reasonCounts[
            static_cast<std::size_t>(
                LaunchAdmissionReason::
                    Admitted)] == 1);
    assert(
        diagnostics.reasonCounts[
            static_cast<std::size_t>(
                LaunchAdmissionReason::
                    TargetResolutionFailed)] ==
        1);
    assert(
        diagnostics.rejectedSamples
            .size() == 1);
    assert(
        diagnostics.rejectedSamples[0]
            .title ==
        L"Broken shortcut");
    assert(
        !diagnostics.rejectedSamples[0]
             .targetResolved);

    std::cout
        << "Provider index policy tests passed\n";
    return 0;
}
