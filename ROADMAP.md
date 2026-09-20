# ALTRun Next Roadmap

## v0.1.x - Classic foundation

Completed:

- Native C++23 / Win32 x64 and ARM64 baseline
- Classic ALTRun-style keyboard-first launcher
- Global activation and tray lifecycle
- Per-monitor DPI behavior
- Portable local state
- Screenshot-driven Classic UI reconstruction

## v0.2.x - Configuration & command management

Completed:

- Versioned JSON Config Core with atomic writes and backup recovery
- Modern Settings Shell while keeping the launcher independent
- User Command Manager with stable UUIDs, aliases and import/export
- Configurable global hotkeys with runtime conflict recovery
- Start with Windows
- Data maintenance tools and legacy ALTRun import

## v0.3.x - Smart Search

Completed:

- Chinese full-pinyin and pinyin-initial matching
- Phrase-aware polyphonic conversion
- Multi-token search
- English initials and mixed Chinese/English abbreviation matching
- Frequency and recency ranking
- Portable static cpp-pinyin integration

## v0.4.x - Windows application discovery

Completed in v0.4.0:

- Multi-source discovery from Start Menu, App Paths, PATH and AppsFolder
- UWP / MSIX / Microsoft Store application discovery
- Cross-provider de-duplication with user shortcuts authoritative
- Persistent provider cache and non-blocking background refresh
- Provider Registry with stable IDs and source controls
- Per-provider cache and provider-level refresh failure isolation
- Incremental / change-driven provider refresh
- Refresh debounce and provider scheduling
- Provider cache counts and last-refresh diagnostics
- Deterministic discovery de-duplication with regression coverage
- Per-provider active/suppressed/error diagnostics
- Windows 10 API baseline and Windows compatibility CI gate

Release hardening:

- Upgrade/migration matrix for alpha-era settings and provider caches
- Backup self-healing and downgrade-safe newer-schema protection
- Startup data-health diagnostics and portable-directory writeability checks
- Windows Provider Registry runtime smoke tests on current and Windows Server 2022 runners
- Version-metadata and final-package release gates
- Real Windows 10/11 desktop discovery remains part of manual release QA in addition to automated smoke coverage
- v0.4.0 stable was promoted from RC1 without adding new v0.4 features

## v0.4.1 - Classic settings parity

Completed in v0.4.1:

- Independent settings schemaVersion 2 with downgrade-safe migration
- Optional auxiliary global hotkey
- Show-on-startup behavior
- Opt-in * / ? wildcard search
- Classic numeric quick execution and selectable number order
- Optional single-result immediate execution
- Settings UI now exposes show-on-startup, wildcard matching, numeric quick launch/order, single-result execution and the auxiliary hotkey
- Primary and auxiliary hotkey registration state is surfaced independently in General settings
- Alpha 3 hardens hotkey retry/reset behavior, IME-safe immediate execution, numeric-key repeat handling and high-DPI/narrow-window Settings layout
- General Settings now supports responsive stacking plus vertical scrolling instead of requiring an oversized fixed-height window
- Windows CI includes hotkey-codec regression coverage in both current and compatibility runners
- Beta 1 freezes schemas, provider IDs, safe defaults and core Classic geometry through a dedicated release-contract gate
- Beta 2 makes Release-mode assertion tests effective, adds real RegisterHotKey runtime smoke, and moves Classic behavior / Settings layout decisions behind portable regression-tested helpers
- Automated validation covers 100%/125%/150%/200% General-page layout invariants, IME-safe single-result gating, numeric quick-launch mapping and v0.4.0 settings migration/downgrade protection
- RC1 hardens publication with tag/VERSION preflight, checksum self-verification and an exact portable-package top-level allowlist
- v0.4.1 Stable preserves the frozen schema/provider/Classic contracts and uses Windows fixed version 0.4.1.300
- Final x64 ZIPs receive a packaged-runtime startup smoke before publication
- Tag-triggered releases require the same Core/Windows smoke/compatibility gates as main publication and now fail fast on a mismatched release tag
- A packaged desktop-validation checklist defines the remaining real Windows 10/11, mixed-DPI, provider and interactive-input matrix
- Classic launcher geometry remains frozen
- RC fixes are limited to regressions, compatibility, data safety and publication/package issues; stable promotion follows manual desktop sign-off with no release-blocking defect

## v0.5.x - Everything integration

Completed in v0.5.0:

- v0.5.0-alpha.1 establishes native Everything 1.4-compatible Unicode Query2 IPC over WM_COPYDATA
- Dedicated worker thread and hidden reply window keep Everything IPC off the launcher/UI thread
- 70 ms debounce, generation/reply-token stale discard, send timeout and reply timeout handling
- Portable Query2/LIST2 protocol parsing with Unicode and malformed-payload regression tests
- Real Win32 fake-Everything runtime smoke tests run without installing Everything in CI
- x64 and ARM64 production builds compile the IPC foundation without adding an Everything runtime DLL
- settings remains schemaVersion 2 through alpha.2; the formal schemaVersion 3 migration remains reserved for the Settings phase
- v0.5.0-alpha.2 introduces LauncherResult / DynamicQueryProvider / everything.filesystem and actual asynchronous File/Folder launcher results
- Alpha 2 keeps Everything default-off through explicit raw provider opt-in, preserves Classic geometry and keeps file results ephemeral
- v0.5.0-alpha.3 replaces static-first append with unified match-quality ranking plus conservative kind/provider weights
- Alpha 3 also hardens mixed-result numeric execution, deferred single-result execution and Classic File/Folder presentation without changing geometry
- v0.5.0-beta.1 promotes Everything into Search Sources with a supported default-off toggle and live IPC/query diagnostics
- Beta 1 upgrades settings to schemaVersion 3, preserves alpha opt-ins, adds explicit schema-2 downgrade read-only coverage and validates unavailable -> available recovery
- v0.5.0-beta.2 hardens real-world compatibility with unnamed/named Everything instances, conservative multi-instance fallback, sender/payload validation, long/UNC/root path handling and high-churn/large-result runtime stress
- v0.5.0-rc.1 freezes the v0.5 surface, packages the unified real-desktop validation matrix and limits further changes to regression/compatibility/data-safety/publication fixes
- v0.5.0-rc.2 addresses real-desktop Everything onboarding: actionable missing-installation guidance plus official-download and immediate-recheck actions, without adding dependency management or changing frozen v0.5 contracts
- v0.5.0-rc.3 is the final Settings UI polish candidate: fixes default/DPI text overlap, shortcut-editor label/action layout, Search Sources diagnostics spacing and sidebar navigation styling while preserving every frozen v0.5 core contract
- v0.5.0 Stable promotes the frozen RC3 line without new features and uses Windows fixed version 0.5.0.300
- The packaged v0.5 RC validation matrix remains the manual regression record; automated CI is necessary but is not represented as a substitute for unrecorded desktop observations

