# ALTRun Next v0.4.1 Desktop Validation

This is the release-validation checklist for the frozen v0.4.1 feature set. Beta and RC builds may fix regressions or compatibility problems, but they must not introduce a new v0.4.1 feature surface.

For RC builds, the existence of a candidate tag means the automated release contract passed; it does **not** mean the manual items below were observed. Stable promotion requires explicit manual sign-off with no release-blocking defect.

Automated CI covers compilation, Config/Search tests, provider smoke tests, hotkey codec tests, real RegisterHotKey conflict/re-registration smoke, Classic numeric/single-result behavior logic, 100%/125%/150%/200% General-layout invariants, Windows API compatibility, package contents/version metadata and a packaged x64 startup smoke. Assertion-based tests are forced to remain active even in Release builds.

The checks below are the remaining **real interactive Windows desktop** validation items. Automated geometry/behavior tests reduce regression risk but do not replace observing the actual UI, IME, monitor transitions, providers and hotkey lifecycle on a real desktop.

## v0.8.0-alpha.5.44 Confirmation + Sound Feedback validation

- [ ] Launcher and Manager deletion show the same canonical name, Delete / Cancel buttons and target-file-retained text. Cancel has initial focus; Enter, Esc and X retain the shortcut. Explicit Delete removes only the shortcut; no success popup or sound.
- [ ] Long and empty-title shortcut names are readable (keyword fallback); nested modal focus returns correctly to Launcher/Manager, including provider refresh while the dialog is open.
- [ ] General shows 提示音 / Sound effects with one toggle and no preview button; rapid clicks, page changes and 100/125/150/175/200% DPI preserve state and scrolling. Placement controls remain reachable.
- [ ] Upgrade from schema 10 enables sound while preserving startup mode and other preferences. Disable, restart and confirm the choice persists. Reset defaults enables sound. A failed settings write leaves the old preference intact.
- [ ] Notification startup plays one startup cue; Show launcher startup plays one reveal cue; Silent startup plays none. Updating with suppressed startup presentation stays quiet.
- [ ] Hidden-to-visible Launcher plays one short cue; requesting an already-visible Launcher does not. Rapid hide/reveal and launch never queue an audio backlog.
- [ ] Successful ordinary, packaged and numeric/delayed launches each play one execution cue. Canceling UAC is silent; a genuine failure produces one failure cue and readable error dialog.
- [ ] Typing, Enter with no results, Tab, Escape, navigation, ordinary confirmation, saving/deleting successfully and background refresh produce no warning beep.
- [ ] Sound off stops any current cue immediately; launch/error dialogs and all subsequent application actions remain silent. Windows UAC and other applications retain their own sound policies.
- [ ] Uninstaller confirmations remain silent and retain default buttons, cancellation and foreground behavior. Validate on real Windows; no destructive uninstall is needed for sound testing.
- [ ] `s`/`st` query isolation, packaged entry integrity and Classic frozen visuals remain intact. Settings schema 11; Commands 2, Usage 2, Provider Cache 22; fixed version 0.8.0.214.

## Test matrix

Record each environment that was actually tested.

| Item | Required coverage |
| --- | --- |
| Windows | Windows 10 desktop and Windows 11 desktop |
| Architecture | x64; ARM64 when a physical ARM64 Windows device is available |
| Display scaling | 100%, 125%, 150%, 200% |
| Language | Simplified Chinese and English UI |
| Input | normal Latin input plus a Windows CJK IME |
| Upgrade path | v0.4.0 stable -> v0.4.1 Beta/RC |
| In-place path | v0.4.1-alpha.3 -> current Beta/RC |
| Downgrade safety | v0.4.1 -> v0.4.0 read-only protection -> v0.4.1 |

For every run record Windows edition/build, CPU architecture, display resolution/scaling, package architecture, package SHA-256 and whether the test used a clean data directory or an upgraded one.

## Startup and portable data

- [ ] Extract the ZIP to a normal user-writable folder and start ALTRunNext.exe.
- [ ] Fresh-install startup follows the configured Startup behavior; the current default shows a lightweight startup notification without revealing the Launcher.
- [ ] data/ is created/used beside the portable executable and remains writable.
- [ ] Starting from a read-only location shows the existing data-directory warning rather than corrupting data.
- [ ] Closing/restarting preserves settings, commands and usage.
- [ ] A corrupt primary settings/commands/usage file with a valid .bak self-heals and the Data page reports the recovery.
- [ ] A newer unsupported user-data schema remains read-only and is not overwritten.

## Hotkey lifecycle

- [ ] Default primary Alt + Space opens and hides the launcher repeatedly.
- [ ] Change the primary hotkey, save it, close Settings and verify it still works.
- [ ] Configure an occupied primary hotkey; save must fail and the previous working binding must remain active.
- [ ] Enable the auxiliary hotkey as bare Pause; both primary and auxiliary bindings work.
- [ ] Configure the auxiliary hotkey to collide with the primary; failure must preserve the previous auxiliary binding.
- [ ] Put Windows to sleep and resume; enabled hotkeys work after resume.
- [ ] Open Settings repeatedly; a working hotkey remains registered and does not intermittently stop working.
- [ ] Restore defaults after configuring the auxiliary hotkey to Alt + Space while the primary uses another binding; defaults restore without a self-conflict.

## Classic search behavior

- [ ] With wildcard matching disabled, * and ? do not silently switch normal search semantics.
- [ ] Enable wildcard matching and validate representative * and ? queries.
- [ ] Enable numeric quick launch with 1-9,0 order; top ten labels and execution targets match.
- [ ] Switch to 0-9 order; labels and execution targets match.
- [ ] Main keyboard number keys and numpad number keys behave consistently.
- [ ] Modifier + number does not trigger numeric quick launch.
- [ ] Holding a number key launches at most once per physical press.
- [ ] Numeric quick launch remains inactive in Modern Compact.
- [ ] Enable single-result immediate execution and verify a committed non-empty query launches the sole result.
- [ ] Empty query with one result does not immediately launch.

## IME

Use a Windows Chinese/Japanese/Korean IME with single-result immediate execution enabled.

- [ ] Intermediate composition text may refresh search results but does not launch a result.
- [ ] A result may launch only after the IME composition is committed and the committed query leaves exactly one result.
- [ ] Canceling an IME composition does not leave the launcher in a stale composition state.
- [ ] Hide and re-open the launcher after an interrupted composition; normal immediate-execution behavior resumes.

## General Settings layout

Repeat the General page checks at 100%, 125%, 150% and 200% display scaling.

- [ ] Settings opens completely inside the current monitor work area.
- [ ] Launcher/Search behavior cards remain side-by-side when enough width is available.
- [ ] At narrow width the two cards stack without clipping controls.
- [ ] Hotkey controls wrap into the compact layout when needed and remain usable.
- [ ] The vertical scrollbar appears only when content does not fit.
- [ ] Mouse-wheel and scrollbar movement expose all General controls.
- [ ] Switching to another Settings page removes the General-only scrollbar.
- [ ] Returning to General starts at a predictable top position.
- [ ] Chinese and English labels do not overlap critical controls.

## Provider and background refresh regression

- [ ] Start Menu, Windows Apps, App Paths and PATH each populate when enabled.
- [ ] Disabling a provider removes its entries from active search without destroying its cache.
- [ ] Re-enabling a provider restores cached entries immediately and schedules refresh.
- [ ] Rebuild program index remains non-blocking.
- [ ] Installing/removing a representative application or shortcut causes the corresponding provider to refresh without requiring restart.
- [ ] Duplicate automatic entries retain the established provider precedence and user shortcuts remain authoritative.

## Upgrade / downgrade

### v0.4.0 -> v0.4.1

- [ ] Existing commands and usage survive.
- [ ] settings schema migrates to version 2.
- [ ] New v0.4.1 behavior options keep their documented safe defaults.
- [ ] Existing provider enable/disable selections survive.

### v0.4.1-alpha.3 -> Beta/RC

- [ ] No schema migration is performed.
- [ ] Primary/auxiliary hotkeys and Classic behavior settings are preserved.
- [ ] General page opens without clipped controls at the previously used DPI.

### v0.4.1 -> v0.4.0 -> v0.4.1

- [ ] v0.4.0 detects settings schema 2 as newer and does not rewrite it.
- [ ] The original v0.4.1 settings file remains byte-for-byte unchanged during the downgrade test.
- [ ] Reopening with v0.4.1 restores the same settings and exits read-only compatibility mode.

## Classic UI freeze regression

This is a regression check, not a request for visual redesign.

- [ ] Classic launcher remains 420 logical px wide with 10 visible results and the established compact row geometry.
- [ ] Title bar, right frame, fixed result columns, selection rendering and command strip have no unintended geometry regression.
- [ ] Modern Compact changes do not leak into Classic rendering.

## v0.8.0-alpha.4.8 Classic DPI audit

Repeat the Classic launcher checks at all four supported validation scales. The physical geometry below is generated from the frozen logical Classic contract using the same integer rounding as the runtime.

| Scale | DPI | Client | Input | Result list | Command | Row | Dividers |
| --- | ---: | --- | --- | --- | --- | ---: | --- |
| 100% | 96 | 420×250 | 8,30 / 404×22 | 8,56 / 404×164 | 8,226 / 404×16 | 16 | 23 / 230 |
| 125% | 120 | 525×313 | 10,38 / 505×28 | 10,70 / 505×205 | 10,283 / 505×20 | 20 | 29 / 288 |
| 150% | 144 | 630×375 | 12,45 / 606×33 | 12,84 / 606×246 | 12,339 / 606×24 | 24 | 35 / 345 |
| 200% | 192 | 840×500 | 16,60 / 808×44 | 16,112 / 808×328 | 16,452 / 808×32 | 32 | 46 / 460 |

- [ ] Background crop remains visually identical to the 100% Classic baseline; no black strip, color shift or unexpected crop appears.
- [ ] Logo remains sharp enough and correctly positioned; the 25px source glyph scales to 25 / 31 / 38 / 50 physical px at 100% / 125% / 150% / 200%.
- [ ] Close X remains visually centered, is not unexpectedly clipped, and its click target follows the scaled title geometry.
- [ ] SimSun -16 / -13 logical-height fonts remain vertically centered and readable in title, input, results and Command.
- [ ] Ten result rows remain fully visible; row height matches 16 / 20 / 24 / 32 physical px.
- [ ] The two result dividers stay aligned at the expected x positions and remain exactly one physical pixel thick.
- [ ] Command remains a single 16-logical-px row and DT_PATH_ELLIPSIS preserves the beginning and final filename for long paths.
- [ ] Move the visible launcher between monitors with different scaling and verify Per-Monitor V2 relayout occurs without stale size, wrong font scale, black frame or misplaced hit target.
- [ ] Compare 125% and 150% bitmap/glyph sharpness against 100% before changing stretch mode or introducing DPI-bucket assets; visual evidence is required before such a rendering change.

