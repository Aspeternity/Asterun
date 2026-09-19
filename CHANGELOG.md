# Changelog

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
