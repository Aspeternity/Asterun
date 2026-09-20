#!/usr/bin/env python3

from pathlib import Path
import json
import re
import sys

ROOT = Path(__file__).resolve().parents[1]


def fail(message: str) -> None:
    print(f"release-contract error: {message}", file=sys.stderr)
    raise SystemExit(1)


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def cpp_int(path: str, name: str) -> int:
    text = read(path)
    match = re.search(rf"\b{re.escape(name)}\s*=\s*(\d+)\s*;", text)
    if not match:
        fail(f"{name} was not found in {path}")
    return int(match.group(1))


version = read("VERSION").strip()
match = re.fullmatch(
    r"(\d+)\.(\d+)\.(\d+)(?:-(alpha|beta|rc)\.(\d+)(?:\.(\d+))?)?",
    version,
)
if not match:
    fail(f"unsupported VERSION: {version!r}")

base = ".".join(match.group(1, 2, 3))
channel = match.group(4)

if version in ("0.7.0-alpha.2", "0.7.0-alpha.2.1", "0.7.0-alpha.2.2", "0.7.0-alpha.2.3", "0.7.0-alpha.2.4", "0.7.0-alpha.2.5", "0.7.0-alpha.2.6", "0.7.0-alpha.3", "0.7.0-alpha.3.1", "0.7.0-alpha.4", "0.7.0-alpha.5", "0.7.0-alpha.5.1", "0.7.0-alpha.5.2", "0.7.0-alpha.6", "0.7.0-alpha.7", "0.7.0-alpha.8"):
    expected_settings_schema = 6 if version in ("0.7.0-alpha.5.1", "0.7.0-alpha.5.2", "0.7.0-alpha.6", "0.7.0-alpha.7", "0.7.0-alpha.8") else (5 if version in ("0.7.0-alpha.2.5", "0.7.0-alpha.2.6", "0.7.0-alpha.3", "0.7.0-alpha.3.1", "0.7.0-alpha.4", "0.7.0-alpha.5") else 4)
    expected_commands_schema = 2 if version in ("0.7.0-alpha.4", "0.7.0-alpha.5", "0.7.0-alpha.5.1", "0.7.0-alpha.5.2", "0.7.0-alpha.6", "0.7.0-alpha.7", "0.7.0-alpha.8") else 1
    expected_schemas = {
        "kSettingsSchemaVersion": expected_settings_schema,
        "kCommandsSchemaVersion": expected_commands_schema,
        "kUsageSchemaVersion": 1,
    }
    for name, expected in expected_schemas.items():
        actual = cpp_int("src/core/ConfigIO.hpp", name)
        if actual != expected:
            fail(f"{name}={actual}, expected v0.7 alpha.2 value {expected}")

    if cpp_int("src/core/ProviderCache.cpp", "kProviderCacheSchemaVersion") != 2:
        fail("v0.7 alpha.2 must keep provider-cache schemaVersion 2")

    settings = json.loads(read("config/settings.example.json"))
    expected_providers = {
        "windows.startmenu": True,
        "windows.packaged": True,
        "windows.apppaths": True,
        "windows.path": True,
        "everything.filesystem": False,
    }
    if settings.get("schemaVersion") != expected_settings_schema:
        fail(
            f"v0.7 alpha.2 settings schema mismatch: "
            f"{settings.get('schemaVersion')} != {expected_settings_schema}"
        )
    if settings.get("providers") != expected_providers:
        fail("v0.7 alpha.2 changed frozen provider defaults")

    expected_bindings = {
        "launcher.activate",
        "launcher.activateSecondary",
        "launcher.openSettings",
        "result.navigateCurrentFileManager",
        "result.copySelectedTarget",
    }
    bindings = settings.get("hotkeys", {}).get("bindings", {})
    if set(bindings) != expected_bindings:
        fail("v0.7 alpha.2 changed frozen Hotkey Registry action IDs")

    manager = read("src/ui/ShortcutManagerWindow.cpp") + read("src/ui/ShortcutManagerWindow.hpp")
    for token in (
        "kIdPathConversion",
        "pathConversion_",
        "ConvertPaths",
        "ShortcutPathConverterDialog::Show",
        'T(L"路径转换...", L"Path conversion...")',
        "LVCF_TEXT",
    ):
        if token not in manager:
            fail(f"v0.7 alpha.2 Shortcut Manager usability contract missing: {token}")

    for forbidden in (
        "kIdMoveUp",
        "kIdMoveDown",
        "moveUp_",
        "moveDown_",
        "MoveSelected(",
        'T(L"上移", L"Move up")',
        'T(L"下移", L"Move down")',
    ):
        if forbidden in manager:
            fail(f"v0.7 alpha.2 Shortcut Manager still exposes manual ordering: {forbidden}")

    converter = read("src/ui/ShortcutPathConverterDialog.cpp") + read("src/ui/ShortcutPathConverterDialog.hpp")
    for token in (
        "Mode::Portable",
        "Mode::Absolute",
        "MakePortablePath",
        "ExpandPortablePath",
        "ApplyUserCommandPathUpdates",
        "LVS_EX_CHECKBOXES",
        "Arguments、URL、UNC",
    ):
        if token not in converter:
            fail(f"v0.7 alpha.2 path conversion UI contract missing: {token}")

    winutil = read("src/platform/WinUtil.cpp") + read("src/platform/WinUtil.hpp")
    for token in (
        "ResolvePortablePath",
        "MakePortablePath",
        "ExpandPortablePath",
        "IsUncPath",
        "bareRelativeIsPath",
        "LOCALAPPDATA",
        "USERPROFILE",
        "WINDIR",
        "ProgramFiles",
    ):
        if token not in winutil:
            fail(f"v0.7 alpha.2 portable path runtime contract missing: {token}")

    app = read("src/app/App.cpp") + read("src/app/App.hpp")
    for token in (
        "ApplyUserCommandPathUpdates",
        "ResolvePortablePath",
        "baseDirectory_",
        "resolved.arguments",
    ):
        if token not in app:
            fail(f"v0.7 alpha.2 App path integration missing: {token}")

    store = read("src/core/UserCommandStore.cpp") + read("src/core/UserCommandStore.hpp")
    for token in (
        "UserCommandPathUpdate",
        "ApplyPathUpdates",
        "const auto previous = commands_",
        "if (!Save())",
    ):
        if token not in store:
            fail(f"v0.7 alpha.2 atomic path update contract missing: {token}")

    if "sortOrder" not in read("src/core/Command.hpp"):
        fail("v0.7 alpha.2 removed sortOrder compatibility field")

    cmake = read("CMakeLists.txt")
    for token in (
        "src/ui/ShortcutPathConverterDialog.cpp",
        "path_portability_tests",
        "user_command_path_update_tests",
    ):
        if token not in cmake:
            fail(f"v0.7 alpha.2 CMake contract missing: {token}")

    workflow = read(".github/workflows/build.yml")
    for token in (
        "path_portability_tests",
        "user_command_path_update_tests",
    ):
        if workflow.count(token) < 4:
            fail(f"v0.7 alpha.2 Windows CI gate missing: {token}")

    path_tests = read("tests/PathPortabilityTests.cpp")
    for token in (
        "notepad.exe",
        "..\\Tools\\Demo\\demo.exe",
        "IsUncPath",
        "%WINDIR%",
        "MakePortablePath",
        "ExpandPortablePath",
    ):
        if token not in path_tests:
            fail(f"v0.7 alpha.2 path portability test coverage missing: {token}")

    batch_tests = read("tests/UserCommandPathUpdateTests.cpp")
    for token in (
        "ApplyPathUpdates",
        "missing-command-id",
        "ReadAll(jsonPath) == beforeFailure",
    ):
        if token not in batch_tests:
            fail(f"v0.7 alpha.2 atomic path update test coverage missing: {token}")

    launcher_header = read("src/ui/LauncherWindow.hpp")
    for name, expected in {
        "widthLogical_": 420,
        "rowHeightLogical_": 16,
        "maxResults_": 10,
    }.items():
        found = re.search(rf"\b{re.escape(name)}\s*\{{(\d+)\}}", launcher_header)
        if not found or int(found.group(1)) != expected:
            fail(f"Classic geometry changed during v0.7 alpha.2: {name}")

    if version in ("0.7.0-alpha.6", "0.7.0-alpha.7", "0.7.0-alpha.8"):
        editor = read("src/ui/ShortcutEditorDialog.cpp") + read("src/ui/ShortcutEditorDialog.hpp")
        manager = read("src/ui/ShortcutManagerWindow.cpp") + read("src/ui/ShortcutManagerWindow.hpp")
        model = read("src/core/ShortcutEditorModel.cpp") + read("src/core/ShortcutEditorModel.hpp")
        store = read("src/core/UserCommandStore.cpp")
        model_test = read("tests/ShortcutEditorModelTests.cpp")
        schema_test = read("tests/UserCommandSchemaTests.cpp")

        for forbidden in (
            "paused_",
            "pinned_",
            "kIdPaused",
            "kIdPinned",
            'T(L"暂停此快捷项",',
            'T(L"置顶",',
        ):
            if forbidden in editor:
                fail(f"v0.7 alpha.6 still exposes pause/pin shortcut UI: {forbidden}")

        for token in (
            "FindShortcutKeywordConflict",
            "ShortcutMatchesFilter",
            "ShortcutKeywordConflict",
        ):
            if token not in model:
                fail(f"v0.7 alpha.6 shortcut integrity model missing: {token}")

        for token in (
            "kIdFilter",
            "filter_",
            "EM_SETCUEBANNER",
            "ShortcutMatchesFilter",
            "FormatShortcutKeywords",
        ):
            if token not in manager:
                fail(f"v0.7 alpha.6 Shortcut Manager completion missing: {token}")

        for token in (
            "command.enabled = true;",
            "command.pinned = false;",
            "legacyEnabled",
            "legacyPinned",
        ):
            if token not in store:
                fail(f"v0.7 alpha.6 legacy pause/pin normalization missing: {token}")

        for token in (
            "FindShortcutKeywordConflict",
            "ShortcutMatchesFilter",
            'L"BROWSER"',
        ):
            if token not in model_test:
                fail(f"v0.7 alpha.6 shortcut model regression coverage missing: {token}")

        for token in (
            "commands-legacy-flags.json",
            ".enabled",
            ".pinned",
        ):
            if token not in schema_test:
                fail(f"v0.7 alpha.6 legacy flag regression coverage missing: {token}")

    if version in ("0.7.0-alpha.7", "0.7.0-alpha.8"):
        context_policy = read("src/core/ContextActions.cpp") + read("src/core/ContextActions.hpp")
        context_test = read("tests/ContextActionsTests.cpp")
        launcher = read("src/ui/LauncherWindow.cpp") + read("src/ui/LauncherWindow.hpp")
        manager = read("src/ui/ShortcutManagerWindow.cpp") + read("src/ui/ShortcutManagerWindow.hpp")
        editor = read("src/ui/ShortcutEditorDialog.cpp") + read("src/ui/ShortcutEditorDialog.hpp")
        shell_actions = read("src/platform/ShellActions.cpp") + read("src/platform/ShellActions.hpp")
        cmake = read("CMakeLists.txt")

        for token in (
            "LauncherContextActions",
            "EvaluateLauncherContextActions",
            "CanRevealTargetInExplorer",
            "ShortcutSeedFromLauncherResult",
            "addAsShortcut",
            "navigateCurrentFileManager",
        ):
            if token not in context_policy:
                fail(f"v0.7 alpha.7 context-action policy missing: {token}")

        for token in (
            "ShowResultContextMenu",
            "WM_CONTEXTMENU",
            "TrackPopupMenuEx",
            "ShortcutEditorDialog::ShowNew",
            "ShortcutSeedFromLauncherResult",
            "RevealInExplorer",
            "kResultContextDeleteShortcut",
            "contextActionModalActive_",
        ):
            if token not in launcher:
                fail(f"v0.7 alpha.7 Launcher context action missing: {token}")

        for token in (
            "ShowContextMenu",
            "WM_CONTEXTMENU",
            "kShortcutContextEdit",
            "kShortcutContextTest",
            "kShortcutContextLocate",
            "kShortcutContextCopy",
            "kShortcutContextDelete",
            "kShortcutContextAdd",
            "CopySelectedTarget",
        ):
            if token not in manager:
                fail(f"v0.7 alpha.7 Shortcut Manager context action missing: {token}")

        for token in (
            "ShowNew",
            "const Command& seed",
            "const Command* seed",
            "BeginNew(",
        ):
            if token not in editor:
                fail(f"v0.7 alpha.7 pre-filled Shortcut Editor missing: {token}")

        for token in (
            "RevealInExplorer",
            "SearchPathW",
            'L"/select,\\\""',
        ):
            if token not in shell_actions:
                fail(f"v0.7 alpha.7 Shell reveal contract missing: {token}")

        for token in (
            "context_actions_tests",
            "src/core/ContextActions.cpp",
            "src/platform/ShellActions.cpp",
        ):
            if token not in cmake:
                fail(f"v0.7 alpha.7 build/test wiring missing: {token}")

        for token in (
            "ResultKind::UserCommand",
            "ResultKind::Folder",
            "shell:AppsFolder",
            "ShortcutSeedFromLauncherResult",
            "CanRevealTargetInExplorer",
        ):
            if token not in context_test:
                fail(f"v0.7 alpha.7 context-action regression coverage missing: {token}")

    if version == "0.7.0-alpha.8":
        bootstrap_policy = read("src/core/EverythingBootstrapPolicy.cpp") + read("src/core/EverythingBootstrapPolicy.hpp")
        bootstrap_test = read("tests/EverythingBootstrapPolicyTests.cpp")
        bootstrapper = read("src/platform/EverythingBootstrapper.cpp") + read("src/platform/EverythingBootstrapper.hpp")
        app = read("src/app/App.cpp") + read("src/app/App.hpp")
        settings_ui = read("src/ui/SettingsWindow.cpp") + read("src/ui/SettingsWindow.hpp")
        cmake = read("CMakeLists.txt")

        for token in (
            "EverythingPackageSpec",
            'L"1.4.1.1032"',
            'L".x64.zip"',
            'L".ARM64.zip"',
            'L".sha256"',
            "FindSha256ForFile",
            "IsSha256Hex",
        ):
            if token not in bootstrap_policy:
                fail(f"v0.7 alpha.8 Everything package policy missing: {token}")

        for forbidden in (
            '.x64.Lite.zip',
            '.ARM64.Lite.zip',
        ):
            if forbidden in bootstrap_policy:
                fail(f"v0.7 alpha.8 must never select Everything Lite: {forbidden}")

        for token in (
            "FindExistingCandidates",
            "App Paths\\\\Everything.exe",
            'L"ProgramFiles"',
            "SearchPathW",
            "WinHttpOpen",
            "WinHttpCrackUrl",
            "BCryptOpenAlgorithmProvider",
            "BCRYPT_SHA256_ALGORITHM",
            "CopyHere",
            'L".download"',
            "-startup -first-instance",
            "WaitForIpc",
            "allowDownload",
            "NeedsInstall",
        ):
            if token not in bootstrapper:
                fail(f"v0.7 alpha.8 managed Everything bootstrap missing: {token}")

        for forbidden in (
            "powershell",
            "PowerShell",
            "Invoke-WebRequest",
            "Expand-Archive",
        ):
            if forbidden in bootstrapper:
                fail(f"v0.7 alpha.8 bootstrap must stay native: {forbidden}")

        for token in (
            "StartEverythingBootstrap",
            "EverythingBootstrapStatus",
            "everythingBootstrapThread_",
            "everythingBootstrapMutex_",
            "kEverythingBootstrapMessage",
            "HandleEverythingBootstrapCompleted",
            "StartEverythingBootstrap(false)",
        ):
            if token not in app:
                fail(f"v0.7 alpha.8 App bootstrap lifecycle missing: {token}")

        for token in (
            "AcquireEverything",
            "RecheckEverything",
            'T(L"获取并启动 Everything",',
            "StartEverythingBootstrap(\n            true)",
            "StartEverythingBootstrap(false)",
            "DownloadingPackage",
            "VerifyingPackage",
            "ExtractingPackage",
            "WaitingForIpc",
            "SHA-256",
        ):
            if token not in settings_ui:
                fail(f"v0.7 alpha.8 Everything onboarding UX missing: {token}")

        for forbidden in (
            "OpenEverythingDownloadPage",
            "ALTRun Next 不内置或自动启动 Everything",
        ):
            if forbidden in settings_ui:
                fail(f"v0.7 alpha.8 obsolete Everything handoff remains: {forbidden}")

        for token in (
            "Everything-1.4.1.1032.x64.zip",
            "Everything-1.4.1.1032.ARM64.zip",
            "FindSha256ForFile",
            'find(L"Lite")',
        ):
            if token not in bootstrap_test:
                fail(f"v0.7 alpha.8 package-policy regression coverage missing: {token}")

        for token in (
            "src/core/EverythingBootstrapPolicy.cpp",
            "src/platform/EverythingBootstrapper.cpp",
            "everything_bootstrap_policy_tests",
            "tests/EverythingBootstrapPolicyTests.cpp",
            "winhttp",
            "bcrypt",
        ):
            if token not in cmake:
                fail(f"v0.7 alpha.8 build/test wiring missing: {token}")

    if version == "0.7.0-alpha.2.1":
        grouped_converter = read("src/ui/ShortcutPathConverterDialog.cpp")
        grouped_header = read("src/ui/ShortcutPathConverterDialog.hpp")
        for token in (
            "convertibleShortcutCount",
            "firstPreviewForCommand",
            "commandHasPreview",
            "groupTitle",
            'firstPreviewForCommand\n                    ? groupTitle\n                    : L""',
        ):
            if token not in grouped_converter:
                fail(f"v0.7 alpha.2.1 grouped path preview missing: {token}")

        if "std::wstring shortcut;" in grouped_header:
            fail("v0.7 alpha.2.1 still stores redundant per-row shortcut text")
        for forbidden in (
            "ListView_EnableGroupView",
            "ListView_InsertGroup",
            "LVIF_GROUPID",
        ):
            if forbidden in grouped_converter:
                fail(f"v0.7 alpha.2.1 unexpectedly requires Common Controls v6 grouping: {forbidden}")

    if version == "0.7.0-alpha.2.2":
        grouped_converter = read("src/ui/ShortcutPathConverterDialog.cpp")
        grouped_header = read("src/ui/ShortcutPathConverterDialog.hpp")

        for token in (
            "kGroupHeaderItemParam",
            "InsertGroupHeader",
            "InsertPreviewRow",
            "IsGroupHeaderItem",
            "RowIndexForListItem",
            "HandleListCustomDraw",
            "NM_CUSTOMDRAW",
            "LVN_ITEMCHANGING",
            "CDRF_SKIPDEFAULT",
            "FW_SEMIBOLD",
            "GetSysColorBrush",
            "ListView_GetItemCount",
            "std::array<int, 4> widths",
        ):
            if token not in grouped_converter and token not in grouped_header:
                fail(f"v0.7 alpha.2.2 true grouping contract missing: {token}")

        if 'T(L"快捷项", L"Shortcut")' in grouped_converter:
            fail("v0.7 alpha.2.2 still exposes a redundant Shortcut data column")

        for forbidden in (
            "ListView_EnableGroupView",
            "ListView_InsertGroup",
            "LVIF_GROUPID",
        ):
            if forbidden in grouped_converter:
                fail(f"v0.7 alpha.2.2 unexpectedly depends on Common Controls v6 group view: {forbidden}")

        manifest = read("src/app.manifest")
        if "Microsoft.Windows.Common-Controls" in manifest:
            fail("v0.7 alpha.2.2 unexpectedly changes the global Common Controls manifest")

    if version in ("0.7.0-alpha.2.3", "0.7.0-alpha.2.4", "0.7.0-alpha.2.5", "0.7.0-alpha.2.6", "0.7.0-alpha.3", "0.7.0-alpha.3.1", "0.7.0-alpha.4", "0.7.0-alpha.5", "0.7.0-alpha.5.1", "0.7.0-alpha.5.2", "0.7.0-alpha.6", "0.7.0-alpha.7", "0.7.0-alpha.8"):
        app_h = read("src/app/App.hpp")
        app_cpp = read("src/app/App.cpp")
        settings_h = read("src/ui/SettingsWindow.hpp")
        settings_cpp = read("src/ui/SettingsWindow.cpp")
        process_h = read("src/platform/ProcessMemory.hpp")
        process_cpp = read("src/platform/ProcessMemory.cpp")
        pinyin_h = read("src/core/PinyinSearch.hpp")
        pinyin_cpp = read("src/core/PinyinSearch.cpp")
        search_h = read("src/core/SearchEngine.hpp")
        command_store_h = read("src/core/CommandStore.hpp")
        cmake = read("CMakeLists.txt")
        workflow = read(".github/workflows/build.yml")
        memory_test = read("tests/ProcessMemoryTests.cpp")

        for token in (
            "RuntimeDiagnosticsSnapshot",
            "RuntimeDiagnostics() const noexcept",
            "QueryCurrentProcessMemory",
            "userCommandCount",
            "providerCommandCount",
            "mergedCommandCount",
            "pinyinLoaded",
            "pinyinCacheEntryCount",
            "providerRefreshRunning",
            "providerMonitorRunning",
        ):
            if token not in app_h and token not in app_cpp:
                fail(f"v0.7 alpha.2.3 runtime diagnostics contract missing: {token}")

        for token in (
            "ProcessMemorySnapshot",
            "workingSetBytes",
            "peakWorkingSetBytes",
            "privateBytes",
            "GetProcessMemoryInfo",
            "PROCESS_MEMORY_COUNTERS_EX",
        ):
            if token not in process_h and token not in process_cpp:
                fail(f"v0.7 alpha.2.3 process-memory platform contract missing: {token}")

        for token in (
            "Loaded() const noexcept",
            "CacheEntryCount() const noexcept",
        ):
            if token not in pinyin_h and token not in pinyin_cpp:
                fail(f"v0.7 alpha.2.3 Pinyin diagnostics missing: {token}")

        for token in (
            "PinyinLoaded",
            "PinyinCacheEntryCount",
        ):
            if token not in search_h:
                fail(f"v0.7 alpha.2.3 SearchEngine diagnostics missing: {token}")

        if "ProviderCommandCount() const noexcept" not in command_store_h:
            fail("v0.7 alpha.2.3 raw Provider command count is not exposed")

        for token in (
            "diagnosticsMemoryTitle_",
            "diagnosticsMemoryStatus_",
            "diagnosticsSearchTitle_",
            "diagnosticsSearchStatus_",
        ):
            if token not in settings_h:
                fail(f"v0.7 alpha.2.3 Settings diagnostics control missing: {token}")

        for token in (
            'T(L"进程内存",',
            'T(L"搜索数据与后台",',
            "Working Set",
            "Peak Working Set",
            "Private Bytes",
            "Provider 原始命令",
            "Pinyin:",
            "Provider Refresh",
            "app_.RuntimeDiagnostics()",
            "1000",
        ):
            if token not in settings_cpp:
                fail(f"v0.7 alpha.2.3 Diagnostics UI contract missing: {token}")

        for forbidden in (
            "EmptyWorkingSet",
            "SetProcessWorkingSetSize",
            "SetProcessWorkingSetSizeEx",
        ):
            if forbidden in app_cpp or forbidden in settings_cpp or forbidden in process_cpp:
                fail(f"v0.7 alpha.2.3 must remain observation-only: {forbidden}")

        for token in (
            "src/platform/ProcessMemory.cpp",
            "process_memory_tests",
            "psapi",
        ):
            if token not in cmake:
                fail(f"v0.7 alpha.2.3 CMake memory diagnostics gate missing: {token}")

        if workflow.count("process_memory_tests") < 4:
            fail("v0.7 alpha.2.3 process_memory_tests must run in Windows smoke and compatibility gates")

        for token in (
            "QueryCurrentProcessMemory",
            "snapshot.available",
            "snapshot.workingSetBytes > 0",
            "snapshot.privateBytes > 0",
        ):
            if token not in memory_test:
                fail(f"v0.7 alpha.2.3 process-memory test coverage missing: {token}")

    if version in ("0.7.0-alpha.2.4", "0.7.0-alpha.2.5", "0.7.0-alpha.2.6", "0.7.0-alpha.3", "0.7.0-alpha.3.1", "0.7.0-alpha.4", "0.7.0-alpha.5", "0.7.0-alpha.5.1", "0.7.0-alpha.5.2", "0.7.0-alpha.6", "0.7.0-alpha.7", "0.7.0-alpha.8"):
        pinyin_cpp = read("src/core/PinyinSearch.cpp")
        search_test = read("tests/SearchEngineTests.cpp")

        for token in (
            "enum class State",
            "State::Unloaded",
            "EnsureLoaded() const noexcept",
            "std::atomic<State>",
            "std::scoped_lock lock",
            "ContainsSupportedHanzi(text)",
        ):
            if token not in pinyin_cpp:
                fail(f"v0.7 alpha.2.4 lazy Pinyin contract missing: {token}")

        impl_ctor = pinyin_cpp.find("explicit Impl(")
        ensure_loaded = pinyin_cpp.find("EnsureLoaded() const noexcept")
        eager_make = pinyin_cpp.find(
            "std::make_unique<Pinyin::Pinyin>",
            impl_ctor,
            ensure_loaded,
        )
        lazy_make = pinyin_cpp.find(
            "std::make_unique<Pinyin::Pinyin>",
            ensure_loaded,
        )
        if impl_ctor < 0 or ensure_loaded < 0:
            fail("v0.7 alpha.2.4 Pinyin lazy-init boundaries were not found")
        if eager_make >= 0:
            fail("v0.7 alpha.2.4 must not construct Pinyin::Pinyin in Impl constructor")
        if lazy_make < 0:
            fail("v0.7 alpha.2.4 must construct Pinyin::Pinyin in EnsureLoaded")

        for token in (
            "assert(!engine.PinyinLoaded());",
            "assert(engine.PinyinAvailable());",
            "assert(engine.PinyinCacheEntryCount() == 0);",
            'L"weixin"',
            "assert(engine.PinyinLoaded());",
            "assert(engine.PinyinCacheEntryCount() > 0);",
            "assert(!fallback.PinyinLoaded());",
            "assert(!fallback.PinyinAvailable());",
        ):
            if token not in search_test:
                fail(f"v0.7 alpha.2.4 lazy Pinyin regression coverage missing: {token}")

    if version in ("0.7.0-alpha.2.5", "0.7.0-alpha.2.6", "0.7.0-alpha.3", "0.7.0-alpha.3.1", "0.7.0-alpha.4", "0.7.0-alpha.5", "0.7.0-alpha.5.1", "0.7.0-alpha.5.2", "0.7.0-alpha.6", "0.7.0-alpha.7", "0.7.0-alpha.8"):
        command_store_h = read("src/core/CommandStore.hpp")
        command_store_cpp = read("src/core/CommandStore.cpp")
        command_merge_h = read("src/core/CommandMerge.hpp")
        command_merge_cpp = read("src/core/CommandMerge.cpp")
        settings_h = read("src/core/Settings.hpp")
        settings_cpp = read("src/core/Settings.cpp")
        search_h = read("src/core/SearchEngine.hpp")
        search_cpp = read("src/core/SearchEngine.cpp")
        settings_ui_h = read("src/ui/SettingsWindow.hpp")
        settings_ui_cpp = read("src/ui/SettingsWindow.cpp")
        upgrade_test = read("tests/UpgradeMatrixTests.cpp")
        merge_test = read("tests/CommandMergeTests.cpp")
        search_test = read("tests/SearchEngineTests.cpp")
        runtime_smoke = read("scripts/verify_runtime_smoke.ps1")

        if "providerCommands_" in command_store_h or "providerCommands_" in command_store_cpp:
            fail("v0.7 alpha.2.5 must not retain a raw Provider Command vector")

        for token in (
            "providerEnabled_",
            "providerCommandCount_",
            "ProviderCacheData cache",
            "std::vector<const Command*>",
            "MergeCommandViews",
        ):
            if token not in command_store_h and token not in command_store_cpp:
                fail(f"v0.7 alpha.2.5 Provider storage dedup missing: {token}")

        for token in (
            "MergeCommandViews",
            "std::vector<const Command*>",
        ):
            if token not in command_merge_h and token not in command_merge_cpp:
                fail(f"v0.7 alpha.2.5 merge-view contract missing: {token}")

        behavior = settings.get("behavior", {})
        if behavior.get("pinyinSearch") is not True:
            fail("v0.7 alpha.2.5 Pinyin search must default to enabled")

        for token in (
            "bool pinyinSearch{true};",
            '"pinyinSearch"',
        ):
            if token not in settings_h and token not in settings_cpp:
                fail(f"v0.7 alpha.2.5 persisted Pinyin setting missing: {token}")

        for token in (
            "bool allowPinyin = true",
            "allowPinyin &&",
            "ReleasePinyinResources",
        ):
            if token not in search_h and token not in search_cpp:
                fail(f"v0.7 alpha.2.5 Pinyin runtime control missing: {token}")

        for token in (
            "kIdPinyinSearch",
            "pinyinSearch_",
            'T(L"启用拼音搜索",',
        ):
            if token not in settings_ui_h and token not in settings_ui_cpp:
                fail(f"v0.7 alpha.2.5 Pinyin Settings UI missing: {token}")

        expected_pinyin_upgrade_tokens = [
            "AssertSchema4Migration",
            "MigratedFromSchemaVersion() ==",
            'at("pinyinSearch")',
        ]
        if version in ("0.7.0-alpha.5.1", "0.7.0-alpha.5.2", "0.7.0-alpha.6", "0.7.0-alpha.7", "0.7.0-alpha.8"):
            expected_pinyin_upgrade_tokens.append(
                "config::kSettingsSchemaVersion"
            )
        else:
            expected_pinyin_upgrade_tokens.append(
                "downgrade.schemaVersion == 5"
            )

        for token in expected_pinyin_upgrade_tokens:
            if token not in upgrade_test:
                fail(f"v0.7 alpha.2.5 migration coverage missing: {token}")

        for token in (
            "MergeCommandViews",
            "nullptr",
        ):
            if token not in merge_test:
                fail(f"v0.7 alpha.2.5 merge-view regression coverage missing: {token}")

        for token in (
            "pinyinDisabledEngine",
            "ReleasePinyinResources",
        ):
            if token not in search_test:
                fail(f"v0.7 alpha.2.5 Pinyin toggle regression coverage missing: {token}")

        expected_runtime_schema = (
            6 if version in ("0.7.0-alpha.5.1", "0.7.0-alpha.5.2", "0.7.0-alpha.6", "0.7.0-alpha.7", "0.7.0-alpha.8") else 5
        )
        for token in (
            f"$migratedSettings.schemaVersion -ne {expected_runtime_schema}",
            "$migratedSettings.behavior.pinyinSearch -ne $true",
            f"schema 2 -> {expected_runtime_schema}",
        ):
            if token not in runtime_smoke:
                fail(f"v0.7 alpha.2.5 packaged runtime migration gate missing: {token}")

    if version == "0.7.0-alpha.2.6":
        settings_ui_cpp = read("src/ui/SettingsWindow.cpp")

        draw_toggle_start = settings_ui_cpp.find(
            "void SettingsWindow::DrawGeneralToggle"
        )
        draw_toggle_end = settings_ui_cpp.find(
            "\nvoid SettingsWindow::",
            draw_toggle_start + 1,
        )
        if draw_toggle_start < 0 or draw_toggle_end < 0:
            fail("v0.7 alpha.2.6 DrawGeneralToggle boundaries were not found")

        draw_toggle = settings_ui_cpp[
            draw_toggle_start:draw_toggle_end
        ]
        for token in (
            "case kIdPinyinSearch:",
            'L"启用拼音搜索"',
            "cpp-pinyin",
        ):
            if token not in draw_toggle:
                fail(f"v0.7 alpha.2.6 Pinyin owner-draw content missing: {token}")

        draw_item_start = settings_ui_cpp.find("case WM_DRAWITEM:")
        draw_item_end = settings_ui_cpp.find(
            "case WM_VSCROLL:",
            draw_item_start + 1,
        )
        if draw_item_start < 0 or draw_item_end < 0:
            fail("v0.7 alpha.2.6 WM_DRAWITEM boundaries were not found")

        draw_item = settings_ui_cpp[
            draw_item_start:draw_item_end
        ]
        if "item->CtlID == kIdPinyinSearch" not in draw_item:
            fail("v0.7 alpha.2.6 Pinyin toggle is not routed through owner-draw")

    if version == "0.7.0-alpha.3":
        editor_h = read("src/ui/ShortcutEditorDialog.hpp")
        editor_cpp = read("src/ui/ShortcutEditorDialog.cpp")
        for token in (
            "shortcutGroup_",
            "executionGroup_",
            "kEditorWidthLogical = 620",
            "kEditorHeightLogical = 480",
            "kTypeDropdownHeightLogical = 150",
            "CB_SETMINVISIBLE",
            "CBN_SELCHANGE",
            'T(L"快捷项", L"Shortcut")',
            'T(L"启动选项", L"Launch options")',
            'T(L"命令行", L"Command line")',
            "type == CommandType::Url",
        ):
            if token not in editor_h and token not in editor_cpp:
                fail(f"v0.7 alpha.3 shortcut-editor usability contract missing: {token}")
        if "std::array<const wchar_t*, 4>" not in editor_cpp:
            fail("v0.7 alpha.3 type selector must expose exactly four command types")

    if version == "0.7.0-alpha.3.1":
        editor_h = read("src/ui/ShortcutEditorDialog.hpp")
        editor_cpp = read("src/ui/ShortcutEditorDialog.cpp")
        editor_model_h = read("src/core/ShortcutEditorModel.hpp")
        editor_model_cpp = read("src/core/ShortcutEditorModel.cpp")
        editor_model_test = read("tests/ShortcutEditorModelTests.cpp")
        app_cpp = read("src/app/App.cpp")
        cmake = read("CMakeLists.txt")

        for token in (
            'T(L"快捷词 *",',
            "ParseShortcutKeywords",
            "FormatShortcutKeywords",
            "BrowseTargetFile",
            "BrowseTargetFolder",
            "advancedToggle_",
            "advancedExpanded_",
            'T(L"固定参数",',
            'T(L"工作目录（留空自动使用目标所在目录）",',
            'T(L"暂停此快捷项",',
            "command.enabled =\n        !IsChecked(paused_);",
            "std::array<const wchar_t*, 5>",
            'T(L"自动识别",',
            "InferShortcutCommandType",
            "SuggestShortcutTitle",
        ):
            if token not in editor_h and token not in editor_cpp:
                fail(f"v0.7 alpha.3.1 shortcut-editor workflow missing: {token}")

        for forbidden in (
            "aliases_",
            'T(L"主快捷词 *",',
            'T(L"别名（逗号分隔）",',
        ):
            if forbidden in editor_h or forbidden in editor_cpp:
                fail(f"v0.7 alpha.3.1 still exposes internal keyword structure: {forbidden}")

        editor_model = editor_model_h + editor_model_cpp
        for token in (
            "struct ShortcutKeywordSet",
            "ParseShortcutKeywords",
            "FormatShortcutKeywords",
            "InferShortcutCommandType",
            "SuggestShortcutTitle",
            "DefaultShortcutWorkingDirectory",
        ):
            if token not in editor_model:
                fail(f"v0.7 alpha.3.1 editor model missing: {token}")

        for token in (
            'L"v2rayN, vpn，proxy; VPN"',
            "CommandType::Url",
            "CommandType::Folder",
            "CommandType::CommandLine",
            'L"C:\\\\Tools\\\\v2rayN\\\\v2rayN.exe"',
            "DefaultShortcutWorkingDirectory",
        ):
            if token not in editor_model_test:
                fail(f"v0.7 alpha.3.1 editor model regression missing: {token}")

        for token in (
            '#include "../core/ShortcutEditorModel.hpp"',
            "DefaultShortcutWorkingDirectory(",
            "resolved.source ==",
            "CommandSource::User",
        ):
            if token not in app_cpp:
                fail(f"v0.7 alpha.3.1 automatic working-directory runtime missing: {token}")

        for token in (
            "src/core/ShortcutEditorModel.cpp",
            "shortcut_editor_model_tests",
            "tests/ShortcutEditorModelTests.cpp",
        ):
            if token not in cmake:
                fail(f"v0.7 alpha.3.1 editor-model CI wiring missing: {token}")

    if version in ("0.7.0-alpha.4", "0.7.0-alpha.5", "0.7.0-alpha.5.1", "0.7.0-alpha.5.2", "0.7.0-alpha.6", "0.7.0-alpha.7", "0.7.0-alpha.8"):
        command_h = read("src/core/Command.hpp")
        runtime_h = read("src/core/RuntimeInput.hpp")
        runtime_cpp = read("src/core/RuntimeInput.cpp")
        runtime_test = read("tests/RuntimeInputTests.cpp")
        schema_test = read("tests/UserCommandSchemaTests.cpp")
        user_store = read("src/core/UserCommandStore.cpp")
        editor_h = read("src/ui/ShortcutEditorDialog.hpp")
        editor_cpp = read("src/ui/ShortcutEditorDialog.cpp")
        app_h = read("src/app/App.hpp")
        app_cpp = read("src/app/App.cpp")
        web_cpp = read("src/core/WebAction.cpp")
        web_test = read("tests/WebActionTests.cpp")
        commands_example = json.loads(read("config/commands.example.json"))
        commands_tsv = read("config/commands.example.tsv")
        cmake = read("CMakeLists.txt")

        for token in (
            "enum class RuntimeInputMode",
            "RuntimeInputMode::None",
            "RuntimeInputMode runtimeInputMode",
        ):
            if token not in command_h:
                fail(f"v0.7 alpha.4 Command runtime-input model missing: {token}")

        if cpp_int("src/core/ConfigIO.hpp", "kCommandsSchemaVersion") != 2:
            fail("v0.7 alpha.4 commands.json must advance to schemaVersion 2")

        for token in (
            '"runtimeInputMode"',
            "RuntimeInputModeName",
            "ParseRuntimeInputMode",
            'L"{query}"',
            "RuntimeInputMode::UrlEncoded",
            '{"runtimeInputMode", RuntimeInputModeName(command.runtimeInputMode)}',
        ):
            if token not in user_store:
                fail(f"v0.7 alpha.4 command persistence/migration missing: {token}")

        runtime = runtime_h + runtime_cpp
        for token in (
            'L"{input}"',
            'L"{query}"',
            "HasRuntimeInputPlaceholder",
            "CanAcceptRuntimeInput",
            "EncodeRuntimeInput",
            "ResolveRuntimeInput",
            "BuildRuntimeInputActionResults",
            "PercentEncodeUtf8",
        ):
            if token not in runtime:
                fail(f"v0.7 alpha.4 runtime-input core missing: {token}")

        for token in (
            'L"心脏 MRI"',
            'L"%E5%BF%83%E8%84%8F%20MRI"',
            'L"g 8.8.8.8"',
            "RuntimeInputMode::Raw",
            "RuntimeInputMode::UrlEncoded",
            "BuildRuntimeInputActionResults",
        ):
            if token not in runtime_test:
                fail(f"v0.7 alpha.4 runtime-input regression coverage missing: {token}")

        for token in (
            "schemaVersion",
            "RuntimeInputMode::UrlEncoded",
            '"url-encoded"',
            "UnsupportedSchema",
            "downgrade.schemaVersion == 2",
            "RuntimeInputMode::Raw",
        ):
            if token not in schema_test:
                fail(f"v0.7 alpha.4 commands schema migration coverage missing: {token}")

        app = app_h + app_cpp
        for token in (
            '#include "../core/RuntimeInput.hpp"',
            "BuildRuntimeInputActionResults",
            "action.payload",
            "std::wstring_view runtimeInput",
            "ResolveRuntimeInput",
        ):
            if token not in app:
                fail(f"v0.7 alpha.4 App runtime-input integration missing: {token}")

        editor = editor_h + editor_cpp
        for token in (
            "runtimeInput_",
            "SelectedRuntimeInputMode",
            "UpdateRuntimeInputHint",
            'T(L"运行时输入",',
            'T(L"不接受额外输入",',
            'T(L"原样传递",',
            'T(L"URL 编码（UTF-8）",',
            "{input}",
            "CanAcceptRuntimeInput",
        ):
            if token not in editor:
                fail(f"v0.7 alpha.4 Shortcut Editor runtime-input UI missing: {token}")

        for token in (
            "command.runtimeInputMode !=",
            "RuntimeInputMode::None",
        ):
            if token not in web_cpp:
                fail(f"v0.7 alpha.4 legacy WebAction ownership gate missing: {token}")

        if "explicitRuntime.runtimeInputMode" not in web_test:
            fail("v0.7 alpha.4 WebAction duplicate-ownership regression is missing")

        if commands_example.get("schemaVersion") != 2:
            fail("v0.7 alpha.4 commands.example.json must use schemaVersion 2")
        example_commands = commands_example.get("commands", [])
        if not example_commands or any("runtimeInputMode" not in item for item in example_commands):
            fail("v0.7 alpha.4 example commands must declare runtimeInputMode")
        if not any(
            item.get("runtimeInputMode") == "url-encoded"
            and "{input}" in item.get("target", "")
            for item in example_commands
        ):
            fail("v0.7 alpha.4 example commands need an encoded {input} URL shortcut")

        expected_tsv_marker = (
            "commands TSV v3"
            if version in ("0.7.0-alpha.5", "0.7.0-alpha.5.1", "0.7.0-alpha.5.2", "0.7.0-alpha.6", "0.7.0-alpha.7", "0.7.0-alpha.8")
            else "commands TSV v2"
        )
        for token in (
            expected_tsv_marker,
            "runtimeInputMode",
            "{input}",
            "url-encoded",
        ):
            if token not in commands_tsv:
                fail(f"v0.7 alpha.4 TSV v2 example missing: {token}")

        for token in (
            "src/core/RuntimeInput.cpp",
            "runtime_input_tests",
            "tests/RuntimeInputTests.cpp",
            "user_command_schema_tests",
            "tests/UserCommandSchemaTests.cpp",
        ):
            if token not in cmake:
                fail(f"v0.7 alpha.4 CI wiring missing: {token}")

    if version in ("0.7.0-alpha.5", "0.7.0-alpha.5.1", "0.7.0-alpha.5.2", "0.7.0-alpha.6", "0.7.0-alpha.7", "0.7.0-alpha.8"):
        editor_h = read("src/ui/ShortcutEditorDialog.hpp")
        editor_cpp = read("src/ui/ShortcutEditorDialog.cpp")
        launcher_h = read("src/ui/LauncherWindow.hpp")
        launcher_cpp = read("src/ui/LauncherWindow.cpp")
        launcher_result = read("src/core/LauncherResult.hpp")
        app_h = read("src/app/App.hpp")
        app_cpp = read("src/app/App.cpp")
        user_store_h = read("src/core/UserCommandStore.hpp")
        user_store_cpp = read("src/core/UserCommandStore.cpp")
        path_h = read("src/ui/ShortcutPathConverterDialog.hpp")
        path_cpp = read("src/ui/ShortcutPathConverterDialog.cpp")
        schema_test = read("tests/UserCommandSchemaTests.cpp")
        path_test = read("tests/UserCommandPathUpdateTests.cpp")
        runtime_test = read("tests/RuntimeInputTests.cpp")
        web_test = read("tests/WebActionTests.cpp")
        commands_tsv = read("config/commands.example.tsv")

        expected_alpha5_settings_schema = (
            6 if version in ("0.7.0-alpha.5.1", "0.7.0-alpha.5.2", "0.7.0-alpha.6", "0.7.0-alpha.7", "0.7.0-alpha.8") else 5
        )
        if cpp_int("src/core/ConfigIO.hpp", "kSettingsSchemaVersion") != expected_alpha5_settings_schema:
            fail("v0.7 alpha.5 line has unexpected settings schemaVersion")
        if cpp_int("src/core/ConfigIO.hpp", "kCommandsSchemaVersion") != 2:
            fail("v0.7 alpha.5 must keep commands schemaVersion 2")
        if cpp_int("src/core/ConfigIO.hpp", "kUsageSchemaVersion") != 1:
            fail("v0.7 alpha.5 must keep usage schemaVersion 1")
        if cpp_int("src/core/ProviderCache.cpp", "kProviderCacheSchemaVersion") != 2:
            fail("v0.7 alpha.5 must keep provider-cache schemaVersion 2")

        editor = editor_h + editor_cpp
        for token in (
            "testInput_",
            "UpdateRuntimeTestVisibility",
            'T(L"测试输入",',
            "BrowseIcon",
            "ResetIcon",
            "icon_",
            'T(L"图标（留空 = 自动跟随目标）",',
            'L"Icon sources',
            "app_.TestCommand(\n        command,\n        runtimeInput)",
        "command.icon",
        'L"auto"',
        "kIdBrowseIcon",
            "kIdResetIcon",
        ):
            if token not in editor:
                fail(f"v0.7 alpha.5 Shortcut Editor completion missing: {token}")

        app = app_h + app_cpp
        for token in (
            "bool TestCommand(",
            "std::wstring_view runtimeInput = {}",
            "return LaunchCommand(\n        command,\n        false,\n        runtimeInput);",
            "result.iconSource",
            "command.icon == L\"auto\"",
        ):
            if token not in app:
                fail(f"v0.7 alpha.5 App shortcut completion missing: {token}")

        for token in (
            "std::wstring iconSource",
            "explicit icon file/executable/shortcut path",
        ):
            if token not in launcher_result:
                fail(f"v0.7 alpha.5 LauncherResult icon metadata missing: {token}")

        launcher = launcher_h + launcher_cpp
        for token in (
            "resultIconCache_",
            "ResultIcon",
            "ClearResultIconCache",
            "ResolvePortablePath",
            "SearchPathW",
            "LoadImageW",
            "SHGetFileInfoW",
            "ExtractIconExW",
            "DrawIconEx",
            "DestroyIcon",
        ):
            if token not in launcher:
                fail(f"v0.7 alpha.5 launcher icon rendering missing: {token}")

        store = user_store_h + user_store_cpp
        for token in (
            "std::optional<std::wstring> icon",
            "command.icon = TrimWide(command.icon)",
            "fields.size() >= 13",
            "commands TSV v3",
            "runtimeInputMode\\ticon",
            "update.icon",
        ):
            if token not in store:
                fail(f"v0.7 alpha.5 icon persistence/interchange missing: {token}")

        converter = path_h + path_cpp
        for token in (
            "Field::Icon",
            'T(L"    自定义图标",',
            "command.icon != L\"auto\"",
            "it->icon",
        ):
            if token not in converter:
                fail(f"v0.7 alpha.5 custom-icon path conversion missing: {token}")

        for token in (
            'L"C:\\\\Icons\\\\ping.ico"',
            "commands TSV v3",
            "runtimeInputMode\\ticon",
            "importedIt->icon",
        ):
            if token not in schema_test:
                fail(f"v0.7 alpha.5 custom-icon round-trip coverage missing: {token}")

        for token in (
            "first.icon",
            "firstAfter->icon",
            "changed.ico",
        ):
            if token not in path_test:
                fail(f"v0.7 alpha.5 custom-icon path rollback coverage missing: {token}")

        if "results[0].iconSource" not in runtime_test:
            fail("v0.7 alpha.5 runtime-input results must preserve custom icons")
        if "results[0].iconSource" not in web_test:
            fail("v0.7 alpha.5 legacy web results must preserve custom icons")

        for token in (
            "commands TSV v3",
            "runtimeInputMode<TAB>icon",
            "auto",
        ):
            if token not in commands_tsv:
                fail(f"v0.7 alpha.5 TSV v3 example missing: {token}")

    if version == "0.7.0-alpha.5.1":
        settings_h = read("src/core/Settings.hpp")
        settings_cpp = read("src/core/Settings.cpp")
        settings_ui_h = read("src/ui/SettingsWindow.hpp")
        settings_ui_cpp = read("src/ui/SettingsWindow.cpp")
        launcher_h = read("src/ui/LauncherWindow.hpp")
        launcher_cpp = read("src/ui/LauncherWindow.cpp")
        app_h = read("src/app/App.hpp")
        app_cpp = read("src/app/App.cpp")
        config_test = read("tests/ConfigCoreTests.cpp")
        upgrade_test = read("tests/UpgradeMatrixTests.cpp")
        runtime_smoke = read("scripts/verify_runtime_smoke.ps1")

        if settings.get("appearance", {}).get("showResultIcons") is not False:
            fail("v0.7 alpha.5.1 result icons must default to disabled")

        settings_source = settings_h + settings_cpp
        for token in (
            "bool showResultIcons{false}",
            "SetShowResultIcons",
            '"showResultIcons"',
        ):
            if token not in settings_source:
                fail(f"v0.7 alpha.5.1 settings persistence missing: {token}")

        settings_ui = settings_ui_h + settings_ui_cpp
        for token in (
            "kIdShowResultIcons",
            "showResultIcons_",
            "resultIconsNote_",
            'T(L"显示搜索结果图标",',
            "app_.SetShowResultIcons",
            "settings.showResultIcons",
        ):
            if token not in settings_ui:
                fail(f"v0.7 alpha.5.1 Appearance UI missing: {token}")

        app = app_h + app_cpp
        for token in (
            "SetShowResultIcons",
            "ApplyResultIconPreference",
        ):
            if token not in app:
                fail(f"v0.7 alpha.5.1 App icon preference integration missing: {token}")

        launcher = launcher_h + launcher_cpp
        for token in (
            "ApplyResultIconPreference",
            ".showResultIcons",
            "DpiScale(165)",
            "numberRect.right",
        ):
            if token not in launcher:
                fail(f"v0.7 alpha.5.1 icon preference/layout missing: {token}")

        result_icon_start = launcher_cpp.find(
            "HICON LauncherWindow::ResultIcon"
        )
        result_icon_end = launcher_cpp.find(
            "void LauncherWindow::RebuildVisibleResults",
            result_icon_start,
        )
        if result_icon_start < 0 or result_icon_end < 0:
            fail("v0.7 alpha.5.1 ResultIcon block is missing")
        result_icon_block = launcher_cpp[
            result_icon_start:result_icon_end
        ]
        preference_guard = result_icon_block.find(
            ".showResultIcons"
        )
        shell_positions = [
            result_icon_block.find(token)
            for token in (
                "ResolvePortablePath",
                "SearchPathW",
                "SHGetFileInfoW",
                "LoadImageW",
            )
        ]
        shell_positions = [
            pos for pos in shell_positions if pos >= 0
        ]
        if (
            preference_guard < 0
            or not shell_positions
            or preference_guard > min(shell_positions)
        ):
            fail(
                "v0.7 alpha.5.1 must gate icons before shell/path resolution"
            )

        for token in (
            "!featureSettings.Data().showResultIcons",
            "SetShowResultIcons(true)",
            ".showResultIcons",
        ):
            if token not in config_test:
                fail(f"v0.7 alpha.5.1 settings test coverage missing: {token}")

        for token in (
            "AssertSchema5Migration",
            "showResultIcons",
            "MigratedFromSchemaVersion() ==",
            "config::kSettingsSchemaVersion",
        ):
            if token not in upgrade_test:
                fail(f"v0.7 alpha.5.1 upgrade/downgrade coverage missing: {token}")

        for token in (
            "$migratedSettings.schemaVersion -ne 6",
            "$migratedSettings.appearance.showResultIcons -ne $false",
            "schema 2 -> 6",
        ):
            if token not in runtime_smoke:
                fail(f"v0.7 alpha.5.1 runtime migration gate missing: {token}")

    if version == "0.7.0-alpha.5.2":
        settings_h = read("src/core/Settings.hpp")
        settings_cpp = read("src/core/Settings.cpp")
        settings_ui = (
            read("src/ui/SettingsWindow.hpp")
            + read("src/ui/SettingsWindow.cpp")
        )
        launcher_h = read("src/ui/LauncherWindow.hpp")
        launcher_cpp = read("src/ui/LauncherWindow.cpp")
        policy_h = read("src/core/ResultIconPipeline.hpp")
        policy_cpp = read("src/core/ResultIconPipeline.cpp")
        policy_test = read("tests/ResultIconPipelineTests.cpp")
        cmake = read("CMakeLists.txt")

        if settings.get("appearance", {}).get("showResultIcons") is not False:
            fail("v0.7 alpha.5.2 must preserve default-off result icons")
        if cpp_int("src/core/ConfigIO.hpp", "kSettingsSchemaVersion") != 6:
            fail("v0.7 alpha.5.2 must keep settings schemaVersion 6")

        for token in (
            "bool showResultIcons{false}",
            "SetShowResultIcons",
            '"showResultIcons"',
        ):
            if token not in settings_h + settings_cpp:
                fail(f"v0.7 alpha.5.2 icon preference persistence missing: {token}")

        for token in (
            "kIdShowResultIcons",
            'T(L"显示搜索结果图标",',
            "app_.SetShowResultIcons",
        ):
            if token not in settings_ui:
                fail(f"v0.7 alpha.5.2 Appearance icon control missing: {token}")

        policy = policy_h + policy_cpp
        for token in (
            "kResultIconCacheCapacity = 96",
            "ResultIconRequestStamp",
            "MakeResultIconCacheKey",
            "ShouldAcceptResultIconCompletion",
            "searchGeneration",
            "iconEpoch",
            "pixelSize",
        ):
            if token not in policy:
                fail(f"v0.7 alpha.5.2 icon pipeline policy missing: {token}")

        for token in (
            "small != large",
            "ShouldAcceptResultIconCompletion",
            "kResultIconCacheCapacity >= 64",
            "kResultIconCacheCapacity <= 128",
        ):
            if token not in policy_test:
                fail(f"v0.7 alpha.5.2 icon policy regression missing: {token}")

        for token in (
            "ResultIconCacheEntry",
            "ResultIconPending",
            "ResultIconJob",
            "ResultIconCompletion",
            "resultIconJobs_",
            "resultIconCompletions_",
            "resultIconWorkerMutex_",
            "resultIconWorkerCv_",
            "resultIconWorker_",
            "resultIconEpoch_",
            "kIconReadyMessage",
        ):
            if token not in launcher_h:
                fail(f"v0.7 alpha.5.2 async launcher state missing: {token}")

        for token in (
            "EnsureResultIconWorker",
            "ResultIconWorkerLoop",
            "HandleResultIconCompletions",
            "CancelPendingResultIconRequests",
            "TrimResultIconCache",
            "InvalidateResultRowsForIconKey",
            "PostMessageW",
            "CoInitializeEx",
            "kIconReadyMessage",
            "ShouldAcceptResultIconCompletion",
        ):
            if token not in launcher_cpp:
                fail(f"v0.7 alpha.5.2 async launcher pipeline missing: {token}")

        result_icon_start = launcher_cpp.find(
            "HICON LauncherWindow::ResultIcon"
        )
        result_icon_end = launcher_cpp.find(
            "void LauncherWindow::ResultIconWorkerLoop",
            result_icon_start,
        )
        if result_icon_start < 0 or result_icon_end < 0:
            fail("v0.7 alpha.5.2 ResultIcon async boundaries missing")
        paint_lookup_block = launcher_cpp[
            result_icon_start:result_icon_end
        ]
        for forbidden in (
            "ResolvePortablePath",
            "SearchPathW",
            "LoadImageW",
            "SHGetFileInfoW",
            "ExtractIconExW",
        ):
            if forbidden in paint_lookup_block:
                fail(
                    "v0.7 alpha.5.2 WM_DRAWITEM/cache lookup path "
                    f"still resolves icons synchronously: {forbidden}"
                )
        for token in (
            "MakeResultIconCacheKey",
            "resultIconCache_.find",
            "QueueResultIcon",
            "return nullptr",
        ):
            if token not in paint_lookup_block:
                fail(f"v0.7 alpha.5.2 cache-only ResultIcon path missing: {token}")

        loader_start = launcher_cpp.find(
            "LoadResultIconSource"
        )
        loader_end = launcher_cpp.find(
            "} // namespace",
            loader_start,
        )
        if loader_start < 0 or loader_end < 0:
            fail("v0.7 alpha.5.2 background icon loader boundaries missing")
        loader_block = launcher_cpp[
            loader_start:loader_end
        ]
        for token in (
            "ResolvePortablePath",
            "SearchPathW",
            "LoadImageW",
            "SHGetFileInfoW",
            "ExtractIconExW",
        ):
            if token not in loader_block:
                fail(f"v0.7 alpha.5.2 background resolver missing: {token}")

        rebuild_start = launcher_cpp.find(
            "void LauncherWindow::RebuildVisibleResults"
        )
        rebuild_end = launcher_cpp.find(
            "void LauncherWindow::UpdatePreview",
            rebuild_start,
        )
        rebuild_block = launcher_cpp[
            rebuild_start:rebuild_end
        ]
        if "ClearResultIconCache" in rebuild_block:
            fail(
                "v0.7 alpha.5.2 must preserve the bounded icon cache "
                "across search queries"
            )

        for token in (
            "++searchGeneration_;\n    CancelPendingResultIconRequests();",
            "completion.stamp",
            "searchGeneration_",
            "resultIconEpoch_",
        ):
            if token not in launcher_cpp:
                fail(f"v0.7 alpha.5.2 stale-generation handling missing: {token}")

        for token in (
            "src/core/ResultIconPipeline.cpp",
            "result_icon_pipeline_tests",
            "tests/ResultIconPipelineTests.cpp",
        ):
            if token not in cmake:
                fail(f"v0.7 alpha.5.2 build/test wiring missing: {token}")

    print(
        "v0.7.0 alpha.2/alpha.5.2 contract verified:",
        f"| commands={expected_commands_schema} settings={expected_settings_schema} provider-cache=2",
        "| Move Up/Down UI removed, sortOrder retained",
        "| path preview + atomic apply + relative runtime resolution",
        "| blank headers fixed at column creation",
        "| Windows portability + atomic update CI gates",
        "| alpha.2.4 lazy Pinyin first-use initialization",
        "| alpha.2.5 Provider storage dedup + Pinyin search control",
        "| alpha.2.6 Pinyin Settings owner-draw routing",
        "| alpha.3 compact grouped Shortcut Editor + four command types",
        "| alpha.3.1 shortcut workflow model + progressive advanced options",
        "| alpha.4 dynamic runtime input + commands schema 2",
        "| alpha.5 custom icons + dynamic test input + TSV v3",
        "| alpha.5.1 optional result icons + settings schema 6",
        "| alpha.5.2 async icon worker + generation/LRU cache",
    )
    raise SystemExit(0)