## v0.8.0-alpha.4.9 Classic HiDPI glyph validation

Repeat the title-bar check at the five real-Windows scaling levels already used for the alpha.4.8 audit.

| Scale | DPI | Target glyph | Selected asset | Expected path |
| --- | ---: | ---: | ---: | --- |
| 100% | 96 | 25px | 25px original | TransparentBlt / original pixels |
| 125% | 120 | 31px | 31px HiDPI | AlphaBlend / exact size |
| 150% | 144 | 38px | 38px HiDPI | AlphaBlend / exact size |
| 175% | 168 | 44px | 44px HiDPI | AlphaBlend / exact size |
| 200% | 192 | 50px | 50px HiDPI | AlphaBlend / exact size |

- [ ] At 100%, Logo and X are visually identical to alpha.4.8; no color, transparency, position or sharpness change is acceptable.
- [ ] At 125/150/175/200%, Logo and X retain the same Classic shapes/colors but no longer show the large square-pixel enlargement visible when the 25px source was stretched.
- [ ] Semi-transparent HiDPI edges blend cleanly with the gray title bitmap: no black halo, white fringe, rectangular background or missing pixels.
- [ ] Logo and X remain centered in the same alpha.4.8 geometry and the X hit target is unchanged.
- [ ] Move the launcher across monitors with different scaling and confirm the selected tier changes with Per-Monitor V2 without stale glyph size or redraw artifacts.
- [ ] Test one non-standard scale if available; it may resample from the next larger tier, but must never fall back to enlarging a smaller tier while a larger tier exists.

## v0.8.0-alpha.5.1 Settings behavior validation

- [ ] General shows exactly three groups: Windows 与启动, 搜索与执行, 窗口位置.
- [ ] Windows 与启动 contains 开机启动, 启动行为, 显示系统托盘图标, 添加到“发送到”菜单 — no helper text below SendTo.
- [ ] 启动行为 offers 静默启动 / 显示启动通知 / 显示启动器 and persists across restart.
- [ ] 显示启动通知 does not reveal Launcher; the notification shows the current primary activation hotkey. With tray hidden, its temporary icon disappears after the balloon closes.
- [ ] Every normal Launcher reveal clears the previous query; successful execution hides; focus loss hides except while a modal/context action is active.
- [ ] * / ? wildcard queries work without a Settings switch. Numeric quick launch, when enabled, uses fixed 1–9,0 numbering.
- [ ] Enabling 添加到“发送到”菜单 creates ALTRun Next in Windows Send To. Sending a file/folder opens the confirmed New Shortcut editor prefilled with that target.
- [ ] Send To works while ALTRun Next is already running and does not show the duplicate-instance warning.
- [ ] Disabling SendTo removes the shell entry. Reset settings also removes it.
- [ ] Hotkeys include 打开快捷项管理 (default Alt+S) and 退出 ALTRun Next (disabled by default).
- [ ] With tray hidden, Alt+S still reaches Shortcut Manager and Alt+F4 or an enabled Exit action can terminate the process.
- [ ] Launcher monitor plus Launcher / Settings / Shortcut Manager placement preferences remain unchanged.
- [ ] Classic appearance at 100/125/150/175/200% is pixel-identical to alpha.4.9 outside Settings.

## v0.8.0-alpha.5.2 Settings UX validation

- [ ] Startup notification source/title reads ALTRun Next; body is “已在后台启动” plus “按 <current activation hotkey> 呼出” with no duplicated product name.
- [ ] Change the primary activation hotkey and restart; startup notification shows the new binding.
- [ ] Settings dropdowns for Startup behavior, Launcher monitor, Launcher/Settings/Shortcut Manager position, Launcher style and Interface language share the same Next-themed closed surface, arrow area and dropdown-row styling.
- [ ] Dropdown text remains vertically centered at 100/125/150/175/200% scaling and no native-looking square border leaks through.
- [ ] Hotkeys page shows a vertical scrollbar when its action cards exceed the viewport; mouse wheel and scrollbar thumb both expose every action.
- [ ] “恢复全部默认快捷键” remains fixed in the bottom action area while the hotkey cards scroll behind the content viewport and never overlap it.
- [ ] Leaving and returning to Hotkeys starts at the top and does not leave clipped/hidden controls from the prior scroll position.
- [ ] Default Alt+S opens Shortcut Manager while Launcher is hidden.
- [ ] With system tray icon disabled, Alt+S still opens Shortcut Manager.
- [ ] Repeated Alt+S activates/restores the existing Shortcut Manager rather than creating duplicate windows.
- [ ] Rebind Open Shortcut Manager to another free modified chord; the new chord works globally and Alt+S stops working.
- [ ] Attempt an occupied global chord; save fails and the previous working Shortcut Manager binding remains registered.
- [ ] Restore all hotkeys restores Alt+S and it works immediately without restart.
- [ ] Exit ALTRun Next and confirm the Shortcut Manager global chord is released for other applications.
- [ ] Classic launcher appearance at 100/125/150/175/200% remains identical to alpha.4.9.

## v0.8.0-alpha.5.3 Settings polish validation

- [ ] All seven Settings dropdowns use a continuous rounded closed surface with no vertical divider or separate arrow cell.
- [ ] The chevron is clearly visible but remains visually lighter than the selected text; hover is subtle and focus/dropdown-open uses the existing accent border.
- [ ] Startup behavior, each Window placement selector, Launcher style and Interface language are content-sized rather than padded to one fixed long width.
- [ ] Switch between Simplified Chinese and English and verify the preferred widths recompute for the localized items without clipping the longest option.
- [ ] Repeat ComboBox text/arrow alignment at 100/125/150/175/200% scaling; closed text and dropdown rows remain vertically centered after DPI transitions.
- [ ] Open Hotkeys with no capture/error status active: no empty status strip or pale rectangle appears below the final row of either card.
- [ ] Scroll both Hotkeys cards through the viewport; business-hidden status/reset controls never reappear merely because their HWND intersects the viewport.
- [ ] Start hotkey capture so a status row is legitimately visible, scroll it partly through the viewport, then cancel capture; clipping is correct and the status disappears completely.
- [ ] Modify a hotkey so its per-row Reset action appears, scroll away/back, then restore default; the Reset action disappears and is not resurrected by scrolling.
- [ ] “恢复全部默认快捷键” stays fixed in the bottom action area and the alpha.5.2 Alt+S global Shortcut Manager behavior remains unchanged.
- [ ] Classic launcher appearance at 100/125/150/175/200% remains identical to alpha.4.9.
## v0.8.0-alpha.5.4 shared ComboBox validation

- [ ] Settings dropdowns remain visually identical to the approved alpha.5.3 treatment: continuous rounded surface, clear chevron, subtle hover and accent focus/open state.
- [ ] New/Edit Shortcut shows the same Next-themed closed surface for Target type and Runtime input; no old square Windows arrow button remains.
- [ ] Open both Shortcut Editor dropdowns and verify list row height, text padding and selection background match Settings.
- [ ] Target type and Runtime input remain aligned to one compact value-column width and do not crowd their hint text in Simplified Chinese or English.
- [ ] Clicking editor blank/static space still dismisses ComboBox focus as before; Enter on a closed dropdown still follows the existing Save workflow.
- [ ] Target type Auto detect/Application/URL/Folder/Command line selection behavior is unchanged.
- [ ] Runtime input None/Pass through/URL encode behavior, conditional Test input row and Advanced layout remain unchanged.
- [ ] Repeat Settings and Shortcut Editor ComboBox checks at 100/125/150/175/200% scaling; text, chevron and list rows remain vertically centered.
- [ ] Hotkeys page still has no empty status strip and Reset all hotkeys remains fixed.
- [ ] Classic launcher appearance at 100/125/150/175/200% remains identical to alpha.4.9.
## v0.8.0-alpha.5.5 shared ListView validation

- [ ] Shortcut Manager list no longer has the old dark hard table border; the surrounding frame is light and belongs to the same visual system as Next buttons/ComboBoxes.
- [ ] Shortcut Manager Header is light, semibold and vertically comfortable; column dividers are subtle rather than dominant.
- [ ] Shortcut Manager rows are about 30 logical pixels high, text is vertically centered, body columns have no vertical grid lines, and only light horizontal separators remain.
- [ ] Hovering an unselected Shortcut Manager row gives a very subtle background response; selecting a row uses the shared light-blue selection and does not show a black dotted focus rectangle.
- [ ] Drag Keywords/Name/Type columns repeatedly left/right and verify minimum widths, no ghosting, no bottom flashing bar, and elastic Target behavior remain unchanged.
- [ ] Reopen Shortcut Manager after custom column widths and verify the existing width/position behavior remains unchanged.
- [ ] Target text still ellipsizes correctly; double-click editing, right-click actions, Delete, Ctrl+N, Ctrl+F and Ctrl+Enter still work.
- [ ] Path Conversion Header and normal preview rows visually match Shortcut Manager: same header treatment, row height, padding, hover/selection and horizontal separators.
- [ ] Path Conversion `KOOK — KOOK`-style group rows remain semibold/light-section rows and cannot become selected.
- [ ] Path Conversion high-DPI checkbox remains sharp and toggles only the intended field; converted/current path text and status still ellipsize correctly.
- [ ] Path Conversion column dragging and the fixed elastic Status column behavior remain unchanged; Rescan and Apply selected still work.
- [ ] Repeat both lists at 100/125/150/175/200% scaling and verify Header/row text, checkbox alignment and frame remain crisp.
- [ ] Settings/Shortcut Editor ComboBoxes remain visually identical to alpha.5.4 and Hotkeys still have no empty status strip.
- [ ] Classic launcher appearance at 100/125/150/175/200% remains identical to alpha.4.9.
## v0.8.0-alpha.5.6 Next Table Surface validation

- [ ] Shortcut Manager Header no longer looks like a native Windows table Header: no beveled/button-like column cells and no permanent vertical separators.
- [ ] Header background belongs to the same light Next card family as Settings/ComboBox surfaces; labels are semibold and vertically centered in a ~34 logical-pixel Header.
- [ ] A single soft horizontal divider separates Header from body. Normal body rows keep only very light horizontal separators and no vertical grid.
- [ ] Move the pointer slowly across Header column boundaries: normal areas stay visually clean, and a subtle divider hint appears only directly over a resizable boundary while the native resize cursor remains available.
- [ ] Drag Keywords/Name/Type repeatedly left/right in Shortcut Manager. Minimum widths, deferred drag behavior, no ghosting, no flashing bottom bar and elastic Target commitment remain unchanged.
- [ ] Target remains non-resizable/elastic and still absorbs the remaining width exactly after window resize and column drag.
- [ ] Shortcut Manager text padding/ellipsis remains correct in all four columns; selection/hover still use the shared light Next treatment without a dotted focus rectangle.
- [ ] Path Conversion uses the exact same Table Header surface and row spacing as Shortcut Manager.
- [ ] Path Conversion `KOOK — KOOK`-style group row is slightly lighter than alpha.5.5 and reads as a section band, not a second Header; it still cannot be selected.
- [ ] Path Conversion checkbox remains sharp and centered; Current/Converted/Status text, rescan/apply and conversion behavior are unchanged.
- [ ] Path Conversion first three columns remain resizable within their existing minimums and Status remains elastic/non-resizable.
- [ ] Repeat both Table Surfaces at 100/125/150/175/200% scaling; Header height, text centering, resize hit zones, body padding and checkbox alignment remain crisp.
- [ ] Settings/Shortcut Editor ComboBoxes remain visually identical to alpha.5.4 and Hotkeys still have no empty status strip.
- [ ] Classic launcher appearance at 100/125/150/175/200% remains identical to alpha.4.9.
## v0.8.0-alpha.5.7 Table interaction validation

