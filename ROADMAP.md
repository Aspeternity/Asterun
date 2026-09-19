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

In progress:

- v0.5.0-alpha.1 establishes native Everything 1.4-compatible Unicode Query2 IPC over WM_COPYDATA
- Dedicated worker thread and hidden reply window keep Everything IPC off the launcher/UI thread
- 70 ms debounce, generation/reply-token stale discard, send timeout and reply timeout handling
- Portable Query2/LIST2 protocol parsing with Unicode and malformed-payload regression tests
- Real Win32 fake-Everything runtime smoke tests run without installing Everything in CI
- x64 and ARM64 production builds compile the IPC foundation without adding an Everything runtime DLL
- settings remains schemaVersion 2 in alpha.1; everything.filesystem and schemaVersion 3 are reserved for later v0.5 phases
- Alpha 2 will introduce LauncherResult / DynamicQueryProvider / everything.filesystem and actual File/Folder launcher results
- Later phases will add unified ranking, Settings/diagnostics and real-world 1.4/1.5 compatibility hardening

## v0.6.x - Smart actions & Windows integration

Planned:

- Calculator provider
- URL / web-search aliases
- Clipboard and text actions
- Explorer current-folder actions
- Open / Save dialog folder jump
- Total Commander integration
- Provider / action contracts suitable for future extensions

## v0.7.x - Distribution & extensibility

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
