# Native update system

ALTRun Next v0.7.0-alpha.9 introduces a native portable updater. It is designed around four constraints: update checks stay low-noise, installation is always user-triggered, portable user data is never replaced, and a failed new build can roll back to the previous application files.

## Channels

- **Stable** reads `/releases/latest/download/update-manifest.json`, which follows GitHub's latest non-prerelease release.
- **Development** reads `/releases/download/dev-latest/update-manifest.json`, the rolling release published only after the full main CI succeeds.

Prerelease binaries default to Development. Stable binaries default to Stable. The persisted settings surface is:

```json
"update": {
  "autoCheck": true,
  "channel": "development"
}
```

Automatic checks are throttled by `data/update/update-state.json` to at most once per 24 hours. Runtime status/progress is not persisted into settings.

## Rolling development release publication

`dev-latest` is part of the updater contract, not merely a convenience GitHub Release. The client anonymously reads:

```text
https://github.com/Aspeternity/ALTRunNext/releases/download/dev-latest/update-manifest.json
```

A GitHub Draft Release is invisible at that endpoint and returns HTTP 404 even when authenticated CI can still enumerate the draft. The main workflow therefore treats publication as a verified transaction:

1. release publication is serialized and is not cancelled midway;
2. `origin/main` is rechecked immediately before mutating `dev-latest`;
3. an existing release is found through the authenticated Releases collection, which includes orphan drafts;
4. the rolling tag is forced to the current main commit;
5. an orphan draft is repaired to `draft=false` / `prerelease=true`;
6. package/checksum assets are replaced first and `update-manifest.json` is uploaded last;
7. bootstrap uses an explicit draft followed by an explicit publish PATCH;
8. CI re-validates the authenticated release state/tag, then anonymously downloads the exact `update-manifest.json` endpoint used by installed Development clients;
9. the anonymous manifest must byte-match the locally generated manifest and its version/commit must match VERSION/GITHUB_SHA.

The verifier intentionally does not make a second anonymous `api.github.com` release-metadata request: hosted-runner IPs share GitHub's unauthenticated API rate limit and can receive HTTP 403 even when the release asset is healthy. Draft state is already checked through authenticated release metadata, while an unpublished/inaccessible release still fails the exact anonymous manifest download. Any Draft state, HTTP 404, stale tag, stale manifest or content mismatch fails the workflow.

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
      "name": "ALTRunNext-x64.zip",
      "sha256": "<sha256>"
    },
    "ARM64": {
      "name": "ALTRunNext-ARM64.zip",
      "sha256": "<sha256>"
    }
  }
}
```

Asset names are restricted to a single safe filename. The client chooses the package matching its own architecture.

## Download and staging

The main process uses native WinHTTP over HTTPS. An update ZIP is first stored with a `.download` suffix, SHA-256 is computed with Windows BCrypt, and only a matching package is promoted to the real ZIP filename. Windows Shell ZIP extraction writes to:

```text
data/update/staging/<version>/
```

Before installation, the staged tree must contain a matching `VERSION`, `ALTRunNext.exe`, `Update.exe` and `Uninstall.exe`.

## Apply / rollback

`Update.exe` is packaged beside the main executable. Before apply, the main process copies it to `%TEMP%`, launches that temporary copy and exits normally. This lets the helper replace both the main EXE and the packaged updater.

The helper:

1. waits for the old ALTRun Next PID to exit;
2. validates the staged source again;
3. backs up each existing application file that will be replaced under `data/update/backup/<old-version>`;
4. copies the staged application files while explicitly skipping any staged `data/` subtree;
5. restarts the new `ALTRunNext.exe` with a one-shot local health-event name;
6. waits up to 30 seconds for normal startup to signal health;
7. deletes backup/staging on success;
8. on copy, launch or health failure, restores the backed-up files and relaunches the previous build.

The normal ALTRun Next destructor still owns managed Everything shutdown during the update exit, so the updater does not duplicate or bypass the existing Everything lifecycle.

If the installation directory is not writable, only the updater requests UAC. When running elevated, it attempts to create the restarted main process with the normal Explorer user's token so ALTRun Next does not remain elevated.

## Data boundary

The updater treats the portable `data/` directory as user/runtime state, not application payload. It is never overwritten by staged package contents. This preserves settings, shortcuts, usage, provider cache, Managed Everything, update state and future runtime data.

## Security boundary

The current system provides HTTPS transport plus SHA-256 package integrity tied to the CI-generated release manifest. This detects corruption, truncation and a package that does not match the published manifest. It does **not** protect against compromise of the GitHub repository/release credentials that could replace both package and manifest. A future signed-release pipeline should add signature verification before apply.


## Helper names from alpha.9.3

The portable package uses only `Update.exe` and `Uninstall.exe`. The old `ALTRunNext.Updater.exe` filename is not shipped. During the current development cycle, upgrades from alpha.9/alpha.9.1 to alpha.9.2.x are performed manually, so no legacy helper-name compatibility layer is required.

## Native uninstall boundary

`Uninstall.exe` is intentionally separate from normal application exit. Normal exit closes the ALTRun-owned Everything client but leaves the auto-start Everything Service running. Uninstall copies itself to `%TEMP%`, optionally preserves user data, elevates the temporary worker, and removes the service only when its ImagePath is inside the current portable `data/tools/Everything` tree or matches the known alpha.9.1 detached ALTRun host. External Everything services fail the ownership test and are left untouched.