- [ ] In Shortcut Manager, move slowly across the first three Header boundaries: the resize cursor appears within an easy-to-hit ~8 logical-pixel zone and no guessing is required.
- [ ] Hovering a resizable boundary shows a short, clearer divider hint only while the pointer is in the hit zone; no permanent vertical grid lines return.
- [ ] Drag each Shortcut Manager divider rapidly left/right. The table body does not continuously reflow during drag; only the shared vertical preview guide moves.
- [ ] Releasing the mouse commits the dragged width once, recalculates elastic Target once, and leaves no guide, ghosting, flashing bottom bar or stale divider.
- [ ] Shortcut Manager minimum widths and Target elastic/non-resizable behavior remain unchanged; first-open widths are approximately 110 / 180 / 110 logical pixels before Target.
- [ ] Repeat the same interaction in Path Conversion for Field / Current path / Converted path. Dragging is deferred and Status remains elastic/non-resizable.
- [ ] Path Conversion first-open intent is 110 / 360 / 380 logical pixels before Status, shrinking only as needed to respect the existing minimum window and Status minimum.
- [ ] Double-click/normal Header interaction does not make the last elastic column resizable.
- [ ] Existing Shortcut Manager column persistence behavior, filtering, right-click menu, double-click editing, Ctrl+N/Ctrl+F/Ctrl+Enter/Delete remain unchanged.
- [ ] Path Conversion checkbox, grouped rows, Rescan, Apply selected and conversion results remain unchanged.
- [ ] Repeat hover + drag at 100/125/150/175/200% scaling; hit zone, cursor and guide remain crisp and aligned with the committed divider.
- [ ] Alpha.5.6 Table visual surface, alpha.5.4 ComboBoxes, Hotkeys and Classic launcher visuals remain unchanged.
## v0.8.0-alpha.5.8 resize-guide artifact validation

- [ ] Shortcut Manager: drag each resizable divider slowly and rapidly left/right; exactly one preview line is visible at any time.
- [ ] No previously visited divider positions remain visible after the guide moves; rapid back-and-forth dragging never builds a blue block or vertical trail.
- [ ] Releasing the mouse removes the preview guide completely before/with the final one-shot column commit.
- [ ] Move the pointer away immediately after release: no residual vertical line remains in Header, rows or empty ListView body.
- [ ] Cancel/interrupt a drag by moving outside the window or changing capture; the overlay disappears with no stale guide.
- [ ] During drag, actual row text and column geometry remain stationary; only the overlay guide moves. Final widths update once on release.
- [ ] Native Header tracking feedback does not appear alongside the Next overlay; there is only one visible drag indicator.
- [ ] Repeat all checks in Path Conversion for Field / Current path / Converted path; Status remains elastic/non-resizable.
- [ ] Expanded alpha.5.7 ±4 logical-pixel hit zone and resize cursor remain easy to discover.
- [ ] Repeat at 100/125/150/175/200% scaling; guide width and alignment remain crisp with no trail.
- [ ] Alpha.5.6 Table visuals, alpha.5.4 ComboBoxes, Hotkeys, Checkbox rendering and Classic launcher remain unchanged.
## v0.8.0-alpha.5.9 unified resize-controller validation

- [ ] Shortcut Manager: slowly cross all three resizable Header boundaries; the existing ±4 logical-pixel hit zone and `IDC_SIZEWE` cursor remain easy to discover.
- [ ] Drag each divider rapidly left/right for several seconds; exactly one preview guide moves and actual column geometry stays stationary until release.
- [ ] Release normally over the Header, body, outside the table and outside the window; a successful mouse-up commits exactly once and leaves no residual line.
- [ ] Interrupt a drag with Alt+Tab, window deactivation or another capture owner; the guide disappears, the pre-drag widths remain unchanged, and no later notification resurrects the guide.
- [ ] Repeated fast drag/release cycles never show native Header tracking marks, black divider fragments, blue trails, blue blocks or bottom-bar flashes.
- [ ] Drag a column to both minimum and maximum limits; the preview and final divider remain aligned, Target/Status retains its minimum, and no horizontal scrollbar flashes.
- [ ] Verify grow ordering and shrink ordering produce no transient overflow.
- [ ] Path Conversion repeats the same behavior for Field / Current path / Converted path; Status remains elastic and directly non-resizable.
- [ ] Divider double-click is inert in both tables and never re-enters the native Header resize engine.
- [ ] Resize each window after manually resizing columns; user-adjusted first-three widths remain stable while only the elastic final column absorbs available width changes.
- [ ] Repeat at 100/125/150/175/200% scaling; Header height, hit zone, guide alignment, row padding and Path Conversion checkbox remain crisp.
- [ ] Shortcut Manager filtering, New/Edit/Delete/Test/Path Conversion, double-click editing, right-click actions, Ctrl+N/Ctrl+F/Ctrl+Enter/Delete remain unchanged.
- [ ] Path Conversion grouped rows, Rescan, Apply selected and conversion results remain unchanged.
- [ ] Alpha.5.6 Table visuals, alpha.5.4 ComboBoxes, Hotkeys and Classic launcher visuals remain unchanged.

## v0.8.0-alpha.5.10 composited-guide validation

- [ ] Path Conversion: repeatedly drag the Field / Current path divider left and right directly across a visible group title such as “KOOK”; the title remains byte-for-byte visually intact with no duplicated or clipped character fragments.
- [ ] Drag the same divider a long distance left and then back right across the empty lower ListView body; no old guide position remains as a vertical line.
- [ ] Rapid back-and-forth dragging for several seconds shows exactly one two-logical-pixel guide and never builds a blue block, trail or black/native tracker fragment.
- [ ] Releasing over Header, body or outside the table removes the layered guide immediately and commits the width exactly once.
- [ ] Alt+Tab / capture loss cancels the drag, hides the popup guide and preserves pre-drag widths.
- [ ] The guide never steals activation/focus, never appears in Alt+Tab/taskbar, and cannot intercept mouse input.
- [ ] Repeat the same artifact checks in Shortcut Manager across normal row text and the empty table body.
- [ ] Minimum/maximum drag limits and elastic Target/Status behavior remain identical to alpha.5.9 with no horizontal-scrollbar or bottom-bar flash.
- [ ] Repeat at 100/125/150/175/200% scaling; guide thickness/alignment remains crisp and the owner-drawn Header/rows remain unchanged.
- [ ] Alpha.5.9 unified ownership, alpha.5.6 Table visuals, Path Conversion checkbox/group bands, alpha.5.4 ComboBoxes, Hotkeys and Classic launcher visuals remain unchanged.

## v0.8.0-alpha.5.11 in-Header preview validation

- [ ] Path Conversion: repeatedly drag the Field / Current path divider across the horizontal position occupied by “KOOK”; “KOOK” never duplicates, clips, smears or briefly shows stale fragments.
- [ ] During drag, the only moving feedback is a two-logical-pixel line inside the Header; no preview line enters group rows, data rows or the empty ListView body.
- [ ] Long left/right drags over the empty body leave zero vertical traces, including transient traces that disappear a moment later.
- [ ] Rapid back-and-forth dragging for several seconds keeps Header text/background clean with exactly one preview line.
- [ ] Release over Header/body/outside the table commits exactly once; Alt+Tab/capture loss cancels and restores the pre-drag widths.
- [ ] Shortcut Manager repeats the same behavior: body rows and empty area remain completely untouched while Header preview tracks the pointer.
- [ ] Minimum/maximum limits and elastic Target/Status behavior remain identical to alpha.5.9/5.10.
- [ ] Divider hover hint remains visible when idle and disappears while the active Header preview is shown.
- [ ] Repeat at 100/125/150/175/200% scaling; Header preview stays aligned and crisp.
- [ ] Alpha.5.9 unified ownership, Path Conversion group/checkbox rendering, Table visuals, ComboBoxes, Hotkeys and Classic visuals remain unchanged.

## v0.8.0-alpha.5.12 atomic column-commit validation

- [ ] Path Conversion: drag Field / Current path repeatedly across the horizontal position occupied by “kook — KOOK”, release, and do not move the mouse; the group title is immediately correct with no stale fragment waiting for hover repaint.
- [ ] After every release, leave the pointer stationary outside the ListView body for at least several seconds; no old vertical line remains in the empty body.
- [ ] Perform 20+ alternating large left/right commits; the empty body stays uniformly clean and never accumulates historical one-pixel column/frame traces.
- [ ] During drag, alpha.5.11 Header-only preview remains the only moving feedback; ListView rows/body stay static.
- [ ] On mouse release, dragged and elastic widths appear together as one visual state—no intermediate column arrangement is visible.
- [ ] Shortcut Manager repeats the same test across normal rows and its empty body with zero stale text or vertical traces.
- [ ] Mouse entering/leaving rows after a commit must not visibly “repair” anything; hover should change only hover styling.
- [ ] Minimum/maximum clamp, elastic Target/Status behavior and grow/shrink commit ordering remain unchanged.
- [ ] Alt+Tab/capture loss still cancels rather than committing.
- [ ] Repeat at 100/125/150/175/200% scaling.
- [ ] Path Conversion group/checkbox rendering, Table visuals, ComboBoxes, Hotkeys and Classic visuals remain unchanged.

## v0.8.0-alpha.5.13 Classic input-arbitration validation

