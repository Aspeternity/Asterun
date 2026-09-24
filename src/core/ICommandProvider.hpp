#pragma once

#include "Command.hpp"
#include "LaunchCandidate.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace altrun {

struct ProviderDescriptor {
    std::string id;
    std::wstring name;
    bool defaultEnabled{true};
    int priority{0};
};

struct ProviderAdmissionSample {
    std::wstring title;
    std::wstring discoveredTarget;
    std::wstring resolvedTarget;
    LaunchTargetKind targetKind{
        LaunchTargetKind::Unknown};
    LaunchSurfaceClass surface{
        LaunchSurfaceClass::
            PrimaryApplication};
    LaunchAdmissionReason reason{
        LaunchAdmissionReason::
            UnsupportedTarget};
    bool targetResolved{false};
};

struct ProviderAdmissionDiagnostics {
    static constexpr std::size_t
        kMaxRejectedSamples = 256;

    std::size_t evaluated{0};
    std::size_t admitted{0};
    std::size_t rejected{0};

    std::array<
        std::size_t,
        static_cast<std::size_t>(
            LaunchAdmissionReason::Count)>
        reasonCounts{};

    std::vector<ProviderAdmissionSample>
        rejectedSamples;

    void Record(
        std::wstring_view title,
        std::wstring_view discoveredTarget,
        std::wstring_view resolvedTarget,
        LaunchTargetKind targetKind,
        LaunchSurfaceClass surface,
        bool targetResolved,
        const LaunchAdmission& decision) {

        ++evaluated;

        const auto reasonIndex =
            static_cast<std::size_t>(
                decision.reason);

        if (reasonIndex <
            reasonCounts.size()) {
            ++reasonCounts[
                reasonIndex];
        }

        if (decision.admit) {
            ++admitted;
            return;
        }

        ++rejected;

        if (rejectedSamples.size() >=
            kMaxRejectedSamples) {
            return;
        }

        ProviderAdmissionSample sample;
        sample.title =
            std::wstring(title);
        sample.discoveredTarget =
            std::wstring(
                discoveredTarget);
        sample.resolvedTarget =
            std::wstring(
                resolvedTarget);
        sample.targetKind =
            targetKind;
        sample.surface =
            surface;
        sample.reason =
            decision.reason;
        sample.targetResolved =
            targetResolved;

        rejectedSamples.push_back(
            std::move(sample));
    }
};

struct ProviderDiscoveryPayload {
    std::vector<Command> commands;
    ProviderAdmissionDiagnostics admission;
};

struct ProviderDiscoveryResult {
    std::string id;
    bool success{false};
    std::vector<Command> commands;
    ProviderAdmissionDiagnostics admission;
    std::wstring error;
};

struct ProviderChangeToken {
    std::string id;
    std::uint64_t token{0};
    bool success{false};
};

class ICommandProvider {
public:
    virtual ~ICommandProvider() = default;

    [[nodiscard]] virtual const ProviderDescriptor&
    Descriptor() const noexcept = 0;

    [[nodiscard]] virtual std::vector<Command>
    Discover() const = 0;

    [[nodiscard]] virtual ProviderDiscoveryPayload
    DiscoverDetailed() const {
        ProviderDiscoveryPayload payload;
        payload.commands = Discover();
        payload.admission.evaluated =
            payload.commands.size();
        payload.admission.admitted =
            payload.commands.size();

        const auto admittedIndex =
            static_cast<std::size_t>(
                LaunchAdmissionReason::
                    Admitted);

        if (admittedIndex <
            payload.admission
                .reasonCounts.size()) {
            payload.admission
                .reasonCounts[
                    admittedIndex] =
                payload.commands.size();
        }

        return payload;
    }

    [[nodiscard]] virtual std::uint64_t
    ChangeToken() const = 0;
};

} // namespace altrun