if version == "0.7.0-alpha.1":
    expected_schemas = {
        "kSettingsSchemaVersion": 4,
        "kCommandsSchemaVersion": 1,
        "kUsageSchemaVersion": 1,
    }
    for name, expected in expected_schemas.items():
        actual = cpp_int("src/core/ConfigIO.hpp", name)
        if actual != expected:
            fail(f"{name}={actual}, expected v0.7 alpha.1 value {expected}")

    if cpp_int("src/core/ProviderCache.cpp", "kProviderCacheSchemaVersion") != 2:
        fail("v0.7 alpha.1 must keep provider-cache schemaVersion 2")

    settings = json.loads(read("config/settings.example.json"))
    expected_providers = {
        "windows.startmenu": True,
        "windows.packaged": True,
        "windows.apppaths": True,
        "windows.path": True,
        "everything.filesystem": False,
    }
    if settings.get("schemaVersion") != 4:
        fail("v0.7 alpha.1 settings must remain schemaVersion 4")
    if settings.get("providers") != expected_providers:
        fail("v0.7 alpha.1 changed frozen provider defaults")

    expected_bindings = {
        "launcher.activate",
        "launcher.activateSecondary",
        "launcher.openSettings",
        "result.navigateCurrentFileManager",
        "result.copySelectedTarget",
    }
    bindings = settings.get("hotkeys", {}).get("bindings", {})
    if set(bindings) != expected_bindings:
        fail("v0.7 alpha.1 changed frozen Hotkey Registry action IDs")

    manager = read("src/ui/ShortcutManagerWindow.cpp") + read("src/ui/ShortcutManagerWindow.hpp")
    editor = read("src/ui/ShortcutEditorDialog.cpp") + read("src/ui/ShortcutEditorDialog.hpp")
    for token in (
        "ShortcutManagerWindow",
        "AddShortcut",
        "EditSelected",
        "DeleteSelected",
        "TestSelected",
        "MoveSelected",
        "ShortcutEditorDialog::Show",
    ):
        if token not in manager and token not in editor:
            fail(f"v0.7 alpha.1 shortcut-management architecture missing: {token}")

    for token in (
        "CreateUserCommand",
        "UpdateUserCommand",
        "TestCommand",
        "Primary keyword",
        "Working directory",
    ):
        if token not in editor:
            fail(f"v0.7 alpha.1 reusable shortcut editor missing: {token}")

    app = read("src/app/App.cpp") + read("src/app/App.hpp")
    for token in (
        "ShowShortcutManager",
        "shortcutManagerWindow_",
        "ShortcutManagerWindow",
    ):
        if token not in app:
            fail(f"v0.7 alpha.1 App shortcut-manager integration missing: {token}")

    launcher = read("src/ui/LauncherWindow.cpp") + read("src/ui/LauncherWindow.hpp")
    for token in (
        "kMenuShortcuts",
        "Shortcut Manager...",
        "快捷项管理...",
        "app_.ShowShortcutManager()",
        "kMenuAbout",
    ):
        if token not in launcher:
            fail(f"v0.7 alpha.1 tray shortcut-manager contract missing: {token}")
    for forbidden in (
        "kMenuThemeClassic",
        "kMenuThemeModern",
        "kMenuLangZh",
        "kMenuLangEn",
    ):
        if forbidden in launcher:
            fail(f"v0.7 alpha.1 tray still duplicates Settings control: {forbidden}")

    settings_h = read("src/ui/SettingsWindow.hpp")
    settings_cpp = read("src/ui/SettingsWindow.cpp")
    create_start = settings_cpp.find("void SettingsWindow::CreateControls()")
    create_end = settings_cpp.find("void SettingsWindow::CreateCommandPage()", create_start)
    create_block = settings_cpp[create_start:create_end]
    if "navCommands_ =" in create_block or "CreateCommandPage();" in create_block:
        fail("v0.7 alpha.1 Settings still creates the legacy Shortcuts page/navigation")
    if "Page page_{Page::General};" not in settings_h:
        fail("v0.7 alpha.1 Settings must open on General")

    expected_nav = """std::array<HWND, 7> nav{
        navGeneral_,
        navHotkeys_,
        navAppearance_,
        navProviders_,
        navData_,
        navDiagnostics_,
        navAbout_,
    };"""
    if expected_nav not in settings_cpp:
        fail("v0.7 alpha.1 Settings navigation order changed")

    draw_start = settings_cpp.find("case WM_DRAWITEM")
    draw_end = settings_cpp.find("case WM_VSCROLL", draw_start)
    if "kIdNavCommands" in settings_cpp[draw_start:draw_end]:
        fail("v0.7 alpha.1 hidden Shortcuts nav remains in owner-draw dispatch")

    cmake = read("CMakeLists.txt")
    for token in (
        "src/ui/ShortcutEditorDialog.cpp",
        "src/ui/ShortcutManagerWindow.cpp",
    ):
        if token not in cmake:
            fail(f"v0.7 alpha.1 CMake UI source missing: {token}")

    launcher_header = read("src/ui/LauncherWindow.hpp")
    for name, expected in {
        "widthLogical_": 420,
        "rowHeightLogical_": 16,
        "maxResults_": 10,
    }.items():
        found = re.search(rf"\b{re.escape(name)}\s*\{{(\d+)\}}", launcher_header)
        if not found or int(found.group(1)) != expected:
            fail(f"Classic geometry changed during v0.7 alpha.1: {name}")

    print(
        "v0.7.0-alpha.1 Shortcut Management Architecture verified:",
        "| commands schema=1, settings=4, provider-cache=2",
        "| standalone Manager + reusable Editor",
        "| tray entry + Settings Shortcuts removal",
        "| Settings order: General/Hotkeys/Appearance/Sources/Data/Diagnostics/About",
        "| frozen v0.6 provider/Hotkey/Classic contracts",
    )
    raise SystemExit(0)