- [ ] Classic: Tab cycles 1 → 2 → … → 9 → 0 → 1 continuously; Shift+Tab cycles in reverse.
- [ ] Classic: Down wraps 0 → 1 and Up wraps 1 → 0. Modern Compact remains bounded at its first/last result.
- [ ] With Numeric Quick Launch enabled, type `v2ray`, `7zip`, `1password`, `cs2`, `h264` and `python3` at natural typing speed; digits remain query text and no result launches.
- [ ] Type `v`, pause for more than the typing window, then press `2` when a v2* command is present in the in-memory command cache; query becomes `v2` rather than launching row 2.
- [ ] For a query with no strong digit continuation, pause, press an existing result number and keep hands still; the originally numbered result launches after the short grace without the digit ever appearing in the Edit control.
- [ ] In the previous case, press another character immediately after the digit; the digit is committed as text before the new character and no numbered result launches.
- [ ] During a deferred text commit, single-result immediate execution does not fire between the pending digit and the following character.
- [ ] Ctrl+1…0 and Alt+1…0 immediately execute the corresponding numbered result even when a text continuation exists.
- [ ] Shift+digit produces its normal symbol/text path; IME composition digits remain text; Win+digit is never consumed by Numeric Quick Launch.
- [ ] With fewer than N results, bare digit N remains query text rather than disappearing.
- [ ] Hold a numeric Quick Launch key; only one pending/immediate launch is possible and key-repeat never creates repeated launches or query digits.
- [ ] Enable Everything and force asynchronous result reordering during the 90ms grace; execution still targets the result snapshot that occupied the numbered row at key-down.
- [ ] With Numeric Quick Launch disabled, all bare numeric input behaves exactly as normal Edit text.
- [ ] Classic geometry, fonts, result numbering, Search/Provider ranking, Everything, Settings, Table Surface and Modern Compact visuals remain unchanged.

## v0.8.0-alpha.5.14 Search relevance hygiene validation

- [ ] Disable Everything and PATH, type `cs`: `cs2` or genuine CS-prefix/initial results stay relevant; SOLIDWORKS/iSCSI/ODBC-style path or subsequence noise does not appear.
- [ ] Type `df`: Windows Defender Firewall does not appear solely because `df` is a fuzzy suffix of derived initials `wdf`.
- [ ] Type `de`: real `De...` applications may match; Start Menu documentation/website shortcuts are absent and Developer PowerShell/Command Prompt remain discoverable only as lower-priority developer auxiliaries.
- [ ] Verify a genuine initials query such as `wt` → Windows Terminal and `vsc` → Visual Studio Code still works.
- [ ] Verify pinyin regressions `wx`, `wyy`, `jsq`, `chongqing`, `wangyy` and spaced Hybrid Pinyin `wei x` still resolve correctly with pinyin enabled.
- [ ] Verify a two-character subsequence such as `cd` does not guess `Code`, while the bounded three-character fuzzy case `cde` still finds Code.
- [ ] Verify an explicit path-style query such as `cloudmusic.exe`, a backslash-containing path, or an explicit wildcard can still match command targets.
- [ ] Rebuild the program index once after upgrade; Provider Cache schema 3 replaces older Start Menu cache data, and `.url`/documentation/help/website/release-note entries do not return.
- [ ] Windows administrative/developer/maintenance shortcuts remain launchable when directly searched; primary Start Menu applications retain normal ranking.
- [ ] Provider duplicate canonicalization remains Start Menu → Packaged App → App Paths → PATH; previously fixed TeamSpeak/user-shortcut dedupe does not regress.
- [ ] Classic geometry, fonts, table surface, numeric Quick Launch, Everything IPC behavior and Modern Compact visuals remain unchanged.

## v0.8.0-alpha.5.15 Launch surface & unified relevance validation

- [ ] With PATH and Everything disabled, type a single `h`: normal primary apps with a genuine literal/pinyin/initial match may appear, but recovery/admin tools, Get Help/support entries and helper/host/native-messaging/internal components must not fill the top 10.
- [ ] Specifically verify entries resembling `idmhelp`, `gpuviewhelp`, `grabberhelp`, `gethelp`, `PlatformExperienceShell` and `BrowserNativeMessaging` are absent for casual `h`; typing a sufficiently explicit strong prefix may still expose auxiliary entries when intentionally requested.
- [ ] `华硕管家` and `画图` remain valid `h` pinyin-primary candidates; `恢复驱动器` / system utilities do not compete on a single-character query and become discoverable once the query is sufficiently specific.
- [ ] Verify primary application ranking is stable for `chrome`, `code`, `cs2`, `wt`, `vsc`, plus pinyin regressions `wx`, `wyy`, `jsq`, `chongqing`, `wangyy`, `wei x`.
- [ ] Verify a PATH CLI such as `git`, `python`, `adb` or `ffmpeg` does not appear from a one-character guess; with PATH enabled, an explicit 2–3 character prefix finds it normally.
- [ ] On a clean settings profile, PATH Provider is off by default. Upgrade an existing profile that already stores windows.path=true/false and confirm the stored choice is preserved.
- [ ] Enable Everything: an ordinary one-character query does not issue dynamic filesystem results or cause asynchronous list reordering. Two-character dynamic results require strong exact/prefix/boundary intent.
- [ ] Everything explicit syntax such as `ext:exe` and explicit path/wildcard queries still work; multi-token ordinary queries remain strict.
- [ ] Confirm static Search and Everything agree on short-query fuzzy behavior; there must be no case where the static provider rejects a weak match but Everything reintroduces it through an independent subsequence floor.
- [ ] Provider Cache schema 4 rebuilds once after upgrade and preserves LaunchSurfaceClass on subsequent loads.
- [ ] User shortcuts/pinned commands remain authoritative. Usage changes ordering only among comparable relevance tiers and cannot lift an auxiliary fuzzy match above a primary prefix.
- [ ] Classic visuals, geometry, numeric Quick Launch arbitration, table surface and Modern Compact visuals remain unchanged.

## v0.8.0-alpha.5.16 Launch candidate admission validation

- [ ] Refresh/rebuild providers after upgrade. Provider Cache schema 5 must rebuild once and schema-4 Start Menu/AppsFolder/App Paths entries must not survive unchanged.
- [ ] Search for `最新版本里有哪些新功能`: the WinRAR What's New shortcut from the Start Menu must not appear. `控制台 RAR 中文手册`, WinRAR help/manual/documentation shortcuts must likewise stay out of the normal launcher index.
- [ ] Search `winrar`: the real WinRAR application remains available and launches normally through its original Start Menu shortcut.
- [ ] Search `f` / `fangwen`: AppsFolder vendor website entries such as `访问 Java.com` with an `https://java.com/` target must not appear.
- [ ] Confirm normal packaged applications (Calculator, Paint, Settings/other installed MSIX apps as available) remain discoverable; HTTP/HTTPS/FTP/mail Shell entries are not treated as applications.
- [ ] Confirm `Uninstall`, `Repair`, updater/update-helper, `*Help`, `*Helper`, `*Host`, `*Broker`, NativeMessaging, crash-handler and comparable internal/maintenance candidates do not enter Start Menu, AppsFolder, App Paths or PATH result sets merely because they are launchable files.
- [ ] Verify Windows administrative tools and meaningful developer tools remain searchable with their existing stricter LaunchSurface admission thresholds.
- [ ] With PATH enabled, `git`, `python`, `adb`, `ffmpeg` and comparable CLI tools remain available on explicit queries; helper/maintenance binaries are absent.
- [ ] Right-click/launch several normal Start Menu shortcuts with arguments or custom working directories. Execution must still use the original `.lnk` and preserve Windows shortcut semantics.
- [ ] Recheck search relevance regressions `h`, `cs`, `de`, `df`, `wt`, `vsc`, `wx`, `wyy`, `jsq`, `wei x`, `v2ray`, `7zip`, `1password`, `cs2`.
- [ ] Everything behavior is unchanged from alpha.5.15: ordinary one-character queries do not start dynamic filesystem IPC and explicit syntax/path queries still work.
- [ ] Classic visuals/geometry, table resize, numeric Quick Launch arbitration and Modern Compact visuals remain unchanged.

## v0.8.0-alpha.5.17 Executable admission recovery validation

- [ ] Upgrade from alpha.5.16 and allow Provider Cache schema 6 to rebuild once.
- [ ] Search `teamsp`: both `TeamSpeak 3 Client` and the user-level `TeamSpeak` / TeamSpeak 6 shortcut must be present when both are installed.
- [ ] Confirm the TeamSpeak 6 result launches the original Start Menu shortcut targeting `C:\\Users\\Asp\\AppData\\Local\\Programs\\TeamSpeak\\TeamSpeak.exe`.
- [ ] Recheck `f` / `fangwen`: `访问 Java.com` remains excluded.
- [ ] Recheck WinRAR `最新版本里有哪些新功能`, help/manual and uninstall/update entries: they remain excluded.
- [ ] Recheck SOLIDWORKS only for regression in presence/absence; component-family filtering is intentionally deferred to the next hygiene step.
- [ ] Normal Start Menu/App Paths applications whose `.exe` subsystem is recognized continue to behave identically.
- [ ] Classic visuals/geometry, table resize, numeric Quick Launch, pinyin search and Everything behavior remain unchanged.

## v0.8.0-alpha.5.18 Provider index lifecycle & admission observability validation

- [ ] With an existing valid schema-6 Provider Cache, launch ALTRun Next: the full cached result set is immediately available and background refresh does not blank or replace it with a user-only intermediate list.
- [ ] Delete/rename `data/provider-cache.json` and start ALTRun Next with startup behavior `Show launcher`: the launcher must not appear with only `calc/cmd/explorer/...`; it should reveal only after the initial static-provider refresh attempt is complete.
- [ ] Repeat the no-cache test and press the global launcher hotkey while discovery is Building: the request is deferred, then one fully published result snapshot appears when discovery completes.
- [ ] After the first no-cache build completes, hide/show the launcher repeatedly: results remain complete and stable; typing is not required to make provider results appear.
- [ ] Force or simulate a provider failure: after the completed attempt the index may enter Degraded and remain usable, but it must not expose provider-by-provider intermediate refresh states.
- [ ] Recheck `teamsp`: TeamSpeak 3 and the user-level TeamSpeak/TeamSpeak 6 entry remain simultaneously available when installed.
- [ ] Recheck `f`/`fangwen`, WinRAR help/What's New and obvious uninstall/helper entries: alpha.5.16/.17 admission hygiene remains intact.
- [ ] Search-source settings and manual `重建程序索引` still complete without blocking the UI; valid old cache remains usable until the completed refresh snapshot is published.
- [ ] Classic visuals/geometry, table resize, numeric Quick Launch, pinyin, Everything and result ranking remain unchanged.

## v0.8.0-alpha.5.19 Launch target inspector root-cause validation

- [ ] Do not delete Provider Cache and do not rebuild the program index before diagnosis; alpha.5.19 intentionally keeps schema 6 so the investigation is not altered by another forced refresh.
- [ ] Run `ALTRunNext.exe --diagnose-shortcut "C:\\Users\\Asp\\AppData\\Roaming\\Microsoft\\Windows\\Start Menu\\Programs\\TeamSpeak.lnk"`.
- [ ] Open `data\\launch-target-diagnostic.json` and preserve the complete file. Check `shellLink.stage`, exact resolved `target`, `targetInspection.legacyAlpha516Kind`, `currentKind`, executable `stage`, `optionalMagic`, `subsystem`, `fallbackUsed`, plus alpha.5.16/current admission decisions.
- [ ] If `legacyAlpha516Kind` is `gui-executable`, treat ExecutableUnknown as unrelated to the TeamSpeak failure and remove/reassess that fallback before further search-hygiene work.
- [ ] If `legacyAlpha516Kind` is `unknown`, use the recorded executable stage to repair the actual generic failure rather than adding a TeamSpeak special case.
- [ ] Confirm running the diagnostic command does not start a second launcher instance, change settings, rebuild Provider Cache, or alter normal search results.
- [ ] Normal launcher startup, TeamSpeak 3/6 results, alpha.5.18 provider lifecycle and all previous admission-hygiene cases remain unchanged.

