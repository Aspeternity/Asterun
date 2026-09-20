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

Each document carries its own schema version. As of v0.7.0-alpha.5.1:

```text
settings.json       schemaVersion 6
commands.json       schemaVersion 2
usage.json          schemaVersion 1
provider-cache.json schemaVersion 2
```

Schema versions are intentionally independent so adding launcher preferences does not force unrelated command or usage migrations.

v0.6.0-alpha.2 introduces only runtime Windows Activation Context and Explorer navigation state. The captured foreground HWND, Explorer view/browser handles and current folder are **session-only** and are never written to settings.json, commands.json, usage.json or provider-cache.json. No schema migration is performed.

v0.6.0-alpha.2.1 keeps that persistence contract unchanged and allows virtual Explorer source locations such as Home / This PC / Quick access. Their source filesystem path is simply absent from the session snapshot; no shell namespace identifier or additional state is persisted.

v0.6.0-alpha.3 adds standard Open / Save / folder-picker activation context, also as runtime-only state. Captured file-dialog HWND/process identifiers are cleared when the launcher hides and are never serialized. No schema migration is performed.

v0.6.0-alpha.4 adds Total Commander activation context and `{folder}` command templates without changing the persisted document shape. Captured TC HWND/process/panel/folder data is session-only. A user command may contain the literal `{folder}` token in target, arguments or workingDirectory; substitution happens in memory from a captured real filesystem folder and the stored command remains unchanged. Commands requiring `{folder}` are omitted from launcher search when no filesystem context is available. commands.json therefore remains schemaVersion 1.

v0.6.0-alpha.5 adds runtime-only clipboard/text actions. `builtin.clipboard`, CopyText payloads and Ctrl+Shift+C copy-selection state are never written to settings.json, commands.json, usage.json or provider-cache.json. The feature writes only the explicitly requested output to the Windows Unicode clipboard and does not read or persist previous clipboard contents. No schema migration is performed.

v0.6.0-alpha.6 upgrades **settings.json to schemaVersion 4** for the centralized Hotkey Registry. The new `hotkeys.bindings` object is keyed by stable action ID and stores only `enabled`, `modifiers` and `key`; scope, display text, validation policy and defaults remain code-owned Registry metadata. The first stable IDs are `launcher.activate`, `launcher.activateSecondary`, `launcher.openSettings`, `result.navigateCurrentFileManager` and `result.copySelectedTarget`.

When a schema-3 document is loaded, its existing primary and auxiliary global hotkeys are imported into the matching Registry actions and launcher-local actions receive their defaults. Starting with v0.6.0-alpha.6.1, existing user global chords have migration priority: if a newly introduced optional launcher-local default duplicates one of them, that new local action is disabled rather than changing the established global chord or keeping two enabled actions with the same binding. The legacy `hotkey` object is still written as a compatibility mirror for the two global activation bindings. A v0.6.0-alpha.5 downgrade therefore sees schema 4, enters the existing read-only newer-schema mode, may read the familiar global fields, and must not rewrite the document. commands.json, usage.json and provider-cache.json are not migrated by alpha.6.

v0.7.0-alpha.2.5 upgrades **settings.json to schemaVersion 5** to persist the new `behavior.pinyinSearch` preference. The default is `true`, preserving existing search behavior. A schema-4 document migrates atomically to schema 5 and receives `pinyinSearch: true`; disabling it prevents ASCII queries from invoking Hanzi-to-pinyin matching and immediately releases any loaded converter/cache. Re-enabling remains lazy. A schema-4 binary opening the migrated file sees a newer schema and enters read-only downgrade protection, so the preference cannot be silently discarded. commands.json, usage.json and provider-cache.json remain unchanged.

v0.7.0-alpha.3 does **not** change any persisted schema. The Shortcut Editor usability/type-selector work is UI/runtime-only: settings remains schemaVersion 5, commands/usage remain schemaVersion 1 and provider-cache remains schemaVersion 2. The four command types were already represented by the existing `CommandType` field; alpha.3 fixes their editor presentation rather than changing stored command data.

