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
- [ ] Default startup remains silent: launcher is hidden and the tray icon is present.
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
