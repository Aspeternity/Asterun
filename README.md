# ALTRun Next

ALTRun Next is a clean-room Windows launcher inspired by classic ALTRun: small, keyboard-first, fast, and intentionally low-noise.

## Downloads

### Stable v0.7.0

The current stable release is published at the immutable `v0.7.0` tag:

- Release: https://github.com/Aspeternity/ALTRunNext/releases/tag/v0.7.0
- x64 direct download: https://github.com/Aspeternity/ALTRunNext/releases/download/v0.7.0/ALTRunNext-x64.zip
- ARM64 direct download: https://github.com/Aspeternity/ALTRunNext/releases/download/v0.7.0/ALTRunNext-ARM64.zip
- SHA-256 checksums: https://github.com/Aspeternity/ALTRunNext/releases/download/v0.7.0/SHA256SUMS.txt

### Rolling development build

The latest successful `main` build is always published to the fixed prerelease tag:

- Development release: https://github.com/Aspeternity/ALTRunNext/releases/tag/dev-latest
- x64 direct download: https://github.com/Aspeternity/ALTRunNext/releases/download/dev-latest/ALTRunNext-x64.zip
- ARM64 direct download: https://github.com/Aspeternity/ALTRunNext/releases/download/dev-latest/ALTRunNext-ARM64.zip

You no longer need to find the correct GitHub Actions run. The `dev-latest` release is replaced automatically only after a successful build and test run.

## v0.8.0-alpha.3.39 — Interaction Reliability Hardening

Alpha 3.39 fixes two real-Windows interaction failures that shared the same UX symptom — a visible control sometimes appeared to ignore a click — but had different causes.

Path Conversion now treats the **entire first-column field cell** as the checkbox interaction target instead of only the 15/17/19/21/23 physical-pixel visual box. The visible checkbox, field label and row model remain exactly as introduced in alpha.3.38; only hit testing changes. `GetResultFieldInteractionRect()` derives the active area from the real first Header column plus the real ListView row bounds, so clicking the box, **目标 / 工作目录 / 自定义图标**, or the small gap around them toggles the same `Row::selected` state. Other columns remain non-toggling.

Shortcut Editor also removes an input-focus race in the existing ComboBox de-focus helper. The old `WM_PARENTNOTIFY` path called `SetFocus(hwnd_)` during **every** child-control mouse-down while a Type/Runtime Input ComboBox still had focus. That could steal focus in the middle of an owner-drawn BUTTON down/up sequence and prevent `BN_CLICKED`, most visibly on **高级选项 / Advanced** immediately after using a ComboBox. Parent-surface clicks still dismiss ComboBox focus, and child STATIC labels/hints still count as passive surface clicks, but interactive Buttons/Edits/ComboBoxes are now left alone to complete their native mouse sequence and take focus normally.

No checkbox visual, Path Conversion layout/column ratio/conversion behavior, Shortcut Editor layout, alpha.3.34 selector raster or alpha.3.36 updater architecture changes in this release. Settings schema remains **9**. Windows fixed FileVersion/ProductVersion is `0.8.0.69`.

## v0.8.0-alpha.3.38 — Path Conversion Checkbox Ownership

Alpha 3.38 removes the remaining Win32 checkbox rendering path from Path Conversion after real-Windows alpha.3.37 validation exposed a black state-image square. The hybrid approach — native `LVS_EX_CHECKBOXES` state images hidden behind transparent `LVSIL_STATE` slots with a custom post-paint checkbox layered on top — is deleted rather than patched again.

Checkbox ownership now lives entirely in the Path Conversion row model. Each convertible `Row` has its own `selected` flag, initialized from the existing-path rule (accessible paths selected by default; missing paths unselected). Selection counts and Apply operate directly on that model instead of `LVIS_STATEIMAGEMASK`.

The first result column is intentionally stored with empty native ListView text. After the normal row background/selection paint, ALTRun Next draws both the modern checkbox and the field label itself. A shared `GetResultFieldLayout()` computes the checkbox and label rectangles from the real row bounds, current first-column width and the same row-center coordinate. The checkbox therefore cannot drift independently above or below **目标 / 工作目录 / 自定义图标**.

Mouse clicks are hit-tested against the exact custom checkbox rectangle and toggle `Row::selected`. With the ListView focused, Space toggles the focused data row. Group-header selection remains suppressed. The existing 15/17/19/21/23 physical-pixel checkbox templates, rounded-corner 4×4 coverage raster and accent-blue checkmark are retained, but no state image, mask bitmap, themed checkbox or native checkbox glyph remains anywhere in this dialog.

The validated alpha.3.34 mode selector, 13/36/39/remainder columns, conversion semantics, group headers, empty state and alpha.3.36 updater dispatch hardening remain frozen. Settings schema remains **9**. Windows fixed FileVersion/ProductVersion is `0.8.0.68`.

## v0.8.0-alpha.3.37 — Path Conversion Checkbox Visual & Alignment Final Fix

Alpha 3.37 replaces the last visually inconsistent control in Path Conversion: the legacy Win32 ListView checkbox. Real-Windows alpha.3.35 validation showed that simply returning row height to native Explorer metrics moved the checkbox from visibly low to visibly high, and the old gray system state image still looked out of place beside the modern selector cards and flat actions.

The ListView still owns checkbox state, click hit-testing, Space-key toggling, `LVIS_STATEIMAGEMASK` and `LVN_ITEMCHANGED`. Its two state images are replaced with transparent DPI-sized slots, while the visible checkbox is drawn after the normal row paint. This keeps the proven native interaction model without exposing the legacy checkbox artwork.

The visible checkbox uses the same physical-pixel size buckets as the validated alpha.3.34 mode selector: 15 / 17 / 19 / 21 / 23 px. A white flat body, rounded corners, restrained gray unchecked border, accent-blue checked border and accent-blue checkmark are rasterized with fixed 4×4 coverage supersampling directly into the final HDC. The box is positioned from the real ListView row center and label rectangle rather than a hard-coded vertical offset, so checkbox and text share the row's actual visual center at fractional DPI.

Path Conversion geometry, 13/36/39/remainder columns, conversion semantics, group headers, empty state and the alpha.3.34 selector remain frozen. The alpha.3.36 message-only HWND updater hardening is also unchanged. Settings schema remains **9**. Windows fixed FileVersion/ProductVersion is `0.8.0.67`.

## v0.8.0-alpha.3.36 — Update Status Dispatch Hardening

Alpha 3.36 is a systemic updater-status reliability fix after real-Windows testing again reproduced an About page stuck on **正在检查更新... / Checking for updates...** even though closing and reopening Settings immediately showed the completed result.

The audit found that the previous primary notification and its 250 ms watchdog shared the same failure mode: both were `hwnd == nullptr` thread messages (`PostThreadMessageW` and `SetTimer(nullptr, ...)`). Nested Windows message loops may remove such thread messages without routing them through ALTRun Next's outer `App::Run()` loop. The background worker could therefore reach Available/UpToDate/Failed correctly while the visible Settings instance never received a completion refresh.

Update status delivery now goes through an App-lifetime **message-only HWND** (`HWND_MESSAGE`). Worker progress/completion uses window-targeted `PostMessageW`, and the active-only 250 ms watchdog is also an HWND-targeted timer on that dispatcher. Window messages continue through normal dispatch even inside nested Windows loops. A thread-message path remains only as an exceptional fallback if the invisible dispatcher cannot be created or posted to.

The About page also owns a second, active-only HWND timer while an update operation is running. It reconciles the visible controls directly from authoritative App state and stops automatically at a terminal state or when leaving/destroying About. The existing synchronous owner-draw repaint, 60-second absolute check watchdog, WinHTTP operation timeouts, stop-token cancellation, generation invalidation, safe staging/hash/install pipeline and update-channel semantics are preserved.

No Path Conversion behavior is touched; alpha.3.35 native row alignment and the validated alpha.3.34 DPI-bucketed selector remain frozen. Settings schema remains **9**. Windows fixed FileVersion/ProductVersion is `0.8.0.66`.

## v0.8.0-alpha.3.35 — Path Conversion Row Alignment Fix

Alpha 3.35 closes the result-row alignment issue found during real-Windows validation after the selector itself passed in alpha.3.34.

The report ListView no longer attaches a synthetic 1-pixel small-image list purely to force a 24 logical-pixel row height. At fractional DPI that artificial image metric was not vertically aligned with the native `LVS_EX_CHECKBOXES` state image, so the checkbox could sit visibly lower than the field/path/status text. Result rows now use the native Explorer ListView font/state-image metrics so the checkbox and all four text columns share the same Windows-controlled vertical baseline across DPI scales.

The manual four-space indentation before Target / Working directory / Custom icon has also been removed. The ListView already reserves the state-image slot for the checkbox, so the field text now starts at the native post-checkbox inset instead of receiving a second artificial indent.

The alpha.3.34 DPI-bucketed 4×4 coverage selector is frozen unchanged, as are the 960×560 default / 820×480 minimum window geometry, 13/36/39/remainder column policy, softened Header, flat actions, conversion rules, live counts and close behavior. Settings schema remains **9**. Windows fixed FileVersion/ProductVersion is `0.8.0.65`.

## v0.8.0-alpha.3.34 — Path Conversion Selector Final Fix

Alpha 3.34 is a narrowly scoped correction for the remaining mode-selector clarity issue reported in real-Windows alpha.3.33 testing.

The previous selector used hard-edged integer disk/ring fills, which removed blur but still produced a visually rough/unclear small circle on the actual desktop. The new selector keeps the 1:1 device-pixel rule but changes how those final pixels are generated: each DPI bucket has a hand-tuned physical size, and every destination pixel receives **4×4 subpixel coverage** for the outer ring, inner cutout and selected center dot. Coverage is blended directly against the current card background, so there is no intermediate bitmap, theme glyph, stretch or second interpolation pass.

The DPI buckets use 15/17/19/21/23 physical-pixel selector diameters with progressively tuned ring widths and center-dot sizes. This keeps the control visually stable across common 100/125/150/175/200% Windows scale ranges while allowing the edge pixels to be anti-aliased instead of jagged.

No other Path Conversion UI or behavior is changed in this release: mode-card border/background, flat action buttons, softened Header, 24 logical-pixel result-row baseline, responsive columns, live status, conversion semantics and close behavior remain frozen. Window geometry remains **960×560 / 820×480 logical pixels**; settings schema remains **9**. Windows fixed FileVersion/ProductVersion is `0.8.0.64`.

## v0.8.0-alpha.3.33 — Path Conversion Visual Closeout

Alpha 3.33 is the final real-Windows visual closeout for Path Conversion before returning to Launcher UX work.

The mode selector no longer uses either GDI ellipses or the Windows `BP_RADIOBUTTON` theme glyph. Instead, ALTRun Next draws a compact integer-raster ring directly on the final device-pixel grid. The ring diameter is DPI-aware but forced to an odd physical pixel count; the inner cutout and selected dot are rendered with the same pixel-span method. This removes the blurred dot from alpha.3.31 and the thick/soft themed ring visible in alpha.3.32.

Mode-card emphasis is also reduced: selected/focused cards now keep a **1-pixel accent border** rather than the heavier scaled border. The existing light selection fill remains, so the card still reads clearly without stacking multiple strong blue accents.

The result table receives only restrained polish. Header background/separators are lighter and vertical dividers are shorter, while the responsive Field/Current/Converted/Status widths and drag rules are unchanged. Report rows get a **24 logical-pixel** height baseline through a tiny ListView small-image-list, adding a little breathing room between shortcut group titles and field rows without increasing font size.

The alpha.3.32 flat Rescan/Apply buttons, Explorer ListView theme, empty-state position, live counts and non-blocking apply feedback remain unchanged. Window geometry remains **960×560 / 820×480 logical pixels**, settings schema remains **9**, and the shared top-level first-frame presentation lifecycle is preserved. Windows fixed FileVersion/ProductVersion is `0.8.0.63`.

## v0.8.0-alpha.3.32 — Path Conversion Visual Polish

Alpha 3.32 is the real-Windows visual closeout pass for Path Conversion. It intentionally leaves conversion logic, responsive column rules and the alpha.3.31 interaction model unchanged.

The mode-card selector now uses the Windows themed radio renderer (`BP_RADIOBUTTON`) instead of two hand-drawn GDI ellipses. The checked/unchecked glyph is rendered with native hot/pressed/disabled states on the current DPI grid, eliminating the blurred selected dot visible in alpha.3.31 screenshots while retaining the custom card shell.

**重新扫描** and **应用所选** now use flat owner-drawn buttons consistent with the rest of ALTRun Next. Rescan stays a neutral secondary action; Apply becomes the single accent primary action when at least one field is checked and falls back to a subdued disabled surface at zero selections.

The result surface is also quieter: the old sunken `WS_EX_CLIENTEDGE` frame is removed, Explorer theming is applied to the ListView, and its Header is custom-drawn with flat card/separator colors. Header hit-testing and the alpha.3.31 resize policy are unchanged: Field/Current/Converted remain manually adjustable and Status remains protected/elastic. The empty-state block is moved slightly upward to 44% of the result-area height for better visual balance.

Default/minimum geometry remains **960×560 / 820×480 logical pixels**. Conversion scope, missing-path defaults, live counts, non-blocking apply confirmation, X/Alt+F4/Escape close behavior, settings schemaVersion 9 and shared first-frame presentation are all preserved. Windows fixed FileVersion/ProductVersion is `0.8.0.62`.

## v0.8.0-alpha.3.31 — Path Conversion UX Redesign

Alpha 3.31 turns Path Conversion from an oversized utility table into a denser, clearer workflow without changing what is actually converted.

The default window is now **960×560 logical pixels** with an **820×480 logical** minimum. The two conversion directions are presented as compact owner-drawn cards: **便携化** (absolute → relative/environment variable) and **展开** (relative/environment variable → current-machine absolute). Each card is clickable across its whole surface, retains Tab/Space keyboard activation and supports Left/Right switching. **重新扫描** stays secondary, while the fixed conversion rule is separated into a muted helper line rather than being mixed into live result status.

The result list now uses **字段 / 当前路径 / 转换后路径 / 状态**. Field, Current and Converted use responsive default proportions with logical minimums; Status fills the remaining width and cannot be dragged narrower independently. User drags of the first three columns remain valid during the open dialog. Gridlines are removed, group headers are lighter, and an empty result set renders an explicit centered state instead of a blank white table.

The footer is simplified to live status plus a single **应用所选** action. The redundant **关闭** button is removed; X, Alt+F4 and Escape close the window. The Apply button is enabled only when at least one convertible field is checked, and the footer reports shortcut count, convertible fields and current selection. A successful apply no longer interrupts with a confirmation MessageBox: the list rescans immediately and the footer reports **已应用 N 个路径转换**. Error dialogs remain blocking so write failures cannot be missed.

Conversion semantics remain unchanged: Target, Working Directory and custom icon paths are handled; arguments, URLs, UNC paths and bare commands stay untouched, and missing paths start unchecked. Settings schema remains **9**, and the alpha.3.30 shared top-level first-frame presentation lifecycle is preserved. Windows fixed FileVersion/ProductVersion is `0.8.0.61`.

## v0.8.0-alpha.3.30 — Systematic Top-Level Window Presentation

Alpha 3.30 responds to the Shortcut Editor upper-left flash by auditing the full custom top-level window surface rather than adding another window-specific workaround.

The audit covers the five ALTRun Next-owned user-visible top-level windows: **Launcher, Settings, Shortcut Manager, Shortcut Editor and Path Conversion**. Settings and Shortcut Manager already had hardened first-frame behavior, while Launcher, Shortcut Editor and Path Conversion still contained legacy `CW_USEDEFAULT` creation paths that could leave a cached/default USER32 birth rectangle for DWM to expose during a later show.

A new shared `TopLevelWindowPresentation` module now owns the common presentation contract: target-monitor DPI probing, owner/work-area centering for owned popups, forced-off DWM show/hide transitions, cloaked first reveal with synchronous full-frame/child painting and flush, and cloak-before-destroy hiding. Settings and Shortcut Manager have been migrated away from their private copies of the DWM barrier so future fixes apply consistently.

Shortcut Editor now starts at a safe owner/monitor-resolved rectangle instead of `CW_USEDEFAULT`. Its content-dependent final height is calculated while hidden, the final window is re-centered, and only the completed frame is revealed. Path Conversion uses the same lifecycle for its fixed logical geometry. Launcher also receives an explicit safe birth rectangle and uses the shared reveal barrier only for its first presentation; repeated launcher invocations remain on the existing low-latency show path.

Windows-owned modal UI such as MessageBox and IFileOpenDialog is intentionally outside this custom HWND policy because its creation/presentation lifecycle is controlled by the OS.

Settings schema remains **9**. Shortcut Manager's default-size reset and 16/24/14/46 column policy, Shortcut Editor behavior and update hardening are unchanged. Windows fixed FileVersion/ProductVersion is `0.8.0.60`.

## v0.8.0-alpha.3.29 — Window Placement & Presentation Hardening

Alpha 3.29 is a real-Windows corrective pass over the unified placement work in alpha.3.28.

The Settings **靠近屏幕顶部** bug came from two independent placement implementations. The pre-creation geometry resolver correctly handled `top`, but `PositionForShow()` later treated every non-`last` mode as centered and moved the still-hidden Settings window back to the middle immediately before DWM reveal. Alpha 3.29 removes that divergence: both creation and show-time placement use the shared, unit-tested `settings_layout::ResolveWindowOrigin()` helper for near-top/center geometry, while `ClampRectToWorkArea()` remains the single last-position clamp.

Shortcut Manager's occasional upper-left flash from the tray is hardened at the HWND/compositor boundary rather than patched after the fact. The Manager no longer creates its real top-level HWND with `CW_USEDEFAULT` and then moves it while hidden. Before `CreateWindowExW`, ALTRun Next now resolves the configured monitor, probes that monitor's DPI without exposing a real Manager window, calculates the 720×480 logical default size, and computes the final top/center/last rectangle. The real HWND is born directly at that final rectangle.

The Manager also adopts the proven Settings first-frame DWM barrier: window transitions are forced off; first show happens while DWM-cloaked; the complete frame and child tree are synchronously painted and flushed before uncloak; close cloaks before hide/destroy. Creation-time and show-time Manager placement share `ResolveShortcutManagerRect()`, so there is no alternate geometry path that can regress independently.

Settings schema remains **9**, commands schema remains 2, usage schema remains 1 and provider-cache schema remains 2. Manager default-size/reset behavior and the validated 16/24/14/46 column policy are unchanged. Windows fixed FileVersion/ProductVersion is `0.8.0.59`.

## v0.8.0-alpha.3.28 — Unified Window Placement

Alpha 3.28 closes the Settings/Shortcut Manager placement model before Launcher UX work begins.

The General -> **Window placement** card now uses one vocabulary across all top-level windows: **启动器显示器**, **启动器窗口位置**, **设置窗口位置** and **快捷项管理窗口位置**. Every position ComboBox uses the same three choices: **靠近屏幕顶部**, **屏幕居中** and **上次位置**. Settings gains the missing top mode, and Shortcut Manager gains its own independent placement preference. Launcher keeps its separate target-monitor selector because it is the frequently hotkey-invoked primary window; Settings and Shortcut Manager resolve top/center on the monitor containing the mouse.

Shortcut Manager now separates **size** from **position** completely. A newly opened Manager always starts at the published **720×480 logical** default even if the previous instance was temporarily enlarged to inspect a long path. Manual resizing is still unrestricted for that open instance. When **上次位置** is selected, only the last X/Y coordinates are persisted; reopening restores those coordinates with a fresh default-size window and clamps it into the nearest monitor work area. The old local `WINDOWPLACEMENT` cache has been removed.

Settings top placement follows the same near-top vertical rule already used by Launcher, while Settings center/last behavior remains work-area safe. The placement card expands to four rows without changing the rest of the General-page hierarchy.

Because Shortcut Manager placement is now persisted user configuration, **settings.json advances from schemaVersion 8 to schemaVersion 9**. Existing schema-8 documents migrate atomically, preserving Launcher and Settings placement and receiving a centered Shortcut Manager default with no valid last position. Commands remain schemaVersion 2, usage schemaVersion 1 and provider-cache schemaVersion 2. Windows fixed FileVersion/ProductVersion is `0.8.0.58`.

## v0.8.0-alpha.3.27 — Shortcut Manager Column Lifecycle Closeout

Alpha 3.27 closes the remaining Shortcut Manager column-state issue exposed by real-Windows reopen testing.

The intended first-open table remains **16% Keywords / 24% Name / 14% Type / 46% Target**. Manual Header dragging is still supported, but those widths now belong only to the currently open Shortcut Manager window. Closing the Manager destroys that temporary column state; reopening starts again from the validated default balance. Window placement continues to be retained independently.

The previous first-three-column cache has therefore been removed rather than repaired. This avoids a second bug in the old restore path: saved widths were being applied while the newly created ListView/Header still had effectively zero layout width, so the normal width constraints could collapse the restored values down to their minimums before the first real Layout pass.

The minimum widths have also been aligned with the visually accepted first-open table at the Manager's 720×480 minimum geometry: **107 logical pixels for Keywords, 161 for Name and 94 for Type**. Target keeps its 180 logical-pixel minimum. The practical effect is that users can widen the first three columns and borrow space from Target, but can no longer shrink them below the same baseline seen on a fresh open.

Alpha 3.26's Advanced disclosure/administrator-checkbox fixes, alpha.3.24 Editor geometry and alpha.3.23 updater hardening remain unchanged. Settings schema remains 8 and commands schema remains 2. Windows fixed FileVersion/ProductVersion is `0.8.0.57`.

## v0.8.0-alpha.3.26 — Shortcut Interaction Reliability & Column Default Repair

Alpha 3.26 is a corrective real-Windows pass over alpha.3.25. It fixes three interaction/layout defects without changing Shortcut Editor geometry or shortcut execution behavior.

The Advanced disclosure no longer uses mouse-origin bookkeeping or focus transfer after `BN_CLICKED`. Those alpha.3.25 mechanics were unnecessary once the owner-drawn header stopped using accent focus styling, and real-Windows testing showed they could make repeated pointer clicks feel unreliable. Advanced is again driven directly by the native Button notification path. Its renderer intentionally ignores `ODS_FOCUS`, so the classic dotted Win32 focus rectangle is never painted, while click/Tab/Space behavior remains native.

