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
- `appearance` — launcher skin and interface language.

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

Starting with v0.4.0-alpha.2, automatic Windows application discovery is cached separately from user configuration.

- contains only provider-generated entries from Start Menu, App Paths, PATH and AppsFolder;
- never stores user-defined shortcuts;
- is loaded during startup so the launcher does not wait for a full Windows application scan;
- is refreshed in the background and replaced atomically after a successful scan;
- uses the same one-generation `.bak` recovery behavior as the other JSON stores;
- can be deleted safely at any time because it is generated state.

The Data -> Rebuild program index action now starts the same background refresh instead of blocking the UI.

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
