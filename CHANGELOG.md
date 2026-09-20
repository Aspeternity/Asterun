# Changelog

## 0.7.0-alpha.7

- Added context-sensitive Launcher result menus instead of a single copied classic menu.
- User shortcuts expose Run, Edit shortcut, filesystem reveal when applicable, Copy target and destructive Delete at the bottom.
- Discovered Application/File/Folder results can be added as pre-filled user shortcuts; Everything folders also expose current Explorer/Total Commander navigation when activation context is available.
- URL and Smart Action context menus remain minimal and action-specific.
- Added Shortcut Manager row menus with Edit, Test, optional File Explorer reveal, Copy target and Delete; blank-list context offers New shortcut only.
- Added Windows mouse right-click plus keyboard context-menu invocation support.
- Added pre-filled Shortcut Editor creation while leaving the user-facing Keywords field empty.
- Added platform-independent ContextActions policy and regression coverage for user/discovered/folder/web/packaged-result behavior and shortcut seeding.
- Added a Windows ShellActions helper for resolving portable/PATH-backed filesystem targets and revealing them with File Explorer.
- Kept settings schemaVersion 6, commands schemaVersion 2, TSV v3, usage schemaVersion 1 and provider-cache schemaVersion 2 unchanged.
- Updated Windows fixed FileVersion/ProductVersion to 0.7.0.70.

## 0.7.0-alpha.6

- Completed the Shortcut Manager workflow with a local filter across shortcut keywords, aliases, names and targets.
- Show the complete comma-separated shortcut keyword set in the manager instead of exposing only the persisted primary keyword.
- Replaced primary-keyword-only conflict warnings with case-insensitive conflict detection across every keyword and alias, including the exact conflicting token and shortcut name.
- Removed Pause shortcut and Pinned from the Shortcut Editor because temporary disable/manual ordering are not part of the product workflow.
- Normalized legacy user-command enabled/pinned values to active/non-pinned during load, create, update and TSV import while retaining the schemaVersion 2 and TSV v3 compatibility fields.
- Added regression coverage for alias conflicts, manager filtering and legacy pause/pin normalization.
- Updated the roadmap to reflect the actual v0.7 Shortcut / Launcher workflow line and moved distribution/extensibility work to a later phase.
- Kept settings schemaVersion 6, commands schemaVersion 2, TSV v3, usage schemaVersion 1 and provider-cache schemaVersion 2 unchanged.
- Updated Windows fixed FileVersion/ProductVersion to 0.7.0.60.

## 0.7.0-alpha.5.2

- Moved result-icon file/PATH/Shell resolution completely off the Launcher UI/WM_DRAWITEM thread.
- Added one lazy background icon worker with a protected job/completion queue and WM_APP completion notification.
- Added search-generation + icon-epoch stamps so stale icon completions are destroyed instead of repainting or populating the current query.
- Cancelled queued obsolete icon jobs as the search generation advances while allowing already-running work to finish safely in the background.
- Replaced per-query icon-cache destruction with a 96-entry cross-query LRU cache keyed by icon source + requested pixel size.
- Added negative-cache entries for failed icon resolutions so unresolved sources do not repeatedly hit Windows Shell APIs.
- Invalidates only result rows that use a newly completed icon instead of repainting the whole Launcher.
- Preserved the default-off zero-resolution path from alpha.5.1; the worker is created lazily only after an enabled icon cache miss.
- Added platform-independent icon pipeline policy tests for cache-key sizing, stale generations, epochs, disabled mode and bounded cache capacity.
- Kept settings schemaVersion 6, commands schemaVersion 2, TSV v3, usage schemaVersion 1 and provider-cache schemaVersion 2 unchanged.
- Updated Windows fixed FileVersion/ProductVersion to 0.7.0.52.

## 0.7.0-alpha.5.1

- Added Appearance -> Show search result icons as a persisted, default-off preference.
- Moved result-icon presentation from mandatory behavior to an opt-in visual feature while retaining all per-shortcut custom icon metadata.
- Gated icon handling before portable-path resolution, PATH lookup and Windows Shell/icon APIs so the disabled path performs no icon resolution or caching.
- Restored the pre-icon text geometry in both Classic and Modern result rows when icons are disabled instead of leaving an empty icon gutter.
- Added immediate runtime switching: disabling icons clears/destroys the current icon cache and redraws the list; enabling redraws and resolves icons only as rows are painted.
- Advanced settings.json from schemaVersion 5 to 6 with `appearance.showResultIcons=false` as the migration/default value and schema-5 downgrade read-only protection.
- Added settings persistence, schema-5 -> 6 upgrade, downgrade, clean-install and packaged-runtime migration coverage.
- Kept commands schemaVersion 2, TSV v3, usage schemaVersion 1 and provider-cache schemaVersion 2 unchanged.
- Deferred asynchronous icon loading to the final performance-polish phase.
- Updated Windows fixed FileVersion/ProductVersion to 0.7.0.51.

## 0.7.0-alpha.5

- Exposed the existing user-command icon field in the standalone Shortcut Editor with Choose and Auto/reset actions.
- Added custom icon source support for `.ico`, `.exe`, `.dll` and `.lnk` files while keeping blank as target-derived Auto.
- Propagated command icon metadata into normal, runtime-input and legacy web-search Launcher results.
- Added Classic/Modern launcher icon rendering with shell/executable/icon-file resolution and current-result-set handle cleanup.
- Added a context-sensitive Test input field for dynamic shortcuts and routed editor tests through the same runtime-input execution path as Launcher execution.
- Added test-time validation for missing runtime placeholders and empty dynamic test input.
- Extended Path Conversion to include custom icon paths in the same atomic Target/Working Directory batch.
- Advanced shortcut TSV interchange to v3 with an appended `icon` column while retaining older v1/v2 import compatibility.
- Added custom-icon JSON/TSV round-trip, runtime/web result icon propagation and atomic path-update regression coverage.
- Kept settings schemaVersion 5, commands schemaVersion 2, usage schemaVersion 1 and provider-cache schemaVersion 2.
- Updated Windows fixed FileVersion/ProductVersion to 0.7.0.50.

## 0.7.0-alpha.4

- Added first-class dynamic Runtime input for user shortcuts with None, Pass through and UTF-8 URL-encoded modes.
- Added the canonical `{input}` placeholder across target, fixed arguments and working directory; retained legacy `{query}` as a compatibility alias.
- Added exact case-insensitive keyword/alias + argument-tail matching so runtime arguments are not treated as fuzzy-search terms.
- Application and Command line shortcuts can auto-append runtime input after fixed arguments when no placeholder is present; URL and Folder shortcuts require explicit placement.
- Routed runtime input through LauncherAction payload into the normal command launch path, after `{folder}` substitution and before portable-path/environment resolution.
- Added Runtime input controls, contextual guidance and invalid-template validation to the Shortcut Editor.
- Advanced commands.json to schemaVersion 2 with persisted `runtimeInputMode`, atomic schema-1 migration and downgrade read-only protection.
- Automatically migrates legacy schema-1 URL `{query}` shortcuts to UTF-8 URL-encoded runtime input.
- Advanced shortcut TSV export/examples to v2 with an appended runtimeInputMode column while retaining older TSV import compatibility.
- Added RuntimeInput, commands-schema migration and legacy-WebAction ownership regression tests.
- Kept settings schemaVersion 5, usage schemaVersion 1 and provider-cache schemaVersion 2.
- Updated Windows fixed FileVersion/ProductVersion to 0.7.0.40.

## 0.7.0-alpha.3.1

- Reworked New/Edit Shortcut around user tasks instead of exposing the internal Command structure directly.
- Merged primary keyword + aliases into one comma-separated Keywords field while preserving the existing persisted primary/alias model.
- Made Name optional with automatic target-based suggestion and manual override.
- Added separate File and Folder target pickers.
- Added Auto detect as the default command-type mode while retaining explicit Application, URL, Folder and Command line overrides.
- Moved fixed arguments, working directory, administrator, pinned and pause controls behind a collapsible Advanced section.
- Reframed enabled state as the user-facing Pause this shortcut option without changing the stored enabled boolean.
- Added automatic target-directory working directory for user Application/Command line shortcuts when the advanced working-directory field is blank.
- Added ShortcutEditorModel + regression tests for keyword parsing, type inference, title suggestion and automatic working-directory rules.
- Kept every persisted schema unchanged; dynamic runtime input/parameter encoding remains deferred to the next feature version.
- Updated Windows fixed FileVersion/ProductVersion to 0.7.0.31.

## 0.7.0-alpha.3

- Reorganized New/Edit Shortcut into compact Shortcut and Launch options groups instead of the previous oversized flat form.
- Reordered the primary workflow to keyword, name, aliases, type and target before launch-specific fields.
- Fixed the Win32 command-type ComboBox dropdown height so all four existing types are visible: Application, URL, Folder and Command line.
- Added a four-item minimum-visible dropdown contract and immediate type-change handling.
- Disabled the local target browse button for URL commands while retaining browsing for other command types.
- Added release-contract coverage for the grouped editor structure and four command-type options.
- Kept all persisted schemas and Provider/Pinyin/search behavior unchanged.
- Updated Windows fixed FileVersion/ProductVersion to 0.7.0.30.

## 0.7.0-alpha.2.6

