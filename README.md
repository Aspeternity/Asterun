# ALTRun Next

ALTRun Next is a clean-room Windows launcher inspired by classic ALTRun: small, keyboard-first, fast, and intentionally low-noise.

The project does **not** copy the original Delphi source. The behavior and visual direction are being reimplemented from scratch for current Windows versions.

## v0.1.1

Current development baseline:

- Native C++23 + Win32
- `Alt + Space` global launcher hotkey
- Two switchable UI styles:
  - **Classic ALTRun** — compact silver/gray layout inspired by the old ALTRun experience
  - **Modern Compact** — cleaner Win11-oriented layout without cards or oversized UI
- Simplified Chinese (`zh-CN`) and English (`en-US`)
- Language and UI can be changed from the tray menu without restarting
- Settings persist in portable `settings.ini`
- Localized search placeholder and system messages
- Custom commands from `commands.tsv`
- Automatic Start Menu shortcut indexing
- Lightweight fuzzy matching
- Usage frequency + recency ranking
- Portable `usage.tsv` history
- Windows 11 per-monitor DPI awareness v2
- x64 and ARM64 GitHub Actions builds

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

The default configuration is:

```ini
[general]
ui=classic
language=zh-CN
```

Changing UI or language writes the values to `settings.ini` next to `ALTRunNext.exe`.

Supported UI values:

- `classic`
- `modern-compact`

Supported language values:

- `zh-CN`
- `en-US`

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

Search, settings, localization and rendering are separated so additional skins such as **Classic Dark**, **Windows 11**, or **Minimal** can be added without rewriting the launcher core.

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

Example:

```text
np      Notepad          notepad.exe
calc    Calculator       calc.exe
gh      GitHub           https://github.com
```

After editing, right-click the tray icon and choose **重新加载 commands.tsv / Reload commands.tsv**.

## Next

The next milestone is to improve Classic visual fidelity and then add pinyin matching + Everything as search providers.