## v0.8.0-alpha.5.20 Evidence-backed admission cleanup validation

- [ ] Upgrade from alpha.5.19. Provider Cache schema 7 must rebuild once.
- [ ] During the schema-7 rebuild, startup behavior `Show launcher` must not reveal a user-only/partial result list; the launcher becomes searchable only after the completed provider snapshot is published.
- [ ] Search `teamsp` after the rebuild: TeamSpeak 3 and TeamSpeak/TeamSpeak 6 must both remain available without any ExecutableUnknown fallback.
- [ ] Confirm TeamSpeak launches through the original Start Menu shortcut targeting `C:\\Users\\Asp\\AppData\\Local\\Programs\\TeamSpeak\\TeamSpeak.exe`.
- [ ] Recheck WinRAR help/What's New, `访问 Java.com`, uninstall/update/helper/native-messaging entries: strict positive admission remains intact.
- [ ] Confirm ordinary GUI/CUI Start Menu and App Paths applications remain discoverable.
- [ ] Confirm there is no `--diagnose-shortcut` product path and no temporary launch-target diagnostic JSON is produced during normal use.
- [ ] Classic visuals/geometry, table resize, numeric Quick Launch, pinyin, Everything and result ranking remain unchanged.

## v0.8.0-alpha.5.21 Intelligent Launch Catalog validation

- [ ] Upgrade from alpha.5.20. Provider Cache schema 8 rebuilds once; the alpha.5.18 Building gate must still prevent a partial first-launch index.
- [ ] Search `chro`: only one normal Google Chrome representation should remain. Prefer the Start Menu title/shortcut when both Start Menu and App Paths resolve to the same `chrome.exe` identity.
- [ ] Verify a distinct shortcut to the same executable with meaningful arguments is not incorrectly merged with the normal application.
- [ ] Launch `时钟` and `设置` from Windows Apps results. Both must activate normally; no `系统找不到指定的文件` error for an AUMID target.
- [ ] Search for `关于 Java` / equivalent product About entry: ProductInfo actions should not enter the normal catalog; no Java-specific blacklist is used.
- [ ] Recheck the previously observed `单击以执行` / similar internal Windows activation surface. Entries carrying Windows hidden/internal shell evidence should be absent while normal packaged apps remain.
- [ ] Search `team`: TeamSpeak 3 and TeamSpeak/TeamSpeak 6 remain simultaneously present after the schema-8 strict rebuild; there is no TeamSpeak-specific production or merge-test logic.
- [ ] Recheck WinRAR documentation/What's New, uninstall/update/helper/native-messaging entries: existing positive-admission hygiene remains intact.
- [ ] Classic visuals/geometry, table resize, numeric Quick Launch, pinyin, Everything and result ranking remain unchanged.

## v0.8.0-alpha.5.22 Provider monitor performance hygiene validation

- [ ] Upgrade from alpha.5.21. Provider Cache stays schema 8; no forced catalog rebuild is caused by this version alone.
- [ ] Leave ALTRun Next idle for at least 1–5 minutes with Windows Apps enabled. Task Manager CPU should remain effectively idle between brief provider-monitor checks; there should be no sustained CPU activity.
- [ ] Compare idle behavior with alpha.5.21 on the same machine if practical; the packaged provider monitor should perform less Shell/COM work while preserving change detection.
- [ ] Install/remove or otherwise change a visible Windows Apps entry, wait for the normal provider monitor cycle, and confirm the catalog still refreshes.
- [ ] Recheck `时钟` and `设置` launch correctly, Chrome remains deduplicated, ProductInfo/internal surfaces remain filtered, and TeamSpeak 3/6 remain present.
- [ ] Recheck repeated launcher invocation and continuous typing: search responsiveness is unchanged because no new work was added to the search hot path.
- [ ] Record working-set memory after 30 seconds idle for comparison with the existing alpha.5.x baseline.

## v0.8.0-alpha.5.23 Classic live-result repaint validation

- [ ] With Classic active, invoke the launcher and 连续输入 a multi-character query at normal and fast typing speed. The result area must update without the previous whole-list flash.
- [ ] Repeat while backspacing rapidly, changing between queries that return 10, a few, one and zero results. Removed rows must clear cleanly without stale pixels or blank stripes.
- [ ] Verify the best result remains selected after each text edit; asynchronous Everything merges preserve the currently selected result by identity when it still exists.
- [ ] Verify Up/Down/Tab wrapping, numeric Quick Launch timing, single-result immediate execution and IME composition behavior are unchanged.
- [ ] With result icons enabled, first-use asynchronous icon arrival redraws only the affected rows and does not reintroduce whole-list flashing.
- [ ] Recheck idle CPU remains effectively 0% after the alpha.5.22 event-driven monitor change.
- [ ] Record working set at 30 seconds before first search (~7 MB observed baseline) and after the first normal search (~11 MB observed baseline); confirm it stabilizes rather than growing continuously.
- [ ] Recheck 时钟/设置 activation, Chrome dedupe and TeamSpeak 3/6 presence. SOLIDWORKS suite-role filtering is not claimed by this version and remains a later Intelligent Launch Catalog step.
- [ ] Classic geometry/assets and Provider Cache schema 8 remain unchanged.

## v0.8.0-alpha.5.24 Classic repaint-isolation validation

- [ ] Classic: type `s` then continue quickly through `so` / `sol` / `solidworks`. The EDIT/title area must remain visually stable while results change.
- [ ] Classic: type a nonsense query such as `2dsfs` until zero results, continue typing several characters, then backspace repeatedly while remaining at zero results. There must be no whole-window or input-area flash.
- [ ] Transition repeatedly between result-bearing and zero-result queries. The LISTBOX must clear/fill without stale rows, blank stripes, background flashes or input-area redraw.
- [ ] Hold Backspace through a result-bearing query into the empty query and then type again at normal and very fast speed.
- [ ] With Everything enabled, repeat while asynchronous results arrive; selection preservation and delayed merges must not reintroduce parent/input flicker.
- [ ] With result icons both enabled and disabled, first-use icon arrival must repaint only result rows.
- [ ] Recheck Up/Down/Tab wrapping, numeric Quick Launch timing, single-result immediate execution and IME composition behavior.
- [ ] Recheck idle CPU remains effectively 0%, working set stays near the observed ~7 MB idle / ~11 MB post-search baseline without continuous growth, and alpha.5.22 provider-monitor behavior does not regress.
- [ ] Recheck 时钟/设置 activation, Chrome dedupe and TeamSpeak 3/6 presence. SOLIDWORKS suite-role admission remains explicitly outside alpha.5.24.
- [ ] Classic geometry/assets and Provider Cache schema 8 remain unchanged.

## v0.8.0-alpha.5.25 Launch Role Evidence Model validation

- [ ] Upgrade from alpha.5.24. The generated Provider Cache rebuilds once under schema 9; user settings/commands/usage schemas remain unchanged.
- [ ] After the provider rebuild, verify normal application search results are materially unchanged from alpha.5.24. Alpha.5.25 must not apply `CatalogVisibility` in SearchEngine/ResultRanking yet.
- [ ] Recheck representative Start Menu, Windows Apps and App Paths applications launch normally; PATH remains disabled by default unless explicitly enabled.
- [ ] Recheck Chrome-style canonical dedupe, packaged Clock/Settings activation and TeamSpeak 3/6 coexistence.
- [ ] Inspect `data/provider-cache.json` and confirm generated commands contain `applicationRole`, `roleConfidence`, `catalogVisibility`, `catalogGroupKey` and `distinctiveTokens`.
- [ ] For several installed GUI executables with Version Resource data, confirm role evidence is populated without adding any visible delay while typing in the Launcher.
- [ ] Leave ALTRun Next idle after catalog rebuild and confirm CPU remains effectively 0%; executable metadata parsing must occur only during provider discovery/refresh.
- [ ] Invoke Classic and type/backspace through result-bearing and zero-result queries to confirm alpha.5.24 repaint isolation remains intact.
- [ ] Confirm working-set memory remains in the established range and does not continuously grow after provider refresh/search activity.
- [ ] Observe a multi-entry installed software suite only as evidence validation. Do not judge alpha.5.25 by whether auxiliary suite entries are suppressed: role-aware StrongMatchOnly/Hidden query admission is intentionally deferred to the next stage.
- [ ] No product-specific blacklist/rule should exist for SolidWorks, TeamSpeak or any other real application family.

## v0.8.0-alpha.5.26 Role-aware Query Admission validation

- [ ] Upgrade from alpha.5.25. Provider Cache remains schema 9; there should be no schema-triggered full rebuild caused by alpha.5.26 itself.
- [ ] Search the shared family/product name of a real multi-entry installed suite. Normal primary/companion applications should remain, while entries already classified `StrongMatchOnly` should no longer appear merely because they share the product name.
- [ ] Explicitly search a distinctive auxiliary intent such as its settings/configuration, performance/benchmark, diagnostic or download-manager wording. The corresponding `StrongMatchOnly` entry should become searchable.
- [ ] Type the complete title of a `StrongMatchOnly` entry and confirm it remains reachable even when distinctive grouping metadata is incomplete.
- [ ] Entries classified `Hidden` (for example a high-confidence updater/service/internal component) should not appear in normal Launcher search even when its complete title or a wildcard is entered.
- [ ] A user-created shortcut to any such target must remain searchable; user intent overrides automatic catalog visibility.
- [ ] With wildcard/path syntax enabled, verify `StrongMatchOnly` entries can still be explicitly addressed while `Hidden` generated entries remain excluded.
- [ ] Recheck a product with multiple legitimate applications/companions: role-aware admission must not collapse the suite to one executable.
- [ ] Recheck pinyin, short-query relevance, Usage ranking, Everything merge and provider ordering; alpha.5.26 does not change their scoring.
- [ ] Leave the app idle and type rapidly after the catalog is warm. CPU/memory behavior should stay near the alpha.5.25 baseline because query admission only reads cached enum/token fields.
- [ ] Recheck alpha.5.24 typing/backspace repaint isolation and Classic geometry/assets.
- [ ] Validate any observed SolidWorks/other-suite improvement only as a real-world example of the generic model; there must be no product-specific production rule.

## v0.8.0-alpha.5.27 Role Evidence Calibration + Catalog Context validation

