#include "App.hpp"
#include "../ui/SettingsWindow.hpp"
#include "Version.hpp"

#include <chrono>
#include <cstdint>
#include <string>
#include <utility>

namespace altrun {

bool App::SetUpdateSettings(
    bool autoCheck,
    UpdateChannel channel) {
    const auto previous =
        settingsStore_.Data();

    if (previous.autoCheckUpdates ==
            autoCheck &&
        previous.updateChannel ==
            channel) {
        return true;
    }

    if (!settingsStore_
             .SetUpdateSettings(
                 autoCheck,
                 channel)) {
        return false;
    }

    const bool channelChanged =
        previous.updateChannel !=
        channel;

    if (!channelChanged) {
        return true;
    }

    InvalidateUpdateCheckForChannelChange();
    return true;
}

void App::InvalidateUpdateCheckForChannelChange() {
    bool checking = false;
    bool preserveActiveUpdate = false;

    {
        std::scoped_lock lock(
            updateMutex_);

        // A quiet refresh of an offer is a check too: its result belongs
        // to the previous channel and must never be published.
        checking =
            (updateStatus_.stage ==
                 win::UpdateStage::Checking &&
             updateStatus_.running) ||
            updateRefreshingOffer_;

        preserveActiveUpdate =
            updateStatus_.stage ==
                win::UpdateStage::Downloading ||
            updateStatus_.stage ==
                win::UpdateStage::Verifying ||
            updateStatus_.stage ==
                win::UpdateStage::Extracting ||
            updateStatus_.stage ==
                win::UpdateStage::ReadyToInstall ||
            updateStatus_.stage ==
                win::UpdateStage::Applying;

        updateSettingsChangedSinceCheck_ =
            true;

        if (checking) {
            ++updateGeneration_;
            updateRefreshingOffer_ = false;
            updateCheckAutomatic_ = false;
        }

        if (!preserveActiveUpdate) {
            updateStatus_ = {};
            updateManifest_.reset();
            updateInstallWhenReady_ =
                false;
        }
    }

    if (checking &&
        updateThread_.joinable()) {
        updateThread_.request_stop();
    }

    if (settingsWindow_) {
        settingsWindow_->
            OnUpdateStatusChanged();
    }
}

win::UpdateSnapshot
App::UpdateStatus() const {
    std::scoped_lock lock(
        updateMutex_);
    return updateStatus_;
}

bool App::UpdateSettingsChangedSinceCheck()
    const {
    std::scoped_lock lock(
        updateMutex_);
    return
        updateSettingsChangedSinceCheck_;
}

bool App::UpdateWorkerRunning()
    const noexcept {
    return updateWorkerRunning_.load();
}

bool App::StartUpdateCheck(
    bool force) {
    if (!force &&
        !settingsStore_.Data()
             .autoCheckUpdates) {
        return false;
    }

    if (!force) {
        const auto now =
            std::chrono::system_clock::
                to_time_t(
                    std::chrono::
                        system_clock::now());

        if (!win::UpdateAutoCheckDue(
                dataDirectory_,
                static_cast<std::int64_t>(
                    now))) {
            return false;
        }
    }

    if (updateWorkerRunning_.load()) {
        return false;
    }

    // An automatic check over an existing offer runs quietly: the offer
    // stays visible and is replaced only by another offer, so a newer
    // release supersedes it while a failed or timed-out check cannot.
    bool refreshOffer = false;

    {
        std::scoped_lock lock(
            updateMutex_);

        if (updateStatus_.running) {
            return false;
        }

        // An automatic check never replaces a download in progress or a
        // prepared install the user can still retry.
        if (!force) {
            const auto stage =
                updateStatus_.stage;
            refreshOffer =
                stage == win::UpdateStage::Available &&
                updateManifest_.has_value();
            const bool replaceable =
                refreshOffer ||
                stage == win::UpdateStage::Idle ||
                stage == win::UpdateStage::UpToDate ||
                stage == win::UpdateStage::ChannelNotNewer ||
                (stage == win::UpdateStage::Failed &&
                 !updateManifest_);

            if (!replaceable) {
                return false;
            }
        }
    }

    if (updateThread_.joinable()) {
        updateThread_.join();
    }

    const auto channel =
        settingsStore_.Data()
            .updateChannel;
    const std::uint64_t generation =
        ++updateGeneration_;

    {
        std::scoped_lock lock(
            updateMutex_);

        if (!refreshOffer) {
            updateStatus_ = {};
            updateStatus_.stage =
                win::UpdateStage::Checking;
            updateStatus_.running = true;
            updateStatus_.currentVersion =
                std::string(kVersion);
            updateManifest_.reset();
            updateSettingsChangedSinceCheck_ =
                false;
            updateInstallWhenReady_ =
                false;
        }

        updateCheckAutomatic_ = !force;
        updateRefreshingOffer_ = refreshOffer;
    }

    updateWorkerStartedTick_.store(
        static_cast<std::uint64_t>(
            GetTickCount64()));
    updateWorkerRunning_.store(true);

    updateThread_ =
        std::jthread(
            [this,
             channel,
             generation,
             refreshOffer](
                std::stop_token stopToken) {
                try {
                const auto progress =
                    [this,
                     generation,
                     refreshOffer](
                        const win::
                            UpdateSnapshot&
                                snapshot) {
                        if (refreshOffer ||
                            generation !=
                                updateGeneration_
                                    .load()) {
                            return;
                        }

                        {
                            std::scoped_lock lock(
                                updateMutex_);

                            if (generation !=
                                updateGeneration_
                                    .load()) {
                                return;
                            }

                            updateStatus_ =
                                snapshot;
                        }

                        PostUpdateStatusNotification(
                            generation);
                    };

                const auto result =
                    win::CheckForUpdate(
                        dataDirectory_,
                        kVersion,
                        channel,
                        progress,
                        stopToken);

                if (generation ==
                    updateGeneration_.load()) {
                    std::scoped_lock lock(
                        updateMutex_);

                    if (generation ==
                        updateGeneration_
                            .load()) {
                        if (!refreshOffer ||
                            (result.snapshot.stage ==
                                 win::UpdateStage::
                                     Available &&
                             result.manifest)) {
                            updateStatus_ =
                                result.snapshot;
                            updateManifest_ =
                                result.manifest;
                        }

                        updateRefreshingOffer_ =
                            false;
                    }
                }
                } catch (...) {
                    std::scoped_lock lock(updateMutex_);
                    if (generation == updateGeneration_.load()) {
                        if (!refreshOffer) {
                            updateStatus_.stage = win::UpdateStage::Failed;
                            updateStatus_.failure = win::UpdateFailure::UnexpectedFailure;
                            updateStatus_.nativeError = ERROR_GEN_FAILURE;
                            updateStatus_.running = false;
                            updateManifest_.reset();
                        }
                        updateRefreshingOffer_ = false;
                    }
                }

                updateWorkerRunning_.store(
                    false);
                updateWorkerStartedTick_.store(
                    0);

                PostUpdateStatusNotification(
                    generation);
            });

    StartUpdateReconcileTimer();

    if (settingsWindow_) {
        settingsWindow_->
            OnUpdateStatusChanged();
    }

    return true;
}

bool App::StartUpdateDownloadAndInstall() {
    UpdateManifest manifest;

    {
        std::scoped_lock lock(
            updateMutex_);

        // Failed with a retained manifest means preparing or launching the
        // offered release failed; downloading it again needs no new check.
        if (updateStatus_.running ||
            (updateStatus_.stage !=
                 win::UpdateStage::
                     Available &&
             updateStatus_.stage !=
                 win::UpdateStage::
                     Failed) ||
            !updateManifest_) {
            return false;
        }

        manifest = *updateManifest_;
    }

    if (updateWorkerRunning_.load()) {
        return false;
    }

    if (updateThread_.joinable()) {
        updateThread_.join();
    }

    const auto channel =
        settingsStore_.Data()
            .updateChannel;
    const std::uint64_t generation =
        ++updateGeneration_;

    {
        std::scoped_lock lock(
            updateMutex_);
        updateStatus_.running = true;
        updateStatus_.stage =
            win::UpdateStage::
                Downloading;
        updateStatus_.failure =
            win::UpdateFailure::None;
        updateStatus_.nativeError = 0;
        updateStatus_.downloadedBytes =
            0;
        updateStatus_.totalBytes = 0;
        updateStatus_.stagingDirectory.clear();
        updateStatus_.verifiedArchive.clear();
        updateStatus_.assetSha256.clear();
        updateInstallWhenReady_ =
            true;
    }

    updateWorkerStartedTick_.store(
        static_cast<std::uint64_t>(
            GetTickCount64()));
    updateWorkerRunning_.store(true);

    updateThread_ =
        std::jthread(
            [this,
             channel,
             manifest =
                 std::move(manifest),
             generation](
                std::stop_token stopToken) {
                try {
                const auto progress =
                    [this,
                     generation](
                        const win::
                            UpdateSnapshot&
                                snapshot) {
                        if (generation !=
                            updateGeneration_
                                .load()) {
                            return;
                        }

                        {
                            std::scoped_lock lock(
                                updateMutex_);

                            if (generation !=
                                updateGeneration_
                                    .load()) {
                                return;
                            }

                            updateStatus_ =
                                snapshot;
                        }

                        PostUpdateStatusNotification(
                            generation);
                    };

                const auto result =
                    win::PrepareUpdate(
                        dataDirectory_,
                        channel,
                        manifest,
                        kVersion,
                        progress,
                        stopToken);

                if (generation ==
                    updateGeneration_.load()) {
                    std::scoped_lock lock(
                        updateMutex_);

                    if (generation ==
                        updateGeneration_
                            .load()) {
                        updateStatus_ =
                            result.snapshot;

                        if (stopToken.stop_requested()) {
                            // Only CancelUpdateDownload() stops a current
                            // prepare: return to the offer, install nothing.
                            updateStatus_.stage =
                                win::UpdateStage::Available;
                            updateStatus_.failure =
                                win::UpdateFailure::None;
                            updateStatus_.nativeError = 0;
                            updateStatus_.running = false;
                            updateStatus_.downloadedBytes = 0;
                            updateStatus_.totalBytes = 0;
                            updateStatus_.stagingDirectory.clear();
                            updateStatus_.verifiedArchive.clear();
                            updateStatus_.assetSha256.clear();
                            updateInstallWhenReady_ =
                                false;
                        } else if (
                            result.snapshot
                                .stage !=
                            win::UpdateStage::
                                ReadyToInstall) {
                            updateInstallWhenReady_ =
                                false;
                        }
                    }
                }
                } catch (...) {
                    std::scoped_lock lock(updateMutex_);
                    if (generation == updateGeneration_.load()) {
                        updateStatus_.stage = win::UpdateStage::Failed;
                        updateStatus_.failure = win::UpdateFailure::UnexpectedFailure;
                        updateStatus_.nativeError = ERROR_GEN_FAILURE;
                        updateStatus_.running = false;
                        updateInstallWhenReady_ = false;
                    }
                }

                updateWorkerRunning_.store(
                    false);
                updateWorkerStartedTick_.store(
                    0);

                PostUpdateStatusNotification(
                    generation);
            });

    StartUpdateReconcileTimer();

    if (settingsWindow_) {
        settingsWindow_->
            OnUpdateStatusChanged();
    }

    return true;
}

bool App::RetryUpdate() {
    win::UpdateFailure failure =
        win::UpdateFailure::None;
    bool hasManifest = false;
    bool prepared = false;

    {
        std::scoped_lock lock(
            updateMutex_);

        if (updateStatus_.running ||
            updateStatus_.stage !=
                win::UpdateStage::Failed) {
            return false;
        }

        failure = updateStatus_.failure;
        hasManifest =
            updateManifest_.has_value();
        prepared =
            !updateStatus_.stagingDirectory.empty() &&
            !updateStatus_.verifiedArchive.empty();

        // The verified package is still staged when only the updater
        // handoff failed (for example a declined UAC prompt): launch again.
        if (prepared &&
            (failure ==
                 win::UpdateFailure::
                     LaunchUpdaterFailed ||
             failure ==
                 win::UpdateFailure::
                     UpdaterMissing)) {
            updateStatus_.stage =
                win::UpdateStage::
                    ReadyToInstall;
            updateStatus_.failure =
                win::UpdateFailure::None;
            updateStatus_.nativeError = 0;
        } else {
            prepared = false;
        }
    }

    if (prepared) {
        return BeginPreparedUpdate();
    }

    // A hash mismatch may mean the release changed since the check; fetch
    // the manifest again rather than retrying the same expectation.
    if (hasManifest &&
        failure !=
            win::UpdateFailure::
                AssetHashMismatch) {
        return StartUpdateDownloadAndInstall();
    }

    return StartUpdateCheck(true);
}

bool App::CancelUpdateDownload() {
    {
        std::scoped_lock lock(
            updateMutex_);

        const auto stage =
            updateStatus_.stage;

        if (!updateStatus_.running ||
            (stage !=
                 win::UpdateStage::Downloading &&
             stage !=
                 win::UpdateStage::Verifying &&
             stage !=
                 win::UpdateStage::Extracting)) {
            return false;
        }

        // Withdraw the install handoff first so a package that finishes
        // preparing in the meantime is never applied.
        updateInstallWhenReady_ =
            false;
    }

    if (updateThread_.joinable()) {
        updateThread_.request_stop();
    }

    return true;
}

} // namespace altrun