v0.7.0-alpha.3.1 also keeps every schema unchanged. The editor combines the stored primary `keyword` and `aliases` into one comma-separated UI field and splits them back into the same existing fields on save. Auto-detected command type is resolved into the existing `type` field; no "auto" value is persisted. The user-facing Pause checkbox is the inverse presentation of the existing `enabled` boolean. A blank `workingDirectory` remains blank in commands.json; at launch time user Application/Command-line shortcuts derive a working directory from an absolute resolved target path, so no new persistence field is required.

v0.7.0-alpha.4 advances **commands.json to schemaVersion 2** for dynamic shortcut input. Each user command now persists `runtimeInputMode` as `none`, `raw` or `url-encoded`. Schema-1 documents migrate through the existing atomic write path. A schema-1 URL command whose target contains the legacy `{query}` token is promoted to `url-encoded`; other existing commands receive `none`. The stored target is not rewritten, so `{query}` remains usable as a compatibility alias, while new shortcuts and examples use `{input}`. A schema-1 reader opening the migrated document sees schema 2 and enters existing read-only downgrade protection. settings.json remains schemaVersion 5, usage.json remains schemaVersion 1 and provider-cache.json remains schemaVersion 2.

v0.7.0-alpha.5 keeps all JSON schemas unchanged. The already-existing `icon` command field becomes user-editable and is rendered in Launcher results; blank editor input persists as `auto`, while a custom path stores the selected icon source. Dynamic **Test input** is editor-session state only and is never persisted. Custom icon paths are included in the atomic Path Conversion workflow. Shortcut TSV interchange advances independently to v3 by appending an optional `icon` column.

v0.7.0-alpha.5.1 advances **settings.json to schemaVersion 6** for the Appearance preference `showResultIcons`. The default is `false`: schema-5 and older documents therefore migrate to a text-first Launcher unless the user explicitly enables result icons afterward. Disabling the preference also bypasses icon resolution/cache/rendering at runtime; it does not delete or rewrite any per-command `icon` value. A schema-5 binary opening the migrated schema-6 settings file enters the existing read-only downgrade-protection path. commands.json remains schemaVersion 2, usage.json remains schemaVersion 1 and provider-cache.json remains schemaVersion 2.

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
- `appearance` — launcher skin, interface language and the default-off `showResultIcons` result-icon preference;
- `providers` — stable provider IDs mapped to enabled / disabled state.

New schema-2 behavior defaults preserve v0.4.0 behavior: auxiliary hotkey disabled, wildcard matching disabled, numeric quick launch disabled, numeric order `one-to-zero` (1–9,0), single-result immediate execution disabled and show-on-startup disabled. The auxiliary binding defaults to bare `Pause` when enabled and intentionally permits an empty modifier list.

Starting with v0.4.1-alpha.2, all of these schema-2 fields are configurable from the General Settings page. The number-order selector is active only while numeric quick launch is enabled; disabling a feature does not delete its stored companion values.

v0.4.1-alpha.3 does not change the schema. It hardens the runtime/UI contract around these fields: working hotkeys are not churned when Settings opens, failed registrations can still be retried, resume forces revalidation, Restore defaults avoids primary/auxiliary self-conflicts, and the General page can scroll/stack on constrained high-DPI displays.

v0.4.1-beta.1 declares this configuration surface frozen for the remainder of the v0.4.1 Beta/RC cycle. CI now rejects accidental changes to the v0.4.1 schema versions, stable provider IDs or documented default settings. Beta/RC regression fixes therefore do not require a data migration unless the release plan is explicitly reopened.

