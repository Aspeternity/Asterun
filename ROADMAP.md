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

Current phase:

- Multi-source discovery from Start Menu, App Paths, PATH and AppsFolder
- UWP / MSIX / Microsoft Store application discovery
- Cross-provider de-duplication with user shortcuts authoritative
- Persistent provider cache and non-blocking background refresh
- Provider Registry with stable IDs and source controls
- Per-provider cache and provider-level refresh failure isolation

Planned before v0.4 beta:

- Incremental / change-driven provider refresh
- Refresh debounce and provider scheduling
- Provider status and diagnostics refinement
- Discovery and de-duplication regression coverage
- Windows 10 / Windows 11 compatibility verification

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
