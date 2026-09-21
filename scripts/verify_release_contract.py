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





if version == "0.8.0-alpha.2.4":
    expected_schemas = {
        "kSettingsSchemaVersion": 8,
        "kCommandsSchemaVersion": 2,
        "kUsageSchemaVersion": 1,
    }
    for name, expected in expected_schemas.items():
        actual = cpp_int("src/core/ConfigIO.hpp", name)
        if actual != expected:
            fail(f"v0.8 alpha.2.4 {name}={actual}, expected {expected}")

    if cpp_int("src/core/ProviderCache.cpp", "kProviderCacheSchemaVersion") != 2:
        fail("v0.8 alpha.2.4 must keep provider-cache schemaVersion 2")

    settings = json.loads(read("config/settings.example.json"))
    if settings.get("schemaVersion") != 8:
        fail("v0.8 alpha.2.4 settings example must remain schemaVersion 8")
    if settings.get("update") != {"autoCheck": True, "channel": "development"}:
        fail("v0.8 alpha.2.4 prerelease must default to Development updates")

    expected_providers = {
        "windows.startmenu": True,
        "windows.packaged": True,
        "windows.apppaths": True,
        "windows.path": True,
        "everything.filesystem": False,
    }
    if settings.get("providers") != expected_providers:
        fail("v0.8 alpha.2.4 changed frozen provider defaults")

    expected_hotkeys = {
        "launcher.activate",
        "launcher.activateSecondary",
        "launcher.openSettings",
        "result.navigateCurrentFileManager",
        "result.copySelectedTarget",
    }
    bindings = settings.get("hotkeys", {}).get("bindings", {})
    if set(bindings) != expected_hotkeys:
        fail("v0.8 alpha.2.4 changed frozen Hotkey Registry action IDs")

    expected_placement = {
        "launcherMode": "top",
        "settingsMode": "center",
        "launcherLastValid": False,
        "launcherLastX": 0,
        "launcherLastY": 0,
        "settingsLastValid": False,
        "settingsLastX": 0,
        "settingsLastY": 0,
    }
    if settings.get("windowPlacement") != expected_placement:
        fail("v0.8 alpha.2.4 changed window-placement defaults")

    metrics = read("src/ui/UiMetrics.hpp")
    for token in (
        "kSettingsClientWidthLogical = 820",
        "kSettingsClientHeightLogical = 620",
        "kSettingsSidebarWidthLogical = 176",
        "kSettingsToggleRowLogical = 50",
        "kSettingsComboRowLogical = 54",
    ):
        if token not in metrics:
            fail(f"v0.8 alpha.2.4 Settings metric contract missing: {token}")

    settings_cpp = read("src/ui/SettingsWindow.cpp")
    settings_h = read("src/ui/SettingsWindow.hpp")

    # Sidebar navigation stays deliberately left-aligned.
    nav_start = settings_cpp.find("void SettingsWindow::DrawNavigationButton(")
    nav_end = settings_cpp.find("void SettingsWindow::DrawSwitchGlyph(", nav_start)
    if nav_start < 0 or nav_end < 0:
        fail("v0.8 alpha.2.4 navigation drawing implementation was not found")
    nav_draw = settings_cpp[nav_start:nav_end]
    for token in ("DT_LEFT", "selectionBackground"):
        if token not in nav_draw:
            fail(f"v0.8 alpha.2.4 left-aligned navigation contract missing: {token}")
    if "DrawFocusRect" in nav_draw:
        fail("v0.8 alpha.2.4 sidebar navigation must not restore dotted focus")

    for token in (
        "const int comboWidth =\n            Scale(220);",
        "Scale(560)",
        "Scale(200)",
        "hotkeyGlobalTitle_",
        "hotkeyLauncherTitle_",
        "HotkeyRowControls",
        "kIdHotkeyCaptureBase",
        "kIdHotkeyEnabledBase",
        "kIdHotkeyResetBase",
        "BeginHotkeyCapture(\n        std::string_view actionId)",
        "ToggleHotkeyActionEnabled(",
        "ResetHotkeyAction(",
        "DrawHotkeyToggle(",
        "DrawSwitchGlyph(",
        "pendingProviderStates_",
        "kProviderCommitTimerId",
        "CommitPendingProviderChanges()",
        "providerCommitInProgress_",
        "SetProviderEnabledBatch(",
        "ordinaryChanges",
        "180",
        "ImportCommands()",
        'T(L"导入快捷项…"',
        'T(L"导出快捷项…"',
        "const int transferWidth =",
        "const int maintenanceWidth =",
    ):
        if token not in settings_cpp and token not in settings_h:
            fail(f"v0.8 alpha.2.4 Settings final-polish contract missing: {token}")

    for token in (
        "hotkeyActionList_",
        "hotkeyEditorTitle_",
        "hotkeyEditorDescription_",
        "hotkeyScope_",
        "hotkeyEnabled_",
        "hotkeyCapture_",
        "hotkeyResetCurrent_",
        "selectedHotkeyActionId_",
        "DrawHotkeyActionItem",
        "LoadHotkeyEditor",
        "ToggleSelectedHotkeyEnabled",
        "ResetSelectedHotkey",
        "kIdDataImportLegacy",
        "dataImportLegacy_",
        "legacyMode",
        "ImportCommands(false)",
        "ImportCommands(true)",
    ):
        if token in settings_cpp or token in settings_h:
            fail(f"v0.8 alpha.2.4 obsolete Settings path remains: {token}")

    user_store = read("src/core/UserCommandStore.cpp") + read("src/core/UserCommandStore.hpp")
    command_store = read("src/core/CommandStore.cpp") + read("src/core/CommandStore.hpp")
    settings_store = read("src/core/Settings.cpp") + read("src/core/Settings.hpp")
    app = read("src/app/App.cpp") + read("src/app/App.hpp")

    if "SetProviderEnabledBatch(" not in settings_store or "SetProviderEnabledBatch(" not in app:
        fail("v0.8 alpha.2.4 ordinary provider batching API is missing")

    for token in ("legacyMode",):
        if token in user_store or token in command_store or token in app:
            fail(f"v0.8 alpha.2.4 manual legacy import mode remains in production API: {token}")

    # Manual legacy import is gone, but automatic data migration and identity
    # compatibility must remain available to existing ALTRun Next users.
    for token in ("MigrateLegacyTsv", "legacyTsvPath_", "legacyIds"):
        if token not in user_store:
            fail(f"v0.8 alpha.2.4 automatic legacy compatibility was removed: {token}")

    if "line.find(L'=')" in user_store:
        fail("v0.8 alpha.2.4 permissive manual key=value legacy import parser remains")

    if "ImportTsv(\n        const std::filesystem::path& path,\n        bool" in user_store:
        fail("v0.8 alpha.2.4 UserCommandStore still exposes legacy-mode import flag")

    update_tests = read("tests/UpdatePolicyTests.cpp")
    for token in (
        '"0.8.0-alpha.2.3"',
        '"0.8.0-alpha.2.4"',
        '"0.8.0-alpha.3"',
        "UpdateChannel::Development",
    ):
        if token not in update_tests:
            fail(f"v0.8 alpha.2.4 update ordering/default coverage missing: {token}")

    config_tests = read("tests/ConfigCoreTests.cpp")
    schema_tests = read("tests/UserCommandSchemaTests.cpp")
    if "ImportTsv(\n        exportedCommands,\n        false," in config_tests:
        fail("v0.8 alpha.2.4 ConfigCore still uses removed legacy-mode argument")
    if "ImportTsv(\n            exportPath,\n            false," in schema_tests:
        fail("v0.8 alpha.2.4 UserCommandSchema still uses removed legacy-mode argument")

    for token in (
        "RuntimeDiagnosticsSnapshot",
        "ProcessMemory",
        "Page::Diagnostics",
        "kIdNavDiagnostics",
    ):
        if token in settings_cpp or token in settings_h or token in app:
            fail(f"v0.8 alpha.2.4 removed Diagnostics wiring returned: {token}")

    readme = read("README.md")
    changelog = read("CHANGELOG.md")
    roadmap = read("ROADMAP.md")

    for token in (
        "## v0.8.0-alpha.2.4 — Settings Final Polish & Hotkey Redesign",
        "0.8.0.24",
    ):
        if token not in readme:
            fail(f"v0.8 alpha.2.4 README contract missing: {token}")

    for token in (
        "## 0.8.0-alpha.2.4",
        "0.8.0.24",
    ):
        if token not in changelog:
            fail(f"v0.8 alpha.2.4 changelog contract missing: {token}")

    for token in (
        "v0.8.0-alpha.2.4",
        "v0.8.0-alpha.3",
    ):
        if token not in roadmap:
            fail(f"v0.8 alpha.2.4 roadmap contract missing: {token}")

    print(
        "v0.8.0-alpha.2.4 Settings final-polish contract verified:",
        "| settings=8 commands=2 usage=1 provider-cache=2",
        "| sidebar navigation remains left-aligned",
        "| compact placement/appearance selectors",
        "| grouped inline hotkey editor",
        "| 180ms final-intent provider debounce",
        "| manual legacy import removed; automatic migration preserved",
        "| balanced Data action layout",
        "| frozen v0.7 behavior preserved",
    )
    raise SystemExit(0)

if version == "0.8.0-alpha.2.3":
    expected_schemas = {
        "kSettingsSchemaVersion": 8,
        "kCommandsSchemaVersion": 2,
        "kUsageSchemaVersion": 1,
    }
    for name, expected in expected_schemas.items():
        actual = cpp_int("src/core/ConfigIO.hpp", name)
        if actual != expected:
            fail(f"v0.8 alpha.2.3 {name}={actual}, expected {expected}")

    if cpp_int("src/core/ProviderCache.cpp", "kProviderCacheSchemaVersion") != 2:
        fail("v0.8 alpha.2.3 must keep provider-cache schemaVersion 2")

    settings = json.loads(read("config/settings.example.json"))
    if settings.get("schemaVersion") != 8:
        fail("v0.8 alpha.2.3 settings example must remain schemaVersion 8")
    if settings.get("update") != {"autoCheck": True, "channel": "development"}:
        fail("v0.8 alpha.2.3 prerelease must default to Development updates")

    expected_providers = {
        "windows.startmenu": True,
        "windows.packaged": True,
        "windows.apppaths": True,
        "windows.path": True,
        "everything.filesystem": False,
    }
    if settings.get("providers") != expected_providers:
        fail("v0.8 alpha.2.3 changed frozen provider defaults")

    expected_hotkeys = {
        "launcher.activate",
        "launcher.activateSecondary",
        "launcher.openSettings",
        "result.navigateCurrentFileManager",
        "result.copySelectedTarget",
    }
    bindings = settings.get("hotkeys", {}).get("bindings", {})
    if set(bindings) != expected_hotkeys:
        fail("v0.8 alpha.2.3 changed frozen Hotkey Registry action IDs")

    expected_placement = {
        "launcherMode": "top",
        "settingsMode": "center",
        "launcherLastValid": False,
        "launcherLastX": 0,
        "launcherLastY": 0,
        "settingsLastValid": False,
        "settingsLastX": 0,
        "settingsLastY": 0,
    }
    if settings.get("windowPlacement") != expected_placement:
        fail("v0.8 alpha.2.3 changed window-placement defaults")

    metrics = read("src/ui/UiMetrics.hpp")
    for token in (
        "kSettingsClientWidthLogical = 820",
        "kSettingsClientHeightLogical = 620",
        "kSettingsSidebarWidthLogical = 176",
        "kSettingsToggleRowLogical = 50",
        "kSettingsComboRowLogical = 54",
        "kSettingsCardRadiusLogical = 8",
        "kSettingsNavHeightLogical = 40",
        "kSettingsNavGapLogical = 4",
    ):
        if token not in metrics:
            fail(f"v0.8 alpha.2.3 Settings metric contract missing: {token}")

    settings_cpp = read("src/ui/SettingsWindow.cpp")
    settings_h = read("src/ui/SettingsWindow.hpp")

    for token in (
        "ui::kSettingsClientWidthLogical",
        "ui::kSettingsClientHeightLogical",
        "SS_CENTER | SS_NOPREFIX",
        "HALFTONE",
        "StretchBlt",
        "redrawClickedToggle",
        "notify == BN_CLICKED",
        "notify == BN_DOUBLECLICKED",
        "const bool focused =",
        ": focused",
        "? kCardPressed",
    ):
        if token not in settings_cpp:
            fail(f"v0.8 alpha.2.3 interaction contract missing: {token}")

    nav_start = settings_cpp.find("void SettingsWindow::DrawNavigationButton(")
    nav_end = settings_cpp.find("void SettingsWindow::DrawHotkeyActionItem(", nav_start)
    if nav_start < 0 or nav_end < 0:
        fail("v0.8 alpha.2.3 navigation drawing implementation was not found")
    nav_draw = settings_cpp[nav_start:nav_end]
    if "DrawFocusRect" in nav_draw:
        fail("v0.8 alpha.2.3 sidebar navigation must not use dotted DrawFocusRect")

    for token in (
        "WS_OVERLAPPEDWINDOW",
        "WS_THICKFRAME",
        "WS_MAXIMIZEBOX",
        "WM_GETMINMAXINFO",
        "Page::Diagnostics",
        "kIdNavDiagnostics",
        "navDiagnostics_",
        "CreateDiagnosticsPage",
        "RefreshActionDiagnostics",
        "kDiagnosticsStatusTimerId",
        "diagnosticsControls_",
        "RuntimeDiagnosticsSnapshot",
        "ProcessMemory",
    ):
        if token in settings_cpp or token in settings_h:
            fail(f"v0.8 alpha.2.3 obsolete Settings/Diagnostics wiring remains: {token}")

    app = read("src/app/App.cpp") + read("src/app/App.hpp")
    for token in (
        "RuntimeDiagnosticsSnapshot",
        "RuntimeDiagnostics()",
        "ProcessMemory",
        "QueryCurrentProcessMemory",
    ):
        if token in app:
            fail(f"v0.8 alpha.2.3 runtime diagnostics API remains: {token}")

    cmake = read("CMakeLists.txt")
    build_workflow = read(".github/workflows/build.yml")
    for token in (
        "src/platform/ProcessMemory.cpp",
        "process_memory_tests",
        "tests/ProcessMemoryTests.cpp",
        "psapi",
    ):
        if token in cmake or token in build_workflow:
            fail(f"v0.8 alpha.2.3 deleted diagnostics build dependency remains: {token}")

    ui_test = read("tests/UiFoundationTests.cpp")
    for token in (
        "kSettingsClientWidthLogical == 820",
        "kSettingsClientHeightLogical == 620",
        "kSettingsSidebarWidthLogical == 176",
    ):
        if token not in ui_test:
            fail(f"v0.8 alpha.2.3 UI viewport coverage missing: {token}")

    desktop_test = read("tests/DesktopValidationTests.cpp")
    for token in (
        "kSettingsClientWidthLogical",
        "kSettingsClientHeightLogical",
        "kGeneralCardMaxWidthLogical == 560",
        "layout.behavior.left ==",
        "layout.search.top >",
    ):
        if token not in desktop_test:
            fail(f"v0.8 alpha.2.3 compact-layout coverage missing: {token}")

    update_tests = read("tests/UpdatePolicyTests.cpp")
    for token in (
        '"0.8.0-alpha.2.2"',
        '"0.8.0-alpha.2.3"',
        '"0.8.0-alpha.3"',
        "UpdateChannel::Development",
    ):
        if token not in update_tests:
            fail(f"v0.8 alpha.2.3 update ordering/default coverage missing: {token}")

    readme = read("README.md")
    changelog = read("CHANGELOG.md")
    roadmap = read("ROADMAP.md")

    for token in (
        "## v0.8.0-alpha.2.3 — Settings Compactness & Input Polish",
        "0.8.0.23",
    ):
        if token not in readme:
            fail(f"v0.8 alpha.2.3 README contract missing: {token}")

    for token in (
        "## 0.8.0-alpha.2.3",
        "0.8.0.23",
    ):
        if token not in changelog:
            fail(f"v0.8 alpha.2.3 changelog contract missing: {token}")

    for token in (
        "v0.8.0-alpha.2.3",
        "v0.8.0-alpha.3",
    ):
        if token not in roadmap:
            fail(f"v0.8 alpha.2.3 roadmap contract missing: {token}")

    print(
        "v0.8.0-alpha.2.3 Settings compactness/input contract verified:",
        "| settings=8 commands=2 usage=1 provider-cache=2",
        "| fixed 820x620 Settings client",
        "| no dotted sidebar DrawFocusRect",
        "| BN_CLICKED + BN_DOUBLECLICKED toggle activation",
        "| alpha.2.2 supersampled switch rendering preserved",
        "| embedded Diagnostics remains removed",
        "| frozen v0.7 behavior preserved",
    )
    raise SystemExit(0)