v0.4.1-beta.2 does not change any schema. Regression coverage now includes a representative v0.4.0 schema-1 settings document and verifies that migration preserves existing preferences while applying safe defaults for new schema-2 fields. The same test simulates an older schema-1 reader and confirms the migrated schema-2 file remains byte-for-byte unchanged under downgrade protection.

v0.4.1-rc.1 also keeps every schema unchanged. RC1 changes only release-candidate validation/publication contracts: tag/VERSION alignment, checksum self-verification and an exact portable-package root allowlist. No migration is performed when moving from beta.2 to RC1.

v0.4.1 Stable keeps the same schema versions and performs no additional migration from RC1. Stable promotion changes only release/version metadata; the schemaVersion 2 downgrade-safety contract remains unchanged.

v0.5.0-alpha.1 does not change any persisted schema. The Everything IPC foundation is intentionally transport-only: `settings.json` remains schemaVersion 2, `commands.json` / `usage.json` remain schemaVersion 1, and `provider-cache.json` remains schemaVersion 2. No `everything.filesystem` provider setting is written in alpha.1, and Everything query results are not persisted to provider-cache or usage history. The planned settings schemaVersion 3 migration is deferred until the Settings/provider integration phase.

v0.5.0-alpha.2 still does not bump the persisted schema. It introduces the dynamic provider ID `everything.filesystem`, but the provider is absent from defaults and therefore remains disabled unless an alpha tester explicitly adds `"everything.filesystem": true` to the existing `providers` object. The schemaVersion 2 provider map already preserves boolean provider keys generically, so this experimental opt-in can be read without changing the document shape. File/folder query results remain ephemeral and are not written to `provider-cache.json` or `usage.json`.

The formal v0.5 settings contract is still planned as schemaVersion 3 when Search Sources / Everything diagnostics become user-facing. That later bump is what provides explicit downgrade read-only protection for the complete v0.5 settings surface.

v0.5.0-alpha.3 also keeps every persisted schema unchanged. Unified ranking and Classic mixed-result behavior are runtime-only changes. The experimental `everything.filesystem` opt-in remains a generic schemaVersion 2 provider-map key and is still absent from defaults. No rank score, File/Folder result, dynamic query state or file execution history is persisted.

v0.5.0-beta.1 upgrades `settings.json` to **schemaVersion 3** and makes `everything.filesystem` a formal Search Sources setting. Its default is `false`; the four static Windows application sources keep their previous `true` defaults. Commands remain schemaVersion 1, usage remains schemaVersion 1 and provider-cache remains schemaVersion 2.

Schema-2 settings migrate in place using the existing atomic save path. Because the schema-2 provider map already preserved unknown boolean provider IDs, an alpha user who manually set `"everything.filesystem": true` retains that choice after migration. A schema-2 file without the experimental key receives the new formal default `false`. The running SettingsStore records that this startup migrated an older schema and the source schema version for regression/diagnostic purposes.

Downgrade safety is explicit. A v0.5.0-beta.1 schema-3 settings file opened by a schema-2 reader is returned as `UnsupportedSchema`; known fields may be read for compatibility, but writes are blocked and the original bytes are not replaced. CI exercises this with a simulated v0.4.1 schema ceiling of 2.

The schema-3 change does **not** persist Everything query results, ranking scores, availability, latency or file usage. Those remain runtime-only diagnostics. `everything.filesystem` is still a Dynamic Query Provider and is not written into `provider-cache.json`.
v0.5.0-rc.1 freezes the beta.2 persisted contract for the entire RC line. No schema/default change is permitted during RC stabilization.

v0.5.0-beta.2 does not change any persisted schema or default. It is a transport/runtime compatibility release: settings remains schemaVersion 3, commands/usage remain schemaVersion 1, provider-cache remains schemaVersion 2 and `everything.filesystem` remains default-off. Named-instance endpoint discovery, IPC endpoint diagnostics, long-path handling and reply-validation state are runtime-only and are not persisted.


As of v0.5.0-beta.1, known provider IDs are:

```text
windows.startmenu
windows.packaged
windows.apppaths
windows.path
everything.filesystem
```

The first four are static Catalog providers and default to enabled. `everything.filesystem` is a Dynamic Query Provider and defaults to disabled. It participates only in live query-time search and is never stored in `provider-cache.json`.

As of v0.2.0-beta.1, `startWithWindows` and the `hotkey` section are wired to live Windows behavior. A new hotkey is saved only after `RegisterHotKey` succeeds, so a conflicting binding does not overwrite the previous working configuration.

### v0.6.0-alpha.1 smart-action compatibility

v0.6.0-alpha.1 does **not** change any persisted schema. settings.json remains schemaVersion 3, commands.json and usage.json remain schemaVersion 1, and provider-cache.json remains schemaVersion 2. The five v0.5 provider defaults are unchanged.

The new `builtin.web` identifier is a runtime-only Smart Action source. It is deliberately not written to the settings `providers` map and does not enter provider-cache.json.

Existing user commands with `type: "url"` may opt into web-search alias behavior by placing `{query}` in `target`. The text after the matched keyword or alias is UTF-8 percent-encoded and substituted at query time. Because this reuses existing command fields and does not alter the JSON document shape, commands.json remains schemaVersion 1. URL commands without `{query}` retain their existing execution semantics.

## commands.json

User commands are persistent user data and are now separate from automatically discovered Start Menu entries.

Each command supports:

- stable `id`
- `name`
- primary `keyword`
- `aliases`
- `type`
- target / arguments / working directory
- runtime input mode (`none`, `raw`, `url-encoded`)
- icon source
- enabled state
- administrator launch flag
- pinned state
- manual sort order
- legacy ID aliases used only for migration

Automatic provider commands are never written into `commands.json`.

Starting with v0.6.0-alpha.4, user commands may use `{folder}` in `target`, `arguments` or `workingDirectory`. This is a runtime template, not a schema field. It resolves only from a real filesystem folder captured when ALTRun Next is invoked from File Explorer or Total Commander.

Starting with v0.7.0-alpha.4, a command with runtime input enabled may use `{input}` in `target`, `arguments` or `workingDirectory`. `raw` replaces the token unchanged; `url-encoded` replaces it with UTF-8 percent-encoded text. Application and Command-line shortcuts without a placeholder append the resolved input to fixed arguments. URL and Folder shortcuts require a placeholder. The legacy `{query}` token remains accepted as an alias for `{input}` so older web-search shortcuts keep working.

## provider-cache.json

Automatic Windows application discovery is cached separately from user configuration.

Starting with v0.4.0-alpha.3, this generated file uses its own **provider-cache schemaVersion 2**. Commands and usage remain schemaVersion 1. Settings moved independently to schemaVersion 2 in v0.4.1-alpha.1 and schemaVersion 3 in v0.5.0-beta.1.

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


## Shortcut TSV interchange — v3

v0.7.0-alpha.5 appends the optional `icon` column to the v2 runtime-input format. Existing v1/v2 rows remain importable; a missing or blank icon defaults to `auto`.

Columns:

```text
keyword    name    aliases    type    target    arguments    workingDirectory    enabled    runAsAdmin    pinned    sortOrder    runtimeInputMode    icon
```

- aliases are comma-separated;
- `runtimeInputMode` accepts `none`, `raw` or `url-encoded`;
- `icon` accepts `auto` or a path to an icon source such as `.ico`, `.exe`, `.dll` or `.lnk`;
- booleans accept `1/0`, `true/false`, `yes/no` or `on/off`;
- v1 eleven-column rows, v2 twelve-column rows and the older five-column `keyword / title / target / arguments / workingDirectory` TSV remain importable;
- legacy ALTRun Beta import also accepts simple `keyword=target` rows as a best-effort compatibility path;
- imported commands receive fresh stable UUIDs and duplicates with the same keyword + target are skipped.
