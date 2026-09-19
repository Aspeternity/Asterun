# Config Core schemas

ALTRun Next now stores live configuration under the portable `data/` directory:

```text
ALTRunNext.exe
data/
├─ settings.json
├─ commands.json
├─ usage.json
└─ provider-cache.json
```

Each document carries its own schema version. As of v0.4.1-alpha.1:

```text
settings.json       schemaVersion 2
commands.json       schemaVersion 1
usage.json          schemaVersion 1
provider-cache.json schemaVersion 2
```

Schema versions are intentionally independent so adding launcher preferences does not force unrelated command or usage migrations.

## Migration

On the first v0.2.0-alpha.1 launch:

- `settings.ini` is read and migrated to `data/settings.json`;
- `commands.tsv` is read and migrated to `data/commands.json`;
- `usage.tsv` is read and migrated to `data/usage.json`;
- the legacy files are **not deleted or renamed**.

User command IDs change from the old derived identifier to a stable UUID. Migrated commands retain the old derived ID in `legacyIds`, allowing usage history to be remapped automatically.

## Atomic writes

JSON saves use:

```text
file.json.tmp
      ↓ validate JSON
file.json.bak  ← previous version
      ↓ atomic replace
file.json
```

If the live JSON is unreadable, ALTRun Next attempts to read the `.bak` copy. Starting with v0.4.0-beta.2, a valid backup also repairs the live primary automatically. A corrupt primary is never copied over a known-good backup.

If `settings.json`, `commands.json` or `usage.json` has a `schemaVersion` newer than the running binary supports, ALTRun Next reads known fields when possible but treats that document as **read-only**. This makes temporary downgrades non-destructive: the older binary does not rewrite the newer document. The Data page lists files currently protected this way.

Starting with v0.4.0-rc.1, each user-data store also remembers when the current startup recovered from a `.bak` file. The Data page reports those recovered files for the rest of the session, even though the primary JSON has already been repaired. Startup also probes whether the portable `data/` directory is writable and warns when changes may not persist.

## settings.json

v0.4.1-alpha.1 upgrades settings to **schemaVersion 2**. A schema-1 settings document is read with the same defaults as v0.4.0 and then rewritten atomically as schema 2. Because v0.4.0 supports settings schema 1 only, temporarily downgrading after the migration places `settings.json` into the existing read-only compatibility mode instead of deleting the new fields.

Schema 2 contains:

- `general` — startup, launcher behavior, tray visibility and monitor placement, including optional show-on-startup;
- `hotkey` — primary global hotkey plus an optional auxiliary hotkey;
- `behavior` — opt-in wildcard matching, Classic numeric quick launch/order and single-result immediate execution;
- `appearance` — launcher skin and interface language;
- `providers` — stable provider IDs mapped to enabled / disabled state.

New schema-2 behavior defaults preserve v0.4.0 behavior: auxiliary hotkey disabled, wildcard matching disabled, numeric quick launch disabled, numeric order `one-to-zero` (1–9,0), single-result immediate execution disabled and show-on-startup disabled. The auxiliary binding defaults to bare `Pause` when enabled and intentionally permits an empty modifier list.

Starting with v0.4.1-alpha.2, all of these schema-2 fields are configurable from the General Settings page. The number-order selector is active only while numeric quick launch is enabled; disabling a feature does not delete its stored companion values.

v0.4.1-alpha.3 does not change the schema. It hardens the runtime/UI contract around these fields: working hotkeys are not churned when Settings opens, failed registrations can still be retried, resume forces revalidation, Restore defaults avoids primary/auxiliary self-conflicts, and the General page can scroll/stack on constrained high-DPI displays.

v0.4.1-beta.1 declares this configuration surface frozen for the remainder of the v0.4.1 Beta/RC cycle. CI now rejects accidental changes to the v0.4.1 schema versions, stable provider IDs or documented default settings. Beta/RC regression fixes therefore do not require a data migration unless the release plan is explicitly reopened.

v0.4.1-beta.2 does not change any schema. Regression coverage now includes a representative v0.4.0 schema-1 settings document and verifies that migration preserves existing preferences while applying safe defaults for new schema-2 fields. The same test simulates an older schema-1 reader and confirms the migrated schema-2 file remains byte-for-byte unchanged under downgrade protection.

