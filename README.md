# ALTRun Next

ALTRun Next is a clean-room Windows launcher inspired by classic ALTRun: small, keyboard-first, fast, and intentionally low-noise.

## Downloads

### Rolling development build

The latest successful `main` build is always published to the fixed prerelease tag:

- Development release: https://github.com/Aspeternity/ALTRunNext/releases/tag/dev-latest
- x64 direct download: https://github.com/Aspeternity/ALTRunNext/releases/download/dev-latest/ALTRunNext-x64.zip
- ARM64 direct download: https://github.com/Aspeternity/ALTRunNext/releases/download/dev-latest/ALTRunNext-ARM64.zip

You no longer need to find the correct GitHub Actions run. The `dev-latest` release is replaced automatically only after a successful build and test run.

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
- Alt+Space global launcher hotkey
- Custom commands from `commands.tsv`
- Start Menu indexing
- Lightweight fuzzy matching
- Frequency + recency ranking
- x64 / ARM64 GitHub Actions builds

## Build

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
```
