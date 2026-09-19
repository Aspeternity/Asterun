# Everything IPC foundation

v0.5.0-alpha.1 introduced the transport foundation. v0.5.0-alpha.2 connects that transport to the launcher through a dynamic provider and unified result model.

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
EverythingProvider -- async ------+--> static-first result merge --> Launcher
                                      (alpha.2 temporary policy)
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

Unified ranking and mixed-result interaction policy are alpha.3 work; Settings/diagnostics and schemaVersion 3 follow later.
