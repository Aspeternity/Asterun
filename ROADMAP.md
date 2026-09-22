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

Completed in v0.7.0:

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
- alpha.8.3 closes managed-client lifecycle: app exit/source disable stops only ALTRun Next's owned Everything client while retaining the Windows service
- alpha.8.4 repairs stale persistent Everything Service paths after the portable ALTRun Next folder is moved/re-extracted
- alpha.9 adds native Stable / Development update channels, SHA-256 verified staged download and transactional helper-based apply/rollback
- alpha.9.2 adds portable native `Update.exe` / `Uninstall.exe` lifecycle ownership
- alpha.9.4 freezes the Managed Everything provider lifecycle after real Windows validation
- beta.1 freezes settings schemaVersion 7, commands schemaVersion 2, usage schemaVersion 1, provider-cache schemaVersion 2 and TSV v3
- beta.1 adds the commands schema 1 -> 2 / TSV v1-v2-v3 compatibility matrix, prerelease update-order gates and packaged v0.7 Beta desktop-validation checklist
- beta.2 fixes real-Windows Shortcut Editor dynamic-layout repaint corruption and the unlabeled Working Directory browse control without changing frozen contracts

- beta.12 completes Provider-to-shortcut promotion and stable two-stage Provider de-duplication
- rc.1 freezes the validated v0.7 runtime and final upgrade/release contracts
- v0.7.0 Stable promotes the validated RC runtime with no new runtime feature

## v0.8.x - UI / UX refinement & product polish

In progress:

- v0.8.0-alpha.1 establishes shared UiTheme / UiMetrics / UiTypography foundations without intentional visual redesign
- Alpha 1 preserves Classic 420/16/10 and Modern Compact 620/32/9 launcher geometry through regression-tested shared metrics
- Alpha 1 removes the unreachable legacy Settings Command editor after Shortcut Manager / Editor became the authoritative workflow
- v0.8.0-alpha.2 redesigns Settings with compact cards/rows, unified toggles/buttons, the final navigation order, concise Provider status and persisted Launcher/Settings placement modes
- Alpha 2 advances settings schemaVersion to 8 only for window-placement preferences/state; existing v0.7 functional contracts remain frozen
- v0.8.0-alpha.2.1 stabilizes real-Windows Settings rendering, replaces transparent STATIC repaint behavior, compacts the information hierarchy and adopts the two-line ALTRun / Next sidebar brand without changing persisted behavior
- v0.8.0-alpha.2.2 locks Settings to a fixed compact window, moves General to a single-column 560px card system, restores placement combo choices, sharpens toggle rendering and removes the development-only Diagnostics page/runtime probe stack from the product binary
- v0.8.0-alpha.2.3 shortens the fixed Settings viewport to 820×620, replaces dotted sidebar focus with a restrained native focus state and fixes rapid owner-drawn toggle clicks by handling BN_DOUBLECLICKED
- v0.8.0-alpha.2.4 completes Settings polish with compact selectors, grouped inline Hotkeys, debounced Search-source commits, balanced Data actions and removal of the manual legacy-AltRun import path while preserving automatic migration
- v0.8.0-alpha.2.5 applies the final Settings alignment hotfix: shorter General selectors, one Hotkey capture baseline, lower-right Reset-all placement and corrected Appearance combo vertical centering
- v0.8.0-alpha.2.6 clears native ComboBox focus on internal Settings clicks and shortens the two Appearance selectors to 160 logical pixels
- v0.8.0-alpha.2.7 introduces the compact About page, opt-in prerelease updates and one state-driven update action
- v0.8.0-alpha.2.8 completes About interaction polish with a lightweight GitHub link and fully decoupled update preferences: switches only change future automatic/manual check behavior and never trigger a check themselves
- v0.8.0-alpha.2.9 closes About alignment: version/GitHub metadata and update status/action rows share explicit visual centers without changing behavior
- v0.8.0-alpha.2.10 closes Hotkey-page typography/density: body-font action labels, 54px normal rows, conditional auxiliary expansion and Settings-consistent separators without changing hotkey behavior
- v0.8.0-alpha.2.11 polishes Hotkey auxiliary states: lightweight per-item Reset links, right-column helper text, measured expansion and separator-safe padding while preserving all hotkey behavior
- v0.8.0-alpha.2.12 closes Hotkey capture lifecycle: outside interaction/navigation/hide/deactivation cancel transient capture sessions
- v0.8.0-alpha.2.13 fixes the remaining Hotkey rendering regression by making auxiliary layout state explicit and committing text/visibility/layout through one parent-level redraw transaction
- v0.8.0-alpha.2.14 moves per-item Reset inline beside the capture control so modified bindings no longer expand rows or create fixed-viewport overflow; only transient/error status uses auxiliary height
- v0.8.0-alpha.3.1 starts Shortcut workflow consolidation with the native resizable Shortcut Manager: search/New hierarchy, dense ListView polish, selected-item action grouping, empty states, keyboard workflow and DPI hardening
- v0.8.0-alpha.3.2 closes the first Shortcut Manager real-Windows pass: 24px dense rows, restrained selection, non-accent New action and atomic resize redraw
- v0.8.0-alpha.3.3 closes Shortcut Manager reopen-state and search-edit issues while exposing the remaining native Header drag edge case
- v0.8.0-alpha.3.4 hardens live four-column Header resizing with minimum widths and an always-elastic Target column
- v0.8.0-alpha.3.5 follows with Shortcut Editor visual consolidation without changing Runtime Input / Path Conversion / Advanced Options semantics
- v0.8.0-alpha.3.6 is reserved for final real-Windows Shortcut workflow polish plus Path Conversion visual integration only
- Launcher visual polish remains last so keyboard-first behavior, density and performance stay protected
- UI performance consolidation remains a release requirement; v0.8 must not trade responsiveness for decoration

## v0.9.x - Distribution & extensibility

Planned:

- Public plugin/provider API after the first-party product surface is stable
- Signed release pipeline and signature enforcement in the existing updater
- Migration assistant and expanded diagnostics

## v1.0 - Stable classic launcher

Target:

- Stable Classic and Classic Dark experiences
- Mature provider/search architecture
- Reliable migration and recovery
- Signed releases and updater
- Long-term compatibility baseline
