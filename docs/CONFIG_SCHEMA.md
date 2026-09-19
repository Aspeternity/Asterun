# Config Core schema — v0.2.0-alpha.1

ALTRun Next now stores live configuration under the portable `data/` directory:

```text
ALTRunNext.exe
data/
├─ settings.json
├─ commands.json
└─ usage.json
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

The first schema reserves the sections needed by the upcoming Settings UI:

- `general`
- `hotkey`
- `appearance`

Not every stored setting is wired to UI behavior in alpha.1 yet; this release establishes the persistent model first.

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

Automatic Start Menu commands are supplied by `StartMenuProvider` at runtime and are never written into `commands.json`.

## usage.json

Usage statistics are keyed by stable command ID:

- launch count
- last-used Unix timestamp

This allows names, keywords and targets to change later without losing ranking history.