The **Run as administrator** control no longer spans the entire Advanced value column. Its HWND is sized to the checkbox's ideal content width through `BCM_GETIDEALSIZE`, with a font- and DPI-aware fallback using the active text metrics plus the native checkmark width. Only the visible checkbox/label area is therefore clickable.

The Shortcut Manager column screenshot also exposed a separate state bug rather than a bad 16/24/14/46 ratio. `ApplyLanguage()` updates Header text with `HDI_TEXT`, which raises `HDN_ITEMCHANGED`; the old handler treated every item-changed notification as a width edit and enabled `customColumnWidths_` before the first layout. That bypassed the percentage defaults and left the table using creation/minimum-like widths. Alpha 3.26 only treats `HDN_ITEMCHANGED` as a user resize when `HDI_WIDTH` is present. Fresh Manager windows can now apply the intended **16% Keywords / 24% Name / 14% Type / 46% Target** layout, while real user drag widths still enter custom mode and remain elastic against Target.

Alpha 3.24 Editor geometry and alpha.3.23 updater hardening are preserved unchanged. Settings schema remains 8 and commands schema remains 2. Windows fixed FileVersion/ProductVersion is `0.8.0.56`.

## v0.8.0-alpha.3.25 — Shortcut Workflow Focus & Column Balance Closeout

Alpha 3.25 closes the two remaining real-Windows Shortcut workflow polish items without reopening Shortcut Editor geometry or shortcut behavior.

Shortcut Editor now distinguishes pointer activation from keyboard activation for **Advanced**. A left-click still expands/collapses the section through the native owner-drawn Button, but after the click completes focus returns to the dialog surface, so the classic dotted Win32 Button focus rectangle does not remain around the header. Tab navigation and Space activation retain native Button focus and visible keyboard focus cues. The 590-pixel editor width, current vertical rhythm, measured ComboBox sizing and 18-pixel Advanced right inset are unchanged.

Shortcut Manager also receives a fresh default column balance. The four columns now resolve to **16% Keywords / 24% Name / 14% Type / 46% Target** instead of 22/26/12/40. This gives typical names such as *Command Prompt* and fixed type labels more usable width while keeping Target the largest column without allowing it to visually overwhelm the table. Responsive minimums are now 88/128/88 logical pixels for Keywords/Name/Type and 180 logical pixels for Target. Existing constrained Header dragging, elastic Target behavior and in-session custom column widths remain intact.

Alpha 3.23 update hardening is preserved unchanged, as are Settings schemaVersion 8 and commands schemaVersion 2. Windows fixed FileVersion/ProductVersion is `0.8.0.55`.

## v0.8.0-alpha.3.24 — Shortcut Editor Density Closeout

Alpha 3.24 is the final density closeout for the native Shortcut Editor based on the latest real-Windows screenshots. It changes visual geometry only: shortcut persistence, Runtime Input, path conversion, pickers, execution and schemas remain frozen.

The editor width moves from 620 to 590 logical pixels. Name/Keywords keeps the 36:64 split with a smaller 190-pixel Name minimum, while the inline label column moves 8 pixels left in both Chinese and English. Target type and Runtime input remain native, aligned ComboBoxes sized from the active localized text; their shared clamp is widened to 158–185 logical pixels and the measured chrome allowance gains 8 pixels so values such as **URL 编码（UTF-8）** no longer crowd the drop arrow.

Advanced keeps the arrow + semibold title + neutral continuation divider, but removes the blue accent underline and accent pressed surface. Mouse interaction therefore does not leave a web-tab-like selected treatment; Windows keyboard focus cues are drawn only when the UI state exposes focus cues. Expanded Advanced fields, picker buttons and **Run as administrator** also stop at an 18-logical-pixel right inset before the normal form edge, restoring a quieter secondary hierarchy.

Vertical metrics are intentionally unchanged from alpha.3.23: the 16-pixel top rhythm, 28-pixel rows, font-derived native Edit height, Runtime Input/Test Input expansion and adaptive footer remain intact. Alpha.3.23's interruptible WinHTTP update transport, explicit CheckTimedOut state and 60-second App watchdog are preserved unchanged. Settings schema remains 8 and commands schema remains 2. Windows fixed FileVersion/ProductVersion is `0.8.0.54`.

## v0.8.0-alpha.3.23 — Shortcut Editor Compact Width & Update Check Hardening

Alpha 3.23 closes the final Shortcut Editor horizontal-density pass and hardens the update checker against a second class of real-Windows **正在检查更新... / Checking for updates...** stalls.

Shortcut Editor is reduced from 680 to 620 logical pixels. Name/Keywords now use a 36:64 split with a 200-pixel Name minimum, and the two inline native ComboBox controls no longer reserve a fixed 190 pixels. Their localized items are measured with the active UI font via `GetTextExtentPoint32W`; text width, native drop-arrow chrome and padding are combined, clamped to 150–175 logical pixels, and the wider result is shared by Target type and Runtime input so both rows remain aligned in Chinese and English. The existing native Edit height calculation, IME behavior, Runtime Input, modern pickers, Advanced expansion and adaptive footer remain unchanged. Top/section rhythm is tightened by only 8 logical pixels overall.

The update audit found that alpha.3.8's 250 ms reconciliation timer and alpha.3.16's synchronous owner-draw repaint fixed missed delivery and stale pixels, but did not bound the underlying synchronous WinHTTP worker. If a request itself stopped making progress, App state could legitimately remain in `Checking`, so every repaint correctly kept showing the same state. WinHTTP request handles are now safely closeable from a stop callback, timeout/redirect setup failures are no longer ignored, manifest/package bodies use bounded fixed-size `WinHttpReadData` loops instead of `WinHttpQueryDataAvailable`, and native timeout errors map to an explicit retryable CheckTimedOut state. A 60-second App watchdog invalidates the old generation and requests cancellation, preventing a late worker result from overwriting the timeout state. The Ready-to-Install handoff flag is also consumed under the update mutex, removing a previously unsynchronized UI/worker access.

Settings schema remains 8 and commands schema remains 2. Shortcut Manager stays at the validated 720×480 minimum/default geometry. Windows fixed FileVersion/ProductVersion is `0.8.0.53`.

## v0.8.0-alpha.3.22 — Shortcut Manager Compact Default & Editor Interaction Polish

Alpha 3.22 finishes the latest real-Windows Shortcut workflow pass without changing shortcut execution or schemas.

Shortcut Manager now opens at the same compact geometry already proven by manual resizing: the fresh-session default is 720x480 logical pixels, matching the existing minimum track size. The manager still preserves its established session window-placement behavior, so a user who resizes or moves it continues to reopen at that captured placement instead of being forced back to the compact default. Default/minimum geometry is now expressed through shared constants rather than repeated literal sizes.

Shortcut Editor's two inline ComboBox rows now share one 190-logical-pixel value width. **目标类型 / Target type** and **运行时输入 / Runtime input** therefore align their controls and right-hand explanatory text to the same vertical column. The Advanced keyboard-focus treatment is reduced again: section text remains neutral while only a short accent underline indicates focus, preserving native Button Tab/Space/click semantics without a link-like blue title.

The editor also adopts the already-validated Settings ComboBox focus rule. If Target type or Runtime input still owns keyboard focus after a selection, clicking the dialog background, another child control, or the non-client/title area moves focus away before normal click processing continues. This removes the persistent native selection highlight without replacing ComboBox controls or weakening keyboard navigation.

Vertical footer rhythm is slightly tighter: content-to-footer gap is 16 logical pixels, separator gap 10, and bottom margin 16, while the 32-pixel action buttons and adaptive content-height model remain unchanged. Native Edit/ComboBox controls, IME, Runtime Input/Test Input logic, modern pickers, path conversion, saved manager placement, column dragging and shortcut schemas are unchanged. Settings remains schemaVersion 8 and commands remains schemaVersion 2. Windows fixed FileVersion/ProductVersion is `0.8.0.52`.

## v0.8.0-alpha.3.21 — Shortcut Editor Final Visual Polish

Alpha 3.21 returns to the Shortcut Editor after the Settings reliability work and performs the final compact-layout pass based on real-Windows screenshots of the collapsed, Advanced-expanded and Runtime Input + Advanced states.

The editor width is reduced from 720 to 680 logical pixels so long Advanced fields no longer dominate the dialog. The top identity row shifts from a 42/58 Name/Keywords split to 38/62, giving the keyword/alias field more practical space while keeping short display names compact. Existing native single-line Edit controls, font-derived edit height, IME behavior, modern IFileOpenDialog pickers, dynamic Runtime Input/Test Input expansion and content-derived dialog height remain unchanged.

Auto-detect presentation now distinguishes an empty Target from an actual Application inference. The core model intentionally continues to use Application as its internal empty-target fallback, preserving all command/model behavior, but the editor shows **等待输入目标 / Waiting for target** while Auto detect is selected and Target is blank. Once a target exists, the existing **识别为 / Detected** state resumes; explicit type selections continue to show **手动指定 / Override**.

The Advanced header keeps native Button keyboard semantics but replaces the classic dotted focus rectangle with a lightweight accent treatment: focused text turns accent-colored and receives a short underline while the continuation divider stays neutral. Advanced field geometry remains stable, with a small extra gap before **以管理员身份运行 / Run as administrator** so the checkbox reads as an option belonging to the section rather than a fourth text field.

Footer actions remain intentionally unchanged: Test stays on the left, Save remains the single primary accent action, Cancel stays secondary on the right, and Enter-to-save behavior is preserved. No shortcut schema, execution, Runtime Input, path conversion, picker or Settings behavior changes are introduced. Settings remains schemaVersion 8 and commands remains schemaVersion 2. Windows fixed FileVersion/ProductVersion is `0.8.0.51`.

## v0.8.0-alpha.3.20 — Settings DWM First-Frame Barrier

Alpha 3.20 moves the remaining Settings flash fix from USER32 placement logic to the Desktop Window Manager composition layer.

By alpha.3.19 the real Settings HWND is born directly at its final Center/Last rectangle with the correct target-monitor DPI-aware outer size, and the open/close paths no longer use `SW_SHOWNORMAL`. Real-Windows validation nevertheless still observed a rare upper-left flash. With no remaining real Settings placement at the monitor origin, the failure is treated as a compositor transition/redirection-surface race rather than another positioning bug.

Settings now disables its own DWM transitions through `DWMWA_TRANSITIONS_FORCEDISABLED`. On first show, the window is explicitly cloaked with `DWMWA_CLOAK`, positioned, shown logically with `SW_SHOW`, synchronously paints client + child + non-client content, flushes DWM while still cloaked, and only then uncloaks and flushes again. The first user-visible DWM frame is therefore the fully-painted frame at the final Settings rectangle.

The close path mirrors the same barrier in reverse: capture the real visible rectangle, cloak the Settings HWND, flush DWM, then hide with `SWP_HIDEWINDOW`, persist the captured position and destroy the hidden HWND. Any cached DWM redirect surface or teardown transition remains invisible.

These changes are scoped to Settings only. Shortcut Manager, Shortcut Editor, Launcher, update-check logic, window-placement semantics and schemas are unchanged. The project already linked `dwmapi`, so no new runtime dependency is introduced. Settings remains schemaVersion 8. Windows fixed FileVersion/ProductVersion is `0.8.0.50`.

## v0.8.0-alpha.3.19 — Settings Birth-Rect Synchronization

Alpha 3.19 removes the last real upper-left placement from the Settings HWND lifecycle instead of trying to mask it later with ShowWindow ordering.

The audit found that alpha.3.18 still created the actual Settings top-level HWND at the target monitor work-area origin. It was hidden and immediately moved to Center/Last afterwards, but that monitor-origin rectangle still existed as the window's creation/normal-transition state. This differs from the stable Shortcut Manager path, which restores complete native placement state. It also explains why an upper-left frame could survive several later fixes even though the visible SetWindowPos/Show/close sequence looked correct.

The real Settings HWND is now created only after its target monitor, DPI, fixed-client outer size and final Center/Last rectangle are known. A never-visible 1x1 `WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE` probe is created on the target monitor solely to obtain that monitor's DPI through `GetDpiForWindow`, then destroyed before the Settings HWND exists. The actual Settings top-level window is created with `CreateWindowExW` directly at its final rectangle with the exact DPI-aware outer size for the 820x620 logical client area.

After creation, Settings verifies the actual HWND DPI/outer size and performs only hidden correction if Windows resolved a different DPI context. `PositionForShow()` still provides final Center/Last clamping, but there is no longer any real Settings HWND whose birth rectangle is the monitor upper-left corner.

Alpha.3.18's `SW_SHOW` current-rectangle semantics and hide-before-destroy close path remain unchanged, as do the alpha.3.16 owner-draw update-status repaint fix, destroy-on-close behavior, About Show-first routing and all frozen Shortcut Editor/Manager/search behavior. Settings remains schemaVersion 8. Windows fixed FileVersion/ProductVersion is `0.8.0.49`.

## v0.8.0-alpha.3.18 — Settings Native Show/Close Lifecycle Cleanup

Alpha 3.18 removes the last synthetic first-show workaround from Settings and aligns the window lifecycle with the proven native behavior already used by Shortcut Manager.

Real-Windows validation showed alpha.3.17 could open Centered correctly but, after the user manually dragged the Settings window elsewhere, closing could briefly paint the window back at the centered position before destruction. The settings-position persistence path was not responsible: RememberSettingsPosition only writes coordinates and never repositions or refreshes the Settings HWND. The remaining suspect was the artificial `SW_HIDE -> SW_SHOWNORMAL` lifecycle. Microsoft documents `SW_SHOWNORMAL` as restoring a window to its original size and position, whereas `SW_SHOW` displays the window in its current size and position. Settings already owns its rectangle through `SetWindowPos`, so asking USER32 to restore a separate normal placement is unnecessary and can diverge from the rectangle after a manual drag.

The new lifecycle is deliberately simpler. Create builds and sizes the HWND hidden, resolves Center/Last with `PositionForShow()`, and does not call `ShowWindow` at all. The real open path re-resolves placement and uses `SW_SHOW`, so USER32 displays exactly the rectangle Settings already owns instead of restoring a second placement state.

Closing is now visually atomic: the current visible rectangle is captured first, the top-level HWND is immediately hidden with `SWP_HIDEWINDOW`, and only then are pending settings committed, the last position persisted, timers stopped and `DestroyWindow` called. Any USER32/DWM bookkeeping during destruction therefore occurs while the window is already invisible and cannot produce a final centered flash.

Centered placement, Last-position persistence, destroy-on-close, About Show-first routing, alpha.3.16 update-status repaint reliability, and all frozen Shortcut Editor/Manager/search behavior remain unchanged. Settings remains schemaVersion 8. Windows fixed FileVersion/ProductVersion is `0.8.0.48`.

## v0.8.0-alpha.3.17 — Settings Center Placement Finalization

Alpha 3.17 corrects the Center-placement regression exposed by real-Windows validation of alpha.3.16. Last-position mode already behaved correctly, while Center mode consistently opened at the upper-left corner of the selected monitor.

The reason was deterministic: alpha.3.16 intentionally created a hidden Center-mode Settings HWND at the target monitor's work-area origin so the initial DPI context belonged to the correct monitor. It then expected `SetWindowPos(... SWP_SHOWWINDOW)` to replace that creation point with the centered rectangle on the first visible transition. In practice, USER32 can retain the creation point as the HWND's first normal placement, so Last mode worked because its creation point was already the desired saved position, while Center mode exposed the monitor-origin anchor.

Alpha 3.17 keeps explicit non-`CW_USEDEFAULT` creation and the correct monitor/DPI anchor, but changes the first-show sequence. After controls, fonts and final DPI-aware outer size are established, `PositionForShow()` now moves the still-hidden HWND to the real Center/Last rectangle without showing it. A hidden `ShowWindow(SW_HIDE)` call then consumes USER32's first-show state only after the normal position is already correct. The real `Show()` re-resolves placement and uses `SW_SHOWNORMAL`; Center therefore restores from a centered normal position instead of the monitor origin, while Last keeps its already validated semantics.

The alpha.3.16 owner-draw update-status repaint fix remains unchanged. Settings still destroys its HWND on close, About still routes through Show before selecting the About page, and all Shortcut Editor/Manager/search behavior remains frozen. Settings remains schemaVersion 8. Windows fixed FileVersion/ProductVersion is `0.8.0.47`.

## v0.8.0-alpha.3.16 — Settings First-Paint & Update Status Reliability

Alpha 3.16 is a targeted Settings reliability fix for two real-Windows regressions that remained after the destroy/recreate lifecycle work.

The update checker itself was completing correctly, but the About-page status surface is an `SS_OWNERDRAW` static. `RefreshUpdateStatus()` updated its backing text with `SetWindowTextW` while only invalidating the adjacent action button. On affected runs, the existing “正在检查更新... / Checking for updates...” pixels therefore remained on screen even after App state had already reached Available/UpToDate/Failed; recreating Settings forced a full repaint and exposed the correct result. Alpha 3.16 keeps the existing posted notifications and 250 ms reconciliation watchdog, but now synchronously redraws both the owner-drawn status surface and action button whenever update state is reconciled.

Settings first-show placement no longer relies on `CW_USEDEFAULT` plus a synthetic first `ShowWindow(SW_HIDE)` call. A recreated Settings HWND is now created hidden on the monitor that will own the session — saved-position monitor for Last placement, current-cursor monitor for Centered placement — so its initial DPI context is correct without carrying a default upper-left normal placement. The configured final rectangle is then positioned and revealed atomically with `SetWindowPos(... SWP_SHOWWINDOW)`. There is no separate `SW_HIDE` first-show consumption and no subsequent `SW_SHOWNORMAL` step that can expose or restore an upper-left frame.

Centered and Last-position semantics, destroy-on-close behavior, About's shared Show-first entry path, update worker/network behavior, Shortcut Editor alpha.3.15 layout and all frozen shortcut/search functionality remain unchanged. Settings remains schemaVersion 8. Windows fixed FileVersion/ProductVersion is `0.8.0.46`.

## v0.8.0-alpha.3.15 — Shortcut Editor Visual Hierarchy & Adaptive Layout

Alpha 3.15 is the visual-hierarchy closeout pass for the native Shortcut Editor after alpha.3.14 established the compact row model and font-derived Edit height. It keeps the shortcut model, Runtime Input semantics and modern Shell pickers unchanged while tightening the relationships between labels, fields and actions seen in real-Windows validation.

Primary stacked labels — **名称 / Name**, **快捷词 / Keywords**, and **目标 / Target** — remain left aligned and semibold. Inline form labels — Target type, Runtime input, Test input, Fixed arguments, Working directory and Icon — now share one fixed label column, use normal Body weight, and right-align toward the value column with a deliberate 12-logical-pixel gap. Advanced fields therefore read as one compact form instead of several unrelated left-aligned text blocks; the administrator checkbox aligns with the same value column.

In-form browse actions now match the native Edit field height and vertical position. File, Folder and Choose actions use the same restrained owner-drawn secondary-button surface as Shortcut Manager, while the bottom action row keeps Test/Cancel secondary and makes Save the single accent primary action. The editor preserves its previous Enter-to-save workflow for native Edit and closed ComboBox controls even though Save is now owner-drawn.

Window height is no longer selected from separate collapsed/expanded constants. The editor measures the currently visible content rhythm — including Runtime Test and Advanced state — and reserves a fixed 24-logical-pixel content-to-footer gap before the footer separator/actions. This removes the remaining collapsed-state dead space while keeping all four dynamic combinations stable. Helper text is also slightly de-emphasized without reducing font size.

No shortcut schema/model, commands.json format, target inference, icon-auto semantics, Runtime Input behavior, Settings placement/lifecycle, Shortcut Manager behavior or modern picker flow changes. Settings remains schemaVersion 8. Windows fixed FileVersion/ProductVersion is `0.8.0.45`.

## v0.8.0-alpha.3.14 — Shortcut Editor Density & Native Edit Alignment

Alpha 3.14 is a focused real-Windows density pass on the Shortcut Editor after alpha.3.13 validated the modern Shell pickers and owner-drawn Advanced header. It keeps those parts frozen and targets the two remaining desktop-validation issues: excessive vertical whitespace and native single-line Edit fields whose border was substantially taller than their text/caret.

Native Edit height is now derived from the active Body font metrics, following the same proven approach already used by Shortcut Manager's search box. The editor keeps real single-line Win32 Edit controls, IME behavior, tab navigation and keyboard semantics; it does not switch to multiline or custom-painted text fields just to simulate vertical centering. Edit controls also receive restrained left/right text margins so typed values and cue banners no longer sit against the border.

The Advanced section is reorganized from three stacked label-then-field blocks into dense label/value rows: **固定参数 / Fixed arguments**, **工作目录 / Working directory**, and **图标 / Icon** now keep their labels on the left and native inputs/actions on the same row. **以管理员身份运行 / Run as administrator** aligns with the value column below them. This removes repeated blank bands while preserving the exact field order and semantics.

The editor's collapsed/expanded geometry and Runtime-Test expansion are reduced to match the new content density. Target browsing, Runtime Input, Advanced toggle behavior, icon auto semantics, shortcut model/persistence, Settings placement/lifecycle and Shortcut Manager behavior are unchanged. Settings remains schemaVersion 8. Windows fixed FileVersion/ProductVersion is `0.8.0.44`.

## v0.8.0-alpha.3.13 — Shortcut Editor Native Polish & Modern Pickers

Alpha 3.13 is the real-Windows polish pass following the alpha.3.12 Shortcut Editor visual consolidation. It keeps the compact form structure and shortcut/runtime semantics unchanged, while removing two pieces of UI that still felt visibly legacy in desktop validation.

The redundant **图标 / Icon → 自动 / Auto** button is removed. An empty Icon field already means “follow the target icon automatically”, so the extra button only cleared the field and duplicated an existing state. The Icon row is now simply the wider native edit plus **选择… / Choose…**; clearing the field manually returns to automatic target-icon behavior.

Target-file, target-folder, Working-directory and Icon browsing now prefer the modern Windows `IFileOpenDialog`. Folder selection uses `FOS_PICKFOLDERS`, replacing the old tree-style `SHBrowseForFolderW` experience on normal Windows 10/11 systems. The dialogs use scenario-specific localized titles and localized filter names, seed their initial folder from the current field/target when possible, and still keep the existing legacy common-dialog/folder-browser code as a compatibility fallback if the modern shell dialog cannot be created.

