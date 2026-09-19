# Everything IPC foundation

v0.5.0-alpha.1 introduced the transport foundation, alpha.2 connected it to the launcher, alpha.3 unified ranking, and beta.1 promotes the dynamic provider into the supported Settings/diagnostics surface.

## Compatibility target

The transport uses the Everything 1.4.1-compatible Unicode Query2 protocol over Win32 `WM_COPYDATA`. This keeps the application portable and avoids an Everything SDK DLL runtime dependency. Everything must already be running and exposing IPC. Everything Lite is treated as unavailable because Lite does not expose IPC.

## Layers

```text
EverythingIpcProtocol
    portable Query2 encode / LIST2 parse
             |
             v
EverythingIpcClient
    worker thread
    hidden reply HWND
    availability detection
    debounce / timeout / stale discard
             |
             v
EverythingQueryResult
    ephemeral query-time File/Folder data
```

The existing static catalog pipeline remains independent. Alpha.2 maps Everything replies through `EverythingProvider` into `LauncherResult` File/Folder entries and posts them back to the UI thread without blocking the static search path.

## Query lifecycle

```text
Query #101 "doc"
Query #102 "dock"
Query #103 "docker"
       |
       +-- latest pending query replaces earlier pending work
       +-- 70 ms debounce
       +-- Query2W WM_COPYDATA on worker thread
       +-- unique 32-bit reply token
       +-- LIST2 reply to hidden HWND
       +-- generation/token validation
       +-- stale replies discarded
```

A missing Everything IPC window completes as `Unavailable`. Sending uses `SendMessageTimeoutW` rather than an unbounded synchronous send. An accepted query also has a bounded reply timeout. Closing or restarting Everything therefore cannot block the UI thread.

## Requested data

Alpha.1 requests only:

- file/folder name;
- parent path;
- full path and name.

No size, dates, attributes, highlights or Everything run-history data are requested.

## Dynamic provider and launcher integration

Alpha.2 fixes the dynamic provider ID as `everything.filesystem`. It is not added to the static Catalog Provider Registry and it remains disabled unless explicitly enabled in the generic provider map.

```text
Static Search --------------------+
                                  |
EverythingProvider -- async ------+--> unified candidate ranking --> Launcher
                                      (alpha.3+ policy)
```

Files use the file name as `title`, parent directory as `subtitle`, and full path as `target`. Folders use the same presentation model with `ResultKind::Folder`. File execution opens the target with the Windows default application; folder execution opens the folder through Shell/Explorer.

Dynamic replies are accepted only for the current launcher generation. Results are never persisted to provider-cache or usage history.

Alpha.2 still does not:

- change settings schemaVersion 2;
- expose an Everything toggle/status UI;
- add cross-kind/provider ranking weights;
- persist file usage;
- load or ship `Everything64.dll`;
- use Everything 1.5-only named-pipe APIs.

Alpha.3 now supplies the unified ranking and mixed-result interaction policy. Everything results receive a local filename/stem/path score, then compete with static results in one candidate pool. Static duplicate targets stay authoritative. Match quality dominates, while result-kind/provider weights are intentionally small.

The launcher requests approximately three times the visible result count from each side before final ranking, so a strong File/Folder match is not lost merely because ten static candidates arrived first. Numeric quick launch follows the final visible order. Single-result immediate execution waits until the current dynamic generation settles, and hide/manual execution cancels the deferred action.

## Beta 1 settings, diagnostics and fallback

v0.5.0-beta.1 promotes `everything.filesystem` into Settings > Search sources and bumps settings to schemaVersion 3. The provider remains default-off. Enabling the checkbox creates the dynamic provider in the running process; disabling it tears the dynamic provider down and returns immediately to static-only search.

The Search Sources page re-probes the Everything IPC window every second while visible and after completed dynamic queries. Diagnostics report current availability plus the last query status, returned/total result counts, latency and native error when present. Availability is intentionally not latched: Everything may be started after ALTRun Next or restarted after a failure, and a later query can recover without restarting the launcher.

If Everything is enabled but unavailable, the dynamic request completes as `Unavailable` and ALTRun Next keeps the already-produced User Command/Application results. This is the supported fallback mode; ALTRun Next never auto-starts Everything and does not emulate IPC for Everything Lite.

Schema-2 -> schema-3 migration preserves an alpha-era explicit `everything.filesystem=true` value. When the key was absent, schema 3 writes the formal default `false`. A schema-3 file presented to a schema-2 reader enters the existing newer-schema read-only path and remains byte-for-byte unchanged.

Everything results and diagnostics remain ephemeral: no File/Folder result, match score, latency or availability state is persisted to provider-cache or usage history.