## v0.6.x - Smart actions & Windows integration

In progress:

- v0.6.0-alpha.1 establishes ResultKind::Action, explicit action payloads and OpenUrl as the first reusable Smart Action contract
- Direct HTTP/HTTPS and www. input becomes an executable URL action
- Existing URL user commands can use {query} as a UTF-8 percent-encoded web-search template through their keyword/aliases
- v0.6.0-alpha.2 adds per-launch Windows Activation Context capture before the launcher takes foreground focus
- Alpha 2.1 hardens that context so virtual Explorer sources such as Home / This PC / Quick access remain valid even though they have no filesystem source path
- Explorer resolution remains conservative through the Windows Shell model and refuses ambiguous multi-tab/multi-window guesses
- Everything Folder results keep Enter=open while Ctrl+Enter navigates the captured Explorer to that folder
- v0.6.0-alpha.3 extends the same Activation Context to standard Open / Save / folder-picker dialogs; Folder results use Enter to navigate the captured dialog directly
- File-dialog targeting is conservative (#32770 + Shell view), revalidated before execution, and refuses unsafe foreground/input injection
- v0.6.0-alpha.4 adds Total Commander 9+ Activation Context, exact-window active-panel navigation via Ctrl+Enter and developer {folder} templates for Target / Arguments / Working Directory
- {folder} is available only from a captured real filesystem folder (Explorer or Total Commander); virtual/plugin/FTP contexts do not guess
- v0.6.0-alpha.5 adds runtime CopyText actions, copy / clip / 复制 text commands and Ctrl+Shift+C selected-result target copying using the Unicode Windows clipboard
- Ctrl+C remains native query-text copy; clipboard Smart Actions never read or persist previous clipboard contents
- v0.6.0-alpha.6 centralizes global and launcher-local shortcuts behind stable Hotkey Registry action IDs and a dedicated Settings page
- Alpha 6 upgrades settings to schemaVersion 4, migrates schema-3 primary/auxiliary bindings and retains a downgrade-readable compatibility mirror
- Contextual navigation is session-scoped and cleared when the launcher hides
- commands/usage remain schemaVersion 1, provider-cache remains schemaVersion 2 and v0.5 provider defaults remain unchanged
- Classic launcher geometry remains frozen

Planned next:

- v0.6.0-beta.1 feature freeze, Smart Actions UX, diagnostics and desktop-validation hardening
- Broader provider/action contracts suitable for future extensions
- Calculator may return later as an optional smart action
- Managed / Portable Everything remains a separate candidate after the core v0.6.0 Smart Actions line

## v0.7.x - Shortcut & launcher workflow

In progress:

- Standalone Shortcut Manager and task-oriented Shortcut Editor
- Unified comma-separated shortcut keywords while keeping the persisted primary/alias compatibility model internal
- Portable path conversion for Target, Working Directory and custom Icon
- Dynamic runtime input with {input}, raw and UTF-8 URL-encoded modes
- Custom shortcut icons with optional asynchronous Launcher result icon rendering
- Shortcut Manager local filtering and full keyword/alias conflict detection
- Legacy pause/pin values normalized out of the active user-facing shortcut workflow
- Context-sensitive Launcher result actions plus Shortcut Manager row/blank-space context menus
- Add-as-shortcut workflow with pre-filled Shortcut Editor data for discovered results
- Managed Everything Bootstrap: local-first reuse/start plus user-confirmed official portable download, SHA-256 verification and IPC readiness
- alpha.8.1 fixes the real-Windows verified archive handoff by promoting .zip.download to .zip only after SHA-256 succeeds
- alpha.8.2 completes the managed runtime path with Everything Service-backed NTFS indexing and a headless/no-tray managed client
- settings schemaVersion 6, commands schemaVersion 2 and TSV v3 remain compatibility baselines through alpha.8.2

Planned next:

- v0.7.0-beta.1 feature freeze and shortcut-workflow hardening
- Upgrade/import/export regression matrix for commands schema 1 -> 2 and TSV v1/v2/v3
- Real Windows desktop validation for Runtime Input, Path Conversion, custom icons, filtering, DPI and compatibility behavior
- Final UI/visual/performance consolidation remains deferred until the functional surface is stable

## v0.8.x - Distribution & extensibility

Planned:

- Public plugin/provider API
- Update-channel settings
- Auto updater
- Signed release pipeline
- Migration assistant and diagnostics

## v1.0 - Stable classic launcher

Target:

- Stable Classic and Classic Dark experiences
- Mature provider/search architecture
- Reliable migration and recovery
- Signed releases and updater
- Long-term compatibility baseline