- Fixed the blank Pinyin search row in General -> Search behavior.
- Routed `kIdPinyinSearch` through the Settings owner-draw path and added its title/description to `DrawGeneralToggle`.
- Added a release-contract regression gate for both owner-draw routing and Pinyin-row content.
- Kept Provider storage deduplication, Pinyin runtime behavior and settings schemaVersion 5 unchanged.
- Updated Windows fixed FileVersion/ProductVersion to 0.7.0.26.

## 0.7.0-alpha.2.5

- Removed the long-lived raw Provider `providerCommands_` vector; Provider cache Commands now exist only during merge and the final searchable `commands_` vector remains resident.
- Added temporary `const Command*` Provider merge views so rebuilding the merged index does not create another full Provider copy.
- Preserved raw Provider count diagnostics as a scalar and kept Provider accepted/suppressed statistics, refresh behavior, command indexes and user override semantics unchanged.
- Added a default-on Pinyin search toggle to General -> Search behavior.
- Disabled Pinyin search now bypasses Hanzi-to-pinyin matching; turning it off also releases a loaded converter and Pinyin cache, while re-enabling stays lazy.
- Advanced settings.json from schemaVersion 4 to 5 for the persisted `behavior.pinyinSearch` preference with schema-4 -> 5 migration and schema-4 downgrade read-only protection.
- Added merge-view, Pinyin-disabled/unload/reload, settings persistence/migration and five-row Search behavior layout regression coverage.
- Kept commands/usage schemaVersion 1 and provider-cache schemaVersion 2.
- Updated Windows fixed FileVersion/ProductVersion to 0.7.0.25.

## 0.7.0-alpha.2.4

- Changed cpp-pinyin initialization from application-start eager construction to first-use construction inside the Pinyin search path.
- Fresh startup now reports Pinyin not loaded with an empty cache while retaining a side-effect-free dictionary availability check.
- Preserved ASCII pinyin lookup of Chinese commands, including `weixin -> 微信`, initials and hybrid pinyin matching.
- Added synchronization around lazy converter state and cache diagnostics so search and Diagnostics reads cannot race initialization/cache mutation.
- Added regression coverage for unloaded startup, first pinyin search loading, non-empty post-search cache, and missing-dictionary fallback.
- Kept Provider storage, Pinyin cache policy, search scoring/ranking and all persisted schemas unchanged for a clean memory A/B.
- Updated Windows fixed FileVersion/ProductVersion to 0.7.0.24.

## 0.7.0-alpha.2.3

- Added live Diagnostics-page process memory counters: Working Set, Peak Working Set and Private Bytes.
- Added runtime search/storage baseline counters for user commands, raw Provider commands and merged searchable commands.
- Added Pinyin converter loaded/ready state and cache-entry count diagnostics.
- Added Provider refresh and Provider monitor runtime-state diagnostics.
- Added a reusable Windows ProcessMemory platform layer plus process_memory_tests on Windows smoke/compatibility CI.
- Kept memory diagnostics passive: no EmptyWorkingSet, SetProcessWorkingSetSize or other active working-set trimming.
- Kept Pinyin initialization, Provider storage, search behavior and all persisted schemas unchanged.
- Updated Windows fixed FileVersion/ProductVersion to 0.7.0.23.

## 0.7.0-alpha.2.2

- Replaced the alpha.2.1 repeated-row presentation with true structural shortcut header rows.
- Added full-width custom-drawn shortcut headers using system colors and a semibold system font.
- Removed the redundant Shortcut data column; data rows now show Field, Current path, Converted and Status.
- Indented Target and Working Directory rows beneath each shortcut header while keeping their checkboxes independent.
- Prevented structural header rows from selection, checkbox state changes and path-application batches.
- Kept the current Common Controls version and all alpha.2 conversion/persistence semantics unchanged.
- Updated Windows fixed FileVersion/ProductVersion to 0.7.0.22.

## 0.7.0-alpha.2.1

- Grouped path-conversion preview rows by shortcut without changing the global Common Controls version.
- Show the shortcut label only on the first convertible field row; subsequent Target/Working Directory rows in the same shortcut group leave the Shortcut cell blank.
- Kept Target and Working Directory independently selectable.
- Updated the footer to report convertible shortcut count and convertible field count.
- Kept alpha.2 path-conversion semantics and all persisted schemas unchanged.
- Updated Windows fixed FileVersion/ProductVersion to 0.7.0.21.

## 0.7.0-alpha.2

- Removed Shortcut Manager Move Up / Move Down UI while preserving the internal sortOrder compatibility field.
- Added Path conversion preview/apply workflow for shortcut Target and Working Directory fields.
- Added portable conversion to nearby ALTRun Next-relative paths and known Windows environment-variable paths.
- Added reverse expansion from relative/environment-variable paths to current-machine absolute paths.
- Defined runtime resolution of structured relative Target paths and relative Working Directory paths against the ALTRunNext.exe directory while preserving bare shell-command lookup.
- Left Arguments, URL and UNC targets unchanged by path conversion.
- Added atomic multi-shortcut path updates with rollback on validation/save failure.
- Fixed blank Shortcut Manager list headers by assigning header text at column creation.
- Added Windows path portability tests and cross-platform atomic path-update tests to CI.
- Kept commands.json schemaVersion 1 and all v0.6 persisted/provider/Hotkey/Everything/Smart Actions contracts unchanged.
- Updated Windows fixed FileVersion/ProductVersion to 0.7.0.2.

## 0.7.0-alpha.1

- Promoted shortcuts from a Settings subsection into a standalone Shortcut Manager workflow.
- Added a dedicated Shortcut Manager window with add, edit, delete, test, move-up and move-down operations.
- Added a reusable Shortcut Editor dialog for creating/editing one user shortcut without depending on SettingsWindow.
- Added a system-tray Shortcut Manager entry and simplified the tray around launcher, shortcut management, settings, reload, about and exit.
- Removed the visible Shortcuts navigation/page from Settings and made General the default Settings page.
- Reordered Settings navigation to General, Hotkeys, Appearance, Search sources, Data, Diagnostics, About, placing Diagnostics immediately above About.
- Kept commands.json at schemaVersion 1; existing v0.6 shortcuts require no migration.
- Kept settings schemaVersion 4, commands/usage schemaVersion 1, provider-cache schemaVersion 2 and all v0.6 provider/Hotkey/Everything/Smart Actions contracts unchanged.
- Updated Windows fixed FileVersion/ProductVersion to 0.7.0.1.

## 0.6.0

- Promoted the validated v0.6.0-rc.1 contract to Stable with no user-facing runtime behavior changes.
- Shipped centralized customizable Hotkeys, Smart Actions, Explorer/Open-Save/Total Commander context navigation, {folder}/{query} templates, Web/URL and clipboard/text actions, optional Everything filesystem search and runtime Diagnostics.
- Kept upgrade/downgrade hardening and release gates from RC.1: clean install, schema 3 -> 4 migration, schema 4 compatibility, downgrade read-only protection, packaged schema 2 -> 4 runtime migration, Windows 10 baseline and desktop/runtime smoke.
- Kept settings schemaVersion 4, commands/usage schemaVersion 1, provider-cache schemaVersion 2, frozen provider defaults/IDs, five Hotkey Registry IDs, Everything Query2/WM_COPYDATA contract, Smart Actions semantics, Diagnostics routing and Classic 420/16/10 geometry.
- Updated Windows fixed FileVersion/ProductVersion to 0.6.0.300.
- Published v0.6.0 as the new Stable download line, replacing v0.5.0 in the README download section.

## 0.6.0-rc.1

- Entered v0.6 release freeze; no new user-facing feature, provider, Hotkey action or persisted schema is introduced.
- Added `upgrade_matrix_tests` as a release gate for clean install defaults, v0.5.0/alpha.5 schema 3 -> 4 migration, alpha.6.1/beta.1/beta.2 schema 4 -> 4 compatibility and downgrade read-only protection.
- Added representative historical upgrade fixtures with custom providers, behavior, appearance and Hotkey Registry bindings.
- Added explicit regression coverage for the schema-3 Ctrl+Enter migration collision in the versioned upgrade matrix.
- Hardened packaged x64 runtime smoke to require successful schema 2 -> 4 migration, all five frozen Hotkey Registry action IDs and Everything opt-in preservation.
- Added `V0.6_RC_VALIDATION.md` to release packages and made it part of the package allowlist.
- RC release-contract now freezes settings/commands/usage/provider-cache schemas, provider IDs/defaults, Hotkey IDs, Everything Query2 contract, Smart Actions evaluation, Diagnostics owner-draw routing and Classic 420/16/10 geometry.
- Updated Windows fixed version to 0.6.0.200.

## 0.6.0-beta.2

- Renamed the Settings Smart Actions runtime page from Actions / 操作 to Diagnostics / 诊断 so the label matches the page's actual purpose.
- Fixed the blank selected sidebar item by adding the Diagnostics owner-draw control ID to the WM_DRAWITEM navigation dispatch path.
- Renamed the page/router identifiers from Actions to Diagnostics to keep internal UI terminology aligned with the visible product language.
- Added release-contract checks that require the Diagnostics ID in both DrawNavigationButton and WM_DRAWITEM dispatch, preventing this blank-label regression.
- Kept v0.6 feature-freeze contracts unchanged: settings schemaVersion 4, provider-cache schemaVersion 2, frozen provider defaults, Hotkey Registry IDs and Smart Actions execution behavior.
- Updated Windows fixed version to 0.6.0.101.

## 0.6.0-beta.1

