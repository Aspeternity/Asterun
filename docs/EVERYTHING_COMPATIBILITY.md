# Everything compatibility matrix

ALTRun Next v0.5.0-rc.1 freezes the beta.2 transport baseline: the Everything 1.4-compatible Unicode Query2 protocol over local Win32 `WM_COPYDATA`.

## Endpoint selection

The default IPC class is:

```text
EVERYTHING_TASKBAR_NOTIFICATION
```

Named Everything instances use:

```text
EVERYTHING_TASKBAR_NOTIFICATION_(instance-name)
```

Selection is intentionally conservative:

1. Prefer the unnamed/default class when it exists.
2. If the unnamed class is absent, enumerate named-instance classes.
3. Auto-select a named instance only when exactly one candidate exists.
4. If multiple named instances exist, do not guess. Report ambiguity and fall back to application-only search.

This behavior prevents ALTRun Next from silently querying the wrong Everything database on machines that deliberately run multiple instances.

## Version matrix

| Everything setup | RC1 behavior |
| --- | --- |
| 1.4 unnamed/default instance | Query2 WM_COPYDATA through the default class |
| 1.4 single named instance | Unique named-instance fallback |
| 1.4 multiple named instances | Ambiguous; no automatic selection |
| 1.5a default alpha instance | Unique `1.5a` named-instance fallback when no unnamed instance exists |
| 1.5b+ unnamed/default instance | Query2 WM_COPYDATA through the default class |
| 1.5b+ single custom named instance | Unique named-instance fallback |
| 1.5b+ multiple named instances | Ambiguous; no automatic selection |
| Everything Lite | IPC unavailable; static application search continues |

Everything 1.5's SDK3 uses a newer named-pipe IPC transport. beta.2 deliberately does not switch to that 1.5-only path so one implementation continues to cover the existing 1.4-compatible Query2 contract. A future phase can add a second transport only if real-world validation demonstrates a concrete need.

## Reply hardening

beta.2 accepts a LIST2 response only when all of the following hold:

- reply token matches the current in-flight request;
- WM_COPYDATA sender HWND is the endpoint that received the query;
- payload size is within the client safety limit;
- LIST2 request flags match the requested name/path/full-path fields;
- returned item count does not exceed the request limit;
- LIST2 total/count/offset relationships are internally consistent;
- all requested UTF-16 fields are bounded and null terminated.

Unexpected replies fail closed and never replace static launcher results.

## Path behavior

File and folder targets remain opaque Windows paths. beta.2 specifically validates:

