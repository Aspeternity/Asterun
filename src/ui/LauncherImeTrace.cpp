#include "LauncherImeTrace.hpp"

#include <imm.h>

#include <array>
#include <cstdio>
#include <cwchar>
#include <iterator>

namespace altrun {

void LauncherImeTrace::EnableIfRequested() noexcept {
    wchar_t flag[4]{};
    const DWORD length =
        GetEnvironmentVariableW(L"ASTERUN_IME_TRACE", flag, 4);
    if (length != 1 || flag[0] != L'1') return;

    std::array<wchar_t, MAX_PATH> temp{};
    const DWORD tempLength =
        GetTempPathW(static_cast<DWORD>(temp.size()), temp.data());
    if (tempLength == 0 || tempLength >= temp.size()) return;

    wchar_t filename[96]{};
    const int written = std::swprintf(
        filename, std::size(filename),
        L"Asterun-ime-trace-%lu-%llu.log",
        GetCurrentProcessId(),
        static_cast<unsigned long long>(GetTickCount64()));
    if (written <= 0) return;

    try {
        logPath_ = std::wstring(temp.data()) + filename;
        // All storage is allocated before first reveal, never in the hook.
        samples_.reserve(384);
        enabled_ = true;
    } catch (...) {
        samples_.clear();
        logPath_.clear();
        enabled_ = false;
    }
}

void LauncherImeTrace::BeginReveal() noexcept {
    if (enabled_) ++reveal_;
}

void LauncherImeTrace::Record(
    const char* event, HWND owner, HWND edit,
    bool overrideActive, bool originalOpen, bool composing,
    UINT message, WPARAM messageInfo) noexcept {

    if (!enabled_) return;
    if (samples_.size() == samples_.capacity()) {
        ++dropped_;
        return;
    }

    Sample sample{};
    sample.sequence = ++nextSequence_;
    sample.tick = GetTickCount64();
    sample.reveal = reveal_;
    sample.event = event;
    sample.message = message;
    // Callers pass only non-text input metadata, never VK/WM_CHAR values.
    sample.messageInfo = static_cast<ULONG_PTR>(messageInfo);
    sample.visible = owner && IsWindowVisible(owner);
    sample.foreground = owner && GetForegroundWindow() == owner;
    sample.active = owner && GetActiveWindow() == owner;
    sample.focus = edit && GetFocus() == edit;
    sample.overrideActive = overrideActive;
    sample.originalOpen = originalOpen;
    sample.composing = composing;
    sample.keyboardLayout =
        reinterpret_cast<UINT_PTR>(GetKeyboardLayout(0));

    if (edit && IsWindow(edit)) {
        HIMC context = ImmGetContext(edit);
        sample.inputContext = reinterpret_cast<UINT_PTR>(context);
        if (context) {
            sample.openStatus = ImmGetOpenStatus(context) ? 1 : 0;
            sample.conversionValid =
                ImmGetConversionStatus(
                    context, &sample.conversion,
                    &sample.sentence) != FALSE;
            ImmReleaseContext(edit, context);
        }
    }

    // Capacity is preallocated. Nothing is written to disk while typing.
    samples_.push_back(sample);
}

void LauncherImeTrace::Flush() noexcept {
    if (!enabled_ || samples_.empty()) return;

    HANDLE file = CreateFileW(
        logPath_.c_str(), FILE_APPEND_DATA,
        FILE_SHARE_READ, nullptr, OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return;

    LARGE_INTEGER size{};
    const bool fresh =
        GetFileSizeEx(file, &size) && size.QuadPart == 0;
    if (fresh) {
        constexpr char header[] =
            "Asterun IME passive trace v1; "
            "no typed text, clipboard or key values captured\r\n"
            "status: -1=no HIMC; conv_ok=0 means unavailable; "
            "conv_native=1 means IME_CMODE_NATIVE\r\n";
        DWORD written = 0;
        WriteFile(file, header,
            static_cast<DWORD>(sizeof(header) - 1),
            &written, nullptr);
    }

    for (const Sample& sample : samples_) {
        char line[512]{};
        const int count = std::snprintf(
            line, sizeof(line),
            "seq=%llu tick=%llu reveal=%u event=%s msg=%u info=%llu "
            "visible=%d foreground=%d active=%d edit_focus=%d "
            "hkl=%llx himc=%llx open=%d conv_ok=%d conv=0x%08lx "
            "conv_native=%d sentence=0x%08lx "
            "override=%d originally_open=%d composing=%d\r\n",
            static_cast<unsigned long long>(sample.sequence),
            static_cast<unsigned long long>(sample.tick),
            sample.reveal, sample.event ? sample.event : "?",
            sample.message,
            static_cast<unsigned long long>(sample.messageInfo),
            sample.visible ? 1 : 0,
            sample.foreground ? 1 : 0,
            sample.active ? 1 : 0,
            sample.focus ? 1 : 0,
            static_cast<unsigned long long>(sample.keyboardLayout),
            static_cast<unsigned long long>(sample.inputContext),
            sample.openStatus, sample.conversionValid ? 1 : 0,
            static_cast<unsigned long>(sample.conversion),
            (sample.conversionValid &&
             (sample.conversion & IME_CMODE_NATIVE)) ? 1 : 0,
            static_cast<unsigned long>(sample.sentence),
            sample.overrideActive ? 1 : 0,
            sample.originalOpen ? 1 : 0,
            sample.composing ? 1 : 0);
        if (count < 0 ||
            static_cast<std::size_t>(count) >= sizeof(line)) {
            continue;
        }
        DWORD written = 0;
        WriteFile(file, line, static_cast<DWORD>(count), &written, nullptr);
    }

    if (dropped_ != 0) {
        char line[96]{};
        const int count = std::snprintf(
            line, sizeof(line), "trace_dropped=%u\r\n", dropped_);
        if (count > 0 &&
            static_cast<std::size_t>(count) < sizeof(line)) {
            DWORD written = 0;
            WriteFile(file, line,
                static_cast<DWORD>(count), &written, nullptr);
        }
    }

    CloseHandle(file);
    samples_.clear();
    dropped_ = 0;
}

} // namespace altrun