if version == "0.6.0":
    expected_schemas = {
        "kSettingsSchemaVersion": 4,
        "kCommandsSchemaVersion": 1,
        "kUsageSchemaVersion": 1,
    }
    for name, expected in expected_schemas.items():
        actual = cpp_int("src/core/ConfigIO.hpp", name)
        if actual != expected:
            fail(f"{name}={actual}, expected v0.6 stable value {expected}")

    if cpp_int("src/core/ProviderCache.cpp", "kProviderCacheSchemaVersion") != 2:
        fail("v0.6 stable must keep provider-cache schemaVersion 2")

    settings = json.loads(read("config/settings.example.json"))
    expected_providers = {
        "windows.startmenu": True,
        "windows.packaged": True,
        "windows.apppaths": True,
        "windows.path": True,
        "everything.filesystem": False,
    }
    if settings.get("schemaVersion") != 4:
        fail("v0.6 stable settings must remain schemaVersion 4")
    if settings.get("providers") != expected_providers:
        fail("v0.6 stable changed frozen provider defaults")

    expected_bindings = {
        "launcher.activate",
        "launcher.activateSecondary",
        "launcher.openSettings",
        "result.navigateCurrentFileManager",
        "result.copySelectedTarget",
    }
    bindings = settings.get("hotkeys", {}).get("bindings", {})
    if set(bindings) != expected_bindings:
        fail("v0.6 stable changed frozen Hotkey Registry action IDs")

    hotkey_registry = read("src/core/HotkeyRegistry.hpp")
    for action_id in expected_bindings:
        if action_id not in hotkey_registry:
            fail(f"v0.6 stable Hotkey Registry source lost action ID: {action_id}")

    provider_ids = read("src/core/ProviderIds.hpp")
    for provider_id in (
        "windows.startmenu",
        "windows.packaged",
        "windows.apppaths",
        "windows.path",
        "everything.filesystem",
        "builtin.web",
        "builtin.clipboard",
    ):
        if provider_id not in provider_ids:
            fail(f"v0.6 stable provider/action ID missing: {provider_id}")

    everything_protocol = read("src/core/EverythingIpcProtocol.hpp")
    for token in (
        "kCopyDataQuery2W = 18",
        "Query2WireRequest",
        "ParseList2",
    ):
        if token not in everything_protocol:
            fail(f"v0.6 stable changed frozen Everything Query2 contract: {token}")

    policy = read("src/core/LauncherActionPolicy.hpp") + read("src/core/LauncherActionPolicy.cpp")
    for token in (
        "ActionEvaluation",
        "ActionUnavailableReason",
        "EvaluateLauncherAction",
        "ResultNotFolder",
        "NoSupportedFileManager",
        "NoCopyableTarget",
        "InvalidActionTarget",
    ):
        if token not in policy:
            fail(f"v0.6 stable Smart Actions contract missing: {token}")

    settings_header = read("src/ui/SettingsWindow.hpp")
    settings_ui = settings_header + read("src/ui/SettingsWindow.cpp")
    page_enum_start = settings_header.find("enum class Page")
    page_enum_end = settings_header.find("};", page_enum_start)
    if (
        page_enum_start < 0
        or page_enum_end < 0
        or "Diagnostics," not in settings_header[page_enum_start:page_enum_end]
        or "Actions," in settings_header[page_enum_start:page_enum_end]
    ):
        fail("v0.6 stable Diagnostics page enum contract changed")

    for token in (
        "Page::Diagnostics",
        "kIdNavDiagnostics",
        "navDiagnostics_",
        "CreateDiagnosticsPage",
        "diagnosticsControls_",
        'L"诊断"',
        'L"Diagnostics"',
    ):
        if token not in settings_ui:
            fail(f"v0.6 stable Diagnostics UI contract missing: {token}")

    draw_start = settings_ui.find("void SettingsWindow::DrawNavigationButton")
    draw_end = settings_ui.find("void SettingsWindow::DrawGeneralToggle", draw_start)
    if draw_start < 0 or draw_end < 0 or "kIdNavDiagnostics" not in settings_ui[draw_start:draw_end]:
        fail("v0.6 stable Diagnostics nav ID is missing from DrawNavigationButton")

    dispatch_start = settings_ui.find("case WM_DRAWITEM")
    dispatch_end = settings_ui.find("case WM_VSCROLL", dispatch_start)
    if dispatch_start < 0 or dispatch_end < 0 or "kIdNavDiagnostics" not in settings_ui[dispatch_start:dispatch_end]:
        fail("v0.6 stable Diagnostics nav ID is missing from WM_DRAWITEM dispatch")

    launcher_header = read("src/ui/LauncherWindow.hpp")
    for name, expected in {
        "widthLogical_": 420,
        "rowHeightLogical_": 16,
        "maxResults_": 10,
    }.items():
        found = re.search(
            rf"\b{re.escape(name)}\s*\{{(\d+)\}}",
            launcher_header,
        )
        if not found or int(found.group(1)) != expected:
            fail(f"Classic geometry changed during v0.6 stable: {name}")

    upgrade_tests = read("tests/UpgradeMatrixTests.cpp")
    for token in (
        "AssertCleanInstall",
        "v0.5.0-schema3.json",
        "v0.6.0-alpha.5-schema3-conflict.json",
        "v0.6.0-alpha.6.1-schema4.json",
        "v0.6.0-beta.1-schema4.json",
        "v0.6.0-beta.2-schema4.json",
        "AssertDowngradeReadOnly",
    ):
        if token not in upgrade_tests:
            fail(f"v0.6 stable upgrade matrix gate missing: {token}")

    fixture_expectations = {
        "tests/fixtures/upgrade/v0.5.0-schema3.json": 3,
        "tests/fixtures/upgrade/v0.6.0-alpha.5-schema3-conflict.json": 3,
        "tests/fixtures/upgrade/v0.6.0-alpha.6.1-schema4.json": 4,
        "tests/fixtures/upgrade/v0.6.0-beta.1-schema4.json": 4,
        "tests/fixtures/upgrade/v0.6.0-beta.2-schema4.json": 4,
    }
    for path, expected_schema in fixture_expectations.items():
        fixture = json.loads(read(path))
        if fixture.get("schemaVersion") != expected_schema:
            fail(f"{path} has wrong upgrade-matrix schemaVersion")

    cmake = read("CMakeLists.txt")
    workflow = read(".github/workflows/build.yml")
    for token in (
        "upgrade_matrix_tests",
        "tests/fixtures/upgrade",
    ):
        if token not in cmake:
            fail(f"v0.6 stable CMake upgrade gate missing: {token}")
    if workflow.count("upgrade_matrix_tests") < 4:
        fail("v0.6 stable Windows smoke/compat workflow does not gate upgrade_matrix_tests")

    runtime_smoke = read("scripts/verify_runtime_smoke.ps1")
    for token in (
        "schemaVersion -ne 4",
        "launcher.activate",
        "result.navigateCurrentFileManager",
        "everything.filesystem",
        "schema 2 -> 4",
    ):
        if token not in runtime_smoke:
            fail(f"v0.6 stable packaged runtime migration gate missing: {token}")

    package_contract = read("scripts/verify_package.ps1")
    if "V0.6_RC_VALIDATION.md" not in package_contract:
        fail("v0.6 stable package contract does not require V0.6_RC_VALIDATION.md")
    if "V0.6_RC_VALIDATION.md" not in workflow:
        fail("v0.6 stable workflow does not package V0.6_RC_VALIDATION.md")

    readme = read("README.md")
    for token in (
        "### Stable v0.6.0",
        "/releases/tag/v0.6.0",
        "/releases/download/v0.6.0/ALTRunNext-x64.zip",
        "/releases/download/v0.6.0/ALTRunNext-ARM64.zip",
    ):
        if token not in readme:
            fail(f"v0.6 stable README publication contract missing: {token}")

    workflow = read(".github/workflows/build.yml")
    for token in (
        'TAG="v$VERSION"',
        'if [[ "$VERSION" == *-* ]]; then',
        "EXTRA_ARGS+=(--prerelease)",
        'gh release create "$TAG"',
    ):
        if token not in workflow:
            fail(f"v0.6 stable publication workflow contract missing: {token}")

    print(
        "v0.6.0 stable release contract verified:",
        "| settings=4 commands=1 usage=1 provider-cache=2",
        "| frozen providers/Hotkey IDs/Everything/Smart Actions/Diagnostics",
        "| Classic 420/16/10",
        "| clean install + historical upgrade matrix + downgrade readonly",
        "| packaged schema2->4 runtime migration + package contract",
    )
    raise SystemExit(0)