The **高级选项 / Advanced** row is now a real owner-drawn section header instead of a full-width native pushbutton. It keeps keyboard focus and button behavior, but paints as a lightweight text header with a continuation divider and a compact focus indication, avoiding the large gray button treatment seen in alpha.3.12 real-machine screenshots. The rest of the editor stays native: Edit/ComboBox controls, file semantics, modal message loop, IME and tab navigation are not replaced by a custom UI framework.

No shortcut schema/model, commands.json format, keyword/alias parsing, automatic Name suggestion, target-type inference, Runtime Input semantics, Test execution, Working Directory semantics, Icon auto semantics, administrator launch, Shortcut Manager, Path Conversion, Provider, Hotkey or search/ranking behavior changes. Settings remains schemaVersion 8. Windows fixed FileVersion/ProductVersion is `0.8.0.43`.
## v0.8.0-alpha.3.12 — Shortcut Editor Visual Consolidation

Alpha 3.12 is a presentation-only consolidation of the native Shortcut Editor. It keeps the existing shortcut model and execution semantics intact while bringing the editor into the same restrained application visual language as Settings and Shortcut Manager.

The editor is now slightly wider and materially more compact vertically. **名称 / Name** and **快捷词 / Keywords** share the first row, with Keywords retaining the comma-separated alias hint. **目标 / Target** remains a full-width path-sensitive row with the existing File/Folder actions. Target type is presented as a compact metadata row, followed by a lightly separated **运行时输入 / Runtime input** section. Test input continues to appear only for Raw/URL-encoded runtime modes.

**高级选项 / Advanced** is now a full-width collapsible header rather than a small isolated button. Its existing contents remain in the same semantic order: Fixed arguments, Working directory, Icon, then Run as administrator. Working-directory and icon auto behavior are expressed through native edit cue text instead of long labels.

The editor now uses `ui::kApplicationPalette`, Body/BodySemibold typography, flat native bordered Edit controls, muted helper text, subtle runtime/footer separators, and a restrained bottom action bar. Test and Cancel stay secondary; Save is the only accent primary action. Native Edit/ComboBox behavior, tab navigation, IME behavior, file/folder/icon pickers and the modal message loop are deliberately retained rather than replaced by a custom form framework.

Dynamic behavior is unchanged: Runtime Input still uses None/Raw/URL-encoded modes, `{input}` placement rules are unchanged, the Runtime Test row and Advanced section still resize through the existing `RefreshDynamicLayout()` path, and the previously hardened redraw logic remains in place.

No shortcut schema/model, commands.json format, keyword/alias parsing, automatic Name suggestion, target-type inference, Runtime Input semantics, Test execution, Working Directory, Icon auto behavior, administrator launch, Shortcut Manager, Path Conversion, Provider, Hotkey or search/ranking behavior changes. Settings remains schemaVersion 8. Windows fixed FileVersion/ProductVersion is `0.8.0.42`.
## v0.8.0-alpha.3.11 — About Entry Placement Fix

Alpha 3.11 closes the remaining Settings-placement regression specific to the tray **关于… / About...** entry.

Real-Windows validation of alpha.3.10 proved that the generic Settings centering path is correct: tray **设置… / Settings...** opens centered and **上次位置 / Last position** is preserved. The remaining defect was isolated to `SettingsWindow::ShowAbout()`. Unlike the normal Settings entry, About changed the hidden Settings HWND to the About page *before* running the shared top-level `Show()` path. That made About the only entry route that mutated the recreated hidden window before the first visible Settings show.

Alpha 3.11 removes that path divergence. `ShowAbout()` now calls the exact same `Show()` routine used by tray Settings first, so creation, hidden first-show consumption, `PositionForShow()`, restore behavior and foreground activation all complete identically. Only after the top-level window has been shown in its configured position does it switch to `Page::About`. This keeps **屏幕居中 / Centered** and **上次位置 / Last position** behavior identical between tray Settings and tray About.

No Settings lifecycle rollback is involved: closing still destroys the Settings HWND, and alpha.3.10's restored hidden first `ShowWindow(SW_HIDE)` remains in place. The alpha.3.8 update-status reconciliation watchdog is also unchanged.

No Settings schema, updater endpoint/manifest, Shortcut Manager, Shortcut Editor, Runtime Input, Path Conversion, Provider, Hotkey or search/ranking behavior changes. Settings remains schemaVersion 8. Windows fixed FileVersion/ProductVersion is `0.8.0.41`.
## v0.8.0-alpha.3.10 — Settings Placement Regression Fix

Alpha 3.10 is a targeted correction after real-Windows testing showed that the previous alpha.3.8/3.9 placement changes still did not restore **屏幕居中 / Centered** when Settings was recreated from the tray.

The regression was traced by diffing the last known-good alpha.3.6 Settings lifecycle against alpha.3.7+. The important difference was not the center-coordinate calculation: alpha.3.7 removed the apparently redundant `ShowWindow(hwnd_, SW_HIDE)` call at the end of `SettingsWindow::Create()`. In alpha.3.6 that first `ShowWindow` call consumed USER32's first-show/default-placement state while the HWND was still invisible. Once it was removed, every destroy/recreate cycle left a fresh native first-show placement pending, which could overwrite our centered position with Windows' upper-left cascade. **上次位置 / Last position** often appeared correct because its explicit saved coordinates masked the regression.

Alpha 3.10 therefore removes the alpha.3.9 explicit creation-anchor workaround and restores the proven alpha.3.6 first-show sequence while keeping alpha.3.7's destroy-on-close lifecycle: create with the normal `CW_USEDEFAULT` coordinates, finish constructing/layouting the hidden window, call `ShowWindow(SW_HIDE)` once to consume the native first-show state, then on the real open call `PositionForShow()` followed by `ShowWindow(SW_SHOWNORMAL)`. There is no second post-show reposition pass.

The alpha.3.8 update-status reconciliation watchdog remains unchanged. Manual update checking is valid even when no newer version exists: a successful check must leave **正在检查更新… / Checking for updates...** and end in **已是最新版本 / Up to date**.

No Settings schema, updater endpoint/manifest, Shortcut Manager, Shortcut Editor, Runtime Input, Path Conversion, Provider, Hotkey or search/ranking behavior changes. Settings remains schemaVersion 8. Windows fixed FileVersion/ProductVersion is `0.8.0.40`.
## v0.8.0-alpha.3.9 — Settings First-Show Placement Final Fix

Alpha 3.9 is a second, narrower placement fix after real-Windows validation showed that alpha.3.8 could still reopen Settings near USER32's default upper-left cascade position.

The remaining cause was not the Center/Last calculation itself. Settings was still created with `CW_USEDEFAULT` coordinates, so the recreated top-level HWND retained a native first-show placement even though alpha.3.8 combined positioning and visibility. Alpha 3.9 removes `CW_USEDEFAULT` from Settings creation entirely. The hidden HWND is created at an explicit point on the intended monitor: the saved Last-position monitor when valid, otherwise the current mouse monitor. This also gives `GetDpiForWindow` the correct monitor context before the fixed client size is converted to a DPI-aware outer size.

On first display, Settings is positioned once while hidden, shown normally, then positioned once more after USER32 and any Per-Monitor-DPI transition have settled. This post-show correction is intentional and only runs for a newly hidden/recreated Settings window; reopening an already visible or merely minimized window does not recenter it. **屏幕居中 / Centered** and **上次位置 / Last position** keep their existing monitor/work-area semantics.

The alpha.3.8 update-status reconciliation watchdog is unchanged. Manual update checking can be validated even when there is no newer build: a successful check must leave **正在检查更新… / Checking for updates...** and reach the terminal **已是最新版本 / Up to date** state.

No Settings schema, updater endpoint/manifest, Shortcut Manager, Shortcut Editor, Runtime Input, Path Conversion, Provider, Hotkey or search/ranking behavior changes. Settings remains schemaVersion 8. Windows fixed FileVersion/ProductVersion is `0.8.0.39`.
## v0.8.0-alpha.3.8 — Settings Placement & Update Status Reliability

Alpha 3.8 is a narrow reliability follow-up for two real-Windows regressions exposed by the alpha.3.7 destroy/recreate Settings lifecycle. It does not redesign Settings or start the Shortcut Editor visual pass.

A newly recreated Settings window is now positioned and revealed in one SetWindowPos operation with `SWP_SHOWWINDOW`. The previous hidden-position-then-first-`ShowWindow(SW_SHOWNORMAL)` sequence could let Windows reapply the original `CW_USEDEFAULT` placement, which made tray-opened Settings appear near the upper-left regardless of the selected placement mode. **屏幕居中 / Centered** continues to center on the current mouse monitor work area, while **上次位置 / Last position** is clamped back into a valid work area. Closing Settings also records the actual final window rectangle, so Last position reflects the previous Settings session rather than only the last completed drag.

Update checking keeps the existing background worker and `PostThreadMessageW` notifications, but adds a 250 ms UI-thread reconciliation timer only while an update worker is active. This watchdog reads the already-authoritative App update state through the existing handler, so a missed thread notification can no longer leave About stuck on **正在检查更新… / Checking for updates...** until Settings is reopened. The timer stops as soon as the worker reaches a terminal state, and the handler no longer joins a cancelled worker merely because the visible snapshot was reset to idle.

No update endpoint, manifest format, install flow, shortcut persistence, Provider, Hotkey, Runtime Input, Path Conversion, search/ranking or Settings schema changes. Settings remains schemaVersion 8. Windows fixed FileVersion/ProductVersion is `0.8.0.38`.

## v0.8.0-alpha.3.7 — Window Lifecycle & Confirmation Polish

Alpha 3.7 is a focused lifecycle follow-up after the Shortcut Manager naming pass. It does not redesign Settings, Shortcut Manager or Shortcut Editor.

Closing Settings now destroys its native top-level window instead of leaving a hidden control tree resident. Reopening Settings recreates a clean window and returns to **常规 / General**; calling Settings again while the same window is still open keeps the current page. The recreation path explicitly clears transient Hotkey/provider/page state and releases Settings-owned fonts and brushes.

Shortcut Manager now uses the same close-to-destroy rule, including Esc with an empty search box. Before destruction it captures the normal window placement and any user-adjusted first-three-column widths, so reopening still preserves Manager geometry/column preferences while resetting search, selection, focus and scroll state exactly as before. Shortcut Editor and Path Conversion were audited and already use stack-scoped modal objects that destroy their HWNDs on close.

Shortcut deletion confirmation no longer exposes `commands.json`. It now asks whether to delete the named shortcut and states that the action cannot be undone. No shortcut persistence format, Provider, Hotkey, Runtime Input, Path Conversion, updater/uninstaller or search/ranking behavior changes. Settings remains schemaVersion 8. Windows fixed FileVersion/ProductVersion is `0.8.0.37`.

## v0.8.0-alpha.3.6 — Shortcut Workflow Naming Polish

Alpha 3.6 is a narrow naming and mode-identity pass before the Shortcut Editor visual consolidation. No Manager layout, Header resize, search, row-density or shortcut execution behavior changes.

The Shortcut Manager context menu is reduced to context-appropriate labels. On a selected item the menu is now **编辑… / Edit...**, **测试 / Test**, **打开所在目录 / Open containing folder**, **复制目标 / Copy target**, and **删除 / Delete**, with the existing separators and Edit kept as the default action. The file-system action remains conditional, and blank-area right-click continues to expose only **新建快捷项… / New shortcut...**.

Shortcut Editor now owns an explicit mode-title update path. Language refreshes, new-item initialization and existing-command loading all synchronize the title from the actual mode, so new flows show **新建快捷项 / New shortcut** and edit flows show **编辑快捷项 / Edit shortcut**. This fixes the previous initialization-order bug where ApplyLanguage ran before LoadCommand and edit windows could retain the New title.

No Shortcut Editor form layout, Runtime Input, Advanced Options, Path Conversion, shortcut persistence, Provider, Hotkey, updater or Shortcut TSV v3 behavior changes. Settings remains schemaVersion 8. Windows fixed FileVersion/ProductVersion is `0.8.0.36`.

## v0.8.0-alpha.3.5 — Shortcut Manager Interaction Polish

Alpha 3.5 removes the last visible interaction artifacts from Shortcut Manager column resizing and fixes first-open placement. All alpha.3.4 column limits and alpha.3.3 search/reopen/density behavior remain unchanged.

The Header no longer uses HDS_FULLDRAG. While the mouse is held, the native Header moves only its tracking guide; Keywords / Name / Type are clamped to their existing logical minimum/maximum ranges without resizing the ListView on every pointer movement. The final dragged width is committed once on HDN_ENDTRACK, together with the elastic Target column. This avoids repeated row/header repaint work while dragging large shortcut collections.

Column commits are ordered so Target shrinks before a growing source column and expands after a shrinking source column. The temporary sum therefore never exceeds the Header client width, eliminating the transient horizontal-scrollbar flash while preserving the exact four-column fill.

On the first creation of Shortcut Manager, the window is centered in the work area of the monitor containing the current mouse pointer. The centering pass is repeated with the settled physical size after a cross-DPI WM_DPICHANGED adjustment. Subsequent hide/reopen behavior remains unchanged: the current-process position and size are preserved while search/selection/scroll interaction state is reset.

No row-density, search-edit, selection, keyboard, shortcut persistence, context action, Shortcut Editor, Path Conversion, Runtime Input, Provider, Hotkey, updater or Shortcut TSV v3 behavior changes. Settings remains schemaVersion 8. Windows fixed FileVersion/ProductVersion is `0.8.0.35`.

## v0.8.0-alpha.3.4 — Shortcut Manager Column Resize Hardening

Alpha 3.4 is a narrow follow-up for the remaining real-Windows ListView header-resize bug. All alpha.3.3 reopen/search/density behavior remains unchanged.

Header resizing is now constrained while the drag is in progress instead of being repaired only after mouse release. Keywords / Name / Type keep user-resizable dividers, but each column has an enforced logical minimum and the dragged column alone is clamped when the remaining Target width would fall below its 120-logical-pixel minimum. Target remains the locked elastic fourth column and is recomputed from the Header's exact client width on every accepted drag step.

The Header path handles ANSI and Unicode HDN notifications, uses HDS_FULLDRAG for live updates, and routes the same constraint logic through both the ListView subclass path and the Manager WM_NOTIFY path. Programmatic normalization is guarded against recursive Header notifications. Because the four real column widths always sum to the Header client width, manual resizing can no longer expose a pseudo-fifth header region or leave selected-row paint outside the real Target column.

No search, reopen-state, row-density, window-resize, shortcut persistence, context action, Shortcut Editor, Path Conversion, Runtime Input, Provider, Hotkey, updater or Shortcut TSV v3 behavior changes. Settings remains schemaVersion 8. Windows fixed FileVersion/ProductVersion is `0.8.0.34`.

## v0.8.0-alpha.3.3 — Shortcut Manager Final Polish

Alpha 3.3 closes the Shortcut Manager phase with the remaining real-Windows reopen, column-resize and search-edit fixes. Shortcut Editor and Path Conversion internals remain unchanged.

The Manager now treats window geometry and interaction state separately. Hiding and reopening the Manager keeps the current-process window position and size, but each reopen clears the search query, clears ListView selection/focus, returns the list to the top and disables Test / Edit / Delete until the user makes a new selection. Internal create/edit refreshes still keep targeting the newly created or edited shortcut through the existing preferred-ID path.

The table remains exactly four columns after manual resizing. Keywords / Name / Type can be resized by the user; Target is the elastic final column and immediately consumes the remaining ListView client width. The Target divider itself is locked, and oversized first-three-column layouts are clamped so a useful Target area remains. A full ListView redraw after header tracking removes stale selection pixels and prevents the previous pseudo-fifth-column residue.

The search field keeps the native single-line EDIT behavior but now derives its compact height from the active body-font metrics and centers that input surface in the top action row. Custom placeholder drawing follows the EDIT formatting rectangle, and every EN_CHANGE erases/repaints the edit before filtering so typed text cannot coexist with stale **搜索快捷项 / Search shortcuts** pixels.

The alpha.3.2 24px row density, restrained selection, DeferWindowPos resize fix, free resize/maximize behavior, 900×560 default and 720×480 minimum remain unchanged. No shortcut persistence, filtering semantics, context actions, Path Conversion, Shortcut Editor, Runtime Input, Provider, Hotkey, updater or Shortcut TSV v3 behavior changes. Settings remains schemaVersion 8. Windows fixed FileVersion/ProductVersion is `0.8.0.33`.

## v0.8.0-alpha.3.2 — Shortcut Manager Real-world Polish

Alpha 3.2 keeps the alpha.3.1 information architecture and fixes the real-Windows density, resize and native-control issues found during desktop validation. Shortcut Editor and Path Conversion internals remain untouched.

The Manager remains freely resizable/maximizable, but its default window is reduced to 900×560 logical pixels while retaining the 720×480 minimum. Layout now moves all seven child controls through one DeferWindowPos transaction with SWP_NOCOPYBITS and performs one parent/child redraw afterward, eliminating stale owner-drawn button pixels after repeated width/height changes.

The table remains exactly four columns. Keywords / Name / Type use 22% / 26% / 12% of the live ListView client width and Target consumes the exact remainder, so resize does not expose an empty pseudo-fifth header area or create a horizontal scrollbar under normal layouts. Body rows are reduced from 30 to 24 logical pixels and the header uses a slightly smaller shared font.

List rows are now fully custom-drawn: ordinary rows are white, the single selected row uses the restrained application selection background, and the saturated native dark-blue selection surface is no longer used. Thin row separators and ellipsized cell text preserve high-density scanning for large shortcut collections.

The top search control uses a thin native border and a reliable custom-drawn **搜索快捷项 / Search shortcuts** placeholder instead of depending on the inconsistent native cue-banner rendering. The New button is demoted from a large solid-accent CTA to the same restrained white secondary surface as the other Manager actions; Delete keeps red semantic text without a permanent red outline.

No shortcut persistence, filtering semantics, context actions, Path Conversion, Shortcut Editor, Runtime Input, Provider, Hotkey, updater or Shortcut TSV v3 behavior changes. Settings remains schemaVersion 8. Windows fixed FileVersion/ProductVersion is `0.8.0.32`.

## v0.8.0-alpha.3.1 — Shortcut Manager Visual Consolidation

Alpha 3.1 starts the Shortcut Manager / Editor consolidation with the Manager only. Existing shortcut persistence, editing, testing, context actions, Path Conversion and v0.7 command semantics are unchanged.

The Manager keeps the native resizable Win32 ListView and its dense four-column information model, but reorganizes the workflow around a top search row with one primary **新建快捷项 / New shortcut** action, the shortcut list in the center, and a bottom action row. The low-value Close button is removed; **路径转换 / Path conversion** is separated as a collection-level tool on the lower left, while **测试 / Test**, **编辑 / Edit** and **删除 / Delete** stay grouped on the lower right as selected-item actions.

The list drops heavy grid lines, uses a compact 30-logical-pixel row rhythm, restrained row separators and the shared application selection palette. The Target column now expands with the window while Keywords / Name / Type keep compact fixed widths. Empty and filtered-empty states use concise native ListView text instead of decorative cards.

Manager controls now use the shared UiTheme / UiMetrics / UiTypography foundation, owner-drawn primary/secondary/danger buttons, per-monitor DPI resource rebuilds and a minimum useful window size. Keyboard workflow adds **Ctrl+F** search, **Ctrl+N** new shortcut, **Ctrl+Enter** test and **Esc** clear-search/close behavior while preserving Enter edit, Delete delete, double-click edit and the existing context menu.

Shortcut Editor and Path Conversion internals are intentionally untouched in this release. No persisted schema, Provider ID/default, Hotkey Registry ID, search/ranking behavior, Everything lifecycle, Runtime Input, Path Conversion, Context Action or Shortcut TSV v3 contract changes. Settings remains schemaVersion 8. Windows fixed FileVersion/ProductVersion is `0.8.0.31`.

## v0.8.0-alpha.2.14 — Hotkey Inline Reset Polish

Alpha 2.14 removes the last unnecessary height expansion caused by modified Hotkey bindings.

Per-item **恢复默认 / Reset** remains a lightweight owner-drawn text action, but now sits on the same main row immediately to the left of the shortcut capture control. The reset column is reserved consistently so capture controls never move when a binding becomes modified. Changing one or all five shortcuts therefore no longer increases Hotkey row/card height or pushes **恢复全部默认快捷键 / Reset all hotkeys** below the fixed Settings viewport.

Only genuine transient/status information—capture guidance and validation/registration errors—uses the auxiliary row below the shortcut control. Those status rows continue to use the alpha.2.13 explicit visibility state and parent-level atomic redraw path.

No scrolling is added to the Hotkeys page in this release because ordinary modified bindings no longer create overflow. No persisted schema, Hotkey Registry ID, binding validation, conflict detection, global registration, Provider behavior, updater contract, search/ranking behavior, Everything lifecycle, Runtime Input, Path Conversion or Shortcut TSV v3 contract changes. Settings remains schemaVersion 8. Windows fixed FileVersion/ProductVersion is `0.8.0.34`.

## v0.8.0-alpha.2.13 — Hotkey Atomic Layout Fix

Alpha 2.13 fixes the real-Windows rendering regression exposed by alpha.2.12 when Hotkey rows expand for capture/reset/error auxiliary content.

The root cause was child-level `WM_SETREDRAW(FALSE)`. Win32 can alter a child window's visible style while redraw is disabled, while Hotkey row height was simultaneously derived from `WS_VISIBLE`. Layout therefore sometimes measured an auxiliary control as hidden even though that same control was restored and painted immediately afterward, producing card-height mismatches, overlapping controls, white strips and stale pixels.

Hotkey rows now own explicit `statusVisible/resetVisible` state instead of inferring layout from child-window styles. Refresh/status changes are performed as one parent-level transaction: suspend only the Settings parent, update text and visibility, recompute all geometry, restore parent redraw, then repaint the parent and all children once. Child controls are never individually sent `WM_SETREDRAW`.

Alpha 2.12's capture lifecycle remains intact: outside clicks, navigation, hide/close and deactivation cancel capture, while same-button click cancels and another capture button switches targets.

No persisted schema, Hotkey Registry ID, binding validation, conflict detection, global registration, Provider behavior, updater contract, search/ranking behavior, Everything lifecycle, Runtime Input, Path Conversion or Shortcut TSV v3 contract changes. Settings remains schemaVersion 8. Windows fixed FileVersion/ProductVersion is `0.8.0.33`.

## v0.8.0-alpha.2.12 — Hotkey Capture Lifecycle & Redraw Fix

