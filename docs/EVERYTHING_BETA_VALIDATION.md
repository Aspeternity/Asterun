# ALTRun Next v0.5.0 Beta Everything Validation

This checklist covers the real-desktop validation that complements automated v0.5.0 Everything IPC, ranking, schema migration and package tests. Checkboxes remain intentionally unmarked until observed on a real machine.

## Required environments

- [ ] Windows 10 x64 with standard Everything 1.4.x.
- [ ] Windows 11 x64 with standard Everything 1.4.x.
- [ ] Windows 11 x64 with current Everything 1.5 beta when practical.
- [ ] Windows ARM64 physical device when available.
- [ ] 100%, 125%, 150% and 200% display scaling for the Settings Search Sources page.

Record OS build, Everything version/edition, ALTRun Next package architecture and package SHA-256 for each run.

## Clean install / schema 3

- [ ] A clean data directory creates `settings.json` with schemaVersion 3.
- [ ] Start Menu, Windows Apps, App Paths and PATH default to enabled.
- [ ] Everything files & folders defaults to disabled.
- [ ] Search Sources exposes the Everything row without changing Classic launcher geometry.
- [ ] Enabling/disabling Everything persists across ALTRun Next restart.

## Schema 2 -> schema 3 migration

- [ ] Upgrade an alpha schema-2 file with no `everything.filesystem` key; schema becomes 3 and Everything remains disabled.
- [ ] Upgrade an alpha schema-2 file with `"everything.filesystem": true`; schema becomes 3 and Everything remains enabled.
- [ ] Existing General/Appearance/Hotkey/provider choices remain unchanged.
- [ ] The pre-migration file is available through the normal atomic-write backup behavior.

## Downgrade protection

- [ ] After beta migration, open the same portable data directory with a v0.4.1-era schema-2 build.
- [ ] The older build reports settings.json as a newer schema/read-only compatibility file.
- [ ] The schema-3 settings file remains byte-for-byte unchanged.
- [ ] Reopen with v0.5.0 beta; the Everything choice and all prior settings are intact.

## Everything availability and fallback

- [ ] With Everything disabled, application/user-command search behaves exactly as before.
- [ ] Enable Everything while standard Everything is not running; Search Sources reports IPC unavailable and application-search fallback active.
- [ ] Static application/user-command results remain responsive while IPC is unavailable.
- [ ] Start Everything without restarting ALTRun Next; the status changes to IPC available and a later query returns File/Folder results.
- [ ] Stop Everything while ALTRun Next remains running; subsequent query falls back without hanging the launcher.
- [ ] Restart Everything; a later query recovers without restarting ALTRun Next.
- [ ] Everything Lite is shown as unavailable because it has no IPC.
- [ ] ALTRun Next never starts Everything automatically.

## Diagnostics

- [ ] Search Sources updates Everything availability while the page remains open.
- [ ] After a successful query, last-query status, returned/total count and latency are plausible.
- [ ] A reply-timeout test reports timeout without blocking UI input.
- [ ] When a native Windows IPC error exists, the native error value is displayed.
- [ ] Leaving Search Sources stops its periodic status refresh; reopening it resumes refresh.

## Search and execution regression

- [ ] Exact File/Folder matches participate in the alpha.3 unified ranking as expected.
- [ ] Static duplicate application targets remain authoritative over an Everything duplicate.
- [ ] Enter/double-click opens files with the Windows default application and folders through Explorer/Shell.
- [ ] Numeric quick launch operates on the final mixed result order.
- [ ] Single-result immediate execution waits for the current dynamic query to settle.
- [ ] Hiding the launcher or manually executing a result prevents a late dynamic reply from launching a second item.
- [ ] Everything File/Folder activity does not add entries to `provider-cache.json` or file paths to `usage.json`.

## Packaging

- [ ] x64 ZIP starts from a clean writable directory.
- [ ] ARM64 ZIP contains the same portable contract.
- [ ] No Everything DLL/executable is shipped.
- [ ] SHA256SUMS verifies both ZIP assets.