- Entered v0.6 feature freeze: no new provider, Smart Action family or persisted behavior toggle is introduced in beta.1.
- Added the ActionEvaluation contract so contextual Smart Actions expose availability plus a concrete unavailable reason while preserving existing execution/fallback semantics.
- Added a dedicated Actions / 操作 Settings page for Smart Actions capability and runtime diagnostics.
- Surface the last captured Windows activation context, Explorer/Total Commander folder context, TC active panel, file-dialog state, {folder} availability and current-file-manager navigation availability.
- Surface Everything IPC availability/fallback state on the Actions diagnostics page without changing the external optional Everything contract.
- Keep the most recent activation snapshot in process memory for diagnostics after the launcher hides; it is not persisted.
- Clarified Hotkey runtime status as Windows-global registration vs launcher-local readiness.
- Expanded Launcher Action Policy regression coverage for file-dialog navigation, unsupported file-manager contexts, non-folder navigation intents, empty copy targets and invalid action targets.
- Expanded Windows runtime smoke coverage to Total Commander right-panel selection plus UNC, spaces and Unicode paths.
- Kept settings schemaVersion 4, commands/usage schemaVersion 1, provider-cache schemaVersion 2, frozen provider defaults, Hotkey Registry IDs and Classic geometry 420/16/10 unchanged.
- Updated Windows fixed version to 0.6.0.100.

## 0.6.0-alpha.6.1

- Hardened schema-3 -> 4 Hotkey Registry migration when an existing user global activation chord collides with a newly introduced launcher-local default.
- Preserve established primary/auxiliary global bindings; disable the conflicting new optional local action instead of creating duplicate enabled chords.
- Seed schema-4 Registry globals from the legacy compatibility mirror before applying hotkeys.bindings, improving recovery from partial schema-4 documents.
- Added Config Core regression coverage for a legacy primary F2 binding colliding with the new Open Settings default.
- Kept settings schemaVersion 4, stable Hotkey Registry action IDs, commands/usage schemaVersion 1, provider-cache schemaVersion 2, provider defaults and Classic geometry unchanged.
- Updated Windows fixed version to 0.6.0.61.

## 0.6.0-alpha.6

- Added a centralized Hotkey Registry with stable action IDs, scope metadata, defaults and validation.
- Registered launcher.activate, launcher.activateSecondary, launcher.openSettings, result.navigateCurrentFileManager and result.copySelectedTarget.
- Replaced hard-coded F2, Ctrl+Enter and Ctrl+Shift+C launcher handling with registry-driven dispatch.
- Added a dedicated Hotkeys / 快捷键 Settings page with one centralized action list and editor.
- Added press-to-capture shortcut editing, Esc cancellation, optional enable/disable, reset-current and reset-all actions.
- Added registry-wide duplicate binding detection plus reserved-key protection for fixed launcher navigation.
- Kept primary activation mandatory and modifier-protected.
- Preserved transactional Windows RegisterHotKey behavior: failed global rebinds restore the previous working binding.
- Upgraded settings.json to schemaVersion 4 and added hotkeys.bindings.
- Migrated schema-3 primary/auxiliary bindings into the registry while supplying published defaults for launcher-local actions.
- Retained the legacy hotkey object as a compatibility mirror so alpha.5 downgrade reads known global fields but leaves schema 4 read-only and byte-identical.
- Added portable Hotkey Registry regression tests and extended current/Windows 10 CI lanes.
- Moved hotkey editing out of General Settings; General now focuses on launcher behavior, search and placement.
- Kept commands/usage schemaVersion 1, provider-cache schemaVersion 2, provider IDs/defaults and Classic geometry 420/16/10 unchanged.
- Updated Windows fixed version to 0.6.0.60.

## 0.6.0-alpha.5

- Added CopyText as a reusable Smart Action kind.
- Added runtime-only builtin.clipboard provider without changing Search Sources or persisted provider defaults.
- Added explicit copy / clip / 复制 text actions that copy the remainder of the launcher query instead of executing it.
- Added Ctrl+Shift+C to copy the currently selected result target/path/resolved URL while preserving normal Ctrl+C query-text behavior.
- Copy-selected execution is fail-safe: the copy intent never falls through to launching the selected command when no payload exists.
- Added Unicode Win32 clipboard writing through CF_UNICODETEXT with short retry handling for temporary clipboard contention.
- Clipboard actions do not read or persist previous clipboard contents.
- Added localized Copy text / copy-failure strings.
- Added portable clipboard-action regression tests, expanded launcher-action policy coverage and a Windows clipboard runtime smoke.
- Run the new portable/runtime tests on Windows current and Windows 10 API-baseline CI lanes.
- Kept settings schemaVersion 3, commands/usage schemaVersion 1, provider-cache schemaVersion 2, provider defaults and Classic geometry 420/16/10 unchanged.
- Updated Windows fixed version to 0.6.0.50.

## 0.6.0-alpha.4

- Added Total Commander 9+ as a runtime Activation Context without introducing a provider or hard dependency.
- Capture the exact foreground TTOTAL_CMD window, process ID, active panel and filesystem folder when available.
- Added NavigateTotalCommander to the Smart Action contract and generalized Ctrl+Enter intent to NavigateCurrentFileManager while preserving the previous NavigateCurrentExplorer alias.
- Ctrl+Enter on a Folder result now navigates the captured Total Commander active/source panel.
- Use Total Commander's WM_USER+50 active-panel/path-control queries and WM_COPYDATA CD protocol; Unicode target paths are sent as UTF-8 with BOM.
- Revalidate the exact captured TC window/process/active panel before navigation, so multiple instances and stale contexts are not guessed.
- Added {folder} templates for User Command Target, Arguments and Working Directory.
- Resolve {folder} from a real Explorer filesystem path or Total Commander active-panel filesystem path without mutating commands.json.
- Contextual commands are excluded from search when the activation context has no real filesystem folder; no empty, stale or guessed fallback is substituted.
- {folder} is resolved before {query} web-action generation, allowing deliberate combinations in URL commands.
- Added portable CommandTemplate regression tests and a fake-TOTAL_CMD Windows runtime smoke that validates active-panel capture and WM_COPYDATA navigation without installing Total Commander in CI.
- Kept settings schemaVersion 3, commands/usage schemaVersion 1, provider-cache schemaVersion 2, provider defaults and Classic geometry 420/16/10 unchanged.
- Updated Windows fixed version to 0.6.0.40.

## 0.6.0-alpha.3

- Added Windows Activation Context detection for standard Open / Save / folder-picker dialogs.
- Added NavigateFileDialog to the Smart Action contract.
- Folder results now navigate the captured file dialog on normal Enter/default execution instead of opening another Explorer window.
- Preserved Explorer semantics: Enter opens normally and Ctrl+Enter navigates the captured Explorer.
- Limited file-dialog recognition to #32770 roots that host SHELLDLL_DefView so ordinary dialogs are not mistaken for file pickers.
- Revalidate the captured dialog HWND and process ID immediately before navigation.
- File-dialog navigation uses the standard Ctrl+L address surface plus Unicode SendInput; it does not use the clipboard or overwrite the File name field.
- Refuse keyboard injection unless the captured dialog actually regains foreground, protecting against stale context and foreground/UIPI failures.
- File results remain unchanged; alpha 3 only adds Folder navigation.
- Extended Launcher Action Policy regression coverage for file-dialog/default and Ctrl+Enter behavior.
- Kept settings schemaVersion 3, commands/usage schemaVersion 1, provider-cache schemaVersion 2, provider defaults and Classic geometry 420/16/10 unchanged.
- Updated Windows fixed version to 0.6.0.30.

## 0.6.0-alpha.2.1

- Fixed Ctrl+Enter Explorer navigation when ALTRun Next is invoked from Home / 主文件夹, This PC, Quick access, Network or another virtual Shell namespace location.
- Changed Explorer source-context validity from "active Shell view plus filesystem source path" to "active Shell view"; only the destination Folder result still requires a filesystem path.
- Kept the source filesystem path as optional session-only diagnostic data when one exists.
- Preserved focused-view/visible-view/single-candidate selection and the no-guess ambiguity policy for Windows 11 tabs and multiple Explorer windows.
- Added release-contract protection so future changes cannot accidentally make Explorer source validity depend on explorerFolder again.
- Extended version tooling to support alpha hotfix versions such as alpha.2.1; Windows fixed version for this build is 0.6.0.21.
- Kept settings schemaVersion 3, commands/usage schemaVersion 1, provider-cache schemaVersion 2, provider defaults and Classic geometry 420/16/10 unchanged.

## 0.6.0-alpha.2

- Added per-launch Activation Context capture before ALTRun Next takes foreground focus.
- Added Windows Explorer context discovery through IShellWindows, IShellBrowser, active IShellView and filesystem PIDL resolution instead of title/address-bar scraping.
- Added conservative Explorer candidate selection using focused shell view, unique visible view, then single-candidate fallback; ambiguous multi-tab/multi-window states are never guessed.
- Added NavigateExplorer to the Smart Action contract and NavigateCurrentExplorer as an execution intent.
- Added Ctrl+Enter for Everything Folder results: navigate the captured Explorer to the selected folder.
- Kept Enter, double-click, numeric quick launch and single-result execution on their existing normal-open behavior.
- Made Ctrl+Enter fall back to normal folder opening when the launcher was invoked without an Explorer context.
- Refuse to redirect a different Explorer when the captured browser/view can no longer be resolved.
- Return focus to the captured Explorer after a successful navigation.
- Added portable action-policy and Explorer ambiguity-policy regression tests.
- Added Win32 Windows-context runtime smoke and run it on both current-Windows and Windows 10 API-baseline CI.
- Kept settings schemaVersion 3, commands/usage schemaVersion 1, provider-cache schemaVersion 2, provider defaults and Classic geometry 420/16/10 unchanged.
- Deferred {folder} command templates, Open/Save dialogs and Total Commander to later v0.6 phases.
- Updated Windows version metadata to 0.6.0-alpha.2 / 0.6.0.2.