Alpha 2.12 closes two real-Windows bugs in the Settings Hotkeys page without changing any hotkey binding semantics.

Hotkey capture is now an explicit transient session. Clicking the same capture control cancels it; clicking another capture control switches the session to that action; clicking elsewhere in Settings, leaving the Hotkeys page, closing/hiding Settings, or deactivating the Settings window cancels capture. Reopening Settings therefore always starts from a non-capture state. Escape and successful capture keep their existing behavior.

Dynamic Hotkey row expansion/collapse now uses a full redraw transaction. After auxiliary state changes, Settings suspends redraw, recomputes layout, then invalidates/erases the parent and all child controls synchronously. This removes stale child-window pixels, white strips and clipped text previously visible after capture/status rows changed height.

No persisted schema, Hotkey Registry ID, binding validation, conflict detection, global registration, Provider behavior, updater contract, search/ranking behavior, Everything lifecycle, Runtime Input, Path Conversion or Shortcut TSV v3 contract changes. Settings remains schemaVersion 8. Windows fixed FileVersion/ProductVersion is `0.8.0.32`.

## v0.8.0-alpha.2.11 — Hotkey Auxiliary State Polish

Alpha 2.11 refines the Hotkey page's temporary/secondary states without changing capture, validation, conflict detection, registration or reset behavior.

Per-item **恢复默认 / Reset** is now an owner-drawn lightweight text action with no bordered button surface and a hand cursor, aligned directly below the shortcut capture column. Capture and validation/registration messages use the same right-side auxiliary column instead of stretching across the left action-label area. Auxiliary content is padded away from row separators, so text no longer sits on or overlaps a border line.

Rows now measure status text at the actual right-side auxiliary width and expand only as much as needed, with bounded padding for wrapped error messages. Auxiliary states have a strict priority: capture/error status hides the per-item Reset link until that status clears, preventing stacked controls in the same row. Normal rows remain 54 logical pixels and the validated capture/switch alignment stays unchanged. The Reset-all button remains lower-right with a slightly tighter gap to the compact launcher card.

No persisted schema, Hotkey Registry ID, capture/conflict/global-registration behavior, Provider behavior, updater contract, search/ranking behavior, Everything lifecycle, Runtime Input, Path Conversion or Shortcut TSV v3 contract changes. Settings remains schemaVersion 8. Windows fixed FileVersion/ProductVersion is `0.8.0.31`.

## v0.8.0-alpha.2.10 — Hotkey Typography & Density Polish

Alpha 2.10 closes the remaining visual mismatch on the Settings Hotkeys page without changing any Hotkey Registry ID or execution behavior.

Hotkey action names now use the same normal body font as other Settings rows while the Global / Launcher group headings keep the section-title font. Normal shortcut rows shrink from 72 to 54 logical pixels and align the action label, capture button and optional switch on one shared row center. The switch column remains reserved for required actions so all five capture controls keep the previously validated vertical baseline.

Per-row Reset and validation/registration status no longer reserve permanent empty space. A row expands by 18 logical pixels only while auxiliary content is actually visible, and the cards recompute their height immediately as capture, validation or reset state changes. Row separators use the same inset/border treatment as the Search Sources page. The Reset-all button remains lower-right and follows the compact launcher card.

No persisted schema, Hotkey Registry ID, global-registration behavior, capture validation, Provider behavior, updater contract, search/ranking behavior, Everything lifecycle, Runtime Input, Path Conversion or Shortcut TSV v3 contract changes. Settings remains schemaVersion 8. Windows fixed FileVersion/ProductVersion is `0.8.0.30`.

## v0.8.0-alpha.2.9 — About Alignment Polish

Alpha 2.9 is a narrow visual-alignment closeout for the About page. It does not change the alpha.2.8 update semantics.

The version text and lightweight `GitHub ↗` entry now share one 24-logical-pixel metadata row and the version STATIC is vertically centered in that row, removing the previous baseline mismatch. The update status is owner-drawn within a 42-logical-pixel area whose center is shared exactly with the 34-logical-pixel update action button; wrapped status/error text remains supported while the normal one-line state aligns visually with the button label.

No update behavior, persisted schema, Provider ID/default, Hotkey Registry ID, search/ranking behavior, Everything lifecycle, Runtime Input, Path Conversion or Shortcut TSV v3 contract changes. Settings remains schemaVersion 8. Windows fixed FileVersion/ProductVersion is `0.8.0.29`.

## v0.8.0-alpha.2.8 — About UX & Update Semantics

Alpha 2.8 is the final About-page interaction pass before the Settings redesign closes.

The About header now keeps GitHub as a lightweight `GitHub ↗` text link beside the version metadata instead of a full secondary button. The product block is slightly tighter vertically, and the two update switches now render their complete labels with a divider between each setting row and the status/action row.

Update preferences are now passive settings. **自动检查更新 / Automatically check for updates** only controls whether startup automation may perform a due check; changing the switch does not immediately start or cancel a check. **接收预发布版本更新 / Get prerelease updates** only selects the channel used by the next automatic or manual check; changing it does not immediately run a new check. If the channel changes while a check is already running, that old check is invalidated asynchronously without blocking either switch, and its stale result is discarded.

The manual action remains **检查更新 / Check for updates** after a successful check rather than changing to “Check again”. Changing the prerelease setting invalidates any completed result and shows that the current settings have not yet been checked. Download/install, rollback, startup-health and immutable-release safety contracts remain unchanged.

No persisted schema, Provider ID/default, Hotkey Registry ID, search/ranking behavior, Everything lifecycle, Runtime Input, Path Conversion or Shortcut TSV v3 contract changes. Settings remains schemaVersion 8. Windows fixed FileVersion/ProductVersion is `0.8.0.28`.

## v0.8.0-alpha.2.7 — About Page & Update UX Polish

Alpha 2.7 finishes the Settings redesign with a compact About page and a user-facing prerelease preference instead of the previous technical update-channel selector.

The About header now groups the product name, version, one-line product description and GitHub entry without a separate low-density Project card. Updates use a 560-logical-pixel card with two native switches: automatic checks and **接收预发布版更新 / Get prerelease updates**. Stable remains the internal default channel for every first run and settings reset, regardless of whether the current build itself is Stable, Alpha, Beta or RC; existing persisted channel choices are preserved.

Update status and action now share one compact row. A single state-driven action changes between Check, Check again, Download and install, progress states and Retry instead of permanently showing two competing buttons. Changing the prerelease preference invalidates the previous update result through the existing update-generation/reset path and starts a fresh check only when automatic checking is enabled.

No persisted schema, Provider ID/default, Hotkey Registry ID, search/ranking behavior, Everything lifecycle, updater safety/rollback/startup-health contract, Runtime Input, Path Conversion or Shortcut TSV v3 contract changes. Settings remains schemaVersion 8. Windows fixed FileVersion/ProductVersion is `0.8.0.27`.

## v0.8.0-alpha.2.6 — Settings Focus & Combo Polish

Alpha 2.6 fixes the last two native ComboBox rough edges found in real-Windows Settings validation.

All Settings combo boxes now release their focus when the user clicks elsewhere inside the Settings window, including blank/card/static areas. The window handles both direct background clicks and child mouse notifications, so the previous blue selection highlight no longer remains stuck until another combo box or an external desktop area is clicked.

Appearance keeps the validated vertical alignment from alpha.2.5 but reduces both Launcher style and interface-language selectors from 200 to 160 logical pixels. The controls remain right-aligned inside the same 560-logical-pixel cards.

No persisted schema, Provider ID/default, Hotkey Registry ID, search/ranking behavior, Everything lifecycle, updater/uninstaller behavior, Runtime Input, Path Conversion or Shortcut TSV v3 contract changes. Settings remains schemaVersion 8. Windows fixed FileVersion/ProductVersion is `0.8.0.26`.
## v0.8.0-alpha.2.5 — Settings Alignment Hotfix

Alpha 2.5 is a narrow real-Windows alignment follow-up to the final Settings polish. It intentionally keeps the validated 820×620 fixed window, 176px left-aligned sidebar, grouped Hotkeys, Provider debounce and Data layout unchanged.

General now reduces the numeric-order selector from 150 to 100 logical pixels and the three Window placement selectors from 220 to 180 logical pixels while keeping them right-aligned. This removes the remaining oversized-combo feel without changing any placement choices or persistence.

Hotkeys now reserves the optional Switch column for every action, including the required primary activation action. As a result all five shortcut capture buttons share one vertical baseline. The page-wide Reset all hotkeys action moves from the lower left to the lower right and aligns with the Hotkey card's inner right edge.

Appearance keeps 200-logical-pixel selectors but moves both native combo boxes down 5 logical pixels so their visible edit portions align vertically with the centered labels inside each card.

No persisted schema, Provider ID/default, Hotkey Registry ID, search/ranking behavior, Everything lifecycle, updater/uninstaller behavior, Runtime Input, Path Conversion or Shortcut TSV v3 contract changes. Settings remains schemaVersion 8. Windows fixed FileVersion/ProductVersion is `0.8.0.25`.
## v0.8.0-alpha.2.4 — Settings Final Polish & Hotkey Redesign

Alpha 2.4 is the final Settings-focused polish pass before Shortcut Manager / Editor work. Sidebar navigation remains deliberately left-aligned while the centered two-line ALTRun / Next brand stays unchanged.

General keeps the validated single-column layout, but Window placement selectors are now fixed at a compact 220 logical pixels instead of stretching with the card. Appearance adopts the same visual rhythm: 560-logical-pixel cards, 200-logical-pixel right-aligned selectors and vertically aligned labels.

Hotkeys is rebuilt as a single-column grouped editor instead of the previous master/detail split. Global actions and launcher-internal actions are shown in separate cards. Each action has an inline binding button, optional enable switch, per-row error/capture status and a small Reset action that appears only when the binding differs from its default. Required primary activation has no meaningless enable switch. Existing Hotkey Registry action IDs, scopes, validation, conflict detection and global registration behavior are unchanged.

Search-source switches now update visually immediately and debounce commits for 180 ms. Rapid clicks collapse to the final requested state instead of queueing synchronous provider work, and all ordinary discovery-source changes from the debounce window are applied through one Settings save, one provider-cache merge and one Launcher refresh. During the actual commit the source switches are temporarily disabled; failed changes are restored to the real persisted state. Everything remains a separate final lifecycle operation so its managed-service/UAC behavior is unchanged.

Data removes the low-value manual legacy-AltRun import button and its permissive legacy import mode. Normal ALTRun Next shortcut TSV import/export remains, while automatic startup migration, legacy IDs and downgrade/schema compatibility are preserved. Import/export buttons are evenly split and the three maintenance actions now share the card width with equal margins/gaps.

No persisted schema, Provider ID/default, Hotkey Registry ID, search/ranking behavior, updater/uninstaller behavior, Runtime Input, Path Conversion or Shortcut TSV v3 contract changes. Settings remains schemaVersion 8. Windows fixed FileVersion/ProductVersion is `0.8.0.24`.
## v0.8.0-alpha.2.3 — Settings Compactness & Input Polish

Alpha 2.3 is a focused real-Windows follow-up to the alpha.2.2 Settings validation. The fixed Settings viewport keeps its 820 logical-pixel width but drops from 720 to 620 logical pixels so Hotkeys, Search sources, Appearance, Data and About no longer sit inside an unnecessarily tall window. General remains intentionally single-column and scrollable.

Sidebar navigation no longer uses the legacy dotted `DrawFocusRect` treatment. The current page still uses the existing selection background, bold label and left accent bar, while keyboard focus on a non-selected navigation item is represented only by a subtle background state.

Owner-drawn Settings toggles now treat both `BN_CLICKED` and `BN_DOUBLECLICKED` as physical toggle activations. Win32 reports the second press of a rapid double-click on an owner-drawn button as `BN_DOUBLECLICKED`, which previously made quick repeated clicks appear to be ignored. The alpha.2.2 supersampled switch rendering and explicit immediate repaint remain unchanged.

No persisted schema, Provider ID/default, Hotkey Registry ID, search/ranking behavior, Everything ownership/lifecycle, updater/uninstaller behavior, Runtime Input, Path Conversion or Shortcut TSV contract changes in this polish pass. Settings remains schemaVersion 8. Windows fixed FileVersion/ProductVersion is `0.8.0.23`.
## v0.8.0-alpha.2.2 — Settings Interaction & Layout Polish

Alpha 2.2 locks the Settings product structure around a fixed, compact native window instead of a freely resizable dashboard. The default client viewport is now 820×720 logical pixels, the resize/maximize affordances are removed, and the sidebar shrinks from 208 to 176 logical pixels. The two-line `ALTRun` / `Next` brand is centered over the same navigation width used below it.

General is now permanently single-column. Launcher behavior, search behavior and window placement share one left/right edge with a 560-logical-pixel maximum card width, so labels and right-side controls no longer drift far apart on a wide window. The three placement combo boxes again populate their actual choices: current mouse monitor / active-window monitor / primary monitor, Launcher near-top / centered / last position, and Settings centered / last position.

Toggle switches keep the compact right-side interaction but are rendered at 3× and downsampled with HALFTONE for cleaner edges. The old dotted focus rectangle is replaced by a narrow accent focus bar, and successful toggle clicks explicitly repaint the clicked control for immediate feedback.

The development-only Diagnostics Settings page is removed rather than hidden. Its page enum, navigation, controls, timers, paint/layout/message routing, runtime snapshot API, ProcessMemory platform layer, process-memory test target and psapi production dependency are deleted. No separate Diagnostics executable is introduced in this phase; future support tooling can be designed independently if it becomes a real product requirement.

No persisted schema, Provider ID/default, Hotkey Registry ID, search/ranking behavior, Everything ownership/lifecycle, updater/uninstaller behavior, Runtime Input, Path Conversion or Shortcut TSV contract changes in this polish pass. Settings remains schemaVersion 8. Windows fixed FileVersion/ProductVersion is `0.8.0.22`.
## v0.8.0-alpha.2.1 — Settings Rendering & Information Hierarchy Stabilization

Alpha 2.1 is a focused real-Windows stabilization pass for the Settings redesign. It fixes page-switch repaint corruption by giving native static controls explicit sidebar/card/window backgrounds and repainting a completed page swap in one pass instead of leaving transparent child regions over stale pixels.

The Settings information hierarchy is reduced rather than padded with explanation text. The sidebar brand is now a two-line `ALTRun` / `Next` mark with no separate “Settings” subtitle or white brand tile. General uses compact one-line toggle rows and label-plus-combo placement rows; Hotkeys uses the same toggle language for per-action enablement; Search sources keeps concise Everything state and actions; Appearance, Data and About drop migration/healthy-state filler text. Diagnostics remains intentionally information-dense.

General row metrics are compacted to 50 logical pixels and placement rows to 54 logical pixels. The initial Settings window is sized from a 1080×720 logical client viewport through `AdjustWindowRectExForDpi`, eliminating the alpha.2 outer-window/client-area mismatch while retaining responsive stacking and scrolling on genuinely narrow windows.

No persisted schema, Provider ID/default, Hotkey Registry ID, search/ranking behavior, Everything ownership/lifecycle, updater/uninstaller behavior, Runtime Input, Path Conversion or Shortcut TSV contract changes in this hotfix. Settings remains schemaVersion 8. Windows fixed FileVersion/ProductVersion is `0.8.0.21`.
## v0.8.0-alpha.2 — Settings UX Redesign

Alpha 2 is the first visible v0.8 product-polish release. Settings now uses a compact native card/row system with a 208 logical-pixel sidebar, fixed page header, right-side toggle switches, unified secondary/primary/danger buttons, and the navigation order `General -> Hotkeys -> Search sources -> Appearance -> Data -> Diagnostics -> About`.

General now owns launcher behavior, search behavior and window placement. Search-result icons move from Appearance into Launcher behavior. Monitor selection remains independent from placement: Launcher can open near the top, centered, or at its last manually moved position; Settings can open centered or at its last position. Last positions are clamped to a currently available monitor so display/DPI changes cannot strand a window off-screen.

Hotkeys keeps its two-column workflow but gains a two-line owner-drawn action list. Search sources separates Windows application providers from Everything file/folder search and presents concise Everything state. Appearance now contains only launcher style and language. Data owns the data-directory entry plus import/export and maintenance actions. Diagnostics is reorganized into scan-friendly cards. About now focuses on product identity, updates and the project link.

The obsolete hidden General-page hotkey controls are removed completely. Settings schemaVersion advances from 7 to 8 only for the new window-placement preferences/state; all existing v0.7 Provider IDs/defaults, Hotkey Registry IDs, search/ranking behavior, Managed Everything lifecycle, updater/uninstaller contracts and Shortcut TSV v3 remain unchanged. Windows fixed FileVersion/ProductVersion is `0.8.0.2`.

## v0.8.0-alpha.1 — Unified UI Foundation & Legacy Cleanup

Alpha 1 starts the v0.8 product-polish line without redesigning the visible interface yet. It introduces shared `UiTheme`, `UiMetrics` and `UiTypography` foundations so Launcher, Settings, Shortcut Manager, Shortcut Editor and Path Conversion use one source for semantic palettes, DPI scaling, geometry tokens and font specifications while preserving their existing appearance.

Classic Launcher remains exactly `420 / 16 / 10` (logical width / row height / maximum results), and Modern Compact remains `620 / 32 / 9`. A new portable UI-foundation regression test locks these values and the Settings `190 / 38 / 34 / 54` layout metrics before later v0.8 visual work begins.

The unreachable legacy Shortcut/Command editor embedded in Settings has been removed completely, including its old page enum, controls, layout, painting, message dispatch and App refresh hook. Shortcut functionality remains owned by the standalone Shortcut Manager and task-oriented Shortcut Editor; Command storage, import/export, Runtime Input, Path Conversion and Provider-to-shortcut behavior are unchanged.

No persisted schema, Provider ID/default, Hotkey Registry ID, search/ranking behavior, Everything lifecycle, updater/uninstaller behavior or v0.7 compatibility contract changes in this alpha. Windows fixed FileVersion/ProductVersion is `0.8.0.1`.

## v0.7.0 — Stable

v0.7.0 promotes the fully validated RC1 runtime to Stable without adding another runtime feature. The final real-Windows gate `beta.12 -> rc.1` native automatic update passed, and the RC release contract already froze schemas, Provider IDs, Hotkey Registry action IDs, Provider dedup, launcher selection behavior, Runtime Input/Path Conversion, Native Uninstall and Managed Everything ownership/lifecycle.

Stable keeps settings schemaVersion 7, commands schemaVersion 2, usage schemaVersion 1, provider-cache schemaVersion 2 and Shortcut TSV v3. The portable executable contract remains `ALTRunNext.exe`, `Update.exe` and `Uninstall.exe`.

The only release-channel change is intentional: a fresh or migrated Stable build defaults to the Stable update channel, while prerelease builds continue to default to Development. Existing user-selected update preferences remain persisted normally.

No v0.7 runtime behavior is changed during Stable promotion. Windows fixed FileVersion/ProductVersion is `0.7.0.300`.

## v0.7.0-rc.1 — Release Freeze & Upgrade Gate

RC1 freezes the validated Beta 12 runtime rather than adding another feature. The final Beta 12 Provider-to-shortcut workflow passed real-Windows validation, including the TeamSpeak `ts3, teamspeak3` promotion case and the regression that prevents a lower App Paths/PATH Provider duplicate from reviving after a user shortcut is created.

The RC contract keeps settings schemaVersion 7, commands schemaVersion 2, usage schemaVersion 1, provider-cache schemaVersion 2 and Shortcut TSV v3 unchanged. Provider IDs, Hotkey Registry action IDs, launcher geometry, update behavior, Native Uninstall ownership/DELETE-lease behavior and Managed Everything lifecycle are frozen. The portable executable contract remains `ALTRunNext.exe`, `Update.exe` and `Uninstall.exe`.

`V0.7_RC_VALIDATION.md` is now the final manual release gate. It covers clean install, native `beta.12 -> rc.1` update, Provider-to-shortcut regression, Shortcut/Runtime Input/Path Conversion, launcher selection/performance, Managed Everything ownership/lifecycle, Native Uninstall, DPI/Windows 10 compatibility, x64/ARM64 packaging and release identity. Release ordering is frozen as `beta.12 < rc.1 < stable`.

No new user-facing behavior is introduced in RC1. Windows fixed FileVersion/ProductVersion is `0.7.0.200`.

## v0.7.0-beta.12 — Provider-to-Shortcut Workflow & Stable Provider Dedup

Beta 12 completes the practical Provider-to-user-shortcut workflow used for personal muscle-memory aliases such as `ts3, teamspeak3`. The existing Launcher context action is now named `Add to shortcuts...` / `添加到快捷项...`, and the surrounding menu is reorganized around execution, file operations, and shortcut management. The file operation label is `Open containing folder` / `打开所在目录`; direct folder results intentionally omit that redundant action.

Eligible executable/application results also expose `Run as administrator`. This is an execution override only: it does not mutate the saved shortcut's `runAsAdmin` setting. User shortcuts retain `Edit shortcut...` and `Delete shortcut`, while Provider/file results use `Add to shortcuts...`. The existing shortcut seed already preserves the discovered name and exact target while leaving the keyword field empty and focused in the normal `New shortcut` editor.

The command merge pipeline is now explicitly two-stage. Provider-vs-Provider duplicates are canonicalized first according to Provider priority, then user shortcuts are overlaid. This prevents a lower App Paths/PATH executable from reappearing after the winning Start Menu shortcut has been promoted to a user shortcut. In the TeamSpeak example, adding a `ts3` user shortcut for the Start Menu `.lnk` no longer causes the previously suppressed TeamSpeak 3 `.exe` Provider result to revive.

No persisted schema, Provider ID/default, Provider cache format, search scoring weights, update/uninstall contract or Everything lifecycle changes are introduced. Update ordering now includes `beta.11 < beta.12 < rc.1`. Windows fixed FileVersion/ProductVersion is `0.7.0.111`.

## v0.7.0-beta.11 — Query-Edit Selection Reset

Beta 11 completes the launcher selection-state fix started in Beta 10. Selection preservation is useful only while the query itself is unchanged (for example when asynchronous Everything results arrive). Previously every `EN_CHANGE` rebuilt results while remembering the old result id/provider, so a command that ranked first for an early prefix could remain selected after a longer query moved it to row 2, row 9, or another lower position.

