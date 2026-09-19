# ALTRun Next

ALTRun Next is a clean-room Windows launcher inspired by classic ALTRun: small, keyboard-first, fast, and intentionally low-noise.

The project does **not** copy the original Delphi source. The behavior and visual direction are being reimplemented from scratch for current Windows versions.

## v0.1.2

The Classic UI has received its first high-fidelity pass while **Modern Compact remains available and unchanged in direction**.

### Classic ALTRun

Classic mode now deliberately recreates the recognizable structure of the old launcher:

- silver/gray vertical gradient shell;
- square Win32 window corners on Windows 11;
- classic, non-themed edit/list borders;
- narrow input box at the upper-left;
- light-gray contextual operation hint at the upper-right;
- rotating hints when the launcher is shown;
- compact white two-column result list;
- numeric prefixes before shortcut keywords;
- classic blue full-row selection;
- fixed divider between shortcut and description columns;
- separate recessed command preview box at the bottom;
- compact 500 px baseline width and 22 px result rows;
- SimSun for Simplified Chinese Classic UI and Tahoma for English Classic UI.

### Modern Compact

Modern Compact keeps the newer visual language:

- wider 620 px layout;
- Win11 rounded window corners;
- modern themed controls;
- larger 32 px result rows;
- no numeric prefix or classic column divider;
- modern blue-accent result rendering.

### Language

- Simplified Chinese (`zh-CN`) — default
- English (`en-US`)
- UI and language can be switched live from the tray menu.

## UI and language

Right-click the tray icon:

```text
显示 / Show
重新加载 commands.tsv / Reload commands.tsv

界面 / Appearance
  ✓ 经典 ALTRun / Classic ALTRun
    现代紧凑 / Modern Compact

语言 / Language
  ✓ 简体中文
    English

退出 / Exit
```

Settings persist to `settings.ini` beside the executable:

```ini
[general]
ui=classic
language=zh-CN
```

Supported UI values:

- `classic`
- `modern-compact`

Supported language values:

- `zh-CN`
- `en-US`

## Current core features

- Native C++23 + Win32
- `Alt + Space` global launcher hotkey
- Custom commands from `commands.tsv`
- Automatic Start Menu shortcut indexing
- Lightweight fuzzy matching
- Usage frequency + recency ranking
- Portable `usage.tsv` history
- Windows 11 per-monitor DPI awareness v2
- x64 and ARM64 GitHub Actions builds

## Architecture

```text
ALTRunNext
├─ app
│  └─ App
├─ core
│  ├─ Command
│  ├─ CommandStore
│  ├─ Localization
│  ├─ SearchEngine
│  ├─ Settings
│  └─ UsageStore
├─ platform
│  └─ WinUtil
└─ ui
   └─ LauncherWindow
```

Search, settings, localization and rendering are separated so additional skins can be added without rewriting the launcher core.

## Build on Windows

Requirements:

- Visual Studio 2022 or newer
- Desktop development with C++ workload
- CMake 3.24+

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
```

Output:

```text
build/Release/ALTRunNext.exe
```

## Portable custom commands

On first launch the app creates `commands.tsv` next to `ALTRunNext.exe`.

Format:

```text
keyword<TAB>title<TAB>target<TAB>arguments<TAB>working_directory
```

After editing, right-click the tray icon and choose **重新加载 commands.tsv / Reload commands.tsv**.

## Next

The next search-focused milestone is pinyin matching + Everything provider integration. Further Classic work will be driven by real side-by-side screenshots and user testing.