- drive roots such as `C:\`;
- UNC paths such as `\\server\share\folder\file.txt`;
- extended-length paths such as `\\?\C:\...`;
- Unicode file and directory names.

Drive/root results keep an empty parent subtitle rather than deriving the misleading parent `C:` → `C:`/drive-relative variants.

## Performance envelope

The launcher normally asks Everything for only a small candidate pool. `EverythingProvider` additionally clamps direct transport requests to 1000 results. CI stress covers:

- 128 rapid replacement queries coalesced by the debounce path;
- 256 returned items with a 500,000-result total-match count;
- stale-generation suppression;
- reply timeout;
- endpoint disappearance/recovery;
- malformed over-limit replies;
- oversized reply payload rejection.

Everything results remain query-time only and are never persisted to `provider-cache.json` or `usage.json`.


## v0.7 Managed bootstrap

v0.7.0-alpha.8 adds dependency onboarding without changing the Query2 transport contract above.

When `everything.filesystem` is enabled, ALTRun Next uses a local-first bootstrap order:

1. Use a currently available Everything IPC endpoint immediately.
2. Look for an ALTRun Next-managed copy, registered App Paths, normal Program Files locations and PATH.
3. Start an existing copy with `-startup -first-instance` and wait for IPC.
4. If no usable IPC is available, stop and report that installation is needed. This local recheck path never downloads anything.
5. Only after the user explicitly chooses **Get and start Everything**, fetch the official stable standard portable package from voidtools, fetch the matching official SHA-256 manifest, verify the archive, extract it under `data/tools/Everything`, start it in the background and wait for IPC.

The managed package is architecture-matched to the ALTRun Next binary (x64 or ARM64). Lite packages are never selected because Lite does not expose the IPC contract used by ALTRun Next. Network transfer uses native WinHTTP and package hashing uses Windows BCrypt; PowerShell and external download helpers are not part of the runtime path.

Bootstrap state is runtime-only. Paths, download progress, errors and IPC readiness are not persisted into settings or provider-cache. The only persistent choice remains the existing `everything.filesystem` provider boolean.

v0.7.0-alpha.8.1 hardens the archive handoff: network bytes remain in a `.zip.download` file until SHA-256 verification succeeds, then the verified file is atomically promoted to the real `.zip` filename before Windows Shell ZIP extraction. This is required because the Shell ZIP namespace is extension-sensitive on real Windows systems.

v0.7.0-alpha.8.2 completes managed first-run indexing. The official portable client is configured to remain a standard-user background process with `show_tray_icon=0` and IPC enabled. ALTRun Next requires the Windows Everything Service for its managed copy so NTFS USN Journal indexing works without keeping the client elevated. Service installation/start is performed only from the explicit Get-and-start flow and may show a Windows UAC prompt; Recheck remains local/non-elevating. Existing external Everything copies are never reconfigured by this managed policy.

v0.7.0-alpha.8.3 closes the managed-client lifecycle. Actual ALTRun Next process exit and disabling the Everything source stop only the managed client executable. The Windows Everything Service remains installed/running. Before invoking `-exit`, ALTRun Next verifies that the default Everything IPC window's process image path exactly matches the managed executable under `data/tools/Everything`; an external Everything instance is never closed by this path. Hiding the launcher to tray is not process exit and therefore does not stop the managed client.

v0.7.0-alpha.8.4 handles the fact that the Windows service outlives a portable folder location. Before starting a stopped service, ALTRun Next reads its SCM ImagePath and resolves the service executable. If that executable no longer exists, Recheck reports a stale service without elevation. The explicit Get-and-start path can then launch an elevated one-shot ALTRun Next maintenance mode which changes the existing service ImagePath to the current managed `Everything.exe -svc`, sets automatic start and starts it. A healthy running service is never retargeted, and a healthy stopped external service is started without changing its path.

v0.7.0-alpha.9.1 separates the persistent service host from the portable application tree. The managed standard-user client remains under `data/tools/Everything`, but an ALTRun-owned Everything Service now runs from the protected `%ProgramFiles%\Aspeternity\ALTRunNext\EverythingService\<Everything-version-arch>` host. A running legacy service whose executable is inside an ALTRun Next portable `data/tools/Everything` tree is classified as needing migration; the explicit elevated maintenance flow stops it, copies the managed executable into the protected host, retargets and restarts the service. Keeping the service warm therefore no longer keeps an old ALTRun Next release folder open. Healthy external service paths remain untouched.

v0.7.0-alpha.9.2 supersedes the detached service host introduced in alpha.9.1. Managed Everything is again entirely portable under `data/tools/Everything`; the Windows Everything Service remains auto-start and can stay running after ALTRun Next exits, while the managed client exits with the launcher. The explicit repair flow recognizes the exact alpha.9.1 `%ProgramFiles%\Aspeternity\ALTRunNext\EverythingService` host as ALTRun-owned, retargets the service back to the current portable managed executable, restarts it and removes that temporary host. A new TEMP-self-copying `Uninstall.exe` provides the correct destructive lifecycle: it removes only service state whose ImagePath belongs to the current managed tree (plus the known alpha.9.1 host), then removes Managed Everything; unrelated Everything installations remain untouched.

v0.7.0-alpha.9.4 distinguishes normal launcher exit from explicitly disabling the Everything provider. Normal exit continues to close only the ALTRun-owned client and leaves an enabled managed service warm. When the provider is turned off, an exact ALTRun-owned service ImagePath is required before the elevated maintenance path may stop the service and set `SERVICE_DISABLED`; turning the provider back on restores `SERVICE_AUTO_START` and starts it. External Everything services never cross this ownership boundary and are left untouched.

## RC1 freeze

v0.5.0-rc.1 makes no protocol or endpoint-selection change relative to beta.2. This document is shipped inside both portable RC packages so a real-world validation run can identify the intended 1.4/1.5/named-instance behavior without relying on repository access. Any post-RC transport change requires a concrete release-blocking compatibility defect and corresponding regression coverage.