The edit-control `EN_CHANGE` handler now clears list selection before rebuilding results. Every user query edit therefore selects the new query's current best result at row 1, including typing, backspace and clearing the field. Asynchronous refreshes that do not change the edit text continue to preserve the user's current selection. Beta 10's fresh Hide/Show reset remains in place for "Clear query on show".

No search scoring, usage ranking, provider weights, schemas, update/uninstall behavior or Everything lifecycle are changed. Update ordering now includes `beta.10 < beta.11 < rc.1`. Windows fixed FileVersion/ProductVersion is `0.7.0.110`.

## v0.7.0-beta.10 — Fresh Launcher Selection Reset

Beta 10 fixes a small launcher-state regression found during final RC review. With "Clear query on show" enabled, the launcher correctly cleared the previous query when invoked again, but result rebuilding still remembered the previously selected command by ID/provider. If that command ranked lower in the empty-query default list after usage was recorded, the newly opened launcher could start with a middle row selected (for example row 5) instead of row 1.

A fresh Show now clears the old list selection before clearing the edit control. This ordering matters because `SetWindowTextW` emits `EN_CHANGE` synchronously: clearing selection first ensures the empty-query rebuild starts from row 1. Selection preservation remains unchanged for refreshes within the same visible session, including asynchronous Everything result merges. When "Clear query on show" is disabled, the existing reopen behavior is preserved.

No ranking weights, usage schema, provider behavior, shortcut schema, update/uninstall behavior or Everything lifecycle are changed. Update ordering now includes `beta.9 < beta.10 < rc.1`. Windows fixed FileVersion/ProductVersion is `0.7.0.109`.

## v0.7.0-beta.9 — Stale App Paths Filtering

Beta 9 is a focused provider-data fix discovered after the full Beta validation pass. Windows can retain `App Paths` registry entries after an application has been moved or uninstalled; previous ALTRun Next builds accepted the registry string as authoritative, so a stale entry such as `chrome.exe -> D:\\Chrome\\App\\chrome.exe` could outrank a valid Start Menu shortcut and fail only when executed.

The App Paths provider now requires the resolved target to be a live regular file before indexing it. Provider cache loading applies the same rule on Windows, so stale App Paths commands written by older builds disappear from the very first search after upgrade instead of remaining visible until the asynchronous provider refresh completes. Existing Start Menu, PATH and packaged-app behavior is unchanged.

Windows provider smoke coverage now verifies both live App Paths discovery and old-cache stale-entry suppression. No persisted schema, Provider ID/default, ranking weight, update/uninstall contract, Everything lifecycle or shortcut format changes are introduced. Update ordering now includes `beta.8 < beta.9 < rc.1`. Windows fixed FileVersion/ProductVersion is `0.7.0.108`.

## v0.7.0-beta.8 — Native Uninstall Shell-Parking Fix

Beta 8 corrects the Beta 7 delete-lease handoff after real Windows validation showed frequent error 1460 timeouts. The issue was self-inflicted: Beta 7 navigated Explorer only to the installation folder's immediate parent and then asked the normal-integrity broker to acquire DELETE access to the root. Explorer could immediately enumerate or select the just-left ALTRun folder in that parent view, keeping a non-delete-sharing Shell handle alive and causing the broker lease to time out.

Full removal now parks any Explorer view one level farther away from the installation root (falling back to the Windows temporary directory for root-level layouts). The broker no longer acquires a DELETE lease at all. It signals Shell release immediately and stays alive only until the elevated worker has independently acquired the single root DELETE lease. The elevated worker acquires that lease before deleting any installation-tree entries, then keeps it through precise child cleanup and final handle-based root deletion.

This also removes the Beta 7 double-timeout failure pattern: a broker-side lease failure can no longer leave the worker waiting for `ReleaseDone` until error 1460. If the elevated worker still cannot acquire the root lease, it reports the real root path/error and Restart Manager lock owners when available, before destructive cleanup begins.

No reboot-delete fallback is used. UAC cancellation, preserve-data behavior, foreground completion dialogs, Stable update behavior and Managed Everything ownership remain unchanged. Update ordering now includes `beta.7 < beta.8 < rc.1`. Windows fixed FileVersion/ProductVersion is `0.7.0.107`.

## v0.7.0-beta.7 — Native Uninstall Delete-Lease Handoff

Beta 7 closes the remaining intermittent full-remove race observed after the Beta 6 Explorer broker change. A successful Explorer navigation alone is not a durable guarantee that the installation root will still be delete-share-compatible several milliseconds later. The broker and elevated worker now perform a continuous DELETE-access lease handoff on the installation root.

After Explorer is released, the normal-integrity broker opens the installation directory with DELETE access and `FILE_SHARE_DELETE`, then keeps that handle alive while signaling the elevated worker. The elevated worker acquires its own matching root handle before acknowledging the handoff; only then does the broker close its lease and exit. This removes the window in which Explorer or another Shell component could reopen the directory with incompatible sharing.

The elevated worker keeps its root lease for the entire destructive phase. It deletes child entries individually using the existing precise cleanup path, then marks the now-empty root for deletion through `SetFileInformationByHandle(FileDispositionInfo)` on the already-held handle instead of reopening the root by path. If a root lease cannot be obtained, uninstall fails before destructive tree cleanup rather than leaving an empty outer folder.

No reboot-delete fallback is used. Precise file diagnostics, Restart Manager lock-owner reporting, UAC cancellation behavior, preserve-data semantics, Stable update behavior and Managed Everything ownership remain unchanged. Update ordering now includes `beta.6 < beta.7 < rc.1`. Windows fixed FileVersion/ProductVersion is `0.7.0.106`.

## v0.7.0-beta.6 — Native Uninstall Broker UX

Beta 6 keeps the synchronous, precise cleanup from Beta 5 while making the Explorer-release step behave like a mature uninstaller. The original normal-integrity `Uninstall.exe` now remains alive after launching the elevated TEMP worker and acts as a short-lived Explorer broker. Two named local events coordinate the handoff.

For full removal, UAC now appears before any Explorer navigation. The elevated worker first closes ALTRun Next and cleans owned Managed Everything state, then requests the broker to release any Explorer view inside the installation tree only when final deletion is actually about to start. The broker navigates that view to the parent, acknowledges the request, exits, and the elevated worker waits for that exact parent PID before deleting the installation tree. Cancelling UAC therefore leaves Explorer untouched.

No reboot-delete fallback returns, and the precise entry-by-entry failure path/Restart Manager diagnostics from Beta 5 remain intact. Preserve-data uninstall keeps the simpler parent-exit flow. No persisted schema, Provider ID, Hotkey action, update contract, Stable-channel behavior or Managed Everything lifecycle changes are introduced. Update ordering now includes `beta.5 < beta.6 < rc.1`. Windows fixed FileVersion/ProductVersion is `0.7.0.105`.

## v0.7.0-beta.5 — Precise Native Uninstall Cleanup

Beta 5 replaces the Beta 4 reboot-deletion fallback with deterministic, diagnosable cleanup. Full-remove now enumerates the portable installation tree and deletes files and directories individually, deepest-first, so a persistent failure identifies the exact path rather than only the installation root.

Because users commonly launch `Uninstall.exe` from an Explorer window that is currently displaying the portable directory, the non-elevated launcher now detects Explorer views inside the installation tree and navigates them to the parent directory before UAC and final root removal. A second best-effort release pass runs in the elevated worker. Regular-file failures query Windows Restart Manager and include possible locking application/PID information in the failure dialog when available.

The previous behavior that queued the whole remaining installation tree for deletion at the next reboot has been removed. A normal full uninstall is expected to complete synchronously; true third-party locks are reported precisely instead of being hidden behind a reboot workaround.

No persisted schema, Provider ID, Hotkey action, update package contract, Stable-channel behavior or Managed Everything lifecycle changes are introduced. Update ordering now includes `beta.4 < beta.5 < rc.1`. Windows fixed FileVersion/ProductVersion is `0.7.0.104`.

## v0.7.0-beta.4 — Stable Update & Native Uninstall Completion Fixes

Beta 4 is the second focused fix from real Windows Beta validation. Stable-channel update checks no longer report HTTP 404 as a system error when the current stable release predates the native updater. If `releases/latest` has no `update-manifest.json`, ALTRun Next now reads the latest stable release metadata, reports that older stable version without offering a downgrade, and keeps future stable releases with manifests on the normal signed/checksummed update path.

Native Uninstall now handles the remaining full-remove edge case seen on Windows: after bounded retry, an owned file or directory that is still held by Windows can be queued with `MOVEFILE_DELAY_UNTIL_REBOOT` instead of making the entire uninstall fail. This fallback stays inside the already validated ALTRun installation/Managed Everything ownership boundary. Completion dialogs now use foreground/topmost message-box flags so success and failure results are not hidden behind unrelated windows.

No persisted schema, Provider ID, Hotkey action, updater package contract or Managed Everything lifecycle is changed. Update ordering now includes `beta.3 < beta.4 < rc.1`. Windows fixed FileVersion/ProductVersion is `0.7.0.103`.

## v0.7.0-beta.3 — Native Uninstall Full-Remove Hardening

Beta 3 is a focused fix from real Windows Native Uninstall validation. The preserve-data path passed, but the full-remove path could fail immediately after stopping Managed Everything when Windows, antivirus or another short-lived shell/indexing handle still held a file. The elevated TEMP uninstaller now retries only transient delete failures (sharing violation, lock violation, access denied, directory-not-empty and busy) for a bounded period before declaring the uninstall failed.

The failure path is now diagnostic instead of misleading: filesystem deletion errors are propagated explicitly rather than displaying a stale Win32 `GetLastError()` value, and an unrecoverable failure dialog includes the path/phase that could not be removed. Preserve-data semantics, exact-install process ownership, Managed Everything service ownership and External Everything protection are unchanged.

The v0.7 Beta freeze remains intact: no schema, Provider ID, Hotkey action, updater contract or Everything lifecycle changes are introduced. Update ordering now includes `beta.2 < beta.3 < rc.1`. Windows fixed FileVersion/ProductVersion is `0.7.0.102`.

## v0.7.0-beta.2 — Shortcut Editor Dynamic Layout Fixes

Beta 2 is the first focused fix from real Windows Beta 1 validation. It does not add a feature or change any persisted schema. The Shortcut Editor working-directory browse button now has an explicit localized label and the same usable width as the other compact browse controls.

Runtime Input and Advanced are both dynamic Shortcut Editor sections. Switching Runtime Input between No extra input / Pass through / URL encode moves the Test input row and every control below it; when Advanced was already expanded this could leave stale child-control borders/backgrounds, producing a line through the Test button or severe overlapping/fragmented paint trails. Beta 2 makes these transitions one explicit dynamic-relayout path: resize without intermediate parent redraw, lay out the final child rectangles, then perform a final full erase-and-redraw pass over the parent and all children. Advanced expand/collapse uses the same path.

The v0.7 Beta freeze remains intact: settings schemaVersion 7, commands schemaVersion 2, usage schemaVersion 1, provider-cache schemaVersion 2 and Shortcut TSV v3 are unchanged. Provider IDs, Hotkey action IDs, Update/Uninstall contracts and Managed Everything lifecycle are unchanged. Windows fixed FileVersion/ProductVersion is `0.7.0.101`.

## v0.7.0-beta.1 — Feature Freeze & Workflow Hardening

Beta 1 freezes the v0.7 shortcut and launcher workflow surface after the alpha.9.4 Managed Everything lifecycle passed real Windows validation. No new user-facing feature or persisted data model is introduced. The compatibility baseline is now settings schemaVersion 7, commands schemaVersion 2, usage schemaVersion 1, provider-cache schemaVersion 2 and Shortcut TSV v3.

The frozen public identifiers include the existing Windows/Everything Provider IDs and Hotkey Registry action IDs. The portable executable contract is `ALTRunNext.exe`, `Update.exe` and `Uninstall.exe`; the obsolete `ALTRunNext.Updater.exe` name remains prohibited. Managed Everything remains portable under `data/tools/Everything`; normal application exit keeps an enabled owned service warm, while explicitly disabling the Everything provider stops/disables only the owned service. External Everything ownership remains protected.

Beta 1 adds a dedicated shortcut compatibility matrix covering commands schema-1 migration plus TSV v1/v2/v3 imports, legacy five-column import and full v3 export/import round-trip of aliases, command type, portable paths, runtime input, custom icon and elevation state. Update policy tests now freeze the real prerelease progression `alpha.9.4 < beta.1 < beta.2 < beta.3 < beta.4 < beta.5 < beta.6 < beta.7 < beta.8 < beta.9 < beta.10 < beta.11 < beta.12 < rc.1 < stable` and explicitly reject downgrade to alpha or v0.6 stable.

The package now includes `V0.7_BETA_VALIDATION.md`, the manual real-Windows sign-off matrix for native alpha.9.4 -> beta.1 update, Shortcut Manager/Editor, Runtime Input, Path Conversion, asynchronous icons, Context Actions, Managed/External Everything ownership, native uninstall, DPI and performance. Windows fixed FileVersion/ProductVersion is `0.7.0.100`.

## v0.7.0-alpha.9.4 — Managed Everything Provider Lifecycle

Alpha 9.4 makes the Everything provider checkbox control the full runtime state of an ALTRun Next-owned Everything installation. Normal application exit keeps the existing alpha.9.2 design: ALTRun Next closes only its managed Everything client and leaves the owned `SERVICE_AUTO_START` service warm for the next launcher session. Explicitly disabling **Everything files & folders** is different: the managed client is closed, the owned Everything service is stopped, and its startup type is changed to `SERVICE_DISABLED`, so it does not come back at the next Windows boot while the provider remains disabled.

Re-enabling the provider detects an ALTRun-owned installed service, restores `SERVICE_AUTO_START`, starts the service, and then starts the managed client. Service policy changes are explicit privileged operations, so Windows may request UAC when toggling an owned managed installation. Cancelling or failing the privileged transition leaves the persisted provider setting unchanged and restores the previous client state when necessary.

Ownership remains the hard safety boundary. The service ImagePath must resolve to the exact current `data/tools/Everything/.../Everything.exe` managed path or the known historical alpha.9.1 detached host before ALTRun Next may stop or reconfigure it. External/user-installed Everything services are never stopped, disabled, started, or reconfigured; disabling the provider only makes ALTRun Next stop using them. No persisted schema changes are made. Windows fixed FileVersion/ProductVersion is `0.7.0.94`.

## v0.7.0-alpha.9.3 — Clean Generic Update/Uninstall Helpers

Alpha 9.3 removes the temporary legacy updater-name bridge from the portable package. Development testing is intentionally manual across the alpha.9/alpha.9.1 -> alpha.9.2.x boundary, so there is no reason to carry `ALTRunNext.Updater.exe` forward. The canonical package contract is now simply `ALTRunNext.exe`, `Update.exe` and `Uninstall.exe`; future releases inherit those generic helper names.

The alpha.9.2 portable Managed Everything and native-uninstaller architecture is otherwise unchanged: Managed Everything remains entirely under `data/tools/Everything`, the owned Windows Service remains `SERVICE_AUTO_START` and stays warm across normal ALTRun Next exit, the client exits with ALTRun Next, and `Uninstall.exe` performs the destructive service/application cleanup from a temporary elevated copy. No persisted schema changes are made. Windows fixed FileVersion/ProductVersion is `0.7.0.93`.

## v0.7.0-alpha.9.2 — Portable Managed Everything & Native Uninstaller

Alpha 9.2 restores the portable ownership boundary after real-world lifecycle review. Managed Everything is again fully contained under ALTRun Next's own `data/tools/Everything` tree. The Everything Windows Service remains `SERVICE_AUTO_START` and continues running when ALTRun Next exits, so Windows can keep the index warm and the next launcher start does not require another administrator confirmation. The session client still exits with ALTRun Next; normal exit is deliberately different from uninstall.

The temporary Program Files service-host direction from alpha.9.1 is superseded. Alpha 9.2 detects that exact ALTRun-owned alpha.9.1 service path, asks for UAC only through the existing explicit repair flow, stops the service, retargets it back to the current portable managed `Everything.exe -svc`, restarts it, and removes the old `%ProgramFiles%\Aspeternity\ALTRunNext\EverythingService` residue. Healthy external/user-installed Everything services remain untouched.

The portable package now uses the generic helper names `Update.exe` and `Uninstall.exe`. `Update.exe` keeps the existing native transactional update/rollback design. `Uninstall.exe` copies itself to `%TEMP%`, asks once whether user data should also be removed, elevates the temporary worker, closes the exact ALTRun Next installation being removed, stops/deletes only an Everything service whose executable belongs to this portable managed tree (or the known alpha.9.1 detached host), removes managed Everything and update runtime data, removes the matching current-user startup entry, then deletes application files. If user data is preserved, only the `data` user-state tree remains.

For one transition release, the ZIP also contains an `ALTRunNext.Updater.exe` compatibility copy of `Update.exe`: alpha.9/alpha.9.1 hard-code that filename during staging, so removing it immediately would break native self-update into alpha.9.2. On first alpha.9.2 startup the installed compatibility name is deleted automatically; future packages can remove the bridge entirely. No persisted schemas change: settings stays 7, commands 2, usage 1, provider-cache 2 and Shortcut TSV v3. Windows fixed FileVersion/ProductVersion is `0.7.0.92`.

## v0.7.0-alpha.9.1 — Managed Service Detachment & Updater Validation Target

Alpha 9.1 fixes a portable-lifecycle conflict discovered during real Windows validation. Alpha 8.3 correctly stopped the ALTRun Next-managed Everything client on application exit, but deliberately kept the Windows Everything Service warm. Because the legacy service `ImagePath` also pointed into `data/tools/Everything` inside that portable ALTRun Next folder, the service process could keep an old release directory locked even after ALTRun Next and its client had exited.

ALTRun Next now separates the two lifetimes. The standard-user managed Everything client remains under portable `data/tools/Everything`, while an ALTRun-owned Windows service is hosted from the system-protected `%ProgramFiles%\Aspeternity\ALTRunNext\EverythingService\<Everything-version-arch>\Everything.exe`. An existing legacy ALTRun-managed service is detected even while running and is migrated with one explicit UAC confirmation from **Get and start Everything**. The maintenance helper stops the old service, copies the already managed Everything binary into the protected host directory, retargets the service, restarts it and waits for readiness. After migration the service may remain warm without pinning any portable ALTRun Next folder, so old release directories can be moved or deleted normally.

The ownership boundary remains strict: a healthy external/user-installed Everything service is never retargeted. A stale service path can still be repaired only through the explicit elevated flow, while local Recheck stays non-elevating. No persisted schema changes are made: settings remains schemaVersion 7, commands 2, usage 1, provider-cache 2 and Shortcut TSV v3. This build also intentionally serves as the first real updater target for validating the alpha.9 native updater end to end. Windows fixed FileVersion/ProductVersion is `0.7.0.91`.

## v0.7.0-alpha.9 — Native Update & Safe Apply

Alpha 9 adds ALTRun Next's native self-update path so development and later stable builds no longer require manually visiting GitHub, downloading a ZIP and replacing files. **Settings → About** now exposes Stable / Development update channels, a default-on low-noise automatic check throttled to at most once per 24 hours, manual **Check for updates**, and explicit **Download and install**. Prerelease builds default to Development; stable builds default to Stable.

Release CI now publishes a machine-readable `update-manifest.json` beside the x64/ARM64 ZIPs and `SHA256SUMS.txt`. The client fetches that manifest over HTTPS using WinHTTP, compares versions, selects the running architecture, downloads to a temporary non-ZIP filename, verifies SHA-256 with Windows BCrypt, extracts to `data/update/staging/<version>`, and validates the staged VERSION plus both update executables before any live file is touched.

Installation is handed to the small packaged `ALTRunNext.Updater.exe`, copied to `%TEMP%` before launch. It waits for the current ALTRun Next process to exit, backs up only application files that will be replaced, never copies a staged `data/` tree over the user's data, applies the staged package, then restarts ALTRun Next. The restarted process signals a one-shot health event only after normal startup initialization; launch/health failure triggers rollback and relaunch of the previous version. A protected installation directory elevates only the updater; the main program is relaunched with the normal Explorer user token when possible.

The updater is user-driven: update checks may run silently in the background, but packages are not downloaded or installed until **Download and install** is clicked. SHA-256 protects transfer/package integrity; cryptographic release signing remains a future distribution hardening step. settings.json advances to schemaVersion 7 for `update.autoCheck` and `update.channel`; commands.json remains schemaVersion 2, usage.json schemaVersion 1, provider-cache.json schemaVersion 2 and Shortcut TSV remains v3. Windows fixed FileVersion/ProductVersion is `0.7.0.90`.

## v0.7.0-alpha.8.4 — Everything Service Path Repair

Alpha 8.4 hardens Managed Everything for ALTRun Next's portable/movable deployment model. A Windows service persists independently from the portable application folder, so an existing stopped Everything Service can still point to an old managed `Everything.exe` after ALTRun Next is moved, re-extracted or an old test directory is removed. Starting that stale service returns Win32 `ERROR_FILE_NOT_FOUND (2)`.

ALTRun Next now reads the installed Everything Service `ImagePath` with `QueryServiceConfigW` before starting a stopped service. A healthy running service is reused without modification, and a stopped service whose executable still exists is started normally. A stopped service whose executable no longer exists is classified as stale instead of blindly calling `-start-service`.

Local **Recheck** remains non-elevating and reports that the stale service path needs repair. The explicit **Get and start Everything** flow launches a one-shot elevated ALTRun Next maintenance mode, updates only the existing Everything service binary path to the current managed `Everything.exe -svc`, restores automatic start, starts the service, then returns to the normal bootstrap flow. This repair uses one UAC confirmation and does not delete/recreate the service. External healthy Everything services are still left untouched.

Core regression coverage validates quoted, unquoted, spaced and case-varied service command lines. No persisted schema changes are made. Windows fixed FileVersion/ProductVersion is `0.7.0.84`.

## v0.7.0-alpha.8.3 — Managed Everything Lifecycle

Alpha 8.3 closes the managed-runtime lifecycle. When ALTRun Next actually exits, or when **Everything files & folders** is disabled in Search Sources, ALTRun Next now stops only the portable Everything client under its own `data/tools/Everything` directory. The Windows **Everything** service is intentionally left running so NTFS indexing remains warm and the next ALTRun Next launch can start the client without another UAC prompt.

Shutdown is ownership-safe. Before issuing Everything's `-exit` command, ALTRun Next verifies that the active default Everything IPC window is owned by the exact managed executable path. A separately installed/user-run Everything instance is therefore never closed, even when a managed executable also exists on disk. Bootstrap generations are invalidated and joined before provider disable so a late completion message cannot resurrect the provider or managed client after shutdown.

