#!/usr/bin/env python3
"""Current release invariants. Historical frozen gates remain in Git history."""

from pathlib import Path
import hashlib
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
SUPPORTED = {
    "0.8.0-beta.3": (11, 2, 2, 22, "0.8.0.10003"),
    "1.0.0": (11, 2, 2, 22, "1.0.0.30000"),
    "1.0.1": (11, 2, 2, 22, "1.0.1.30000"),
    "1.0.2": (11, 2, 2, 22, "1.0.2.30000"),
    "1.0.3": (11, 2, 2, 22, "1.0.3.30000"),
    "1.0.4": (12, 2, 2, 22, "1.0.4.30000"),
    "1.0.5": (12, 2, 2, 22, "1.0.5.30000"),
}


def fail(message: str) -> None:
    raise SystemExit(f"release-contract error: {message}")


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def require(path: str, *tokens: str) -> None:
    contents = read(path)
    for token in tokens:
        if token not in contents:
            fail(f"{path}: required contract missing: {token}")


def cpp_int(path: str, name: str) -> int:
    match = re.search(rf"\b{re.escape(name)}\s*=\s*(\d+)\s*;", read(path))
    if not match:
        fail(f"{path}: {name} missing")
    return int(match.group(1))


version = read("VERSION").strip()
if version not in SUPPORTED:
    fail(f"unregistered release version {version!r}; register and review its contract")
settings, commands, usage, cache, fixed = SUPPORTED[version]
for symbol, expected in (
    ("kSettingsSchemaVersion", settings),
    ("kCommandsSchemaVersion", commands),
    ("kUsageSchemaVersion", usage),
):
    if cpp_int("src/core/ConfigIO.hpp", symbol) != expected:
        fail(f"{symbol} changed without a migration review")
if cpp_int("src/core/ProviderCache.cpp", "kProviderCacheSchemaVersion") != cache:
    fail("provider cache schema changed without a migration review")

require("src/resources.rc", version,
        f"FILEVERSION {fixed.replace('.', ',')}",
        f"PRODUCTVERSION {fixed.replace('.', ',')}")
for path in ("src/app.manifest", "src/uninstaller/uninstaller.manifest"):
    require(path, f'version="{fixed}"')
require("src/uninstaller/uninstaller.manifest", "PerMonitorV2", "longPathAware")
release_validation = (
    "docs/V1.0_RELEASE_VALIDATION.md"
    if version.startswith("1.0.")
    else "docs/V0.8_BETA_VALIDATION.md"
)
for path in ("README.md", "CHANGELOG.md", "ROADMAP.md", release_validation):
    require(path, version)

# A release asset, unlike a source file that is deliberately being optimized,
# must remain byte-identical until its visual baseline is reviewed.
classic_assets = {
    "classic_bg.bmp": "bbced49d20184051cf8ad48b153b022cecfecd50",
    "classic_shortcut.bmp": "eee00956b449975f05e63a38e4da4ea29e01c240",
    "classic_close.bmp": "51732fb285d83c2f13437c26a5f923fec2c33d55",
}
for name, expected in classic_assets.items():
    data = (ROOT / "src/resources" / name).read_bytes()
    actual = hashlib.sha1(f"blob {len(data)}\0".encode() + data).hexdigest()
    if actual != expected:
        fail(f"frozen Classic asset changed: {name}")
subprocess.run([sys.executable, str(ROOT / "scripts/generate_classic_hidpi_assets.py"),
                "--verify"], cwd=ROOT, check=True)

for library in ("nlohmann_json", "cpp_pinyin", "miniz"):
    cmake = read("CMakeLists.txt")
    if not re.search(rf"FetchContent_Declare\(\s*{library}\b[^)]*URL_HASH\s+SHA256=[a-f0-9]{{64}}",
                     cmake, re.DOTALL):
        fail(f"{library}: missing pinned URL_HASH")
require("CMakeLists.txt", "altrun_archive", "miniz-LICENSE.txt")
require("scripts/verify_package.ps1", "miniz-LICENSE.txt", "$allowedTopLevel")
require("THIRD_PARTY_NOTICES.md", "miniz", "miniz-LICENSE.txt")

security_contracts = {
    "src/platform/SecureElevation.cpp": (
        "BCryptGenRandom(", "FILE_FLAG_OPEN_REPARSE_POINT",
        "LockExecutableForElevation(", "LaunchSecuredExecutable("),
    "src/platform/UpdateManager.cpp": (
        "WINHTTP_OPTION_REDIRECT_POLICY_DISALLOW_HTTPS_TO_HTTP",
        "ExtractArchive(", "assetSha256"),
    "src/platform/SecureArchive.cpp": ("LockAndVerifySha256(", "ExtractArchive("),
    "src/platform/EverythingBootstrapper.cpp": (
        "VerifyEverythingPublisher(", "WinVerifyTrust(",
        "CopyManagedEverythingToProtectedHost(", "ExtractArchive("),
    "src/updater/UpdaterTransaction.cpp": ("Rollback(", "RollbackResult"),
    "src/updater/UpdaterMain.cpp": ("recovery.Complete()", "secureReextract"),
    "src/core/ConfigIO.cpp": ("InvalidExisting", "SaveJsonAtomic("),
    "src/core/ConfigIO.hpp": ("primaryRepaired", "JsonValidator"),
    "src/core/ArchiveExtractor.cpp": (
        "mz_zip_reader_extract_to_callback(", "stop_requested()", "PlainPath("),
}
for path, tokens in security_contracts.items():
    require(path, *tokens)

for path, tokens in {
    "tests/ArchiveExtractorTests.cpp": ("escape.zip", "corrupt.zip", "operation_canceled"),
    "tests/ConfigCoreTests.cpp": ("WasRecoveredFromBackup()", "commands-recovered.json"),
    "tests/UpdateRuntimeTests.cpp": ("Rollback(",),
    "tests/UpdatePolicyTests.cpp": ('"0.8.0-beta.3"', '"1.0.0"', '"1.0.1"'),
}.items():
    require(path, *tokens)

for path in (".github/workflows/build.yml", ".github/workflows/release.yml"):
    require(path, "permissions:\n  contents: read", "contents: write",
            "verify_release_contract.py", "verify_package.ps1")
require(
    ".github/workflows/build.yml",
    '.prerelease == ($version | contains("-"))',
)
require("scripts/publish_versioned_release.sh", "Versioned tag $TAG is immutable",
        "--draft", "SHA256SUMS.txt", "update-manifest.json")
forbidden = ("--force", "--cleanup-tag")
for token in forbidden:
    if token in read("scripts/publish_versioned_release.sh"):
        fail(f"immutable release publisher contains {token}")

print(f"{version} security, package and release contract verified")