if version == "0.6.0-rc.1":
    expected_schemas = {
        "kSettingsSchemaVersion": 4,
        "kCommandsSchemaVersion": 1,
        "kUsageSchemaVersion": 1,
    }
    for name, expected in expected_schemas.items():
        actual = cpp_int("src/core/ConfigIO.hpp", name)
        if actual != expected:
            fail(f"{name}={actual}, expected v0.6 rc.1 value {expected}")

    if cpp_int("src/core/ProviderCache.cpp", "kProviderCacheSchemaVersion") != 2:
        fail("v0.6 rc.1 must keep provider-cache schemaVersion 2")

    settings = json.loads(read("config/settings.example.json"))
    expected_providers = {
        "windows.startmenu": True,
        "windows.packaged": True,
        "windows.apppaths": True,
        "windows.path": True,
        "everything.filesystem": False,
    }
    if settings.get("schemaVersion") != 4:
        fail("v0.6 rc.1 settings must remain schemaVersion 4")
    if settings.get("providers") != expected_providers:
        fail("v0.6 rc.1 changed frozen provider defaults")

    expected_bindings = {
        "launcher.activate",
        "launcher.activateSecondary",
        "launcher.openSettings",
        "result.navigateCurrentFileManager",
        "result.copySelectedTarget",
    }
    bindings = settings.get("hotkeys", {}).get("bindings", {})
    if set(bindings) != expected_bindings:
        fail("v0.6 rc.1 changed frozen Hotkey Registry action IDs")

    hotkey_registry = read("src/core/HotkeyRegistry.hpp")
    for action_id in expected_bindings:
        if action_id not in hotkey_registry:
            fail(f"v0.6 rc.1 Hotkey Registry source lost action ID: {action_id}")

    provider_ids = read("src/core/ProviderIds.hpp")
    for provider_id in (
        "windows.startmenu",
        "windows.packaged",
        "windows.apppaths",
        "windows.path",
        "everything.filesystem",
        "builtin.web",
        "builtin.clipboard",
    ):
        if provider_id not in provider_ids:
            fail(f"v0.6 rc.1 provider/action ID missing: {provider_id}")

    everything_protocol = read("src/core/EverythingIpcProtocol.hpp")
    for token in (
        "kCopyDataQuery2W = 18",
        "Query2WireRequest",
        "ParseList2",
    ):
        if token not in everything_protocol:
            fail(f"v0.6 rc.1 changed frozen Everything Query2 contract: {token}")

    policy = read("src/core/LauncherActionPolicy.hpp") + read("src/core/LauncherActionPolicy.cpp")
    for token in (
        "ActionEvaluation",
        "ActionUnavailableReason",
        "EvaluateLauncherAction",
        "ResultNotFolder",
        "NoSupportedFileManager",
        "NoCopyableTarget",
        "InvalidActionTarget",
    ):
        if token not in policy:
            fail(f"v0.6 rc.1 Smart Actions contract missing: {token}")

    settings_header = read("src/ui/SettingsWindow.hpp")
    settings_ui = settings_header + read("src/ui/SettingsWindow.cpp")
    page_enum_start = settings_header.find("enum class Page")
    page_enum_end = settings_header.find("};", page_enum_start)
    if (
        page_enum_start < 0
        or page_enum_end < 0
        or "Diagnostics," not in settings_header[page_enum_start:page_enum_end]
        or "Actions," in settings_header[page_enum_start:page_enum_end]
    ):
        fail("v0.6 rc.1 Diagnostics page enum contract changed")

    for token in (
        "Page::Diagnostics",
        "kIdNavDiagnostics",
        "navDiagnostics_",
        "CreateDiagnosticsPage",
        "diagnosticsControls_",
        'L"诊断"',
        'L"Diagnostics"',
    ):
        if token not in settings_ui:
            fail(f"v0.6 rc.1 Diagnostics UI contract missing: {token}")

    draw_start = settings_ui.find("void SettingsWindow::DrawNavigationButton")
    draw_end = settings_ui.find("void SettingsWindow::DrawGeneralToggle", draw_start)
    if draw_start < 0 or draw_end < 0 or "kIdNavDiagnostics" not in settings_ui[draw_start:draw_end]:
        fail("v0.6 rc.1 Diagnostics nav ID is missing from DrawNavigationButton")

    dispatch_start = settings_ui.find("case WM_DRAWITEM")
    dispatch_end = settings_ui.find("case WM_VSCROLL", dispatch_start)
    if dispatch_start < 0 or dispatch_end < 0 or "kIdNavDiagnostics" not in settings_ui[dispatch_start:dispatch_end]:
        fail("v0.6 rc.1 Diagnostics nav ID is missing from WM_DRAWITEM dispatch")

    launcher_header = read("src/ui/LauncherWindow.hpp")
    for name, expected in {
        "widthLogical_": 420,
        "rowHeightLogical_": 16,
        "maxResults_": 10,
    }.items():
        found = re.search(
            rf"\b{re.escape(name)}\s*\{{(\d+)\}}",
            launcher_header,
        )
        if not found or int(found.group(1)) != expected:
            fail(f"Classic geometry changed during v0.6 rc.1: {name}")

    upgrade_tests = read("tests/UpgradeMatrixTests.cpp")
    for token in (
        "AssertCleanInstall",
        "v0.5.0-schema3.json",
        "v0.6.0-alpha.5-schema3-conflict.json",
        "v0.6.0-alpha.6.1-schema4.json",
        "v0.6.0-beta.1-schema4.json",
        "v0.6.0-beta.2-schema4.json",
        "AssertDowngradeReadOnly",
    ):
        if token not in upgrade_tests:
            fail(f"v0.6 rc.1 upgrade matrix gate missing: {token}")

    fixture_expectations = {
        "tests/fixtures/upgrade/v0.5.0-schema3.json": 3,
        "tests/fixtures/upgrade/v0.6.0-alpha.5-schema3-conflict.json": 3,
        "tests/fixtures/upgrade/v0.6.0-alpha.6.1-schema4.json": 4,
        "tests/fixtures/upgrade/v0.6.0-beta.1-schema4.json": 4,
        "tests/fixtures/upgrade/v0.6.0-beta.2-schema4.json": 4,
    }
    for path, expected_schema in fixture_expectations.items():
        fixture = json.loads(read(path))
        if fixture.get("schemaVersion") != expected_schema:
            fail(f"{path} has wrong upgrade-matrix schemaVersion")

    cmake = read("CMakeLists.txt")
    workflow = read(".github/workflows/build.yml")
    for token in (
        "upgrade_matrix_tests",
        "tests/fixtures/upgrade",
    ):
        if token not in cmake:
            fail(f"v0.6 rc.1 CMake upgrade gate missing: {token}")
    if workflow.count("upgrade_matrix_tests") < 4:
        fail("v0.6 rc.1 Windows smoke/compat workflow does not gate upgrade_matrix_tests")

    runtime_smoke = read("scripts/verify_runtime_smoke.ps1")
    for token in (
        "schemaVersion -ne 4",
        "launcher.activate",
        "result.navigateCurrentFileManager",
        "everything.filesystem",
        "schema 2 -> 4",
    ):
        if token not in runtime_smoke:
            fail(f"v0.6 rc.1 packaged runtime migration gate missing: {token}")

    package_contract = read("scripts/verify_package.ps1")
    if "V0.6_RC_VALIDATION.md" not in package_contract:
        fail("v0.6 rc.1 package contract does not require V0.6_RC_VALIDATION.md")
    if "V0.6_RC_VALIDATION.md" not in workflow:
        fail("v0.6 rc.1 workflow does not package V0.6_RC_VALIDATION.md")

    print(
        "v0.6.0-rc.1 release freeze verified:",
        "| settings=4 commands=1 usage=1 provider-cache=2",
        "| frozen providers/Hotkey IDs/Everything/Smart Actions/Diagnostics",
        "| Classic 420/16/10",
        "| clean install + historical upgrade matrix + downgrade readonly",
        "| packaged schema2->4 runtime migration + package contract",
    )
    raise SystemExit(0)


if version == "0.6.0-beta.2":
    expected_schemas = {
        "kSettingsSchemaVersion": 4,
        "kCommandsSchemaVersion": 1,
        "kUsageSchemaVersion": 1,
    }
    for name, expected in expected_schemas.items():
        actual = cpp_int("src/core/ConfigIO.hpp", name)
        if actual != expected:
            fail(f"{name}={actual}, expected v0.6 beta.2 value {expected}")

    if cpp_int("src/core/ProviderCache.cpp", "kProviderCacheSchemaVersion") != 2:
        fail("v0.6 beta.2 must keep provider-cache schemaVersion 2")

    settings = json.loads(read("config/settings.example.json"))
    expected_providers = {
        "windows.startmenu": True,
        "windows.packaged": True,
        "windows.apppaths": True,
        "windows.path": True,
        "everything.filesystem": False,
    }
    if settings.get("schemaVersion") != 4:
        fail("v0.6 beta.2 settings must remain schemaVersion 4")
    if settings.get("providers") != expected_providers:
        fail("v0.6 beta.2 changed frozen provider defaults")

    expected_bindings = {
        "launcher.activate",
        "launcher.activateSecondary",
        "launcher.openSettings",
        "result.navigateCurrentFileManager",
        "result.copySelectedTarget",
    }
    bindings = settings.get("hotkeys", {}).get("bindings", {})
    if set(bindings) != expected_bindings:
        fail("v0.6 beta.2 changed frozen Hotkey Registry action IDs")

    settings_header = read("src/ui/SettingsWindow.hpp")
    settings_ui = settings_header + read("src/ui/SettingsWindow.cpp")

    page_enum_start = settings_header.find("enum class Page")
    page_enum_end = settings_header.find("};", page_enum_start)
    if (
        page_enum_start < 0
        or page_enum_end < 0
        or "Diagnostics," not in settings_header[page_enum_start:page_enum_end]
        or "Actions," in settings_header[page_enum_start:page_enum_end]
    ):
        fail("beta.2 Page enum did not complete the Actions -> Diagnostics rename")

    for token in (
        "Page::Diagnostics",
        "kIdNavDiagnostics",
        "navDiagnostics_",
        "CreateDiagnosticsPage",
        "diagnosticsControls_",
        'L"诊断"',
        'L"Diagnostics"',
    ):
        if token not in settings_ui:
            fail(f"beta.2 Diagnostics UI contract missing: {token}")

    draw_start = settings_ui.find("void SettingsWindow::DrawNavigationButton")
    draw_end = settings_ui.find("void SettingsWindow::DrawGeneralToggle", draw_start)
    if draw_start < 0 or draw_end < 0 or "kIdNavDiagnostics" not in settings_ui[draw_start:draw_end]:
        fail("beta.2 Diagnostics nav ID is missing from DrawNavigationButton")

    dispatch_start = settings_ui.find("case WM_DRAWITEM")
    dispatch_end = settings_ui.find("case WM_VSCROLL", dispatch_start)
    if dispatch_start < 0 or dispatch_end < 0 or "kIdNavDiagnostics" not in settings_ui[dispatch_start:dispatch_end]:
        fail("beta.2 Diagnostics nav ID is missing from WM_DRAWITEM dispatch")

    launcher_header = read("src/ui/LauncherWindow.hpp")
    for name, expected in {"widthLogical_": 420, "rowHeightLogical_": 16, "maxResults_": 10}.items():
        found = re.search(rf"\b{re.escape(name)}\s*\{{(\d+)\}}", launcher_header)
        if not found or int(found.group(1)) != expected:
            fail(f"Classic geometry changed during v0.6 beta.2: {name}")

    print(
        "v0.6.0-beta.2 Diagnostics UX contract verified:",
        "| settings=4 provider-cache=2",
        "| frozen providers/Hotkey IDs/Classic geometry",
        "| Diagnostics owner-draw + WM_DRAWITEM dispatch",
    )
    raise SystemExit(0)


if version == "0.6.0-beta.1":
    expected_schemas = {
        "kSettingsSchemaVersion": 4,
        "kCommandsSchemaVersion": 1,
        "kUsageSchemaVersion": 1,
    }
    for name, expected in expected_schemas.items():
        actual = cpp_int("src/core/ConfigIO.hpp", name)
        if actual != expected:
            fail(f"{name}={actual}, expected v0.6 beta.1 value {expected}")

    if cpp_int("src/core/ProviderCache.cpp", "kProviderCacheSchemaVersion") != 2:
        fail("v0.6 beta.1 must keep provider-cache schemaVersion 2")

    settings = json.loads(read("config/settings.example.json"))
    expected_providers = {
        "windows.startmenu": True,
        "windows.packaged": True,
        "windows.apppaths": True,
        "windows.path": True,
        "everything.filesystem": False,
    }
    if settings.get("schemaVersion") != 4:
        fail("v0.6 beta.1 settings must remain schemaVersion 4")
    if settings.get("providers") != expected_providers:
        fail("v0.6 beta.1 changed frozen provider defaults")

    expected_bindings = {
        "launcher.activate",
        "launcher.activateSecondary",
        "launcher.openSettings",
        "result.navigateCurrentFileManager",
        "result.copySelectedTarget",
    }
    bindings = settings.get("hotkeys", {}).get("bindings", {})
    if set(bindings) != expected_bindings:
        fail("v0.6 beta.1 changed frozen Hotkey Registry action IDs")

    policy = read("src/core/LauncherActionPolicy.hpp") + read("src/core/LauncherActionPolicy.cpp")
    for token in (
        "ActionEvaluation",
        "ActionUnavailableReason",
        "EvaluateLauncherAction",
        "ResultNotFolder",
        "NoSupportedFileManager",
        "NoCopyableTarget",
        "InvalidActionTarget",
    ):
        if token not in policy:
            fail(f"beta.1 ActionEvaluation contract missing: {token}")

    app = read("src/app/App.cpp") + read("src/app/App.hpp")
    for token in ("EvaluateLauncherAction", "lastActivationContext_", "LastActivationContext"):
        if token not in app:
            fail(f"beta.1 App diagnostics integration missing: {token}")

    settings_ui = read("src/ui/SettingsWindow.hpp") + read("src/ui/SettingsWindow.cpp")
    for token in (
        "Page::Actions",
        "kIdNavActions",
        "CreateActionsPage",
        "RefreshActionDiagnostics",
        "Last activation context:",
        "Runtime status:",
    ):
        if token not in settings_ui:
            fail(f"beta.1 Actions/diagnostics UI missing: {token}")

    policy_tests = read("tests/LauncherActionPolicyTests.cpp")
    for token in (
        "EvaluateLauncherAction",
        "NoSupportedFileManager",
        "ResultNotFolder",
        "NoCopyableTarget",
        "NavigateFileDialog",
    ):
        if token not in policy_tests:
            fail(f"beta.1 action evaluation regression missing: {token}")

    windows_tests = read("tests/WindowsContextRuntimeTests.cpp")
    for token in ("gTotalCommanderRightPath", "gTotalCommanderActivePanel = 2", "Folder With Spaces", "uncTarget"):
        if token not in windows_tests:
            fail(f"beta.1 Windows context hardening missing: {token}")

    config_tests = read("tests/ConfigCoreTests.cpp")
    for token in (
        "settings-v0.6-alpha5-hotkeys.json",
        "settings-v0.6-alpha5-hotkey-conflict.json",
        "schema4BeforeDowngrade",
        "conflictNavigate",
    ):
        if token not in config_tests:
            fail(f"beta.1 migration/downgrade coverage missing: {token}")

    launcher_header = read("src/ui/LauncherWindow.hpp")
    for name, expected in {"widthLogical_": 420, "rowHeightLogical_": 16, "maxResults_": 10}.items():
        found = re.search(rf"\b{re.escape(name)}\s*\{{(\d+)\}}", launcher_header)
        if not found or int(found.group(1)) != expected:
            fail(f"Classic geometry changed during v0.6 beta.1: {name}")

    print(
        "v0.6.0-beta.1 Smart Actions UX/diagnostics contract verified:",
        "| settings=4 commands=1 usage=1 provider-cache=2",
        "| frozen Hotkey IDs/providers/Classic geometry",
        "| ActionEvaluation | Actions Settings diagnostics",
        "| TC right-panel UNC/Unicode runtime coverage",
    )
    raise SystemExit(0)


if version in ("0.6.0-alpha.6", "0.6.0-alpha.6.1"):
    expected_schemas = {
        "kSettingsSchemaVersion": 4,
        "kCommandsSchemaVersion": 1,
        "kUsageSchemaVersion": 1,
    }
    for name, expected in expected_schemas.items():
        actual = cpp_int("src/core/ConfigIO.hpp", name)
        if actual != expected:
            fail(
                f"{name}={actual}, expected v0.6 alpha.6 value {expected}"
            )

    if cpp_int(
        "src/core/ProviderCache.cpp",
        "kProviderCacheSchemaVersion",
    ) != 2:
        fail("v0.6 alpha.6 must keep provider-cache schemaVersion 2")

    settings = json.loads(read("config/settings.example.json"))
    expected_providers = {
        "windows.startmenu": True,
        "windows.packaged": True,
        "windows.apppaths": True,
        "windows.path": True,
        "everything.filesystem": False,
    }
    if settings.get("schemaVersion") != 4:
        fail("v0.6 alpha.6 settings must be schemaVersion 4")
    if settings.get("providers") != expected_providers:
        fail("v0.6 alpha.6 changed frozen provider defaults")

    bindings = (
        settings.get("hotkeys", {})
        .get("bindings", {})
    )
    expected_bindings = {
        "launcher.activate",
        "launcher.activateSecondary",
        "launcher.openSettings",
        "result.navigateCurrentFileManager",
        "result.copySelectedTarget",
    }
    if set(bindings) != expected_bindings:
        fail(
            "schema-4 hotkey binding IDs differ from the frozen alpha.6 registry"
        )

    registry_hpp = read("src/core/HotkeyRegistry.hpp")
    registry_cpp = read("src/core/HotkeyRegistry.cpp")
    for token in (
        "launcher.activate",
        "launcher.activateSecondary",
        "launcher.openSettings",
        "result.navigateCurrentFileManager",
        "result.copySelectedTarget",
        "HotkeyScope",
        "FindHotkeyConflict",
        "MatchHotkeyAction",
        "ValidateHotkeyBinding",
    ):
        if token not in registry_hpp + registry_cpp:
            fail(f"alpha.6 Hotkey Registry missing: {token}")

    settings_cpp = read("src/core/Settings.cpp")
    for token in (
        'root.contains("hotkeys")',
        '"bindings"',
        "ImportLegacyHotkeys",
        "ResolveHotkeyBindingConflicts",
        "SyncLegacyHotkeyMirrors",
        "ResetHotkeyBindings",
        'hotkey_actions::kActivate',
        "kActivateSecondary",
    ):
        if token not in settings_cpp:
            fail(f"alpha.6 settings migration/persistence missing: {token}")

    app = read("src/app/App.cpp")
    for token in (
        "SetHotkeyBinding",
        "ResetHotkeyBindings",
        "RebindGlobalHotkey",
        "RebindAuxiliaryHotkey",
        "FindHotkeyConflict",
    ):
        if token not in app:
            fail(f"alpha.6 App hotkey transaction missing: {token}")

    launcher = read("src/ui/LauncherWindow.cpp")
    for token in (
        "MatchHotkeyAction",
        "kOpenSettings",
        "kNavigateCurrentFileManager",
        "kCopySelectedTarget",
        "WM_SYSKEYDOWN",
    ):
        if token not in launcher:
            fail(f"alpha.6 launcher registry dispatch missing: {token}")

    settings_hpp = read("src/ui/SettingsWindow.hpp")
    settings_ui = read("src/ui/SettingsWindow.cpp")
    for token in (
        "Page::Hotkeys",
        "kIdNavHotkeys",
        "CreateHotkeyPage",
        "RefreshHotkeyPage",
        "BeginHotkeyCapture",
        "ApplyCapturedHotkey",
        "ResetAllHotkeys",
        "Press the new shortcut",
    ):
        if token not in settings_hpp + settings_ui:
            fail(f"alpha.6 centralized Hotkeys Settings UI missing: {token}")

    config_tests = read("tests/ConfigCoreTests.cpp")
    for token in (
        "settings-v0.6-alpha5-hotkeys.json",
        "settings-v0.6-alpha5-hotkey-conflict.json",
        "MigratedFromSchemaVersion",
        "schema4BeforeDowngrade",
        "alpha5DowngradeRead",
        "conflictNavigate",
    ):
        if token not in config_tests:
            fail(f"alpha.6 schema-3 -> 4 migration coverage missing: {token}")

    registry_tests = read("tests/HotkeyRegistryTests.cpp")
    for token in (
        "FindHotkeyConflict",
        "MatchHotkeyAction",
        "ValidateHotkeyBinding",
    ):
        if token not in registry_tests:
            fail(f"alpha.6 registry tests missing: {token}")

    for workflow_path in (
        ".github/workflows/build.yml",
        ".github/workflows/release.yml",
    ):
        workflow = read(workflow_path)
        if "hotkey_registry_tests" not in workflow:
            fail(
                f"{workflow_path} does not run hotkey_registry_tests on Windows"
            )

    launcher_header = read("src/ui/LauncherWindow.hpp")
    for name, expected in {
        "widthLogical_": 420,
        "rowHeightLogical_": 16,
        "maxResults_": 10,
    }.items():
        found = re.search(
            rf"\b{re.escape(name)}\s*\{{(\d+)\}}",
            launcher_header,
        )
        if not found or int(found.group(1)) != expected:
            fail(f"Classic geometry changed in v0.6 alpha.6: {name}")

    print(
        "v0.6.0-alpha.6 centralized Hotkey Registry contract verified:",
        "| settings=4 commands=1 usage=1 provider-cache=2",
        "| global registration rollback | launcher-local dispatch",
        "| schema3 migration/downgrade mirror | Hotkeys Settings page",
    )
    raise SystemExit(0)


