# ALTRun Next

ALTRun Next is a clean-room Windows launcher inspired by classic ALTRun: small, keyboard-first, fast, and intentionally low-noise.

## Downloads

### Stable v0.4.0

The current stable release is published at the immutable `v0.4.0` tag:

- Release: https://github.com/Aspeternity/ALTRunNext/releases/tag/v0.4.0
- x64 direct download: https://github.com/Aspeternity/ALTRunNext/releases/download/v0.4.0/ALTRunNext-x64.zip
- ARM64 direct download: https://github.com/Aspeternity/ALTRunNext/releases/download/v0.4.0/ALTRunNext-ARM64.zip
- SHA-256 checksums: https://github.com/Aspeternity/ALTRunNext/releases/download/v0.4.0/SHA256SUMS.txt

### Rolling development build

The latest successful `main` build is always published to the fixed prerelease tag:

- Development release: https://github.com/Aspeternity/ALTRunNext/releases/tag/dev-latest
- x64 direct download: https://github.com/Aspeternity/ALTRunNext/releases/download/dev-latest/ALTRunNext-x64.zip
- ARM64 direct download: https://github.com/Aspeternity/ALTRunNext/releases/download/dev-latest/ALTRunNext-ARM64.zip

You no longer need to find the correct GitHub Actions run. The `dev-latest` release is replaced automatically only after a successful build and test run.

## v0.4.0 — Stable Windows Application Discovery

v0.4.0 promotes the RC1 discovery architecture to stable without adding new v0.4 features. The Classic Launcher remains unchanged.

The stable release includes Start Menu, Windows Apps / UWP / MSIX, App Paths and PATH providers; deterministic cross-provider de-duplication; persistent per-provider cache; background and incremental refresh; source controls and diagnostics; backup self-healing; newer-schema read-only protection; and startup data-health/writeability reporting.

Release safety remains part of the product contract: Core Tests, x64 and ARM64 builds, Windows Provider Registry smoke tests, the Windows 10 API compatibility gate, version-metadata validation and final ZIP package validation must all pass before the immutable release is published. Release assets include `SHA256SUMS.txt`.

User-data schemaVersion remains 1 and provider-cache schemaVersion remains 2, so RC1 users require no data migration for the stable promotion. Windows fixed FileVersion/ProductVersion for stable is `0.4.0.300`.

## v0.4.0-rc.1 — Release Candidate Stabilization

v0.4 RC 1 freezes the Windows application-discovery feature set and turns release safety into a first-class contract.

Startup data health now records whether `settings.json`, `commands.json` or `usage.json` was recovered from its `.bak` file. The Data page keeps that recovery notice visible for the running session, alongside newer-schema read-only protection. ALTRun Next also probes the portable `data/` directory at startup and warns immediately when the directory is not writable.

The About-page version is no longer hard-coded. CMake generates it directly from the root `VERSION` file. CI validates that `VERSION`, the CMake base version, Windows FileVersion/ProductVersion metadata, the application manifest, README and CHANGELOG all describe the same release.

Every Windows archive now passes a package-contract check after compression:

- required EXE, dictionary, license and documentation entries must exist;
- no unexpected runtime DLL may be present;
- packaged `VERSION` must match the source version;
- the packaged EXE FileVersion and ProductVersion must match `VERSION`.

Successful development and versioned releases also publish `SHA256SUMS.txt` for the x64 and ARM64 ZIP archives.

## v0.4.0-beta.2 — Real-world Compatibility & Migration Hardening

v0.4 Beta 2 focuses on data safety and upgrade/downgrade behavior rather than adding new launcher features.

Config Core recovery now follows stricter rules:

```text
primary valid
    → load primary

primary corrupt/missing + backup valid
    → load backup
    → repair primary
    → keep the good backup intact

document schema newer than this binary understands
    → read known fields when possible
    → enter read-only compatibility mode
    → never overwrite the newer document
```

The downgrade guard applies to `settings.json`, `commands.json` and `usage.json`. The Data page reports any file that entered read-only compatibility mode and shows the unsupported schema version.

Migration coverage now exercises old settings without provider keys, every combination of the four provider source switches, alpha.2 provider-cache schema migration, corrupt-primary recovery, future-schema protection and provider/source cache validation.

Windows CI also runs a real Provider Registry smoke executable on both `windows-latest` and `windows-2022`. The smoke test invokes provider discovery/change-token paths and verifies source isolation without assuming that a server runner must contain desktop applications. Real Windows 10/11 desktop behavior remains a manual release-validation item.

