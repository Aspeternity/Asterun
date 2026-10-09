#pragma once

#include <windows.h>

namespace altrun::notification_identity {

// Best-effort registration for the Windows notification attribution row.
// Failure must never prevent Asterun from starting or showing its existing
// Shell_NotifyIcon notification.
void Register(HINSTANCE instance) noexcept;

} // namespace altrun::notification_identity