A Windows runtime regression test freezes both no-install/no-running behavior and the external-ownership safety boundary. No service stop/delete operation is part of the lifecycle path. No persisted schema changes are made. Windows fixed FileVersion/ProductVersion is `0.7.0.83`.

## v0.7.0-alpha.8.2 — Managed Index Service & Headless Runtime

Alpha 8.2 fixes the second real-Windows bootstrap gap found during desktop validation. A freshly extracted portable Everything can expose IPC before it has permission to index NTFS volumes; Everything then shows its access-denied setup dialog and the launcher sees an apparently healthy IPC endpoint backed by an empty database.

ALTRun Next now treats its own managed Everything copy as a complete runtime dependency rather than merely an executable. The managed copy is configured with `app_data=0`, `run_as_admin=0`, `run_in_background=1`, `show_tray_icon=0`, update checks disabled and IPC enabled. Existing unrelated Everything installations are not rewritten.

For the managed copy, ALTRun Next checks the Windows **Everything** service before declaring the dependency ready. If the service is already running, no elevation is requested. If it is missing/stopped, the explicit **Get and start Everything** flow asks Windows for elevation once and invokes the official executable's `-install-service` / `-start-service` command, waits for the service, launches the client with `-startup -first-instance`, then waits for IPC. Local-only Recheck never triggers UAC. A previously running managed alpha.8.x client is stopped and restarted only when managed configuration/service repair is actually required.

This also makes the managed dependency headless by default: no search window and no Everything tray icon. No persisted ALTRun Next schema changes are made. Windows fixed FileVersion/ProductVersion is `0.7.0.82`.

## v0.7.0-alpha.8.1 — Verified ZIP Staging Fix

Alpha 8.1 fixes the real-Windows extraction failure discovered during Managed Everything Bootstrap validation. Alpha 8 correctly downloaded and SHA-256 verified the official archive, but passed the temporary `.zip.download` path directly to the Windows Shell ZIP namespace. Shell ZIP discovery is extension-sensitive and returned `0x80004005 (E_FAIL)` for that non-`.zip` path.

The bootstrap now keeps unverified bytes under `.zip.download`, verifies SHA-256 first, atomically promotes the verified file to its real `.zip` name, and only then invokes Windows Shell extraction. Both temporary artifacts are cleaned on success/failure. A regression policy test now freezes the distinction between the unverified download name and the verified ZIP name. No persisted schema changes are made. Windows fixed FileVersion/ProductVersion is `0.7.0.81`.

## v0.7.0-alpha.8 — Managed Everything Bootstrap

Alpha 8 replaces the old Everything download-page handoff with an opt-in managed bootstrap flow. When the Everything source is enabled, ALTRun Next first probes the existing IPC endpoint and can automatically reuse/start an already-present Everything copy found in its managed tools directory, App Paths, Program Files or PATH. This discovery/start path never downloads anything.

If no usable IPC is available, **Get and start Everything** asks for explicit confirmation before network access. After confirmation, ALTRun Next fetches the current stable Everything 1.4.1.1032 standard portable ZIP for the running ALTRun Next architecture from official voidtools, fetches the official SHA-256 manifest, verifies the downloaded archive with Windows BCrypt, extracts it under `data/tools/Everything`, starts it in the background with `-startup -first-instance`, and waits for IPC readiness. Lite packages are never selected.

The bootstrap runs on a dedicated worker thread. Settings shows discovery, download progress, verification, extraction, startup and IPC-wait state without blocking the launcher UI; application-only search remains available whenever Everything is unavailable. Recheck performs only local discovery/start and never downloads. WinHTTP and the Windows Shell ZIP namespace are used directly—no PowerShell or external downloader is invoked.

No persisted schema changes are made: settings schemaVersion 6, commands schemaVersion 2, TSV v3, usage schemaVersion 1 and provider-cache schemaVersion 2 remain unchanged. Windows fixed FileVersion/ProductVersion is `0.7.0.80`.

## v0.7.0-alpha.7 — Context Actions

Alpha 7 adds context-sensitive right-click actions without copying the original ALTRun menus mechanically. Launcher result menus now follow the selected result: user shortcuts can run, edit, copy their target, reveal filesystem targets and delete; discovered applications/files/folders can be opened or run, added as a pre-filled user shortcut, revealed when they represent a filesystem target, and copied; Everything folders also expose current Explorer/Total Commander navigation when that activation context exists. URL/Smart Action results stay intentionally minimal.

The Shortcut Manager gets its own object-focused menu. Right-clicking a shortcut offers Edit, Test, filesystem reveal when applicable, Copy target and Delete; right-clicking blank list space offers only New shortcut. Toolbar-level Path conversion and window-level Close are intentionally not duplicated into row menus. Mouse right-click and Windows keyboard context-menu invocation are both supported.

Add as shortcut opens the existing Shortcut Editor with name/target/type pre-filled while leaving Keywords empty for the user to choose. Context-action availability and shortcut seeding live in a platform-independent core policy with regression tests; File Explorer reveal remains a Windows shell action. No persisted schema changes are made: settings schemaVersion 6, commands schemaVersion 2, TSV v3, usage schemaVersion 1 and provider-cache schemaVersion 2 remain unchanged. Windows fixed FileVersion/ProductVersion is `0.7.0.70`.

## v0.7.0-alpha.6 Development build

v0.7.0-alpha.6 completes the current Shortcut Manager workflow: the manager shows the full comma-separated keyword set, filters locally by keyword/name/target, and the editor checks conflicts across both primary keywords and aliases. Pause and pin are removed from the user-facing shortcut model; legacy values are normalized to active/non-pinned on load while the schema/TSV compatibility columns remain unchanged.

## v0.7.0-alpha.5.2 — Async Result Icon Pipeline

Alpha 5.2 removes Windows Shell/file icon resolution from the Launcher paint path. With result icons enabled, `WM_DRAWITEM` now performs only a bounded cache lookup; a miss queues work to one lazy background icon worker and immediately continues drawing text. Completed icons return through a private `WM_APP` message and only the matching visible result rows are invalidated.

Icon requests carry both the current search generation and an icon epoch. Typing a newer query clears queued obsolete work, while an already-running old request is discarded when it completes instead of repainting or caching stale results. Appearance/DPI/preference changes advance the icon epoch for the same reason. Cache keys include the requested pixel size, so Classic/DPI-sized icons cannot be reused at the wrong size.

The cache now survives normal query changes and uses a 96-entry LRU cap instead of being destroyed on every result rebuild. Repeated results therefore reuse resolved icons across searches without allowing unbounded HICON growth. Cache entries also remember failed resolutions, preventing repeated Shell calls for the same unresolved source/size.

The default-off preference from alpha.5.1 remains the absolute fast path: no worker is started on a default install until a visible row actually requests an icon, and when icons are disabled the paint path does not queue, resolve, cache or draw them. The existing setting/schema layout is unchanged: settings schemaVersion 6, commands 2, usage 1 and provider-cache 2. Windows fixed FileVersion/ProductVersion is `0.7.0.52`.

## v0.7.0-alpha.5.1 — Optional Result Icons

Alpha 5.1 makes Launcher result icons an explicit **Appearance** preference instead of forcing their shell-resolution cost on every user. **Show search result icons** defaults to **off**, preserving the lightweight text-first behavior by default while keeping every shortcut's custom icon metadata intact.

When the preference is off, the Launcher does not call its icon resolver at all: no portable icon-path resolution, PATH lookup, `LoadImageW`, `SHGetFileInfoW`, `ExtractIconExW`, icon cache population or `DrawIconEx` occurs. Classic and Modern layouts also reclaim the icon column so disabled icons do not leave an empty gutter. Turning the preference on immediately redraws visible results and uses the existing current-result-set icon cache; turning it off immediately destroys that cache.

`settings.json` advances from schemaVersion 5 to **schemaVersion 6** to persist `appearance.showResultIcons`. Schema-5 and older settings migrate atomically with the new preference set to `false`; a schema-5 binary opening the migrated file sees a newer schema and remains read-only. Custom per-command icons, commands.json schemaVersion 2, TSV v3, usage schemaVersion 1 and provider-cache schemaVersion 2 are unchanged. Asynchronous icon loading remains intentionally deferred to the final performance-polish phase. Windows fixed FileVersion/ProductVersion is `0.7.0.51`.

## v0.7.0-alpha.5 — Shortcut Completion

Alpha 5 closes the remaining shortcut-editor gaps before feature freeze. The existing persisted `icon` field is now actually user-configurable from **Advanced options**. Leaving the field blank keeps `auto` behavior; users can choose an `.ico`, `.exe`, `.dll` or `.lnk` source, or reset to Auto without editing JSON.

Launcher results now carry icon-source metadata. Auto icons are derived from the command target when Windows can resolve one, while an explicit user icon takes priority. The owner-drawn Classic and Modern result rows render those icons through the Windows shell/icon APIs. Icon handles are cached only for the current visible result set and destroyed on refresh/destruction, avoiding a long-lived search-history icon cache.

Dynamic shortcuts can now be tested directly inside the Shortcut Editor. Selecting Pass through or URL encode reveals a non-persisted **Test input** field; the Test button sends that value through the same `App::TestCommand -> LaunchCommand -> ResolveRuntimeInput` path used by the real launcher. Invalid URL/Folder templates are rejected before test launch, and an empty Test input is called out instead of silently testing the wrong behavior.

Custom icon paths participate in the existing Path Conversion workflow alongside Target and Working Directory, so portable/environment-variable conversion does not leave a newly configured icon path behind. Shortcut TSV interchange advances to **v3** by appending an optional `icon` column; v1/v2 rows remain importable. JSON schemas do not change: settings stays 5, commands stays 2, usage stays 1 and provider-cache stays 2. Windows fixed FileVersion/ProductVersion is `0.7.0.50`.

## v0.7.0-alpha.4 — Dynamic Shortcut Input

Alpha 4 turns runtime text after an exact shortcut keyword/alias into a first-class command input instead of forcing the whole launcher query through fuzzy search. The Shortcut Editor now exposes **Runtime input** with three modes: **No extra input**, **Pass through**, and **URL encode (UTF-8)**. New templates use `{input}` in target, fixed arguments or working directory.

For Application and Command line shortcuts, Pass through / URL-encoded input may omit `{input}`; the resolved text is then appended after fixed arguments. URL and Folder shortcuts require an explicit `{input}` placement so ALTRun Next never guesses where text belongs. For example, `ping 8.8.8.8` can launch `ping.exe 8.8.8.8`, while `g 心脏 MRI` with `https://www.google.com/search?q={input}` produces a UTF-8 percent-encoded search URL.

The execution payload is carried by the existing LauncherResult action model and resolved only at launch time, after `{folder}` context substitution and before portable-path/environment expansion. The runtime-input matcher uses an exact, case-insensitive first token against the primary keyword or aliases, so the user's argument text is not reinterpreted as fuzzy-search tokens.

`commands.json` advances from schemaVersion 1 to **schemaVersion 2** to persist `runtimeInputMode` (`none`, `raw`, or `url-encoded`). Schema-1 commands migrate atomically. Existing URL shortcuts that use the legacy `{query}` template are promoted automatically to UTF-8 URL-encoded runtime input; `{query}` remains accepted as a compatibility alias, while new UI/examples use `{input}`. An older schema-1 binary sees the migrated schema-2 file as newer and keeps it read-only.

Shortcut TSV interchange advances to v2 by appending the optional `runtimeInputMode` column; existing v1/legacy rows remain importable. Settings stays schemaVersion 5, usage stays schemaVersion 1 and provider-cache stays schemaVersion 2. Windows fixed FileVersion/ProductVersion is `0.7.0.40`.

## v0.7.0-alpha.3.1 — Shortcut Editor Workflow Rework

Alpha 3.1 replaces the alpha.3 form-shaped editor with a task-oriented shortcut workflow. The visible shortcut field now accepts the primary keyword and aliases together (for example `v2rayN, vpn, proxy`); the first unique item remains the persisted primary keyword and the remaining items remain aliases, so commands.json schemaVersion 1 is unchanged.

Name is now optional and auto-suggested from the selected target while remaining editable. Target selection has explicit **File...** and **Folder...** actions. Command type defaults to **Auto detect** and reports the detected runtime type, while Application / URL / Folder / Command line remain available as manual overrides for unusual targets. Existing commands reopen in Auto mode when their stored type agrees with detection, otherwise their explicit type is preserved.

Rare launch fields move behind a progressive **Advanced** section: fixed arguments, working directory, administrator launch, pinned state and **Pause this shortcut**. A blank working directory now means "use the target's directory" for user Application / Command line shortcuts when the resolved target is an absolute filesystem path; URL/Folder commands and non-filesystem targets keep the previous empty-directory behavior.

The parsing, type inference, title suggestion and default-working-directory rules live in a dedicated `ShortcutEditorModel` with regression tests rather than being embedded only in Win32 UI code. Dynamic runtime input/parameter encoding is intentionally not exposed yet; that execution model remains the next feature step.

No persisted schema changes: settings stays schemaVersion 5, commands/usage stay schemaVersion 1 and provider-cache stays schemaVersion 2. Windows fixed FileVersion/ProductVersion is `0.7.0.31`.

## v0.7.0-alpha.3 — Shortcut Editor Usability & Command Type Completion

Alpha 3 begins the feature-completion phase and deliberately leaves global visual polish and further memory tuning for final product cleanup. The standalone New/Edit Shortcut dialog is reorganized into two compact sections inspired by the original AltRun workflow: **Shortcut** for keyword/name/alias/type/target and **Launch options** for arguments, working directory and execution flags.

The command type selector now reliably exposes all four existing runtime command types — **Application, URL, Folder and Command line**. The Win32 dropdown is given a real list height and a four-item minimum-visible count instead of being laid out as a 32-pixel-tall control that could show only one item. URL targets are entered directly and therefore disable the local-file browse button; Application, Folder and Command line retain browse behavior.

No persisted schema changes are made: settings stays schemaVersion 5, commands/usage stay schemaVersion 1 and provider-cache stays schemaVersion 2. Provider storage deduplication, lazy/optional Pinyin and search/ranking behavior are unchanged. Windows fixed FileVersion/ProductVersion is `0.7.0.30`.

## v0.7.0-alpha.2.6 — Pinyin Search Toggle Rendering Fix

Alpha 2.6 fixes the blank first row in General → Search behavior introduced with the Pinyin search toggle. The control was created, localized, laid out and wired to the setting correctly, but its owner-draw ID was omitted from the WM_DRAWITEM routing list and from DrawGeneralToggle's content switch.

The row now renders **Enable Pinyin search / 启用拼音搜索** with an explanatory description. Its runtime behavior is unchanged from alpha.2.5: disabling Pinyin bypasses Hanzi-to-pinyin matching and releases loaded cpp-pinyin resources/cache, while re-enabling remains lazy. Provider command-storage deduplication and settings schemaVersion 5 are unchanged. Windows fixed FileVersion/ProductVersion is `0.7.0.26`.

## v0.7.0-alpha.2.5 — Provider Storage Deduplication & Pinyin Search Control

Alpha 2.5 removes the long-lived raw Provider command copy. Provider cache data is now loaded only for a synchronous merge, exposed to the merge algorithm through temporary `const Command*` views, and released immediately after the final searchable `commands_` vector is built. The raw Provider command count remains available as a scalar Diagnostics metric, while accepted/suppressed source statistics retain their previous semantics.

User shortcut edits still rebuild Provider de-duplication correctly: the current Provider enable map is retained, the generated provider cache is re-read transiently, and no Provider pointer survives the merge call. This preserves stable command indexes, user-command override behavior, Provider refresh behavior, usage mapping and result lifetime while eliminating the second resident Provider `Command` vector.

The General → Search behavior card now includes **Enable Pinyin search / 启用拼音搜索**, enabled by default. When disabled, ASCII queries do not enter the Pinyin path. If cpp-pinyin was already loaded, disabling the setting releases the converter and Pinyin cache immediately; re-enabling keeps first-use lazy initialization.

Because the new Pinyin preference is persisted, settings.json advances from schemaVersion 4 to **schemaVersion 5**. Existing schema-4 files migrate with `behavior.pinyinSearch: true`; an alpha.2.4/schema-4 downgrade sees the newer schema and remains read-only. Commands/usage stay schemaVersion 1 and provider-cache stays schemaVersion 2. Windows fixed FileVersion/ProductVersion is `0.7.0.25`.

## v0.7.0-alpha.2.4 — Lazy Pinyin Initialization

Alpha 2.4 converts cpp-pinyin from eager startup initialization to first-use initialization. At process startup ALTRun Next now keeps only the dictionary path and a lightweight dictionary-presence state; the `Pinyin::Pinyin` converter is not constructed until an ASCII pinyin-capable search actually reaches a field containing supported Hanzi.

This keeps pinyin discovery behavior intact, including `weixin → 微信`, initials and hybrid pinyin matching. The existing cache semantics and search scoring/ranking are unchanged. Diagnostics now report **Pinyin: Not loaded / 未加载** with Cache 0 on a fresh start, then switch to **Loaded / Ready / 已加载 / 可用** after the first search that needs Hanzi-to-pinyin conversion.

The lazy state is guarded for concurrent Diagnostics/search access. `PinyinAvailable()` remains side-effect-free: before first use it reports whether the required dictionary is present, and after a load attempt it reflects whether the converter initialized successfully.

This release deliberately does not change Provider command storage, cache limits, persisted schemas, Everything IPC or Smart Actions so the memory A/B remains attributable to Pinyin initialization alone. Windows fixed FileVersion/ProductVersion is `0.7.0.24`.

## v0.7.0-alpha.2.3 — Memory Diagnostics & Baseline

Alpha 2.3 adds runtime observability before any memory-optimization work. It deliberately does not trim the process working set or change search/provider storage architecture.

The Diagnostics page now refreshes once per second while visible and reports **Working Set**, **Peak Working Set** and **Private Bytes** using the Windows process-memory counters. It also exposes the search-data baseline: user shortcut count, raw loaded Provider command count, merged searchable command count, Pinyin converter loaded/ready state, Pinyin cache entry count, Provider refresh state and Provider monitor state.

A dedicated Windows `ProcessMemory` platform layer and `process_memory_tests` runtime test keep the measurement API reusable for later before/after optimization work. The Diagnostics note explicitly warns that these counters use a different accounting model from Task Manager's Memory column and that ALTRun Next does not call `EmptyWorkingSet` or `SetProcessWorkingSetSize`.

No persisted schema changes in alpha.2.3: settings schemaVersion 4, commands/usage schemaVersion 1 and provider-cache schemaVersion 2 remain unchanged. Search behavior, Pinyin behavior, Provider storage, Everything IPC and Smart Actions are unchanged. Windows fixed FileVersion/ProductVersion is `0.7.0.23`.

## v0.7.0-alpha.2.2 — True Grouped Path Preview

Alpha 2.2 replaces the alpha.2.1 compact presentation with an actual hierarchical path-preview structure while preserving the existing Common Controls version and all alpha.2 conversion semantics.

Each shortcut now receives its own full-width header row, drawn across the entire ListView using system colors and a semibold system font (for example `test — Test`). Header rows have no checkbox, cannot be selected or applied, and visually separate one shortcut from the next. The convertible Target and Working Directory rows are indented beneath the header and remain independently checkable.

The redundant Shortcut data column is removed. The data grid now contains only Field, Current path, Converted and Status. Applying selected conversions walks only real data rows; structural header rows can never enter the atomic commands.json update batch.

No Common Controls v6 manifest is introduced, so this focused UX change does not alter the visual style of Settings, Shortcut Manager or other Win32 controls. Windows fixed FileVersion/ProductVersion is `0.7.0.22`.

## v0.7.0-alpha.2.1 — Grouped Path Conversion Preview

Alpha 2.1 is a focused UX patch for the path conversion dialog. Conversion behavior, path resolution and persisted schemas are unchanged from alpha.2.

Preview rows are now visually grouped by shortcut without requiring a global Common Controls v6 migration. The first convertible field row shows the shortcut label (for example `test — KOOK`); additional fields from the same shortcut are placed directly below it with the Shortcut cell left blank. This removes the confusing repeated shortcut name while preserving the current Win32 control stack and Windows 10 compatibility.

Target and Working Directory remain independently checkable, so either field or both can be applied. The footer now reports both the number of shortcuts with convertible paths and the number of convertible fields.

Windows fixed FileVersion/ProductVersion is `0.7.0.21`.

## v0.7.0-alpha.2 — Shortcut Manager Usability & Path Portability

Alpha 2 removes the manual Move Up / Move Down controls from Shortcut Manager. The persisted `sortOrder` field remains for compatibility and deterministic tie-breaking, but it is no longer presented as a primary user action because launcher ranking is driven by matching, usage and pinning rather than manual list position.

Shortcut Manager now exposes **Path conversion... / 路径转换...**. The conversion dialog scans user shortcut Target and Working Directory fields, previews every safe conversion, verifies whether the resolved path currently exists, checks accessible rows by default, and applies selected conversions in one atomic `commands.json` save. Arguments are deliberately untouched, and URL, UNC and bare shell commands are left unchanged.

Two conversion modes are provided: **Portable** converts absolute paths into a nearby ALTRun Next-relative path or a known Windows environment-variable path; **Expand** resolves relative/environment-variable paths into the current machine's absolute path. Relative Target paths that contain path structure, and all relative Working Directory paths, are resolved against the directory containing `ALTRunNext.exe`. Bare targets such as `notepad.exe` or `cmd.exe` keep normal Windows/Shell lookup behavior.

The Shortcut Manager list columns now receive their labels when they are created, fixing the blank header regression seen in alpha.1. The visible columns are Keyword, Name, Type and Target / command.

No persisted schema changes in alpha.2: settings schemaVersion 4, commands/usage schemaVersion 1 and provider-cache schemaVersion 2 remain unchanged. Windows fixed FileVersion/ProductVersion is `0.7.0.2`.

## v0.7.0-alpha.1 — Shortcut Management Architecture

v0.7 starts by promoting user shortcuts into a first-class workflow instead of treating them as a Settings page. The persisted command contract is unchanged: existing `commands.json` schemaVersion 1 data is reused directly with no migration.

A new standalone **Shortcut Manager / 快捷项管理** window provides add, edit, delete, test and ordering actions over user shortcuts. A reusable **Shortcut Editor / 快捷项编辑** dialog owns single-shortcut editing and is intentionally separated from the manager so the same editor can be invoked by Launcher result context actions in the next alpha.