if version == "0.8.0-alpha.2.2":
    expected_schemas = {
        "kSettingsSchemaVersion": 8,
        "kCommandsSchemaVersion": 2,
        "kUsageSchemaVersion": 1,
    }
    for name, expected in expected_schemas.items():
        actual = cpp_int("src/core/ConfigIO.hpp", name)
        if actual != expected:
            fail(f"v0.8 alpha.2.2 {name}={actual}, expected {expected}")

    if cpp_int("src/core/ProviderCache.cpp", "kProviderCacheSchemaVersion") != 2:
        fail("v0.8 alpha.2.2 must keep provider-cache schemaVersion 2")

    settings = json.loads(read("config/settings.example.json"))
    if settings.get("schemaVersion") != 8:
        fail("v0.8 alpha.2.2 settings example must remain schemaVersion 8")
    if settings.get("update") != {"autoCheck": True, "channel": "development"}:
        fail("v0.8 alpha.2.2 prerelease must default to Development updates")

    expected_providers = {
        "windows.startmenu": True,
        "windows.packaged": True,
        "windows.apppaths": True,
        "windows.path": True,
        "everything.filesystem": False,
    }
    if settings.get("providers") != expected_providers:
        fail("v0.8 alpha.2.2 changed frozen provider defaults")

    expected_hotkeys = {
        "launcher.activate",
        "launcher.activateSecondary",
        "launcher.openSettings",
        "result.navigateCurrentFileManager",
        "result.copySelectedTarget",
    }
    bindings = settings.get("hotkeys", {}).get("bindings", {})
    if set(bindings) != expected_hotkeys:
        fail("v0.8 alpha.2.2 changed frozen Hotkey Registry action IDs")

    expected_placement = {
        "launcherMode": "top",
        "settingsMode": "center",
        "launcherLastValid": False,
        "launcherLastX": 0,
        "launcherLastY": 0,
        "settingsLastValid": False,
        "settingsLastX": 0,
        "settingsLastY": 0,
    }
    if settings.get("windowPlacement") != expected_placement:
        fail("v0.8 alpha.2.2 changed window-placement defaults")

    metrics = read("src/ui/UiMetrics.hpp")
    for token in (
        "kClassicLauncherMetrics",
        "420",
        "16",
        "10",
        "kModernCompactLauncherMetrics",
        "620",
        "32",
        "9",
        "kSettingsSidebarWidthLogical = 176",
        "kSettingsToggleRowLogical = 50",
        "kSettingsComboRowLogical = 54",
        "kSettingsCardRadiusLogical = 8",
        "kSettingsNavHeightLogical = 40",
        "kSettingsNavGapLogical = 4",
    ):
        if token not in metrics:
            fail(f"v0.8 alpha.2.2 Settings metric contract missing: {token}")

    layout_h = read("src/core/SettingsLayout.hpp")
    layout_cpp = read("src/core/SettingsLayout.cpp")
    for token in (
        "kPageTitleTopLogical = 22",
        "kPageDividerTopLogical = 84",
        "kSectionTitleTopLogical = 108",
        "kFirstCardTopLogical = 140",
        "kGeneralCardMaxWidthLogical = 560",
    ):
        if token not in layout_h:
            fail(f"v0.8 alpha.2.2 single-column metric missing: {token}")

    if "stackedCards" in layout_h or "stackedCards" in layout_cpp:
        fail("v0.8 alpha.2.2 General must not retain responsive two-column/stacked switching")

    settings_cpp = read("src/ui/SettingsWindow.cpp")
    settings_h = read("src/ui/SettingsWindow.hpp")

    for token in (
        "WS_CAPTION",
        "WS_SYSMENU",
        "WS_MINIMIZEBOX",
        "Scale(820)",
        "Scale(720)",
        "SS_CENTER | SS_NOPREFIX",
        'T(L"当前鼠标所在显示器"',
        'T(L"当前活动窗口所在显示器"',
        'T(L"主显示器"',
        'T(L"靠近屏幕上方"',
        'T(L"屏幕居中"',
        'T(L"上次位置"',
        "HALFTONE",
        "StretchBlt",
        "redrawClickedToggle",
        "WM_SETREDRAW",
        "OPAQUE",
    ):
        if token not in settings_cpp:
            fail(f"v0.8 alpha.2.2 Settings interaction contract missing: {token}")

    for token in (
        "WS_OVERLAPPEDWINDOW",
        "WS_THICKFRAME",
        "WS_MAXIMIZEBOX",
        "WM_GETMINMAXINFO",
        "Page::Diagnostics",
        "kIdNavDiagnostics",
        "navDiagnostics_",
        "CreateDiagnosticsPage",
        "RefreshActionDiagnostics",
        "kDiagnosticsStatusTimerId",
        "diagnosticsControls_",
        "diagnosticsMemory",
        "diagnosticsSearch",
        "actionsWindows",
        "actionsClipboard",
        "actionsWeb",
    ):
        if token in settings_cpp or token in settings_h:
            fail(f"v0.8 alpha.2.2 obsolete Settings/Diagnostics wiring remains: {token}")

    app = read("src/app/App.cpp") + read("src/app/App.hpp")
    for token in (
        "RuntimeDiagnosticsSnapshot",
        "RuntimeDiagnostics()",
        "ProcessMemory",
        "QueryCurrentProcessMemory",
    ):
        if token in app:
            fail(f"v0.8 alpha.2.2 runtime diagnostics API remains: {token}")

    cmake = read("CMakeLists.txt")
    for token in (
        "src/platform/ProcessMemory.cpp",
        "process_memory_tests",
        "tests/ProcessMemoryTests.cpp",
        "psapi",
    ):
        if token in cmake:
            fail(f"v0.8 alpha.2.2 diagnostics build dependency remains: {token}")

    build_workflow = read(".github/workflows/build.yml")
    if "process_memory_tests" in build_workflow:
        fail("v0.8 alpha.2.2 build workflow still references deleted process-memory tests")

    for path in (
        "src/platform/ProcessMemory.cpp",
        "src/platform/ProcessMemory.hpp",
        "tests/ProcessMemoryTests.cpp",
    ):
        if (ROOT / path).exists():
            fail(f"v0.8 alpha.2.2 deleted diagnostics file still exists: {path}")

    ui_test = read("tests/UiFoundationTests.cpp")
    for token in (
        "kSettingsSidebarWidthLogical == 176",
        "kSettingsToggleRowLogical == 50",
        "kSettingsComboRowLogical == 54",
    ):
        if token not in ui_test:
            fail(f"v0.8 alpha.2.2 UI regression coverage missing: {token}")

    desktop_test = read("tests/DesktopValidationTests.cpp")
    for token in (
        "kGeneralCardMaxWidthLogical == 560",
        "scale(820)",
        "scale(720)",
        "layout.behavior.left ==",
        "layout.search.top >",
    ):
        if token not in desktop_test:
            fail(f"v0.8 alpha.2.2 single-column desktop coverage missing: {token}")

    update_tests = read("tests/UpdatePolicyTests.cpp")
    for token in (
        '"0.8.0-alpha.2.1"',
        '"0.8.0-alpha.2.2"',
        '"0.8.0-alpha.3"',
        "UpdateChannel::Development",
    ):
        if token not in update_tests:
            fail(f"v0.8 alpha.2.2 update ordering/default coverage missing: {token}")

    readme = read("README.md")
    changelog = read("CHANGELOG.md")
    roadmap = read("ROADMAP.md")

    for token in (
        "## v0.8.0-alpha.2.2 — Settings Interaction & Layout Polish",
        "0.8.0.22",
    ):
        if token not in readme:
            fail(f"v0.8 alpha.2.2 README contract missing: {token}")

    for token in (
        "## 0.8.0-alpha.2.2",
        "0.8.0.22",
    ):
        if token not in changelog:
            fail(f"v0.8 alpha.2.2 changelog contract missing: {token}")

    for token in (
        "v0.8.0-alpha.2.2",
        "v0.8.0-alpha.3",
    ):
        if token not in roadmap:
            fail(f"v0.8 alpha.2.2 roadmap contract missing: {token}")

    print(
        "v0.8.0-alpha.2.2 Settings interaction/layout contract verified:",
        "| settings=8 commands=2 usage=1 provider-cache=2",
        "| fixed 820x720 Settings client",
        "| 176px sidebar + centered ALTRun / Next brand",
        "| always-single-column General with 560px max cards",
        "| placement combo choices restored",
        "| supersampled toggle rendering + immediate repaint",
        "| embedded Diagnostics and ProcessMemory probe stack removed",
        "| frozen v0.7 behavior preserved",
    )
    raise SystemExit(0)

if version == "0.8.0-alpha.2.1":
    expected_schemas = {
        "kSettingsSchemaVersion": 8,
        "kCommandsSchemaVersion": 2,
        "kUsageSchemaVersion": 1,
    }
    for name, expected in expected_schemas.items():
        actual = cpp_int("src/core/ConfigIO.hpp", name)
        if actual != expected:
            fail(f"v0.8 alpha.2.1 {name}={actual}, expected {expected}")

    if cpp_int("src/core/ProviderCache.cpp", "kProviderCacheSchemaVersion") != 2:
        fail("v0.8 alpha.2.1 must keep provider-cache schemaVersion 2")

    settings = json.loads(read("config/settings.example.json"))
    if settings.get("schemaVersion") != 8:
        fail("v0.8 alpha.2.1 settings example must remain schemaVersion 8")
    if settings.get("update") != {"autoCheck": True, "channel": "development"}:
        fail("v0.8 alpha.2.1 prerelease must default to Development updates")

    expected_providers = {
        "windows.startmenu": True,
        "windows.packaged": True,
        "windows.apppaths": True,
        "windows.path": True,
        "everything.filesystem": False,
    }
    if settings.get("providers") != expected_providers:
        fail("v0.8 alpha.2.1 changed frozen provider defaults")

    expected_hotkeys = {
        "launcher.activate",
        "launcher.activateSecondary",
        "launcher.openSettings",
        "result.navigateCurrentFileManager",
        "result.copySelectedTarget",
    }
    bindings = settings.get("hotkeys", {}).get("bindings", {})
    if set(bindings) != expected_hotkeys:
        fail("v0.8 alpha.2.1 changed frozen Hotkey Registry action IDs")

    expected_placement = {
        "launcherMode": "top",
        "settingsMode": "center",
        "launcherLastValid": False,
        "launcherLastX": 0,
        "launcherLastY": 0,
        "settingsLastValid": False,
        "settingsLastX": 0,
        "settingsLastY": 0,
    }
    if settings.get("windowPlacement") != expected_placement:
        fail("v0.8 alpha.2.1 changed window-placement defaults")

    metrics = read("src/ui/UiMetrics.hpp")
    for token in (
        "kClassicLauncherMetrics",
        "420",
        "16",
        "10",
        "kModernCompactLauncherMetrics",
        "620",
        "32",
        "9",
        "kSettingsSidebarWidthLogical = 208",
        "kSettingsToggleRowLogical = 50",
        "kSettingsComboRowLogical = 54",
        "kSettingsCardRadiusLogical = 8",
        "kSettingsNavHeightLogical = 40",
        "kSettingsNavGapLogical = 4",
    ):
        if token not in metrics:
            fail(f"v0.8 alpha.2.1 Settings metric contract missing: {token}")

    layout_h = read("src/core/SettingsLayout.hpp")
    for token in (
        "kPageTitleTopLogical = 22",
        "kPageDividerTopLogical = 84",
        "kSectionTitleTopLogical = 108",
        "kFirstCardTopLogical = 140",
    ):
        if token not in layout_h:
            fail(f"v0.8 alpha.2.1 compact page metric missing: {token}")

    settings_cpp = read("src/ui/SettingsWindow.cpp")
    settings_h = read("src/ui/SettingsWindow.hpp")

    for token in (
        'CreateStatic(L"ALTRun")',
        'CreateStatic(L"Next")',
        "AdjustWindowRectExForDpi",
        "WM_SETREDRAW",
        "OPAQUE",
        "sidebarBrush_",
        "cardBrush_",
        "backgroundBrush_",
        "kIdHotkeyEnabled",
        "CreateCheckboxRow(",
    ):
        if token not in settings_cpp:
            fail(f"v0.8 alpha.2.1 Settings stabilization missing: {token}")

    if "HOLLOW_BRUSH" in settings_cpp:
        fail("v0.8 alpha.2.1 must not use transparent HOLLOW_BRUSH for Settings statics")

    for token in (
        "legacyHotkeyControls_",
        "hotkeyCtrl_",
        "hotkeyApply_",
        "auxiliaryHotkeyCtrl_",
        "ApplyAuxiliaryHotkeyControl",
        "ApplyHotkeyControl",
        "RefreshHotkeyControls",
    ):
        if token in settings_h or token in settings_cpp:
            fail(f"v0.8 alpha.2.1 resurrected legacy hotkey UI: {token}")

    launcher_cpp = read("src/ui/LauncherWindow.cpp")
    settings_store = read("src/core/Settings.cpp") + read("src/core/Settings.hpp")
    app = read("src/app/App.cpp") + read("src/app/App.hpp")

    for token in (
        'launcherPlacement{"top"}',
        'settingsPlacement{"center"}',
        "launcherLastPositionValid",
        "settingsLastPositionValid",
        '"windowPlacement"',
        "RememberLauncherPosition(",
        "RememberSettingsPosition(",
    ):
        if token not in settings_store:
            fail(f"v0.8 alpha.2.1 placement persistence contract missing: {token}")

    for token in (
        "SetWindowPlacementSettings",
        "RememberLauncherPosition",
        "RememberSettingsPosition",
    ):
        if token not in app:
            fail(f"v0.8 alpha.2.1 App placement integration missing: {token}")

    for token in (
        "RememberLauncherPosition",
        "WM_EXITSIZEMOVE",
    ):
        if token not in launcher_cpp:
            fail(f"v0.8 alpha.2.1 Launcher placement behavior missing: {token}")

    ui_test = read("tests/UiFoundationTests.cpp")
    for token in (
        "kSettingsSidebarWidthLogical == 208",
        "kSettingsToggleRowLogical == 50",
        "kSettingsComboRowLogical == 54",
    ):
        if token not in ui_test:
            fail(f"v0.8 alpha.2.1 UI regression coverage missing: {token}")

    desktop_test = read("tests/DesktopValidationTests.cpp")
    for token in (
        "kPageDividerTopLogical == 84",
        "kSectionTitleTopLogical == 108",
        "kFirstCardTopLogical == 140",
        "scale(680)",
    ):
        if token not in desktop_test:
            fail(f"v0.8 alpha.2.1 desktop layout coverage missing: {token}")

    update_tests = read("tests/UpdatePolicyTests.cpp")
    for token in (
        '"0.8.0-alpha.2"',
        '"0.8.0-alpha.2.1"',
        '"0.8.0-alpha.3"',
        "UpdateChannel::Development",
    ):
        if token not in update_tests:
            fail(f"v0.8 alpha.2.1 update ordering/default coverage missing: {token}")

    readme = read("README.md")
    changelog = read("CHANGELOG.md")
    roadmap = read("ROADMAP.md")

    for token in (
        "## v0.8.0-alpha.2.1 — Settings Rendering & Information Hierarchy Stabilization",
        "0.8.0.21",
    ):
        if token not in readme:
            fail(f"v0.8 alpha.2.1 README contract missing: {token}")

    for token in (
        "## 0.8.0-alpha.2.1",
        "0.8.0.21",
    ):
        if token not in changelog:
            fail(f"v0.8 alpha.2.1 changelog contract missing: {token}")

    for token in (
        "v0.8.0-alpha.2.1",
        "v0.8.0-alpha.3",
    ):
        if token not in roadmap:
            fail(f"v0.8 alpha.2.1 roadmap contract missing: {token}")

    print(
        "v0.8.0-alpha.2.1 Settings stabilization contract verified:",
        "| settings=8 commands=2 usage=1 provider-cache=2",
        "| opaque native static backgrounds",
        "| atomic page redraw",
        "| compact information hierarchy",
        "| two-line ALTRun / Next sidebar brand",
        "| frozen v0.7 behavior preserved",
    )
    raise SystemExit(0)

