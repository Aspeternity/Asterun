# ALTRun Next

ALTRun Next is a clean-room, modern Windows launcher inspired by the interaction model of classic ALTRun: tiny UI, keyboard-first operation, immediate response, and almost no visual noise.

The project does **not** copy the original Delphi source. The UI/behavior is being reimplemented from scratch for current Windows versions.

> Working codename. The final project name can still change before the first public release.

## v0.1.0 prototype

The first development baseline already includes:

- Native C++23 + Win32 implementation
- `Alt + Space` global launcher hotkey
- Classic compact input + result list + command preview layout
- `Up` / `Down` / `Enter` / `Esc` keyboard operation
- Custom commands from `commands.tsv`
- Automatic Start Menu shortcut indexing
- Lightweight fuzzy matching
- Usage frequency + recency ranking
- Portable `usage.tsv` state next to the executable
- Windows 11 per-monitor DPI awareness v2
- System tray: Show / Reload / Exit
- GitHub Actions build design for x64 and ARM64

## Architecture

```text
ALTRunNext
├─ app
│  └─ App                 process lifecycle / orchestration
├─ core
│  ├─ Command             launcher data model
│  ├─ CommandStore        custom + Start Menu command sources
│  ├─ SearchEngine        query matching and ranking
│  └─ UsageStore          frequency / recency persistence
├─ platform
│  └─ WinUtil             UTF-8, environment, Win32 helpers
└─ ui
   └─ LauncherWindow      classic Win32 launcher shell
```

The important rule is that the **search/launch core is separated from the launcher UI**. Later Classic, Classic Dark, or another renderer can be added without rewriting search/indexing.

## Build on Windows 11

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

After editing the file, right-click the tray icon and choose **Reload commands.tsv**.

## Current UI direction

The daily launcher intentionally follows the classic ALTRun idea:

```text
┌────────────────────────────────────────────┐
│ chrome                                     │
├───────────────┬────────────────────────────┤
│ chrome        │ Google Chrome              │
│ chrome beta   │ Google Chrome Beta         │
│ chromedriver  │ ChromeDriver               │
├────────────────────────────────────────────┤
│ C:\...\Google Chrome.lnk                  │
└────────────────────────────────────────────┘
```

No cards, oversized icons, animation-heavy panels, or permanent settings chrome in the launcher itself.

## Next milestone

The next development version will focus on two things before adding more integrations:

1. make the Classic UI visually closer to the old ALTRun skin and interaction details;
2. add pinyin + Everything as providers without coupling them to the UI.

See [ROADMAP.md](ROADMAP.md).