The system-tray menu now exposes the product's primary structure directly: Show Launcher, Shortcut Manager, Settings, Reload, About and Exit. Appearance and language remain available in Settings rather than being duplicated in the tray.

Settings no longer creates or exposes the legacy Shortcuts page. It now opens on General and its sidebar order is General, Hotkeys, Appearance, Search sources, Data, Diagnostics, About, placing Diagnostics immediately above About as part of the support/status area.

No v0.6 persisted contract is changed in alpha.1: settings schemaVersion 4, commands/usage schemaVersion 1, provider-cache schemaVersion 2, provider IDs/defaults, Hotkey Registry action IDs, Everything IPC, Smart Actions and Classic geometry remain compatible. Windows fixed FileVersion/ProductVersion is `0.7.0.1`.

## v0.6.0 — Stable

v0.6.0 promotes the fully validated v0.6.0-rc.1 contract to Stable without changing launcher, provider, Smart Action, Hotkey Registry, Everything, Diagnostics, migration or desktop runtime behavior.

The v0.6 line delivers centralized customizable Hotkeys; Smart Actions for Explorer, Windows Open/Save dialogs and Total Commander; reusable {folder}, {query}, URL, Web and clipboard/text actions; optional Everything Query2/WM_COPYDATA filesystem search with application-search fallback; runtime Diagnostics; and hardened upgrade/downgrade protection.

Stable keeps the RC release gates intact: clean-install defaults, v0.5.0/alpha.5 schema 3 -> 4 migration, alpha.6.1/beta.1/beta.2 schema 4 -> 4 compatibility, downgrade read-only protection, Windows 10 API baseline, Windows desktop/runtime smoke, packaged x64 schema 2 -> 4 migration, x64/ARM64 package contracts and immutable asset checksums.

The persisted contract remains settings schemaVersion 4, commands/usage schemaVersion 1 and provider-cache schemaVersion 2. Provider IDs/defaults, the five Hotkey Registry action IDs, Everything IPC contract, Smart Actions evaluation, Diagnostics owner-draw routing and Classic geometry 420/16/10 remain frozen from RC.1. Windows fixed FileVersion/ProductVersion is `0.6.0.300`.

## v0.6.0-rc.1 — Release Freeze & Upgrade Gate

RC 1 freezes the v0.6 user-facing contract. No new provider, Smart Action family, Hotkey Registry action, settings preference or schema is introduced. This candidate converts the upgrade, downgrade, clean-install and packaging scenarios validated during beta into automated release gates.

A dedicated `upgrade_matrix_tests` target now covers clean settings creation; representative v0.5.0 and v0.6.0-alpha.5 schema-3 upgrades to schema 4; v0.6.0-alpha.6.1, beta.1 and beta.2 schema-4 compatibility loads without rewrite; preservation of custom providers, behavior, appearance and Hotkey Registry bindings; schema-3 hotkey collision handling; and schema-4 downgrade read-only protection.

The packaged x64 runtime smoke now verifies that a legacy schema-2 portable profile actually migrates to schema 4 at runtime, contains all five frozen Hotkey Registry action IDs, and keeps Everything disabled unless the legacy profile opted in. Release ZIPs now include `V0.6_RC_VALIDATION.md`, and the package allowlist requires it.

Frozen contracts remain: settings schemaVersion 4; commands/usage schemaVersion 1; provider-cache schemaVersion 2; external optional Everything Query2/WM_COPYDATA integration; Diagnostics page/owner-draw routing; Smart Actions evaluation semantics; provider IDs/defaults; five Hotkey Registry action IDs; and Classic geometry 420/16/10. Windows fixed FileVersion/ProductVersion is `0.6.0.200`.

## v0.6.0-beta.2 — Diagnostics UX & Real-world Fixes

Beta 2 starts with a real-world Settings navigation fix reported during beta.1 validation. The Smart Actions runtime page is now named **Diagnostics / 诊断**, matching its actual purpose: inspecting Windows activation context, Everything IPC, {folder} availability, clipboard/web readiness and concrete unavailable reasons rather than configuring actions.

The blank selected item in the Settings sidebar is fixed. The Diagnostics navigation button was created, labelled and routed correctly in beta.1, but its owner-draw control ID was accidentally omitted from the WM_DRAWITEM dispatch list. Beta 2 adds the Diagnostics ID to that dispatch path and freezes the requirement in the release-contract verifier.

This is a UI/diagnostics correction only. Settings remains schemaVersion 4, provider-cache remains schemaVersion 2, provider defaults and the five Hotkey Registry action IDs remain frozen, and Smart Actions execution semantics are unchanged. Windows fixed FileVersion/ProductVersion is 0.6.0.101.

## v0.6.0-beta.1 — Smart Actions UX & Diagnostics

Beta 1 starts the v0.6 feature freeze. It does not add a new provider, action family or persisted preference. Instead it makes the Smart Actions already introduced during alpha easier to inspect and safer to stabilize before RC.

Settings now includes a dedicated Actions / 操作 page. The page reports the last captured Windows activation context (Explorer, Open/Save dialog, Total Commander or none), source folder / active TC panel when available, {folder} availability, current-file-manager navigation availability with a concrete reason when unavailable, and Everything IPC/fallback state. The activation snapshot is process-memory only and is not written to settings, usage history or provider-cache.

Launcher action resolution now exposes an ActionEvaluation contract alongside the existing resolved action. It records whether the requested contextual action is available and why it is unavailable (ResultNotFolder, NoSupportedFileManager, NoCopyableTarget, or InvalidActionTarget). Execution still preserves the published alpha fallback behavior: diagnostics can explain that contextual navigation is unavailable without changing what Enter/Ctrl+Enter already does.

Hotkey diagnostics now distinguish Windows-global registration from launcher-local readiness using explicit runtime-status wording. Desktop validation is also hardened: the fake Total Commander runtime smoke now covers both panels plus UNC, spaces and Unicode paths in context capture and WM_COPYDATA navigation.

No persisted contract changes in beta.1: settings.json remains schemaVersion 4, commands.json / usage.json remain schemaVersion 1, provider-cache.json remains schemaVersion 2, provider IDs/defaults and the five Hotkey Registry action IDs stay frozen, Everything remains an external optional Query2/WM_COPYDATA engine, and Classic geometry remains 420/16/10. Windows fixed FileVersion/ProductVersion is 0.6.0.100.

## v0.6.0-alpha.6.1 — Hotkey Migration Collision Hardening

Alpha 6.1 hardens schema-3 → 4 migration for users who had already customized a global activation chord to a key that alpha.6 introduces as a launcher-local default, such as `F2`, `Ctrl+Enter` or `Ctrl+Shift+C`.

Migration priority is deterministic: existing user global activation bindings are preserved. If a newly introduced optional launcher-local default would duplicate one of those established chords, the new local action is migrated as **disabled** rather than changing the user's global binding or persisting two enabled actions with the same chord.

Schema-4 loading also seeds global activation from the compatibility `hotkey` mirror before applying `hotkeys.bindings`, making a partial schema-4 document fail safer. The Hotkey Registry surface, action IDs and settings schema stay unchanged from alpha.6. Windows fixed FileVersion/ProductVersion is `0.6.0.61`.

## v0.6.0-alpha.6 — Centralized Hotkey Registry & Settings

Alpha 6 replaces feature-specific hard-coded shortcut checks with a centralized **Hotkey Registry** and a dedicated **Hotkeys / 快捷键** Settings page. New hotkey-enabled actions now have a stable action ID, scope, default binding and validation policy in one registry.

The initial registry contains:

| Action ID | Scope | Default |
| --- | --- | --- |
| `launcher.activate` | Windows global | `Alt + Space` |
| `launcher.activateSecondary` | Windows global | disabled, `Pause` when enabled |
| `launcher.openSettings` | Launcher | `F2` |
| `result.navigateCurrentFileManager` | Launcher | `Ctrl + Enter` |
| `result.copySelectedTarget` | Launcher | `Ctrl + Shift + C` |

**Global** bindings use Windows `RegisterHotKey` and remain transactional: a replacement is persisted only after Windows accepts it; if registration fails, ALTRun Next re-establishes the previous working binding. **Launcher** bindings are matched only while the launcher is open and do not reserve keys system-wide.

Settings now exposes all five actions on one page. Select an action, click its current binding, then press the replacement key combination. `Esc` cancels capture. The page shows scope and runtime status, supports disabling optional bindings, resetting one binding, and restoring every hotkey to its published default.

Conflict handling is registry-wide. Two enabled actions cannot share the same chord. The primary activation binding cannot be disabled and requires at least one modifier. Launcher-local bindings also protect the search edit control: unmodified character, Space and editing/navigation keys remain owned by normal query input. Bare local action bindings are limited to function-style keys such as `F1–F24` or `Pause`; character/editing keys can still be used when combined with modifiers.

The previous hard-coded launcher checks for `F2`, `Ctrl+Enter` and `Ctrl+Shift+C` now dispatch through `MatchHotkeyAction`, so changing a binding changes runtime behavior immediately without modifying the feature implementation.

### Settings schemaVersion 4

Alpha 6 upgrades `settings.json` from schemaVersion 3 to **schemaVersion 4**. Existing primary and auxiliary global bindings are migrated into:

```json
"hotkeys": {
  "bindings": {
    "launcher.activate": {
      "enabled": true,
      "modifiers": ["alt"],
      "key": "space"
    }
  }
}
```

Launcher-local actions receive their published defaults during migration. The stored schema-3 `hotkey` object is deliberately retained as a compatibility mirror for the two global activation bindings. This allows an alpha.5 binary to read the familiar global fields during a downgrade while its existing newer-schema protection keeps the schema-4 document read-only and byte-preserving.

No other persisted contract changes: `commands.json` and `usage.json` remain schemaVersion 1, `provider-cache.json` remains schemaVersion 2, provider IDs/defaults remain frozen, and Classic geometry remains 420/16/10. Windows fixed FileVersion/ProductVersion is `0.6.0.60`.

## v0.6.0-alpha.5 — Clipboard & Text Actions

Alpha 5 adds the first clipboard-oriented Smart Action while keeping normal edit-control clipboard behavior intact.

### Copy arbitrary text

Type `copy <text>`, `clip <text>` or `复制 <text>`. ALTRun Next generates a runtime-only **Copy text** Action; Enter, double-click or numeric quick execution writes the payload to the Windows Unicode clipboard and then follows the existing hide-after-launch preference.

Examples:

```text
copy docker compose up -d
clip D:\Research\CKD\notes.txt
复制 一段临时文本
```

The text after the alias is copied literally after surrounding whitespace is trimmed. No command, file or URL is executed.

### Copy the selected result target

While the launcher input remains focused, **Ctrl+Shift+C** copies the selected result's executable target, file/folder path, resolved URL or Smart Action payload. This shortcut is intentionally separate from ordinary **Ctrl+C**, which keeps the native edit-control behavior for copying selected query text.

Examples:

- Everything File/Folder result → full filesystem path.
- Direct URL / web-search action → resolved URL.
- Application / User Command → displayed/resolved target.
- Copy-text action → its text payload.

If a result has no copyable target, Ctrl+Shift+C performs no launch and leaves the launcher open.

Clipboard output uses `CF_UNICODETEXT`; Chinese text, paths with spaces and UNC paths are preserved. The implementation retries briefly if another process temporarily owns the clipboard and never reads, logs or persists existing clipboard contents.

`builtin.clipboard` is a runtime-only Smart Action provider, analogous to `builtin.web`. It is not added to Search Sources, settings.json or provider-cache.json.

No persisted schema changes in alpha 5: settings remains schemaVersion 3, commands/usage remain schemaVersion 1, provider-cache remains schemaVersion 2, and Classic geometry remains 420/16/10. Windows fixed FileVersion/ProductVersion is `0.6.0.50`.

## v0.6.0-alpha.4 — Total Commander Context & {folder} Templates

Alpha 4 extends Windows Activation Context to **Total Commander 9+** and adds the developer-oriented `{folder}` user-command template.

When ALTRun Next is invoked from a foreground Total Commander window, it captures that exact `TTOTAL_CMD` HWND, process ID, active panel and (when the active panel is a normal filesystem location) its current folder. Folder search keeps the same file-manager gesture introduced for Explorer:

- Enter keeps normal Folder execution.
- Ctrl+Enter navigates the **captured Total Commander active/source panel** to the Folder result.
- Multiple Total Commander instances are safe: ALTRun Next sends the navigation command to the exact window captured at hotkey activation rather than finding an arbitrary global instance.
- If the captured window disappears, changes process, becomes ambiguous, or the active panel changes before execution, contextual navigation is refused instead of targeting another panel/window.
- Total Commander is optional. When it is absent or does not support the query protocol, normal ALTRun Next behavior is unchanged.

The integration uses Total Commander's external-control interfaces: `WM_USER+50` obtains the active panel/path control and `WM_COPYDATA` with the `CD` command changes the source panel. Unicode destinations are sent as UTF-8 with a BOM. No Total Commander plugin, DLL or configuration file is required.

### `{folder}` developer commands

User Commands may place the literal token `{folder}` in **Target**, **Arguments**, and/or **Working Directory**. At launcher activation time it resolves to the captured real filesystem folder from File Explorer or Total Commander's active panel.

Example — VS Code in the current folder:

```text
Name: VS Code Here
Keyword: codehere
Type: Application
Target: code
Arguments: "{folder}"
Working Directory: {folder}
```

Example — PowerShell in the current folder:

```text
Name: PowerShell Here
Keyword: pshere
Type: Application
Target: powershell.exe
Arguments: -NoExit
Working Directory: {folder}
```

Contextual commands are not shown by launcher search when no real filesystem folder is available. Explorer Home / This PC and Total Commander FTP/plugin panels therefore never substitute an empty string, a remembered old path, or a guessed location. The persisted command itself is never rewritten: substitution happens in a session-only working copy immediately before search/presentation and is revalidated again before execution. `{folder}` may coexist with `{query}` in a URL command because the folder token is resolved before web-action generation.

No persistence migration is required: settings remains schemaVersion 3, commands/usage remain schemaVersion 1, provider-cache remains schemaVersion 2, and Classic geometry remains 420/16/10. Windows fixed FileVersion/ProductVersion is `0.6.0.40`.

## v0.6.0-alpha.3 — Open / Save Dialog Folder Jump

Alpha 3 extends the Activation Context foundation to Windows standard Open / Save / folder-picker dialogs. If ALTRun Next is invoked while a supported Common Item Dialog or Explorer-style Common File Dialog is foreground, a filesystem **Folder** result now uses the captured dialog as its default navigation surface.

The interaction is intentionally context-sensitive:

- From a normal Explorer window: Enter keeps the established normal-open behavior; Ctrl+Enter navigates the captured Explorer.
- From a supported Open / Save / folder-picker dialog: Enter, double-click and other default-result execution navigate that same file dialog to the Folder result instead of opening a separate Explorer window.
- Ctrl+Enter remains the explicit Explorer-navigation gesture and is deliberately not repurposed inside a file dialog while the physical Ctrl key is still held.
- File results are not auto-selected or submitted in this alpha; only Folder navigation is contextual.

File-dialog recognition is conservative: the foreground root must be the standard `#32770` dialog class and must host a `SHELLDLL_DefView`. Ordinary message/settings dialogs are therefore not treated as file pickers. Execution revalidates the captured HWND and process ID before sending any input.

Cross-process navigation does not use the clipboard and does not overwrite the dialog's File name field. ALTRun Next restores the captured dialog to foreground, invokes its standard Ctrl+L address surface, sends the target path as Unicode input, then presses Enter. If Windows foreground/UIPI rules prevent safe targeting, the contextual action fails rather than injecting input into another window.

No persisted Settings or provider contract changes in alpha 3. settings.json remains schemaVersion 3, commands/usage remain schemaVersion 1, provider-cache remains schemaVersion 2, and Classic geometry remains 420/16/10. Windows fixed FileVersion/ProductVersion is `0.6.0.30`.

## v0.6.0-alpha.2.1 — Explorer Virtual-Location Hotfix

Alpha 2.1 fixes contextual folder navigation when ALTRun Next is invoked from Explorer **Home / 主文件夹**, This PC, Quick access, Network or another Shell namespace location that has no ordinary filesystem path.

The alpha.2 implementation correctly captured Explorer through the Windows Shell automation model, but it treated the source Explorer context as valid only when the source location could also be converted to a filesystem path. That condition was unnecessarily strict: Ctrl+Enter only needs a reliable captured Explorer browser/view plus a filesystem path for the **destination** folder.

The source context now becomes valid as soon as a unique active Explorer Shell view is resolved. The current source path remains optional diagnostic data. Filesystem locations continue to behave exactly as before, ambiguous Windows 11 tab/window candidates are still rejected instead of guessed, and Ctrl+Enter from a non-Explorer application still falls back to normal folder opening.

No persisted schema, provider default, Everything transport or Classic geometry changes in this hotfix. Windows fixed FileVersion/ProductVersion is `0.6.0.21`.

## v0.6.0-alpha.2 — Windows Context & Explorer Navigation

Alpha 2 turns the v0.6 Smart Action contract into the first context-aware Windows action. When the global hotkey opens ALTRun Next from File Explorer, the application captures the foreground Explorer **before** the launcher takes focus and keeps that activation snapshot for the current launcher session.

Everything Folder results keep their existing default behavior: **Enter**, double-click and Classic numeric quick launch still open the folder normally. The new shortcut is **Ctrl+Enter**. When a valid Explorer activation context exists, Ctrl+Enter navigates that same captured Explorer window/tab to the selected folder instead of opening another Explorer window.

Explorer discovery uses the Windows Shell automation model rather than window titles or address-bar text. ALTRun Next enumerates `IShellWindows`, resolves the active `IShellView` / filesystem folder and uses foreground focus/visibility signals to identify the active Explorer view. Multiple unresolved candidates are treated as ambiguous and no contextual navigation is attempted; the code deliberately does not guess between Windows 11 tabs. Shell namespace locations without a filesystem path are not considered valid Explorer contexts in this alpha.

If ALTRun Next was invoked from a non-Explorer application, Ctrl+Enter safely retains the normal folder-open behavior. If a captured Explorer disappears or can no longer be resolved before execution, the contextual action fails rather than silently navigating a different window.

This phase intentionally does **not** add `{folder}` command templates, Open/Save dialog control, Total Commander integration or new Settings. settings.json remains schemaVersion 3, commands/usage remain schemaVersion 1, provider-cache remains schemaVersion 2, provider defaults are unchanged, and Classic Launcher geometry remains frozen at 420/16/10.

Windows fixed FileVersion/ProductVersion for this build is `0.6.0.2`.

## v0.6.0-alpha.1 — Action Contract Foundation & URL/Web Actions

v0.6 starts the Smart Actions & Windows Navigation line by separating action execution data from launcher presentation metadata. `LauncherAction` now carries an explicit payload and supports `OpenUrl` alongside the existing command/file/folder actions, while `ResultKind::Action` lets future Windows-context operations participate in the same unified result model without inventing a new result kind for every action.

The first user-facing consumer is lightweight URL/web integration rather than a calculator. Typing a complete `http://` or `https://` URL creates an Open URL action directly; `www.` addresses are normalized to HTTPS. Normal application/file/folder search continues unchanged.

Existing URL user commands can now become web-search aliases without changing commands.json schemaVersion 1. Put `{query}` in the URL target, for example:

```text
Keyword: g
Aliases: google
Type: URL
Target: https://www.google.com/search?q={query}
```

Then `g ALTRun Next` resolves to `https://www.google.com/search?q=ALTRun%20Next`. Query text is UTF-8 percent-encoded, including Chinese and other Unicode text. Commands without `{query}` keep their previous behavior. Template actions are limited to HTTP/HTTPS targets; the generated action participates in unified ranking, Enter/double-click, Classic numeric quick launch and the existing single-result execution policy.

This alpha deliberately keeps settings schemaVersion 3, commands/usage schemaVersion 1, provider-cache schemaVersion 2, all five v0.5 provider defaults and Classic geometry (420/16/10) unchanged. `builtin.web` is a runtime action-provider ID only and is not persisted in Search Sources.

Windows fixed FileVersion/ProductVersion for this build is `0.6.0.1`.

## v0.5.0 — Stable

v0.5.0 promotes the validated RC3 code line to Stable without adding new launcher features or changing the frozen v0.5 contracts.

The release brings native Everything-backed File/Folder search into ALTRun Next through the external standard Everything application. Search remains responsive when Everything is unavailable, recovers without restarting ALTRun Next after Everything starts again, and combines User Commands, Applications, Folders and Files through the unified ranking path. Everything remains disabled by default and continues to use the 1.4-compatible Unicode Query2/WM_COPYDATA transport with conservative named-instance fallback.

Settings schemaVersion 3 is now the stable v0.5 settings format. v0.4.1 schema-2 settings migrate atomically while preserving existing preferences, and newer schema-3 settings stay protected from writes when temporarily opened by a v0.4.1-era binary. Commands/usage remain schemaVersion 1 and provider-cache remains schemaVersion 2.

The final RC3 Settings polish is included unchanged: 54-logical-pixel owner-drawn toggle rows, single-line shortcut-editor labels, width-aware editor actions, separated Everything diagnostics/onboarding controls, aligned page gutters and owner-drawn sidebar navigation. Classic Launcher geometry remains frozen at 420 logical px width, 16 logical px row height and 10 visible results.

Stable publication keeps the full release safety chain: Core regression tests, 100/125/150/200% layout checks, current-Windows runtime smoke, Windows 10 API-baseline smoke, x64/ARM64 production builds, exact package allowlisting, packaged x64 startup smoke and SHA-256 self-verification. The packaged RC validation document is retained as the explicit real-desktop regression record; Stable promotion does not rewrite unchecked observations as automated passes.

Windows fixed FileVersion/ProductVersion for this build is `0.5.0.300`.

## v0.5.0-rc.3 — Final Settings UI Polish & Stable Candidate

RC3 is the final Settings-layout polish pass for the frozen v0.5.0 feature set. It fixes the real desktop overlap/cropping issues found during RC validation without changing launcher search behavior, schemas, provider IDs or the Everything transport.

General Settings owner-drawn rows now use a shared 54-logical-pixel metric with dedicated title and description bands, eliminating the previous 46-pixel layout where explanatory text could overlap the next row. The General layout helper and UI now share the same content gutters and row metric, with regression coverage at 96/120/144/192 DPI.