## v0.4.0-beta.1 — Discovery Hardening

v0.4 Beta 1 freezes the Windows application-discovery architecture and focuses on regression safety, deterministic de-duplication and diagnostics.

Provider results now pass through a standalone `CommandMerge` layer with explicit precedence:

```text
User shortcuts
    >
Start Menu
    >
Windows Apps
    >
App Paths
    >
PATH
```

Explicit user shortcuts are authoritative and are never silently de-duplicated against one another. Automatic provider entries are suppressed when they resolve to the same normalized target, or when two automatic entries expose the same normalized name plus keyword. Provider precedence is deterministic even if discovery/cache input order changes.

The **Search sources** page now distinguishes cached commands, commands that actually participate in search after de-duplication, duplicate-suppressed commands, the last successful refresh time, and per-provider refresh failures from the current session.

CI now includes a separate `windows-2022` x64 compatibility build while the executable is compiled against the Windows 10 API baseline (`_WIN32_WINNT=0x0A00`). A versioned beta release is blocked unless Core Tests, x64, ARM64 and the compatibility build all pass.

## v0.4.0-alpha.4 — Incremental Refresh

v0.4 Alpha 4 changes automatic Windows discovery from "periodically rescan everything" into source-aware incremental refresh.

Each enabled provider now exposes a lightweight change token. ALTRun Next checks those tokens in a low-frequency monitor thread and refreshes only providers whose underlying source changed.

```text
Provider monitor (5 s)
        │
        ├─ Start Menu fingerprint
        ├─ Windows Apps fingerprint
        ├─ App Paths fingerprint
        └─ PATH fingerprint
                │
                ▼
        changed provider IDs
                │
          750 ms debounce
                │
                ▼
      targeted background refresh
                │
                ▼
     per-provider cache hot reload
```

Rapid source changes are coalesced before discovery starts. If a refresh is already running, new provider IDs are queued and processed afterward instead of forcing another full scan. Disabled providers are excluded from both search and change-token monitoring.

The **Search sources** page now also shows each provider's enabled state, cached command count and last successful cache refresh time.

## v0.4.0-alpha.3 — Provider Registry & Source Control

v0.4 Alpha 3 turns the Windows application-discovery layer into a real provider system while keeping the Classic launcher visually unchanged.

The provider registry is now:

```text
ProviderRegistry
├─ windows.startmenu   → StartMenuProvider
├─ windows.packaged    → PackagedAppProvider
├─ windows.apppaths    → AppPathsProvider
└─ windows.path        → PathProvider
        │
        ▼
per-provider cache
        │
        ▼
CommandStore
        │
        ▼
SearchEngine
```

Each provider has a stable ID, metadata, default-enabled state and priority. The Settings window now includes **Search sources**, where Start Menu, Windows Apps, App Paths and PATH discovery can be enabled or disabled independently.

`data/provider-cache.json` is upgraded to cache schema 2 and stores each provider independently. If one provider fails to refresh, successful providers still update while the failing provider keeps its previous cached results. Alpha 2's flat cache schema is migrated automatically in memory and rewritten as schema 2 after the next successful refresh.

## v0.4.0-alpha.2 — Background Provider Cache

v0.4 Alpha 2 keeps the multi-source Windows application providers from Alpha 1, but moves automatic discovery off the launcher startup path.

The runtime flow is now:

```text
Startup
├─ load data/commands.json
├─ load data/provider-cache.json
└─ launcher becomes usable immediately
        │
        └─ background provider refresh
           ├─ Start Menu
           ├─ App Paths
           ├─ PATH
           └─ AppsFolder (UWP / MSIX / Store)
                    │
                    ├─ atomic cache write
                    └─ UI-thread hot reload
```

`data/provider-cache.json` is generated state, not user configuration. It can be deleted safely; ALTRun Next will rebuild it in the background. The existing **Rebuild program index** action is now non-blocking and keeps the current cached results usable until the refreshed index is ready.

## v0.4.0-alpha.1 — Windows App Provider Core

v0.4 begins the Windows 11 application-discovery phase. Search ranking and the launcher UI remain unchanged; the main change is where applications can come from.

The runtime provider pipeline is now:

```text
CommandStore
├─ persistent user commands
├─ StartMenuProvider
└─ WindowsAppProvider
   ├─ App Paths (HKCU / HKLM)
   ├─ PATH executables
   └─ AppsFolder (UWP / MSIX / Store)
```