if version == "0.6.0-alpha.5":
    expected_schemas = {
        "kSettingsSchemaVersion": 3,
        "kCommandsSchemaVersion": 1,
        "kUsageSchemaVersion": 1,
    }
    for name, expected in expected_schemas.items():
        actual = cpp_int("src/core/ConfigIO.hpp", name)
        if actual != expected:
            fail(
                f"{name}={actual}, expected v0.6 alpha.5 value {expected}"
            )

    if cpp_int(
        "src/core/ProviderCache.cpp",
        "kProviderCacheSchemaVersion",
    ) != 2:
        fail("v0.6 alpha.5 must keep provider-cache schemaVersion 2")

    settings = json.loads(read("config/settings.example.json"))
    expected_providers = {
        "windows.startmenu": True,
        "windows.packaged": True,
        "windows.apppaths": True,
        "windows.path": True,
        "everything.filesystem": False,
    }
    if settings.get("schemaVersion") != 3:
        fail("v0.6 alpha.5 settings must remain schemaVersion 3")
    if settings.get("providers") != expected_providers:
        fail("v0.6 alpha.5 changed frozen provider defaults")

    provider_ids = read("src/core/ProviderIds.hpp")
    for token in (
        '"builtin.web"',
        '"builtin.clipboard"',
    ):
        if token not in provider_ids:
            fail(f"alpha.5 runtime provider missing: {token}")

    settings_text = read("config/settings.example.json")
    if "builtin.clipboard" in settings_text:
        fail("builtin.clipboard must remain runtime-only")
    default_enabled_block = provider_ids.split(
        "DefaultEnabled()", 1
    )[1]
    if "kBuiltinClipboard" in default_enabled_block:
        fail("builtin.clipboard must not enter persisted provider defaults")

    result_contract = read("src/core/LauncherResult.hpp")
    for token in (
        "CopyText",
        "CopySelectedText",
        "NavigateCurrentFileManager",
        "NavigateFileDialog",
        "NavigateTotalCommander",
    ):
        if token not in result_contract:
            fail(f"alpha.5 action contract missing: {token}")

    clipboard_action = read("src/core/ClipboardAction.cpp")
    for token in (
        "BuildClipboardActionResults",
        'L"copy"',
        'L"clip"',
        'L"复制"',
        "kBuiltinClipboard",
        "LauncherActionKind::",
        "CopyText",
        "result.score = 1500",
    ):
        if token not in clipboard_action:
            fail(f"alpha.5 clipboard smart action missing: {token}")

    action_policy = read("src/core/LauncherActionPolicy.cpp")
    for token in (
        "CopySelectedText",
        "CopyText",
        "result.action.payload",
        "result.target",
        "commandIndex",
    ):
        if token not in action_policy:
            fail(f"alpha.5 copy-selection policy missing: {token}")

    clipboard_platform = read("src/platform/WinClipboard.cpp")
    for token in (
        "OpenClipboard",
        "EmptyClipboard",
        "GlobalAlloc",
        "GlobalLock",
        "CF_UNICODETEXT",
        "SetClipboardData",
        "CloseClipboard",
    ):
        if token not in clipboard_platform:
            fail(f"alpha.5 Win32 clipboard integration missing: {token}")

    app = read("src/app/App.cpp")
    for token in (
        "BuildClipboardActionResults",
        "CopyTextAction",
        "SetClipboardUnicodeText",
        "UnableToCopy",
        "runtimeActions",
    ):
        if token not in app:
            fail(f"alpha.5 App clipboard integration missing: {token}")

    launcher = read("src/ui/LauncherWindow.cpp")
    for token in (
        "VK_CONTROL",
        "VK_SHIFT",
        "L'C'",
        "CopySelectedText",
    ):
        if token not in launcher:
            fail(f"alpha.5 Ctrl+Shift+C shortcut missing: {token}")

    action_tests = read("tests/ClipboardActionTests.cpp")
    for token in (
        "copy hello world",
        "builtin.clipboard",
        "CopyText",
        "copyright notice",
    ):
        if token not in action_tests:
            fail(f"alpha.5 clipboard action tests missing: {token}")

    runtime_tests = read("tests/ClipboardRuntimeTests.cpp")
    for token in (
        "SetClipboardUnicodeText",
        "GetClipboardData",
        "CF_UNICODETEXT",
        "GlobalLock",
    ):
        if token not in runtime_tests:
            fail(f"alpha.5 clipboard runtime smoke missing: {token}")

    cmake = read("CMakeLists.txt")
    for token in (
        "src/core/ClipboardAction.cpp",
        "src/platform/WinClipboard.cpp",
        "clipboard_action_tests",
        "clipboard_runtime_tests",
    ):
        if token not in cmake:
            fail(f"alpha.5 build/test wiring missing: {token}")

    for workflow_path in (
        ".github/workflows/build.yml",
        ".github/workflows/release.yml",
    ):
        workflow = read(workflow_path)
        for test_name in (
            "clipboard_action_tests",
            "clipboard_runtime_tests",
        ):
            if test_name not in workflow:
                fail(
                    f"{workflow_path} does not run {test_name} on Windows"
                )

    launcher_header = read("src/ui/LauncherWindow.hpp")
    for name, expected in {
        "widthLogical_": 420,
        "rowHeightLogical_": 16,
        "maxResults_": 10,
    }.items():
        found = re.search(
            rf"\b{re.escape(name)}\s*\{{(\d+)\}}",
            launcher_header,
        )
        if not found or int(found.group(1)) != expected:
            fail(f"Classic geometry changed in v0.6 alpha.5: {name}")

    print(
        "v0.6.0-alpha.5 clipboard/text action contract verified:",
        "| schemas unchanged | Classic 420/16/10",
        "| copy/clip text action | Ctrl+Shift+C selected target",
        "| Unicode Win32 clipboard runtime smoke",
    )
    raise SystemExit(0)


if version == "0.6.0-alpha.4":
    expected_schemas = {
        "kSettingsSchemaVersion": 3,
        "kCommandsSchemaVersion": 1,
        "kUsageSchemaVersion": 1,
    }
    for name, expected in expected_schemas.items():
        actual = cpp_int("src/core/ConfigIO.hpp", name)
        if actual != expected:
            fail(
                f"{name}={actual}, expected v0.6 alpha.4 value {expected}"
            )

    if cpp_int(
        "src/core/ProviderCache.cpp",
        "kProviderCacheSchemaVersion",
    ) != 2:
        fail("v0.6 alpha.4 must keep provider-cache schemaVersion 2")

    settings = json.loads(read("config/settings.example.json"))
    expected_providers = {
        "windows.startmenu": True,
        "windows.packaged": True,
        "windows.apppaths": True,
        "windows.path": True,
        "everything.filesystem": False,
    }
    if settings.get("schemaVersion") != 3:
        fail("v0.6 alpha.4 settings must remain schemaVersion 3")
    if settings.get("providers") != expected_providers:
        fail("v0.6 alpha.4 changed frozen provider defaults")

    result_contract = read("src/core/LauncherResult.hpp")
    for token in (
        "NavigateTotalCommander",
        "NavigateCurrentFileManager",
        "NavigateCurrentExplorer",
        "NavigateFileDialog",
    ):
        if token not in result_contract:
            fail(f"alpha.4 action contract missing: {token}")

    action_policy = read("src/core/LauncherActionPolicy.cpp")
    for token in (
        "totalCommanderContextAvailable",
        "NavigateTotalCommander",
        "NavigateCurrentFileManager",
        "NavigateExplorer",
        "NavigateFileDialog",
        "ResultKind::Folder",
    ):
        if token not in action_policy:
            fail(f"alpha.4 action policy missing: {token}")

    context_header = read("src/platform/WindowsContext.hpp")
    for token in (
        "TotalCommander",
        "totalCommanderWindow",
        "totalCommanderProcessId",
        "totalCommanderActivePanel",
        "totalCommanderFolder",
        "HasTotalCommander",
        "CurrentFilesystemFolder",
        "NavigateTotalCommanderToFolder",
    ):
        if token not in context_header:
            fail(f"alpha.4 Total Commander context missing: {token}")

    windows_context = read("src/platform/WindowsContext.cpp")
    for token in (
        'L"TTOTAL_CMD"',
        "WM_USER + 50",
        "1000",
        "kTotalCommanderLeftPathControl",
        "kTotalCommanderRightPathControl",
        "WM_GETTEXT",
        "WM_COPYDATA",
        "kTotalCommanderChangeDirectory",
        "WideCharToMultiByte",
        "0xEF",
        "0xBB",
        "0xBF",
        "NavigateTotalCommanderToFolder",
    ):
        if token not in windows_context:
            fail(f"alpha.4 Total Commander integration missing: {token}")

    if "FindWindowW" in windows_context:
        fail(
            "alpha.4 must target the captured foreground Total Commander "
            "window, not a guessed global instance"
        )

    command_template = read("src/core/CommandTemplate.cpp")
    for token in (
        'L"{folder}"',
        "UsesFolderTemplate",
        "ResolveFolderTemplate",
        "command.target",
        "command.arguments",
        "command.workingDirectory",
    ):
        if token not in command_template:
            fail(f"alpha.4 {{folder}} template contract missing: {token}")

    app = read("src/app/App.cpp")
    for token in (
        "CurrentFilesystemFolder",
        "searchableCommands",
        "sourceIndices",
        "ResolveFolderTemplate",
        "UsesFolderTemplate",
        "HasTotalCommander",
        "NavigateTotalCommanderToFolder",
    ):
        if token not in app:
            fail(f"alpha.4 App integration missing: {token}")

    template_tests = read("tests/CommandTemplateTests.cpp")
    for token in (
        "{folder}",
        "workingDirectory",
        "\\\\server",
    ):
        if token not in template_tests:
            fail(f"alpha.4 command-template tests missing: {token}")

    runtime_tests = read("tests/WindowsContextRuntimeTests.cpp")
    for token in (
        'L"TTOTAL_CMD"',
        "WM_USER + 50",
        "WM_COPYDATA",
        "HasTotalCommander",
        "DecodeTotalCommanderPath",
    ):
        if token not in runtime_tests:
            fail(f"alpha.4 Total Commander runtime smoke missing: {token}")

    cmake = read("CMakeLists.txt")
    for token in (
        "src/core/CommandTemplate.cpp",
        "command_template_tests",
        "windows_context_runtime_tests",
    ):
        if token not in cmake:
            fail(f"alpha.4 build/test wiring missing: {token}")

    for workflow_path in (
        ".github/workflows/build.yml",
        ".github/workflows/release.yml",
    ):
        workflow = read(workflow_path)
        if "command_template_tests" not in workflow:
            fail(
                f"{workflow_path} does not run command_template_tests on Windows"
            )

    launcher_header = read("src/ui/LauncherWindow.hpp")
    for name, expected in {
        "widthLogical_": 420,
        "rowHeightLogical_": 16,
        "maxResults_": 10,
    }.items():
        found = re.search(
            rf"\b{re.escape(name)}\s*\{{(\d+)\}}",
            launcher_header,
        )
        if not found or int(found.group(1)) != expected:
            fail(f"Classic geometry changed in v0.6 alpha.4: {name}")

    print(
        "v0.6.0-alpha.4 Total Commander/{folder} contract verified:",
        "| schemas unchanged | Classic 420/16/10",
        "| captured TC active panel | WM_COPYDATA CD",
        "| contextual user-command templates",
    )
    raise SystemExit(0)


if version == "0.6.0-alpha.3":
    expected_schemas = {
        "kSettingsSchemaVersion": 3,
        "kCommandsSchemaVersion": 1,
        "kUsageSchemaVersion": 1,
    }
    for name, expected in expected_schemas.items():
        actual = cpp_int("src/core/ConfigIO.hpp", name)
        if actual != expected:
            fail(
                f"{name}={actual}, expected v0.6 alpha.3 value {expected}"
            )

    if cpp_int(
        "src/core/ProviderCache.cpp",
        "kProviderCacheSchemaVersion",
    ) != 2:
        fail("v0.6 alpha.3 must keep provider-cache schemaVersion 2")

    settings = json.loads(read("config/settings.example.json"))
    expected_providers = {
        "windows.startmenu": True,
        "windows.packaged": True,
        "windows.apppaths": True,
        "windows.path": True,
        "everything.filesystem": False,
    }
    if settings.get("schemaVersion") != 3:
        fail("v0.6 alpha.3 settings must remain schemaVersion 3")
    if settings.get("providers") != expected_providers:
        fail("v0.6 alpha.3 changed frozen provider defaults")

    launcher_result = read("src/core/LauncherResult.hpp")
    for token in (
        "NavigateExplorer",
        "NavigateFileDialog",
        "NavigateCurrentExplorer",
        "std::wstring payload",
    ):
        if token not in launcher_result:
            fail(f"alpha.3 action contract missing: {token}")

    action_policy = read("src/core/LauncherActionPolicy.cpp")
    for token in (
        "fileDialogContextAvailable",
        "LauncherExecutionIntent::Default",
        "NavigateFileDialog",
        "NavigateCurrentExplorer",
        "NavigateExplorer",
        "ResultKind::Folder",
    ):
        if token not in action_policy:
            fail(f"alpha.3 action policy missing: {token}")

    context_header = read("src/platform/WindowsContext.hpp")
    for token in (
        "WindowsContextKind",
        "FileDialog",
        "fileDialogWindow",
        "fileDialogProcessId",
        "HasFileDialog",
        "NavigateFileDialogToFolder",
    ):
        if token not in context_header:
            fail(f"alpha.3 Windows context contract missing: {token}")

    windows_context = read("src/platform/WindowsContext.cpp")
    for token in (
        'L"#32770"',
        'L"SHELLDLL_DefView"',
        "IsSupportedFileDialogWindow",
        "GetWindowThreadProcessId",
        "SetForegroundWindow",
        "GetForegroundWindow",
        "VK_CONTROL",
        "L'L'",
        "KEYEVENTF_UNICODE",
        "SendInput",
        "NavigateFileDialogToFolder",
    ):
        if token not in windows_context:
            fail(f"alpha.3 file-dialog integration missing: {token}")

    if "OpenClipboard" in windows_context or "SetClipboardData" in windows_context:
        fail("alpha.3 file-dialog navigation must not depend on clipboard mutation")

    app = read("src/app/App.cpp")
    for token in (
        "ResolveLauncherAction",
        "HasExplorer",
        "HasFileDialog",
        "NavigateExplorerToFolder",
        "NavigateFileDialogToFolder",
    ):
        if token not in app:
            fail(f"alpha.3 App contextual execution missing: {token}")

    policy_tests = read("tests/LauncherActionPolicyTests.cpp")
    for token in (
        "NavigateFileDialog",
        "file-dialog action",
        "OpenFile",
    ):
        if token not in policy_tests:
            fail(f"alpha.3 action policy tests missing: {token}")

    launcher_header = read("src/ui/LauncherWindow.hpp")
    for name, expected in {
        "widthLogical_": 420,
        "rowHeightLogical_": 16,
        "maxResults_": 10,
    }.items():
        found = re.search(
            rf"\b{re.escape(name)}\s*\{{(\d+)\}}",
            launcher_header,
        )
        if not found or int(found.group(1)) != expected:
            fail(f"Classic geometry changed in v0.6 alpha.3: {name}")

    print(
        "v0.6.0-alpha.3 file-dialog navigation contract verified:",
        "| schemas unchanged | Classic 420/16/10",
        "| Explorer Ctrl+Enter | File dialog Enter",
        "| no clipboard mutation",
    )
    raise SystemExit(0)


if version in ("0.6.0-alpha.2", "0.6.0-alpha.2.1"):
    expected_schemas = {
        "kSettingsSchemaVersion": 3,
        "kCommandsSchemaVersion": 1,
        "kUsageSchemaVersion": 1,
    }
    for name, expected in expected_schemas.items():
        actual = cpp_int("src/core/ConfigIO.hpp", name)
        if actual != expected:
            fail(
                f"{name}={actual}, expected v0.6 alpha.2 value {expected}"
            )

    if cpp_int(
        "src/core/ProviderCache.cpp",
        "kProviderCacheSchemaVersion",
    ) != 2:
        fail("v0.6 alpha.2 must keep provider-cache schemaVersion 2")

    provider_text = read("src/core/ProviderIds.hpp")
    for token in (
        '"windows.startmenu"',
        '"windows.packaged"',
        '"windows.apppaths"',
        '"windows.path"',
        '"everything.filesystem"',
        '"builtin.web"',
        "{std::string(kStartMenu), true}",
        "{std::string(kPackaged), true}",
        "{std::string(kAppPaths), true}",
        "{std::string(kPath), true}",
        "{std::string(kEverythingFilesystem), false}",
    ):
        if token not in provider_text:
            fail(f"v0.6 alpha.2 changed frozen provider/action IDs: {token}")

    settings = json.loads(read("config/settings.example.json"))
    expected_providers = {
        "windows.startmenu": True,
        "windows.packaged": True,
        "windows.apppaths": True,
        "windows.path": True,
        "everything.filesystem": False,
    }
    if settings.get("schemaVersion") != 3:
        fail("v0.6 alpha.2 settings must remain schemaVersion 3")
    if settings.get("providers") != expected_providers:
        fail("v0.6 alpha.2 changed frozen provider defaults")

    launcher_result = read("src/core/LauncherResult.hpp")
    for token in (
        "NavigateExplorer",
        "NavigateCurrentExplorer",
        "std::wstring payload",
    ):
        if token not in launcher_result:
            fail(f"Explorer action contract missing: {token}")

    action_policy = read("src/core/LauncherActionPolicy.cpp")
    for token in (
        "ResultKind::Folder",
        "NavigateCurrentExplorer",
        "NavigateExplorer",
        "explorerContextAvailable",
    ):
        if token not in action_policy:
            fail(f"Explorer action policy missing: {token}")

    selection = read("src/core/ExplorerContextSelection.cpp")
    for token in (
        "hasShellView",
        "focused.size() == 1",
        "visible.size() == 1",
        "valid.size() == 1",
        "return std::nullopt",
    ):
        if token not in selection:
            fail(f"Explorer ambiguity policy missing: {token}")

    windows_context = read("src/platform/WindowsContext.cpp")
    for token in (
        "CLSID_ShellWindows",
        "SID_STopLevelBrowser",
        "QueryActiveShellView",
        "GetActiveViewContext",
        "IPersistFolder2",
        "SHGetPathFromIDListEx",
        "GetGUIThreadInfo",
        "Navigate2",
        "SetForegroundWindow",
    ):
        if token not in windows_context:
            fail(f"Windows Explorer context integration missing: {token}")

    context_header = read("src/platform/WindowsContext.hpp")
    has_explorer_body = re.search(
        r"HasExplorer\(\) const noexcept \{(.*?)\n    \}",
        context_header,
        re.DOTALL,
    )
    if not has_explorer_body:
        fail("WindowsContextSnapshot::HasExplorer contract was not found")
    if "explorerFolder" in has_explorer_body.group(1):
        fail(
            "Explorer source context must not require a filesystem path; "
            "Home/This PC/Quick access must remain navigable sources"
        )

    app = read("src/app/App.cpp")
    capture_pos = app.find("CaptureActivationContext();")
    toggle_pos = app.find("window_->Toggle();", capture_pos)
    if capture_pos < 0 or toggle_pos < 0 or capture_pos > toggle_pos:
        fail("activation context must be captured before Launcher takes focus")

    for token in (
        "ResolveLauncherAction",
        "activationContext_",
        ".HasExplorer()",
        "NavigateExplorerToFolder",
        "GetForegroundWindow",
    ):
        if token not in app:
            fail(f"App Explorer-context integration missing: {token}")

    launcher = read("src/ui/LauncherWindow.cpp")
    for token in (
        "VK_RETURN",
        "VK_CONTROL",
        "NavigateCurrentExplorer",
        "ClearActivationContext",
    ):
        if token not in launcher:
            fail(f"Ctrl+Enter Explorer UX contract missing: {token}")

    cmake = read("CMakeLists.txt")
    for token in (
        "src/platform/WindowsContext.cpp",
        "src/core/LauncherActionPolicy.cpp",
        "src/core/ExplorerContextSelection.cpp",
        "windows_context_runtime_tests",
        "launcher_action_policy_tests",
        "explorer_context_selection_tests",
        "oleaut32",
    ):
        if token not in cmake:
            fail(f"Explorer alpha.2 build/test wiring missing: {token}")

    for workflow_path in (
        ".github/workflows/build.yml",
        ".github/workflows/release.yml",
    ):
        workflow = read(workflow_path)
        for test_name in (
            "windows_context_runtime_tests",
            "launcher_action_policy_tests",
            "explorer_context_selection_tests",
        ):
            if test_name not in workflow:
                fail(
                    f"{workflow_path} does not run {test_name} on Windows"
                )

    launcher_header = read("src/ui/LauncherWindow.hpp")
    for name, expected in {
        "widthLogical_": 420,
        "rowHeightLogical_": 16,
        "maxResults_": 10,
    }.items():
        found = re.search(
            rf"\b{re.escape(name)}\s*\{{(\d+)\}}",
            launcher_header,
        )
        if not found or int(found.group(1)) != expected:
            fail(f"Classic geometry changed in v0.6 alpha.2: {name}")

    print(
        "v0.6.0-alpha.2 Explorer context/navigation contract verified:",
        "| schemas unchanged | Classic 420/16/10",
        "| hotkey capture before focus | Ctrl+Enter current Explorer",
    )
    raise SystemExit(0)