The shortcut editor keeps all field labels on one line, widens the bilingual label column, aligns edit/browse controls to a common height, and distributes option/action controls from the actual available field width so the lower editor controls no longer collide at the minimum Settings width.

Search Sources now uses the same polished toggle-row metric. Everything diagnostics, onboarding actions and explanatory text occupy separate vertical regions, preventing the RC2 Get Everything/Recheck controls from covering status text. Provider content aligns with the same page gutters used by the page header.

The Settings minimum width is now 960 logical pixels so Chinese/English labels remain usable while still fitting a 1920-pixel-wide 200% DPI desktop. General remains vertically scrollable on constrained work areas; the existing PerMonitorV2 DPI and work-area clamping behavior is unchanged.

The sidebar navigation is also polished into a borderless owner-drawn list: the active page uses a subtle selected background, a four-logical-pixel accent bar and semibold text instead of native boxed buttons plus a textual bullet. Navigation order and keyboard/click behavior are unchanged.

RC3 does not change Classic Launcher geometry (420/16/10), settings schemaVersion 3, commands/usage schemaVersion 1, provider-cache schemaVersion 2, provider defaults, ranking or Query2/WM_COPYDATA. This is intended to be the final code candidate before v0.5.0 Stable unless a release-blocking regression is found.

Windows fixed FileVersion/ProductVersion for this build is `0.5.0.202`.

## v0.5.0-rc.2 — Everything Onboarding & Diagnostics UX

RC2 is a release-candidate UX fix discovered during real desktop validation. It does not change the frozen v0.5 provider/schema/transport surface.

When Everything search is enabled but no Everything IPC endpoint is present, Search Sources now reports **Everything not detected** instead of surfacing raw Windows error 2 as the primary diagnosis. The page explains that ALTRun Next does not bundle or auto-start Everything, that the standard edition must be installed and running, and that Everything Lite has no IPC. Application-search fallback remains active.

Two small recovery actions are available only while Everything is unavailable: **Get Everything** opens the official voidtools download page in the default browser, and **Recheck** immediately re-probes the existing live IPC status. Starting Everything later still requires no ALTRun Next restart. Ambiguous named-instance diagnostics remain distinct from the missing-installation guidance.

This remains an external-first integration. RC2 does not download, install, update, start or manage Everything. A future managed/portable Everything workflow remains a post-v0.5 feature.

All frozen RC contracts remain unchanged: settings schemaVersion 3, commands/usage schemaVersion 1, provider-cache schemaVersion 2, Everything default-off, Query2/WM_COPYDATA, no Everything DLL/named-pipe dependency and unchanged Classic launcher geometry.

Windows fixed FileVersion/ProductVersion for this build is `0.5.0.201`.

## v0.5.0-rc.1 — Release Candidate Stabilization & Real-world Validation

RC1 freezes the v0.5.0 feature surface. No new provider, schema, launcher geometry or Everything transport is introduced after beta.2. The candidate keeps settings schemaVersion 3, commands/usage schemaVersion 1, provider-cache schemaVersion 2, the five existing provider IDs, Everything default-off, the 1.4-compatible Unicode Query2/WM_COPYDATA transport and the established Classic geometry.

The beta.2 compatibility behavior is now treated as the RC baseline: unnamed Everything IPC is preferred, exactly one named instance may be selected conservatively when the unnamed endpoint is absent, multiple named instances fall back rather than guessing, and reply sender/payload/count/range validation remains mandatory. File/Folder results stay query-time only and are never written to provider-cache or usage history.

RC1 adds a single packaged v0.5 validation checklist, `V0.5_RC_VALIDATION.md`, covering real Windows 10/11 desktop validation, Everything 1.4/current 1.5 beta, named/multiple instances, schema 2→3 migration and downgrade protection, mixed DPI, IME, UNC/extended-length paths, long-run soak, package/hash verification and final stable sign-off. The existing Everything compatibility matrix is also shipped in both x64 and ARM64 packages.

Automated CI still covers Core tests, Windows current runtime smoke, Windows 10 API baseline, x64/ARM64 production builds, exact package contents, packaged x64 startup and SHA-256 self-verification. Manual checklist boxes remain intentionally unmarked until observed on real hardware/software; an RC tag is not itself a manual sign-off.

Windows fixed FileVersion/ProductVersion for this build is `0.5.0.200`.

## v0.5.0-beta.2 — Performance & Real-world Compatibility Hardening

Beta 2 keeps the beta.1 settings/schema surface frozen and hardens the existing native Query2 IPC path for real Everything installations. The default unnamed `EVERYTHING_TASKBAR_NOTIFICATION` endpoint is always preferred. When that endpoint is absent, ALTRun Next can discover the standard named-instance form `EVERYTHING_TASKBAR_NOTIFICATION_(instance)`; it auto-selects only when exactly one named instance exists. Multiple named instances are reported as ambiguous and ALTRun Next falls back to application-only search rather than guessing which database to query.

This keeps the 1.4-compatible Unicode Query2/WM_COPYDATA transport as the common baseline. Everything 1.5's newer SDK uses named pipes, but beta.2 intentionally does not adopt that 1.5-only transport or add an Everything DLL dependency. Current Everything 1.5 beta can use the normal unnamed endpoint, while legacy 1.5a/custom named-instance setups can use the conservative named-window fallback.

IPC reply hardening now validates the sender HWND, enforces the requested result-count ceiling, rejects oversized reply payloads, rejects inconsistent LIST2 total/offset/count ranges, and preserves the drive/root flag separately from normal folders. File/folder normalization is covered for drive roots, UNC targets and extended-length `\\?\` paths.

Windows runtime smoke now stresses 128-query debounce/coalescing, 256-result replies with large total-match counts, the provider's 1000-result transport cap, unavailable/recovery, unique/ambiguous named instances, spoofed reply senders, reply-size limits and malformed over-limit result lists.

Persisted data remains unchanged from beta.1: settings schemaVersion 3, commands/usage schemaVersion 1 and provider-cache schemaVersion 2. Everything remains default-off and File/Folder results remain ephemeral. Classic launcher geometry remains frozen.

Windows fixed FileVersion/ProductVersion for this build is `0.5.0.101`.

## v0.5.0-beta.1 — Everything Settings, Diagnostics & Migration

Beta 1 promotes Everything from an alpha-only raw JSON opt-in to a supported Search Sources setting. Settings > Search sources now includes **Everything files & folders** alongside Start Menu, Windows Apps, App Paths and PATH. Everything remains off by default; enabling it starts no process and installs nothing. ALTRun Next continues to use the native Everything 1.4-compatible Unicode Query2 IPC and requires a running standard Everything instance. Everything Lite has no IPC and is reported as unavailable.

The Search Sources page now exposes live Everything diagnostics while it is open. It refreshes availability once per second and after completed dynamic queries, showing whether IPC is currently available, the most recent query outcome, returned/total match counts, latency and native Windows error code when relevant. If Everything is enabled but IPC is unavailable, ALTRun Next explicitly reports that application-search fallback is active; static User Command/Application search remains fully usable.

Availability is re-probed rather than permanently cached. If Everything is started after ALTRun Next, or is stopped and later restarted, the same running launcher can recover on a later query without an ALTRun Next restart. CI covers the unavailable → available transition with the same IPC client and fake Everything window class.

Beta 1 formalizes this configuration surface as **settings schemaVersion 3**. The schema-3 provider map now contains `"everything.filesystem": false` by default. Existing schema-2 settings migrate atomically: users who manually enabled Everything during alpha keep `true`; users without that experimental key migrate with Everything safely disabled. Commands remain schemaVersion 1, usage remains schemaVersion 1 and provider-cache remains schemaVersion 2.

Downgrade protection is explicit: a schema-3 settings file presented to a schema-2 reader is classified as newer/unsupported and is left byte-for-byte unchanged. The existing newer-schema read-only compatibility path therefore protects beta settings when temporarily returning to v0.4.1-era binaries.

Everything File/Folder results remain query-time ephemeral data: they are not written to provider-cache or usage history. Classic launcher geometry remains frozen.

Windows fixed FileVersion/ProductVersion for this build is `0.5.0.100`.

## v0.5.0-alpha.3 — Unified Ranking & Classic UX

Alpha 3 replaces the temporary static-first append policy with unified ranking across User Command, Application, Folder and File results. Static catalog matches keep their mature SearchEngine score, while Everything File/Folder results receive a lightweight local filename/path match score before all candidates enter the same ranking pass.

Ranking remains intentionally conservative. Match quality dominates; result-kind and provider weights are small tie/near-tie adjustments. Explicit User Commands receive the strongest protection, then Applications, Folders and Files. Within application sources, Start Menu / Windows Apps / App Paths receive small stable preferences over PATH. An exact Everything filename can therefore outrank a weak/fuzzy application match without normally displacing an exact user command or exact application result.

The launcher now asks both static search and Everything for roughly three times the visible result count, then de-duplicates and ranks the candidate pool down to the existing Classic/Modern visible limits. A dynamic result pointing at the same target as an existing static command is suppressed in favor of the static command so application execution semantics and usage history stay authoritative.

Classic geometry remains frozen. File rows continue to use the existing primary/secondary columns as filename + parent path; Folder rows add only a trailing `\` to the primary text as a compact classic folder cue. The preview strip shows the direct full path for File/Folder results instead of the command prefix.

Numeric quick launch now naturally executes whichever unified result currently owns that number, including File and Folder rows. Single-result immediate execution is also dynamic-aware: while an Everything query is pending it is deferred; after the current generation settles successfully, unavailable or timed out, the final merged list is evaluated once. Hiding the launcher or manually executing a result cancels the deferred automatic action, preventing late IPC replies from causing a second launch.

Everything remains default-off in alpha.3 and Settings remains schemaVersion 2. Alpha testers can keep using `providers["everything.filesystem"] = true` in `data/settings.json`; the formal schemaVersion 3 + Search Sources UI remains a beta-phase task.

Windows fixed FileVersion/ProductVersion for this build is `0.5.0.3`.

## v0.5.0-alpha.2 — Dynamic File & Folder Results

Alpha 2 connects the native Everything IPC foundation to the launcher through a new unified `LauncherResult` model. Existing static Catalog search still returns immediately; Everything runs independently as a `DynamicQueryProvider`, and its File/Folder results are merged only after the asynchronous reply reaches the UI thread.

`everything.filesystem` is now the fixed dynamic provider ID. The provider remains **off by default** in this alpha and is deliberately not added to the default settings document or Settings UI yet. For alpha testing, explicitly add `"everything.filesystem": true` inside the existing `providers` object in `data/settings.json`, then restart ALTRun Next with normal Everything already running. Removing the key or setting it to `false` disables the dynamic provider again.

The initial merge policy is intentionally conservative: static User Command/Application results keep their existing order and Everything File/Folder results are appended into remaining result slots, with case-insensitive target de-duplication. Cross-kind/provider weighting is reserved for alpha.3. Classic geometry is unchanged; file rows reuse the existing two text columns as file name + parent path, with ellipsis for long paths.

Enter/double-click executes the unified result action. Files open through the Windows default application and folders open through Explorer/Shell. Everything results remain query-time ephemeral data: they are not written to `provider-cache.json` or `usage.json`. Dynamic replies never trigger single-result immediate execution in alpha.2; that mixed-result policy is intentionally deferred to alpha.3.

Settings remains schemaVersion 2 in alpha.2. The formal schemaVersion 3 migration and user-facing Search Sources controls remain scheduled for the Settings/diagnostics phase.

Windows fixed FileVersion/ProductVersion for this build is `0.5.0.2`.

## v0.5.0-alpha.1 — Everything IPC Foundation

This alpha begins the Everything file/folder-search architecture without exposing file results in the launcher yet. Existing User Commands, Start Menu, Windows Apps, App Paths and PATH search behavior remains unchanged.

The new foundation uses Everything's native 1.4-compatible Unicode Query2 IPC over `WM_COPYDATA`. ALTRun Next does not load or ship `Everything64.dll`, does not add a runtime DLL, does not start Everything automatically and does not build its own file index. The Windows transport runs on a dedicated worker thread with a hidden reply window, a 70 ms latest-query debounce, per-query reply tokens, generation-based stale-result discard, bounded `SendMessageTimeoutW` delivery and reply timeout handling.

Query2 binary encoding and LIST2 parsing live in a portable protocol layer. Core CI exercises UTF-16/Chinese payloads, malformed buffers, invalid offsets and unsupported request flags. Windows CI adds a real `WM_COPYDATA` fake-Everything server that validates availability fallback, asynchronous Unicode replies, rapid-typing coalescing, stale reply discard and reply timeout behavior without requiring Everything to be installed on GitHub-hosted runners.

v0.5.0-alpha.1 deliberately does **not** add `everything.filesystem` to the existing static Provider Registry, does not change `settings.json` to schemaVersion 3, does not write file results to provider-cache/usage data and does not change Classic launcher geometry. Dynamic provider/result integration begins in alpha.2.

Windows fixed FileVersion/ProductVersion for this build is `0.5.0.1`.

## v0.4.1 — Stable Classic Settings Parity

v0.4.1 promotes the RC1 feature set to stable without adding another feature, changing a data schema, changing a provider ID or modifying the frozen Classic launcher geometry.

The stable release adds the independent settings schemaVersion 2, show-on-startup, the optional auxiliary global hotkey with bare `Pause` support, opt-in `* / ?` wildcard matching, Classic numeric quick execution with selectable 1–9,0 or 0–9 order, and optional single-result immediate execution. Hotkey registration remains transactional and is revalidated after resume; IME composition and numeric-key auto-repeat retain the RC hardening behavior.

General Settings keeps the responsive stacked layout, vertical scrolling and high-DPI work-area clamping introduced during the v0.4.1 cycle. Existing Start Menu, Windows Apps, App Paths and PATH discovery behavior remains unchanged.

Release safety is unchanged from RC1: tag/VERSION preflight, Release-mode assertions, real `RegisterHotKey` smoke, Windows 10 API-baseline validation, x64/ARM64 package contracts, exact ZIP top-level allowlisting, packaged x64 runtime startup smoke and SHA256 self-verification all remain required. Stable Windows FileVersion/ProductVersion is `0.4.1.300`.

The packaged `DESKTOP_VALIDATION.md` remains the explicit real-desktop QA checklist; the Stable promotion does not rewrite or fabricate unchecked manual observations.

## v0.4.1-rc.1 — Release Candidate Stabilization

RC1 keeps the v0.4.1 feature set and data schemas frozen. The candidate contains no new launcher behavior; changes from beta.2 are limited to publication safety and final release-contract hardening.

Tag-triggered releases now start with a dedicated preflight job. The pushed tag must exactly equal `v` + the repository `VERSION` before Windows builds are allowed to start, preventing a manually mistyped or stale tag from publishing a package whose internal version differs from the release name. The same guard is exercised on every successful main build with both a matching and intentionally mismatched tag.

Both main publication and tag-triggered publication now self-verify the generated `SHA256SUMS.txt` before creating a GitHub Release. The portable package contract also enforces an exact top-level allowlist, so stale build output, a stray `data/` directory or any other unexpected root entry cannot silently enter x64/ARM64 ZIP assets.

All beta.2 validation-integrity gates remain active: Release-mode assertions, real `RegisterHotKey` runtime smoke, 100%/125%/150%/200% layout checks, IME-safe single-result gating, v0.4.0 migration/downgrade protection, Windows 10 API baseline, final ZIP runtime startup and frozen Classic geometry/schema contracts.

RC1 is therefore a release-candidate build for real desktop validation. The packaged `DESKTOP_VALIDATION.md` remains the manual Windows 10/11 sign-off matrix; unchecked manual items are not represented as automated passes.

## v0.4.1-beta.2 — Validation Integrity & Real Desktop Hardening

Beta 2 keeps the v0.4.1 feature set frozen and focuses on whether the release gates actually prove the behavior they claim to cover.

C++ regression targets now explicitly undefine `NDEBUG` in Release CI builds, and the new desktop-validation test contains a compile-time guard that fails if assertions are disabled. This closes a validation hole where assertion-based tests could otherwise compile and run successfully without evaluating their checks.

Classic numeric quick-launch ordering and single-result immediate-execution gating are now isolated in portable behavior helpers that are used by the real LauncherWindow and exercised directly by CI. The single-result matrix covers disabled execution, empty queries, active IME composition and non-single result sets.

General Settings geometry is now calculated through a portable layout helper used by the real SettingsWindow. Automated tests exercise 100%, 125%, 150% and 200% DPI, wide versus stacked cards, compact hotkey layout, scrolling and monitor work-area clamping. During a real `WM_DPICHANGED`, the suggested window rectangle is clamped to the destination monitor work area; minimum tracking dimensions are also capped by the available work area.

Windows smoke coverage now includes a real `RegisterHotKey` / `UnregisterHotKey` test for duplicate conflicts and re-registration, in addition to the existing codec tests. The same validation set runs on the current Windows runner and the Windows 10 API-baseline runner.

Config Core regression coverage now includes a representative v0.4.0 schema-1 settings document. The test verifies that known v0.4.0 preferences survive migration to schema 2, new v0.4.1 fields receive safe defaults, and a simulated v0.4.0-era reader treats the migrated file as newer without rewriting it.

No schemas, provider IDs, Classic launcher geometry or v0.4.1 user-facing features changed in beta.2.

## v0.4.1-beta.1 — Feature Freeze & Desktop Validation

Beta 1 freezes the v0.4.1 product feature set. No new launcher behavior, provider, schema field or Classic visual redesign is introduced in this release.

A new v0.4.1 release-contract gate now protects the frozen settings/commands/usage/provider-cache schema versions, the four stable Windows provider IDs, documented default settings and the established Classic launcher width/result geometry. The gate runs on main and on tag releases so accidental feature-surface drift is caught before publication.

Windows package validation now includes an x64 portable runtime startup smoke in addition to compile/test/package checks. The smoke extracts the final ZIP, starts the packaged executable with an isolated writable data directory and non-conflicting test hotkey, verifies the process remains healthy through startup, checks write-probe cleanup, then terminates the test instance.

The tag-triggered Release workflow is now aligned with the main publication path: Core Tests, v0.4.1 freeze verification, Windows provider/hotkey smoke tests, Windows 10 API-baseline compatibility, x64/ARM64 package contracts and the x64 packaged-runtime smoke must succeed before assets are published.

A formal DESKTOP_VALIDATION.md checklist is shipped in each ZIP. It covers the remaining interactive items that hosted CI cannot honestly validate: real Windows 10/11 desktops, 100%/125%/150%/200% DPI, primary/auxiliary hotkey conflict and sleep/resume behavior, IME composition, Classic numeric execution, provider refresh and v0.4.0 upgrade/downgrade safety.

Configuration remains frozen at settings schemaVersion 2, commands/usage schemaVersion 1 and provider-cache schemaVersion 2.

## v0.4.1-alpha.3 — Real-world Settings UX & Compatibility

Alpha 3 is a hardening release for the settings added in alpha.1/alpha.2. It does not add another v0.4.1 feature surface and it keeps the Classic launcher geometry frozen.

Global-hotkey handling now distinguishes between **retrying a missing registration** and **force revalidating after resume**. Opening Settings no longer tears down a working primary or auxiliary hotkey simply to refresh its status, while a binding that failed earlier is still retried. Resume events continue to force a real Windows re-registration and the Settings status is refreshed afterward.

Restoring defaults is now transactional across both hotkeys. ALTRun Next first releases the optional auxiliary binding before restoring the default primary `Alt + Space`, preventing an auxiliary binding from creating a conflict against ALTRun Next itself. Any later reset failure rolls the previous primary/auxiliary configuration back.

Single-result immediate execution is suppressed while an IME composition is in progress, preventing intermediate Chinese/Japanese/Korean composition text from launching a result before the composition is committed. Classic numeric quick launch also ignores auto-repeat keydown events, so holding a number cannot launch the same entry repeatedly when hide-after-launch is disabled.

The General Settings page is now work-area aware and vertically scrollable. At narrow client widths the behavior cards stack instead of overflowing, hotkey controls switch to a compact two-line layout, and the Settings window is clamped to the current monitor work area at high DPI. The pre-alpha.2 minimum height is restored because the General page can now scroll safely.

Windows CI now includes a dedicated hotkey-codec smoke test covering bare `Pause/Break`, modifier normalization and `MOD_NOREPEAT`. No configuration schema changes are introduced in alpha.3.

## v0.4.1-alpha.2 — Classic Settings UI

Alpha 2 exposes the behavior core introduced in alpha.1 through the existing Settings Shell. The Classic launcher itself remains visually frozen.

The General page now contains separate launcher-behavior and search-behavior cards. Users can configure show-on-startup, `*` / `?` wildcard matching, Classic numeric quick launch, the 1–9,0 versus 0–9 number order, and single-result immediate execution without editing `settings.json` manually.

Global-hotkey settings now expose both the existing primary hotkey and the optional auxiliary hotkey. The auxiliary binding supports bare `Pause` by default, can also use modifiers, reports its registration state independently, and keeps the previous working binding if Windows rejects a new one.

The Settings window is slightly taller to fit the additional controls while retaining the existing navigation and card design. No schema migration is required from alpha.1: settings remains schemaVersion 2, commands/usage remain schemaVersion 1 and provider-cache remains schemaVersion 2.

## v0.4.1-alpha.1 — Settings Schema & Classic Behavior Core

This alpha starts the Classic-settings parity phase before Everything integration. It adds behavior core and migration safety without changing the frozen Classic launcher geometry or adding the new controls to the Settings UI yet.

`settings.json` now has an independent schemaVersion 2 while `commands.json` and `usage.json` remain schemaVersion 1. Existing schema-1 settings are migrated atomically to schema 2. Downgrading to v0.4.0 therefore activates the existing newer-schema read-only guard instead of silently dropping v0.4.1 settings.

The new schema and runtime support an optional auxiliary global hotkey (bare Pause by default), show-on-startup, opt-in `*` / `?` glob matching, Classic numeric quick execution with either 1–9,0 or 0–9 ordering, and optional immediate execution when a non-empty query has exactly one result. All new behaviors default to off, preserving v0.4.0 interaction unless explicitly enabled in `data/settings.json`.

The auxiliary hotkey participates in the same transactional Windows registration lifecycle as the primary hotkey and is revalidated after resume. Numeric quick launch is intentionally limited to Classic mode so Modern Compact does not gain invisible number shortcuts.

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