All discovered entries feed the same SearchEngine, including Smart Search and pinyin matching. User-defined shortcuts remain authoritative, while duplicate automatic entries are suppressed when providers expose the same effective app.

The existing **Rebuild program index** action now rescans all providers. v0.4.0-alpha.1 intentionally performs discovery synchronously; persistent provider caching and incremental/background refresh are reserved for later v0.4 alphas.

## v0.3.0-alpha.2 — Smart Search

The second v0.3 alpha focuses on search ergonomics rather than UI changes.

New query styles include:

- `Visual Studio Code` -> `vsc`
- `Windows Terminal` -> `wt`
- `网易云音乐` -> `wangyy`
- `微信` -> `wei x`
- `微信 DevTools` -> `wxdt`
- multi-word queries such as `visual code`

Explicit user keywords and aliases still outrank automatically derived initials and pinyin forms.

## v0.3.0-alpha.1.1 — Portable runtime hotfix

This hotfix fixes the Windows launch error from v0.3.0-alpha.1 where `ALTRunNext.exe` could require a missing `cpp-pinyin.dll`.

cpp-pinyin is now compiled directly into ALTRun Next as an explicit static library. The portable archive still contains the Mandarin dictionaries under `dict/mandarin`, but no cpp-pinyin DLL is required.

CI now rejects Windows builds that produce `cpp-pinyin.dll` or any unexpected runtime DLL beside `ALTRunNext.exe`.

## v0.3.0-alpha.1 — Pinyin Search Core

v0.3 starts the search-experience phase while keeping the launcher and Settings UI visually frozen.

Chinese names now gain derived runtime search keys without changing user data:

- `微信` → `weixin` / `wx`
- `网易云音乐` → `wangyiyunyinyue` / `wyyy` (prefixes such as `wyy` also match)
- `计算器` → `jisuanqi` / `jsq`
- phrase-aware polyphonic conversion, e.g. `重庆` → `chongqing`

Pinyin matching applies to user shortcut keywords, aliases and titles as well as automatically discovered Start Menu titles. Explicit keywords and aliases remain stronger ranking signals than generated pinyin.

The conversion layer uses `cpp-pinyin 1.0.2` (Apache-2.0). Release archives include the required Mandarin dictionary under `dict/mandarin` and the dependency license under `third_party/`. If those dictionary files are missing, ALTRun Next simply falls back to the original search behavior.

## v0.2.0-beta.1.1 — Hotkey reliability hotfix

This hotfix rebuilds the global-hotkey lifecycle so the Settings display reflects the real Windows registration state.

- global hotkeys are owned by the UI thread rather than the hidden Launcher window;
- saving a hotkey always performs a real Windows re-registration, even when the combination did not change;
- failed changes restore the previous working hotkey;
- Settings shows whether the current binding is actually registered and exposes the Windows error code on failure;
- the binding is revalidated when Settings opens and after resume from suspend;
- only one ALTRun Next instance can run per Windows session, preventing two tray processes from fighting over the hotkey.

## v0.2.0-beta.1 — Hotkey & Data

Beta 1 completes the first settings/data workflow before visual polish.

- configurable global hotkey with Ctrl / Alt / Shift / Win modifiers;
- real Windows hotkey conflict detection with automatic rollback to the previous binding;
- Start with Windows through the current-user Run key;
- Data page for opening the portable data directory;
- ALTRun Next TSV shortcut import/export;
- backward-compatible five-column TSV import;
- best-effort legacy ALTRun import (Beta);
- clear usage history;
- rebuild the Start Menu program index;
- restore default settings without deleting shortcuts or usage history.

The default launcher hotkey remains `Alt+Space`.

## v0.2.0-alpha.3 — Command Manager

The Settings window now includes a real shortcut manager backed by `data/commands.json`.

- searchable user-shortcut list;
- create, edit and delete shortcuts;
- primary keyword plus multiple aliases;
- application / URL / folder / command types;
- target, arguments and working-directory editing;
- native target-file and working-directory pickers;
- enabled, administrator and pinned flags;
- test launch without modifying usage history;
- manual move-up / move-down ordering;
- duplicate-keyword warnings;
- unsaved-change protection;
- immediate Launcher refresh after saving, deleting or reordering.

Start Menu results remain automatic provider data and are intentionally not shown as editable user shortcuts.

## v0.2.0-alpha.2 — Settings Shell

