# First-reveal IME diagnosis (opt-in)

This is a **diagnostic only**, not a fix. The default-English preference and
normal launcher behavior are unchanged by this diagnostic.

## Reproduce on a real Windows machine

1. Exit Asterun **completely** from its tray menu. Ensure no Asterun process
   remains.
2. Extract the x64 artifact from the latest **PR #62 Build** to a separate
   writable test directory. Launch PowerShell in that directory.
3. Select Microsoft Pinyin / Chinese input in the application from which you
   intend to summon Asterun.
4. Execute:

   ~~~powershell
   $env:ASTERUN_IME_TRACE = '1'
   Start-Process -FilePath '.\Asterun.exe'
   Remove-Item Env:\ASTERUN_IME_TRACE
   ~~~

5. On the **first** Alt+Space reveal, type a short word and note whether
   candidate suggestions appear. Press Esc.
6. Reveal a **second** time and repeat. Press Esc.
7. The passive trace is saved as
   \`$env:TEMP\Asterun-ime-trace-<pid>-<uptime>.log\`.

   ~~~powershell
   Get-ChildItem "$env:TEMP\Asterun-ime-trace-*.log" |
     Sort-Object LastWriteTime -Descending |
     Select-Object -First 1 FullName,LastWriteTime
   ~~~

Share the **newest** trace and describe first/second observed behavior.
Optionally repeat with Classic and Modern Compact styles.

## Interpretation / safety

- Each \`reveal=N\` contains startup, foreground handoff, EDIT focus,
  IMM override and IME messages in chronological \`seq=\` order.
- \`edit_focus=1\` and \`foreground=1\` indicate the target was active when
  sampled. \`open=1\` is IME open status; \`conv_native=1\` indicates
  native-language conversion mode, when supported.
- A \`conv_ok=0\` / absent HIMC **does not mean English**. It means IMM cannot
  report that state. TSF-only cases need a separate follow-up investigation.
- The logger **never records typed content or virtual-key codes**, nor reads
  clipboard or search queries.
- The log is created/appended **only on Hide/exit**, not while drawing the
  first launcher frame or handling typing. All samples are buffered, bounded
  and enabled **only** when the environment variable is set on startup.
- The trace does not create a TSF thread manager or change keyboard layout
  (doing so could perturb the timing under investigation).
- Delete diagnostic logs from Temp when the investigation is complete.
