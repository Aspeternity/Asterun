#include "UpdateIconRefresh.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shlobj.h>

#include <array>
#include <string>
#include <system_error>

namespace altrun::win {

bool SignalUpdateHealthAndRefreshIcons(
    std::wstring_view healthEventName,
    const std::filesystem::path& installDirectory,
    UpdateIconNotification observer,
    void* observerContext) {
    if (healthEventName.empty()) {
        return false;
    }

    const std::wstring name(healthEventName);
    const HANDLE event =
        OpenEventW(EVENT_MODIFY_STATE, FALSE, name.c_str());
    if (!event) {
        return false;
    }

    const bool signaled = SetEvent(event) != FALSE;
    CloseHandle(event);
    if (!signaled) {
        return false;
    }

    // Per-file, asynchronous Shell change hints: no global cache deletion,
    // Explorer restart, or synchronous first-frame wait. The Shell may still
    // retain a stale icon on some Windows versions.
    constexpr std::array<const wchar_t*, 3> filenames{
        L"Asterun.exe",
        L"Update.exe",
        L"Uninstall.exe",
    };

    for (const wchar_t* filename : filenames) {
        try {
            const auto path = installDirectory / filename;
            std::error_code error;
            if (!std::filesystem::is_regular_file(path, error) || error) {
                continue;
            }

            if (observer) {
                observer(path, observerContext);
            } else {
                SHChangeNotify(
                    SHCNE_UPDATEITEM,
                    SHCNF_PATHW | SHCNF_FLUSHNOWAIT,
                    path.c_str(),
                    nullptr);
            }
        } catch (...) {
            // Icon refresh is best-effort: never fail a healthy update.
        }
    }

    return true;
}

} // namespace altrun::win
