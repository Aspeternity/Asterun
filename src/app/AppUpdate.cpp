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

        checking =
            updateStatus_.stage ==
                win::UpdateStage::Checking &&
            updateStatus_.running;

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

    {
        std::scoped_lock lock(
            updateMutex_);

        if (updateStatus_.running) {
            return false;
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

    updateWorkerStartedTick_.store(
        static_cast<std::uint64_t>(
            GetTickCount64()));
    updateWorkerRunning_.store(true);

    updateThread_ =
        std::jthread(
            [this,
             channel,
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
                        updateStatus_ =
                            result.snapshot;
                        updateManifest_ =
                            result.manifest;
                    }
                }
                } catch (...) {
                    std::scoped_lock lock(updateMutex_);
                    if (generation == updateGeneration_.load()) {
                        updateStatus_.stage = win::UpdateStage::Failed;
                        updateStatus_.failure = win::UpdateFailure::UnexpectedFailure;
                        updateStatus_.nativeError = ERROR_GEN_FAILURE;
                        updateStatus_.running = false;
                        updateManifest_.reset();
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

        if (updateStatus_.running ||
            updateStatus_.stage !=
                win::UpdateStage::
                    Available ||
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

                        if (result.snapshot
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

} // namespace altrun