v0.4.1-rc.1 also keeps every schema unchanged. RC1 changes only release-candidate validation/publication contracts: tag/VERSION alignment, checksum self-verification and an exact portable-package root allowlist. No migration is performed when moving from beta.2 to RC1.

v0.4.1 Stable keeps the same schema versions and performs no additional migration from RC1. Stable promotion changes only release/version metadata; the schemaVersion 2 downgrade-safety contract remains unchanged.

As of v0.4.0-alpha.3, known provider IDs are:

```text
windows.startmenu
windows.packaged
windows.apppaths
windows.path
```

All four default to enabled. Existing `settings.json` files without a `providers` object therefore keep the same discovery behavior after upgrading.

As of v0.2.0-beta.1, `startWithWindows` and the `hotkey` section are wired to live Windows behavior. A new hotkey is saved only after `RegisterHotKey` succeeds, so a conflicting binding does not overwrite the previous working configuration.

## commands.json

User commands are persistent user data and are now separate from automatically discovered Start Menu entries.

Each command supports:

- stable `id`
- `name`
- primary `keyword`
- `aliases`
- `type`
- target / arguments / working directory
- icon source
- enabled state
- administrator launch flag
- pinned state
- manual sort order
- legacy ID aliases used only for migration

Automatic provider commands are never written into `commands.json`.

## provider-cache.json

Automatic Windows application discovery is cached separately from user configuration.

Starting with v0.4.0-alpha.3, this generated file uses its own **provider-cache schemaVersion 2**. Commands and usage remain schemaVersion 1; settings moves independently to schemaVersion 2 in v0.4.1-alpha.1.

The cache is grouped by stable provider ID:

```json
{
  "schemaVersion": 2,
  "providers": {
    "windows.startmenu": {
      "generatedAtUnix": 1700000000,
      "commands": []
    },
    "windows.packaged": {
      "generatedAtUnix": 1700000000,
      "commands": []
    }
  }
}
```

Properties:

- contains only automatically discovered commands;
- never stores user-defined shortcuts;
- loads before background discovery so startup does not wait for a Windows application scan;
- updates successful providers independently;
- retains the previous cache for a provider whose refresh fails;
- preserves disabled-provider cache entries so re-enabling a source can restore results immediately;
- uses the same atomic write and one-generation `.bak` recovery behavior as the other JSON stores;
- can be deleted safely because it is generated state.

The flat provider-cache schemaVersion 1 written by v0.4.0-alpha.2 is recognized automatically. Its commands are grouped by `CommandSource` in memory and the next successful refresh writes schemaVersion 2.

Starting with v0.4.0-alpha.4, enabled providers also expose lightweight change tokens. A low-frequency monitor compares those tokens and schedules a background refresh only for providers whose source changed. Changes arriving close together are debounced for 750 ms, and source-specific refresh requests are queued if another provider refresh is already running.

Disabled providers remain cached but are excluded from both search and change-token monitoring. Re-enabling a provider restores its cached commands immediately and schedules a targeted background refresh.

The Data -> Rebuild program index action remains an explicit full non-blocking refresh of all enabled providers.

v0.4.0-beta.1 does **not** change the provider-cache schema. De-duplication counts and provider refresh errors are runtime diagnostics only; they are derived from the current cache/refresh session and are not persisted into user configuration.

v0.4.0-beta.2 also keeps provider-cache schemaVersion 2. Cache entries are validated against their stable provider ID, so for example a `source: "path"` command cannot be consumed from the `windows.startmenu` bucket. A future provider-cache schema is ignored and rebuilt because this file is generated state rather than user-authored data.

## usage.json

Usage statistics are keyed by stable command ID:

- launch count
- last-used Unix timestamp

This allows names, keywords and targets to change later without losing ranking history.


## Shortcut TSV interchange — v1

v0.2.0-beta.1 adds a portable TSV import/export format for user shortcuts.

Columns:

```text
keyword    name    aliases    type    target    arguments    workingDirectory    enabled    runAsAdmin    pinned    sortOrder
```

- aliases are comma-separated;
- booleans accept `1/0`, `true/false`, `yes/no` or `on/off`;
- the older five-column `keyword / title / target / arguments / workingDirectory` TSV remains importable;
- legacy ALTRun Beta import also accepts simple `keyword=target` rows as a best-effort compatibility path;
- imported commands receive fresh stable UUIDs and duplicates with the same keyword + target are skipped.
