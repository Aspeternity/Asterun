# Changelog

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