if version == "0.8.0-alpha.2":
    expected_schemas = {
        "kSettingsSchemaVersion": 8,
        "kCommandsSchemaVersion": 2,
        "kUsageSchemaVersion": 1,
    }
    for name, expected in expected_schemas.items():
        actual = cpp_int("src/core/ConfigIO.hpp", name)
        if actual != expected:
            fail(f"v0.8 alpha.2 {name}={actual}, expected {expected}")

    if cpp_int("src/core/ProviderCache.cpp", "kProviderCacheSchemaVersion") != 2:
        fail("v0.8 alpha.2 must keep provider-cache schemaVersion 2")

    settings = json.loads(read("config/settings.example.json"))
    if settings.get("schemaVersion") != 8:
        fail("v0.8 alpha.2 settings example must use schemaVersion 8")
    if settings.get("update") != {"autoCheck": True, "channel": "development"}:
        fail("v0.8 alpha.2 prerelease must default to Development updates")

    expected_providers = {
        "windows.startmenu": True,
        "windows.packaged": True,
        "windows.apppaths": True,
        "windows.path": True,
        "everything.filesystem": False,
    }
    if settings.get("providers") != expected_providers:
        fail("v0.8 alpha.2 changed frozen provider defaults")

    expected_hotkeys = {
        "launcher.activate",
        "launcher.activateSecondary",
        "launcher.openSettings",
        "result.navigateCurrentFileManager",
        "result.copySelectedTarget",
    }
    bindings = settings.get("hotkeys", {}).get("bindings", {})
    if set(bindings) != expected_hotkeys:
        fail("v0.8 alpha.2 changed frozen Hotkey Registry action IDs")

    expected_placement = {
        "launcherMode": "top",
        "settingsMode": "center",
        "launcherLastValid": False,
        "launcherLastX": 0,
        "launcherLastY": 0,
        "settingsLastValid": False,
        "settingsLastX": 0,
        "settingsLastY": 0,
    }
    if settings.get("windowPlacement") != expected_placement:
        fail("v0.8 alpha.2 windowPlacement defaults do not preserve prior behavior")

    metrics = read("src/ui/UiMetrics.hpp")
    for token in (
        "kClassicLauncherMetrics",
        "420",
        "16",
        "10",
        "kModernCompactLauncherMetrics",
        "620",
        "32",
        "9",
        "kSettingsSidebarWidthLogical = 208",
        "kSettingsToggleRowLogical = 62",
        "kSettingsComboRowLogical = 68",
        "kSettingsCardRadiusLogical = 8",
        "kSettingsNavHeightLogical = 40",
        "kSettingsNavGapLogical = 4",
    ):
        if token not in metrics:
            fail(f"v0.8 alpha.2 Settings metric contract missing: {token}")

    settings_h = read("src/ui/SettingsWindow.hpp")
    settings_cpp = read("src/ui/SettingsWindow.cpp")
    launcher_cpp = read("src/ui/LauncherWindow.cpp")
    settings_store = read("src/core/Settings.cpp") + read("src/core/Settings.hpp")
    app = read("src/app/App.cpp") + read("src/app/App.hpp")

    legacy_hotkey_tokens = (
        "legacyHotkeyControls_",
        "hotkeyCtrl_",
        "hotkeyApply_",
        "auxiliaryHotkeyCtrl_",
        "ApplyAuxiliaryHotkeyControl",
        "ApplyHotkeyControl",
        "RefreshHotkeyControls",
        "hotkeySectionTitle_",
        "primaryHotkeyLabel_",
    )
    for token in legacy_hotkey_tokens:
        if token in settings_h or token in settings_cpp:
            fail(f"v0.8 alpha.2 resurrected legacy General hotkey UI: {token}")

    for token in (
        "brandName_",
        "brandSubtitle_",
        "kIdLauncherPlacement",
        "kIdSettingsPlacement",
        "placementSectionTitle_",
        "launcherPlacement_",
        "settingsPlacement_",
        "DrawActionButton",
        "DrawHotkeyActionItem",
        "LBS_OWNERDRAWFIXED",
        "providerFilesTitle_",
        "appearanceLauncherTitle_",
        "appearanceAppTitle_",
        "aboutProjectTitle_",
    ):
        if token not in settings_h + settings_cpp:
            fail(f"v0.8 alpha.2 Settings redesign wiring missing: {token}")

    create_general_start = settings_cpp.find("void SettingsWindow::CreateGeneralPage")
    create_general_end = settings_cpp.find("void SettingsWindow::CreateHotkeyPage", create_general_start)
    create_general = settings_cpp[create_general_start:create_general_end]
    if "showResultIcons_" not in create_general:
        fail("v0.8 alpha.2 result icons must live in General / Launcher behavior")

    create_appearance_start = settings_cpp.find("void SettingsWindow::CreateAppearancePage")
    create_appearance_end = settings_cpp.find("void SettingsWindow::CreateProviderPage", create_appearance_start)
    create_appearance = settings_cpp[create_appearance_start:create_appearance_end]
    if "showResultIcons_" in create_appearance or "resultIconsNote_" in create_appearance:
        fail("v0.8 alpha.2 Appearance must not own result-icon controls")

    layout_start = settings_cpp.find("void SettingsWindow::Layout")
    layout_end = settings_cpp.find("RECT SettingsWindow::ProviderCardRect", layout_start)
    layout = settings_cpp[layout_start:layout_end]
    required_nav_order = (
        "navGeneral_",
        "navHotkeys_",
        "navProviders_",
        "navAppearance_",
        "navData_",
        "navDiagnostics_",
    )
    nav_positions = [layout.find(token) for token in required_nav_order]
    if any(position < 0 for position in nav_positions) or nav_positions != sorted(nav_positions):
        fail("v0.8 alpha.2 sidebar order must be General -> Hotkeys -> Search sources -> Appearance -> Data -> Diagnostics")

    for token in (
        'launcherPlacement{"top"}',
        'settingsPlacement{"center"}',
        "launcherLastPositionValid",
        "settingsLastPositionValid",
        '"windowPlacement"',
        "SetWindowPlacement(",
        "RememberLauncherPosition(",
        "RememberSettingsPosition(",
    ):
        if token not in settings_store:
            fail(f"v0.8 alpha.2 placement persistence contract missing: {token}")

    for token in (
        "SetWindowPlacementSettings",
        "RememberLauncherPosition",
        "RememberSettingsPosition",
    ):
        if token not in app:
            fail(f"v0.8 alpha.2 App placement integration missing: {token}")

    for token in (
        'settings.launcherPlacement == "last"',
        'settings.launcherPlacement ==\n                "center"',
        "MonitorFromRect",
        "RememberLauncherPosition",
        "WM_EXITSIZEMOVE",
    ):
        if token not in launcher_cpp:
            fail(f"v0.8 alpha.2 Launcher placement behavior missing: {token}")

    for token in (
        "PositionForShow",
        "settings.settingsPlacement ==",
        '"last"',
        "ClampRectToWorkArea",
        "RememberSettingsPosition",
        "WM_EXITSIZEMOVE",
    ):
        if token not in settings_cpp:
            fail(f"v0.8 alpha.2 Settings placement behavior missing: {token}")

    ui_test = read("tests/UiFoundationTests.cpp")
    for token in (
        "kSettingsSidebarWidthLogical == 208",
        "kSettingsToggleRowLogical == 62",
        "kSettingsComboRowLogical == 68",
        "kSettingsCardRadiusLogical == 8",
    ):
        if token not in ui_test:
            fail(f"v0.8 alpha.2 UI metric regression coverage missing: {token}")

    upgrade = read("tests/UpgradeMatrixTests.cpp")
    for token in (
        "AssertSchema7Migration",
        "schema7-to-schema8",
        'launcherPlacement ==\n        "top"',
        'settingsPlacement ==\n        "center"',
        '"windowPlacement"',
    ):
        if token not in upgrade:
            fail(f"v0.8 alpha.2 schema 7 -> 8 migration coverage missing: {token}")

    runtime_smoke = read("scripts/verify_runtime_smoke.ps1")
    for token in (
        "schemaVersion -ne 8",
        "schema 2 -> 8",
        "windowPlacement.launcherMode",
        "windowPlacement.settingsMode",
    ):
        if token not in runtime_smoke:
            fail(f"v0.8 alpha.2 packaged runtime migration gate missing: {token}")

    update_tests = read("tests/UpdatePolicyTests.cpp")
    for token in (
        '"0.8.0-alpha.1"',
        '"0.8.0-alpha.2"',
        "UpdateChannel::Development",
    ):
        if token not in update_tests:
            fail(f"v0.8 alpha.2 update ordering/default coverage missing: {token}")

    readme = read("README.md")
    changelog = read("CHANGELOG.md")
    roadmap = read("ROADMAP.md")
    for token in (
        "## v0.8.0-alpha.2 — Settings UX Redesign",
        "schemaVersion advances from 7 to 8",
        "0.8.0.2",
    ):
        if token not in readme:
            fail(f"v0.8 alpha.2 README contract missing: {token}")

    for token in (
        "## 0.8.0-alpha.2",
        "schemaVersion from 7 to 8",
        "0.8.0.2",
    ):
        if token not in changelog:
            fail(f"v0.8 alpha.2 changelog contract missing: {token}")

    for token in (
        "v0.8.0-alpha.2",
        "v0.8.0-alpha.3",
        "settings schemaVersion to 8",
    ):
        if token not in roadmap:
            fail(f"v0.8 alpha.2 roadmap contract missing: {token}")

    print(
        "v0.8.0-alpha.2 Settings UX contract verified:",
        "| settings=8 commands=2 usage=1 provider-cache=2",
        "| nav=General/Hotkeys/Search sources/Appearance/Data/Diagnostics/About",
        "| result icons moved to General",
        "| Launcher monitor separated from top/center/last placement",
        "| Settings center/last placement with remembered positions",
        "| legacy General hotkey UI removed",
    )
    raise SystemExit(0)

if version == "0.8.0-alpha.1":
    expected_schemas = {
        "kSettingsSchemaVersion": 7,
        "kCommandsSchemaVersion": 2,
        "kUsageSchemaVersion": 1,
    }
    for name, expected in expected_schemas.items():
        actual = cpp_int("src/core/ConfigIO.hpp", name)
        if actual != expected:
            fail(f"v0.8 alpha.1 changed frozen {name}: {actual} != {expected}")

    if cpp_int("src/core/ProviderCache.cpp", "kProviderCacheSchemaVersion") != 2:
        fail("v0.8 alpha.1 must keep provider-cache schemaVersion 2")

    settings = json.loads(read("config/settings.example.json"))
    if settings.get("schemaVersion") != 7:
        fail("v0.8 alpha.1 settings example must remain schemaVersion 7")
    if settings.get("update") != {"autoCheck": True, "channel": "development"}:
        fail("v0.8 alpha.1 prerelease must default the example update channel to Development")

    expected_providers = {
        "windows.startmenu": True,
        "windows.packaged": True,
        "windows.apppaths": True,
        "windows.path": True,
        "everything.filesystem": False,
    }
    if settings.get("providers") != expected_providers:
        fail("v0.8 alpha.1 changed frozen provider defaults")

    expected_hotkeys = {
        "launcher.activate",
        "launcher.activateSecondary",
        "launcher.openSettings",
        "result.navigateCurrentFileManager",
        "result.copySelectedTarget",
    }
    bindings = settings.get("hotkeys", {}).get("bindings", {})
    if set(bindings) != expected_hotkeys:
        fail("v0.8 alpha.1 changed frozen Hotkey Registry IDs")

    metrics = read("src/ui/UiMetrics.hpp")
    for token in (
        "kClassicLauncherMetrics",
        "420",
        "16",
        "10",
        "kModernCompactLauncherMetrics",
        "620",
        "32",
        "9",
        "kSettingsSidebarWidthLogical = 190",
        "kSettingsContentLeftInsetLogical = 38",
        "kSettingsContentRightInsetLogical = 34",
        "kSettingsToggleRowLogical = 54",
        "constexpr int Scale(",
    ):
        if token not in metrics:
            fail(f"v0.8 alpha.1 shared UI metrics contract missing: {token}")

    theme = read("src/ui/UiTheme.hpp")
    for token in (
        "struct UiPalette",
        "kApplicationPalette",
        "kClassicLauncherPalette",
        "kModernCompactLauncherPalette",
        "LauncherPalette(",
    ):
        if token not in theme:
            fail(f"v0.8 alpha.1 shared theme contract missing: {token}")

    typography_h = read("src/ui/UiTypography.hpp")
    typography_cpp = read("src/ui/UiTypography.cpp")
    for token in (
        "enum class UiFontRole",
        "ApplicationFontSpec",
        "LauncherFontSpec",
        "CreateFontHandle",
    ):
        if token not in typography_h + typography_cpp:
            fail(f"v0.8 alpha.1 shared typography contract missing: {token}")

    migrated_ui = {
        "Launcher": read("src/ui/LauncherWindow.cpp"),
        "Settings": read("src/ui/SettingsWindow.cpp"),
        "Shortcut Manager": read("src/ui/ShortcutManagerWindow.cpp"),
        "Shortcut Editor": read("src/ui/ShortcutEditorDialog.cpp"),
        "Path Conversion": read("src/ui/ShortcutPathConverterDialog.cpp"),
    }
    for name, source in migrated_ui.items():
        if "ui::" not in source:
            fail(f"v0.8 alpha.1 {name} is not wired to the shared UI foundation")
        if "CreateFontW(" in source:
            fail(f"v0.8 alpha.1 {name} still creates its own font directly")

    launcher_h = read("src/ui/LauncherWindow.hpp")
    launcher_cpp = migrated_ui["Launcher"]
    for token in (
        "ui::kClassicLauncherMetrics",
        "ui::kModernCompactLauncherMetrics",
        "ui::LauncherPalette",
        "ui::LauncherFontSpec",
    ):
        if token not in launcher_h + launcher_cpp:
            fail(f"v0.8 alpha.1 launcher foundation wiring missing: {token}")

    settings_h = read("src/ui/SettingsWindow.hpp")
    settings_cpp = migrated_ui["Settings"]
    legacy_settings_tokens = (
        "Page::Commands",
        "CreateCommandPage",
        "kIdNavCommands",
        "kIdCommandSearch",
        "commandControls_",
        "RefreshCommandList",
        "LoadCommandEditor",
        "editingCommandId_",
        "filteredCommandIds_",
        "搜索快捷项...",
        "Search shortcuts...",
        "快捷项详情",
        "Shortcut details",
        "主快捷词 *",
        "Primary keyword *",
        "取消更改",
        "Discard changes",
        "oldType",
    )
    for token in legacy_settings_tokens:
        if token in settings_h or token in settings_cpp:
            fail(f"v0.8 alpha.1 resurrected legacy Settings Command UI: {token}")

    for token in (
        "RefreshProviderStatus",
        "RefreshActionDiagnostics",
        "AcquireEverything",
        "RecheckEverything",
        "RefreshDataCompatibilityStatus",
        "RefreshUpdateStatus",
        "ApplyUpdateSettings",
    ):
        if token not in settings_h or token not in settings_cpp:
            fail(f"v0.8 alpha.1 lost active Settings helper during legacy cleanup: {token}")

    show_page_start = settings_cpp.find("void SettingsWindow::ShowPage")
    show_page_end = settings_cpp.find("void SettingsWindow::RefreshHotkeyControls", show_page_start)
    show_page = settings_cpp[show_page_start:show_page_end]
    if show_page_start < 0 or show_page_end < 0:
        fail("v0.8 alpha.1 Settings ShowPage block is missing")
    if "if (\n        page == Page::Hotkeys)" not in show_page:
        fail("v0.8 alpha.1 Settings ShowPage lost Hotkeys first refresh branch")
    if "\nelse if (\n        page == Page::Hotkeys)" in show_page:
        fail("v0.8 alpha.1 Settings ShowPage retained orphan Hotkeys else-if")

    app_cpp = read("src/app/App.cpp")
    if "->RefreshCommands()" in app_cpp:
        fail("v0.8 alpha.1 kept the obsolete Settings shortcut refresh hook")

    cmake = read("CMakeLists.txt")
    for token in (
        "src/ui/UiTypography.cpp",
        "ui_foundation_tests",
        "tests/UiFoundationTests.cpp",
    ):
        if token not in cmake:
            fail(f"v0.8 alpha.1 build/test wiring missing: {token}")

    ui_test = read("tests/UiFoundationTests.cpp")
    for token in (
        "kClassicLauncherMetrics.widthLogical == 420",
        "kClassicLauncherMetrics.rowHeightLogical == 16",
        "kClassicLauncherMetrics.maxResults == 10",
        "kModernCompactLauncherMetrics.widthLogical == 620",
        "kModernCompactLauncherMetrics.rowHeightLogical == 32",
        "kModernCompactLauncherMetrics.maxResults == 9",
        "kSettingsSidebarWidthLogical == 190",
        "kSettingsToggleRowLogical == 54",
    ):
        if token not in ui_test:
            fail(f"v0.8 alpha.1 UI regression coverage missing: {token}")

    update_tests = read("tests/UpdatePolicyTests.cpp")
    for token in (
        '"0.7.0"',
        '"0.8.0-alpha.1"',
        "UpdateChannel::Development",
    ):
        if token not in update_tests:
            fail(f"v0.8 alpha.1 update ordering/default coverage missing: {token}")

    readme = read("README.md")
    changelog = read("CHANGELOG.md")
    roadmap = read("ROADMAP.md")
    for token in (
        "## v0.8.0-alpha.1 — Unified UI Foundation & Legacy Cleanup",
        "UiTheme",
        "UiMetrics",
        "UiTypography",
        "0.8.0.1",
    ):
        if token not in readme:
            fail(f"v0.8 alpha.1 README contract missing: {token}")

    for token in (
        "## 0.8.0-alpha.1",
        "ui_foundation_tests",
        "0.8.0.1",
    ):
        if token not in changelog:
            fail(f"v0.8 alpha.1 changelog contract missing: {token}")

    for token in (
        "## v0.8.x - UI / UX refinement & product polish",
        "## v0.9.x - Distribution & extensibility",
        "v0.8.0-alpha.1",
    ):
        if token not in roadmap:
            fail(f"v0.8 alpha.1 roadmap contract missing: {token}")

    print(
        "v0.8.0-alpha.1 UI foundation contract verified:",
        "| settings=7 commands=2 usage=1 provider-cache=2",
        "| shared Theme/Metrics/Typography/DPI",
        "| Classic=420/16/10 Modern=620/32/9",
        "| legacy Settings Command UI removed",
        "| no runtime feature/schema change",
    )
    raise SystemExit(0)

