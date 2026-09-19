# Everything IPC foundation

v0.5.0-alpha.1 introduces the transport foundation for Everything file and folder search. It does not yet connect file results to the launcher.

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

The existing static catalog pipeline remains unchanged in alpha.1.

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

## Persistence and UI

Alpha.1 does not:

- add `everything.filesystem` to the static Provider Registry;
- change settings schemaVersion 2;
- write Everything results to `provider-cache.json`;
- write file usage to `usage.json`;
- display files/folders in Classic;
- load or ship `Everything64.dll`;
- use Everything 1.5-only named-pipe APIs.

Those integrations begin in later v0.5 phases.
