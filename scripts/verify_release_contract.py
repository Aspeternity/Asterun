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
