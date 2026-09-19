# Config Core schema — schemaVersion 1

ALTRun Next now stores live configuration under the portable `data/` directory:

```text
ALTRunNext.exe
data/
├─ settings.json
├─ commands.json
├─ usage.json
└─ provider-cache.json
```

Every JSON document starts with:

```json
{
  "schemaVersion": 1
}
```

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

If the live JSON is unreadable, ALTRun Next attempts to read the `.bak` copy.

## settings.json

The first schema contains:

- `general` — startup, launcher behavior, tray visibility and monitor placement;
- `hotkey` — global-hotkey modifiers and key;
- `appearance` — launcher skin and interface language;
- `providers` — stable provider IDs mapped to enabled / disabled state.

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

Starting with v0.4.0-alpha.3, this generated file uses its own **provider-cache schemaVersion 2** even though the user configuration documents remain Config Core schemaVersion 1.

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
