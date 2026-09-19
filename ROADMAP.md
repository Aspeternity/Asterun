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

In progress:

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
- Classic launcher geometry remains frozen
- Remaining v0.4.1 work is real desktop validation and regression fixes before beta/RC promotion

## v0.5.x - Everything integration

Planned:

- Everything SDK / IPC provider
- File and folder search without blocking application search
- Provider-aware result types and ranking
- Search-source controls integrated with the existing Provider Registry
- Graceful fallback when Everything is unavailable

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