if version == "0.6.0-alpha.1":
    expected_schemas = {
        "kSettingsSchemaVersion": 3,
        "kCommandsSchemaVersion": 1,
        "kUsageSchemaVersion": 1,
    }
    for name, expected in expected_schemas.items():
        actual = cpp_int("src/core/ConfigIO.hpp", name)
        if actual != expected:
            fail(
                f"{name}={actual}, expected v0.6 alpha.1 value {expected}"
            )

    if cpp_int(
        "src/core/ProviderCache.cpp",
        "kProviderCacheSchemaVersion",
    ) != 2:
        fail("v0.6 alpha.1 must keep provider-cache schemaVersion 2")

    provider_text = read("src/core/ProviderIds.hpp")
    for token in (
        '"windows.startmenu"',
        '"windows.packaged"',
        '"windows.apppaths"',
        '"windows.path"',
        '"everything.filesystem"',
        '"builtin.web"',
        "{std::string(kStartMenu), true}",
        "{std::string(kPackaged), true}",
        "{std::string(kAppPaths), true}",
        "{std::string(kPath), true}",
        "{std::string(kEverythingFilesystem), false}",
    ):
        if token not in provider_text:
            fail(f"v0.6 alpha.1 provider/action ID contract missing: {token}")

    settings = json.loads(read("config/settings.example.json"))
    expected_providers = {
        "windows.startmenu": True,
        "windows.packaged": True,
        "windows.apppaths": True,
        "windows.path": True,
        "everything.filesystem": False,
    }
    if settings.get("schemaVersion") != 3:
        fail("v0.6 alpha.1 settings must remain schemaVersion 3")
    if settings.get("providers") != expected_providers:
        fail("v0.6 alpha.1 changed frozen provider defaults")
    if "builtin.web" in settings.get("providers", {}):
        fail("builtin.web is runtime-only and must not enter provider settings")

    launcher_result = read("src/core/LauncherResult.hpp")
    for token in (
        "    Action,",
        "OpenUrl",
        "std::wstring payload",
    ):
        if token not in launcher_result:
            fail(f"smart-action contract missing: {token}")

    web_action = read("src/core/WebAction.cpp")
    for token in (
        'L"{query}"',
        "PercentEncodeQuery",
        "CommandType::Url",
        "providers::kBuiltinWeb",
        "LauncherActionKind::OpenUrl",
        "http://",
        "https://",
    ):
        if token not in web_action:
            fail(f"web-action foundation missing: {token}")

    app = read("src/app/App.cpp")
    for token in (
        "BuildWebActionResults",
        "MergeLauncherResultsRanked",
        "LauncherActionKind::OpenUrl",
        "result.action.payload",
    ):
        if token not in app:
            fail(f"App smart-action integration missing: {token}")

    everything = read("src/core/EverythingProvider.cpp")
    if "result.action.payload" not in everything:
        fail("Everything file/folder actions must populate the action payload")

    tests = read("tests/WebActionTests.cpp")
    for token in (
        "ALTRun%20Next",
        "%E5%BF%83%E8%84%8F%20MRI",
        "WWW.Example.com",
        "file:///tmp/{query}",
    ):
        if token not in tests:
            fail(f"web-action regression coverage missing: {token}")

    cmake = read("CMakeLists.txt")
    for token in (
        "src/core/WebAction.cpp",
        "web_action_tests",
    ):
        if token not in cmake:
            fail(f"web-action build/test wiring missing: {token}")

    for workflow_path in (
        ".github/workflows/build.yml",
        ".github/workflows/release.yml",
    ):
        if "web_action_tests" not in read(workflow_path):
            fail(
                f"{workflow_path} does not run web_action_tests on Windows"
            )

    launcher = read("src/ui/LauncherWindow.hpp")
    for name, expected in {
        "widthLogical_": 420,
        "rowHeightLogical_": 16,
        "maxResults_": 10,
    }.items():
        found = re.search(
            rf"\b{re.escape(name)}\s*\{{(\d+)\}}",
            launcher,
        )
        if not found or int(found.group(1)) != expected:
            fail(f"Classic geometry changed in v0.6 alpha.1: {name}")

    print(
        "v0.6.0-alpha.1 smart-action/web contract verified:",
        "| schemas unchanged | Classic 420/16/10",
        "| builtin.web runtime-only | URL aliases + direct URL actions",
    )
    raise SystemExit(0)


if version == "0.5.0":
    expected_schemas = {
        "kSettingsSchemaVersion": 3,
        "kCommandsSchemaVersion": 1,
        "kUsageSchemaVersion": 1,
    }
    for name, expected in expected_schemas.items():
        actual = cpp_int("src/core/ConfigIO.hpp", name)
        if actual != expected:
            fail(
                f"{name}={actual}, expected frozen Stable value {expected}"
            )

    if cpp_int(
        "src/core/ProviderCache.cpp",
        "kProviderCacheSchemaVersion",
    ) != 2:
        fail("provider-cache schema must remain 2 in stable v0.5.0")

    provider_text = read("src/core/ProviderIds.hpp")
    for token in (
        '"windows.startmenu"',
        '"windows.packaged"',
        '"windows.apppaths"',
        '"windows.path"',
        '"everything.filesystem"',
        "{std::string(kStartMenu), true}",
        "{std::string(kPackaged), true}",
        "{std::string(kAppPaths), true}",
        "{std::string(kPath), true}",
        "{std::string(kEverythingFilesystem), false}",
    ):
        if token not in provider_text:
            fail(f"Stable provider/default freeze missing: {token}")

    settings = json.loads(read("config/settings.example.json"))
    if settings.get("schemaVersion") != 3:
        fail("Stable settings.example.json must remain schemaVersion 3")

    launcher_hpp = read("src/ui/LauncherWindow.hpp")
    for name, expected in {
        "widthLogical_": 420,
        "rowHeightLogical_": 16,
        "maxResults_": 10,
    }.items():
        found = re.search(
            rf"\b{re.escape(name)}\s*\{{(\d+)\}}",
            launcher_hpp,
        )
        if not found or int(found.group(1)) != expected:
            fail(f"Classic geometry changed during stable promotion: {name}")

    layout_h = read("src/core/SettingsLayout.hpp")
    layout_cpp = read("src/core/SettingsLayout.cpp")
    for token in (
        "kContentLeftInsetLogical = 38",
        "kContentRightInsetLogical = 34",
        "kToggleRowLogical = 54",
    ):
        if token not in layout_h:
            fail(f"Stable polished Settings metric missing: {token}")
    for token in (
        "kContentLeftInsetLogical",
        "kContentRightInsetLogical",
        "kToggleRowLogical",
    ):
        if token not in layout_cpp:
            fail(f"Stable Settings layout helper is not using: {token}")

    settings_ui = read("src/ui/SettingsWindow.cpp")
    for token in (
        "SS_LEFTNOWORDWRAP | SS_NOPREFIX",
        "labelWidth = Scale(132)",
        "fieldWidth * 24 / 100",
        "fieldWidth * 22 / 100",
        "const int actionWidth",
        "settings_layout::",
        "kToggleRowLogical",
        "Scale(108)",
        "providerCard.bottom +",
        "Scale(140)",
        "Scale(188)",
        "rect.top + Scale(27)",
        "rect.top + Scale(29)",
        "Scale(960)",
        "BS_OWNERDRAW",
        "DrawNavigationButton",
        "RGB(232, 241, 250)",
        "Everything not detected",
        "Get Everything",
        "Recheck",
    ):
        if token not in settings_ui:
            fail(f"Stable Settings polish integration missing: {token}")

    if "const int behaviorRowHeight =\n            Scale(46)" in settings_ui:
        fail("Stable regressed General owner-draw rows back to 46px")
    if 'page_ == page ? L"●  "' in settings_ui:
        fail("Stable regressed to text-bullet navigation selection")

    desktop_test = read("tests/DesktopValidationTests.cpp")
    for token in (
        "kToggleRowLogical >= 54",
        "kContentLeftInsetLogical == 38",
        "kContentRightInsetLogical == 34",
        "wide.behavior.bottom -",
        "wide.search.bottom -",
        "96, 120, 144, 192",
    ):
        if token not in desktop_test:
            fail(f"Stable DPI/layout regression coverage missing: {token}")

    registry = read("src/core/ProviderRegistry.cpp")
    if "everything.filesystem" in registry or "kEverythingFilesystem" in registry:
        fail(
            "Everything must remain a Dynamic Query Provider outside "
            "the static ProviderRegistry during stable promotion"
        )

    protocol_h = read("src/core/EverythingIpcProtocol.hpp")
    client_cpp = read("src/platform/EverythingIpcClient.cpp")
    provider_cpp = read("src/core/EverythingProvider.cpp")
    everything_source = protocol_h + client_cpp + provider_cpp
    for token in (
        "kCopyDataQuery2W = 18",
        "discoverNamedInstances",
        "EnumNamedEverythingWindows",
        "ambiguousNamedInstances",
        "inFlight_->sourceWindow",
        "client_.Status()",
        "1000",
    ):
        if token not in everything_source:
            fail(f"Stable lost frozen Everything transport behavior: {token}")

    for forbidden in (
        "Everything64.dll",
        "Everything32.dll",
        "Everything3_",
        r"\\.\PIPE\Everything IPC",
        "LoadLibraryW",
        "LoadLibraryA",
    ):
        if forbidden in everything_source:
            fail(
                "Stable may not add an Everything DLL/SDK3 named-pipe "
                f"dependency: {forbidden}"
            )

    validation = read("docs/V0.5_RC_VALIDATION.md")
    for token in (
        "General Launcher/Search behavior rows show title and description",
        "Shortcut editor labels remain single-line",
        "Enabled/Admin/Pinned controls stay inside",
        "Test/Delete/Discard/Save buttons remain separated",
        "Search Sources diagnostics do not overlap",
        "cards align with the page-header content gutter",
    ):
        if token not in validation:
            fail(f"Stable manual UI validation coverage missing: {token}")

    if "[x]" in validation.lower():
        fail(
            "Stable real-desktop validation items must not be "
            "pre-marked as completed by automation"
        )

    package_script = read("scripts/verify_package.ps1")
    for token in (
        '"V0.5_RC_VALIDATION.md"',
        '"EVERYTHING_COMPATIBILITY.md"',
        "$allowedTopLevel",
        "EXE fixed FileVersion",
    ):
        if token not in package_script:
            fail(f"Stable package contract missing: {token}")

    for workflow_path in (
        ".github/workflows/build.yml",
        ".github/workflows/release.yml",
    ):
        workflow = read(workflow_path)
        for token in (
            "everything_ipc_runtime_tests",
            "V0.5_RC_VALIDATION.md",
            "EVERYTHING_COMPATIBILITY.md",
            "Verify package contract",
        ):
            if token not in workflow:
                fail(f"{workflow_path} missing Stable gate/package item: {token}")

    expected_providers = {
        "windows.startmenu": True,
        "windows.packaged": True,
        "windows.apppaths": True,
        "windows.path": True,
        "everything.filesystem": False,
    }
    if settings.get("providers") != expected_providers:
        fail("stable v0.5.0 provider defaults changed from RC3")

    runtime_test = read("tests/EverythingIpcRuntimeTests.cpp")
    for token in (
        "generation = 351",
        "generation = 352",
        'L"_(1.5b)"',
        "status.ambiguousNamedInstances",
        'L"drive-root"',
        'L"unc"',
        'L"longpath"',
        "i < 128",
        "500000",
        "Mode::WrongSender",
    ):
        if token not in runtime_test:
            fail(f"stable v0.5.0 lost Everything regression coverage: {token}")

    config_tests = read("tests/ConfigCoreTests.cpp")
    for token in (
        "settings-v0.5-alpha-everything.json",
        "settings-v0.5-alpha-default.json",
        "v041DowngradeRead",
        "MigratedFromSchemaVersion",
        "mask < 32",
    ):
        if token not in config_tests:
            fail(f"stable v0.5.0 lost migration/downgrade coverage: {token}")

    package_script = read("scripts/verify_package.ps1")
    if "default { 300 }" not in package_script:
        fail("stable v0.5.0 package revision must remain 300")

    build_workflow = read(".github/workflows/build.yml")
    for token in (
        'if [[ "$VERSION" == *-* ]]; then',
        'EXTRA_ARGS+=(--prerelease)',
        'TAG="v$VERSION"',
        "sha256sum -c SHA256SUMS.txt",
    ):
        if token not in build_workflow:
            fail(f"stable main publication guard missing: {token}")

    release_workflow = read(".github/workflows/release.yml")
    for token in (
        "prerelease: ${{ contains(github.ref_name, '-') }}",
        "sha256sum -c SHA256SUMS.txt",
        "everything_ipc_runtime_tests",
    ):
        if token not in release_workflow:
            fail(f"stable tag-release guard missing: {token}")

    readme = read("README.md")
    for token in (
        "## v0.5.0 — Stable",
        "### Stable v0.5.0",
        "/releases/tag/v0.5.0",
        "0.5.0.300",
    ):
        if token not in readme:
            fail(f"stable README metadata missing: {token}")

    changelog = read("CHANGELOG.md")
    if "## 0.5.0" not in changelog:
        fail("stable v0.5.0 changelog entry missing")

    roadmap = read("ROADMAP.md")
    for token in (
        "Completed in v0.5.0:",
        "v0.5.0 Stable promotes the frozen RC3 line",
        "0.5.0.300",
    ):
        if token not in roadmap:
            fail(f"stable roadmap state missing: {token}")
    print(
        "v0.5.0 Stable promotion contract verified:",
        "| schemas/providers/Classic/Everything frozen",
        "| 54px owner-draw rows + single-line editor labels",
        "| 100/125/150/200% DPI layout regression coverage",
        "| Windows revision 300 | non-prerelease publication path",
    )
    raise SystemExit(0)


if version == "0.5.0-rc.3":
    expected_schemas = {
        "kSettingsSchemaVersion": 3,
        "kCommandsSchemaVersion": 1,
        "kUsageSchemaVersion": 1,
    }
    for name, expected in expected_schemas.items():
        actual = cpp_int("src/core/ConfigIO.hpp", name)
        if actual != expected:
            fail(
                f"{name}={actual}, expected frozen RC3 value {expected}"
            )

    if cpp_int(
        "src/core/ProviderCache.cpp",
        "kProviderCacheSchemaVersion",
    ) != 2:
        fail("provider-cache schema must remain 2 throughout v0.5 RC")

    provider_text = read("src/core/ProviderIds.hpp")
    for token in (
        '"windows.startmenu"',
        '"windows.packaged"',
        '"windows.apppaths"',
        '"windows.path"',
        '"everything.filesystem"',
        "{std::string(kStartMenu), true}",
        "{std::string(kPackaged), true}",
        "{std::string(kAppPaths), true}",
        "{std::string(kPath), true}",
        "{std::string(kEverythingFilesystem), false}",
    ):
        if token not in provider_text:
            fail(f"RC3 provider/default freeze missing: {token}")

    settings = json.loads(read("config/settings.example.json"))
    if settings.get("schemaVersion") != 3:
        fail("RC3 settings.example.json must remain schemaVersion 3")

    launcher_hpp = read("src/ui/LauncherWindow.hpp")
    for name, expected in {
        "widthLogical_": 420,
        "rowHeightLogical_": 16,
        "maxResults_": 10,
    }.items():
        found = re.search(
            rf"\b{re.escape(name)}\s*\{{(\d+)\}}",
            launcher_hpp,
        )
        if not found or int(found.group(1)) != expected:
            fail(f"Classic geometry changed during RC3: {name}")

    layout_h = read("src/core/SettingsLayout.hpp")
    layout_cpp = read("src/core/SettingsLayout.cpp")
    for token in (
        "kContentLeftInsetLogical = 38",
        "kContentRightInsetLogical = 34",
        "kToggleRowLogical = 54",
    ):
        if token not in layout_h:
            fail(f"RC3 polished Settings metric missing: {token}")
    for token in (
        "kContentLeftInsetLogical",
        "kContentRightInsetLogical",
        "kToggleRowLogical",
    ):
        if token not in layout_cpp:
            fail(f"RC3 Settings layout helper is not using: {token}")

    settings_ui = read("src/ui/SettingsWindow.cpp")
    for token in (
        "SS_LEFTNOWORDWRAP | SS_NOPREFIX",
        "labelWidth = Scale(132)",
        "fieldWidth * 24 / 100",
        "fieldWidth * 22 / 100",
        "const int actionWidth",
        "settings_layout::",
        "kToggleRowLogical",
        "Scale(108)",
        "providerCard.bottom +",
        "Scale(140)",
        "Scale(188)",
        "rect.top + Scale(27)",
        "rect.top + Scale(29)",
        "Scale(960)",
        "BS_OWNERDRAW",
        "DrawNavigationButton",
        "RGB(232, 241, 250)",
    ):
        if token not in settings_ui:
            fail(f"RC3 Settings polish integration missing: {token}")

    if "const int behaviorRowHeight =\n            Scale(46)" in settings_ui:
        fail("RC3 regressed General owner-draw rows back to 46px")
    if 'page_ == page ? L"●  "' in settings_ui:
        fail("RC3 regressed to text-bullet navigation selection")

    desktop_test = read("tests/DesktopValidationTests.cpp")
    for token in (
        "kToggleRowLogical >= 54",
        "kContentLeftInsetLogical == 38",
        "kContentRightInsetLogical == 34",
        "wide.behavior.bottom -",
        "wide.search.bottom -",
        "96, 120, 144, 192",
    ):
        if token not in desktop_test:
            fail(f"RC3 DPI/layout regression coverage missing: {token}")

    registry = read("src/core/ProviderRegistry.cpp")
    if "everything.filesystem" in registry or "kEverythingFilesystem" in registry:
        fail(
            "Everything must remain a Dynamic Query Provider outside "
            "the static ProviderRegistry during RC3"
        )

    protocol_h = read("src/core/EverythingIpcProtocol.hpp")
    client_cpp = read("src/platform/EverythingIpcClient.cpp")
    provider_cpp = read("src/core/EverythingProvider.cpp")
    everything_source = protocol_h + client_cpp + provider_cpp
    for token in (
        "kCopyDataQuery2W = 18",
        "discoverNamedInstances",
        "EnumNamedEverythingWindows",
        "ambiguousNamedInstances",
        "inFlight_->sourceWindow",
        "client_.Status()",
        "1000",
    ):
        if token not in everything_source:
            fail(f"RC3 lost frozen Everything transport behavior: {token}")

    for forbidden in (
        "Everything64.dll",
        "Everything32.dll",
        "Everything3_",
        r"\\.\PIPE\Everything IPC",
        "LoadLibraryW",
        "LoadLibraryA",
    ):
        if forbidden in everything_source:
            fail(
                "RC3 may not add an Everything DLL/SDK3 named-pipe "
                f"dependency: {forbidden}"
            )

    validation = read("docs/V0.5_RC_VALIDATION.md")
    for token in (
        "General Launcher/Search behavior rows show title and description",
        "Shortcut editor labels remain single-line",
        "Enabled/Admin/Pinned controls stay inside",
        "Test/Delete/Discard/Save buttons remain separated",
        "Search Sources diagnostics do not overlap",
        "cards align with the page-header content gutter",
    ):
        if token not in validation:
            fail(f"RC3 manual UI validation coverage missing: {token}")

    if "[x]" in validation.lower():
        fail(
            "RC3 real-desktop validation items must not be "
            "pre-marked as completed by automation"
        )

    package_script = read("scripts/verify_package.ps1")
    for token in (
        '"V0.5_RC_VALIDATION.md"',
        '"EVERYTHING_COMPATIBILITY.md"',
        "$allowedTopLevel",
        "EXE fixed FileVersion",
    ):
        if token not in package_script:
            fail(f"RC3 package contract missing: {token}")

    for workflow_path in (
        ".github/workflows/build.yml",
        ".github/workflows/release.yml",
    ):
        workflow = read(workflow_path)
        for token in (
            "everything_ipc_runtime_tests",
            "V0.5_RC_VALIDATION.md",
            "EVERYTHING_COMPATIBILITY.md",
            "Verify package contract",
        ):
            if token not in workflow:
                fail(f"{workflow_path} missing RC3 gate/package item: {token}")

    roadmap = read("ROADMAP.md")
    for token in (
        "v0.5.0-rc.3 is the final Settings UI polish candidate",
        "v0.5.0 Stable is promoted from the validated RC line",
    ):
        if token not in roadmap:
            fail(f"RC3 stable-candidate policy missing: {token}")

    print(
        "v0.5.0-rc.3 final Settings UI/stable-candidate contract verified:",
        "| schemas/providers/Classic/Everything frozen",
        "| 54px owner-draw rows + single-line editor labels",
        "| 100/125/150/200% DPI layout regression coverage",
    )
    raise SystemExit(0)