## 0.6.0-alpha.1

- Added the v0.6 smart-action contract foundation with ResultKind::Action, OpenUrl and explicit action payloads.
- Added runtime-only provider ID builtin.web without changing Search Sources or persisted provider defaults.
- Added direct HTTP/HTTPS URL actions and automatic https:// normalization for www. input.
- Added {query} semantics for existing URL user commands so keywords/aliases can perform web searches without changing commands.json schemaVersion 1.
- Added UTF-8 percent encoding for web-search query text, including Unicode/Chinese input.
- Suppressed the unresolved URL-template command when its resolved smart action is active.
- Kept ordinary URL commands without {query} unchanged.
- Recorded usage against the source user command after a generated web-search action launches successfully.
- Routed web actions through unified ranking and the existing Enter/double-click/numeric/single-result execution path.
- Made Everything File/Folder actions populate the new explicit action payload while preserving their existing behavior.
- Added portable regression coverage for direct URLs, www. normalization, aliases, empty queries, Unicode encoding and non-HTTP template rejection.
- Added the new web-action test to current-Windows and Windows 10 API-baseline CI.
- Kept settings schemaVersion 3, commands/usage schemaVersion 1, provider-cache schemaVersion 2 and Classic geometry 420/16/10 unchanged.
- Updated Windows version metadata to 0.6.0-alpha.1 / 0.6.0.1.

## 0.5.0

- Promoted the frozen v0.5.0 RC3 code line to Stable without adding new launcher behavior.
- Shipped native external Everything File/Folder search with unified User Command/Application/Folder/File ranking and dynamic recovery/fallback.
- Kept Everything disabled by default and retained the Unicode Query2/WM_COPYDATA transport, unnamed-first endpoint selection and conservative unique-named-instance fallback.
- Stabilized settings schemaVersion 3 with atomic v0.4.1 schema-2 migration and newer-schema downgrade read-only protection; commands/usage remain schemaVersion 1 and provider-cache remains schemaVersion 2.
- Retained the RC3 Settings polish: 54-logical-pixel owner-draw rows, single-line shortcut-editor labels, width-aware actions, separated Everything diagnostics/onboarding and owner-drawn sidebar navigation.
- Preserved Classic Launcher geometry at 420 logical px width, 16 logical px row height and 10 visible results.
- Retained current-Windows and Windows 10 runtime smoke, 100/125/150/200% layout regression coverage, x64/ARM64 package contracts, packaged x64 startup smoke and SHA256 self-verification.
- Updated the primary Stable download links from v0.4.1 to v0.5.0.
- Updated Windows version metadata to `0.5.0` / `0.5.0.300`.
- Kept the packaged v0.5 RC validation checklist as the explicit manual regression record rather than fabricating unchecked observations.


## 0.5.0-rc.3

- Fixed the General Settings title/description overlap by replacing 46px owner-draw rows with a shared 54-logical-pixel toggle-row metric.
- Aligned General card content gutters with the common Settings page header gutters.
- Added explicit DPI regression assertions for the polished row height and 38/34 logical content insets at 100/125/150/200%.
- Made shortcut-editor field labels single-line and widened the label column so Chinese aliases/working-directory labels no longer wrap into adjacent fields.
- Reworked shortcut-editor option and action rows to derive widths from available field space instead of fixed absolute positions.
- Matched edit and browse-control heights for cleaner field alignment.
- Reworked Search Sources vertical spacing so Everything diagnostics, onboarding actions and explanatory text no longer overlap.
- Unified Search Sources toggle rows with the same polished owner-draw row metric.
- Raised the Settings minimum width to 960 logical pixels for bilingual layout stability while preserving work-area clamping.
- Replaced native boxed sidebar buttons/text bullets with owner-drawn navigation using a subtle selected background, accent bar and semibold active-page text.
- Preserved Classic Launcher 420/16/10 geometry, settings schemaVersion 3, commands/usage schemaVersion 1, provider-cache schemaVersion 2 and the frozen Everything Query2 transport.
- Updated Windows version metadata to `0.5.0-rc.3` / `0.5.0.202`.


## 0.5.0-rc.2

- Replaced the confusing raw `ERROR_FILE_NOT_FOUND (2)` primary Everything diagnosis with explicit “Everything not detected” onboarding when no IPC endpoint exists.
- Added bilingual guidance that standard Everything must be installed/running, ALTRun Next does not bundle or auto-start it, and Everything Lite has no IPC.
- Added **Get Everything** to open the official voidtools download page.
- Added **Recheck** to immediately re-probe Everything availability without restarting ALTRun Next.
- Kept ambiguous named-instance diagnostics separate from the missing-Everything onboarding path.
- Kept application-only fallback active while Everything is unavailable.
- Did not add automatic download/install/start/update management; managed Everything remains post-v0.5 work.
- Preserved settings schemaVersion 3, provider IDs/defaults, Query2/WM_COPYDATA, persistence boundaries and Classic launcher geometry.
- Updated Windows version metadata to `0.5.0-rc.2` / `0.5.0.201`.


## 0.5.0-rc.1

- Entered v0.5.0 feature freeze: no new provider, schema, Classic geometry or Everything transport surface.
- Froze settings schemaVersion 3, commands/usage schemaVersion 1 and provider-cache schemaVersion 2.
- Froze provider IDs and safe defaults, including `everything.filesystem=false`.
- Froze the beta.2 Everything compatibility baseline: unnamed-first endpoint selection, unique named-instance fallback, multi-instance ambiguity fallback, Query2 WM_COPYDATA and no Everything DLL/named-pipe dependency.
- Added `docs/V0.5_RC_VALIDATION.md` as the stable-promotion checklist for Windows 10/11, Everything 1.4/1.5, migration/downgrade, mixed DPI, IME, paths, soak and package validation.
- Added `V0.5_RC_VALIDATION.md` and `EVERYTHING_COMPATIBILITY.md` to both portable release ZIPs and the exact package allowlist.
- Kept all real-desktop validation items intentionally unchecked until manually observed.
- Limited post-RC changes to regression, compatibility, data-safety and publication/package fixes.
- Updated Windows version metadata to `0.5.0-rc.1` / `0.5.0.200`.


## 0.5.0-beta.2

