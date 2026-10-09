#pragma once

#include <filesystem>
#include <string_view>

namespace altrun::win {

// Test seam: observe affected file paths without triggering the real Shell.
using UpdateIconNotification = void (*)(
    const std::filesystem::path&, void*);

// The in-place updater runs the old Update.exe. New Asterun.exe performs this
// only after successful startup health signaling, including on its first upgrade.
[[nodiscard]] bool SignalUpdateHealthAndRefreshIcons(
    std::wstring_view healthEventName,
    const std::filesystem::path& installDirectory,
    UpdateIconNotification observer = nullptr,
    void* observerContext = nullptr);

} // namespace altrun::win
