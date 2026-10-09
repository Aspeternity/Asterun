#pragma once

#include <windows.h>

#include <cstdint>
#include <string>
#include <vector>

namespace altrun {

// Opt-in, read-only diagnostic. No typed text or keystroke values are retained.
// Disabled unless ASTERUN_IME_TRACE=1 is present when the launcher is created.
class LauncherImeTrace {
public:
    void EnableIfRequested() noexcept;
    void BeginReveal() noexcept;
    void Record(const char* event, HWND owner, HWND edit,
                bool overrideActive, bool originalOpen, bool composing,
                UINT message = 0, WPARAM messageInfo = 0) noexcept;
    void Flush() noexcept;

    [[nodiscard]] bool Enabled() const noexcept { return enabled_; }
    [[nodiscard]] const std::wstring& LogPath() const noexcept { return logPath_; }

private:
    struct Sample {
        std::uint64_t sequence{};
        ULONGLONG tick{};
        unsigned reveal{};
        const char* event{};
        UINT message{};
        ULONG_PTR messageInfo{};
        bool visible{};
        bool foreground{};
        bool active{};
        bool focus{};
        bool overrideActive{};
        bool originalOpen{};
        bool composing{};
        UINT_PTR keyboardLayout{};
        UINT_PTR inputContext{};
        int openStatus{-1};
        bool conversionValid{};
        DWORD conversion{};
        DWORD sentence{};
    };

    bool enabled_{};
    std::uint64_t nextSequence_{};
    unsigned reveal_{};
    unsigned dropped_{};
    std::wstring logPath_;
    std::vector<Sample> samples_;
};

} // namespace altrun