- Added conservative Everything named-instance discovery using the documented `EVERYTHING_TASKBAR_NOTIFICATION_(instance)` class convention.
- Preserved unnamed/default-instance precedence; a named instance is auto-selected only when exactly one candidate exists.
- Added explicit ambiguous-multiple-instance fallback so ALTRun Next never silently chooses an arbitrary Everything database.
- Exposed the active IPC window class, named-instance fallback state and ambiguous candidate count in Search Sources diagnostics.
- Added sender-HWND validation for LIST2 replies to ignore spoofed or unrelated WM_COPYDATA responses.
- Added a 16 MiB default reply-payload guard and requested-result-count enforcement before accepting dynamic results.
- Hardened LIST2 parsing against item-count/total-count and offset-range inconsistencies.
- Preserved the Everything drive/root flag and normalized drive roots without manufacturing a misleading parent path.
- Added UNC and extended-length `\\?\` path runtime coverage.
- Added 128-query debounce stress, 256-result/500000-total result smoke and provider transport-cap coverage.
- Added wrong-sender, oversized-payload and over-limit-result runtime regressions.
- Kept the 1.4-compatible Query2 WM_COPYDATA transport; no Everything DLL and no 1.5-only named-pipe dependency were added.
- Kept settings schemaVersion 3, commands/usage schemaVersion 1, provider-cache schemaVersion 2, Everything default-off and Classic geometry unchanged.
- Updated Windows version metadata to `0.5.0-beta.2` / `0.5.0.101`.


## 0.5.0-beta.1

- Promoted `everything.filesystem` into the supported Settings > Search sources surface while keeping it disabled by default.
- Bumped `settings.json` to schemaVersion 3; commands/usage remain schemaVersion 1 and provider-cache remains schemaVersion 2.
- Added atomic schema-2 -> schema-3 migration tracking and regression coverage.
- Preserved the alpha experimental `everything.filesystem: true` opt-in during migration; schema-2 files without the key migrate with Everything disabled.
- Added an explicit schema-3 -> schema-2 downgrade regression proving the newer settings document remains byte-for-byte unchanged under read-only compatibility protection.
- Added live Everything diagnostics to Search Sources: current IPC availability, last query status, returned/total matches, latency and native Windows error.
- Added a one-second diagnostics refresh while Search Sources is visible plus immediate refresh after a dynamic query completes.
- Added clear application-search fallback status when Everything is enabled but IPC is unavailable.
- Added same-client unavailable -> available recovery coverage so starting/restarting Everything does not require restarting ALTRun Next.
- Kept standard Everything as an external dependency: no auto-start, no bundled Everything executable/DLL and no Everything Lite IPC workaround.
- Kept Everything File/Folder results out of provider-cache and usage persistence.
- Preserved the alpha.3 unified ranking, execution behavior and frozen Classic launcher geometry.
- Updated Windows version metadata to `0.5.0-beta.1` / `0.5.0.100`.


## 0.5.0-alpha.3

- Replaced the alpha.2 static-first append policy with unified ranking across User Command, Application, Folder and File results.
- Added a portable `ResultRanking` layer with dynamic filename/stem/path match scoring plus conservative kind/provider weights.
- Kept existing static SearchEngine scores as the primary ranking signal so mature command/app relevance, usage and pinning behavior remain intact.
- Added three-times-visible candidate depth for both static and Everything queries before final de-duplication/ranking.
- Kept static commands authoritative for duplicate targets so an Everything copy of the same executable/file path does not replace a richer application result.
- Added deterministic tie breaking by match score, result kind, provider and title.
- Added Classic folder presentation with a trailing backslash while preserving the frozen width, row height and result count.
- Changed File/Folder preview text to show the direct target path without the command prefix.
- Made numeric quick launch operate on the final unified result order, including File and Folder results.
- Made single-result immediate execution dynamic-aware: defer while the current Everything query is pending, then evaluate the settled merged result set once.
- Cancels deferred single-result execution when the launcher hides or the user manually executes a result, preventing late dynamic replies from triggering a second launch.
- Added portable ranking/merger tests and extended EverythingProvider runtime assertions to require a nonzero dynamic rank score.
- Kept Everything default-off, settings schemaVersion 2, commands/usage schemaVersion 1, provider-cache schemaVersion 2, Classic geometry and no-Everything-DLL packaging unchanged.
- Updated Windows version metadata to `0.5.0-alpha.3` / `0.5.0.3`.


## 0.5.0-alpha.2

- Added the unified `LauncherResult` / `ResultKind` / `LauncherAction` model between App and Launcher.
- Added the generic `DynamicQueryProvider` contract and fixed dynamic provider ID `everything.filesystem`.
- Added `EverythingProvider` to translate query-time Everything IPC File/Folder items into unified launcher results.
- Added UI-thread handoff for asynchronous dynamic responses with generation validation on top of the IPC client's stale-reply protection.
- Kept static User Command/Application search synchronous and non-blocking; dynamic Everything results arrive later and rebuild the visible result list.
- Added a conservative alpha.2 result merger: static results first, dynamic results appended into remaining slots, target de-duplication performed case-insensitively.
- File rows render file name + parent path in the existing Classic columns; long text keeps the existing ellipsis behavior without changing Classic geometry.
- Added unified execution: static commands keep their existing execution path, Files open with the default Windows application, and Folders open through Shell/Explorer.
- Kept dynamic File/Folder results ephemeral and out of provider-cache/usage persistence.
- Kept Everything disabled by default; alpha testers can explicitly opt in with `providers["everything.filesystem"] = true` in `data/settings.json`.
- Kept settings schemaVersion 2 and deferred the formal schemaVersion 3 / Settings UI integration to the later Settings phase.
- Suppressed single-result immediate execution while dynamic search is enabled; mixed-result immediate-execution policy remains an alpha.3 task.
- Added portable result-merger regression tests and extended real WM_COPYDATA runtime coverage through `EverythingProvider` result mapping.
- Updated Windows version metadata to `0.5.0-alpha.2` / `0.5.0.2`.


## 0.5.0-alpha.1

- Added a portable Everything Query2 protocol layer for native Unicode `WM_COPYDATA` IPC without an Everything DLL dependency.
- Added Query2 request encoding for name/path/full-path fields and defensive LIST2 parsing with bounds, offset, UTF-16 terminator and request-flag validation.
- Added the Windows `EverythingIpcClient` with a dedicated worker thread and hidden reply window.
- Added 70 ms latest-query debounce, per-query reply tokens, generation-based stale-result discard, bounded send timeout and reply timeout handling.
- Added availability detection and graceful `Unavailable` behavior when the Everything IPC window is missing or IPC is unsupported, including Everything Lite behavior.
- Added query/result foundation types for File/Folder, latency, total matches and native error reporting.
- Added portable protocol tests covering Unicode/Chinese payloads and malformed IPC buffers.
- Added Windows fake-Everything runtime tests using real HWND + `WM_COPYDATA` messaging for success, unavailable fallback, stale reply discard, rapid typing coalescing and reply timeout.
- Added Everything IPC runtime smoke coverage to both current-Windows and Windows 10 API-baseline CI.
- Added an alpha.1 release contract that keeps settings schemaVersion 2, commands/usage schemaVersion 1, provider-cache schemaVersion 2, existing provider IDs and frozen Classic geometry unchanged.
- Kept `everything.filesystem`, LauncherResult integration, settings schemaVersion 3, provider-cache/usage persistence and Launcher file rendering out of alpha.1 by design.
- Updated Windows version metadata to `0.5.0-alpha.1` / `0.5.0.1`.


## 0.4.1

- Promoted the frozen v0.4.1 RC1 feature set to Stable without adding new launcher behavior.
- Kept settings schemaVersion 2, commands/usage schemaVersion 1 and provider-cache schemaVersion 2 unchanged.
- Kept the stable Windows provider IDs and frozen Classic launcher geometry unchanged.
- Retained show-on-startup, the optional auxiliary hotkey with bare Pause support, wildcard matching, Classic numeric quick execution/order and optional single-result immediate execution.
- Retained transactional primary/auxiliary hotkey registration, resume revalidation, IME-safe immediate execution and numeric-key auto-repeat suppression.
- Retained responsive/scrollable General Settings layout and high-DPI work-area clamping.
- Retained tag/VERSION preflight, Release-mode assertion integrity, real RegisterHotKey smoke, Windows 10 API-baseline validation, exact ZIP allowlisting, x64 packaged-runtime startup smoke and SHA256 self-verification.
- Updated Windows version metadata to `0.4.1` / `0.4.1.300`.
- Kept `DESKTOP_VALIDATION.md` as the explicit manual Windows desktop QA record without fabricating unchecked observations.


## 0.4.1-rc.1

- Entered the v0.4.1 release-candidate phase with the feature set, schemas, provider IDs and Classic geometry still frozen.
- Added `scripts/verify_tag_version.py` so a tag-triggered release is rejected unless the pushed tag exactly matches `v` + `VERSION`.
- Added a dedicated Release preflight job so tag/version/metadata/freeze-contract failures occur before expensive Windows build jobs start.
- Added main-CI coverage proving the tag guard accepts the current version tag and rejects an intentionally mismatched tag.
- Added checksum self-verification with `sha256sum -c SHA256SUMS.txt` to both main automatic publication and tag-triggered publication.
- Tightened the portable ZIP contract to an exact top-level allowlist, preventing stale or unexpected root entries from entering release assets.
- Extended the frozen v0.4.1 release-contract gate so tag preflight, checksum verification and package allowlisting cannot be removed accidentally during RC stabilization.
- Retained beta.2 Release-mode assertion integrity, desktop-layout validation, real RegisterHotKey smoke, v0.4.0 migration/downgrade tests and Windows 10 compatibility gates.
- Kept settings schemaVersion 2, commands/usage schemaVersion 1 and provider-cache schemaVersion 2 unchanged.
- Kept all user-facing v0.4.1 behavior and Classic launcher geometry unchanged.
- Updated Windows version metadata to `0.4.1-rc.1` / `0.4.1.200`.


## 0.4.1-beta.2

- Kept the v0.4.1 feature set frozen; this release contains validation-integrity and compatibility hardening only.
- Forced assertion-based C++ test targets to keep `assert()` active in Release CI builds by undefining `NDEBUG`.
- Added a compile-time guard proving the desktop validation target cannot silently run with assertions disabled.
- Extracted Classic numeric quick-launch mapping and single-result immediate-execution gating into portable helpers used by the production LauncherWindow.
- Added portable regression coverage for both numeric orders and single-result gating during IME composition, empty queries, disabled behavior and multiple results.
- Extracted General Settings layout calculation and work-area clamping into a portable helper used by the production SettingsWindow.
- Added automated layout coverage at 100%, 125%, 150% and 200% DPI for wide, stacked, compact and scrollable General-page states.
- Clamped `WM_DPICHANGED` suggested Settings geometry to the destination monitor work area.
- Capped Settings minimum tracking dimensions to the current monitor work area on high-DPI/small-display configurations.
- Added a real Windows `RegisterHotKey` runtime smoke covering duplicate conflict detection, unregister/re-register and a modified no-repeat binding.
- Expanded both current-Windows and Windows 10 API-baseline smoke jobs with desktop-validation and hotkey-runtime tests.
- Added a representative v0.4.0 -> v0.4.1 settings migration regression preserving existing preferences and applying safe schema-2 defaults.
- Added an older-reader simulation proving the migrated schema-2 settings document remains byte-for-byte unchanged when opened with a schema-1 compatibility ceiling.
- Extended the v0.4.1 release-contract gate so validation helpers/tests cannot be accidentally removed during Beta/RC.
- Kept settings schemaVersion 2, commands/usage schemaVersion 1 and provider-cache schemaVersion 2 unchanged.
- Kept Classic launcher geometry frozen.
- Updated Windows version metadata to `0.4.1-beta.2` / `0.4.1.101`.


## 0.4.1-beta.1

- Entered v0.4.1 feature freeze; Beta/RC work is limited to regressions, compatibility and release validation.
- Added scripts/verify_release_contract.py to lock the v0.4.1 settings/commands/usage/provider-cache schema versions, stable provider IDs, safe default settings and core Classic geometry.
- Added docs/DESKTOP_VALIDATION.md with a repeatable real Windows 10/11 desktop matrix covering 100%/125%/150%/200% DPI, hotkey lifecycle, IME, Classic search behavior, provider refresh, migration/downgrade and final release assets.
- Added scripts/verify_runtime_smoke.ps1 and an x64 final-ZIP startup smoke that validates the packaged executable can enter its portable runtime loop without crashing and leaves no writeability-probe residue.
- Added DESKTOP_VALIDATION.md to x64/ARM64 release packages and made it part of the package contract.
- Upgraded the tag-triggered Release workflow to require Core Tests, the v0.4.1 freeze contract, Windows provider/hotkey smoke tests and the Windows 10 API compatibility gate before release publication.
- Kept main automatic publication and manual/external v* tag publication aligned on the same release-safety gates.
- Kept settings schemaVersion 2, commands/usage schemaVersion 1 and provider-cache schemaVersion 2 unchanged.
- Kept the Classic launcher feature set and visual geometry frozen.
- Updated Windows version metadata to 0.4.1-beta.1 / 0.4.1.100.


## 0.4.1-alpha.3

- Hardened the v0.4.1 Settings/behavior feature set without adding a new launcher feature surface.
- Changed Settings-open hotkey validation to retry only missing registrations instead of unregistering/re-registering bindings that are already working.
- Kept resume handling as a forced hotkey revalidation and refresh Settings registration status after repair.
- Fixed Restore defaults when the auxiliary hotkey occupies the default primary `Alt + Space`: the auxiliary binding is released first, and failures roll back the previous configuration.
- Suppressed single-result immediate execution during active IME composition so intermediate CJK input cannot trigger an unintended launch.
- Suppressed repeated Classic numeric quick-launch execution from keyboard auto-repeat.
- Reset stale IME-composition state whenever the launcher is shown.
- Made the General Settings page vertically scrollable when its content exceeds the current client area.
- Added responsive General-page behavior: launcher/search cards stack on narrow windows and hotkey controls use a compact wrapped layout.
- Clamped the Settings window to the active monitor work area so high-DPI scaling cannot center an oversized window partly off-screen.
- Restored the Settings minimum height to the pre-alpha.2 value now that General can scroll.
- Added Windows hotkey codec smoke coverage for `Pause/Break`, zero-modifier auxiliary bindings, modifier aliases and `MOD_NOREPEAT`.
- Added Config Core regression coverage for numeric quick-launch order preservation and invalid-order canonicalization.
- Kept settings schemaVersion 2, commands/usage schemaVersion 1 and provider-cache schemaVersion 2 unchanged.
- Updated Windows version metadata to `0.4.1-alpha.3` / `0.4.1.3`.


## 0.4.1-alpha.2

- Added Settings UI for the v0.4.1 Classic-behavior core without changing the frozen Classic launcher geometry.
- Added a show-on-startup toggle to General settings.
- Added a Search behavior card with toggles for `*` / `?` wildcard matching, Classic numeric quick launch and single-result immediate execution.
- Added a number-order selector for Classic quick launch: `1–9, 0` or `0–9`; the selector is disabled while numeric quick launch is off.
- Added full auxiliary-hotkey UI with enable/disable, Ctrl/Alt/Shift/Win modifiers, key selection, Apply action and independent registration/error status.
- Added `Pause` to the visible key selector so the default auxiliary binding can be configured without editing JSON.
- Kept auxiliary hotkeys transactional: a Windows registration conflict restores the previous working binding and refreshes the controls.
- Reorganized the General page into two behavior cards plus primary/auxiliary hotkey rows and launcher placement, while preserving the existing Settings Shell navigation.
- Updated the About description and settings documentation for the Classic-settings parity phase.
- Kept settings schemaVersion 2, commands/usage schemaVersion 1 and provider-cache schemaVersion 2 unchanged from alpha.1.
- Updated Windows version metadata to `0.4.1-alpha.2` / `0.4.1.2`.


## 0.4.1-alpha.1

- Started the Classic-settings parity phase without changing the frozen Classic launcher geometry.
- Split configuration compatibility by document: settings.json moves to schemaVersion 2 while commands.json and usage.json remain schemaVersion 1 and provider-cache.json remains schemaVersion 2.
- Added atomic schema-1 -> schema-2 settings migration so v0.4.0 downgrades enter read-only compatibility mode instead of dropping new preferences.
- Added show-on-startup state with a default of disabled.
- Added an optional auxiliary global hotkey with an independent Windows hotkey ID, transactional registration/rollback and resume repair; the default auxiliary binding is bare Pause and remains disabled until enabled.
- Added Pause/Break support to the hotkey key-name codec.
- Added opt-in * / ? glob matching across keyword, aliases, title and target while preserving the existing fuzzy/pinyin path for normal queries.
- Added Classic numeric quick execution for top-ten results with selectable 1–9,0 or 0–9 ordering.
- Added optional immediate execution when a non-empty user query has exactly one result.
- Kept every new behavior disabled by default so upgrading from v0.4.0 does not change launcher interaction until the user opts in.
- Updated settings.example.json and CONFIG_SCHEMA.md for settings schemaVersion 2.
- Added Config Core migration/persistence coverage and SearchEngine wildcard regression coverage.
- Updated Windows version metadata to `0.4.1-alpha.1` / `0.4.1.1`.


## 0.4.0

- Promoted v0.4.0-rc.1 to the stable v0.4.0 release with no additional discovery features.
- Finalized multi-source Windows application discovery across Start Menu, Windows Apps / UWP / MSIX, App Paths and PATH.
- Finalized deterministic provider precedence and duplicate suppression while keeping explicit user shortcuts authoritative.
- Finalized persistent per-provider caching, non-blocking background refresh, source-aware incremental refresh, debounce scheduling and provider diagnostics.
- Finalized migration/recovery hardening for settings, commands and usage, including valid-.bak self-healing and newer-schema read-only downgrade protection.
- Finalized startup data-health diagnostics and portable data-directory writeability checks.
- Kept the Windows 10 API baseline plus Provider Registry runtime smoke coverage on current Windows and Windows Server 2022 runners.
- Kept version-consistency and final-package contract gates for x64 and ARM64, including EXE fixed FileVersion/ProductVersion verification and unexpected-DLL rejection.
- Stable and rolling releases include `SHA256SUMS.txt`.
- Kept user-data schemaVersion 1 and provider-cache schemaVersion 2 unchanged from RC1.
- Updated release metadata to `0.4.0` and Windows fixed FileVersion/ProductVersion to `0.4.0.300`.


## 0.4.0-rc.1

- Entered the v0.4 release-candidate phase with the discovery feature set frozen.
- Added startup writeability probing for the portable `data/` directory and a localized warning when settings cannot be persisted safely.
- Added per-store recovery state for `settings.json`, `commands.json` and `usage.json` when Config Core self-heals from a valid `.bak`.
- Extended the Data-page health notice to report recovered files, newer-schema read-only files and an unwritable data directory.
- Added regression coverage proving settings, commands and usage recovery state remains visible after the primary JSON file is repaired.
- Added a generated `Version.hpp` so the About page derives its version directly from the root `VERSION` file instead of a duplicated literal.
- Added `scripts/verify_version.py` to gate releases on consistent VERSION, CMake base version, Windows resource metadata, manifest version, README and CHANGELOG.
- Added post-package contract validation for required portable files, unexpected DLLs, packaged VERSION and EXE FileVersion/ProductVersion.
- Corrected the Windows VERSIONINFO resource include/constants so the embedded fixed FileVersion/ProductVersion is exposed through the standard Windows version APIs instead of appearing as 0.0.0.0.
- Added `SHA256SUMS.txt` to rolling and immutable GitHub releases.
- Kept user-data schemaVersion 1 and provider-cache schemaVersion 2 unchanged.
- Updated Windows version metadata to `0.4.0-rc.1`.


## 0.4.0-beta.2

- Hardened atomic JSON backup semantics so an invalid primary file can no longer overwrite a valid `.bak` during the next save.
- Added automatic primary-file self-healing when a valid backup is used.
- Added schema-aware JSON loading with explicit primary, backup-recovery and unsupported-schema states.
- Added downgrade-safe read-only compatibility for `settings.json`, `commands.json` and `usage.json`.
- Newer-schema documents continue to expose known fields when possible, but all writes are blocked so an older binary cannot destroy newer data.
- Added a Data-page compatibility warning listing each read-only file and its unsupported schema version.
- Made settings appearance/general writes transactional instead of leaving in-memory state changed after a failed save.
- Made usage-history recording transactional and disabled it while usage data is in newer-schema read-only mode.
- Added Provider Cache validation that rejects commands stored under a provider ID that does not match the command source.
- Future provider-cache schemas are treated as disposable generated state and rebuilt rather than interpreted by an older binary.
- Expanded migration tests across all 16 provider-enable combinations, alpha-era settings without provider keys, backup self-healing, future settings/commands/usage schemas, provider/source mismatches and alpha.2 cache migration.
- Added a Windows Provider Registry runtime smoke executable covering stable IDs, enable/disable isolation, targeted discovery and change-token behavior.
- Added provider runtime smoke CI on both `windows-latest` and `windows-2022`; versioned releases now require both smoke gates in addition to Core Tests, x64, ARM64 and the Windows 10 API baseline.
- Kept settings/commands/usage schemaVersion 1 and provider-cache schemaVersion 2 unchanged.
- Updated Windows version metadata to `0.4.0-beta.2`.


## 0.4.0-beta.1

- Extracted provider/user command de-duplication from `CommandStore` into a portable, independently testable `CommandMerge` core module.
- Made provider precedence explicit and deterministic: user shortcuts > Start Menu > Windows Apps > App Paths > PATH.
- Preserved explicit duplicate user shortcuts while suppressing automatic duplicates by normalized target or normalized name + keyword.
- Added dedicated command-merge regression tests for user authority, provider priority, path normalization, semantic duplicate detection, distinct-entry preservation and disabled entries.
- Changed `CommandStore` to retain raw enabled provider cache entries until the merge stage, making de-duplication statistics accurate.
- Added per-provider diagnostics for cached count, active search count and duplicate-suppressed count.
- Added current-session provider refresh diagnostics including last attempt time, success/failure state and provider error text.
- Added a Windows 10 API compile baseline with `_WIN32_WINNT=0x0A00` and `WINVER=0x0A00`.
- Added a separate `windows-2022` x64 compatibility build and made it a required gate for rolling/versioned releases.
- Kept provider-cache schemaVersion 2 and settings schemaVersion 1 unchanged; no user-data migration is required from v0.4 alpha releases.
- Updated Windows version metadata to `0.4.0-beta.1`.


## 0.4.0-alpha.4

- Added lightweight change tokens to Start Menu, Windows Apps, App Paths and PATH providers.
- Added a low-frequency provider monitor that detects source changes without blocking the launcher UI.
- Added targeted provider refresh so a changed source no longer forces unrelated providers to rescan.
- Added 750 ms debounce scheduling to coalesce bursts of application-install/update changes.
- Added queued provider refresh requests when discovery is already running, preserving source-specific requests without falling back to a full rescan.
- Disabled providers are excluded from change-token monitoring as well as search results.
- Treat transient AppsFolder / COM enumeration failures as provider failures so the previous Windows Apps cache is retained instead of being replaced with an empty result set.
- Improved PATH discovery to observe process PATH plus current machine/user registry PATH values, allowing newly added PATH directories to be discovered without restarting ALTRun Next.
- Added provider runtime status in Settings: enabled state, cached command count and last successful cache refresh time.
- Manual **Rebuild program index** remains an explicit full refresh of all enabled providers.
- Added cross-platform ProviderFingerprint coverage to Config Core tests.
- Updated Windows version metadata to `0.4.0-alpha.4`.


## 0.4.0-alpha.3

- Replaced the combined Windows application scanner with four independent providers: Start Menu, Windows Apps, App Paths and PATH.
- Added `ProviderRegistry` with stable provider IDs, metadata, default-enabled state and provider priority.
- Added provider IDs `windows.startmenu`, `windows.packaged`, `windows.apppaths` and `windows.path`.
- Added a Settings -> Search sources page with independent enable/disable controls for all four Windows providers.
- Provider source changes now affect the live search index immediately; re-enabled sources reuse their existing cache while a background refresh runs.
- Added queued provider refresh behavior so source changes made during an active scan are refreshed again using the newest settings.
- Upgraded `provider-cache.json` to cache schema 2 with independent per-provider timestamps and command arrays.
- Added automatic in-memory migration from the flat v0.4.0-alpha.2 provider cache.
- Added provider-level failure isolation: successful providers update independently while failed providers retain their previous cached data.
- Added partial-refresh status reporting in Settings.
- Extended Config Core tests for provider defaults, settings persistence, schema-2 provider cache, user-command exclusion, backup recovery and alpha.2 cache migration.
- Rewrote the project roadmap to match the actual v0.2-v0.4 development history and the planned v0.5+ direction.
- Removed the runtime use of the old combined `WindowsAppProvider`.
- Updated Windows version metadata to `0.4.0-alpha.3`.


## 0.4.0-alpha.2

- Added persistent automatic-provider caching in `data/provider-cache.json`.
- Launcher startup now loads user commands plus the last known provider cache instead of synchronously rescanning Windows application sources.
- Added a background provider refresh worker for Start Menu, App Paths, PATH and AppsFolder discovery.
- Provider results are written atomically and hot-reloaded on the UI thread after a successful background scan.
- Existing cached results remain searchable while discovery is in progress or if a refresh fails.
- Changed Data -> Rebuild program index into a non-blocking background refresh.
- Added Settings status feedback for background index refresh completion/failure without changing the Classic launcher layout.
- Made `WindowsAppProvider` initialize a COM apartment on the discovery thread before enumerating `FOLDERID_AppsFolder`.
- Added Config Core coverage for provider-cache round trips, user-command exclusion and `.bak` recovery.
- Updated Windows version metadata to `0.4.0-alpha.2`.


## 0.4.0-alpha.1

- Started the v0.4 Windows 11 application-discovery phase with a shared command-provider interface.
- Converted the existing Start Menu scanner into an `ICommandProvider`.
- Added `WindowsAppProvider` and merged its results into the same CommandStore/SearchEngine pipeline.
- Added discovery from the current-user and machine-wide Windows `App Paths` registry keys, including 64-bit and 32-bit registry views.
- Added discovery of executables exposed through the effective Windows `PATH`.
- Added UWP / MSIX / Microsoft Store application discovery through the Windows `FOLDERID_AppsFolder` shell namespace.
- Added provider-specific command sources for App Paths, PATH and packaged applications.
- Added cross-provider de-duplication while keeping user-defined shortcuts authoritative.
- Existing Data -> Rebuild program index now rescans every provider, not only the Start Menu.
- Start Menu results retain a higher default search priority than raw PATH entries; packaged apps and App Paths participate without overriding explicit user keywords or aliases.
- No persistent provider cache is introduced yet; background caching and incremental refresh remain scheduled for later v0.4 alphas.
- Updated Windows version metadata to `0.4.0-alpha.1`.


## 0.3.0-alpha.2

- Added multi-token search so spaced queries such as `visual code` can match all requested terms across a command.
- Added English word/camel-case initials, including matches such as `Windows Terminal` -> `wt`.
- Added hybrid pinyin matching that can mix full syllables and initials in one query, for example `网易云音乐` -> `wangyy`.
- Added spaced pinyin token matching such as `微信` -> `wei x`.
- Improved mixed Chinese/English abbreviation handling, including `微信 DevTools` -> `wxdt`.
- Kept explicit user keywords and aliases ranked above automatically derived initials and pinyin.
- Multi-token queries now require every query token to match before receiving the multi-token ranking bonus.
- Extended SearchEngine CI tests for hybrid pinyin, spaced pinyin, multi-word search, English initials and mixed Chinese/English initials.
- Updated Windows version metadata to `0.3.0-alpha.2`.


## 0.3.0-alpha.1.1

- Fixed the Windows startup failure caused by a missing `cpp-pinyin.dll` in the v0.3.0-alpha.1 package.
- Stopped consuming cpp-pinyin through its upstream CMake target on Windows.
- ALTRun Next now builds cpp-pinyin 1.0.2 sources into an explicit internal STATIC library, so `ALTRunNext.exe` has no cpp-pinyin runtime DLL dependency.
- Added a Windows CI portability gate that fails the build if `cpp-pinyin.dll` or any unexpected DLL is produced beside the executable.
- Kept the Mandarin dictionaries and Apache-2.0 license bundled in the portable archive.
- Updated Windows version metadata to `0.3.0-alpha.1.1`.


## 0.3.0-alpha.1

- Added the first Pinyin Search Core for Chinese shortcut and Start Menu names.
- Added full-pinyin matching, so names such as `微信` can be found with `weixin`.
- Added pinyin-initial matching, so `微信`, `网易云音乐` and `计算器` can be found with `wx`, `wyy` and `jsq`.
- Added phrase-aware polyphonic conversion through cpp-pinyin 1.0.2; for example, `重庆` indexes as `chongqing`.
- Pinyin is a derived runtime search signal only and is never persisted into `commands.json`.
- Primary keywords and explicit aliases continue to rank above derived pinyin matches.
- Added an in-memory pinyin-form cache so a Chinese field is converted only once per process.
- Pinyin conversion is attempted only for Latin/digit search queries and only for fields containing supported Han characters.
- Added graceful fallback: if the Mandarin dictionary is missing or fails to initialize, the original keyword/title/alias/fuzzy search remains fully functional.
- Added portable Mandarin dictionaries and the cpp-pinyin Apache-2.0 license to x64/ARM64 release packages.
- Added CI coverage for Chinese literal search, full pinyin, initials, polyphonic words, ranking priority and dictionary-missing fallback.
- Updated Windows version metadata to `0.3.0-alpha.1`.


## 0.2.0-beta.1.1

- Fixed cases where Settings showed a saved global hotkey while the runtime binding was not actually usable.
- Moved global hotkey ownership from the hidden Launcher HWND to the main UI thread message queue.
- Removed the cached same-hotkey early-success path; applying a hotkey now always performs a real unregister/register transaction with Windows.
- Failed hotkey changes atomically restore the previous working binding.
- Added runtime hotkey status to Settings: Registered / Not registered plus the Windows error code when available.
- Revalidates the configured hotkey when Settings opens and after Windows resumes from suspend.
- Added a per-session single-instance guard so multiple ALTRun Next processes cannot silently compete for the same global hotkey.
- The application message loop now handles thread-level WM_HOTKEY directly and toggles the Launcher independently of Launcher window visibility/focus.
- Updated Windows version metadata to `0.2.0-beta.1.1`.


## 0.2.0-beta.1

- Added configurable global launcher hotkeys instead of a hard-coded Alt+Space binding.
- Added Ctrl / Alt / Shift / Win modifier selection and common letter, number, function and navigation keys.
- Hotkey changes are committed only after Windows successfully registers the new combination; conflicts keep the previous binding active.
- Added Start with Windows using the current-user HKCU Run key, without requiring administrator privileges.
- Added a dedicated Data settings page.
- Added full-fidelity ALTRun Next TSV import/export for user shortcuts.
- Added backward-compatible import of the old five-column commands.tsv format.
- Added best-effort legacy ALTRun Beta import for tab-separated rows and simple keyword=target entries.
- Added Clear usage history without deleting shortcuts.
- Added Rebuild program index to rescan Start Menu entries immediately.
- Added Restore default settings without deleting commands or usage history.
- Added import/export, settings reset and usage-clear coverage to Config Core tests.
- Added shared Windows hotkey parsing helpers and advapi32 linkage.
- Updated Windows version metadata to `0.2.0-beta.1`.


## 0.2.0-alpha.3

- Added the first full Command Manager to the Settings window.
- Added a searchable master/detail view for persistent user shortcuts.
- Added create, edit and delete operations backed by UserCommandStore rather than direct JSON editing.
- Added persistent stable UUID handling for newly created shortcuts.
- Added primary keyword, comma-separated aliases, command type, target, arguments and working-directory editing.
- Added enabled, run-as-administrator and pinned options.
- Added target file picker and working-directory folder picker.
- Added test-run support without affecting usage ranking history.
- Added manual move-up / move-down ordering.
- Added duplicate primary-keyword warnings while still allowing intentional conflicts.
- Added unsaved-change protection when switching shortcuts, pages or closing Settings.
- Saving, deleting or reordering a shortcut refreshes the Launcher immediately.
- Added CRUD persistence coverage to Config Core tests.
- Added `comdlg32` for the native target-file picker.
- Updated Windows version metadata to `0.2.0-alpha.3`.


## 0.2.0-alpha.2.1

- Fixed Settings page-switch text ghosting caused by transparent STATIC control backgrounds.
- Added full content redraw when switching Settings pages and changing language.
- Rebuilt the General page into clearly separated Launcher behavior and Launcher placement sections.
- Replaced tiny native checkboxes with full-width owner-drawn setting rows and 20 logical px check indicators.
- Added concise secondary descriptions for every General toggle.
- Added card borders and row separators for clearer visual grouping.
- Improved spacing and slightly increased the Settings window height for the new layout.
- Kept Command Manager work reserved for v0.2.0-alpha.3.


## 0.2.0-alpha.2

- Added an independent modern Win32 Settings Shell.
- Added left-side navigation for General, Appearance and About pages.
- Added tray-menu Settings entry and `F2` shortcut from the launcher.
- Bound General settings directly to the alpha.1 JSON Config Core.
- Added live hide-after-launch, clear-query-on-show, hide-on-focus-loss and tray-icon settings.
- Added launcher placement choices for mouse monitor, active-window monitor and primary monitor.
- Added live Classic / Modern Compact switching from Settings.
- Added live Simplified Chinese / English switching from Settings.
- Added About page with version, data directory, open-data-folder action and GitHub link.
- Preserved the Classic launcher UI as a separate surface.
- Added a rolling `dev-latest` GitHub Prerelease with fixed x64 and ARM64 download URLs.
- Updated tagged release workflow so alpha/beta tags are automatically marked as Prereleases.
- Updated Windows version metadata to `0.2.0-alpha.2`.


## 0.2.0-alpha.1

- Added versioned JSON Config Core with `schemaVersion: 1`.
- Moved live settings to `data/settings.json`.
- Moved user shortcuts to `data/commands.json`.
- Moved usage ranking history to `data/usage.json`.
- Added automatic one-time migration from legacy `settings.ini`, `commands.tsv` and `usage.tsv`.
- Legacy files are preserved unchanged for rollback.
- Added stable UUID v4 identifiers for user commands.
- Added `legacyIds` mapping so migrated usage history follows the new UUIDs.
- Extended the user command model with aliases, type, icon source, enabled state, administrator launch, pinning and manual sort order.
- Separated persistent user commands from automatic Start Menu discovery.
- Added dedicated `StartMenuProvider`.
- Added JSON atomic writes using temporary files, validation and one-generation `.bak` backups.
- Added fallback loading from valid backup JSON when the live document is damaged.
- Added portable Config Core migration/recovery tests.
- Added JSON example files and Config Core schema documentation.
- Added compile-time `nlohmann/json` dependency; no additional runtime is required.
- Updated Windows version metadata to `0.2.0-alpha.1`.


## 0.1.9

- Reverted the v0.1.8 narrow-right-edge experiment.
- Restored the Classic right frame to the same 7 logical px width as the left frame.
- Rebuilt the right rail as a vertical gray gradient rather than a flat solid strip.
- Added section-aware inner blending so the title, green hint strip, white result list and green command strip transition naturally into the frame.
- Kept a single dark outer stroke to visually connect the right edge with the top and bottom frame.
- Preserved the existing left frame, window dimensions, result geometry, typography, colors and Modern Compact layout.
- Bumped application, manifest and Windows resource version to 0.1.9.


## 0.1.8

- Reworked the Classic right edge for visual naturalness rather than strict symmetry with the old skin.
- Reduced the Classic right-side reserved rail from 7 logical px to 2 logical px while keeping the left side at 7.
- Extended the input/hint, result list and bottom command strip closer to the right frame.
- Removed the wide multi-band v0.1.7 bevel that read visually as a separate vertical decoration.
- Replaced it with a restrained narrow finish: one soft transition line and one dark outer edge.
- Extended the title gradient to the same new right-side boundary so the entire frame remains consistent.
- Kept the window size, row height, left frame, column separators, selection colors and Modern Compact unchanged.
- Bumped application, manifest and Windows resource version to 0.1.8.


## 0.1.7

- Reworked the Classic right-side frame from a flat dark rail into an asymmetric 3D bevel.
- Kept the already-natural left rail unchanged.
- Added a bright inner highlight at the content/right-frame boundary.
- Added a medium transition band followed by a light-gray horizontal bevel.
- Preserved a one-pixel dark outer edge for definition.
- Applied the right bevel continuously through the title bar, hint strip, result list and command strip.
- Painted the close button after the bevel so it is never clipped by the frame.
- Kept Classic geometry, result columns, row height and Modern Compact unchanged.
- Bumped application, manifest and Windows resource version to 0.1.7.


## 0.1.6

- Fixed the visible Classic right-edge color interruption between the title bar and content area.
- Root cause: the title gradient extended to the window edge while the content area used a 7 logical px gray side rail.
- Inset the Classic title gradient by the same 7 logical px used by the content layout.
- Added continuous left and right gray side rails spanning the full window height.
- Moved the final outer and inner frame strokes to the end of Classic background painting.
- Repainted the logo and close button above the side rails to preserve their appearance.
- Applied the continuity fix symmetrically to both left and right edges.
- Kept window size, row height, result columns, colors and Modern Compact geometry unchanged.
- Bumped application, manifest and Windows resource version to 0.1.6.


## 0.1.5

- Froze the already-matched Classic window geometry and result-table layout.
- Recalibrated the title gradient to be darker on the left and brighter on the right.
- Softened the horizontal title-bar scanlines.
- Redrew the clean-room launcher emblem with a less circular blue folded form and a larger orange star.
- Rebuilt the Classic close button as a filled beveled polygon instead of crossing strokes.
- Slightly enlarged and repositioned both title-bar corner controls to match the supplied reference.
- Bumped application, manifest and Windows resource version to 0.1.5.


## 0.1.3

- Rebuilt Classic mode against a real user-provided old ALTRun screenshot instead of generic Win32 styling.
- Reduced Classic width from 500 to 420 logical px; at 150% DPI this is about 630 physical px, matching the reference.
- Reduced Classic result rows from 22 to 16 logical px.
- Added a custom dark-gray horizontal-gradient title bar.
- Added a small hand-drawn launcher emblem and large red close X.
- Added a centered dynamic title such as `[calc]`, following the selected shortcut.
- Added a pale-green top input/hint strip and pale-green bottom command strip.
- Added localized `命令：` / `Command: ` prefix.
- Rebuilt the result area into the original three-column structure: hotkey number, shortcut keyword, description.
- Classic hotkeys now render `1..9, 0` for the first ten visible items.
- Changed Classic result background and blue selection colors to values sampled from the reference screenshot.
- Added the two vertical separators visible in the original list.
- Kept Modern Compact as a separate rendering path.
- Bumped application, manifest and Windows resource version to 0.1.3.

## 0.1.2

- Reworked Classic ALTRun mode for higher visual fidelity.
- Added Classic square-corner and legacy-control behavior on Windows 11.
- Added shortcut numbering and compact layout.

## 0.1.1

- Added portable settings, Simplified Chinese / English and live UI switching.

## 0.1.0

- Created the clean-room C++23/Win32 development baseline.