This alpha adds the first independent modern Settings window on top of the JSON Config Core.

- open Settings from the tray or press `F2` in the launcher;
- left navigation with General, Appearance and About pages;
- General settings are saved immediately to `data/settings.json`;
- hide-after-launch, clear-query-on-show, hide-on-focus-loss and tray visibility are live;
- launcher monitor can be set to cursor, active window or primary display;
- Classic / Modern Compact and Simplified Chinese / English can be changed live;
- About page exposes the data directory and GitHub project;
- Classic launcher UI remains separate and unchanged.

## v0.2.0-alpha.1 — Config Core

This alpha moves persistent state to a versioned JSON data layer before the Settings UI is built.

Live data now lives under:

```text
data/
├─ settings.json
├─ commands.json
└─ usage.json
```

On first launch, existing `settings.ini`, `commands.tsv` and `usage.tsv` are migrated automatically. Legacy files are left untouched so rollback remains possible.

Key changes:

- `schemaVersion: 1` for all JSON documents;
- atomic `.tmp` writes with one-generation `.bak` recovery;
- stable UUIDs for user commands;
- old command IDs retained in `legacyIds` so usage history survives migration;
- aliases, enabled state, admin launch, pinning and manual order are part of the command model;
- user commands are separated from runtime Start Menu discovery;
- Start Menu discovery is now a dedicated provider and is never persisted into `commands.json`;
- portable Config Core migration/recovery tests run in CI;
- `nlohmann/json` is compile-time only; the final EXE still has no JSON runtime dependency.

See `docs/CONFIG_SCHEMA.md` for the schema.

## v0.1.9 — natural full-width Classic right frame

Classic once again uses equal 7 logical px left and right frame widths. The right side is no longer a flat gray column: it now uses a top-to-bottom frame gradient plus section-aware inner blending, so each content band transitions naturally into the frame while preserving the classic full-width border.

## v0.1.8 — softer Classic right edge

The right side no longer uses a wide light-gray bevel. Classic now keeps the natural left frame, lets the content extend almost to the right edge, and finishes with only a narrow soft transition plus the outer dark line.

## v0.1.6 — Classic frame continuity fix

Classic geometry remains frozen. This release fixes the visible color break where the title bar met the right-side frame by making both side rails continuous from top to bottom. The same correction is applied to the left edge.

## v0.1.3 — screenshot-driven Classic rebuild

Classic mode is now based on a real reference screenshot of the old ALTRun skin rather than a generic "classic Win32" interpretation.

At 150% Windows scaling, the target Classic window is approximately 630 × 375 physical pixels, which maps to about 420 × 250 logical pixels.

### Classic layout

```text
┌──────────────────────────────────────────┐
│  ✦                [calc]              X  │  dark gray custom title
├───────────────────┬──────────────────────┤
│ input             │ hint                 │  pale green
├───┬──────────────────────┬───────────────┤
│ 1 │ calc                 │ Calculator    │
│ 2 │ cmd                  │ Command Prompt│
│ 3 │ explorer             │ File Explorer │
│ … │                      │               │
│ 0 │ ...                  │ ...           │
├──────────────────────────────────────────┤
│ 命令：calc.exe                           │  pale green
└──────────────────────────────────────────┘
```

Classic v0.1.3 specifically adds:

- 420 logical px baseline width
- 250 logical px baseline height
- custom gray gradient title bar
- dynamic `[shortcut]` title
- red close X
- pale-green top and bottom strips
- 16 logical px compact result rows
- 1–9,0 hotkey number column
- shortcut and description columns with two vertical dividers
- reference-matched blue selection color
- SimSun / Tahoma classic typography

Modern Compact remains available from the tray and keeps its Win11-oriented layout.

### Language

- 简体中文 (`zh-CN`) — default
- English (`en-US`)

Right-click the tray icon to switch appearance or language.

### Portable settings

```ini
[general]
ui=classic
language=zh-CN
```

### Core features

- Native C++23 + Win32
- Configurable global launcher hotkey (default `Alt+Space`)
- Persistent user commands from `data/commands.json`
- Multi-source Windows application discovery (Start Menu, App Paths, PATH, UWP/MSIX)
- Persistent per-provider cache with non-blocking background refresh
- Configurable Windows search sources through Provider Registry
- Lightweight fuzzy matching
- Chinese full-pinyin + pinyin-initial matching
- Frequency + recency ranking
- x64 / ARM64 GitHub Actions builds

## Build

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
```
