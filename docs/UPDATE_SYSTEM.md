# Native update system

Asterun v0.7.0-alpha.9 introduces a native portable updater. It is designed around four constraints: update checks stay low-noise, installation is always user-triggered, portable user data is never replaced, and a failed new build can roll back to the previous application files.

## Release source

There is a single update source: versioned stable releases. The client reads `/releases/latest/download/update-manifest.json`, which follows GitHub's latest non-prerelease release. The package itself is then downloaded from that manifest's own release, `/releases/download/v<version>/`, so a release published between the check and the download cannot swap the package under the manifest's SHA-256. A missing manifest is reported as an HTTP failure; there is no fallback to the GitHub API. When the latest release is older than the running build (a prerelease or a local build), the status says so instead of "up to date"; nothing is ever downgraded.

The former Development channel and its rolling `dev-latest` release were removed: their manifest carried the VERSION of `main`, which only changes at a release, so they never offered anything between releases. Builds from `main` are tested locally instead. A persisted `update.channel` value from older versions is ignored and dropped on the next save.

The persisted settings surface is:

```json
"update": {
  "autoCheck": true
}
```

Automatic checks are throttled by `data/update/update-state.json` to at most once per 6 hours. The launcher re-evaluates that throttle at startup and hourly while it stays in the tray, so a new release is normally noticed within about 7 hours. A failed check does not count and is retried at the next hourly evaluation. Each newly found version is announced once in the notification area (`notifiedVersion` in the same file); clicking it opens Settings at About. An automatic check also refreshes an existing, not yet installed offer without hiding it, so a newer release replaces the older one. Runtime status/progress is not persisted into settings.

## Release manifest

CI creates `update-manifest.json` after both architecture packages are built and SHA-256 checked:

```json
{
  "schemaVersion": 1,
  "version": "0.7.0-alpha.9",
  "commit": "<40-character git sha>",
  "prerelease": true,
  "assets": {
    "x64": {
      "name": "Asterun-x64.zip",
      "sha256": "<sha256>"
    },
    "ARM64": {
      "name": "Asterun-ARM64.zip",
      "sha256": "<sha256>"
    }
  }
}
```

Asset names are restricted to a single safe filename. The client chooses the package matching its own architecture.

## Download and staging

The main process uses native WinHTTP over HTTPS. An update ZIP is first stored with a `.download` suffix, SHA-256 is computed with Windows BCrypt, and only a matching package is promoted to the real ZIP filename. The bounded in-process ZIP extractor (path-traversal, entry-count and size limits) writes to:

```text
data/update/staging/<version>/
```

Before installation, the staged tree must contain a matching `VERSION`, `Asterun.exe`, `Update.exe` and `Uninstall.exe`.

## Apply / rollback

`Update.exe` is packaged beside the main executable. Before apply, the main process copies it to `%TEMP%`, launches that temporary copy and exits normally. This lets the helper replace both the main EXE and the packaged updater.

From its start until it launches an Asterun itself, the helper holds Asterun's single-instance guard plus an update-in-progress marker. An Asterun started by hand during that time exits at once and silently, instead of mapping `Asterun.exe` and making the replacement or a rollback fail.

The helper:

1. waits up to 10 minutes for the old Asterun PID to exit (closing a managed Everything can be slow); if it still runs, the update is cancelled before any file changes;
2. validates the staged source again;
3. backs up each existing application file under a unique `data/update/backup/<old-version>-<per-attempt-token>/` path for portable updates (or `.asterun-update-work.<token>/backup/` under the protected installation root for elevated updates);
4. copies the staged application files while explicitly skipping any staged `data/` subtree;
5. restarts the new `Asterun.exe` with a one-shot local health-event name;
6. waits up to 30 seconds for normal startup to signal health, and rolls back at once if the new process exits first;
7. deletes only the current attempt's backup/staging on success, leaving historical recovery directories untouched, and removes files under the packaged `dict/` and `third_party/` trees that the new package no longer ships;
8. on copy, launch or health failure, restores the backed-up files and relaunches the previous build.

An already-existing backup directory is never reused or cleared: the new attempt fails closed, preserving earlier recovery contents. The generated backup path uses an independent random token rather than the health-event token.

The normal Asterun destructor still owns managed Everything shutdown during the update exit, so the updater does not duplicate or bypass the existing Everything lifecycle.

If the installation directory is not writable, only the updater requests UAC. When running elevated, it attempts to create the restarted main process with the normal Explorer user's token so Asterun does not remain elevated.

## Data boundary

The updater treats the portable `data/` directory as user/runtime state, not application payload. It is never overwritten by staged package contents. This preserves settings, shortcuts, usage, provider cache, Managed Everything, update state and future runtime data.

## Security boundary

The current system provides HTTPS transport plus SHA-256 package integrity tied to the CI-generated release manifest. This detects corruption, truncation and a package that does not match the published manifest. It does **not** protect against compromise of the GitHub repository/release credentials that could replace both package and manifest. A future signed-release pipeline should add signature verification before apply.


## Helper names from alpha.9.3

The portable package uses only `Update.exe` and `Uninstall.exe`. The old `Asterun.Updater.exe` filename is not shipped. During the current development cycle, upgrades from alpha.9/alpha.9.1 to alpha.9.2.x are performed manually, so no legacy helper-name compatibility layer is required.

## Native uninstall boundary

`Uninstall.exe` is intentionally separate from normal application exit. Normal exit closes the ALTRun-owned Everything client but leaves the auto-start Everything Service running. Uninstall copies itself to `%TEMP%`, optionally preserves user data, and elevates the temporary worker. In keep-data mode, `data/update` (including F01/F02 historical backups and recovery errors) and unrecognized top-level files including `.asterun-update-work.*` are preserved; only verified packaged program entries are removed. Explicit full removal deletes the installation root and user data. An Everything service is stopped/removed only when its `ImagePath` belongs to the current portable `data/tools/Everything` tree. The global `%ProgramFiles%/Aspeternity/Asterun/EverythingService` host is not bound to an individual portable installation by an ownership marker, so uninstall leaves it and its service untouched and warns the user. Externally managed Everything is never modified.