- [ ] Upgrade from alpha.5.26. Generated Provider Cache should rebuild once under schema 10; settings, user commands and usage schemas remain unchanged.
- [ ] Search a shared family/product name in a real multi-entry suite. Entries with clear performance/settings/diagnostic/download roles should no longer appear merely because they share the family name once they classify `StrongMatchOnly`.
- [ ] Explicitly search the distinctive role intent (for example performance, benchmark, settings, configuration, diagnostics or download wording). The corresponding `StrongMatchOnly` entry must remain reachable.
- [ ] Verify a high-information title phrase such as Performance Test / Settings Wizard / 性能测试 / 设置向导 is at least Medium confidence even when executable metadata provides no second role field.
- [ ] Verify an isolated entry containing only a weak generic word such as `Settings` is not automatically promoted by that word alone; catalog context must supply the corroboration.
- [ ] In a related product/location group containing a clear primary application, verify weak Settings/Benchmark/Diagnostics/Download roles can be calibrated to Medium and become `StrongMatchOnly`.
- [ ] Verify Quick Launch / Safe Mode / no-plugins style entries become `AlternateLaunch` only when a related group also contains a clear primary application. Same-target/argument evidence may strengthen confidence.
- [ ] Verify two or more legitimate companion applications (for example generic Editor/Renderer/Encoder fixtures) remain present and `Normal`; group membership must never collapse a suite to one executable.
- [ ] Verify high-confidence updater/service/internal roles remain hidden under the existing visibility policy.
- [ ] A user-created shortcut to any generated target must remain searchable and must not be rewritten by catalog-context calibration.
- [ ] Inspect `data/provider-cache.json`: schema is 10 and contains base role/confidence/visibility/group/token evidence. Contextual promotion is recomputed at catalog publication rather than accumulated into cached state.
- [ ] Recheck short-query relevance, pinyin, Usage ranking, Everything merge, provider ordering and explicit wildcard/path behavior; alpha.5.27 does not change SearchEngine or ResultRanking.
- [ ] Leave ALTRun Next idle and type rapidly after the catalog is warm. No executable, registry, Shell, grouping or filesystem work should occur on the keystroke path; idle CPU/memory behavior should remain near the alpha.5.26 baseline.
- [ ] Recheck alpha.5.24 typing/backspace repaint isolation and frozen Classic geometry/assets.
- [ ] Use any installed commercial suite only as real-world evidence. There must be no product-name-specific production rule or regression fixture.

## v0.8.0-alpha.5.28 Distinctive Intent Isolation validation

- [ ] Upgrade from alpha.5.27. Generated Provider Cache must rebuild once under schema 11; settings, user commands and usage schemas remain unchanged.
- [ ] Search a short shared-family prefix in a suite containing compact titles that join family + version + CJK/role text. StrongMatchOnly auxiliary entries must not reappear merely because their display title starts with that family.
- [ ] Explicitly search the auxiliary role intent (for example 性能测试 / 设置向导 / performance / settings / quick launch). The corresponding StrongMatchOnly entry must remain reachable.
- [ ] Type the complete auxiliary entry title exactly and confirm exact full-title fallback still reaches StrongMatchOnly.
- [ ] Inspect `data/provider-cache.json`: schema is 11 and `distinctiveTokens` for compact auxiliary titles must not contain the shared family prefix.
- [ ] Verify ordinary spaced English suite names still retain useful entry-specific tokens such as encoder/editor/render while family tokens are removed.
- [ ] Verify compact no-space English titles and CJK titles both isolate family identity correctly.
- [ ] Recheck multiple legitimate CompanionApplication entries remain Normal; this version changes token isolation, not role suppression policy.
- [ ] Recheck Hidden updater/service/internal behavior, user-shortcut authority, wildcard/path explicit access, pinyin, Usage ranking and Everything merge.
- [ ] Recheck idle CPU/memory and typing latency; SearchEngine performs no new grouping, metadata or token-building work during input.
- [ ] Recheck frozen Classic geometry/assets and alpha.5.24 repaint isolation.
- [ ] No commercial product name should appear in new production rules or new role-model regressions.

## v0.8.0-alpha.5.29 Catalog Evidence Pipeline Hardening validation

- [ ] Upgrade from alpha.5.28. Provider Cache must rebuild once under schema 12; settings, usage and user-command schemas remain unchanged.
- [ ] In a Start Menu suite where helper EXEs report different ProductName values, search only the shared family prefix. Explicit benchmark/settings/diagnostic/download helpers classified StrongMatchOnly must not appear.
- [ ] Explicitly search each helper role term. StrongMatchOnly entries must remain reachable, and exact full-title fallback must still work.
- [ ] Verify Quick Launch / Safe Mode / no-plugins variants become AlternateLaunch only when a related suite primary exists; they must not be suppressed by the phrase alone in isolation.
- [ ] Verify Composer/Renderer/Editor/Encoder-style independent companion applications remain Normal even when their EXE ProductName differs from the suite folder name.
- [ ] Verify a helper whose own ProductName equals its “Performance Test” or “Settings Wizard” title is still classified by the high-information semantic role rather than PrimaryApplication.
- [ ] Inspect `data/provider-cache.json`: schema is 12. Start Menu catalogGroupKey values should use normalized `family:...|menu:...` context and restrictive role `distinctiveTokens` should contain role intent rather than the shared family prefix.
- [ ] Verify nested Start Menu subfolders under one suite remain context-related while generic folders such as Programs/Windows Tools do not manufacture a suite family.
- [ ] Verify user-created shortcuts remain authoritative and unaffected by automatic role/context normalization.
- [ ] Recheck App Paths/PATH/AppsFolder discovery, cross-provider canonical deduplication, Everything merge, pinyin, usage ranking and explicit wildcard/path syntax.
- [ ] Recheck startup/background refresh: no new filesystem, EXE metadata, registry or Shell work may occur on the keystroke path.
- [ ] Recheck idle CPU/memory and alpha.5.24 Classic typing/backspace repaint isolation.
- [ ] No commercial product name may appear in production role/group rules or new regression fixtures.

## v0.8.0-alpha.5.30 Development Release / Updater validation

- [ ] Start from an installed alpha.5.28/alpha.5.29 build with “接收预发布版本更新” enabled and click check/retry. The update check must no longer report HTTP 404 for `update-manifest.json`.
- [ ] GitHub `dev-latest` must be a public prerelease, never Draft, after a green main run.
- [ ] Anonymous GET of `/repos/Aspeternity/ALTRunNext/releases/tags/dev-latest` must succeed and report `draft=false`, `prerelease=true`.
- [ ] Anonymous GET of `/releases/download/dev-latest/update-manifest.json` must return HTTP 200.
- [ ] The public manifest `version` and `commit` must equal the current VERSION and main/dev-latest tag SHA.
- [ ] The public manifest bytes must match the CI-generated manifest; package names and SHA-256 fields remain valid.
- [ ] Trigger/observe consecutive main pushes. A queued stale release run must skip publication rather than overwrite a newer main commit, and an in-progress publish must not be cancelled midway.
- [ ] If an orphan `dev-latest` Draft release exists before a run, the next successful main publish must repair it to public instead of leaving the client endpoint at 404.
- [ ] Verify the versioned prerelease is still published and contains x64/ARM64 ZIPs, SHA256SUMS.txt and update-manifest.json.
- [ ] Provider Cache stays schema 12; search/catalog behavior and frozen Classic UI/geometry are unchanged.

## v0.8.0-alpha.5.31 Alternate / Suite Utility validation

- [ ] Upgrade from alpha.5.30. Provider Cache rebuilds once under schema 13; settings, user commands and usage remain unchanged.
- [ ] Search only a suite family prefix. Quick Launch/Safe Mode/no-plugins variants corroborated by the primary must not appear.
- [ ] Explicitly search quick/safe/no-plugins wording. The corresponding AlternateLaunch entries remain reachable.
- [ ] Validate an alternate shortcut placed in a different Start Menu subfolder. Same canonical primary target + variant arguments must still classify StrongMatchOnly.
- [ ] Validate an alternate entry with a different target but a title base matching the clear primary. It should classify Medium/StrongMatchOnly rather than requiring identical menu location.
- [ ] An isolated unrelated “Quick Launch” entry with no primary relationship must remain Low/Normal.
- [ ] Task Scheduler / Job Scheduler / Sync Manager / automation/maintenance utility-style entries should be StrongMatchOnly when evidence is explicit.
- [ ] A bare Sync/Scheduler/Automation/Maintenance cue remains Normal without suite context and becomes SuiteUtility only when corroborated by a related primary.
- [ ] Verify token-aware weak matching: an unrelated `async` title must not be treated as `sync`.
- [ ] Search a suite containing both Composer (or another independent companion) and Composer Sync. The independent companion remains Normal; family query suppresses Sync, while explicit `sync` finds it.
- [ ] Validate an opaque-title diagnostic whose Version Resource description/internal name contains generic diagnostic/problem-report/support/recovery evidence. Family query suppresses it, while its family-stripped entry name remains usable as explicit intent.
- [ ] User shortcuts remain authoritative and are never rewritten by Alternate/SuiteUtility calibration.
- [ ] Inspect `data/provider-cache.json`: schema is 13 and the new `suite-utility` role persists by name.
- [ ] Recheck Settings/Benchmark suppression from alpha.5.29, App Paths/PATH/AppsFolder, Everything merge, pinyin, usage ranking, idle CPU/memory and frozen Classic repaint/geometry behavior.
- [ ] No commercial product name may appear in production role rules or new generic role regressions.

## v0.8.0-alpha.5.32 Catalog Residual Evidence validation

- [ ] Upgrade from alpha.5.31. Provider Cache rebuilds once under schema 14; settings, user commands and usage remain unchanged.
- [ ] In a suite with a clear primary, a Network Monitor / Network Monitoring entry must become SuiteUtility + StrongMatchOnly and disappear from a plain family-prefix query.
- [ ] A License Manager / Licensing Manager / License Administrator / License Utility style entry must likewise require suite-primary context before suppression.
- [ ] A Service Manager / Service Administrator / Service Console / Service Utility Start Menu surface must be treated as user-facing SuiteUtility rather than generic ServiceComponent when a related primary exists.
- [ ] A standalone Network Monitor or Service Manager with no related primary must stay Low/Normal and remain visible.
- [ ] A true Service Host / daemon/background component must remain ServiceComponent/BackgroundComponent; the management-surface exception must not weaken component suppression.
- [ ] If provider metadata initially labels a contextual Network Monitor as BackgroundComponent or Service Manager as ServiceComponent, catalog publication should repair the role to SuiteUtility only when the title phrase and related primary both corroborate it.
- [ ] Explicit monitor/license/service searches must re-admit the corresponding StrongMatchOnly utility.
- [ ] Nearby independent applications such as Network Designer remain Normal.
- [ ] Routing / Player / Boost / opaque short names remain unchanged unless they have separate generic metadata evidence.
- [ ] Inspect `data/provider-cache.json`: schema is 14 and rebuilt role/token state persists without any commercial-product rule.
- [ ] Recheck alpha.5.31 Quick Launch, Scheduler/Sync, Composer-vs-Composer-Sync behavior, Settings/Benchmark suppression, App Paths/PATH/AppsFolder, Everything merge, pinyin, usage ranking, idle CPU/memory and frozen Classic UI.
- [ ] Recheck the alpha.5.30 anonymous dev-latest publication contract remains green.

