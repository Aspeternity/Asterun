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
- v0.8.0-alpha.3.4 hardens four-column Header limits with minimum widths and an always-elastic Target column
- v0.8.0-alpha.3.5 closes Shortcut Manager interaction polish: guide-only Header dragging, overflow-safe one-shot commits and first-open mouse-monitor centering
- v0.8.0-alpha.3.6 closes Shortcut workflow naming/mode identity: compact Manager context labels and correct New/Edit editor titles without layout or behavior changes
- v0.8.0-alpha.3.7 closes top-level tool-window lifecycle cleanup and deletion-confirmation wording while preserving Manager geometry/column state
- v0.8.0-alpha.3.8 introduces recreated-Settings placement hardening plus update-status reconciliation
- v0.8.0-alpha.3.9 explored explicit creation-monitor anchoring after the first placement regression remained visible on real Windows
- v0.8.0-alpha.3.10 fixes the root lifecycle regression by restoring alpha.3.6's hidden first ShowWindow call while keeping destroy-on-close
- v0.8.0-alpha.3.11 closes the remaining tray About entry-path placement divergence so Settings/About share one top-level show lifecycle
- v0.8.0-alpha.3.12 consolidates Shortcut Editor layout/visual hierarchy while keeping shortcut model, Runtime Input and Advanced semantics frozen
- v0.8.0-alpha.3.13 polishes the Editor after real-Windows review: removes redundant Icon Auto, modernizes native file/folder pickers and refines the Advanced section header
- v0.8.0-alpha.3.14 through alpha.3.22 continue real-Windows Settings/Shortcut workflow stabilization and compact visual refinement without schema changes
- v0.8.0-alpha.3.23 closes Shortcut Editor compact-width/content-fit controls and hardens update-check cancellation/timeouts after the recurring Checking-state report
- v0.8.0-alpha.3.24 closes the final real-Windows Shortcut Editor density pass: 590px width, content-fit ComboBox breathing room, compact inline labels and neutral Advanced hierarchy
- v0.8.0-alpha.3.25 attempted the Shortcut workflow focus/column closeout with mouse-origin Advanced focus handling and a 16/24/14/46 Manager target layout
- v0.8.0-alpha.3.26 corrects the real-Windows interaction/state regressions: direct native Advanced clicks with no dotted renderer focus, content-fit administrator checkbox hit area, and width-only Header custom-state detection so 16/24/14/46 is actually applied
- v0.8.0-alpha.3.27 closes Manager column lifecycle: validated default-width minima plus reset-to-default on Manager reopen instead of persisting temporary drag widths
- v0.8.0-alpha.3.28 unifies Launcher/Settings/Shortcut Manager placement, makes Manager size ephemeral, persists Manager last-position coordinates only, and advances Settings schema to 9
- v0.8.0-alpha.3.29 hardens the unified placement implementation after real-Windows validation: shared top/center geometry plus pre-resolved/cloaked Shortcut Manager first-frame presentation
- v0.8.0-alpha.3.30 systemically closes custom top-level HWND presentation: one shared first-frame/teardown policy across Launcher, Settings, Manager, Editor and Path Conversion, with no real custom window born at CW_USEDEFAULT
- v0.8.0-alpha.3.31 redesigns Path Conversion: compact mode cards, responsive path columns, explicit empty state, live selection/apply feedback and no redundant Close action
- v0.8.0-alpha.3.32 closes the first Path Conversion visual-polish pass with flat buttons/header, lighter result framing and a balanced empty state
- v0.8.0-alpha.3.33 performs the Path Conversion visual closeout pass with pixel-raster selector, 1px card emphasis, softer Header separators and slightly roomier result rows
- v0.8.0-alpha.3.34 replaces the still-unclear selector with DPI-bucketed 4×4 coverage antialiasing rendered directly to final device pixels
- v0.8.0-alpha.3.35 closes Path Conversion result-row alignment by returning row height/checkbox placement to native Explorer ListView metrics and removing redundant field-label indentation; the validated alpha.3.34 selector stays frozen
- v0.8.0-alpha.3.36 systemically hardens update-status delivery with a message-only HWND dispatcher plus active-only Settings self-reconciliation, removing the shared thread-message loss mode
- v0.8.0-alpha.3.37 attempted a hybrid transparent native-state/custom-rendered checkbox, but real-Windows validation exposed a black state-image artifact
- v0.8.0-alpha.3.38 removes native checkbox/state-image ownership entirely and moves selection, hit-testing, keyboard toggling and first-column checkbox/label rendering into the Path Conversion row model
- v0.8.0-alpha.3.39 hardens pointer reliability: Path Conversion uses the whole first field cell as the checkbox target, while Shortcut Editor stops stealing focus from interactive child controls during WM_PARENTNOTIFY
- v0.8.0-alpha.3.40 normalizes rapid toggle clicks: Path Conversion handles NM_CLICK + NM_DBLCLK and Shortcut Editor Advanced handles BN_CLICKED + BN_DOUBLECLICKED, while one-shot actions remain single-fire
- v0.8.0-alpha.4.1 starts Launcher refinement with Classic typography/readability: 10 pt Classic body text plus a vertically centered 18 px native Edit inside the unchanged 22 px input strip
- v0.8.0-alpha.4.2 source-matches Classic typography: 12 pt primary input/result/title text, 10 pt auxiliary hint/preview text, DEFAULT_QUALITY Classic GDI rendering and the native Edit restored to the full 22 px strip height
- v0.8.0-alpha.4.3 uses the original authorized Classic shortcut/close glyph bitmaps, restores their original corner geometry, and raises Hint/Preview auxiliary typography to 11 pt after real-Windows comparison
- v0.8.0-alpha.4.4 establishes source parity from the original DFM/source: authorized BG.jpg skin, borderless 420×250 geometry, 240 alpha/12px round region, exact control coordinates, -16/-13 LOGFONT metrics, native-style fixed-format result text and original Classic colors
- v0.8.0-alpha.4.5 begins Classic+: full-opacity Classic color fidelity, owner-drawn query-aware Hint overlay, and fixed 23/230 logical result columns with per-column ellipsis instead of inherited string-padding misalignment
- v0.8.0-alpha.4.6 removes the Classic Hint architecture completely, corrects the ListBox to 404×164, and replaces hard-clipped command text with lightweight native DT_PATH_ELLIPSIS rendering
- v0.8.0-alpha.4.7 makes Classic a lean visual skin: native BMP background loading, no runtime GDI+ JPEG decode, one cached bitmap DC, and allocation-free Classic row backgrounds/separators
- v0.8.0-alpha.4.8 performs the Classic DPI audit: one cached constexpr geometry contract drives 100%/125%/150%/200% layout, row/divider/title/glyph metrics are regression-tested, and real-Windows Per-Monitor V2 visual validation remains mandatory before changing bitmap scaling policy
- v0.8.0-alpha.4.9 keeps the original Classic Logo/X design while adding deterministic alpha-aware 25/31/38/44/50px DPI resource tiers; 100% remains byte-identical and standard 125/150/175/200% no longer enlarge the 25px bitmap at runtime
- v0.8.0-alpha.5.1 cleans the Settings product model: three-state startup behavior, opt-in SendTo integration, intrinsic launcher lifecycle/search semantics, two new hotkey actions, and schema-10 removal of pseudo-settings while retaining all window placement preferences
- v0.8.0-alpha.5.2 fixes real-desktop Settings UX regressions: Shortcut Manager Alt+S becomes a real global hotkey, the Hotkeys page gains independent scrolling with a fixed reset action, all Settings dropdowns share one themed native ComboBox path, and startup notification copy is simplified
- v0.8.0-alpha.5.3 polishes that Settings path after real-Windows review: ComboBoxes become content-sized continuous surfaces with clearer native-lightweight interaction feedback, and Hotkeys clipping now preserves business visibility so hidden status HWNDs cannot reappear as empty card strips
- v0.8.0-alpha.5.4 promotes the approved ComboBox treatment into one shared native UI component and migrates Shortcut Editor Target type / Runtime input to it, leaving no direct old-style ComboBox creation in Settings or Shortcut Editor
- v0.8.0-alpha.5.5 adds one shared Next ListView visual foundation for Shortcut Manager and Path Conversion: semibold Header, 30-logical-pixel dense rows, subtle hover/selection, horizontal separators and light framing while preserving native scrolling and all existing column/selection behavior
- v0.8.0-alpha.5.6 turns that foundation into a real Next Table Surface: the native Header remains only as the interaction/resize engine while a shared 34-logical-pixel custom surface removes permanent grid dividers and classic Header chrome
- v0.8.0-alpha.5.7 polishes Table interaction: wider divider hit zones, explicit resize cursor/hover feedback, shared drag guide and deferred one-shot column commits for both Shortcut Manager and Path Conversion
- v0.8.0-alpha.5.8 replaces the paint-based drag guide with a dedicated overlay window and suppresses the native tracker, removing real-Windows drag trails while preserving one-shot column commits
- v0.8.0-alpha.5.9 unifies Table resize ownership inside UiListView: native Header sizing/HDN tracking is cut off, shared capture/preview/clamp/elastic commit becomes the only resize state machine, and consumer tracking code is removed
- v0.8.0-alpha.5.10 isolates the resize guide into an owned layered popup so guide motion is composed independently and cannot damage owner-drawn group text or leave trails in the ListView body
- v0.8.0-alpha.5.11 removes resize-preview HWNDs entirely and renders drag feedback only inside the Header paint transaction, structurally isolating owner-drawn rows and the empty ListView body from resize feedback
- v0.8.0-alpha.5.12 makes the actual multi-column ListView width commit atomic: redraw is paused across all dragged/elastic updates and the complete table is synchronously repainted once, eliminating stale owner-draw/back-buffer pixels after release
- v0.8.0-alpha.5.13 closes Classic keyboard arbitration: result navigation wraps, numeric Quick Launch becomes semantic+temporal with a 90ms deferred ambiguity window, consumed digits cannot leak into WM_CHAR, and explicit Ctrl/Alt+digit remains deterministic
- v0.8.0-alpha.5.14 hardens search relevance at the policy boundary: short ASCII fuzzy is gated, initials are exact/prefix-only, ordinary target paths leave lexical search, multi-token matching becomes strict with Hybrid Pinyin preserved, and Start Menu entries gain content hygiene without changing Provider canonicalization
- v0.8.0-alpha.5.15 adds the missing classification/admission layer and a single RelevancePolicy owner shared by static Search and Everything, preventing helper/CLI/system noise from entering casual queries before structured ranking is applied
- v0.8.0-alpha.5.16 makes provider admission positive and target-aware: Start Menu shortcuts are resolved before indexing, AppsFolder web/non-app entries are rejected, App Paths PE roles are inspected, and only meaningful launch candidates reach the search/ranking layer
- v0.8.0-alpha.5.17 hardens that admission boundary against false negatives by preserving real existing `.exe` targets as ExecutableUnknown when PE subsystem inspection is inconclusive, while keeping all content-hygiene rejection rules intact
- v0.8.0-alpha.5.18 makes static-provider publication stateful and observable: incomplete startup snapshots stay Building and hidden until one atomic publish, while bounded admission diagnostics preserve why candidates were rejected
- v0.8.0-alpha.5.19 freezes admission behavior and adds a temporary exact legacy-vs-current launch-target trace so the TeamSpeak 6 false-negative can be proven before retaining or removing ExecutableUnknown
- v0.8.0-alpha.5.20 removes the disproven ExecutableUnknown fallback and temporary probe, rebuilds generated cache under strict admission, and locks empty-cache startup behind the alpha.5.18 Building gate
- v0.8.0-alpha.5.21 establishes an Intelligent Launch Catalog: canonical identity owns provider dedupe, activation semantics distinguish AUMID from ShellExecute, and structural/role evidence prevents internal/product-info surfaces from entering the normal index
- v0.8.0-alpha.5.22 moves provider monitoring to event-driven Start Menu/registry/Shell notifications with a low-frequency safety reconciliation, eliminating the prior 5-second idle scan loop
- v0.8.0-alpha.5.23 removes full LISTBOX reset/erase from live query rebuilds so frozen Classic geometry can update results without visible typing flicker
- v0.8.0-alpha.5.24 isolates Classic parent/title/preview/result repaint domains, removes redundant no-result redraws, and clips parent painting away from native child controls
- v0.8.0-alpha.5.25 introduces provider-neutral launch-role evidence, cached executable metadata, conservative product grouping and schema-9 persisted role decisions without yet changing query admission/ranking
- v0.8.0-alpha.5.26 consumes schema-9 CatalogVisibility in SearchEngine: family-name queries keep normal applications, StrongMatchOnly entries require distinctive intent, Hidden generated entries stay suppressed, and user shortcuts remain authoritative
- v0.8.0-alpha.5.27 calibrates role evidence and catalog context: high-information role phrases reach Medium confidence, weak auxiliary cues can be corroborated only by a related group with a clear primary app, AlternateLaunch models convenience/variant entry points, semantic distinctive tokens cover contiguous CJK intent, and schema-10 rebuilds stale role decisions without changing SearchEngine
- v0.8.0-alpha.5.28 restores distinctive-intent isolation for compact Windows titles: family/version prefixes are removed even when joined to CJK or no-space role text, schema-11 rebuilds contaminated alpha.5.27 tokens, and StrongMatchOnly can no longer be reopened by a short shared-family prefix
- v0.8.0-alpha.5.29 hardens the evidence pipeline end to end: Start Menu suite structure becomes first-class family evidence, strong semantic titles outrank generic ProductName identity, restrictive query intent is separated from residual identity tokens, publication repairs explicit title roles before group context, and schema-12 rebuilds ProductName-centric state
- v0.8.0-alpha.5.30 closes a distribution false-green: dev-latest publication is non-cancellable, repairs orphan Draft releases explicitly, uploads the manifest last, and CI anonymously verifies the exact public Release/manifest endpoints before green
- v0.8.0-alpha.5.31 completes Alternate/SuiteUtility roles: canonical-target/family/title-base evidence can corroborate alternate variants across catalog locations, weak Sync/Scheduler/Automation/Maintenance cues require suite context, diagnostic metadata evidence expands generically, and schema-13 rebuilds role/token state
- v0.8.0-alpha.5.32 completes generic residual management evidence: network monitoring, license/licensing management and user-facing service-management surfaces require suite-primary corroboration, can repair misleading component metadata, and schema-14 rebuilds role/token state without guessing ambiguous product vocabulary
- v0.8.0-alpha.5.33 separates short-query precision from Catalog roles: 1-2 ASCII queries no longer use generic later-word BoundaryPrefix recall, three-character boundary search remains intact, Start Menu classification also consumes resolved-target tool structure, and schema-15 rebuilds cached surface classes
- v0.8.0-alpha.5.34 completes Windows shell launch-surface evidence: Shell PIDL parsing identity is preserved beside GetPath(), trusted control/MMC/Control_RunDLL/ms-settings/namespace activations refine Start Menu entries structurally, path-constrained broker recognition avoids leaf-name heuristics, and schema-16 rebuilds cached surface classes
- v0.8.0-alpha.5.35 adds conservative suite-member topology: a child companion becomes SuiteSubordinate only when both family-stripped display identity and resolved executable stem independently extend a normal companion in the same catalog context; delta-only intent keeps child surfaces reachable without polluting family/parent queries, and schema-17 rebuilds cached role/token state
- v0.8.0-alpha.5.36 corrects the real target-evidence layer: Windows Installer advertised shortcuts resolve side-effect-free to installed component paths for catalog evidence while launch still uses the original .lnk; suite corroboration accepts executable-stem or nearby install-directory topology, stable display-title identity survives metadata changes, and schema-18 rebuilds proxy-based cached state
- next relevance step: validate alpha.5.36 against real advertised suites first; only after target identity and catalog admission are clean should usage ranking be allowed to reorder equivalent-intent candidates
- Classic ALTRun is polished to maturity first; Modern Compact refinement follows only after Classic real-Windows closeout
- Launcher polish remains keyboard-first and density/performance protected
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