if version in ("0.7.0-alpha.2", "0.7.0-alpha.2.1", "0.7.0-alpha.2.2", "0.7.0-alpha.2.3", "0.7.0-alpha.2.4", "0.7.0-alpha.2.5", "0.7.0-alpha.2.6", "0.7.0-alpha.3", "0.7.0-alpha.3.1", "0.7.0-alpha.4", "0.7.0-alpha.5", "0.7.0-alpha.5.1", "0.7.0-alpha.5.2", "0.7.0-alpha.6", "0.7.0-alpha.7", "0.7.0-alpha.8", "0.7.0-alpha.8.1", "0.7.0-alpha.8.2", "0.7.0-alpha.8.3", "0.7.0-alpha.8.4", "0.7.0-alpha.9", "0.7.0-alpha.9.1", "0.7.0-alpha.9.2", "0.7.0-alpha.9.3", "0.7.0-alpha.9.4", "0.7.0-beta.1", "0.7.0-beta.2", "0.7.0-beta.3", "0.7.0-beta.4", "0.7.0-beta.5", "0.7.0-beta.6", "0.7.0-beta.7", "0.7.0-beta.8", "0.7.0-beta.9", "0.7.0-beta.10", "0.7.0-beta.11", "0.7.0-beta.12", "0.7.0-rc.1", "0.7.0"):
    expected_settings_schema = 7 if version in ("0.7.0-alpha.9", "0.7.0-alpha.9.1", "0.7.0-alpha.9.2", "0.7.0-alpha.9.3", "0.7.0-alpha.9.4", "0.7.0-beta.1", "0.7.0-beta.2", "0.7.0-beta.3", "0.7.0-beta.4", "0.7.0-beta.5", "0.7.0-beta.6", "0.7.0-beta.7", "0.7.0-beta.8", "0.7.0-beta.9", "0.7.0-beta.10", "0.7.0-beta.11", "0.7.0-beta.12", "0.7.0-rc.1", "0.7.0") else (6 if version in ("0.7.0-alpha.5.1", "0.7.0-alpha.5.2", "0.7.0-alpha.6", "0.7.0-alpha.7", "0.7.0-alpha.8", "0.7.0-alpha.8.1", "0.7.0-alpha.8.2", "0.7.0-alpha.8.3", "0.7.0-alpha.8.4") else (5 if version in ("0.7.0-alpha.2.5", "0.7.0-alpha.2.6", "0.7.0-alpha.3", "0.7.0-alpha.3.1", "0.7.0-alpha.4", "0.7.0-alpha.5") else 4))
    expected_commands_schema = 2 if version in ("0.7.0-alpha.4", "0.7.0-alpha.5", "0.7.0-alpha.5.1", "0.7.0-alpha.5.2", "0.7.0-alpha.6", "0.7.0-alpha.7", "0.7.0-alpha.8", "0.7.0-alpha.8.1", "0.7.0-alpha.8.2", "0.7.0-alpha.8.3", "0.7.0-alpha.8.4", "0.7.0-alpha.9", "0.7.0-alpha.9.1", "0.7.0-alpha.9.2", "0.7.0-alpha.9.3", "0.7.0-alpha.9.4", "0.7.0-beta.1", "0.7.0-beta.2", "0.7.0-beta.3", "0.7.0-beta.4", "0.7.0-beta.5", "0.7.0-beta.6", "0.7.0-beta.7", "0.7.0-beta.8", "0.7.0-beta.9", "0.7.0-beta.10", "0.7.0-beta.11", "0.7.0-beta.12", "0.7.0-rc.1", "0.7.0") else 1
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

    if version in ("0.7.0-alpha.6", "0.7.0-alpha.7", "0.7.0-alpha.8", "0.7.0-alpha.8.1", "0.7.0-alpha.8.2", "0.7.0-alpha.8.3", "0.7.0-alpha.8.4", "0.7.0-alpha.9", "0.7.0-alpha.9.1", "0.7.0-alpha.9.2", "0.7.0-alpha.9.3", "0.7.0-alpha.9.4", "0.7.0-beta.1", "0.7.0-beta.2", "0.7.0-beta.3", "0.7.0-beta.4", "0.7.0-beta.5", "0.7.0-beta.6", "0.7.0-beta.7", "0.7.0-beta.8", "0.7.0-beta.9", "0.7.0-beta.10", "0.7.0-beta.11", "0.7.0-beta.12", "0.7.0-rc.1", "0.7.0"):
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

    if version in ("0.7.0-alpha.7", "0.7.0-alpha.8", "0.7.0-alpha.8.1", "0.7.0-alpha.8.2", "0.7.0-alpha.8.3", "0.7.0-alpha.8.4", "0.7.0-alpha.9", "0.7.0-alpha.9.1", "0.7.0-alpha.9.2", "0.7.0-alpha.9.3", "0.7.0-alpha.9.4", "0.7.0-beta.1", "0.7.0-beta.2", "0.7.0-beta.3", "0.7.0-beta.4", "0.7.0-beta.5", "0.7.0-beta.6", "0.7.0-beta.7", "0.7.0-beta.8", "0.7.0-beta.9", "0.7.0-beta.10", "0.7.0-beta.11", "0.7.0-beta.12", "0.7.0-rc.1", "0.7.0"):
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

    if version in ("0.7.0-alpha.8", "0.7.0-alpha.8.1", "0.7.0-alpha.8.2", "0.7.0-alpha.8.3", "0.7.0-alpha.8.4", "0.7.0-alpha.9", "0.7.0-alpha.9.1", "0.7.0-alpha.9.2", "0.7.0-alpha.9.3", "0.7.0-alpha.9.4", "0.7.0-beta.1", "0.7.0-beta.2", "0.7.0-beta.3", "0.7.0-beta.4", "0.7.0-beta.5", "0.7.0-beta.6", "0.7.0-beta.7", "0.7.0-beta.8", "0.7.0-beta.9", "0.7.0-beta.10", "0.7.0-beta.11", "0.7.0-beta.12", "0.7.0-rc.1", "0.7.0"):
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

        bootstrap_tokens = [
            "FindExistingCandidates",
            "App Paths\\\\Everything.exe",
            'L"ProgramFiles"',
            "SearchPathW",
            "WinHttpOpen",
            "WinHttpCrackUrl",
            "BCryptOpenAlgorithmProvider",
            "BCRYPT_SHA256_ALGORITHM",
            "CopyHere",
            "-startup -first-instance",
            "WaitForIpc",
            "allowDownload",
            "NeedsInstall",
        ]
        if version == "0.7.0-alpha.8":
            bootstrap_tokens.append('L".download"')

        for token in bootstrap_tokens:
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

        if version in ("0.7.0-alpha.8.1", "0.7.0-alpha.8.2", "0.7.0-alpha.8.3", "0.7.0-alpha.8.4", "0.7.0-alpha.9", "0.7.0-alpha.9.1", "0.7.0-alpha.9.2", "0.7.0-alpha.9.3", "0.7.0-alpha.9.4", "0.7.0-beta.1", "0.7.0-beta.2", "0.7.0-beta.3", "0.7.0-beta.4", "0.7.0-beta.5", "0.7.0-beta.6", "0.7.0-beta.7", "0.7.0-beta.8", "0.7.0-beta.9", "0.7.0-beta.10", "0.7.0-beta.11", "0.7.0-beta.12", "0.7.0-rc.1", "0.7.0"):
            for token in (
                "EverythingArchiveNames",
                "ManagedEverythingArchiveNames",
                "downloadFileName",
                "verifiedZipFileName",
            ):
                if token not in bootstrap_policy:
                    fail(f"v0.7 alpha.8.1 archive staging policy missing: {token}")

            for token in (
                "downloadArchive",
                "verifiedArchive",
                "std::filesystem::rename(",
                "PackageStagingFailed",
                "ExtractZipWithShell(\n            verifiedArchive",
            ):
                if token not in bootstrapper:
                    fail(f"v0.7 alpha.8.1 verified ZIP staging missing: {token}")

            for token in (
                "Everything-1.4.1.1032.x64.zip.download",
                "verifiedZipFileName.ends_with(\n                L\".zip\")",
                "downloadFileName.ends_with(\n                L\".download\")",
            ):
                if token not in bootstrap_test:
                    fail(f"v0.7 alpha.8.1 staging regression coverage missing: {token}")

            if "ExtractZipWithShell(\n            downloadArchive" in bootstrapper:
                fail("v0.7 alpha.8.1 must never pass the .download path to Windows Shell extraction")

        if version in ("0.7.0-alpha.8.2", "0.7.0-alpha.8.3", "0.7.0-alpha.8.4", "0.7.0-alpha.9", "0.7.0-alpha.9.1", "0.7.0-alpha.9.2", "0.7.0-alpha.9.3", "0.7.0-alpha.9.4", "0.7.0-beta.1", "0.7.0-beta.2", "0.7.0-beta.3", "0.7.0-beta.4", "0.7.0-beta.5", "0.7.0-beta.6", "0.7.0-beta.7", "0.7.0-beta.8", "0.7.0-beta.9", "0.7.0-beta.10", "0.7.0-beta.11", "0.7.0-beta.12", "0.7.0-rc.1", "0.7.0"):
            for token in (
                "ApplyManagedEverythingIniPolicy",
                '"app_data", "0"',
                '"run_as_admin", "0"',
                '"run_in_background", "1"',
                '"show_tray_icon", "0"',
                '"check_for_updates_on_startup", "0"',
                '"ipc", "1"',
            ):
                if token not in bootstrap_policy:
                    fail(f"v0.7 alpha.8.2 managed Everything INI policy missing: {token}")

            for token in (
                "OpenSCManagerW",
                "OpenServiceW",
                'L"Everything"',
                "SERVICE_RUNNING",
                "SERVICE_START_PENDING",
                "ShellExecuteExW",
                'L"runas"',
                'L"-install-service"',
                'L"-start-service"',
                'L"-exit"',
                "ManagedDefaultIpcRunning",
                "ConfiguringManaged",
                "InstallingService",
                "WaitingForService",
                "ServiceRequired",
                "ServiceElevationCancelled",
            ):
                if token not in bootstrapper:
                    fail(f"v0.7 alpha.8.2 managed service/runtime behavior missing: {token}")

            for token in (
                "show_tray_icon=0",
                "run_as_admin=0",
                "run_in_background=1",
                "check_for_updates_on_startup=0",
                "ipc=1",
                "ApplyManagedEverythingIniPolicy",
            ):
                if token not in bootstrap_test:
                    fail(f"v0.7 alpha.8.2 managed INI regression coverage missing: {token}")

            for token in (
                "Everything Service",
                "UAC",
                "隐藏托盘图标",
                "ServiceRequired",
                "InstallingService",
                "WaitingForService",
            ):
                if token not in settings_ui:
                    fail(f"v0.7 alpha.8.2 Everything service UX missing: {token}")

        if version in ("0.7.0-alpha.8.3", "0.7.0-alpha.8.4", "0.7.0-alpha.9", "0.7.0-alpha.9.1", "0.7.0-alpha.9.2", "0.7.0-alpha.9.3", "0.7.0-alpha.9.4", "0.7.0-beta.1", "0.7.0-beta.2", "0.7.0-beta.3", "0.7.0-beta.4", "0.7.0-beta.5", "0.7.0-beta.6", "0.7.0-beta.7", "0.7.0-beta.8", "0.7.0-beta.9", "0.7.0-beta.10", "0.7.0-beta.11", "0.7.0-beta.12", "0.7.0-rc.1", "0.7.0"):
            lifecycle_test = read("tests/EverythingLifecycleTests.cpp")

            for token in (
                "ManagedEverythingStopStatus",
                "ManagedEverythingStopResult",
                "StopManagedEverything",
                "ManagedDefaultIpcRunning",
                'L"-exit"',
                "WaitForManagedDefaultIpcToExit",
            ):
                if token not in bootstrapper:
                    fail(f"v0.7 alpha.8.3 managed shutdown runtime missing: {token}")

            for forbidden in (
                "ControlServiceW",
                "DeleteService",
            ):
                if forbidden in bootstrapper:
                    fail(f"v0.7 alpha.8.3 must not stop/delete the Everything service: {forbidden}")

            for token in (
                "StopManagedEverythingLifecycle",
                "++everythingBootstrapGeneration_",
                "request_stop()",
                "everythingProvider_.reset()",
                "win::StopManagedEverything(",
                "everythingBootstrapStatus_ = {}",
            ):
                if token not in app:
                    fail(f"v0.7 alpha.8.3 App lifecycle integration missing: {token}")

            if app.count("StopManagedEverythingLifecycle();") < 2:
                fail("v0.7 alpha.8.3 must stop the managed client on both App exit and provider disable")

            for token in (
                "ManagedEverythingStopStatus::",
                "NotInstalled",
                "NotRunning",
                "ERROR_CLASS_ALREADY_EXISTS",
                "not-an-executable",
                "must not issue -exit",
            ):
                if token not in lifecycle_test:
                    fail(f"v0.7 alpha.8.3 lifecycle regression coverage missing: {token}")

            for token in (
                "everything_lifecycle_tests",
                "tests/EverythingLifecycleTests.cpp",
                "src/platform/EverythingBootstrapper.cpp",
            ):
                if token not in cmake:
                    fail(f"v0.7 alpha.8.3 lifecycle build wiring missing: {token}")

            if workflow.count("everything_lifecycle_tests") < 4:
                fail("v0.7 alpha.8.3 Windows smoke/baseline must build and run lifecycle tests")

            for token in (
                "Everything Service 保留运行",
                "stops only the managed client",
            ):
                if token not in settings_ui and token not in read("README.md"):
                    fail(f"v0.7 alpha.8.3 lifecycle UX/documentation missing: {token}")

        if version in ("0.7.0-alpha.8.4", "0.7.0-alpha.9"):
            main_cpp = read("src/main.cpp")

            for token in (
                "ExtractEverythingServiceExecutable",
                "QueryServiceConfigW",
                "SERVICE_QUERY_CONFIG",
                "serviceExecutableExists",
                "servicePathStale",
                "ERROR_FILE_NOT_FOUND",
                "RunElevatedServiceRepairHelper",
                "--repair-managed-everything-service",
                "RepairManagedEverythingServicePath",
                "ChangeServiceConfigW",
                "SERVICE_AUTO_START",
                "StartServiceW",
                "RepairingService",
                "ServiceRepairRequired",
                "ServiceRepairFailed",
            ):
                if token not in bootstrapper:
                    fail(f"v0.7 alpha.8.4 stale service path repair missing: {token}")

            for token in (
                "--repair-managed-everything-service",
                "RepairManagedEverythingServicePath",
                "ExecutableDirectory()",
            ):
                if token not in main_cpp:
                    fail(f"v0.7 alpha.8.4 elevated maintenance entrypoint missing: {token}")

            for token in (
                "ExtractEverythingServiceExecutable",
                'LR"("D:\\ALTRun Test\\data\\tools\\Everything\\Everything.exe" -svc)"',
                "D:\\\\Portable Apps\\\\Everything.exe -svc",
                "EVERYTHING.EXE -svc",
            ):
                if token not in bootstrap_test:
                    fail(f"v0.7 alpha.8.4 service ImagePath parser coverage missing: {token}")

            for token in (
                "检测到失效的 Everything Service 路径",
                "修复失效的 Everything Service 路径",
                "ServiceRepairRequired",
                "ServiceRepairFailed",
            ):
                if token not in settings_ui:
                    fail(f"v0.7 alpha.8.4 stale service repair UX missing: {token}")

            if 'servicePathStale\n                ? L"-start-service"' in bootstrapper:
                fail("v0.7 alpha.8.4 must not blindly start a stale service path")

        if version == "0.7.0-alpha.9.1":
            main_cpp = read("src/main.cpp")
            lifecycle_test = read("tests/EverythingLifecycleTests.cpp")

            for token in (
                "ManagedEverythingServiceExecutable",
                'L"ProgramFiles"',
                'L"Aspeternity"',
                'L"EverythingService"',
                "LegacyManagedServiceExecutable",
                "ManagedServiceHostExecutable",
                "servicePathNeedsMigration",
                "servicePathNeedsRepair",
                "WaitForEverythingServiceStopped",
                "SERVICE_STOP",
                "SERVICE_CONTROL_STOP",
                "CopyFileW",
                "MoveFileExW",
                "ChangeServiceConfigW",
                "RunElevatedServiceRepairHelper",
                "--repair-managed-everything-service",
            ):
                if token not in bootstrapper and token not in main_cpp:
                    fail(f"v0.7 alpha.9.1 detached Everything service-host contract missing: {token}")

            for token in (
                "ManagedEverythingServiceExecutable",
                "serviceHost.lexically_normal() !=",
                "root.wstring()",
            ):
                if token not in lifecycle_test:
                    fail(f"v0.7 alpha.9.1 service-host lifecycle regression missing: {token}")

            for token in (
                "检测到需要迁移或修复的 Everything Service",
                "迁移 / 修复 Everything Service",
                "Program Files",
                "外部 Everything 不会被改写",
            ):
                if token not in settings_ui:
                    fail(f"v0.7 alpha.9.1 service migration UX missing: {token}")

            readme = read("README.md")
            for token in (
                "%ProgramFiles%\\Aspeternity\\ALTRunNext\\EverythingService",
                "old release directories can be moved or deleted normally",
                "updater target",
            ):
                if token not in readme:
                    fail(f"v0.7 alpha.9.1 service detachment documentation missing: {token}")

        if version == "0.7.0-beta.2":
            editor_cpp = read("src/ui/ShortcutEditorDialog.cpp")
            editor_h = read("src/ui/ShortcutEditorDialog.hpp")
            beta_validation = read("docs/V0.7_BETA_VALIDATION.md")
            readme = read("README.md")
            changelog = read("CHANGELOG.md")

            for token in (
                'T(L"选择...",',
                'L"Browse..."',
                "Scale(66)",
                "RefreshDynamicLayout",
                "SWP_NOREDRAW",
                "RDW_INVALIDATE",
                "RDW_ERASE",
                "RDW_ALLCHILDREN",
            ):
                if token not in editor_cpp:
                    fail(f"v0.7 beta.2 Shortcut Editor dynamic-layout fix missing: {token}")

            if "void RefreshDynamicLayout();" not in editor_h:
                fail("v0.7 beta.2 Shortcut Editor header missing RefreshDynamicLayout")

            for token in (
                "Working Directory browse button label",
                "no stale horizontal line",
                "never overlap, duplicate or leave paint trails",
                "Repeated Advanced expand/collapse",
            ):
                if token not in beta_validation:
                    fail(f"v0.7 beta.2 desktop regression checklist missing: {token}")

            for token in (
                "Shortcut Editor Dynamic Layout Fixes",
                "0.7.0.101",
                "full erase-and-redraw",
            ):
                if token not in readme:
                    fail(f"v0.7 beta.2 README missing: {token}")

            for token in (
                "0.7.0-beta.2",
                "Working Directory browse button",
                "stale paint trails",
                "0.7.0.101",
            ):
                if token not in changelog:
                    fail(f"v0.7 beta.2 changelog missing: {token}")

        if version in ("0.7.0-beta.3", "0.7.0-beta.4", "0.7.0-beta.5", "0.7.0-beta.6", "0.7.0-beta.7", "0.7.0-beta.8", "0.7.0-beta.9", "0.7.0-beta.10", "0.7.0-beta.11", "0.7.0-beta.12", "0.7.0-rc.1", "0.7.0"):
            uninstaller = read("src/uninstaller/UninstallMain.cpp")
            beta_validation = read("docs/V0.7_BETA_VALIDATION.md")
            update_policy_tests = read("tests/UpdatePolicyTests.cpp")
            readme = read("README.md")
            changelog = read("CHANGELOG.md")

            for token in (
                "RemovalFailure",
                "NativeFilesystemError",
                "IsTransientRemovalError",
                "RemoveAllWithRetry",
                "ERROR_SHARING_VIOLATION",
                "ERROR_LOCK_VIOLATION",
                "ERROR_DIR_NOT_EMPTY",
                "gRemovalFailurePath",
                "Failed path: ",
            ):
                if token not in uninstaller:
                    fail(f"v0.7 beta.3 Native Uninstall hardening missing: {token}")

            for token in (
                "transient sharing/lock failures are retried",
                "actual filesystem error and failing path",
            ):
                if token not in beta_validation:
                    fail(f"v0.7 beta.3 uninstall validation regression missing: {token}")

            for token in (
                '"0.7.0-beta.2"',
                '"0.7.0-beta.3"',
                '"0.7.0-rc.1"',
            ):
                if token not in update_policy_tests:
                    fail(f"v0.7 beta.3 update-order regression missing: {token}")

            for token in (
                "Native Uninstall Full-Remove Hardening",
                "0.7.0.102",
                "stale Win32",
            ):
                if token not in readme:
                    fail(f"v0.7 beta.3 README missing: {token}")

            for token in (
                "0.7.0-beta.3",
                "bounded retry",
                "failed removal path",
                "0.7.0.102",
            ):
                if token not in changelog:
                    fail(f"v0.7 beta.3 changelog missing: {token}")

        if version == "0.7.0-beta.4":
            update_manager = read("src/platform/UpdateManager.cpp") + read("src/platform/UpdateManager.hpp")
            settings_ui = read("src/ui/SettingsWindow.cpp")
            uninstaller = read("src/uninstaller/UninstallMain.cpp")
            beta_validation = read("docs/V0.7_BETA_VALIDATION.md")
            update_policy_tests = read("tests/UpdatePolicyTests.cpp")
            readme = read("README.md")
            changelog = read("CHANGELOG.md")

            for token in (
                "ChannelNotNewer",
                "StableManifestUnavailable",
                "kStableLatestReleaseMetadataUrl",
                "ParseLatestStableReleaseVersion",
                "nativeError == 404",
                "releases/latest",
            ):
                if token not in update_manager:
                    fail(f"v0.7 beta.4 stable update fallback missing: {token}")

            for token in (
                "稳定版通道最新为 v",
                "不会降级",
                "未提供应用内更新清单",
            ):
                if token not in settings_ui:
                    fail(f"v0.7 beta.4 stable update UX missing: {token}")

            for token in (
                "ScheduleDeleteOnReboot",
                "MOVEFILE_DELAY_UNTIL_REBOOT",
                "MB_SETFOREGROUND",
                "MB_TOPMOST",
                "removalDeferred",
            ):
                if token not in uninstaller:
                    fail(f"v0.7 beta.4 uninstall completion hardening missing: {token}")

            for token in (
                "without surfacing HTTP 404/system error",
                "scheduled for deletion on the next Windows reboot",
                "foreground/topmost",
            ):
                if token not in beta_validation:
                    fail(f"v0.7 beta.4 desktop regression checklist missing: {token}")

            for token in (
                '"0.7.0-beta.3"',
                '"0.7.0-beta.4"',
                '"0.7.0-rc.1"',
            ):
                if token not in update_policy_tests:
                    fail(f"v0.7 beta.4 update-order regression missing: {token}")

            for token in (
                "Stable Update & Native Uninstall Completion Fixes",
                "0.7.0.103",
                "MOVEFILE_DELAY_UNTIL_REBOOT",
            ):
                if token not in readme:
                    fail(f"v0.7 beta.4 README missing: {token}")

            for token in (
                "0.7.0-beta.4",
                "HTTP 404",
                "foreground/topmost",
                "0.7.0.103",
            ):
                if token not in changelog:
                    fail(f"v0.7 beta.4 changelog missing: {token}")

        if version == "0.7.0-beta.5":
            uninstaller = read("src/uninstaller/UninstallMain.cpp")
            cmake = read("CMakeLists.txt")
            beta_validation = read("docs/V0.7_BETA_VALIDATION.md")
            update_policy_tests = read("tests/UpdatePolicyTests.cpp")
            readme = read("README.md")
            changelog = read("CHANGELOG.md")

            for token in (
                "NavigateExplorerAwayFromInstall",
                "RestartManagerLockOwners",
                "RemoveOneWithRetry",
                "recursive_directory_iterator",
                "Possible lock owner(s): ",
            ):
                if token not in uninstaller:
                    fail(f"v0.7 beta.5 precise uninstall cleanup missing: {token}")

            for forbidden in (
                "ScheduleDeleteOnReboot",
                "removalDeferred",
            ):
                if forbidden in uninstaller:
                    fail(f"v0.7 beta.5 must remove reboot-delete fallback: {forbidden}")

            for token in (
                "shlwapi",
                "ole32",
                "oleaut32",
                "rstrtmgr",
            ):
                if token not in cmake:
                    fail(f"v0.7 beta.5 uninstaller link dependency missing: {token}")

            for token in (
                "entry-by-entry",
                "navigates that Explorer view to the parent",
                "Restart Manager",
                "not deferred wholesale",
            ):
                if token not in beta_validation:
                    fail(f"v0.7 beta.5 desktop regression checklist missing: {token}")

            for token in (
                '"0.7.0-beta.4"',
                '"0.7.0-beta.5"',
                '"0.7.0-rc.1"',
            ):
                if token not in update_policy_tests:
                    fail(f"v0.7 beta.5 update-order regression missing: {token}")

            for token in (
                "Precise Native Uninstall Cleanup",
                "Restart Manager",
                "0.7.0.104",
            ):
                if token not in readme:
                    fail(f"v0.7 beta.5 README missing: {token}")

            for token in (
                "0.7.0-beta.5",
                "exact locked object",
                "Restart Manager",
                "0.7.0.104",
            ):
                if token not in changelog:
                    fail(f"v0.7 beta.5 changelog missing: {token}")

        if version in ("0.7.0-beta.6", "0.7.0-beta.7", "0.7.0-beta.8", "0.7.0-beta.9", "0.7.0-beta.10", "0.7.0-beta.11", "0.7.0-beta.12", "0.7.0-rc.1", "0.7.0"):
            uninstaller = read("src/uninstaller/UninstallMain.cpp")
            beta_validation = read("docs/V0.7_BETA_VALIDATION.md")
            update_policy_tests = read("tests/UpdatePolicyTests.cpp")
            readme = read("README.md")
            changelog = read("CHANGELOG.md")

            for token in (
                "shellReleaseRequest",
                "shellReleaseDone",
                "RequestShellRelease",
                "ServeShellReleaseBroker",
                "CreateEventW",
                "WaitForMultipleObjects",
                "--shell-release-request",
                "--shell-release-done",
            ):
                if token not in uninstaller:
                    fail(f"v0.7 beta.6 uninstall broker missing: {token}")

            for forbidden in (
                "ScheduleDeleteOnReboot",
                "removalDeferred",
            ):
                if forbidden in uninstaller:
                    fail(f"v0.7 beta.6 must keep reboot-delete fallback removed: {forbidden}")

            for token in (
                "does not move Explorer before UAC",
                "only when the elevated TEMP worker reaches the final deletion phase",
                "Cancelling UAC leaves the Explorer location unchanged",
                "original install-directory Uninstall.exe exits",
            ):
                if token not in beta_validation:
                    fail(f"v0.7 beta.6 broker validation missing: {token}")

            for token in (
                '"0.7.0-beta.5"',
                '"0.7.0-beta.6"',
                '"0.7.0-rc.1"',
            ):
                if token not in update_policy_tests:
                    fail(f"v0.7 beta.6 update-order regression missing: {token}")

            for token in (
                "Native Uninstall Broker UX",
                "normal-integrity",
                "0.7.0.105",
            ):
                if token not in readme:
                    fail(f"v0.7 beta.6 README missing: {token}")

            for token in (
                "0.7.0-beta.6",
                "Explorer broker",
                "Cancelling UAC",
                "0.7.0.105",
            ):
                if token not in changelog:
                    fail(f"v0.7 beta.6 changelog missing: {token}")

        if version == "0.7.0-beta.7":
            uninstaller = read("src/uninstaller/UninstallMain.cpp")
            beta_validation = read("docs/V0.7_BETA_VALIDATION.md")
            update_policy_tests = read("tests/UpdatePolicyTests.cpp")
            readme = read("README.md")
            changelog = read("CHANGELOG.md")

            for token in (
                "DirectoryHandle",
                "shellLeaseAcquired",
                "--shell-lease-acquired",
                "AcquireDirectoryDeleteLease",
                "DeleteDirectoryThroughLease",
                "FILE_SHARE_DELETE",
                "SetFileInformationByHandle",
                "FileDispositionInfo",
            ):
                if token not in uninstaller:
                    fail(f"v0.7 beta.7 delete-lease handoff missing: {token}")

            for forbidden in (
                "ScheduleDeleteOnReboot",
                "removalDeferred",
            ):
                if forbidden in uninstaller:
                    fail(f"v0.7 beta.7 must keep reboot-delete fallback removed: {forbidden}")

            for token in (
                "DELETE-capable root-directory lease before deleting any installation-tree entries",
                "keeps its root DELETE lease until the elevated worker confirms",
                "deleted through the already-held directory handle",
                "does not leave only an empty outer directory",
            ):
                if token not in beta_validation:
                    fail(f"v0.7 beta.7 lease validation missing: {token}")

            for token in (
                '"0.7.0-beta.6"',
                '"0.7.0-beta.7"',
                '"0.7.0-rc.1"',
            ):
                if token not in update_policy_tests:
                    fail(f"v0.7 beta.7 update-order regression missing: {token}")

            for token in (
                "Native Uninstall Delete-Lease Handoff",
                "SetFileInformationByHandle",
                "0.7.0.106",
            ):
                if token not in readme:
                    fail(f"v0.7 beta.7 README missing: {token}")

            for token in (
                "0.7.0-beta.7",
                "DELETE-access lease handoff",
                "FileDispositionInfo",
                "0.7.0.106",
            ):
                if token not in changelog:
                    fail(f"v0.7 beta.7 changelog missing: {token}")

        if version == "0.7.0-beta.8":
            uninstaller = read("src/uninstaller/UninstallMain.cpp")
            beta_validation = read("docs/V0.7_BETA_VALIDATION.md")
            update_policy_tests = read("tests/UpdatePolicyTests.cpp")
            readme = read("README.md")
            changelog = read("CHANGELOG.md")

            for token in (
                "ExplorerParkingDirectory",
                "RequestShellRelease",
                "ServeShellReleaseBroker",
                "AcquireDirectoryDeleteLease",
                "shellLeaseAcquired",
                "RestartManagerLockOwners",
            ):
                if token not in uninstaller:
                    fail(f"v0.7 beta.8 shell-parking uninstall fix missing: {token}")

            for forbidden in (
                "ScheduleDeleteOnReboot",
                "removalDeferred",
            ):
                if forbidden in uninstaller:
                    fail(f"v0.7 beta.8 must keep reboot-delete fallback removed: {forbidden}")

            for token in (
                "parked outside the installation root's immediate parent",
                "broker does not acquire the root DELETE lease",
                "without a second ReleaseDone timeout/1460 cascade",
            ):
                if token not in beta_validation:
                    fail(f"v0.7 beta.8 validation missing: {token}")

            for token in (
                '"0.7.0-beta.7"',
                '"0.7.0-beta.8"',
                '"0.7.0-rc.1"',
            ):
                if token not in update_policy_tests:
                    fail(f"v0.7 beta.8 update-order regression missing: {token}")

            for token in (
                "Native Uninstall Shell-Parking Fix",
                "error 1460",
                "0.7.0.107",
            ):
                if token not in readme:
                    fail(f"v0.7 beta.8 README missing: {token}")

            for token in (
                "0.7.0-beta.8",
                "timeout/error 1460",
                "single destructive lease",
                "0.7.0.107",
            ):
                if token not in changelog:
                    fail(f"v0.7 beta.8 changelog missing: {token}")

        if version == "0.7.0-beta.9":
            app_paths = read("src/core/AppPathsProvider.cpp")
            provider_cache = read("src/core/ProviderCache.cpp")
            provider_smoke = read("tests/WindowsProviderSmokeTests.cpp")
            beta_validation = read("docs/V0.7_BETA_VALIDATION.md")
            update_policy_tests = read("tests/UpdatePolicyTests.cpp")
            readme = read("README.md")
            changelog = read("CHANGELOG.md")

            for token in (
                "is_regular_file",
                "ERROR_FILE_NOT_FOUND",
            ):
                if token not in app_paths:
                    fail(f"v0.7 beta.9 App Paths live-target filter missing: {token}")

            for token in (
                "CachedProviderTargetIsUsable",
                "CommandSource::AppPaths",
                "is_regular_file",
            ):
                if token not in provider_cache:
                    fail(f"v0.7 beta.9 provider-cache stale filter missing: {token}")

            for token in (
                "missing.exe",
                "live.exe",
                "apppath:stale",
                "apppath:live",
            ):
                if token not in provider_smoke:
                    fail(f"v0.7 beta.9 Windows provider regression missing: {token}")

            for token in (
                "stale Windows App Paths registry entry",
                "suppressed immediately on startup",
                "valid Start Menu shortcut",
                "does not reintroduce the stale App Paths result",
            ):
                if token not in beta_validation:
                    fail(f"v0.7 beta.9 desktop validation missing: {token}")

            for token in (
                '"0.7.0-beta.8"',
                '"0.7.0-beta.9"',
                '"0.7.0-rc.1"',
            ):
                if token not in update_policy_tests:
                    fail(f"v0.7 beta.9 update-order regression missing: {token}")

            for token in (
                "Stale App Paths Filtering",
                "0.7.0.108",
                "very first search after upgrade",
            ):
                if token not in readme:
                    fail(f"v0.7 beta.9 README missing: {token}")

            for token in (
                "0.7.0-beta.9",
                "stale Windows App Paths",
                "provider-cache",
                "0.7.0.108",
            ):
                if token not in changelog:
                    fail(f"v0.7 beta.9 changelog missing: {token}")

        if version == "0.7.0-beta.10":
            launcher = read("src/ui/LauncherWindow.cpp")
            beta_validation = read("docs/V0.7_BETA_VALIDATION.md")
            update_policy_tests = read("tests/UpdatePolicyTests.cpp")
            readme = read("README.md")
            changelog = read("CHANGELOG.md")

            show_start = launcher.find("void LauncherWindow::Show()")
            show_end = launcher.find("void LauncherWindow::Hide()", show_start)
            show_body = launcher[show_start:show_end]
            for token in (
                "LB_SETCURSEL",
                "static_cast<WPARAM>(-1)",
                "SetWindowTextW(edit_, L\"\")",
            ):
                if token not in show_body:
                    fail(f"v0.7 beta.10 fresh-show selection reset missing: {token}")

            if show_body.find("LB_SETCURSEL") > show_body.find("SetWindowTextW(edit_, L\"\")"):
                fail("v0.7 beta.10 must clear selection before synchronous EN_CHANGE")

            for token in (
                "query is empty and row 1 is selected",
                "dynamic-result refreshes still preserve",
                "Clear query on show disabled",
            ):
                if token not in beta_validation:
                    fail(f"v0.7 beta.10 validation missing: {token}")

            for token in (
                '"0.7.0-beta.9"',
                '"0.7.0-beta.10"',
                '"0.7.0-rc.1"',
            ):
                if token not in update_policy_tests:
                    fail(f"v0.7 beta.10 update-order regression missing: {token}")

            for token in (
                "Fresh Launcher Selection Reset",
                "row 1",
                "0.7.0.109",
            ):
                if token not in readme:
                    fail(f"v0.7 beta.10 README missing: {token}")

            for token in (
                "0.7.0-beta.10",
                "synchronous",
                "0.7.0.109",
            ):
                if token not in changelog:
                    fail(f"v0.7 beta.10 changelog missing: {token}")

        if version == "0.7.0-beta.11":
            launcher = read("src/ui/LauncherWindow.cpp")
            beta_validation = read("docs/V0.7_BETA_VALIDATION.md")
            update_policy_tests = read("tests/UpdatePolicyTests.cpp")
            readme = read("README.md")
            changelog = read("CHANGELOG.md")

            command_start = launcher.find("case WM_COMMAND:")
            command_end = launcher.find("case WM_CONTEXTMENU:", command_start)
            command_body = launcher[command_start:command_end]
            en_change = command_body.find("HIWORD(wParam) == EN_CHANGE")
            if en_change < 0:
                fail("v0.7 beta.11 EN_CHANGE handler missing")

            en_body = command_body[en_change:command_body.find("return 0;", en_change) + len("return 0;")]
            for token in (
                "LB_SETCURSEL",
                "static_cast<WPARAM>(-1)",
                "RefreshResults",
            ):
                if token not in en_body:
                    fail(f"v0.7 beta.11 query-edit selection reset missing: {token}")

            if en_body.find("LB_SETCURSEL") > en_body.find("RefreshResults"):
                fail("v0.7 beta.11 must clear selection before rebuilding changed query")

            for token in (
                "after every text edit",
                "Backspace or clear the query",
                "with no query text change",
            ):
                if token not in beta_validation:
                    fail(f"v0.7 beta.11 validation missing: {token}")

            for token in (
                '"0.7.0-beta.10"',
                '"0.7.0-beta.11"',
                '"0.7.0-rc.1"',
            ):
                if token not in update_policy_tests:
                    fail(f"v0.7 beta.11 update-order regression missing: {token}")

            for token in (
                "Query-Edit Selection Reset",
                "EN_CHANGE",
                "0.7.0.110",
            ):
                if token not in readme:
                    fail(f"v0.7 beta.11 README missing: {token}")

            for token in (
                "0.7.0-beta.11",
                "backspace/manual query clearing",
                "0.7.0.110",
            ):
                if token not in changelog:
                    fail(f"v0.7 beta.11 changelog missing: {token}")

        if version in ("0.7.0-beta.12", "0.7.0-rc.1", "0.7.0"):
            launcher = read("src/ui/LauncherWindow.cpp")
            context_cpp = read("src/core/ContextActions.cpp")
            context_h = read("src/core/ContextActions.hpp")
            launcher_result = read("src/core/LauncherResult.hpp")
            app_cpp = read("src/app/App.cpp")
            merge_cpp = read("src/core/CommandMerge.cpp")
            merge_tests = read("tests/CommandMergeTests.cpp")
            beta_validation = read("docs/V0.7_BETA_VALIDATION.md")
            update_policy_tests = read("tests/UpdatePolicyTests.cpp")
            readme = read("README.md")
            changelog = read("CHANGELOG.md")

            for token in (
                "添加到快捷项...",
                "Add to shortcuts...",
                "打开所在目录",
                "Open containing folder",
                "以管理员身份运行",
                "Run as administrator",
            ):
                if token not in launcher:
                    fail(f"v0.7 beta.12 context-menu UX missing: {token}")

            for token in (
                "runAsAdministrator",
                "result.kind !=\n            ResultKind::Folder",
            ):
                if token not in context_h + context_cpp:
                    fail(f"v0.7 beta.12 context-action policy missing: {token}")

            for token in (
                "RunAsAdministrator",
                "forceRunAsAdmin",
                'L"runas"',
            ):
                if token not in launcher_result + app_cpp:
                    fail(f"v0.7 beta.12 elevation path missing: {token}")

            for token in (
                "providerRepresentatives",
                "Provider-vs-Provider",
                "canonicalized",
            ):
                if token not in merge_cpp:
                    fail(f"v0.7 beta.12 two-stage provider merge missing: {token}")

            for token in (
                "user:ts3",
                "start:teamspeak3",
                "apppath:teamspeak3",
                "start:teamspeak6",
            ):
                if token not in merge_tests:
                    fail(f"v0.7 beta.12 TeamSpeak merge regression missing: {token}")

            for token in (
                "Provider application right-click menu",
                "lower App Paths/PATH duplicate",
                "Run as administrator works",
            ):
                if token not in beta_validation:
                    fail(f"v0.7 beta.12 desktop validation missing: {token}")

            for token in (
                '"0.7.0-beta.11"',
                '"0.7.0-beta.12"',
                '"0.7.0-rc.1"',
            ):
                if token not in update_policy_tests:
                    fail(f"v0.7 beta.12 update-order regression missing: {token}")

            for token in (
                "Provider-to-Shortcut Workflow & Stable Provider Dedup",
                "0.7.0.111",
                "two-stage",
            ):
                if token not in readme:
                    fail(f"v0.7 beta.12 README missing: {token}")

            for token in (
                "0.7.0-beta.12",
                "Add to shortcuts",
                "0.7.0.111",
            ):
                if token not in changelog:
                    fail(f"v0.7 beta.12 changelog missing: {token}")

        if version in ("0.7.0-beta.1", "0.7.0-beta.2", "0.7.0-beta.3", "0.7.0-beta.4", "0.7.0-beta.5", "0.7.0-beta.6", "0.7.0-beta.7", "0.7.0-beta.8", "0.7.0-beta.9", "0.7.0-beta.10", "0.7.0-beta.11", "0.7.0-beta.12", "0.7.0-rc.1", "0.7.0"):
            config_io = read("src/core/ConfigIO.hpp")
            provider_ids = read("src/core/ProviderIds.hpp")
            hotkey_registry = read("src/core/HotkeyRegistry.hpp") + read("src/core/HotkeyRegistry.cpp")
            command_store = read("src/core/UserCommandStore.cpp")
            shortcut_compat = read("tests/ShortcutCompatibilityTests.cpp")
            update_policy_tests = read("tests/UpdatePolicyTests.cpp")
            beta_validation = read("docs/V0.7_BETA_VALIDATION.md")
            package_verify = read("scripts/verify_package.ps1")
            workflow = read(".github/workflows/build.yml")
            release_workflow = read(".github/workflows/release.yml")
            cmake = read("CMakeLists.txt")
            launcher_header = read("src/ui/LauncherWindow.hpp")
            settings_example = json.loads(read("config/settings.example.json"))
            readme = read("README.md")
            changelog = read("CHANGELOG.md")

            expected_schemas = {
                "kSettingsSchemaVersion": 7,
                "kCommandsSchemaVersion": 2,
                "kUsageSchemaVersion": 1,
            }
            for name, expected in expected_schemas.items():
                actual = cpp_int("src/core/ConfigIO.hpp", name)
                if actual != expected:
                    fail(f"v0.7 beta.1 froze {name}={expected}, got {actual}")

            if cpp_int("src/core/ProviderCache.cpp", "kProviderCacheSchemaVersion") != 2:
                fail("v0.7 beta.1 must freeze provider-cache schemaVersion 2")

            if settings_example.get("schemaVersion") != 7:
                fail("v0.7 beta.1 settings example must remain schemaVersion 7")

            expected_providers = {
                "windows.startmenu": True,
                "windows.packaged": True,
                "windows.apppaths": True,
                "windows.path": True,
                "everything.filesystem": False,
            }
            if settings_example.get("providers") != expected_providers:
                fail("v0.7 beta.1 changed frozen provider defaults/IDs")

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
                    fail(f"v0.7 beta.1 Provider ID missing: {provider_id}")

            expected_hotkeys = (
                "launcher.activate",
                "launcher.activateSecondary",
                "launcher.openSettings",
                "result.navigateCurrentFileManager",
                "result.copySelectedTarget",
            )
            bindings = settings_example.get("hotkeys", {}).get("bindings", {})
            if set(bindings) != set(expected_hotkeys):
                fail("v0.7 beta.1 changed frozen Hotkey Registry binding IDs")
            for action_id in expected_hotkeys:
                if action_id not in hotkey_registry:
                    fail(f"v0.7 beta.1 Hotkey Registry source lost action ID: {action_id}")

            for token in (
                "commands TSV v3",
                "runtimeInputMode",
                "icon",
                "command.enabled = true;",
                "command.pinned = false;",
            ):
                if token not in command_store:
                    fail(f"v0.7 beta.1 command/TSV compatibility contract missing: {token}")

            for token in (
                "commands TSV v1",
                "commands TSV v2",
                "commands TSV v3",
                "legacy-five-column",
                "RuntimeInputMode::UrlEncoded",
                "RuntimeInputMode::Raw",
                "runner.ico",
                "roundtrip-v3.tsv",
                "sourceCommand.aliases",
                "sourceCommand.sortOrder",
            ):
                if token not in shortcut_compat:
                    fail(f"v0.7 beta.1 shortcut compatibility matrix missing: {token}")

            for token in (
                "shortcut_compatibility_tests",
                "tests/ShortcutCompatibilityTests.cpp",
            ):
                if token not in cmake:
                    fail(f"v0.7 beta.1 CMake compatibility test wiring missing: {token}")

            for token in (
                '"0.7.0-alpha.9.4",',
                '"0.7.0-beta.1"',
                '"0.7.0-beta.2"',
                '"0.7.0-rc.1"',
                '"0.6.0"',
            ):
                if token not in update_policy_tests:
                    fail(f"v0.7 beta.1 update-order regression missing: {token}")

            for token in (
                "Native update: alpha.9.4 -> beta.1",
                "Shortcut Manager / Editor",
                "Runtime Input",
                "Path Conversion / Portability",
                "Managed Everything lifecycle",
                "Native Uninstall",
                "DPI / compatibility",
                "Exit criteria",
            ):
                if token not in beta_validation:
                    fail(f"v0.7 beta.1 desktop-validation matrix missing: {token}")

            for source_name, source in (
                ("build workflow", workflow),
                ("tag workflow", release_workflow),
                ("package allowlist", package_verify),
            ):
                if "V0.7_BETA_VALIDATION.md" not in source:
                    fail(f"v0.7 beta.1 package validation doc missing from {source_name}")

            for token in (
                '"ALTRunNext.exe"',
                '"Update.exe"',
                '"Uninstall.exe"',
            ):
                if token not in package_verify:
                    fail(f"v0.7 beta.1 package executable contract missing: {token}")
            if "ALTRunNext.Updater.exe" in package_verify:
                fail("v0.7 beta.1 package resurrected obsolete ALTRunNext.Updater.exe")

            for name, expected in {
                "widthLogical_": 420,
                "rowHeightLogical_": 16,
                "maxResults_": 10,
            }.items():
                found = re.search(rf"\b{re.escape(name)}\s*\{{(\d+)\}}", launcher_header)
                if not found or int(found.group(1)) != expected:
                    fail(f"v0.7 beta.1 changed frozen Classic geometry: {name}")

            for token in (
                "Feature Freeze & Workflow Hardening",
                "schemaVersion 7",
                "TSV v3",
                "V0.7_BETA_VALIDATION.md",
                "0.7.0.100",
            ):
                if token not in readme:
                    fail(f"v0.7 beta.1 README freeze contract missing: {token}")

            for token in (
                "0.7.0-beta.1",
                "feature freeze",
                "shortcut_compatibility_tests",
                "V0.7_BETA_VALIDATION.md",
                "0.7.0.100",
            ):
                if token not in changelog:
                    fail(f"v0.7 beta.1 changelog missing: {token}")

        if version in ("0.7.0-alpha.9.4", "0.7.0-beta.1", "0.7.0-beta.2", "0.7.0-beta.3", "0.7.0-beta.4", "0.7.0-beta.5", "0.7.0-beta.6", "0.7.0-beta.7", "0.7.0-beta.8", "0.7.0-beta.9", "0.7.0-beta.10", "0.7.0-beta.11", "0.7.0-beta.12", "0.7.0-rc.1", "0.7.0"):
            bootstrapper = read("src/platform/EverythingBootstrapper.cpp") + read("src/platform/EverythingBootstrapper.hpp")
            main_cpp = read("src/main.cpp")
            lifecycle_test = read("tests/EverythingLifecycleTests.cpp")
            readme = read("README.md")
            changelog = read("CHANGELOG.md")

            for token in (
                "ManagedEverythingServicePolicyStatus",
                "ManagedEverythingServicePolicyResult",
                "SetManagedEverythingServiceEnabled",
                "ApplyManagedEverythingServiceEnabledPolicy",
                "IsManagedEverythingServiceExecutable",
                "RunElevatedServicePolicyHelper",
                "QueryEverythingServiceStartType",
                "SERVICE_DISABLED",
                "SERVICE_AUTO_START",
                "SERVICE_CHANGE_CONFIG",
                "SERVICE_CONTROL_STOP",
                "ChangeServiceConfigW",
                "ERROR_ACCESS_DENIED",
            ):
                if token not in bootstrapper:
                    fail(f"v0.7 alpha.9.4 managed Everything provider-service lifecycle missing: {token}")

            for token in (
                "--set-managed-everything-service",
                'L"enabled"',
                'L"disabled"',
                "ApplyManagedEverythingServiceEnabledPolicy",
                "managedEverythingServiceEnabled",
            ):
                if token not in main_cpp:
                    fail(f"v0.7 alpha.9.4 elevated service-policy entrypoint missing: {token}")

            for token in (
                "wasEnabled",
                "StopManagedEverythingLifecycle();",
                "win::SetManagedEverythingServiceEnabled",
                "ManagedEverythingServicePolicyStatus::",
                "ElevationCancelled",
                "StartEverythingBootstrap",
            ):
                if token not in app:
                    fail(f"v0.7 alpha.9.4 transactional provider toggle missing: {token}")

            for token in (
                "同时停止并禁用其开机自启",
                "重新启用时恢复",
                "外部 Everything 不会被停止或改配置",
                "取消 UAC 后开关会恢复原状态",
            ):
                if token not in settings_ui:
                    fail(f"v0.7 alpha.9.4 Everything provider lifecycle UX missing: {token}")

            for token in (
                "IsManagedEverythingServiceExecutable",
                "external",
                "Everything.exe",
            ):
                if token not in lifecycle_test:
                    fail(f"v0.7 alpha.9.4 managed-service ownership regression missing: {token}")

            for token in (
                "Managed Everything Provider Lifecycle",
                "SERVICE_DISABLED",
                "SERVICE_AUTO_START",
                "External/user-installed Everything services are never stopped",
            ):
                if token not in readme:
                    fail(f"v0.7 alpha.9.4 lifecycle documentation missing: {token}")

            for token in (
                "0.7.0-alpha.9.4",
                "SERVICE_DISABLED",
                "SERVICE_AUTO_START",
                "UAC",
            ):
                if token not in changelog:
                    fail(f"v0.7 alpha.9.4 changelog missing: {token}")

        if version in ("0.7.0-alpha.9.3", "0.7.0-alpha.9.4", "0.7.0-beta.1", "0.7.0-beta.2", "0.7.0-beta.3", "0.7.0-beta.4", "0.7.0-beta.5", "0.7.0-beta.6", "0.7.0-beta.7", "0.7.0-beta.8", "0.7.0-beta.9", "0.7.0-beta.10", "0.7.0-beta.11", "0.7.0-beta.12", "0.7.0-rc.1", "0.7.0"):
            bootstrapper = read("src/platform/EverythingBootstrapper.cpp") + read("src/platform/EverythingBootstrapper.hpp")
            update_manager = read("src/platform/UpdateManager.cpp") + read("src/platform/UpdateManager.hpp")
            updater = read("src/updater/UpdaterMain.cpp")
            package_verify = read("scripts/verify_package.ps1")
            workflow = read(".github/workflows/build.yml")
            release_workflow = read(".github/workflows/release.yml")
            runtime_smoke = read("scripts/verify_runtime_smoke.ps1")
            app_cpp = read("src/app/App.cpp")
            cmake = read("CMakeLists.txt")
            uninstaller = read("src/uninstaller/UninstallMain.cpp")
            readme = read("README.md")
            changelog = read("CHANGELOG.md")

            for token in (
                'L"Update.exe"',
                'L"Uninstall.exe"',
                "LaunchPreparedUpdate",
            ):
                if token not in update_manager:
                    fail(f"v0.7 alpha.9.3 generic updater contract missing: {token}")

            for source_name, source in (
                ("build workflow", workflow),
                ("release workflow", release_workflow),
                ("package allowlist", package_verify),
                ("runtime smoke", runtime_smoke),
                ("App runtime", app_cpp),
                ("UpdateManager", update_manager),
            ):
                if "ALTRunNext.Updater.exe" in source:
                    fail(f"v0.7 alpha.9.3 legacy updater name remains in {source_name}")

            for token in (
                "Update.exe",
                "Uninstall.exe",
                "ALTRunNextUninstaller",
            ):
                if token not in workflow:
                    fail(f"v0.7 alpha.9.3 build/package helper contract missing: {token}")
                if token not in release_workflow:
                    fail(f"v0.7 alpha.9.3 release helper contract missing: {token}")

            for token in (
                '"Update.exe"',
                '"Uninstall.exe"',
                "exact allowlist verified",
            ):
                if token not in package_verify:
                    fail(f"v0.7 alpha.9.3 package allowlist missing: {token}")

            for token in (
                "Update.exe",
                "Uninstall.exe",
            ):
                if token not in runtime_smoke:
                    fail(f"v0.7 alpha.9.3 runtime helper smoke missing: {token}")

            for token in (
                "Clean Generic Update/Uninstall Helpers",
                "Update.exe",
                "Uninstall.exe",
                "manual",
            ):
                if token not in readme:
                    fail(f"v0.7 alpha.9.3 documentation missing: {token}")

            for token in (
                "0.7.0-alpha.9.3",
                "Update.exe",
                "Uninstall.exe",
                "legacy",
            ):
                if token not in changelog:
                    fail(f"v0.7 alpha.9.3 changelog missing: {token}")

        if version == "0.7.0-alpha.9.2":
            bootstrapper = read("src/platform/EverythingBootstrapper.cpp") + read("src/platform/EverythingBootstrapper.hpp")
            update_manager = read("src/platform/UpdateManager.cpp") + read("src/platform/UpdateManager.hpp")
            updater = read("src/updater/UpdaterMain.cpp")
            package_verify = read("scripts/verify_package.ps1")
            workflow = read(".github/workflows/build.yml")
            cmake = read("CMakeLists.txt")
            uninstaller = read("src/uninstaller/UninstallMain.cpp")
            everything_header = read("src/platform/EverythingBootstrapper.hpp")
            release_workflow = read(".github/workflows/release.yml")
            runtime_smoke = read("scripts/verify_runtime_smoke.ps1")
            readme = read("README.md")
            changelog = read("CHANGELOG.md")

            if "ManagedEverythingServiceExecutable" in everything_header:
                fail("v0.7 alpha.9.2 must not expose a detached Program Files service host")

            for token in (
                "DetachedAlpha91ServiceExecutable",
                "DetachedAlpha91ServiceRoot",
                "servicePathNeedsPortableRepair",
                "SERVICE_AUTO_START",
                "SERVICE_CONTROL_STOP",
                "ChangeServiceConfigW",
                "CleanupDetachedAlpha91ServiceHost",
                "ManagedEverythingExecutable",
            ):
                if token not in bootstrapper:
                    fail(f"v0.7 alpha.9.2 portable Everything repair contract missing: {token}")

            for token in (
                'L"Update.exe"',
                'L"Uninstall.exe"',
                "ALTRunNext.Updater.exe",
                "LaunchPreparedUpdate",
            ):
                if token not in update_manager:
                    fail(f"v0.7 alpha.9.2 generic update-helper contract missing: {token}")

            for token in (
                'L"Update.exe"',
                'L"Uninstall.exe"',
                "--apply",
                "ValidateSource",
                "Rollback",
            ):
                if token not in updater:
                    fail(f"v0.7 alpha.9.2 renamed updater validation missing: {token}")

            for token in (
                "--perform",
                "--parent-pid",
                "--install",
                "--delete-data",
                "GetTempPathW",
                "ShellExecuteExW",
                'L"runas"',
                'L"ALTRunNext.Launcher"',
                "WM_CLOSE",
                "HKEY_CURRENT_USER",
                "RegDeleteValueW",
                "SERVICE_STOP",
                "SERVICE_CONTROL_STOP",
                "DeleteService",
                "QueryServiceConfigW",
                "PathStartsWithDirectory",
                "DetachedAlpha91Root",
                'L"data"',
                'L"tools"',
                'L"Everything"',
                'L"update"',
                "MOVEFILE_DELAY_UNTIL_REBOOT",
            ):
                if token not in uninstaller:
                    fail(f"v0.7 alpha.9.2 native uninstaller contract missing: {token}")

            for token in (
                "ALTRunNextUpdater",
                "ALTRunNextUninstaller",
                "src/uninstaller/UninstallMain.cpp",
                'OUTPUT_NAME\n                "Update"',
                'OUTPUT_NAME\n                "Uninstall"',
            ):
                if token not in cmake:
                    fail(f"v0.7 alpha.9.2 helper build wiring missing: {token}")

            for source in (workflow, release_workflow):
                for token in (
                    "Update.exe",
                    "Uninstall.exe",
                    "ALTRunNext.Updater.exe",
                    "ALTRunNextUninstaller",
                ):
                    if token not in source:
                        fail(f"v0.7 alpha.9.2 packaging/CI helper contract missing: {token}")

            for token in (
                '"Update.exe"',
                '"Uninstall.exe"',
                '"ALTRunNext.Updater.exe"',
                "exact allowlist verified",
            ):
                if token not in package_verify:
                    fail(f"v0.7 alpha.9.2 package helper allowlist missing: {token}")

            for token in (
                "Update.exe",
                "Uninstall.exe",
                "legacyUpdaterBridge",
            ):
                if token not in runtime_smoke:
                    fail(f"v0.7 alpha.9.2 packaged helper smoke missing: {token}")

            for token in (
                "Portable Managed Everything & Native Uninstaller",
                "SERVICE_AUTO_START",
                "Uninstall.exe",
                "compatibility copy",
            ):
                if token not in readme:
                    fail(f"v0.7 alpha.9.2 documentation missing: {token}")

            for token in (
                "0.7.0-alpha.9.2",
                "Update.exe",
                "Uninstall.exe",
                "Program Files",
            ):
                if token not in changelog:
                    fail(f"v0.7 alpha.9.2 changelog missing: {token}")

        if version in ("0.7.0-alpha.9", "0.7.0-alpha.9.1", "0.7.0-alpha.9.2", "0.7.0-alpha.9.3", "0.7.0-alpha.9.4", "0.7.0-beta.1", "0.7.0-beta.2", "0.7.0-beta.3", "0.7.0-beta.4", "0.7.0-beta.5", "0.7.0-beta.6", "0.7.0-beta.7", "0.7.0-beta.8", "0.7.0-beta.9", "0.7.0-beta.10", "0.7.0-beta.11", "0.7.0-beta.12", "0.7.0-rc.1", "0.7.0"):
            update_policy = read("src/core/UpdatePolicy.cpp") + read("src/core/UpdatePolicy.hpp")
            update_manifest = read("src/core/UpdateManifest.cpp") + read("src/core/UpdateManifest.hpp")
            update_manager = read("src/platform/UpdateManager.cpp") + read("src/platform/UpdateManager.hpp")
            updater = read("src/updater/UpdaterMain.cpp")
            update_test = read("tests/UpdatePolicyTests.cpp")
            package_verify = read("scripts/verify_package.ps1")
            manifest_generator = read("scripts/generate_update_manifest.py")
            update_doc = read("docs/UPDATE_SYSTEM.md")
            main_cpp = read("src/main.cpp")

            for token in (
                "UpdateChannel",
                "DefaultUpdateChannelForVersion",
                "CompareVersions",
                "IsUpdateVersionNewer",
                "UpdateManifestUrl",
                "dev-latest/update-manifest.json",
                "/releases/latest/download/update-manifest.json",
                "IsSafeUpdateAssetName",
            ):
                if token not in update_policy:
                    fail(f"v0.7 alpha.9 update policy missing: {token}")

            for token in (
                "ParseUpdateManifest",
                '"schemaVersion"',
                '"assets"',
                '"x64"',
                '"ARM64"',
                "IsSha256HexString",
            ):
                if token not in update_manifest:
                    fail(f"v0.7 alpha.9 update manifest parser missing: {token}")

            for token in (
                "WinHttpOpen",
                "WINHTTP_OPTION_REDIRECT_POLICY",
                "BCryptOpenAlgorithmProvider",
                "BCRYPT_SHA256_ALGORITHM",
                'L".download"',
                "ExtractZipWithShell",
                "UpdateAutoCheckDue",
                'L"update-state.json"',
                'L"staging"',
                'L"backup"',
                "LaunchPreparedUpdate",
                'L"runas"',
            ):
                if token not in update_manager:
                    fail(f"v0.7 alpha.9 native update manager missing: {token}")

            update_helper_token = (
                'L"Update.exe"'
                if version in ("0.7.0-alpha.9.3", "0.7.0-alpha.9.4", "0.7.0-beta.1", "0.7.0-beta.2", "0.7.0-beta.3", "0.7.0-beta.4", "0.7.0-beta.5", "0.7.0-beta.6", "0.7.0-beta.7", "0.7.0-beta.8", "0.7.0-beta.9", "0.7.0-beta.10", "0.7.0-beta.11", "0.7.0-beta.12", "0.7.0-rc.1", "0.7.0")
                else "ALTRunNext.Updater.exe"
            )
            if update_helper_token not in update_manager:
                fail(f"v0.7 alpha.9 updater helper name missing: {update_helper_token}")

            for forbidden in (
                "PowerShell",
                "powershell",
                "Invoke-WebRequest",
                "Expand-Archive",
            ):
                if forbidden in update_manager:
                    fail(f"v0.7 alpha.9 update path must stay native: {forbidden}")

            for token in (
                "--apply",
                "--parent-pid",
                "--source",
                "--install",
                "--backup",
                "--health-event",
                "WaitForParent",
                "ApplyPackage",
                "Rollback",
                "IsDataRelative",
                "CreateHealthEvent",
                "WaitForSingleObject",
                "CreateProcessWithTokenW",
                "TerminateProcess",
            ):
                if token not in updater:
                    fail(f"v0.7 alpha.9 safe updater helper missing: {token}")

            for token in (
                "--post-update-health-event",
                "updateHealthEvent",
            ):
                if token not in main_cpp:
                    fail(f"v0.7 alpha.9 startup health entrypoint missing: {token}")

            for token in (
                "StartUpdateCheck",
                "StartUpdateDownloadAndInstall",
                "HandleUpdateStatusMessage",
                "BeginPreparedUpdate",
                "SignalStartupHealthEvent",
                "updateThread_",
                "updateManifest_",
                "updateGeneration_",
                "PostQuitMessage(0)",
            ):
                if token not in app:
                    fail(f"v0.7 alpha.9 App update lifecycle missing: {token}")

            if app.find("SignalStartupHealthEvent();") > app.find("StartUpdateCheck(false);"):
                fail("v0.7 alpha.9 must signal post-update health before starting background update checks")

            for token in (
                "kIdUpdateChannel",
                "kIdUpdateAutoCheck",
                "kIdUpdateCheck",
                "kIdUpdateInstall",
                'T(L"稳定版", L"Stable")',
                'T(L"开发版", L"Development")',
                'T(L"检查更新", L"Check for updates")',
                'T(L"下载并安装", L"Download and install")',
                "RefreshUpdateStatus",
                "ApplyUpdateSettings",
            ):
                if token not in settings_ui:
                    fail(f"v0.7 alpha.9 About update UX missing: {token}")

            settings_cpp = read("src/core/Settings.cpp") + read("src/core/Settings.hpp")
            for token in (
                "autoCheckUpdates",
                "updateChannel",
                "DefaultUpdateChannelForVersion",
                '"update"',
                '"autoCheck"',
                '"channel"',
                "SetUpdateSettings",
            ):
                if token not in settings_cpp:
                    fail(f"v0.7 alpha.9 settings schema-7 update preference missing: {token}")

            update_object = settings.get("update", {})
            expected_update_channel = "stable" if version == "0.7.0" else "development"
            if update_object != {"autoCheck": True, "channel": expected_update_channel}:
                fail(
                    "v0.7 update example defaults mismatch: "
                    f"{update_object!r} != autoCheck=true/channel={expected_update_channel}"
                )

            for token in (
                "ALTRunNextUpdater",
                "src/updater/UpdaterMain.cpp",
                "src/platform/UpdateManager.cpp",
                "src/core/UpdatePolicy.cpp",
                "src/core/UpdateManifest.cpp",
                "update_policy_tests",
            ):
                if token not in cmake:
                    fail(f"v0.7 alpha.9 CMake update wiring missing: {token}")

            for token in (
                "update-manifest.json",
                "generate_update_manifest.py",
                "SHA256SUMS.txt",
                "dev-latest",
            ):
                if token not in workflow:
                    fail(f"v0.7 alpha.9 release update asset wiring missing: {token}")

            if version in ("0.7.0-alpha.9.3", "0.7.0-alpha.9.4", "0.7.0-beta.1", "0.7.0-beta.2", "0.7.0-beta.3", "0.7.0-beta.4", "0.7.0-beta.5", "0.7.0-beta.6", "0.7.0-beta.7", "0.7.0-beta.8", "0.7.0-beta.9", "0.7.0-beta.10", "0.7.0-beta.11", "0.7.0-beta.12", "0.7.0-rc.1", "0.7.0"):
                for token in ("Update.exe", "Uninstall.exe"):
                    if token not in workflow:
                        fail(f"v0.7 alpha.9.3 generic helper packaging missing: {token}")
                if "ALTRunNext.Updater.exe" in workflow:
                    fail("v0.7 alpha.9.3 must not package the legacy updater filename")
            elif "ALTRunNext.Updater.exe" not in workflow:
                fail("v0.7 alpha.9 legacy updater asset wiring missing")

            if workflow.count("update_policy_tests") < 4:
                fail("v0.7 alpha.9 update policy tests must run in both Windows smoke/baseline gates")

            if "exact allowlist verified" not in package_verify:
                fail("v0.7 alpha.9 package exact allowlist verification missing")

            if version in ("0.7.0-alpha.9.3", "0.7.0-alpha.9.4", "0.7.0-beta.1", "0.7.0-beta.2", "0.7.0-beta.3", "0.7.0-beta.4", "0.7.0-beta.5", "0.7.0-beta.6", "0.7.0-beta.7", "0.7.0-beta.8", "0.7.0-beta.9", "0.7.0-beta.10", "0.7.0-beta.11", "0.7.0-beta.12", "0.7.0-rc.1", "0.7.0"):
                for token in ('"Update.exe"', '"Uninstall.exe"'):
                    if token not in package_verify:
                        fail(f"v0.7 alpha.9.3 package helper allowlist missing: {token}")
                if '"ALTRunNext.Updater.exe"' in package_verify:
                    fail("v0.7 alpha.9.3 package allowlist still contains legacy updater")
            elif '"ALTRunNext.Updater.exe"' not in package_verify:
                fail("v0.7 alpha.9 legacy package updater contract missing")

            for token in (
                '"schemaVersion": 1',
                '"x64"',
                '"ARM64"',
                "ALTRunNext-x64.zip",
                "ALTRunNext-ARM64.zip",
                "json.dumps",
            ):
                if token not in manifest_generator:
                    fail(f"v0.7 alpha.9 release manifest generator missing: {token}")

            for token in (
                "0.7.0-alpha.8.4",
                "0.7.0-alpha.9",
                "0.7.0-beta.1",
                "0.7.0-rc.1",
                "0.7.0",
                "DefaultUpdateChannelForVersion",
                "../ALTRunNext.zip",
                "ParseUpdateManifest",
            ):
                if token not in update_test:
                    fail(f"v0.7 alpha.9 update policy regression coverage missing: {token}")

            upgrade_matrix = read("tests/UpgradeMatrixTests.cpp")
            for token in (
                "AssertSchema6Migration",
                "schema6-to-schema7",
                "MigratedFromSchemaVersion() ==",
                "DefaultUpdateChannelForVersion(kVersion)",
                "ExpectedDefaultUpdateChannelName",
                '"development"',
                '"stable"',
            ):
                if token not in upgrade_matrix:
                    fail(f"v0.7 alpha.9 schema-6 -> 7 migration coverage missing: {token}")

            for token in (
                "portable updater",
                "data/update/staging",
                "rollback",
                "signature verification",
            ):
                if token.lower() not in update_doc.lower():
                    fail(f"v0.7 alpha.9 update documentation missing: {token}")

            runtime_smoke = read("scripts/verify_runtime_smoke.ps1")
            for token in (
                "schemaVersion -ne 7",
                "update-state.json",
                "update.autoCheck",
                "$expectedUpdateChannel",
                '"development"',
                '"stable"',
            ):
                if token not in runtime_smoke:
                    fail(f"v0.7 alpha.9 packaged runtime update migration gate missing: {token}")

            if version in ("0.7.0-alpha.9.3", "0.7.0-alpha.9.4", "0.7.0-beta.1", "0.7.0-beta.2", "0.7.0-beta.3", "0.7.0-beta.4", "0.7.0-beta.5", "0.7.0-beta.6", "0.7.0-beta.7", "0.7.0-beta.8", "0.7.0-beta.9", "0.7.0-beta.10", "0.7.0-beta.11", "0.7.0-beta.12", "0.7.0-rc.1", "0.7.0"):
                for token in ("Update.exe", "Uninstall.exe"):
                    if token not in runtime_smoke:
                        fail(f"v0.7 alpha.9.3 runtime helper smoke missing: {token}")
                if "ALTRunNext.Updater.exe" in runtime_smoke:
                    fail("v0.7 alpha.9.3 runtime smoke still expects legacy updater")
            elif "ALTRunNext.Updater.exe" not in runtime_smoke:
                fail("v0.7 alpha.9 legacy runtime updater smoke missing")

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

    if version in ("0.7.0-alpha.2.3", "0.7.0-alpha.2.4", "0.7.0-alpha.2.5", "0.7.0-alpha.2.6", "0.7.0-alpha.3", "0.7.0-alpha.3.1", "0.7.0-alpha.4", "0.7.0-alpha.5", "0.7.0-alpha.5.1", "0.7.0-alpha.5.2", "0.7.0-alpha.6", "0.7.0-alpha.7", "0.7.0-alpha.8", "0.7.0-alpha.8.1", "0.7.0-alpha.8.2", "0.7.0-alpha.8.3", "0.7.0-alpha.8.4", "0.7.0-alpha.9", "0.7.0-alpha.9.1", "0.7.0-alpha.9.2", "0.7.0-alpha.9.3", "0.7.0-alpha.9.4", "0.7.0-beta.1", "0.7.0-beta.2", "0.7.0-beta.3", "0.7.0-beta.4", "0.7.0-beta.5", "0.7.0-beta.6", "0.7.0-beta.7", "0.7.0-beta.8", "0.7.0-beta.9", "0.7.0-beta.10", "0.7.0-beta.11", "0.7.0-beta.12", "0.7.0-rc.1", "0.7.0"):
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

    if version in ("0.7.0-alpha.2.4", "0.7.0-alpha.2.5", "0.7.0-alpha.2.6", "0.7.0-alpha.3", "0.7.0-alpha.3.1", "0.7.0-alpha.4", "0.7.0-alpha.5", "0.7.0-alpha.5.1", "0.7.0-alpha.5.2", "0.7.0-alpha.6", "0.7.0-alpha.7", "0.7.0-alpha.8", "0.7.0-alpha.8.1", "0.7.0-alpha.8.2", "0.7.0-alpha.8.3", "0.7.0-alpha.8.4", "0.7.0-alpha.9", "0.7.0-alpha.9.1", "0.7.0-alpha.9.2", "0.7.0-alpha.9.3", "0.7.0-alpha.9.4", "0.7.0-beta.1", "0.7.0-beta.2", "0.7.0-beta.3", "0.7.0-beta.4", "0.7.0-beta.5", "0.7.0-beta.6", "0.7.0-beta.7", "0.7.0-beta.8", "0.7.0-beta.9", "0.7.0-beta.10", "0.7.0-beta.11", "0.7.0-beta.12", "0.7.0-rc.1", "0.7.0"):
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

    if version in ("0.7.0-alpha.2.5", "0.7.0-alpha.2.6", "0.7.0-alpha.3", "0.7.0-alpha.3.1", "0.7.0-alpha.4", "0.7.0-alpha.5", "0.7.0-alpha.5.1", "0.7.0-alpha.5.2", "0.7.0-alpha.6", "0.7.0-alpha.7", "0.7.0-alpha.8", "0.7.0-alpha.8.1", "0.7.0-alpha.8.2", "0.7.0-alpha.8.3", "0.7.0-alpha.8.4", "0.7.0-alpha.9", "0.7.0-alpha.9.1", "0.7.0-alpha.9.2", "0.7.0-alpha.9.3", "0.7.0-alpha.9.4", "0.7.0-beta.1", "0.7.0-beta.2", "0.7.0-beta.3", "0.7.0-beta.4", "0.7.0-beta.5", "0.7.0-beta.6", "0.7.0-beta.7", "0.7.0-beta.8", "0.7.0-beta.9", "0.7.0-beta.10", "0.7.0-beta.11", "0.7.0-beta.12", "0.7.0-rc.1", "0.7.0"):
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
        if version in ("0.7.0-alpha.5.1", "0.7.0-alpha.5.2", "0.7.0-alpha.6", "0.7.0-alpha.7", "0.7.0-alpha.8", "0.7.0-alpha.8.1", "0.7.0-alpha.8.2", "0.7.0-alpha.8.3", "0.7.0-alpha.8.4", "0.7.0-alpha.9", "0.7.0-alpha.9.1", "0.7.0-alpha.9.2", "0.7.0-alpha.9.3", "0.7.0-alpha.9.4", "0.7.0-beta.1", "0.7.0-beta.2", "0.7.0-beta.3", "0.7.0-beta.4", "0.7.0-beta.5", "0.7.0-beta.6", "0.7.0-beta.7", "0.7.0-beta.8", "0.7.0-beta.9", "0.7.0-beta.10", "0.7.0-beta.11", "0.7.0-beta.12", "0.7.0-rc.1", "0.7.0"):
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
            7 if version in ("0.7.0-alpha.9", "0.7.0-alpha.9.1", "0.7.0-alpha.9.2", "0.7.0-alpha.9.3", "0.7.0-alpha.9.4", "0.7.0-beta.1", "0.7.0-beta.2", "0.7.0-beta.3", "0.7.0-beta.4", "0.7.0-beta.5", "0.7.0-beta.6", "0.7.0-beta.7", "0.7.0-beta.8", "0.7.0-beta.9", "0.7.0-beta.10", "0.7.0-beta.11", "0.7.0-beta.12", "0.7.0-rc.1", "0.7.0") else (
                6 if version in ("0.7.0-alpha.5.1", "0.7.0-alpha.5.2", "0.7.0-alpha.6", "0.7.0-alpha.7", "0.7.0-alpha.8", "0.7.0-alpha.8.1", "0.7.0-alpha.8.2", "0.7.0-alpha.8.3", "0.7.0-alpha.8.4") else 5
            )
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

    if version in ("0.7.0-alpha.4", "0.7.0-alpha.5", "0.7.0-alpha.5.1", "0.7.0-alpha.5.2", "0.7.0-alpha.6", "0.7.0-alpha.7", "0.7.0-alpha.8", "0.7.0-alpha.8.1", "0.7.0-alpha.8.2", "0.7.0-alpha.8.3", "0.7.0-alpha.8.4", "0.7.0-alpha.9", "0.7.0-alpha.9.1", "0.7.0-alpha.9.2", "0.7.0-alpha.9.3", "0.7.0-alpha.9.4", "0.7.0-beta.1", "0.7.0-beta.2", "0.7.0-beta.3", "0.7.0-beta.4", "0.7.0-beta.5", "0.7.0-beta.6", "0.7.0-beta.7", "0.7.0-beta.8", "0.7.0-beta.9", "0.7.0-beta.10", "0.7.0-beta.11", "0.7.0-beta.12", "0.7.0-rc.1", "0.7.0"):
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
            if version in ("0.7.0-alpha.5", "0.7.0-alpha.5.1", "0.7.0-alpha.5.2", "0.7.0-alpha.6", "0.7.0-alpha.7", "0.7.0-alpha.8", "0.7.0-alpha.8.1", "0.7.0-alpha.8.2", "0.7.0-alpha.8.3", "0.7.0-alpha.8.4", "0.7.0-alpha.9", "0.7.0-alpha.9.1", "0.7.0-alpha.9.2", "0.7.0-alpha.9.3", "0.7.0-alpha.9.4", "0.7.0-beta.1", "0.7.0-beta.2", "0.7.0-beta.3", "0.7.0-beta.4", "0.7.0-beta.5", "0.7.0-beta.6", "0.7.0-beta.7", "0.7.0-beta.8", "0.7.0-beta.9", "0.7.0-beta.10", "0.7.0-beta.11", "0.7.0-beta.12", "0.7.0-rc.1", "0.7.0")
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

    if version in ("0.7.0-alpha.5", "0.7.0-alpha.5.1", "0.7.0-alpha.5.2", "0.7.0-alpha.6", "0.7.0-alpha.7", "0.7.0-alpha.8", "0.7.0-alpha.8.1", "0.7.0-alpha.8.2", "0.7.0-alpha.8.3", "0.7.0-alpha.8.4", "0.7.0-alpha.9", "0.7.0-alpha.9.1", "0.7.0-alpha.9.2", "0.7.0-alpha.9.3", "0.7.0-alpha.9.4", "0.7.0-beta.1", "0.7.0-beta.2", "0.7.0-beta.3", "0.7.0-beta.4", "0.7.0-beta.5", "0.7.0-beta.6", "0.7.0-beta.7", "0.7.0-beta.8", "0.7.0-beta.9", "0.7.0-beta.10", "0.7.0-beta.11", "0.7.0-beta.12", "0.7.0-rc.1", "0.7.0"):
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
            7 if version in ("0.7.0-alpha.9", "0.7.0-alpha.9.1", "0.7.0-alpha.9.2", "0.7.0-alpha.9.3", "0.7.0-alpha.9.4", "0.7.0-beta.1", "0.7.0-beta.2", "0.7.0-beta.3", "0.7.0-beta.4", "0.7.0-beta.5", "0.7.0-beta.6", "0.7.0-beta.7", "0.7.0-beta.8", "0.7.0-beta.9", "0.7.0-beta.10", "0.7.0-beta.11", "0.7.0-beta.12", "0.7.0-rc.1", "0.7.0") else (
                6 if version in ("0.7.0-alpha.5.1", "0.7.0-alpha.5.2", "0.7.0-alpha.6", "0.7.0-alpha.7", "0.7.0-alpha.8", "0.7.0-alpha.8.1", "0.7.0-alpha.8.2", "0.7.0-alpha.8.3", "0.7.0-alpha.8.4") else 5
            )
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

    if version == "0.7.0-rc.1":
        rc_validation = read("docs/V0.7_RC_VALIDATION.md")
        package_verify = read("scripts/verify_package.ps1")
        build_workflow = read(".github/workflows/build.yml")
        release_workflow = read(".github/workflows/release.yml")
        update_policy_tests = read("tests/UpdatePolicyTests.cpp")
        readme = read("README.md")
        changelog = read("CHANGELOG.md")

        for token in (
            "Release Freeze & Upgrade Gate",
            "0.7.0-beta.12 < 0.7.0-rc.1 < 0.7.0",
            "settings.json",
            "schemaVersion remains 7",
            "commands.json",
            "schemaVersion remains 2",
            "provider-cache.json",
            "Shortcut TSV remains v3",
            "Provider-to-shortcut final regression",
            "Native update: beta.12 -> rc.1",
            "Managed Everything lifecycle freeze",
            "Native Uninstall freeze",
            "Package and release identity gate",
            "Stable promotion gate",
            "0.7.0.200",
        ):
            if token not in rc_validation:
                fail(f"v0.7 rc.1 validation gate missing: {token}")

        for source_name, source in (
            ("build workflow", build_workflow),
            ("tag workflow", release_workflow),
            ("package allowlist", package_verify),
        ):
            if "V0.7_RC_VALIDATION.md" not in source:
                fail(f"v0.7 rc.1 validation document missing from {source_name}")

        for token in (
            '"0.7.0-beta.12"',
            '"0.7.0-rc.1"',
            '"0.7.0"',
        ):
            if token not in update_policy_tests:
                fail(f"v0.7 rc.1 update ordering gate missing: {token}")

        for token in (
            "Release Freeze & Upgrade Gate",
            "V0.7_RC_VALIDATION.md",
            "0.7.0.200",
            "beta.12 < rc.1 < stable",
        ):
            if token not in readme:
                fail(f"v0.7 rc.1 README freeze gate missing: {token}")

        for token in (
            "0.7.0-rc.1",
            "V0.7_RC_VALIDATION.md",
            "0.7.0.200",
            "0.7.0-beta.12 < 0.7.0-rc.1 < 0.7.0",
        ):
            if token not in changelog:
                fail(f"v0.7 rc.1 changelog freeze gate missing: {token}")

    if version == "0.7.0":
        rc_validation = read("docs/V0.7_RC_VALIDATION.md")
        package_verify = read("scripts/verify_package.ps1")
        build_workflow = read(".github/workflows/build.yml")
        release_workflow = read(".github/workflows/release.yml")
        runtime_smoke = read("scripts/verify_runtime_smoke.ps1")
        update_policy_tests = read("tests/UpdatePolicyTests.cpp")
        settings_example = json.loads(read("config/settings.example.json"))
        readme = read("README.md")
        changelog = read("CHANGELOG.md")

        if settings_example.get("update") != {"autoCheck": True, "channel": "stable"}:
            fail("v0.7 stable settings example must default to the Stable update channel")

        for token in (
            "Native beta.12 -> rc.1 automatic update real-Windows validation: PASS",
            "0.7.0-beta.12 < 0.7.0-rc.1 < 0.7.0",
            "Stable promotion gate",
        ):
            if token not in rc_validation:
                fail(f"v0.7 stable RC validation evidence missing: {token}")

        for source_name, source in (
            ("build workflow", build_workflow),
            ("tag workflow", release_workflow),
            ("package allowlist", package_verify),
        ):
            if "V0.7_RC_VALIDATION.md" not in source:
                fail(f"v0.7 stable RC validation document missing from {source_name}")

        for token in (
            '$expectedUpdateChannel = if ($version -match \'-\') { "development" } else { "stable" }',
            "release-appropriate update defaults",
        ):
            if token not in runtime_smoke:
                fail(f"v0.7 stable runtime update-channel smoke missing: {token}")

        for token in (
            '"0.7.0-rc.1"',
            '"0.7.0"',
            'DefaultUpdateChannelForVersion(',
            'UpdateChannel::Stable',
        ):
            if token not in update_policy_tests:
                fail(f"v0.7 stable update policy gate missing: {token}")

        for token in (
            "### Stable v0.7.0",
            "/releases/tag/v0.7.0",
            "/releases/download/v0.7.0/ALTRunNext-x64.zip",
            "/releases/download/v0.7.0/ALTRunNext-ARM64.zip",
            "## v0.7.0 — Stable",
            "0.7.0.300",
        ):
            if token not in readme:
                fail(f"v0.7 stable README publication contract missing: {token}")

        for token in (
            "## 0.7.0",
            "0.7.0.300",
            "Stable update channel",
        ):
            if token not in changelog:
                fail(f"v0.7 stable changelog contract missing: {token}")

        for token in (
            'TAG="v$VERSION"',
            'if [[ "$VERSION" == *-* ]]; then',
            "EXTRA_ARGS+=(--prerelease)",
            'gh release create "$TAG"',
        ):
            if token not in build_workflow:
                fail(f"v0.7 stable publication workflow contract missing: {token}")

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
