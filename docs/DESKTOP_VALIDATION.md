# ALTRun Next v0.4.1 Desktop Validation

This is the release-validation checklist for the frozen v0.4.1 feature set. Beta and RC builds may fix regressions or compatibility problems, but they must not introduce a new v0.4.1 feature surface.

For RC builds, the existence of a candidate tag means the automated release contract passed; it does **not** mean the manual items below were observed. Stable promotion requires explicit manual sign-off with no release-blocking defect.

Automated CI covers compilation, Config/Search tests, provider smoke tests, hotkey codec tests, real RegisterHotKey conflict/re-registration smoke, Classic numeric/single-result behavior logic, 100%/125%/150%/200% General-layout invariants, Windows API compatibility, package contents/version metadata and a packaged x64 startup smoke. Assertion-based tests are forced to remain active even in Release builds.

The checks below are the remaining **real interactive Windows desktop** validation items. Automated geometry/behavior tests reduce regression risk but do not replace observing the actual UI, IME, monitor transitions, providers and hotkey lifecycle on a real desktop.

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
