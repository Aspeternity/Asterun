#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace altrun {

enum class FeedbackCue { Startup, Reveal, Failure };

// Called only for semantic UI events, never for individual search keystrokes.
class FeedbackPolicy {
public:
    void SetEnabled(bool enabled) noexcept {
        enabled_ = enabled;
        played_.fill(false);
    }

    [[nodiscard]] bool Accept(FeedbackCue cue, std::uint64_t now) noexcept {
        if (!enabled_) return false;
        const auto index = static_cast<std::size_t>(cue);
        const auto interval = cue == FeedbackCue::Failure ? 500u : 150u;
        if (played_[index] && now - last_[index] < interval) return false;
        played_[index] = true;
        last_[index] = now;
        return true;
    }

private:
    bool enabled_{false};
    std::array<bool, 3> played_{};
    std::array<std::uint64_t, 3> last_{};
};

} // namespace altrun