## v0.8.0-alpha.5.33 Short Query Precision validation

- [ ] Upgrade from alpha.5.32. Provider Cache rebuilds once under schema 15; settings, user commands and usage remain unchanged.
- [ ] With representative normal applications whose names genuinely begin with two ASCII characters, a two-character query still returns them by whole-field prefix.
- [ ] A two-character query must not return an otherwise unrelated entry only because a later word begins with those characters (for example generic Admin / Advanced / Additional / Sources cases).
- [ ] Explicit two-character aliases and user shortcuts still work.
- [ ] Two-character derived initials remain available (for example a multi-word app addressed by its initials).
- [ ] Existing pinyin-initial behavior remains available for short Latin queries.
- [ ] Repeat the same later-word query with three ASCII characters; BoundaryPrefix recall should resume.
- [ ] Verify a root-level Start Menu shortcut resolving into Windows Tools / Administrative Tools / System Tools is classified as SystemUtility even when the shortcut file itself is not stored in that folder.
- [ ] Verify a root-level shortcut resolving into Developer Tools / Visual Studio Tools / Windows Kits / SDK is classified as DeveloperTool.
- [ ] A normal root-level application whose resolved target is outside those structural locations remains PrimaryApplication.
- [ ] Search ordering should naturally place a genuine primary-app prefix ahead of a SystemUtility prefix when both match the same short query; do not use title/product blacklists.
- [ ] Recheck alpha.5.32 family suppression and explicit monitor/license/service recovery, alpha.5.31 alternate/suite-utility behavior, Everything merge, pinyin, usage ranking and wildcard/path syntax.
- [ ] Confirm no extra filesystem/registry/Shell work occurs per keystroke; resolved-target surface refinement occurs only during Start Menu discovery.
- [ ] Recheck idle CPU/memory and frozen Classic UI/geometry.
- [ ] Recheck the alpha.5.30 anonymous dev-latest publication contract remains green.

## v0.8.0-alpha.5.34 Launch Surface Evidence Completion validation

- [ ] Upgrade from alpha.5.33. Provider Cache rebuilds once under schema 16; settings, user commands and usage remain unchanged.
- [ ] Find a Start Menu management shortcut whose .lnk resolves to Windows control.exe with a /name-style argument rather than directly into an Administrative/Windows Tools filesystem folder; it is classified as SystemUtility.
- [ ] Verify MMC-backed management shortcuts remain searchable but carry SystemUtility rather than PrimaryApplication semantics.
- [ ] Verify a Control_RunDLL/.cpl shortcut and an ms-settings URI surface are classified as SystemUtility when present.
- [ ] Verify an explorer-hosted or direct ::{...}/shell:::{...} namespace shortcut is classified structurally without any title rule.
- [ ] A normal application named similarly to a Windows broker but installed outside the Windows directory must not become SystemUtility from its leaf filename alone.
- [ ] A normal shell:AppsFolder application remains an application surface and is not treated as a generic namespace utility.
- [ ] Repeat the alpha.5.33 short-query case where a genuine PrimaryApplication prefix and a SystemUtility prefix both match; the primary application should naturally rank ahead because the surface evidence is now correct, not because either title was blacklisted.
- [ ] Inspect data/provider-cache.json: schema is 16 and the corrected surfaceClass persists after restart.
- [ ] Confirm Start Menu discovery may inspect Shell/PIDL data during refresh, but typing performs no new Shell, filesystem, registry or executable-metadata I/O.
- [ ] Recheck alpha.5.33 1-2 ASCII BoundaryPrefix suppression and 3-character recovery, alpha.5.32 catalog-role behavior, Everything merge, pinyin, usage scoring, wildcard/path syntax, idle CPU/memory and frozen Classic UI/geometry.
- [ ] Recheck the alpha.5.30 anonymous dev-latest publication contract remains green.

## v0.8.0-alpha.5.35 Suite Member Topology validation

- [ ] Upgrade from alpha.5.34. Provider Cache rebuilds once under schema 17; settings, user commands and usage remain unchanged.
- [ ] In a real multi-entry suite, a base companion and a child whose display identity and resolved executable stem both extend that base are calibrated as CompanionApplication + SuiteSubordinate respectively.
- [ ] A short family prefix still returns the primary application and independent companion applications, but does not return the SuiteSubordinate child merely because it shares the family prefix.
- [ ] Searching the parent companion name returns the parent without automatically re-admitting its subordinate child.
- [ ] Searching the child-only delta intent re-admits the SuiteSubordinate entry.
- [ ] Typing the exact full child title still reaches it through the existing exact-match escape hatch.
- [ ] A title-only extension whose executable stem does not extend the putative parent remains a normal CompanionApplication.
- [ ] An opaque one-off companion with no corroborating parent/child topology remains Normal; this version must not add product-word blacklists.
- [ ] Inspect `data/provider-cache.json`: schema is 17, the subordinate role is persisted as `suite-subordinate`, and its distinctiveTokens contain only child-specific intent rather than the shared parent/family identity.
- [ ] Recheck alpha.5.34 SystemUtility shell evidence, alpha.5.33 short-query precision, alpha.5.32 suite-role behavior, Everything merge, pinyin, wildcard/path syntax and usage scoring.
- [ ] Confirm catalog topology adds no Shell/filesystem/registry/metadata work while typing and frozen Classic UI/geometry is unchanged.
- [ ] Recheck idle CPU/memory and the dev-latest public-manifest publication contract.

## v0.8.0-alpha.5.36 Advertised Shortcut Resolution + Target Topology validation

- [ ] Upgrade from alpha.5.35. Provider Cache rebuilds once under schema 18; settings, user commands and usage remain unchanged.
- [ ] For a Windows Installer advertised Start Menu shortcut that previously cached a `C:\\Windows\\Installer\\...\\newshortcut...` identity, the Start Menu cache now records the real installed launchable component path as `canonicalIdentity`.
- [ ] Launch that same result and verify Windows still executes the original Start Menu `.lnk`; advertised-target resolution must not change activation behavior.
- [ ] No scan, startup or rebuild operation triggers an MSI repair/configuration dialog, source prompt, install action or feature-usage side effect.
- [ ] A normal non-advertised `.lnk` keeps the same resolved target and launch behavior as alpha.5.35.
- [ ] In a real suite with base and child executables whose stems extend (for example generic `Composer.exe -> ComposerPlayer.exe` topology), the child becomes `suite-subordinate` while the base remains a normal companion.
- [ ] In a real suite where executable names do not extend but nearby install-directory segments do (generic `Visualize -> Visualize Boost` topology), the child can still become `suite-subordinate`.
- [ ] A plain family prefix does not show a successfully classified subordinate; searching the child-only delta still re-admits it, and exact full-title search still works.
- [ ] A title-only extension whose real executable and nearby directory structure do not corroborate the parent remains a normal companion.
- [ ] Metadata changes from resolving the real executable must not break child intent: title identity comes from catalog family + display title, and the cached subordinate intent contains only the child delta.
- [ ] Inspect `data/provider-cache.json`: schema is 18, affected Start Menu canonical identities no longer point at MSI proxy executables, and corrected role/visibility/token state persists after restart.
- [ ] Recheck ambiguous one-off entries separately; alpha.5.36 must not hide an entry solely because of an opaque product word or short name.
- [ ] Confirm MSI/filesystem work occurs only during provider discovery/refresh; typing adds no MSI, filesystem, registry or metadata I/O.
- [ ] Recheck alpha.5.34 SystemUtility evidence, alpha.5.33 short-query precision, Everything merge, pinyin, wildcard/path syntax, usage scoring, idle CPU/memory and frozen Classic UI/geometry.
- [ ] Recheck the dev-latest public-manifest publication contract.

## v0.8.0-alpha.5.37 Utility Container Corroboration validation

- [ ] Upgrade from alpha.5.36. Provider Cache rebuilds once under schema 19; settings, user commands and usage remain unchanged.
- [ ] A normal suite folder and a sibling `Tools`, `Utilities` or `工具` folder now share the same catalog family while retaining different `menu:` locations.
- [ ] Merely living inside the utility folder does not suppress an entry.
- [ ] A utility-folder entry whose resolved executable is a sidecar in the clear primary application's install directory becomes `suite-utility + medium + strong-match-only`.
- [ ] A utility-folder entry with a generic management surface such as `Library Manager` becomes `suite-utility + medium + strong-match-only` when the related clear primary exists, even if its executable is in a nested manager directory.
- [ ] An independent entry in the same utility folder with its own installed subdirectory and no generic utility semantics remains Normal.
- [ ] An isolated Tools/Utilities folder with no clear related primary cannot self-promote or suppress its entries.
- [ ] A plain suite-family query excludes corroborated SuiteUtility entries but keeps the independent entry.
- [ ] Searching a restrictive entry by its own family-stripped identity word re-admits it; a management surface must be reachable by its distinguishing word as well as the generic role phrase.
- [ ] Inspect `data/provider-cache.json`: schema is 19, utility-folder `catalogGroupKey` uses the base suite family, and corrected role/visibility/distinctiveTokens persist after restart.
- [ ] Recheck alpha.5.36 advertised-shortcut targets and SuiteSubordinate behavior; utility-container changes must not restore proxy identities or child entries to family queries.
- [ ] Confirm no product-name blacklist was introduced and ambiguous opaque entries without structural/semantic corroboration remain visible.
- [ ] Confirm all utility-container calibration is publication-time only; typing adds no filesystem, metadata, Shell, MSI or catalog-analysis I/O.
- [ ] Recheck short-query precision, SystemUtility ordering, Everything merge, pinyin, wildcard/path syntax, usage scoring, idle CPU/memory and frozen Classic UI/geometry.
- [ ] Recheck the dev-latest public-manifest publication contract.

## v0.8.0-alpha.5.38 Opaque Auxiliary Corroboration validation