if version == "0.5.0-rc.2":
    expected_schemas = {
        "kSettingsSchemaVersion": 3,
        "kCommandsSchemaVersion": 1,
        "kUsageSchemaVersion": 1,
    }
    for name, expected in expected_schemas.items():
        actual = cpp_int("src/core/ConfigIO.hpp", name)
        if actual != expected:
            fail(
                f"{name}={actual}, expected frozen RC2 value {expected}"
            )

    if cpp_int(
        "src/core/ProviderCache.cpp",
        "kProviderCacheSchemaVersion",
    ) != 2:
        fail("provider-cache schema must remain 2 throughout v0.5 RC")

    provider_text = read("src/core/ProviderIds.hpp")
    for token in (
        '"windows.startmenu"',
        '"windows.packaged"',
        '"windows.apppaths"',
        '"windows.path"',
        '"everything.filesystem"',
        "{std::string(kStartMenu), true}",
        "{std::string(kPackaged), true}",
        "{std::string(kAppPaths), true}",
        "{std::string(kPath), true}",
        "{std::string(kEverythingFilesystem), false}",
    ):
        if token not in provider_text:
            fail(f"RC2 provider/default freeze missing: {token}")

    settings = json.loads(read("config/settings.example.json"))
    expected_providers = {
        "windows.startmenu": True,
        "windows.packaged": True,
        "windows.apppaths": True,
        "windows.path": True,
        "everything.filesystem": False,
    }
    if settings.get("schemaVersion") != 3:
        fail("RC2 settings.example.json must remain schemaVersion 3")
    if settings.get("providers") != expected_providers:
        fail("RC2 provider defaults changed after feature freeze")

    launcher_hpp = read("src/ui/LauncherWindow.hpp")
    for name, expected in {
        "widthLogical_": 420,
        "rowHeightLogical_": 16,
        "maxResults_": 10,
    }.items():
        found = re.search(
            rf"\b{re.escape(name)}\s*\{{(\d+)\}}",
            launcher_hpp,
        )
        if not found or int(found.group(1)) != expected:
            fail(f"Classic geometry changed during RC2: {name}")

    registry = read("src/core/ProviderRegistry.cpp")
    if "everything.filesystem" in registry or "kEverythingFilesystem" in registry:
        fail(
            "Everything must remain a Dynamic Query Provider outside "
            "the static ProviderRegistry during RC2"
        )

    protocol_h = read("src/core/EverythingIpcProtocol.hpp")
    client_cpp = read("src/platform/EverythingIpcClient.cpp")
    provider_cpp = read("src/core/EverythingProvider.cpp")
    everything_source = protocol_h + client_cpp + provider_cpp

    for token in (
        "kCopyDataQuery2W = 18",
        "discoverNamedInstances",
        "EnumNamedEverythingWindows",
        "ambiguousNamedInstances",
        "inFlight_->sourceWindow",
        "ERROR_INSUFFICIENT_BUFFER",
        "client_.Status()",
        "1000",
    ):
        if token not in everything_source:
            fail(f"RC2 lost frozen Everything transport behavior: {token}")

    for forbidden in (
        "Everything64.dll",
        "Everything32.dll",
        "Everything3_",
        r"\\.\PIPE\Everything IPC",
        "LoadLibraryW",
        "LoadLibraryA",
    ):
        if forbidden in everything_source:
            fail(
                "RC2 may not add an Everything DLL/SDK3 named-pipe "
                f"dependency: {forbidden}"
            )

    settings_h = read("src/ui/SettingsWindow.hpp")
    settings_ui = read("src/ui/SettingsWindow.cpp")

    for token in (
        "kIdProviderGetEverything",
        "kIdProviderRecheckEverything",
        "providerGetEverything_",
        "providerRecheckEverything_",
        "OpenEverythingDownloadPage",
    ):
        if token not in settings_h:
            fail(f"RC2 Everything onboarding declaration missing: {token}")

    for token in (
        "Everything not detected",
        "未检测到 Everything",
        "ALTRun Next does not bundle or auto-start Everything",
        "ALTRun Next 不内置或自动启动 Everything",
        "Get Everything",
        "获取 Everything",
        "Recheck",
        "重新检测",
        "https://www.voidtools.com/downloads/",
        "https://www.voidtools.com/zh-cn/downloads/",
        "ShellExecuteW",
        "endpointMissing",
        "showGetEverything =",
        "showRecheck =",
        "Application-search fallback active",
    ):
        if token not in settings_ui:
            fail(f"RC2 Everything onboarding UX missing: {token}")

    if "URLDownloadToFile" in settings_ui or "WinHttp" in settings_ui:
        fail(
            "RC2 onboarding must not become an automatic Everything "
            "download/install manager"
        )

    validation = read("docs/V0.5_RC_VALIDATION.md")
    for token in (
        "Everything not detected",
        "Get Everything opens the official voidtools download page",
        "Recheck immediately refreshes the status",
        "system error 2",
    ):
        if token not in validation:
            fail(f"RC2 real-desktop onboarding validation missing: {token}")

    if "[x]" in validation.lower():
        fail(
            "RC2 real-desktop validation items must not be "
            "pre-marked as completed by automation"
        )

    runtime_test = read("tests/EverythingIpcRuntimeTests.cpp")
    for token in (
        "generation = 351",
        "generation = 352",
        'L"_(1.5b)"',
        "status.ambiguousNamedInstances",
        'L"drive-root"',
        'L"unc"',
        'L"longpath"',
        "i < 128",
        "500000",
        "Mode::WrongSender",
    ):
        if token not in runtime_test:
            fail(f"RC2 lost beta.2 IPC regression coverage: {token}")

    config_tests = read("tests/ConfigCoreTests.cpp")
    for token in (
        "settings-v0.5-alpha-everything.json",
        "settings-v0.5-alpha-default.json",
        "v041DowngradeRead",
        "MigratedFromSchemaVersion",
        "mask < 32",
    ):
        if token not in config_tests:
            fail(f"RC2 lost migration/downgrade regression coverage: {token}")

    package_script = read("scripts/verify_package.ps1")
    for token in (
        '"V0.5_RC_VALIDATION.md"',
        '"EVERYTHING_COMPATIBILITY.md"',
        "$allowedTopLevel",
        "EXE fixed FileVersion",
    ):
        if token not in package_script:
            fail(f"RC2 package contract missing: {token}")

    for workflow_path in (
        ".github/workflows/build.yml",
        ".github/workflows/release.yml",
    ):
        workflow = read(workflow_path)
        for token in (
            "everything_ipc_runtime_tests",
            "V0.5_RC_VALIDATION.md",
            "EVERYTHING_COMPATIBILITY.md",
            "Verify package contract",
        ):
            if token not in workflow:
                fail(f"{workflow_path} missing RC2 gate/package item: {token}")

    roadmap = read("ROADMAP.md")
    if "v0.5.0-rc.2 addresses real-desktop Everything onboarding" not in roadmap:
        fail("RC2 onboarding fix is not recorded in ROADMAP.md")

    print(
        "v0.5.0-rc.2 Everything onboarding/freeze verified:",
        "| settings=3 commands=1 usage=1 provider-cache=2",
        "| actionable missing-IPC guidance + official download/recheck",
        "| no dependency manager | frozen Query2/Classic contracts",
    )
    raise SystemExit(0)


if version == "0.5.0-rc.1":
    expected_schemas = {
        "kSettingsSchemaVersion": 3,
        "kCommandsSchemaVersion": 1,
        "kUsageSchemaVersion": 1,
    }
    for name, expected in expected_schemas.items():
        actual = cpp_int("src/core/ConfigIO.hpp", name)
        if actual != expected:
            fail(
                f"{name}={actual}, expected frozen RC1 value {expected}"
            )

    if cpp_int(
        "src/core/ProviderCache.cpp",
        "kProviderCacheSchemaVersion",
    ) != 2:
        fail("provider-cache schema must remain 2 throughout v0.5 RC")

    provider_text = read("src/core/ProviderIds.hpp")
    expected_provider_tokens = (
        '"windows.startmenu"',
        '"windows.packaged"',
        '"windows.apppaths"',
        '"windows.path"',
        '"everything.filesystem"',
        "{std::string(kStartMenu), true}",
        "{std::string(kPackaged), true}",
        "{std::string(kAppPaths), true}",
        "{std::string(kPath), true}",
        "{std::string(kEverythingFilesystem), false}",
    )
    for token in expected_provider_tokens:
        if token not in provider_text:
            fail(f"RC1 provider/default freeze missing: {token}")

    settings = json.loads(read("config/settings.example.json"))
    expected_providers = {
        "windows.startmenu": True,
        "windows.packaged": True,
        "windows.apppaths": True,
        "windows.path": True,
        "everything.filesystem": False,
    }
    if settings.get("schemaVersion") != 3:
        fail("RC1 settings.example.json must remain schemaVersion 3")
    if settings.get("providers") != expected_providers:
        fail("RC1 provider defaults changed after feature freeze")

    launcher_hpp = read("src/ui/LauncherWindow.hpp")
    for name, expected in {
        "widthLogical_": 420,
        "rowHeightLogical_": 16,
        "maxResults_": 10,
    }.items():
        found = re.search(
            rf"\b{re.escape(name)}\s*\{{(\d+)\}}",
            launcher_hpp,
        )
        if not found or int(found.group(1)) != expected:
            fail(f"Classic geometry changed during RC freeze: {name}")

    registry = read("src/core/ProviderRegistry.cpp")
    if "everything.filesystem" in registry or "kEverythingFilesystem" in registry:
        fail(
            "Everything must remain a Dynamic Query Provider outside "
            "the static ProviderRegistry during RC"
        )

    protocol_h = read("src/core/EverythingIpcProtocol.hpp")
    protocol_cpp = read("src/core/EverythingIpcProtocol.cpp")
    client_h = read("src/platform/EverythingIpcClient.hpp")
    client_cpp = read("src/platform/EverythingIpcClient.cpp")
    provider_cpp = read("src/core/EverythingProvider.cpp")
    everything_source = (
        protocol_h + protocol_cpp + client_h + client_cpp + provider_cpp
    )

    for token in (
        "kCopyDataQuery2W = 18",
        "kItemDriveOrRoot",
        "discoverNamedInstances",
        "maxReplyBytes",
        "EnumNamedEverythingWindows",
        "ambiguousNamedInstances",
        "inFlight_->sourceWindow",
        "ERROR_INSUFFICIENT_BUFFER",
        "client_.Status()",
        "1000",
    ):
        if token not in everything_source:
            fail(f"RC1 lost frozen beta.2 Everything behavior: {token}")

    for forbidden in (
        "Everything64.dll",
        "Everything32.dll",
        "Everything3_",
        r"\\.\PIPE\Everything IPC",
        "LoadLibraryW",
        "LoadLibraryA",
    ):
        if forbidden in everything_source:
            fail(
                "RC1 may not add an Everything DLL/SDK3 named-pipe "
                f"dependency after feature freeze: {forbidden}"
            )

    runtime_test = read("tests/EverythingIpcRuntimeTests.cpp")
    for token in (
        'L"_(1.5b)"',
        "status.ambiguousNamedInstances",
        'L"drive-root"',
        'L"unc"',
        'L"longpath"',
        "i < 128",
        "500000",
        "Mode::WrongSender",
        "options.maxReplyBytes = 64",
        ".limit = 5000",
        "generation = 351",
        "generation = 352",
    ):
        if token not in runtime_test:
            fail(f"RC1 lost Everything runtime regression coverage: {token}")

    config_tests = read("tests/ConfigCoreTests.cpp")
    for token in (
        "settings-v0.5-alpha-everything.json",
        "settings-v0.5-alpha-default.json",
        "v041DowngradeRead",
        "MigratedFromSchemaVersion",
        "mask < 32",
    ):
        if token not in config_tests:
            fail(f"RC1 lost migration/downgrade regression coverage: {token}")

    rc_validation = read("docs/V0.5_RC_VALIDATION.md")
    for token in (
        "Frozen RC contract",
        "Required environment matrix",
        "Upgrade: v0.4.1 stable -> v0.5.0 RC",
        "Downgrade protection",
        "Everything 1.4 default instance",
        "Everything 1.5 current beta",
        "Named and multiple Everything instances",
        "Mixed-DPI and UI validation",
        "Long-run soak",
        "Stable promotion gate",
        "PASS / FAIL",
    ):
        if token not in rc_validation:
            fail(f"RC1 validation checklist missing: {token}")

    if "[x]" in rc_validation.lower():
        fail(
            "RC1 real-desktop validation items must not be "
            "pre-marked as completed by automation"
        )

    compatibility = read("docs/EVERYTHING_COMPATIBILITY.md")
    for token in (
        "EVERYTHING_TASKBAR_NOTIFICATION_(instance-name)",
        "1.4 unnamed/default instance",
        "1.5a default alpha instance",
        "1.5b+ unnamed/default instance",
        "multiple named instances",
        "RC1 freeze",
    ):
        if token not in compatibility:
            fail(f"RC1 compatibility matrix missing: {token}")

    package_script = read("scripts/verify_package.ps1")
    for token in (
        '"V0.5_RC_VALIDATION.md"',
        '"EVERYTHING_COMPATIBILITY.md"',
        "$allowedTopLevel",
        "EXE fixed FileVersion",
        "Top-level package entries: exact allowlist verified",
    ):
        if token not in package_script:
            fail(f"RC1 package contract missing: {token}")

    for workflow_path in (
        ".github/workflows/build.yml",
        ".github/workflows/release.yml",
    ):
        workflow = read(workflow_path)
        for token in (
            "everything_ipc_runtime_tests",
            "V0.5_RC_VALIDATION.md",
            "EVERYTHING_COMPATIBILITY.md",
            "Verify package contract",
        ):
            if token not in workflow:
                fail(f"{workflow_path} missing RC1 gate/package item: {token}")

    build_workflow = read(".github/workflows/build.yml")
    for token in (
        "sha256sum -c SHA256SUMS.txt",
        "Verify packaged x64 runtime startup",
        "Check this is still the latest main commit",
    ):
        if token not in build_workflow:
            fail(f"main publication hardening missing during RC1: {token}")

    release_workflow = read(".github/workflows/release.yml")
    for token in (
        "Release tag preflight",
        "verify_tag_version.py",
        "sha256sum -c SHA256SUMS.txt",
        "Verify packaged x64 runtime startup",
    ):
        if token not in release_workflow:
            fail(f"tagged release hardening missing during RC1: {token}")

    roadmap = read("ROADMAP.md")
    for token in (
        "v0.5.0-rc.1 freezes the v0.5 surface",
        "regression/compatibility/data-safety/publication fixes",
        "no release-blocking defect",
    ):
        if token not in roadmap:
            fail(f"RC1 feature-freeze policy missing from roadmap: {token}")

    print(
        "v0.5.0-rc.1 release-candidate freeze verified:",
        "| settings=3 commands=1 usage=1 provider-cache=2",
        "| provider/default/Classic/Everything transport frozen",
        "| packaged RC validation + compatibility matrix",
    )
    raise SystemExit(0)


if version == "0.5.0-beta.2":
    expected_schemas = {
        "kSettingsSchemaVersion": 3,
        "kCommandsSchemaVersion": 1,
        "kUsageSchemaVersion": 1,
    }
    for name, expected in expected_schemas.items():
        actual = cpp_int("src/core/ConfigIO.hpp", name)
        if actual != expected:
            fail(
                f"{name}={actual}, expected beta.2 value {expected}"
            )

    if cpp_int(
        "src/core/ProviderCache.cpp",
        "kProviderCacheSchemaVersion",
    ) != 2:
        fail("provider-cache schema must remain 2 in beta.2")

    provider_text = read("src/core/ProviderIds.hpp")
    for token in (
        "{std::string(kStartMenu), true}",
        "{std::string(kPackaged), true}",
        "{std::string(kAppPaths), true}",
        "{std::string(kPath), true}",
        "{std::string(kEverythingFilesystem), false}",
    ):
        if token not in provider_text:
            fail(f"beta.2 provider/default contract missing: {token}")

    settings = json.loads(read("config/settings.example.json"))
    expected_providers = {
        "windows.startmenu": True,
        "windows.packaged": True,
        "windows.apppaths": True,
        "windows.path": True,
        "everything.filesystem": False,
    }
    if settings.get("schemaVersion") != 3:
        fail("settings.example.json must remain schemaVersion 3")
    if settings.get("providers") != expected_providers:
        fail("beta.2 provider defaults changed unexpectedly")

    protocol_h = read("src/core/EverythingIpcProtocol.hpp")
    protocol_cpp = read("src/core/EverythingIpcProtocol.cpp")
    for token in (
        "kCopyDataQuery2W = 18",
        "kItemDriveOrRoot",
        "bool root{false}",
    ):
        if token not in protocol_h:
            fail(f"Everything protocol hardening missing: {token}")
    for token in (
        "returned item count exceeds total item count",
        "offset/item count exceeds total item count",
        "item.root",
    ):
        if token not in protocol_cpp:
            fail(f"LIST2 range/root validation missing: {token}")

    client_h = read("src/platform/EverythingIpcClient.hpp")
    client_cpp = read("src/platform/EverythingIpcClient.cpp")
    for token in (
        "discoverNamedInstances",
        "maxReplyBytes",
        "sourceWindow",
        "maxResults",
        "FindEndpoint",
    ):
        if token not in client_h:
            fail(f"Everything client beta.2 declaration missing: {token}")
    for token in (
        "EnumWindows",
        "EnumNamedEverythingWindows",
        'L"_("',
        "ambiguousNamedInstances",
        "ERROR_MORE_DATA",
        "ERROR_INSUFFICIENT_BUFFER",
        "inFlight_->sourceWindow",
        "options_.maxReplyBytes",
        "parsed.value->items.size()",
        "item.root",
    ):
        if token not in client_cpp:
            fail(f"Everything client hardening missing: {token}")

    query_types = read("src/core/EverythingQuery.hpp")
    for token in (
        "bool root{false}",
        "ipcWindowClass",
        "namedInstanceFallback",
        "ambiguousNamedInstances",
        "matchingWindowCount",
    ):
        if token not in query_types:
            fail(f"Everything diagnostics/path state missing: {token}")

    settings_ui = read("src/ui/SettingsWindow.cpp")
    for token in (
        "Multiple named Everything instances detected",
        "IPC endpoint: ",
        "unique named instance auto-selected",
        "matchingWindowCount",
    ):
        if token not in settings_ui:
            fail(f"Settings named-instance diagnostics missing: {token}")

    protocol_test = read("tests/EverythingIpcProtocolTests.cpp")
    for token in (
        "rootList.value->items[1].root",
        "tooManyItems",
        "badRange",
    ):
        if token not in protocol_test:
            fail(f"portable LIST2 hardening regression missing: {token}")

    runtime_test = read("tests/EverythingIpcRuntimeTests.cpp")
    for token in (
        'L"_(1.5b)"',
        "status.ambiguousNamedInstances",
        "status.matchingWindowCount ==",
        'L"drive-root"',
        'L"unc"',
        'L"longpath"',
        "i < 128",
        "500000",
        "Mode::WrongSender",
        "options.maxReplyBytes = 64",
        "ERROR_INSUFFICIENT_BUFFER",
        "LastMaxResults",
        ".limit = 5000",
    ):
        if token not in runtime_test:
            fail(f"Everything beta.2 runtime stress missing: {token}")

    everything_provider = read("src/core/EverythingProvider.cpp")
    for token in (
        "std::min<std::size_t>",
        "request.limit",
        "1000",
        "client_.Status()",
    ):
        if token not in everything_provider:
            fail(f"Everything provider performance boundary missing: {token}")

    registry = read("src/core/ProviderRegistry.cpp")
    if "everything.filesystem" in registry or "kEverythingFilesystem" in registry:
        fail(
            "everything.filesystem must remain dynamic and outside "
            "the static ProviderRegistry"
        )

    combined_source = (
        protocol_h
        + protocol_cpp
        + client_h
        + client_cpp
        + everything_provider
    )
    for forbidden in (
        "Everything64.dll",
        "Everything32.dll",
        "Everything3_",
        r"\\.\PIPE\Everything IPC",
        "LoadLibraryW",
        "LoadLibraryA",
    ):
        if forbidden in combined_source:
            fail(
                "beta.2 must keep the 1.4-compatible native Query2 "
                f"baseline without a new DLL/named-pipe dependency: {forbidden}"
            )

    launcher_hpp = read("src/ui/LauncherWindow.hpp")
    for name, expected in {
        "widthLogical_": 420,
        "rowHeightLogical_": 16,
        "maxResults_": 10,
    }.items():
        found = re.search(
            rf"\b{re.escape(name)}\s*\{{(\d+)\}}",
            launcher_hpp,
        )
        if not found or int(found.group(1)) != expected:
            fail(f"Classic geometry changed during beta.2: {name}")

    validation = read("docs/EVERYTHING_BETA_VALIDATION.md")
    for token in (
        "Everything 1.4 unnamed/default instance",
        "single named Everything instance",
        "two or more named Everything instances",
        "Everything 1.5b default unnamed instance",
        "UNC",
        "Extended-length",
        "Rapid typing",
    ):
        if token not in validation:
            fail(f"beta.2 manual validation coverage missing: {token}")
    if "[x]" in validation.lower():
        fail("manual beta.2 validation items must remain unchecked")

    compatibility = read("docs/EVERYTHING_COMPATIBILITY.md")
    for token in (
        "EVERYTHING_TASKBAR_NOTIFICATION_(instance-name)",
        "1.4 unnamed/default instance",
        "1.5a default alpha instance",
        "1.5b+ unnamed/default instance",
        "multiple named instances",
        "128 rapid replacement queries",
    ):
        if token not in compatibility:
            fail(f"Everything compatibility matrix missing: {token}")

    for workflow_path in (
        ".github/workflows/build.yml",
        ".github/workflows/release.yml",
    ):
        workflow = read(workflow_path)
        if "everything_ipc_runtime_tests" not in workflow:
            fail(
                f"{workflow_path} no longer runs Everything IPC runtime smoke"
            )

    print(
        "v0.5.0-beta.2 compatibility/performance contract verified:",
        "| settings=3 commands=1 usage=1 provider-cache=2",
        "| unnamed-first + unique named fallback",
        "| long/UNC/root + burst/large-result IPC hardening",
    )
    raise SystemExit(0)


if version == "0.5.0-beta.1":
    expected_schemas = {
        "kSettingsSchemaVersion": 3,
        "kCommandsSchemaVersion": 1,
        "kUsageSchemaVersion": 1,
    }
    for name, expected in expected_schemas.items():
        actual = cpp_int("src/core/ConfigIO.hpp", name)
        if actual != expected:
            fail(
                f"{name}={actual}, expected beta.1 value {expected}"
            )

    provider_cache_schema = cpp_int(
        "src/core/ProviderCache.cpp",
        "kProviderCacheSchemaVersion",
    )
    if provider_cache_schema != 2:
        fail("provider-cache schema must remain 2 in beta.1")

    provider_text = read("src/core/ProviderIds.hpp")
    for provider_id in (
        "windows.startmenu",
        "windows.packaged",
        "windows.apppaths",
        "windows.path",
        "everything.filesystem",
    ):
        if provider_id not in provider_text:
            fail(f"provider ID missing in beta.1: {provider_id}")

    for token in (
        "{std::string(kStartMenu), true}",
        "{std::string(kPackaged), true}",
        "{std::string(kAppPaths), true}",
        "{std::string(kPath), true}",
        "{std::string(kEverythingFilesystem), false}",
    ):
        if token not in provider_text:
            fail(f"beta.1 provider default contract missing: {token}")

    settings = json.loads(read("config/settings.example.json"))
    if settings.get("schemaVersion") != 3:
        fail("settings.example.json must use schemaVersion 3 in beta.1")

    expected_providers = {
        "windows.startmenu": True,
        "windows.packaged": True,
        "windows.apppaths": True,
        "windows.path": True,
        "everything.filesystem": False,
    }
    if settings.get("providers") != expected_providers:
        fail(
            "beta.1 example provider defaults no longer match the "
            "formal schema-3 contract"
        )

    settings_hpp = read("src/core/Settings.hpp")
    for token in (
        "WasMigratedFromOlderSchema",
        "MigratedFromSchemaVersion",
        "migratedFromOlderSchema_",
        "migratedFromSchemaVersion_",
    ):
        if token not in settings_hpp:
            fail(f"settings migration diagnostics missing: {token}")

    settings_cpp = read("src/core/Settings.cpp")
    for token in (
        "previousSchema",
        "migratedFromOlderSchema_",
        "migratedFromSchemaVersion_",
        "if (Save())",
    ):
        if token not in settings_cpp:
            fail(f"schema migration implementation missing: {token}")

    config_tests = read("tests/ConfigCoreTests.cpp")
    for token in (
        "settings-v0.5-alpha-everything.json",
        "settings-v0.5-alpha-default.json",
        "v041DowngradeRead",
        "MigratedFromSchemaVersion",
        "mask < 32",
        "everything.filesystem",
    ):
        if token not in config_tests:
            fail(f"beta.1 migration/downgrade regression missing: {token}")

    settings_h = read("src/ui/SettingsWindow.hpp")
    settings_ui = read("src/ui/SettingsWindow.cpp")
    for token in (
        "kIdProviderEverything",
        "providerEverything_",
        "kProviderStatusTimerId",
        "OnDynamicProviderStatusChanged",
    ):
        if token not in settings_h:
            fail(f"Everything Settings declaration missing: {token}")

    for token in (
        "Everything files & folders",
        "Application-search fallback active",
        "lastTotalMatches",
        "lastNativeError",
        "kProviderStatusTimerId",
        "kIdProviderEverything",
        "EverythingQueryStatus::ReplyTimeout",
    ):
        if token not in settings_ui:
            fail(f"Everything Settings diagnostics missing: {token}")

    app_hpp = read("src/app/App.hpp")
    app_cpp = read("src/app/App.cpp")
    if "EverythingStatus" not in app_hpp or "EverythingStatus" not in app_cpp:
        fail("App no longer exposes Everything runtime diagnostics")
    if "OnDynamicProviderStatusChanged" not in app_cpp:
        fail("dynamic-query completion no longer refreshes Settings diagnostics")

    everything_provider_h = read("src/core/EverythingProvider.hpp")
    everything_provider_cpp = read("src/core/EverythingProvider.cpp")
    if "Status() const" not in everything_provider_h:
        fail("EverythingProvider status API is missing")
    for token in (
        "client_.Status()",
        "client_.IsAvailable()",
        "EverythingAvailability",
    ):
        if token not in everything_provider_cpp:
            fail(f"EverythingProvider live availability probe missing: {token}")

    query_types = read("src/core/EverythingQuery.hpp")
    for token in (
        "hasQuery",
        "lastTotalMatches",
        "lastNativeError",
    ):
        if token not in query_types:
            fail(f"Everything diagnostics snapshot missing: {token}")

    runtime_test = read("tests/EverythingIpcRuntimeTests.cpp")
    for token in (
        "generation = 351",
        "generation = 352",
        "recovered.txt",
        "status.hasQuery",
        "lastNativeError",
        "lastTotalMatches",
    ):
        if token not in runtime_test:
            fail(f"Everything fallback/recovery runtime test missing: {token}")

    registry = read("src/core/ProviderRegistry.cpp")
    if "everything.filesystem" in registry or "kEverythingFilesystem" in registry:
        fail(
            "everything.filesystem must remain a Dynamic Query Provider, "
            "not a static ProviderRegistry discovery source"
        )

    combined_everything_source = (
        read("src/core/EverythingIpcProtocol.hpp")
        + read("src/platform/EverythingIpcClient.cpp")
        + everything_provider_cpp
    )
    for forbidden in (
        "Everything64.dll",
        "Everything32.dll",
        "LoadLibraryW",
        "LoadLibraryA",
    ):
        if forbidden in combined_everything_source:
            fail(
                "beta.1 must keep native IPC with no Everything DLL "
                f"dependency: found {forbidden}"
            )

    launcher_hpp = read("src/ui/LauncherWindow.hpp")
    for name, expected in {
        "widthLogical_": 420,
        "rowHeightLogical_": 16,
        "maxResults_": 10,
    }.items():
        found = re.search(
            rf"\b{re.escape(name)}\s*\{{(\d+)\}}",
            launcher_hpp,
        )
        if not found or int(found.group(1)) != expected:
            fail(f"Classic geometry changed during beta.1: {name}")

    validation = ROOT / "docs" / "EVERYTHING_BETA_VALIDATION.md"
    if not validation.exists():
        fail("docs/EVERYTHING_BETA_VALIDATION.md is required for beta.1")

    validation_text = validation.read_text(encoding="utf-8")
    for token in (
        "Schema 2 -> schema 3 migration",
        "Downgrade protection",
        "Everything availability and fallback",
        "Diagnostics",
        "Packaging",
    ):
        if token not in validation_text:
            fail(f"Everything beta validation guide missing: {token}")

    if "[x]" in validation_text.lower():
        fail(
            "manual Everything beta validation items must not be "
            "pre-marked as completed by CI"
        )

    for workflow_path in (
        ".github/workflows/build.yml",
        ".github/workflows/release.yml",
    ):
        workflow = read(workflow_path)
        if "everything_ipc_runtime_tests" not in workflow:
            fail(
                f"{workflow_path} no longer runs Everything IPC runtime smoke"
            )

    print(
        "v0.5.0-beta.1 Everything Settings/migration contract verified:",
        "| settings=3 commands=1 usage=1 provider-cache=2",
        "| Everything default-off | live diagnostics/fallback",
        "| schema2 migration + schema2 downgrade protection",
    )
    raise SystemExit(0)


