# Changelog

## 0.1.2

- Reworked Classic ALTRun mode for much higher visual fidelity.
- Added a silver/gray vertical gradient launcher shell.
- Added a narrow upper-left input field and upper-right operation hint area.
- Added rotating contextual hints that match the classic interaction style.
- Added numeric shortcut prefixes in Classic result rows.
- Added a fixed two-column shortcut/description layout and divider.
- Added the classic full-row blue selection treatment.
- Restyled the command preview as a separate recessed bottom box.
- Switched Classic Chinese typography to SimSun and Classic English to Tahoma.
- Disabled modern control theming in Classic mode.
- Forced square Classic corners on Windows 11 while retaining rounded Modern Compact corners.
- Preserved the Modern Compact layout and rendering path.
- Bumped application, manifest and Windows resource version to 0.1.2.

## 0.1.1

- Added persistent portable `settings.ini`.
- Added Simplified Chinese and English UI strings.
- Added live language switching from the tray menu.
- Added **Classic ALTRun** and **Modern Compact** switchable UI styles.
- Added localized search placeholder and application messages.
- Added theme-specific geometry, fonts, colors and result rendering.
- Kept search/indexing core independent from UI and localization.
- Packaged a `settings.example.ini` with CI artifacts.

## 0.1.0 - development baseline

- Created a clean-room C++23/Win32 project architecture.
- Added classic compact launcher UI shell.
- Added global Alt+Space activation.
- Added custom TSV commands and Start Menu indexing.
- Added lightweight fuzzy search and usage-based ranking.
- Added per-monitor DPI v2 support and tray controls.
- Added portable history storage.
- Added x64/ARM64 CI and tagged-release workflows.