- [ ] Upgrade from alpha.5.37. Provider Cache rebuilds once under schema 20; settings, user commands and usage remain unchanged.
- [ ] A short/opaque family-stripped entry name by itself remains Normal.
- [ ] ProductName carrying a generic diagnostic/repair/update role remains Low confidence by itself, but corroborating Description/InternalName/OriginalFilename evidence can produce the appropriate restrictive role.
- [ ] In a corroborated Tools/Utilities/工具 folder, a short opaque entry whose real target is one or two directory levels beneath the clear primary install directory becomes `suite-utility + medium + strong-match-only`.
- [ ] A short opaque entry in an unrelated install tree remains Normal.
- [ ] A normal family query hides a corroborated opaque utility; searching its own short residual identity re-admits it.
- [ ] Longer independent companions in their own subdirectories remain unchanged.
- [ ] Inspect `data/provider-cache.json`: schema is 20 and corrected role/visibility/distinctiveTokens persist after restart.
- [ ] Confirm no product-specific abbreviation or vendor name was added to production rules.
- [ ] Confirm all new work is discovery/publication-time only; typing adds no filesystem, metadata, Shell, MSI or catalog-analysis I/O.
- [ ] Recheck alpha.5.37 utility-container behavior, alpha.5.36 advertised-shortcut targets, suite-subordinate behavior, short-query precision, Everything merge, pinyin, wildcard/path syntax, usage scoring, idle CPU/memory and frozen Classic UI/geometry.

## v0.8.0-alpha.5.39 Family-Distinctive Intent Boundary validation

- [ ] Upgrade from alpha.5.38. Provider Cache remains schema 20; no generated-state rebuild is required solely for this search admission change.
- [ ] A plain short family prefix and the full family identity do not show a corroborated `StrongMatchOnly` opaque auxiliary.
- [ ] Searching the auxiliary's own short residual identity re-admits it.
- [ ] Searching family + residual identity re-admits it.
- [ ] Normal primary and independent companion applications remain visible for the same family query.
- [ ] An unrelated opaque Normal application remains searchable and is not suppressed by this gate.
- [ ] Wildcard/path explicit syntax, short-query precision, pinyin, Everything merge and usage scoring remain unchanged.
- [ ] Confirm the keystroke path performs cached string matching only: no filesystem, Shell/MSI, EXE metadata or catalog-role inference.
- [ ] Confirm no product/vendor/abbreviation rule was added and Classic UI/geometry is unchanged.

## v0.8.0-alpha.5.43 Query-Scoped Usage + Packaged Entry Integrity validation

- [ ] Upgrade from schema-1 `usage.json`: global counts and empty-query order remain, while searched queries learn anew. Usage schema is 2; Provider Cache remains schema 22.
- [ ] Select Spotify twice from `s` (including a delayed numeric selection), then select Steam twice from `st`: Spotify remains above Steam for `s`, while `st` still selects Steam. Restart and confirm both query histories persist.
- [ ] A single successful launch does not reorder results; failed launch, empty query, explicit path and wildcard search do not create query-specific history. Usage reset clears both global and query counts.
- [ ] In `sto`, the registered Microsoft Store entry remains available while a matching App Paths `WindowsApps\\...\\store.exe` internal entry is absent. A package with no enabled AUMID and an unrelated Win32 executable are not removed solely because of the path or filename.
- [ ] Confirm search keystrokes remain cache-only, Catalog admission and frozen Classic geometry remain unchanged.

## v0.8.0-alpha.5.42 Intent-Stable Usage Ranking validation

- [ ] On a short family query with comparable results, a companion with at least two launches can rise above an unrelated, slightly higher-scoring name. One launch alone does not reorder them.
- [ ] Exact names, pinned/user shortcuts and stronger match kinds remain above weaker frequently used matches. Explicit syntax and path queries preserve their previous order.
- [ ] The frequency effect is capped, stable across idle time and search restarts, and cleared by the existing usage reset. Empty-query recency remains unchanged.
- [ ] Confirm Catalog family-only suppression, residual-only recovery, family-plus-residual recovery and longer independent companions still work. Search stays cache-only; Classic geometry is unchanged.
- [ ] Provider Cache remains schema 22 and Usage schema 1. Windows fixed version is 0.8.0.212.

## v0.8.0-alpha.5.41 Shared Common Files Corroboration validation

- [ ] Upgrade from alpha.5.40; schema 22 rebuilds Provider Cache. Check the short auxiliary under a matching family-owned Common Files directory becomes `suite-utility + medium + strong-match-only` while a clear High primary lives in a related suite Start Menu folder.
- [ ] A family query hides the corroborated opaque entry and preserves independent companions; its own short identity and Family + exact short residual identity each find it.
- [ ] A different owner immediately beneath Common Files, a family word only deeper inside another owner's directory, and a short entry in the main menu with a distant target stay Normal.
- [ ] Search remains cache-only, and Classic geometry and short-query precision remain unchanged.

## v0.8.0-alpha.5.40 Opaque Suite Topology + Multi-Token Distinctive Intent validation

- [ ] Upgrade from alpha.5.39. Provider Cache rebuilds once under schema 21; settings, user commands and usage remain unchanged.
- [ ] A short opaque entry in the same normal suite Start Menu context becomes `suite-utility + medium + strong-match-only` only when a clear High-confidence primary exists and the resolved target is a same-directory sidecar or shallow descendant of the primary install directory.
- [ ] The same short opaque identity in an unrelated install tree remains Normal.
- [ ] Longer independent companions remain Normal even when they share the same suite context/install tree.
- [ ] A family query hides the corroborated opaque auxiliary while keeping the primary and independent companions.
- [ ] The short residual identity alone re-admits the auxiliary.
- [ ] Family + exact short residual identity re-admits the auxiliary.
- [ ] A one-character residual prefix does not re-admit it and global one/two-character BoundaryPrefix behavior remains unchanged.
- [ ] Inspect `data/provider-cache.json`: schema is 21 and the corrected role/visibility/distinctiveTokens persist after restart.
- [ ] Confirm no product/vendor/abbreviation rule was added.
- [ ] Confirm the keystroke path remains cache-only: no filesystem, Shell/MSI, EXE metadata or role inference.
- [ ] Recheck utility-container, advertised-shortcut, suite-subordinate, Everything, pinyin, wildcard/path syntax, usage scoring, idle CPU/memory and frozen Classic UI/geometry.

## Release assets

For the candidate tag:

- [ ] ALTRunNext-x64.zip is present.
- [ ] ALTRunNext-ARM64.zip is present.
- [ ] SHA256SUMS.txt is present.
- [ ] Both ZIP hashes match SHA256SUMS.txt.
- [ ] ZIP VERSION matches the tag.
- [ ] EXE FileVersion/ProductVersion match the fixed Windows version derived from VERSION.
- [ ] dict/mandarin and third_party/cpp-pinyin-LICENSE.txt are present.
- [ ] README.md, CONFIG_SCHEMA.md and DESKTOP_VALIDATION.md are present.
- [ ] No unexpected runtime DLL is present.

## Sign-off

Do not mark a manual item as passed unless it was observed on the stated real desktop environment.

| Field | Value |
| --- | --- |
| Version / tag | |
| Commit | |
| Package / SHA-256 | |
| Windows edition/build | |
| Architecture | |
| Display scaling | |
| Data path | clean / upgraded |
| Result | PASS / FAIL |
| Notes / issue link | |


## v0.6.0-alpha.6 Hotkey Registry validation

The following checks extend the historical desktop matrix for the schema-4 centralized Hotkey Registry.

- [ ] Settings contains a dedicated **快捷键 / Hotkeys** page listing all five published action IDs through localized labels.
- [ ] Default `launcher.activate` is Alt + Space and opens/hides the launcher repeatedly.
- [ ] Change the primary global binding; the new binding works immediately and the old binding stops working.
- [ ] Attempt to assign a global chord already owned by another program; the save/apply fails and the previous ALTRun Next binding remains active.
- [ ] Enable, rebind and disable the secondary global activation; primary activation remains unaffected throughout.
- [ ] Rebind **Open Settings** away from F2; the old F2 action stops and the new launcher-local chord opens Settings.
- [ ] Rebind **Navigate current file manager** away from Ctrl + Enter; the replacement chord navigates the originating Explorer/TC context and Ctrl + Enter no longer triggers that action.
- [ ] Rebind **Copy selected result** away from Ctrl + Shift + C; the replacement chord copies the selected target and normal Ctrl + C still copies selected query text.
- [ ] Attempt to reuse a chord already assigned to another Registry action; Settings rejects it and identifies the conflicting action.
- [ ] Attempt to bind a launcher action to a bare character, Space, Enter/Esc/Tab, arrow/editing key or bare numeric key; Settings rejects it so normal query input remains usable. Verify an unmodified function key such as F8 is accepted.
- [ ] Disable an optional launcher-local action; its chord stops dispatching after closing Settings and remains disabled after restart.
- [ ] Reset one action to default, then use **Reset all hotkeys** and verify all five published defaults are restored.
- [ ] Restart ALTRun Next and verify every customized enabled/disabled state and chord persists.
- [ ] Upgrade an existing schema-3 settings.json with a custom primary/auxiliary binding; schema 4 preserves both global bindings and adds defaults for the three launcher-local actions.
- [ ] After schema-4 migration, record settings.json SHA-256, launch v0.6.0-alpha.5, attempt a settings change, verify newer-schema read-only protection and unchanged SHA-256, then return to alpha.6 and verify the Registry configuration is intact.


## v0.7.0-alpha.9 Native Update validation

Use an installed/extracted alpha.9-or-newer build for these checks. The first transition from an older build still requires one manual download because those binaries do not contain the updater.

- [ ] On a prerelease build, Settings → About defaults the update channel to **Development** after schema-6 → schema-7 migration; a clean future stable build defaults to **Stable**.
- [ ] Disabling automatic checks persists across restart. Enabling it does not trigger more than one automatic network check within 24 hours.
- [ ] **Check for updates** runs asynchronously and leaves Launcher input responsive.
- [ ] Development channel resolves `dev-latest/update-manifest.json`; Stable resolves GitHub's latest non-prerelease release.
- [ ] When the manifest version equals the running version, About reports up to date and **Download and install** is disabled.
- [ ] With a newer Development manifest, About reports the exact version and enables **Download and install**.
- [ ] Download progress updates without blocking Settings or Launcher.
- [ ] Tamper with a downloaded/staged test package in a controlled test build and verify SHA-256 mismatch prevents extraction/apply.
- [ ] A valid update extracts under `data/update/staging/<version>` and verifies `VERSION`, `ALTRunNext.exe` and `ALTRunNext.Updater.exe` before apply.
- [ ] Applying a valid update exits ALTRun Next, stops only its managed Everything client through the normal lifecycle, replaces application files and restarts automatically.
- [ ] Existing `data/settings.json`, `commands.json`, `usage.json`, `provider-cache.json` and `data/tools/Everything` remain intact after update.
- [ ] An install directory writable by the user updates without UAC.
- [ ] A protected install directory requests UAC for the updater only; the restarted ALTRun Next process is not left elevated.
- [ ] Simulate a launch/health failure in a test build and verify the updater restores the previous application files and relaunches the previous version.
- [ ] After successful health confirmation, the backup/staging state is cleaned and the running VERSION matches the requested manifest version.
- [ ] Release assets include `ALTRunNext-x64.zip`, `ALTRunNext-ARM64.zip`, `SHA256SUMS.txt` and `update-manifest.json`; manifest hashes match SHA256SUMS.txt.