if version == "0.5.0-alpha.3":
    expected_schemas = {
        "kSettingsSchemaVersion": 2,
        "kCommandsSchemaVersion": 1,
        "kUsageSchemaVersion": 1,
    }
    for name, expected in expected_schemas.items():
        actual = cpp_int("src/core/ConfigIO.hpp", name)
        if actual != expected:
            fail(
                f"{name}={actual}, expected alpha.3 value {expected}"
            )

    provider_cache_schema = cpp_int(
        "src/core/ProviderCache.cpp",
        "kProviderCacheSchemaVersion",
    )
    if provider_cache_schema != 2:
        fail("provider-cache schema must remain 2 in alpha.3")

    provider_text = read("src/core/ProviderIds.hpp")
    for provider_id in (
        "windows.startmenu",
        "windows.packaged",
        "windows.apppaths",
        "windows.path",
        "everything.filesystem",
    ):
        if provider_id not in provider_text:
            fail(f"provider ID missing in alpha.3: {provider_id}")

    if "{std::string(kEverythingFilesystem)" in provider_text:
        fail(
            "everything.filesystem must remain absent from DefaultEnabled "
            "during alpha.3"
        )

    settings = json.loads(read("config/settings.example.json"))
    if settings.get("schemaVersion") != 2:
        fail("settings schema must remain 2 in alpha.3")
    if "everything.filesystem" in settings.get("providers", {}):
        fail(
            "Everything must remain default-off in alpha.3 example settings"
        )

    ranking = read("src/core/ResultRanking.cpp")
    for token in (
        "ScoreDynamicResultText",
        "UnifiedRankScore",
        "ResultKindWeight",
        "ProviderRankWeight",
        "user.commands",
        "kStartMenu",
    ):
        if token not in ranking:
            fail(f"alpha.3 ranking contract missing: {token}")

    merger = read("src/core/ResultMerger.cpp")
    for token in (
        "MergeLauncherResultsRanked",
        "UnifiedRankScore",
        "SameLauncherTarget",
        "DynamicDuplicatesStatic",
    ):
        if token not in merger:
            fail(f"alpha.3 merger contract missing: {token}")
    if "MergeLauncherResultsStaticFirst" in merger:
        fail("alpha.2 static-first merge policy survived into alpha.3")

    everything_provider = read("src/core/EverythingProvider.cpp")
    for token in (
        "ScoreDynamicResultText",
        "rankingQuery",
        "ResultKind::Folder",
        "ResultKind::File",
    ):
        if token not in everything_provider:
            fail(f"Everything alpha.3 ranking mapping missing: {token}")

    launcher = read("src/ui/LauncherWindow.cpp")
    for token in (
        "MergeLauncherResultsRanked",
        "candidateLimit",
        "maxResults_ * 3",
        "dynamicQueryPending_",
        "immediateExecutionPending_",
        "PrimaryResultText",
        "IsFileSystemResult",
        "ExecuteResult",
    ):
        if token not in launcher:
            fail(f"Launcher alpha.3 UX/ranking integration missing: {token}")

    classic = read("src/core/ClassicBehavior.cpp")
    if "!dynamicQueryPending" not in classic:
        fail(
            "single-result immediate execution no longer waits for "
            "dynamic query settlement"
        )

    desktop_test = read("tests/DesktopValidationTests.cpp")
    if "true, true, false, false, true, 1" not in desktop_test:
        fail(
            "desktop validation no longer covers pending-dynamic "
            "single-result suppression"
        )

    cmake_text = read("CMakeLists.txt")
    for token in (
        "ResultRanking.cpp",
        "result_ranking_tests",
        "result_merger_tests",
        "everything_ipc_runtime_tests",
    ):
        if token not in cmake_text:
            fail(f"alpha.3 build/test wiring missing: {token}")

    runtime_test = read("tests/EverythingIpcRuntimeTests.cpp")
    if "result.score > 0" not in runtime_test:
        fail(
            "EverythingProvider runtime test no longer verifies "
            "dynamic rank scoring"
        )

    combined_everything_source = (
        read("src/core/EverythingIpcProtocol.hpp")
        + read("src/platform/EverythingIpcClient.cpp")
        + everything_provider
    )
    for forbidden in (
        "Everything64.dll",
        "Everything32.dll",
        "LoadLibraryW",
        "LoadLibraryA",
    ):
        if forbidden in combined_everything_source:
            fail(
                "alpha.3 must keep native IPC with no Everything DLL "
                f"dependency: found {forbidden}"
            )

    launcher_hpp = read("src/ui/LauncherWindow.hpp")
    for name, expected in {
        "widthLogical_": 420,
        "rowHeightLogical_": 16,
        "maxResults_": 10,
    }.items():
        found = re.search(
            rf"\b{re.escape(name)}\s*\{{(\d+)\}}",
            launcher_hpp,
        )
        if not found or int(found.group(1)) != expected:
            fail(
                f"Classic geometry changed during alpha.3: {name}"
            )

    print(
        "v0.5.0-alpha.3 unified ranking/Classic UX contract verified:",
        "| settings=2 commands=1 usage=1 provider-cache=2",
        "| Everything default-off | no Everything DLL",
        "| unified ranking + settled single-result policy",
    )
    raise SystemExit(0)


if version == "0.5.0-alpha.2":
    expected_schemas = {
        "kSettingsSchemaVersion": 2,
        "kCommandsSchemaVersion": 1,
        "kUsageSchemaVersion": 1,
    }
    for name, expected in expected_schemas.items():
        actual = cpp_int("src/core/ConfigIO.hpp", name)
        if actual != expected:
            fail(
                f"{name}={actual}, expected alpha.2 value {expected}"
            )

    provider_cache_schema = cpp_int(
        "src/core/ProviderCache.cpp",
        "kProviderCacheSchemaVersion",
    )
    if provider_cache_schema != 2:
        fail("provider-cache schema must remain 2 in alpha.2")

    provider_text = read("src/core/ProviderIds.hpp")
    for provider_id in (
        "windows.startmenu",
        "windows.packaged",
        "windows.apppaths",
        "windows.path",
        "everything.filesystem",
    ):
        if provider_id not in provider_text:
            fail(f"provider ID missing in alpha.2: {provider_id}")

    if "{std::string(kEverythingFilesystem)" in provider_text:
        fail(
            "everything.filesystem must remain absent from DefaultEnabled "
            "during alpha.2"
        )

    settings = json.loads(read("config/settings.example.json"))
    if settings.get("schemaVersion") != 2:
        fail("settings schema must remain 2 in alpha.2")
    if "everything.filesystem" in settings.get("providers", {}):
        fail(
            "everything.filesystem must remain default-off and absent from "
            "the alpha.2 example settings"
        )

    launcher_result = read("src/core/LauncherResult.hpp")
    for token in (
        "enum class ResultKind",
        "UserCommand",
        "Application",
        "File",
        "Folder",
        "LauncherActionKind",
        "providerId",
        "subtitle",
        "target",
        "score",
        "action",
    ):
        if token not in launcher_result:
            fail(f"LauncherResult contract missing: {token}")

    dynamic_provider = read("src/core/DynamicQueryProvider.hpp")
    for token in (
        "DynamicQueryRequest",
        "DynamicQueryResponse",
        "class DynamicQueryProvider",
        "QueryAsync",
    ):
        if token not in dynamic_provider:
            fail(f"dynamic provider contract missing: {token}")

    everything_provider = read("src/core/EverythingProvider.cpp")
    for token in (
        "kEverythingFilesystem",
        "ResultKind::Folder",
        "ResultKind::File",
        "OpenFolder",
        "OpenFile",
    ):
        if token not in everything_provider:
            fail(f"EverythingProvider mapping missing: {token}")

    app = read("src/app/App.cpp")
    for token in (
        "BeginDynamicSearch",
        "kDynamicQueryMessage",
        "HandleDynamicQueryCompleted",
        "DynamicSearchEnabled",
        "ExecuteResult",
        "ShellExecuteExW",
    ):
        if token not in app:
            fail(f"App dynamic-result integration missing: {token}")

    launcher = read("src/ui/LauncherWindow.cpp")
    for token in (
        "ApplyDynamicResults",
        "MergeLauncherResultsStaticFirst",
        "searchGeneration_",
        "DynamicSearchEnabled",
        "ExecuteResult",
    ):
        if token not in launcher:
            fail(f"Launcher dynamic-result integration missing: {token}")

    merger = read("src/core/ResultMerger.cpp")
    for token in (
        "MergeLauncherResultsStaticFirst",
        "SameLauncherTarget",
    ):
        if token not in merger:
            fail(f"alpha.2 merger contract missing: {token}")

    combined_everything_source = (
        read("src/core/EverythingIpcProtocol.hpp")
        + read("src/platform/EverythingIpcClient.cpp")
        + everything_provider
    )
    for forbidden in (
        "Everything64.dll",
        "Everything32.dll",
        "LoadLibraryW",
        "LoadLibraryA",
    ):
        if forbidden in combined_everything_source:
            fail(
                "alpha.2 must keep native IPC with no Everything DLL "
                f"dependency: found {forbidden}"
            )

    cmake_text = read("CMakeLists.txt")
    for token in (
        "EverythingProvider.cpp",
        "LauncherResult.cpp",
        "ResultMerger.cpp",
        "result_merger_tests",
        "everything_ipc_runtime_tests",
    ):
        if token not in cmake_text:
            fail(f"alpha.2 build/test wiring missing: {token}")

    runtime_test = read("tests/EverythingIpcRuntimeTests.cpp")
    for token in (
        "EverythingProvider provider",
        "everything.filesystem",
        "LauncherActionKind",
    ):
        if token not in runtime_test:
            fail(f"EverythingProvider runtime mapping test missing: {token}")

    launcher_hpp = read("src/ui/LauncherWindow.hpp")
    for name, expected in {
        "widthLogical_": 420,
        "rowHeightLogical_": 16,
        "maxResults_": 10,
    }.items():
        found = re.search(
            rf"\b{re.escape(name)}\s*\{{(\d+)\}}",
            launcher_hpp,
        )
        if not found or int(found.group(1)) != expected:
            fail(
                f"Classic geometry changed during alpha.2: {name}"
            )

    print(
        "v0.5.0-alpha.2 dynamic result contract verified:",
        "| settings=2 commands=1 usage=1 provider-cache=2",
        "| Everything default-off | no Everything DLL",
        "| unified LauncherResult + async File/Folder results",
    )
    raise SystemExit(0)


if version == "0.5.0-alpha.1":
    expected_schemas = {
        "kSettingsSchemaVersion": 2,
        "kCommandsSchemaVersion": 1,
        "kUsageSchemaVersion": 1,
    }
    for name, expected in expected_schemas.items():
        actual = cpp_int("src/core/ConfigIO.hpp", name)
        if actual != expected:
            fail(
                f"{name}={actual}, expected alpha.1 foundation value "
                f"{expected}"
            )

    provider_cache_schema = cpp_int(
        "src/core/ProviderCache.cpp",
        "kProviderCacheSchemaVersion",
    )
    if provider_cache_schema != 2:
        fail(
            "provider-cache schema must remain 2 during Everything "
            "IPC foundation"
        )

    provider_text = read("src/core/ProviderIds.hpp")
    for provider_id in (
        "windows.startmenu",
        "windows.packaged",
        "windows.apppaths",
        "windows.path",
    ):
        if provider_id not in provider_text:
            fail(f"existing provider ID changed or disappeared: {provider_id}")
    if "everything.filesystem" in provider_text:
        fail(
            "everything.filesystem must not enter ProviderRegistry "
            "until v0.5.0-alpha.2"
        )

    settings = json.loads(read("config/settings.example.json"))
    if settings.get("schemaVersion") != 2:
        fail("settings schema must remain 2 in v0.5.0-alpha.1")
    if "everything.filesystem" in settings.get("providers", {}):
        fail(
            "Everything settings/provider enablement is reserved for "
            "a later v0.5 phase"
        )

    protocol = read("src/core/EverythingIpcProtocol.hpp")
    for token in (
        "kCopyDataQuery2W = 18",
        "kRequestName = 0x00000001",
        "kRequestPath = 0x00000002",
        "kRequestFullPathAndName = 0x00000004",
    ):
        if token not in protocol:
            fail(f"Everything Query2 protocol contract is missing: {token}")

    client = read("src/platform/EverythingIpcClient.cpp")
    for token in (
        "SendMessageTimeoutW",
        "WM_COPYDATA",
        "FindWindowW",
        "latestGeneration_",
        "kDebounceTimerId",
        "kReplyTimerId",
    ):
        if token not in client:
            fail(f"Everything IPC client foundation is missing: {token}")

    combined_everything_source = protocol + client
    for forbidden in (
        "Everything64.dll",
        "Everything32.dll",
        "LoadLibraryW",
        "LoadLibraryA",
    ):
        if forbidden in combined_everything_source:
            fail(
                "v0.5.0-alpha.1 must use native WM_COPYDATA IPC "
                f"without Everything DLL loading: found {forbidden}"
            )

    cmake_text = read("CMakeLists.txt")
    for token in (
        "EverythingIpcProtocol.cpp",
        "EverythingIpcClient.cpp",
        "everything_ipc_protocol_tests",
        "everything_ipc_runtime_tests",
    ):
        if token not in cmake_text:
            fail(f"Everything alpha.1 build/test wiring is missing: {token}")

    for workflow_path in (
        ".github/workflows/build.yml",
        ".github/workflows/release.yml",
    ):
        workflow = read(workflow_path)
        if "everything_ipc_runtime_tests" not in workflow:
            fail(
                f"{workflow_path} does not run the Everything IPC "
                "runtime smoke"
            )

    launcher = read("src/ui/LauncherWindow.hpp")
    for name, expected in {
        "widthLogical_": 420,
        "rowHeightLogical_": 16,
        "maxResults_": 10,
    }.items():
        found = re.search(
            rf"\b{re.escape(name)}\s*\{{(\d+)\}}",
            launcher,
        )
        if not found or int(found.group(1)) != expected:
            fail(
                f"Classic geometry changed during alpha.1 foundation: "
                f"{name}"
            )

    print(
        "v0.5.0-alpha.1 Everything IPC foundation contract verified:",
        "| settings=2 commands=1 usage=1 provider-cache=2",
        "| no Everything DLL | no launcher integration",
    )
    raise SystemExit(0)

# This gate is intentionally scoped to the frozen v0.4.1 line. It becomes
# a no-op when main advances to a later feature line.
if base != "0.4.1":
    print(f"v0.4.1 release contract skipped for {version}")
    raise SystemExit(0)

if channel == "alpha":
    fail("feature freeze is active; v0.4.1 must not return to an alpha version")

expected_schemas = {
    "kSettingsSchemaVersion": 2,
    "kCommandsSchemaVersion": 1,
    "kUsageSchemaVersion": 1,
}
for name, expected in expected_schemas.items():
    actual = cpp_int("src/core/ConfigIO.hpp", name)
    if actual != expected:
        fail(f"{name}={actual}, expected frozen value {expected}")

provider_cache_schema = cpp_int(
    "src/core/ProviderCache.cpp",
    "kProviderCacheSchemaVersion",
)
if provider_cache_schema != 2:
    fail(
        "provider-cache schema changed during v0.4.1 freeze: "
        f"{provider_cache_schema} != 2"
    )

provider_text = read("src/core/ProviderIds.hpp")
provider_names = {
    "kStartMenu": "windows.startmenu",
    "kPackaged": "windows.packaged",
    "kAppPaths": "windows.apppaths",
    "kPath": "windows.path",
}
for name, expected in provider_names.items():
    found = re.search(
        rf'\b{re.escape(name)}\s*=\s*"([^"]+)"',
        provider_text,
        re.MULTILINE,
    )
    if not found:
        fail(f"{name} was not found in src/core/ProviderIds.hpp")
    if found.group(1) != expected:
        fail(
            f"{name}={found.group(1)!r}, expected frozen provider ID "
            f"{expected!r}"
        )

expected_settings = {
    "schemaVersion": 2,
    "general": {
        "startWithWindows": False,
        "showOnStartup": False,
        "hideAfterLaunch": True,
        "clearQueryOnShow": True,
        "hideOnFocusLost": True,
        "showTrayIcon": True,
        "popupMonitor": "cursor",
    },
    "hotkey": {
        "modifiers": ["alt"],
        "key": "space",
        "auxiliary": {
            "enabled": False,
            "modifiers": [],
            "key": "pause",
        },
    },
    "behavior": {
        "wildcardMatching": False,
        "numericQuickLaunch": False,
        "numericQuickLaunchOrder": "one-to-zero",
        "executeSingleResultImmediately": False,
    },
    "appearance": {
        "launcher": "classic",
        "language": "zh-CN",
    },
    "providers": {
        "windows.startmenu": True,
        "windows.packaged": True,
        "windows.apppaths": True,
        "windows.path": True,
    },
}

actual_settings = json.loads(read("config/settings.example.json"))
if actual_settings != expected_settings:
    fail(
        "config/settings.example.json no longer matches the frozen "
        "v0.4.1 defaults"
    )

launcher = read("src/ui/LauncherWindow.hpp")
classic_geometry = {
    "widthLogical_": 420,
    "rowHeightLogical_": 16,
    "maxResults_": 10,
}
for name, expected in classic_geometry.items():
    found = re.search(
        rf"\b{re.escape(name)}\s*\{{(\d+)\}}",
        launcher,
    )
    if not found:
        fail(f"{name} was not found in src/ui/LauncherWindow.hpp")
    if int(found.group(1)) != expected:
        fail(
            f"{name}={found.group(1)}, expected frozen Classic value "
            f"{expected}"
        )

validation_doc = ROOT / "docs" / "DESKTOP_VALIDATION.md"
if not validation_doc.exists():
    fail("docs/DESKTOP_VALIDATION.md is required during Beta/RC validation")

validation_text = validation_doc.read_text(encoding="utf-8")
for token in (
    "Hotkey lifecycle",
    "IME",
    "100%",
    "125%",
    "150%",
    "200%",
    "Upgrade / downgrade",
    "Release assets",
):
    if token not in validation_text:
        fail(f"desktop validation guide is missing required section/token: {token!r}")

cmake_text = read("CMakeLists.txt")
for token in (
    "altrun_enable_test_assertions",
    "desktop_validation_tests",
    "hotkey_runtime_tests",
):
    if token not in cmake_text:
        fail(f"validation-integrity CMake target/helper is missing: {token}")

desktop_test = read("tests/DesktopValidationTests.cpp")
if "#ifdef NDEBUG" not in desktop_test:
    fail("desktop validation tests no longer prove assert() is enabled")

hotkey_runtime_test = read("tests/HotkeyRuntimeTests.cpp")
for token in (
    "RegisterHotKey",
    "UnregisterHotKey",
):
    if token not in hotkey_runtime_test:
        fail(f"hotkey runtime validation is missing {token}")

launcher_cpp = read("src/ui/LauncherWindow.cpp")
for token in (
    "ClassicBehavior.hpp",
    "ShouldExecuteSingleResult",
    "QuickLaunchIndexForDigit",
):
    if token not in launcher_cpp:
        fail(f"LauncherWindow is no longer wired to validated behavior helper: {token}")

settings_hpp = read("src/ui/SettingsWindow.hpp")
if "SettingsLayout.hpp" not in settings_hpp:
    fail("SettingsWindow no longer includes the validated layout helper")

settings_cpp = read("src/ui/SettingsWindow.cpp")
for token in (
    "MaxScrollOffset",
    "ClampRectToWorkArea",
):
    if token not in settings_cpp:
        fail(f"SettingsWindow is no longer wired to validated layout helper: {token}")

release_workflow = read(".github/workflows/release.yml")
for token in (
    "Release tag preflight",
    "verify_tag_version.py",
    "sha256sum -c SHA256SUMS.txt",
):
    if token not in release_workflow:
        fail(f"release-candidate workflow hardening is missing: {token}")

build_workflow = read(".github/workflows/build.yml")
if "sha256sum -c SHA256SUMS.txt" not in build_workflow:
    fail("main release checksum self-verification is missing")

package_script = read("scripts/verify_package.ps1")
for token in (
    "$allowedTopLevel",
    "commands.example.json",
    "DESKTOP_VALIDATION.md",
):
    if token not in package_script:
        fail(f"package allowlist hardening is missing: {token}")

tag_script = ROOT / "scripts" / "verify_tag_version.py"
if not tag_script.exists():
    fail("scripts/verify_tag_version.py is required for RC publication")

print(
    "v0.4.1 frozen release contract verified:",
    version,
    "| settings=2 commands=1 usage=1 provider-cache=2",
)
