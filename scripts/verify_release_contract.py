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







if version == "0.8.0-alpha.5.8":
    import hashlib
    import subprocess

    expected_schemas = {
        "kSettingsSchemaVersion": 10,
        "kCommandsSchemaVersion": 2,
        "kUsageSchemaVersion": 1,
    }
    for name, expected in expected_schemas.items():
        actual = cpp_int("src/core/ConfigIO.hpp", name)
        if actual != expected:
            fail(f"v0.8 alpha.5.8 {name}={actual}, expected {expected}")

    if cpp_int("src/core/ProviderCache.cpp", "kProviderCacheSchemaVersion") != 2:
        fail("v0.8 alpha.5.8 must keep provider-cache schemaVersion 2")

    settings_h = read("src/core/Settings.hpp")
    settings_cpp = read("src/core/Settings.cpp")
    settings_window_h = read("src/ui/SettingsWindow.hpp")
    settings_window_cpp = read("src/ui/SettingsWindow.cpp")
    shortcut_editor_cpp = read("src/ui/ShortcutEditorDialog.cpp")
    ui_combo_h = read("src/ui/UiComboBox.hpp")
    ui_combo_cpp = read("src/ui/UiComboBox.cpp")
    ui_list_h = read("src/ui/UiListView.hpp")
    ui_list_cpp = read("src/ui/UiListView.cpp")
    shortcut_manager_cpp = read("src/ui/ShortcutManagerWindow.cpp")
    path_converter_cpp = read("src/ui/ShortcutPathConverterDialog.cpp")
    cmake = read("CMakeLists.txt")
    app_h = read("src/app/App.hpp")
    app_cpp = read("src/app/App.cpp")
    launcher_cpp = read("src/ui/LauncherWindow.cpp")
    hotkey_cpp = read("src/core/HotkeyRegistry.cpp")
    hotkey_tests = read("tests/HotkeyRegistryTests.cpp")
    resources = read("src/resources.rc")
    manifest = read("src/app.manifest")
    update_tests = read("tests/UpdatePolicyTests.cpp")
    desktop_validation = read("docs/DESKTOP_VALIDATION.md")
    runtime_smoke = read("scripts/verify_runtime_smoke.ps1")

    removed_runtime_tokens = (
        "showOnStartup",
        "hideAfterLaunch",
        "clearQueryOnShow",
        "hideOnFocusLost",
        "wildcardMatching",
        "numericQuickLaunchOrder",
    )
    for path, text_value in (
        ("Settings.hpp", settings_h),
        ("SettingsWindow.hpp", settings_window_h),
        ("SettingsWindow.cpp", settings_window_cpp),
        ("App.hpp", app_h),
        ("App.cpp", app_cpp),
        ("LauncherWindow.cpp", launcher_cpp),
    ):
        for token in removed_runtime_tokens:
            if token in text_value:
                fail(
                    f"v0.8 alpha.5.8 removed pseudo-setting returned "
                    f"in {path}: {token}"
                )

    if "load.schemaVersion >= 10" not in settings_cpp:
        fail("v0.8 alpha.5.8 must keep schema-10 migration behavior")
    if '"startupBehavior"' not in settings_cpp or '"addToSendToMenu"' not in settings_cpp:
        fail("v0.8 alpha.5.8 must keep alpha.5.1 Settings fields")

    # One shared Next ComboBox implementation owns every product dropdown.
    if "src/ui/UiComboBox.cpp" not in cmake:
        fail("v0.8 alpha.5.8 shared UiComboBox source is not built")

    for token in (
        'L"COMBOBOX"',
        "CBS_DROPDOWNLIST",
        "CBS_OWNERDRAWFIXED",
        "SetWindowSubclass(",
        "NextComboSubclassProc",
        "DrawNextComboBoxSurface(",
        "DrawNextComboBoxItem(",
        "ApplyNextComboBoxMetrics(",
        "MeasureNextComboBoxPreferredWidth(",
        "GetTextExtentPoint32W(",
        "TME_LEAVE",
        "WM_MOUSELEAVE",
        "NextComboBoxItemHeight(",
        "ColorNextComboBoxList(",
    ):
        if token not in ui_combo_cpp:
            fail(f"v0.8 alpha.5.8 shared ComboBox implementation missing: {token}")

    for token in (
        "CreateNextComboBox(",
        "ApplyNextComboBoxMetrics(",
        "MeasureNextComboBoxPreferredWidth(",
        "NextComboBoxItemHeight(",
        "DrawNextComboBoxItem(",
        "ColorNextComboBoxList(",
    ):
        if token not in ui_combo_h:
            fail(f"v0.8 alpha.5.8 shared ComboBox API missing: {token}")

    for path, text_value in (
        ("SettingsWindow.cpp", settings_window_cpp),
        ("ShortcutEditorDialog.cpp", shortcut_editor_cpp),
    ):
        for forbidden in (
            'L"COMBOBOX"',
            "CBS_DROPDOWNLIST",
            "CreateThemedComboBox(",
            "ComboSubclassProc",
            "DrawComboSurface(",
        ):
            if forbidden in text_value:
                fail(
                    f"v0.8 alpha.5.8 {path} reintroduced a private/native "
                    f"ComboBox path: {forbidden}"
                )

    for control in (
        "startupBehavior_",
        "popupMonitor_",
        "launcherPlacement_",
        "settingsPlacement_",
        "shortcutManagerPlacement_",
        "uiStyle_",
        "language_",
    ):
        marker = f"{control} =\n        ui::CreateNextComboBox("
        if marker not in settings_window_cpp:
            fail(f"v0.8 alpha.5.8 Settings dropdown is not shared: {control}")

    for control in (
        "type_",
        "runtimeInput_",
    ):
        marker = f"{control} =\n        ui::CreateNextComboBox("
        if marker not in shortcut_editor_cpp:
            fail(f"v0.8 alpha.5.8 Shortcut Editor dropdown is not shared: {control}")

    for token in (
        "ui::DrawNextComboBoxItem(",
        "ui::ColorNextComboBoxList(",
        "ui::NextComboBoxItemHeight(",
        "ui::MeasureNextComboBoxPreferredWidth(",
    ):
        if token not in settings_window_cpp:
            fail(f"v0.8 alpha.5.8 Settings shared ComboBox route missing: {token}")
        if token not in shortcut_editor_cpp:
            fail(f"v0.8 alpha.5.8 Shortcut Editor shared ComboBox route missing: {token}")

    combo_surface = ui_combo_cpp.split(
        "void DrawNextComboBoxSurface(", 1
    )[1].split("LRESULT CALLBACK NextComboSubclassProc(", 1)[0]
    if "HPEN divider" in combo_surface or "arrowLeft" in combo_surface:
        fail("v0.8 alpha.5.8 must not restore a split ComboBox arrow cell")

    if "const int startupComboWidth = Scale(180)" in settings_window_cpp:
        fail("v0.8 alpha.5.8 Startup behavior returned to a fixed long width")
    if "const int comboWidth =\n            Scale(180);" in settings_window_cpp:
        fail("v0.8 alpha.5.8 placement ComboBoxes returned to one fixed width")

    # Shortcut Manager and Path Conversion share one Next Table Surface.
    if "src/ui/UiListView.cpp" not in cmake:
        fail("v0.8 alpha.5.8 shared UiListView source is not built")

    for token in (
        "InitializeNextListView(",
        "UpdateNextListResizeGuide(",
        "ClearNextListResizeGuide(",
        "NextListRowBackground(",
        "NextListRowText(",
        "NextListCellPadding(",
        "DrawNextListRowSeparator(",
    ):
        if token not in ui_list_h:
            fail(f"v0.8 alpha.5.8 shared ListView API missing: {token}")

    if "DrawNextListHeader(" in ui_list_h:
        fail("v0.8 alpha.5.8 must not expose the old parent NM_CUSTOMDRAW Header API")

    for token in (
        "NextListSubclassProc",
        "NextHeaderSubclassProc",
        "DrawHeaderSurface(",
        "DividerNearPoint(",
        "HDM_LAYOUT",
        "IDC_SIZEWE",
        "HDS_FULLDRAG",
        "NextResizeGuideSubclassProc",
        "EnsureResizeGuideWindow(",
        "PositionResizeGuide(",
        "WS_EX_NOPARENTNOTIFY",
        "HTTRANSPARENT",
        "SWP_SHOWWINDOW",
        "UpdateNextListResizeGuide(",
        "ClearNextListResizeGuide(",
        "resizeGuideX",
        "Scale(\n                        34,",
        "Scale(30, state.dpi)",
        "Scale(\n            12,",
        "SetWindowSubclass(",
        "ListView_HitTest(",
        "LVS_EX_FULLROWSELECT",
        "LVS_EX_DOUBLEBUFFER",
        'SetWindowTheme(\n            header,\n            L"",\n            L"")',
        "kTableHairline",
        "RoundRect(",
        "NextListRowBackground(",
        "DrawNextListRowSeparator(",
    ):
        if token not in ui_list_cpp:
            fail(f"v0.8 alpha.5.8 Table Surface implementation missing: {token}")

    if "DrawNextListHeader(" in ui_list_cpp:
        fail("v0.8 alpha.5.8 old Header NM_CUSTOMDRAW implementation returned")

    for forbidden in (
        "InvalidateGuideStrip(",
        "DrawResizeGuideOnList(",
        "SetResizeGuide(",
    ):
        if forbidden in ui_list_cpp:
            fail(f"v0.8 alpha.5.8 paint-based resize guide returned: {forbidden}")

    if (
        "headerStyle |=\n            static_cast<LONG_PTR>(\n                HDS_FULLDRAG)"
        not in ui_list_cpp
    ):
        fail("v0.8 alpha.5.8 Header must enable HDS_FULLDRAG to suppress native tracker")

    for path, text_value in (
        ("ShortcutManagerWindow.cpp", shortcut_manager_cpp),
        ("ShortcutPathConverterDialog.cpp", path_converter_cpp),
    ):
        for token in (
            "ui::InitializeNextListView(",
            "ui::UpdateNextListResizeGuide(",
            "ui::ClearNextListResizeGuide(",
            "ui::NextListRowBackground(",
            "ui::NextListCellPadding(",
            "ui::DrawNextListRowSeparator(",
            "HandleHeaderNotification(",
        ):
            if token not in text_value:
                fail(f"v0.8 alpha.5.8 {path} Table Surface route missing: {token}")
        if "ui::DrawNextListHeader(" in text_value:
            fail(f"v0.8 alpha.5.8 {path} still paints Header through parent NM_CUSTOMDRAW")

    if "WS_BORDER |\n            LVS_REPORT" in shortcut_manager_cpp:
        fail("v0.8 alpha.5.8 Shortcut Manager restored the old hard ListView border")

    for forbidden in (
        "RebuildRowHeightImageList",
        "rowHeightImageList_",
        "HandleHeaderCustomDraw(",
    ):
        if forbidden in shortcut_manager_cpp or forbidden in path_converter_cpp:
            fail(f"v0.8 alpha.5.8 duplicate ListView visual path returned: {forbidden}")

    if "GetSysColor(\n                  COLOR_HIGHLIGHTTEXT)" in path_converter_cpp:
        fail("v0.8 alpha.5.8 Path Conversion returned to native highlight text colors")

    if "RGB(251, 252, 253)" not in path_converter_cpp:
        fail("v0.8 alpha.5.8 Path Conversion section-band treatment missing")
    if "DrawResultFieldCell(" not in path_converter_cpp:
        fail("v0.8 alpha.5.8 Path Conversion custom checkbox renderer was removed")

    for token in (
        "kKeywordColumnDefaultLogical = 110",
        "kNameColumnDefaultLogical = 180",
        "kTypeColumnDefaultLogical = 110",
        "columnTracking_",
        "trackedColumnWidth_",
    ):
        if token not in shortcut_manager_cpp:
            fail(f"v0.8 alpha.5.8 Shortcut Manager interaction contract missing: {token}")

    for token in (
        "kFieldColumnDefaultLogical = 110",
        "kCurrentColumnDefaultLogical = 360",
        "kConvertedColumnDefaultLogical = 380",
        "ClampTrackedColumnWidth(",
        "columnTracking_",
        "trackedColumnWidth_",
    ):
        if token not in path_converter_cpp:
            fail(f"v0.8 alpha.5.8 Path Conversion interaction contract missing: {token}")

    # Hotkey actions scroll independently; the reset-all action stays fixed.
    for token in (
        "hotkeyScrollOffset_",
        "HotkeyScrollViewport()",
        "HotkeyContentBottom()",
        "UpdatePageScrollBar()",
        "ScrollCurrentPage(",
        "ClipHotkeyControlsToViewport()",
        "page_ == Page::Hotkeys",
        "WM_MOUSEWHEEL",
        "WM_VSCROLL",
        "hotkeyResetAll_",
        "client.bottom",
    ):
        if token not in settings_window_cpp and token not in settings_window_h:
            fail(f"v0.8 alpha.5.8 Hotkeys scrolling contract missing: {token}")
    if "UpdateGeneralScrollBar" in settings_window_cpp or "ScrollGeneral(" in settings_window_cpp:
        fail("v0.8 alpha.5.8 must not retain the General-only scroll implementation")

    for token in (
        "HotkeyControlDesiredVisible(",
        "const bool desiredVisible",
        "if (!desiredVisible)",
        "return row.statusVisible;",
        "return row.resetVisible;",
        "row.statusVisible &&",
        "auxiliaryHeight > 0",
    ):
        if token not in settings_window_cpp and token not in settings_window_h:
            fail(f"v0.8 alpha.5.8 Hotkeys visibility contract missing: {token}")

    clip_body = settings_window_cpp.split(
        "ClipHotkeyControlsToViewport()", 1
    )[1].split("RECT SettingsWindow::BehaviorCardRect()", 1)[0]
    if "HotkeyControlDesiredVisible(" not in clip_body:
        fail("v0.8 alpha.5.8 viewport clipping must consult business visibility")

    # Shortcut Manager must be a real global hotkey, not a Launcher-only chord.
    global_manager_descriptor = (
        'kOpenShortcutManager),HotkeyScope::Global,false,true,'
        '{true,{"alt"},"s"}'
    )
    if global_manager_descriptor not in hotkey_cpp:
        fail("v0.8 alpha.5.8 Shortcut Manager default must be global Alt+S")
    if "HotkeyScope::Global,\n            \"s\"" not in hotkey_tests:
        fail("v0.8 alpha.5.8 Hotkey Registry tests do not assert global Alt+S")

    for token in (
        "kShortcutManagerHotkeyId = 0xA173",
        "RebindShortcutManagerHotkey(",
        "shortcutManagerHotkeyRegistered_",
        "shortcutManagerHotkeyLastError_",
    ):
        if token not in app_h:
            fail(f"v0.8 alpha.5.8 App hotkey state missing: {token}")

    for token in (
        "RegisterHotKey(",
        "kShortcutManagerHotkeyId",
        "RebindShortcutManagerHotkey(",
        "ShowShortcutManager();",
        "UnregisterHotKey(",
        "oldShortcutManager",
        "previousShortcutManager",
    ):
        if token not in app_cpp:
            fail(f"v0.8 alpha.5.8 global Shortcut Manager lifecycle missing: {token}")

    # Exit remains an opt-in Launcher action; do not accidentally globalize it.
    if (
        'kExitApplication),HotkeyScope::Launcher,false,false,{false,{},"f12"}'
        not in hotkey_cpp
    ):
        fail("v0.8 alpha.5.8 Exit action scope/default changed unexpectedly")

    # Startup notification is concise and uses the current activation binding.
    for token in (
        'L"已在后台启动"',
        'L"Running in the background"',
        'L" 呼出"',
        "activationHotkey",
    ):
        if token not in launcher_cpp:
            fail(f"v0.8 alpha.5.8 startup notification copy missing: {token}")
    if 'L"ALTRun Next 已启动"' in launcher_cpp:
        fail("v0.8 alpha.5.8 duplicated old startup notification copy returned")
    if 'VALUE "FileDescription", "ALTRun Next\\0"' not in resources:
        fail("v0.8 alpha.5.8 executable FileDescription must be ALTRun Next")
    if "classic lightweight launcher" in resources:
        fail("v0.8 alpha.5.8 old notification source description returned")

    # Keep alpha.5.1 SendTo/runtime behavior intact.
    for token in (
        "FOLDERID_SendTo",
        "ForwardShortcutRequestsToExistingInstance",
        "ApplySendToRegistration(",
    ):
        if token not in app_cpp:
            fail(f"v0.8 alpha.5.8 alpha.5.1 integration regressed: {token}")
    for token in (
        "schemaVersion -ne 10",
        '"launcher.openShortcutManager"',
        '"launcher.exitApplication"',
    ):
        if token not in runtime_smoke:
            fail(f"v0.8 alpha.5.8 packaged runtime smoke regressed: {token}")

    subprocess.run(
        [
            sys.executable,
            str(ROOT / "scripts" / "generate_classic_hidpi_assets.py"),
            "--verify",
        ],
        cwd=ROOT,
        check=True,
    )

    def git_blob_sha(path: str) -> str:
        data = (ROOT / path).read_bytes()
        header = f"blob {len(data)}\0".encode("ascii")
        return hashlib.sha1(header + data).hexdigest()

    frozen_assets = {
        "src/resources/classic_bg.bmp":
            "bbced49d20184051cf8ad48b153b022cecfecd50",
        "src/resources/classic_shortcut.bmp":
            "eee00956b449975f05e63a38e4da4ea29e01c240",
        "src/resources/classic_close.bmp":
            "51732fb285d83c2f13437c26a5f923fec2c33d55",
        "src/resources/classic_shortcut_200.bmp":
            "e4ef3820d0c177575cd7b974dfcd6c3fd6c653e9",
        "src/resources/classic_close_200.bmp":
            "d872f63b63cf698a6477a31c76382e6dc0be98b1",
    }
    for path, expected in frozen_assets.items():
        if git_blob_sha(path) != expected:
            fail(f"v0.8 alpha.5.8 frozen Classic asset changed: {path}")

    for token in (
        "FILEVERSION 0,8,0,178",
        "PRODUCTVERSION 0,8,0,178",
        "0.8.0-alpha.5.8",
    ):
        if token not in resources:
            fail(f"v0.8 alpha.5.8 resource version missing: {token}")
    if 'version="0.8.0.178"' not in manifest:
        fail("v0.8 alpha.5.8 manifest fixed version must be 0.8.0.178")

    for token in (
        '"0.8.0-alpha.5.7"',
        '"0.8.0-alpha.5.8"',
        "UpdateChannel::Stable",
    ):
        if token not in update_tests:
            fail(f"v0.8 alpha.5.8 update-policy coverage missing: {token}")

    for token in (
        "v0.8.0-alpha.5.8 resize-guide artifact validation",
        "exactly one preview line is visible at any time",
        "never builds a blue block or vertical trail",
        "Native Header tracking feedback does not appear",
        "100/125/150/175/200%",
    ):
        if token not in desktop_validation:
            fail(f"v0.8 alpha.5.8 desktop checklist missing: {token}")

    readme = read("README.md")
    changelog = read("CHANGELOG.md")
    roadmap = read("ROADMAP.md")
    for token in (
        "v0.8.0-alpha.5.8 — Table Resize Guide Fix",
        "dedicated non-interactive 2-logical-pixel resize-guide child overlay",
        "enables `HDS_FULLDRAG` only to suppress the legacy native tracking line",
        "0.8.0.178",
    ):
        if token not in readme:
            fail(f"v0.8 alpha.5.8 README missing: {token}")
    if "## 0.8.0-alpha.5.8" not in changelog:
        fail("v0.8 alpha.5.8 changelog entry missing")
    if "v0.8.0-alpha.5.8 replaces the paint-based drag guide" not in roadmap:
        fail("v0.8 alpha.5.8 roadmap entry missing")

    print(
        "v0.8.0-alpha.5.8 Table Resize Guide Fix verified:",
        "| dedicated overlay guide replaces DC painting",
        "| native tracker suppressed without live width commits",
        "| one-shot elastic Target/Status commits retained",
        "| alpha.5.7 interaction + Table Surface retained",
        "| schema 10 + Classic assets frozen",
    )
    raise SystemExit(0)


if version == "0.8.0-alpha.5.7":
    import hashlib
    import subprocess

    expected_schemas = {
        "kSettingsSchemaVersion": 10,
        "kCommandsSchemaVersion": 2,
        "kUsageSchemaVersion": 1,
    }
    for name, expected in expected_schemas.items():
        actual = cpp_int("src/core/ConfigIO.hpp", name)
        if actual != expected:
            fail(f"v0.8 alpha.5.7 {name}={actual}, expected {expected}")

    if cpp_int("src/core/ProviderCache.cpp", "kProviderCacheSchemaVersion") != 2:
        fail("v0.8 alpha.5.7 must keep provider-cache schemaVersion 2")

    settings_h = read("src/core/Settings.hpp")
    settings_cpp = read("src/core/Settings.cpp")
    settings_window_h = read("src/ui/SettingsWindow.hpp")
    settings_window_cpp = read("src/ui/SettingsWindow.cpp")
    shortcut_editor_cpp = read("src/ui/ShortcutEditorDialog.cpp")
    ui_combo_h = read("src/ui/UiComboBox.hpp")
    ui_combo_cpp = read("src/ui/UiComboBox.cpp")
    ui_list_h = read("src/ui/UiListView.hpp")
    ui_list_cpp = read("src/ui/UiListView.cpp")
    shortcut_manager_cpp = read("src/ui/ShortcutManagerWindow.cpp")
    path_converter_cpp = read("src/ui/ShortcutPathConverterDialog.cpp")
    cmake = read("CMakeLists.txt")
    app_h = read("src/app/App.hpp")
    app_cpp = read("src/app/App.cpp")
    launcher_cpp = read("src/ui/LauncherWindow.cpp")
    hotkey_cpp = read("src/core/HotkeyRegistry.cpp")
    hotkey_tests = read("tests/HotkeyRegistryTests.cpp")
    resources = read("src/resources.rc")
    manifest = read("src/app.manifest")
    update_tests = read("tests/UpdatePolicyTests.cpp")
    desktop_validation = read("docs/DESKTOP_VALIDATION.md")
    runtime_smoke = read("scripts/verify_runtime_smoke.ps1")

    removed_runtime_tokens = (
        "showOnStartup",
        "hideAfterLaunch",
        "clearQueryOnShow",
        "hideOnFocusLost",
        "wildcardMatching",
        "numericQuickLaunchOrder",
    )
    for path, text_value in (
        ("Settings.hpp", settings_h),
        ("SettingsWindow.hpp", settings_window_h),
        ("SettingsWindow.cpp", settings_window_cpp),
        ("App.hpp", app_h),
        ("App.cpp", app_cpp),
        ("LauncherWindow.cpp", launcher_cpp),
    ):
        for token in removed_runtime_tokens:
            if token in text_value:
                fail(
                    f"v0.8 alpha.5.7 removed pseudo-setting returned "
                    f"in {path}: {token}"
                )

    if "load.schemaVersion >= 10" not in settings_cpp:
        fail("v0.8 alpha.5.7 must keep schema-10 migration behavior")
    if '"startupBehavior"' not in settings_cpp or '"addToSendToMenu"' not in settings_cpp:
        fail("v0.8 alpha.5.7 must keep alpha.5.1 Settings fields")

    # One shared Next ComboBox implementation owns every product dropdown.
    if "src/ui/UiComboBox.cpp" not in cmake:
        fail("v0.8 alpha.5.7 shared UiComboBox source is not built")

    for token in (
        'L"COMBOBOX"',
        "CBS_DROPDOWNLIST",
        "CBS_OWNERDRAWFIXED",
        "SetWindowSubclass(",
        "NextComboSubclassProc",
        "DrawNextComboBoxSurface(",
        "DrawNextComboBoxItem(",
        "ApplyNextComboBoxMetrics(",
        "MeasureNextComboBoxPreferredWidth(",
        "GetTextExtentPoint32W(",
        "TME_LEAVE",
        "WM_MOUSELEAVE",
        "NextComboBoxItemHeight(",
        "ColorNextComboBoxList(",
    ):
        if token not in ui_combo_cpp:
            fail(f"v0.8 alpha.5.7 shared ComboBox implementation missing: {token}")

    for token in (
        "CreateNextComboBox(",
        "ApplyNextComboBoxMetrics(",
        "MeasureNextComboBoxPreferredWidth(",
        "NextComboBoxItemHeight(",
        "DrawNextComboBoxItem(",
        "ColorNextComboBoxList(",
    ):
        if token not in ui_combo_h:
            fail(f"v0.8 alpha.5.7 shared ComboBox API missing: {token}")

    for path, text_value in (
        ("SettingsWindow.cpp", settings_window_cpp),
        ("ShortcutEditorDialog.cpp", shortcut_editor_cpp),
    ):
        for forbidden in (
            'L"COMBOBOX"',
            "CBS_DROPDOWNLIST",
            "CreateThemedComboBox(",
            "ComboSubclassProc",
            "DrawComboSurface(",
        ):
            if forbidden in text_value:
                fail(
                    f"v0.8 alpha.5.7 {path} reintroduced a private/native "
                    f"ComboBox path: {forbidden}"
                )

    for control in (
        "startupBehavior_",
        "popupMonitor_",
        "launcherPlacement_",
        "settingsPlacement_",
        "shortcutManagerPlacement_",
        "uiStyle_",
        "language_",
    ):
        marker = f"{control} =\n        ui::CreateNextComboBox("
        if marker not in settings_window_cpp:
            fail(f"v0.8 alpha.5.7 Settings dropdown is not shared: {control}")

    for control in (
        "type_",
        "runtimeInput_",
    ):
        marker = f"{control} =\n        ui::CreateNextComboBox("
        if marker not in shortcut_editor_cpp:
            fail(f"v0.8 alpha.5.7 Shortcut Editor dropdown is not shared: {control}")

    for token in (
        "ui::DrawNextComboBoxItem(",
        "ui::ColorNextComboBoxList(",
        "ui::NextComboBoxItemHeight(",
        "ui::MeasureNextComboBoxPreferredWidth(",
    ):
        if token not in settings_window_cpp:
            fail(f"v0.8 alpha.5.7 Settings shared ComboBox route missing: {token}")
        if token not in shortcut_editor_cpp:
            fail(f"v0.8 alpha.5.7 Shortcut Editor shared ComboBox route missing: {token}")

    combo_surface = ui_combo_cpp.split(
        "void DrawNextComboBoxSurface(", 1
    )[1].split("LRESULT CALLBACK NextComboSubclassProc(", 1)[0]
    if "HPEN divider" in combo_surface or "arrowLeft" in combo_surface:
        fail("v0.8 alpha.5.7 must not restore a split ComboBox arrow cell")

    if "const int startupComboWidth = Scale(180)" in settings_window_cpp:
        fail("v0.8 alpha.5.7 Startup behavior returned to a fixed long width")
    if "const int comboWidth =\n            Scale(180);" in settings_window_cpp:
        fail("v0.8 alpha.5.7 placement ComboBoxes returned to one fixed width")

    # Shortcut Manager and Path Conversion share one Next Table Surface.
    if "src/ui/UiListView.cpp" not in cmake:
        fail("v0.8 alpha.5.7 shared UiListView source is not built")

    for token in (
        "InitializeNextListView(",
        "UpdateNextListResizeGuide(",
        "ClearNextListResizeGuide(",
        "NextListRowBackground(",
        "NextListRowText(",
        "NextListCellPadding(",
        "DrawNextListRowSeparator(",
    ):
        if token not in ui_list_h:
            fail(f"v0.8 alpha.5.7 shared ListView API missing: {token}")

    if "DrawNextListHeader(" in ui_list_h:
        fail("v0.8 alpha.5.7 must not expose the old parent NM_CUSTOMDRAW Header API")

    for token in (
        "NextListSubclassProc",
        "NextHeaderSubclassProc",
        "DrawHeaderSurface(",
        "DividerNearPoint(",
        "HDM_LAYOUT",
        "IDC_SIZEWE",
        "HDS_FULLDRAG",
        "UpdateNextListResizeGuide(",
        "ClearNextListResizeGuide(",
        "resizeGuideX",
        "Scale(\n                        34,",
        "Scale(30, state.dpi)",
        "Scale(\n            12,",
        "SetWindowSubclass(",
        "ListView_HitTest(",
        "LVS_EX_FULLROWSELECT",
        "LVS_EX_DOUBLEBUFFER",
        'SetWindowTheme(\n            header,\n            L"",\n            L"")',
        "kTableHairline",
        "RoundRect(",
        "NextListRowBackground(",
        "DrawNextListRowSeparator(",
    ):
        if token not in ui_list_cpp:
            fail(f"v0.8 alpha.5.7 Table Surface implementation missing: {token}")

    if "DrawNextListHeader(" in ui_list_cpp:
        fail("v0.8 alpha.5.7 old Header NM_CUSTOMDRAW implementation returned")

    for path, text_value in (
        ("ShortcutManagerWindow.cpp", shortcut_manager_cpp),
        ("ShortcutPathConverterDialog.cpp", path_converter_cpp),
    ):
        for token in (
            "ui::InitializeNextListView(",
            "ui::UpdateNextListResizeGuide(",
            "ui::ClearNextListResizeGuide(",
            "ui::NextListRowBackground(",
            "ui::NextListCellPadding(",
            "ui::DrawNextListRowSeparator(",
            "HandleHeaderNotification(",
        ):
            if token not in text_value:
                fail(f"v0.8 alpha.5.7 {path} Table Surface route missing: {token}")
        if "ui::DrawNextListHeader(" in text_value:
            fail(f"v0.8 alpha.5.7 {path} still paints Header through parent NM_CUSTOMDRAW")

    if "WS_BORDER |\n            LVS_REPORT" in shortcut_manager_cpp:
        fail("v0.8 alpha.5.7 Shortcut Manager restored the old hard ListView border")

    for forbidden in (
        "RebuildRowHeightImageList",
        "rowHeightImageList_",
        "HandleHeaderCustomDraw(",
    ):
        if forbidden in shortcut_manager_cpp or forbidden in path_converter_cpp:
            fail(f"v0.8 alpha.5.7 duplicate ListView visual path returned: {forbidden}")

    if "GetSysColor(\n                  COLOR_HIGHLIGHTTEXT)" in path_converter_cpp:
        fail("v0.8 alpha.5.7 Path Conversion returned to native highlight text colors")

    if "RGB(251, 252, 253)" not in path_converter_cpp:
        fail("v0.8 alpha.5.7 Path Conversion section-band treatment missing")
    if "DrawResultFieldCell(" not in path_converter_cpp:
        fail("v0.8 alpha.5.7 Path Conversion custom checkbox renderer was removed")

    for token in (
        "kKeywordColumnDefaultLogical = 110",
        "kNameColumnDefaultLogical = 180",
        "kTypeColumnDefaultLogical = 110",
        "columnTracking_",
        "trackedColumnWidth_",
    ):
        if token not in shortcut_manager_cpp:
            fail(f"v0.8 alpha.5.7 Shortcut Manager interaction contract missing: {token}")

    for token in (
        "kFieldColumnDefaultLogical = 110",
        "kCurrentColumnDefaultLogical = 360",
        "kConvertedColumnDefaultLogical = 380",
        "ClampTrackedColumnWidth(",
        "columnTracking_",
        "trackedColumnWidth_",
    ):
        if token not in path_converter_cpp:
            fail(f"v0.8 alpha.5.7 Path Conversion interaction contract missing: {token}")

    # Hotkey actions scroll independently; the reset-all action stays fixed.
    for token in (
        "hotkeyScrollOffset_",
        "HotkeyScrollViewport()",
        "HotkeyContentBottom()",
        "UpdatePageScrollBar()",
        "ScrollCurrentPage(",
        "ClipHotkeyControlsToViewport()",
        "page_ == Page::Hotkeys",
        "WM_MOUSEWHEEL",
        "WM_VSCROLL",
        "hotkeyResetAll_",
        "client.bottom",
    ):
        if token not in settings_window_cpp and token not in settings_window_h:
            fail(f"v0.8 alpha.5.7 Hotkeys scrolling contract missing: {token}")
    if "UpdateGeneralScrollBar" in settings_window_cpp or "ScrollGeneral(" in settings_window_cpp:
        fail("v0.8 alpha.5.7 must not retain the General-only scroll implementation")

    for token in (
        "HotkeyControlDesiredVisible(",
        "const bool desiredVisible",
        "if (!desiredVisible)",
        "return row.statusVisible;",
        "return row.resetVisible;",
        "row.statusVisible &&",
        "auxiliaryHeight > 0",
    ):
        if token not in settings_window_cpp and token not in settings_window_h:
            fail(f"v0.8 alpha.5.7 Hotkeys visibility contract missing: {token}")

    clip_body = settings_window_cpp.split(
        "ClipHotkeyControlsToViewport()", 1
    )[1].split("RECT SettingsWindow::BehaviorCardRect()", 1)[0]
    if "HotkeyControlDesiredVisible(" not in clip_body:
        fail("v0.8 alpha.5.7 viewport clipping must consult business visibility")

    # Shortcut Manager must be a real global hotkey, not a Launcher-only chord.
    global_manager_descriptor = (
        'kOpenShortcutManager),HotkeyScope::Global,false,true,'
        '{true,{"alt"},"s"}'
    )
    if global_manager_descriptor not in hotkey_cpp:
        fail("v0.8 alpha.5.7 Shortcut Manager default must be global Alt+S")
    if "HotkeyScope::Global,\n            \"s\"" not in hotkey_tests:
        fail("v0.8 alpha.5.7 Hotkey Registry tests do not assert global Alt+S")

    for token in (
        "kShortcutManagerHotkeyId = 0xA173",
        "RebindShortcutManagerHotkey(",
        "shortcutManagerHotkeyRegistered_",
        "shortcutManagerHotkeyLastError_",
    ):
        if token not in app_h:
            fail(f"v0.8 alpha.5.7 App hotkey state missing: {token}")

    for token in (
        "RegisterHotKey(",
        "kShortcutManagerHotkeyId",
        "RebindShortcutManagerHotkey(",
        "ShowShortcutManager();",
        "UnregisterHotKey(",
        "oldShortcutManager",
        "previousShortcutManager",
    ):
        if token not in app_cpp:
            fail(f"v0.8 alpha.5.7 global Shortcut Manager lifecycle missing: {token}")

    # Exit remains an opt-in Launcher action; do not accidentally globalize it.
    if (
        'kExitApplication),HotkeyScope::Launcher,false,false,{false,{},"f12"}'
        not in hotkey_cpp
    ):
        fail("v0.8 alpha.5.7 Exit action scope/default changed unexpectedly")

    # Startup notification is concise and uses the current activation binding.
    for token in (
        'L"已在后台启动"',
        'L"Running in the background"',
        'L" 呼出"',
        "activationHotkey",
    ):
        if token not in launcher_cpp:
            fail(f"v0.8 alpha.5.7 startup notification copy missing: {token}")
    if 'L"ALTRun Next 已启动"' in launcher_cpp:
        fail("v0.8 alpha.5.7 duplicated old startup notification copy returned")
    if 'VALUE "FileDescription", "ALTRun Next\\0"' not in resources:
        fail("v0.8 alpha.5.7 executable FileDescription must be ALTRun Next")
    if "classic lightweight launcher" in resources:
        fail("v0.8 alpha.5.7 old notification source description returned")

    # Keep alpha.5.1 SendTo/runtime behavior intact.
    for token in (
        "FOLDERID_SendTo",
        "ForwardShortcutRequestsToExistingInstance",
        "ApplySendToRegistration(",
    ):
        if token not in app_cpp:
            fail(f"v0.8 alpha.5.7 alpha.5.1 integration regressed: {token}")
    for token in (
        "schemaVersion -ne 10",
        '"launcher.openShortcutManager"',
        '"launcher.exitApplication"',
    ):
        if token not in runtime_smoke:
            fail(f"v0.8 alpha.5.7 packaged runtime smoke regressed: {token}")

    subprocess.run(
        [
            sys.executable,
            str(ROOT / "scripts" / "generate_classic_hidpi_assets.py"),
            "--verify",
        ],
        cwd=ROOT,
        check=True,
    )

    def git_blob_sha(path: str) -> str:
        data = (ROOT / path).read_bytes()
        header = f"blob {len(data)}\0".encode("ascii")
        return hashlib.sha1(header + data).hexdigest()

    frozen_assets = {
        "src/resources/classic_bg.bmp":
            "bbced49d20184051cf8ad48b153b022cecfecd50",
        "src/resources/classic_shortcut.bmp":
            "eee00956b449975f05e63a38e4da4ea29e01c240",
        "src/resources/classic_close.bmp":
            "51732fb285d83c2f13437c26a5f923fec2c33d55",
        "src/resources/classic_shortcut_200.bmp":
            "e4ef3820d0c177575cd7b974dfcd6c3fd6c653e9",
        "src/resources/classic_close_200.bmp":
            "d872f63b63cf698a6477a31c76382e6dc0be98b1",
    }
    for path, expected in frozen_assets.items():
        if git_blob_sha(path) != expected:
            fail(f"v0.8 alpha.5.7 frozen Classic asset changed: {path}")

    for token in (
        "FILEVERSION 0,8,0,177",
        "PRODUCTVERSION 0,8,0,177",
        "0.8.0-alpha.5.7",
    ):
        if token not in resources:
            fail(f"v0.8 alpha.5.7 resource version missing: {token}")
    if 'version="0.8.0.177"' not in manifest:
        fail("v0.8 alpha.5.7 manifest fixed version must be 0.8.0.177")

    for token in (
        '"0.8.0-alpha.5.6"',
        '"0.8.0-alpha.5.7"',
        "UpdateChannel::Stable",
    ):
        if token not in update_tests:
            fail(f"v0.8 alpha.5.7 update-policy coverage missing: {token}")

    for token in (
        "v0.8.0-alpha.5.7 Table interaction validation",
        "resize cursor appears within an easy-to-hit",
        "only the shared vertical preview guide moves",
        "Status remains elastic/non-resizable",
        "100/125/150/175/200%",
    ):
        if token not in desktop_validation:
            fail(f"v0.8 alpha.5.7 desktop checklist missing: {token}")

    readme = read("README.md")
    changelog = read("CHANGELOG.md")
    roadmap = read("ROADMAP.md")
    for token in (
        "v0.8.0-alpha.5.7 — Table Interaction Polish",
        "±4 logical-pixel area",
        "deferred by removing `HDS_FULLDRAG`",
        "0.8.0.177",
    ):
        if token not in readme:
            fail(f"v0.8 alpha.5.7 README missing: {token}")
    if "## 0.8.0-alpha.5.7" not in changelog:
        fail("v0.8 alpha.5.7 changelog entry missing")
    if "v0.8.0-alpha.5.7 polishes Table interaction" not in roadmap:
        fail("v0.8 alpha.5.7 roadmap entry missing")

    print(
        "v0.8.0-alpha.5.7 Table Interaction Polish verified:",
        "| expanded divider hit zones + resize cursor",
        "| shared deferred drag guide",
        "| one-shot elastic Target/Status commits",
        "| alpha.5.6 Table Surface + ComboBox + Hotkeys retained",
        "| schema 10 + Classic assets frozen",
    )
    raise SystemExit(0)


if version == "0.8.0-alpha.5.6":
    import hashlib
    import subprocess

    expected_schemas = {
        "kSettingsSchemaVersion": 10,
        "kCommandsSchemaVersion": 2,
        "kUsageSchemaVersion": 1,
    }
    for name, expected in expected_schemas.items():
        actual = cpp_int("src/core/ConfigIO.hpp", name)
        if actual != expected:
            fail(f"v0.8 alpha.5.6 {name}={actual}, expected {expected}")

    if cpp_int("src/core/ProviderCache.cpp", "kProviderCacheSchemaVersion") != 2:
        fail("v0.8 alpha.5.6 must keep provider-cache schemaVersion 2")

    settings_h = read("src/core/Settings.hpp")
    settings_cpp = read("src/core/Settings.cpp")
    settings_window_h = read("src/ui/SettingsWindow.hpp")
    settings_window_cpp = read("src/ui/SettingsWindow.cpp")
    shortcut_editor_cpp = read("src/ui/ShortcutEditorDialog.cpp")
    ui_combo_h = read("src/ui/UiComboBox.hpp")
    ui_combo_cpp = read("src/ui/UiComboBox.cpp")
    ui_list_h = read("src/ui/UiListView.hpp")
    ui_list_cpp = read("src/ui/UiListView.cpp")
    shortcut_manager_cpp = read("src/ui/ShortcutManagerWindow.cpp")
    path_converter_cpp = read("src/ui/ShortcutPathConverterDialog.cpp")
    cmake = read("CMakeLists.txt")
    app_h = read("src/app/App.hpp")
    app_cpp = read("src/app/App.cpp")
    launcher_cpp = read("src/ui/LauncherWindow.cpp")
    hotkey_cpp = read("src/core/HotkeyRegistry.cpp")
    hotkey_tests = read("tests/HotkeyRegistryTests.cpp")
    resources = read("src/resources.rc")
    manifest = read("src/app.manifest")
    update_tests = read("tests/UpdatePolicyTests.cpp")
    desktop_validation = read("docs/DESKTOP_VALIDATION.md")
    runtime_smoke = read("scripts/verify_runtime_smoke.ps1")

    removed_runtime_tokens = (
        "showOnStartup",
        "hideAfterLaunch",
        "clearQueryOnShow",
        "hideOnFocusLost",
        "wildcardMatching",
        "numericQuickLaunchOrder",
    )
    for path, text_value in (
        ("Settings.hpp", settings_h),
        ("SettingsWindow.hpp", settings_window_h),
        ("SettingsWindow.cpp", settings_window_cpp),
        ("App.hpp", app_h),
        ("App.cpp", app_cpp),
        ("LauncherWindow.cpp", launcher_cpp),
    ):
        for token in removed_runtime_tokens:
            if token in text_value:
                fail(
                    f"v0.8 alpha.5.6 removed pseudo-setting returned "
                    f"in {path}: {token}"
                )

    if "load.schemaVersion >= 10" not in settings_cpp:
        fail("v0.8 alpha.5.6 must keep schema-10 migration behavior")
    if '"startupBehavior"' not in settings_cpp or '"addToSendToMenu"' not in settings_cpp:
        fail("v0.8 alpha.5.6 must keep alpha.5.1 Settings fields")

    # One shared Next ComboBox implementation owns every product dropdown.
    if "src/ui/UiComboBox.cpp" not in cmake:
        fail("v0.8 alpha.5.6 shared UiComboBox source is not built")

    for token in (
        'L"COMBOBOX"',
        "CBS_DROPDOWNLIST",
        "CBS_OWNERDRAWFIXED",
        "SetWindowSubclass(",
        "NextComboSubclassProc",
        "DrawNextComboBoxSurface(",
        "DrawNextComboBoxItem(",
        "ApplyNextComboBoxMetrics(",
        "MeasureNextComboBoxPreferredWidth(",
        "GetTextExtentPoint32W(",
        "TME_LEAVE",
        "WM_MOUSELEAVE",
        "NextComboBoxItemHeight(",
        "ColorNextComboBoxList(",
    ):
        if token not in ui_combo_cpp:
            fail(f"v0.8 alpha.5.6 shared ComboBox implementation missing: {token}")

    for token in (
        "CreateNextComboBox(",
        "ApplyNextComboBoxMetrics(",
        "MeasureNextComboBoxPreferredWidth(",
        "NextComboBoxItemHeight(",
        "DrawNextComboBoxItem(",
        "ColorNextComboBoxList(",
    ):
        if token not in ui_combo_h:
            fail(f"v0.8 alpha.5.6 shared ComboBox API missing: {token}")

    for path, text_value in (
        ("SettingsWindow.cpp", settings_window_cpp),
        ("ShortcutEditorDialog.cpp", shortcut_editor_cpp),
    ):
        for forbidden in (
            'L"COMBOBOX"',
            "CBS_DROPDOWNLIST",
            "CreateThemedComboBox(",
            "ComboSubclassProc",
            "DrawComboSurface(",
        ):
            if forbidden in text_value:
                fail(
                    f"v0.8 alpha.5.6 {path} reintroduced a private/native "
                    f"ComboBox path: {forbidden}"
                )

    for control in (
        "startupBehavior_",
        "popupMonitor_",
        "launcherPlacement_",
        "settingsPlacement_",
        "shortcutManagerPlacement_",
        "uiStyle_",
        "language_",
    ):
        marker = f"{control} =\n        ui::CreateNextComboBox("
        if marker not in settings_window_cpp:
            fail(f"v0.8 alpha.5.6 Settings dropdown is not shared: {control}")

    for control in (
        "type_",
        "runtimeInput_",
    ):
        marker = f"{control} =\n        ui::CreateNextComboBox("
        if marker not in shortcut_editor_cpp:
            fail(f"v0.8 alpha.5.6 Shortcut Editor dropdown is not shared: {control}")

    for token in (
        "ui::DrawNextComboBoxItem(",
        "ui::ColorNextComboBoxList(",
        "ui::NextComboBoxItemHeight(",
        "ui::MeasureNextComboBoxPreferredWidth(",
    ):
        if token not in settings_window_cpp:
            fail(f"v0.8 alpha.5.6 Settings shared ComboBox route missing: {token}")
        if token not in shortcut_editor_cpp:
            fail(f"v0.8 alpha.5.6 Shortcut Editor shared ComboBox route missing: {token}")

    combo_surface = ui_combo_cpp.split(
        "void DrawNextComboBoxSurface(", 1
    )[1].split("LRESULT CALLBACK NextComboSubclassProc(", 1)[0]
    if "HPEN divider" in combo_surface or "arrowLeft" in combo_surface:
        fail("v0.8 alpha.5.6 must not restore a split ComboBox arrow cell")

    if "const int startupComboWidth = Scale(180)" in settings_window_cpp:
        fail("v0.8 alpha.5.6 Startup behavior returned to a fixed long width")
    if "const int comboWidth =\n            Scale(180);" in settings_window_cpp:
        fail("v0.8 alpha.5.6 placement ComboBoxes returned to one fixed width")

    # Shortcut Manager and Path Conversion share one Next Table Surface.
    if "src/ui/UiListView.cpp" not in cmake:
        fail("v0.8 alpha.5.6 shared UiListView source is not built")

    for token in (
        "InitializeNextListView(",
        "NextListRowBackground(",
        "NextListRowText(",
        "NextListCellPadding(",
        "DrawNextListRowSeparator(",
    ):
        if token not in ui_list_h:
            fail(f"v0.8 alpha.5.6 shared ListView API missing: {token}")

    if "DrawNextListHeader(" in ui_list_h:
        fail("v0.8 alpha.5.6 must not expose the old parent NM_CUSTOMDRAW Header API")

    for token in (
        "NextListSubclassProc",
        "NextHeaderSubclassProc",
        "DrawHeaderSurface(",
        "DividerAtPoint(",
        "HDM_LAYOUT",
        "HHT_ONDIVIDER",
        "HHT_ONDIVOPEN",
        "Scale(\n                        34,",
        "Scale(30, state.dpi)",
        "Scale(\n            12,",
        "SetWindowSubclass(",
        "ListView_HitTest(",
        "LVS_EX_FULLROWSELECT",
        "LVS_EX_DOUBLEBUFFER",
        'SetWindowTheme(\n            header,\n            L"",\n            L"")',
        "kTableHairline",
        "RoundRect(",
        "NextListRowBackground(",
        "DrawNextListRowSeparator(",
    ):
        if token not in ui_list_cpp:
            fail(f"v0.8 alpha.5.6 Table Surface implementation missing: {token}")

    if "DrawNextListHeader(" in ui_list_cpp:
        fail("v0.8 alpha.5.6 old Header NM_CUSTOMDRAW implementation returned")

    for path, text_value in (
        ("ShortcutManagerWindow.cpp", shortcut_manager_cpp),
        ("ShortcutPathConverterDialog.cpp", path_converter_cpp),
    ):
        for token in (
            "ui::InitializeNextListView(",
            "ui::NextListRowBackground(",
            "ui::NextListCellPadding(",
            "ui::DrawNextListRowSeparator(",
            "HandleHeaderNotification(",
        ):
            if token not in text_value:
                fail(f"v0.8 alpha.5.6 {path} Table Surface route missing: {token}")
        if "ui::DrawNextListHeader(" in text_value:
            fail(f"v0.8 alpha.5.6 {path} still paints Header through parent NM_CUSTOMDRAW")

    if "WS_BORDER |\n            LVS_REPORT" in shortcut_manager_cpp:
        fail("v0.8 alpha.5.6 Shortcut Manager restored the old hard ListView border")

    for forbidden in (
        "RebuildRowHeightImageList",
        "rowHeightImageList_",
        "HandleHeaderCustomDraw(",
    ):
        if forbidden in shortcut_manager_cpp or forbidden in path_converter_cpp:
            fail(f"v0.8 alpha.5.6 duplicate ListView visual path returned: {forbidden}")

    if "GetSysColor(\n                  COLOR_HIGHLIGHTTEXT)" in path_converter_cpp:
        fail("v0.8 alpha.5.6 Path Conversion returned to native highlight text colors")

    if "RGB(251, 252, 253)" not in path_converter_cpp:
        fail("v0.8 alpha.5.6 Path Conversion section-band treatment missing")
    if "DrawResultFieldCell(" not in path_converter_cpp:
        fail("v0.8 alpha.5.6 Path Conversion custom checkbox renderer was removed")

    # Hotkey actions scroll independently; the reset-all action stays fixed.
    for token in (
        "hotkeyScrollOffset_",
        "HotkeyScrollViewport()",
        "HotkeyContentBottom()",
        "UpdatePageScrollBar()",
        "ScrollCurrentPage(",
        "ClipHotkeyControlsToViewport()",
        "page_ == Page::Hotkeys",
        "WM_MOUSEWHEEL",
        "WM_VSCROLL",
        "hotkeyResetAll_",
        "client.bottom",
    ):
        if token not in settings_window_cpp and token not in settings_window_h:
            fail(f"v0.8 alpha.5.6 Hotkeys scrolling contract missing: {token}")
    if "UpdateGeneralScrollBar" in settings_window_cpp or "ScrollGeneral(" in settings_window_cpp:
        fail("v0.8 alpha.5.6 must not retain the General-only scroll implementation")

    for token in (
        "HotkeyControlDesiredVisible(",
        "const bool desiredVisible",
        "if (!desiredVisible)",
        "return row.statusVisible;",
        "return row.resetVisible;",
        "row.statusVisible &&",
        "auxiliaryHeight > 0",
    ):
        if token not in settings_window_cpp and token not in settings_window_h:
            fail(f"v0.8 alpha.5.6 Hotkeys visibility contract missing: {token}")

    clip_body = settings_window_cpp.split(
        "ClipHotkeyControlsToViewport()", 1
    )[1].split("RECT SettingsWindow::BehaviorCardRect()", 1)[0]
    if "HotkeyControlDesiredVisible(" not in clip_body:
        fail("v0.8 alpha.5.6 viewport clipping must consult business visibility")

    # Shortcut Manager must be a real global hotkey, not a Launcher-only chord.
    global_manager_descriptor = (
        'kOpenShortcutManager),HotkeyScope::Global,false,true,'
        '{true,{"alt"},"s"}'
    )
    if global_manager_descriptor not in hotkey_cpp:
        fail("v0.8 alpha.5.6 Shortcut Manager default must be global Alt+S")
    if "HotkeyScope::Global,\n            \"s\"" not in hotkey_tests:
        fail("v0.8 alpha.5.6 Hotkey Registry tests do not assert global Alt+S")

    for token in (
        "kShortcutManagerHotkeyId = 0xA173",
        "RebindShortcutManagerHotkey(",
        "shortcutManagerHotkeyRegistered_",
        "shortcutManagerHotkeyLastError_",
    ):
        if token not in app_h:
            fail(f"v0.8 alpha.5.6 App hotkey state missing: {token}")

    for token in (
        "RegisterHotKey(",
        "kShortcutManagerHotkeyId",
        "RebindShortcutManagerHotkey(",
        "ShowShortcutManager();",
        "UnregisterHotKey(",
        "oldShortcutManager",
        "previousShortcutManager",
    ):
        if token not in app_cpp:
            fail(f"v0.8 alpha.5.6 global Shortcut Manager lifecycle missing: {token}")

    # Exit remains an opt-in Launcher action; do not accidentally globalize it.
    if (
        'kExitApplication),HotkeyScope::Launcher,false,false,{false,{},"f12"}'
        not in hotkey_cpp
    ):
        fail("v0.8 alpha.5.6 Exit action scope/default changed unexpectedly")

    # Startup notification is concise and uses the current activation binding.
    for token in (
        'L"已在后台启动"',
        'L"Running in the background"',
        'L" 呼出"',
        "activationHotkey",
    ):
        if token not in launcher_cpp:
            fail(f"v0.8 alpha.5.6 startup notification copy missing: {token}")
    if 'L"ALTRun Next 已启动"' in launcher_cpp:
        fail("v0.8 alpha.5.6 duplicated old startup notification copy returned")
    if 'VALUE "FileDescription", "ALTRun Next\\0"' not in resources:
        fail("v0.8 alpha.5.6 executable FileDescription must be ALTRun Next")
    if "classic lightweight launcher" in resources:
        fail("v0.8 alpha.5.6 old notification source description returned")

    # Keep alpha.5.1 SendTo/runtime behavior intact.
    for token in (
        "FOLDERID_SendTo",
        "ForwardShortcutRequestsToExistingInstance",
        "ApplySendToRegistration(",
    ):
        if token not in app_cpp:
            fail(f"v0.8 alpha.5.6 alpha.5.1 integration regressed: {token}")
    for token in (
        "schemaVersion -ne 10",
        '"launcher.openShortcutManager"',
        '"launcher.exitApplication"',
    ):
        if token not in runtime_smoke:
            fail(f"v0.8 alpha.5.6 packaged runtime smoke regressed: {token}")

    subprocess.run(
        [
            sys.executable,
            str(ROOT / "scripts" / "generate_classic_hidpi_assets.py"),
            "--verify",
        ],
        cwd=ROOT,
        check=True,
    )

    def git_blob_sha(path: str) -> str:
        data = (ROOT / path).read_bytes()
        header = f"blob {len(data)}\0".encode("ascii")
        return hashlib.sha1(header + data).hexdigest()

    frozen_assets = {
        "src/resources/classic_bg.bmp":
            "bbced49d20184051cf8ad48b153b022cecfecd50",
        "src/resources/classic_shortcut.bmp":
            "eee00956b449975f05e63a38e4da4ea29e01c240",
        "src/resources/classic_close.bmp":
            "51732fb285d83c2f13437c26a5f923fec2c33d55",
        "src/resources/classic_shortcut_200.bmp":
            "e4ef3820d0c177575cd7b974dfcd6c3fd6c653e9",
        "src/resources/classic_close_200.bmp":
            "d872f63b63cf698a6477a31c76382e6dc0be98b1",
    }
    for path, expected in frozen_assets.items():
        if git_blob_sha(path) != expected:
            fail(f"v0.8 alpha.5.6 frozen Classic asset changed: {path}")

    for token in (
        "FILEVERSION 0,8,0,176",
        "PRODUCTVERSION 0,8,0,176",
        "0.8.0-alpha.5.6",
    ):
        if token not in resources:
            fail(f"v0.8 alpha.5.6 resource version missing: {token}")
    if 'version="0.8.0.176"' not in manifest:
        fail("v0.8 alpha.5.6 manifest fixed version must be 0.8.0.176")

    for token in (
        '"0.8.0-alpha.5.5"',
        '"0.8.0-alpha.5.6"',
        "UpdateChannel::Stable",
    ):
        if token not in update_tests:
            fail(f"v0.8 alpha.5.6 update-policy coverage missing: {token}")

    for token in (
        "v0.8.0-alpha.5.6 Next Table Surface validation",
        "no permanent vertical separators",
        "subtle divider hint appears only directly over a resizable boundary",
        "Path Conversion uses the exact same Table Header surface",
        "100/125/150/175/200%",
    ):
        if token not in desktop_validation:
            fail(f"v0.8 alpha.5.6 desktop checklist missing: {token}")

    readme = read("README.md")
    changelog = read("CHANGELOG.md")
    roadmap = read("ROADMAP.md")
    for token in (
        "v0.8.0-alpha.5.6 — Next Table Surface",
        "native Header still owns hit testing",
        "34 logical pixels high",
        "0.8.0.176",
    ):
        if token not in readme:
            fail(f"v0.8 alpha.5.6 README missing: {token}")
    if "## 0.8.0-alpha.5.6" not in changelog:
        fail("v0.8 alpha.5.6 changelog entry missing")
    if "v0.8.0-alpha.5.6 turns that foundation into a real Next Table Surface" not in roadmap:
        fail("v0.8 alpha.5.6 roadmap entry missing")

    print(
        "v0.8.0-alpha.5.6 Next Table Surface verified:",
        "| native Header retained only as interaction/resize engine",
        "| shared custom Header removes permanent grid chrome",
        "| Shortcut Manager + Path Conversion behavior preserved",
        "| alpha.5.4 ComboBox + Hotkeys + global Alt+S retained",
        "| schema 10 + Classic assets frozen",
    )
    raise SystemExit(0)


if version == "0.8.0-alpha.5.5":
    import hashlib
    import subprocess

    expected_schemas = {
        "kSettingsSchemaVersion": 10,
        "kCommandsSchemaVersion": 2,
        "kUsageSchemaVersion": 1,
    }
    for name, expected in expected_schemas.items():
        actual = cpp_int("src/core/ConfigIO.hpp", name)
        if actual != expected:
            fail(f"v0.8 alpha.5.5 {name}={actual}, expected {expected}")

    if cpp_int("src/core/ProviderCache.cpp", "kProviderCacheSchemaVersion") != 2:
        fail("v0.8 alpha.5.5 must keep provider-cache schemaVersion 2")

    settings_h = read("src/core/Settings.hpp")
    settings_cpp = read("src/core/Settings.cpp")
    settings_window_h = read("src/ui/SettingsWindow.hpp")
    settings_window_cpp = read("src/ui/SettingsWindow.cpp")
    shortcut_editor_cpp = read("src/ui/ShortcutEditorDialog.cpp")
    ui_combo_h = read("src/ui/UiComboBox.hpp")
    ui_combo_cpp = read("src/ui/UiComboBox.cpp")
    ui_list_h = read("src/ui/UiListView.hpp")
    ui_list_cpp = read("src/ui/UiListView.cpp")
    shortcut_manager_cpp = read("src/ui/ShortcutManagerWindow.cpp")
    path_converter_cpp = read("src/ui/ShortcutPathConverterDialog.cpp")
    cmake = read("CMakeLists.txt")
    app_h = read("src/app/App.hpp")
    app_cpp = read("src/app/App.cpp")
    launcher_cpp = read("src/ui/LauncherWindow.cpp")
    hotkey_cpp = read("src/core/HotkeyRegistry.cpp")
    hotkey_tests = read("tests/HotkeyRegistryTests.cpp")
    resources = read("src/resources.rc")
    manifest = read("src/app.manifest")
    update_tests = read("tests/UpdatePolicyTests.cpp")
    desktop_validation = read("docs/DESKTOP_VALIDATION.md")
    runtime_smoke = read("scripts/verify_runtime_smoke.ps1")

    removed_runtime_tokens = (
        "showOnStartup",
        "hideAfterLaunch",
        "clearQueryOnShow",
        "hideOnFocusLost",
        "wildcardMatching",
        "numericQuickLaunchOrder",
    )
    for path, text_value in (
        ("Settings.hpp", settings_h),
        ("SettingsWindow.hpp", settings_window_h),
        ("SettingsWindow.cpp", settings_window_cpp),
        ("App.hpp", app_h),
        ("App.cpp", app_cpp),
        ("LauncherWindow.cpp", launcher_cpp),
    ):
        for token in removed_runtime_tokens:
            if token in text_value:
                fail(
                    f"v0.8 alpha.5.5 removed pseudo-setting returned "
                    f"in {path}: {token}"
                )

    if "load.schemaVersion >= 10" not in settings_cpp:
        fail("v0.8 alpha.5.5 must keep schema-10 migration behavior")
    if '"startupBehavior"' not in settings_cpp or '"addToSendToMenu"' not in settings_cpp:
        fail("v0.8 alpha.5.5 must keep alpha.5.1 Settings fields")

    # One shared Next ComboBox implementation owns every product dropdown.
    if "src/ui/UiComboBox.cpp" not in cmake:
        fail("v0.8 alpha.5.5 shared UiComboBox source is not built")

    for token in (
        'L"COMBOBOX"',
        "CBS_DROPDOWNLIST",
        "CBS_OWNERDRAWFIXED",
        "SetWindowSubclass(",
        "NextComboSubclassProc",
        "DrawNextComboBoxSurface(",
        "DrawNextComboBoxItem(",
        "ApplyNextComboBoxMetrics(",
        "MeasureNextComboBoxPreferredWidth(",
        "GetTextExtentPoint32W(",
        "TME_LEAVE",
        "WM_MOUSELEAVE",
        "NextComboBoxItemHeight(",
        "ColorNextComboBoxList(",
    ):
        if token not in ui_combo_cpp:
            fail(f"v0.8 alpha.5.5 shared ComboBox implementation missing: {token}")

    for token in (
        "CreateNextComboBox(",
        "ApplyNextComboBoxMetrics(",
        "MeasureNextComboBoxPreferredWidth(",
        "NextComboBoxItemHeight(",
        "DrawNextComboBoxItem(",
        "ColorNextComboBoxList(",
    ):
        if token not in ui_combo_h:
            fail(f"v0.8 alpha.5.5 shared ComboBox API missing: {token}")

    for path, text_value in (
        ("SettingsWindow.cpp", settings_window_cpp),
        ("ShortcutEditorDialog.cpp", shortcut_editor_cpp),
    ):
        for forbidden in (
            'L"COMBOBOX"',
            "CBS_DROPDOWNLIST",
            "CreateThemedComboBox(",
            "ComboSubclassProc",
            "DrawComboSurface(",
        ):
            if forbidden in text_value:
                fail(
                    f"v0.8 alpha.5.5 {path} reintroduced a private/native "
                    f"ComboBox path: {forbidden}"
                )

    for control in (
        "startupBehavior_",
        "popupMonitor_",
        "launcherPlacement_",
        "settingsPlacement_",
        "shortcutManagerPlacement_",
        "uiStyle_",
        "language_",
    ):
        marker = f"{control} =\n        ui::CreateNextComboBox("
        if marker not in settings_window_cpp:
            fail(f"v0.8 alpha.5.5 Settings dropdown is not shared: {control}")

    for control in (
        "type_",
        "runtimeInput_",
    ):
        marker = f"{control} =\n        ui::CreateNextComboBox("
        if marker not in shortcut_editor_cpp:
            fail(f"v0.8 alpha.5.5 Shortcut Editor dropdown is not shared: {control}")

    for token in (
        "ui::DrawNextComboBoxItem(",
        "ui::ColorNextComboBoxList(",
        "ui::NextComboBoxItemHeight(",
        "ui::MeasureNextComboBoxPreferredWidth(",
    ):
        if token not in settings_window_cpp:
            fail(f"v0.8 alpha.5.5 Settings shared ComboBox route missing: {token}")
        if token not in shortcut_editor_cpp:
            fail(f"v0.8 alpha.5.5 Shortcut Editor shared ComboBox route missing: {token}")

    combo_surface = ui_combo_cpp.split(
        "void DrawNextComboBoxSurface(", 1
    )[1].split("LRESULT CALLBACK NextComboSubclassProc(", 1)[0]
    if "HPEN divider" in combo_surface or "arrowLeft" in combo_surface:
        fail("v0.8 alpha.5.5 must not restore a split ComboBox arrow cell")

    if "const int startupComboWidth = Scale(180)" in settings_window_cpp:
        fail("v0.8 alpha.5.5 Startup behavior returned to a fixed long width")
    if "const int comboWidth =\n            Scale(180);" in settings_window_cpp:
        fail("v0.8 alpha.5.5 placement ComboBoxes returned to one fixed width")

    # Shortcut Manager and Path Conversion share one dense Next ListView path.
    if "src/ui/UiListView.cpp" not in cmake:
        fail("v0.8 alpha.5.5 shared UiListView source is not built")

    for token in (
        "InitializeNextListView(",
        "DrawNextListHeader(",
        "NextListRowBackground(",
        "NextListRowText(",
        "NextListCellPadding(",
        "DrawNextListRowSeparator(",
    ):
        if token not in ui_list_h:
            fail(f"v0.8 alpha.5.5 shared ListView API missing: {token}")

    for token in (
        "NextListSubclassProc",
        "SetWindowSubclass(",
        "TME_LEAVE",
        "WM_MOUSELEAVE",
        "ListView_HitTest(",
        "Scale(30, state.dpi)",
        "LVS_EX_FULLROWSELECT",
        "LVS_EX_DOUBLEBUFFER",
        "SetWindowTheme(",
        "RoundRect(",
        "DrawNextListHeader(",
        "NextListRowBackground(",
        "DrawNextListRowSeparator(",
    ):
        if token not in ui_list_cpp:
            fail(f"v0.8 alpha.5.5 shared ListView implementation missing: {token}")

    for path, text_value in (
        ("ShortcutManagerWindow.cpp", shortcut_manager_cpp),
        ("ShortcutPathConverterDialog.cpp", path_converter_cpp),
    ):
        for token in (
            "ui::InitializeNextListView(",
            "ui::DrawNextListHeader(",
            "ui::NextListRowBackground(",
            "ui::NextListCellPadding(",
            "ui::DrawNextListRowSeparator(",
        ):
            if token not in text_value:
                fail(f"v0.8 alpha.5.5 {path} shared ListView route missing: {token}")

    if "WS_BORDER |\n            LVS_REPORT" in shortcut_manager_cpp:
        fail("v0.8 alpha.5.5 Shortcut Manager restored the old hard ListView border")

    for forbidden in (
        "RebuildRowHeightImageList",
        "rowHeightImageList_",
        "HandleHeaderCustomDraw(",
    ):
        if forbidden in shortcut_manager_cpp or forbidden in path_converter_cpp:
            fail(f"v0.8 alpha.5.5 duplicate ListView visual path returned: {forbidden}")

    if "GetSysColor(\n                  COLOR_HIGHLIGHTTEXT)" in path_converter_cpp:
        fail("v0.8 alpha.5.5 Path Conversion returned to native highlight text colors")

    if "DrawResultFieldCell(" not in path_converter_cpp:
        fail("v0.8 alpha.5.5 Path Conversion custom checkbox renderer was removed")
    if "HandleHeaderNotification(" not in shortcut_manager_cpp:
        fail("v0.8 alpha.5.5 Shortcut Manager column resize contract was removed")
    if "HandleHeaderNotification(" not in path_converter_cpp:
        fail("v0.8 alpha.5.5 Path Conversion column resize contract was removed")

    # Hotkey actions scroll independently; the reset-all action stays fixed.
    for token in (
        "hotkeyScrollOffset_",
        "HotkeyScrollViewport()",
        "HotkeyContentBottom()",
        "UpdatePageScrollBar()",
        "ScrollCurrentPage(",
        "ClipHotkeyControlsToViewport()",
        "page_ == Page::Hotkeys",
        "WM_MOUSEWHEEL",
        "WM_VSCROLL",
        "hotkeyResetAll_",
        "client.bottom",
    ):
        if token not in settings_window_cpp and token not in settings_window_h:
            fail(f"v0.8 alpha.5.5 Hotkeys scrolling contract missing: {token}")
    if "UpdateGeneralScrollBar" in settings_window_cpp or "ScrollGeneral(" in settings_window_cpp:
        fail("v0.8 alpha.5.5 must not retain the General-only scroll implementation")

    for token in (
        "HotkeyControlDesiredVisible(",
        "const bool desiredVisible",
        "if (!desiredVisible)",
        "return row.statusVisible;",
        "return row.resetVisible;",
        "row.statusVisible &&",
        "auxiliaryHeight > 0",
    ):
        if token not in settings_window_cpp and token not in settings_window_h:
            fail(f"v0.8 alpha.5.5 Hotkeys visibility contract missing: {token}")

    clip_body = settings_window_cpp.split(
        "ClipHotkeyControlsToViewport()", 1
    )[1].split("RECT SettingsWindow::BehaviorCardRect()", 1)[0]
    if "HotkeyControlDesiredVisible(" not in clip_body:
        fail("v0.8 alpha.5.5 viewport clipping must consult business visibility")

    # Shortcut Manager must be a real global hotkey, not a Launcher-only chord.
    global_manager_descriptor = (
        'kOpenShortcutManager),HotkeyScope::Global,false,true,'
        '{true,{"alt"},"s"}'
    )
    if global_manager_descriptor not in hotkey_cpp:
        fail("v0.8 alpha.5.5 Shortcut Manager default must be global Alt+S")
    if "HotkeyScope::Global,\n            \"s\"" not in hotkey_tests:
        fail("v0.8 alpha.5.5 Hotkey Registry tests do not assert global Alt+S")

    for token in (
        "kShortcutManagerHotkeyId = 0xA173",
        "RebindShortcutManagerHotkey(",
        "shortcutManagerHotkeyRegistered_",
        "shortcutManagerHotkeyLastError_",
    ):
        if token not in app_h:
            fail(f"v0.8 alpha.5.5 App hotkey state missing: {token}")

    for token in (
        "RegisterHotKey(",
        "kShortcutManagerHotkeyId",
        "RebindShortcutManagerHotkey(",
        "ShowShortcutManager();",
        "UnregisterHotKey(",
        "oldShortcutManager",
        "previousShortcutManager",
    ):
        if token not in app_cpp:
            fail(f"v0.8 alpha.5.5 global Shortcut Manager lifecycle missing: {token}")

    # Exit remains an opt-in Launcher action; do not accidentally globalize it.
    if (
        'kExitApplication),HotkeyScope::Launcher,false,false,{false,{},"f12"}'
        not in hotkey_cpp
    ):
        fail("v0.8 alpha.5.5 Exit action scope/default changed unexpectedly")

    # Startup notification is concise and uses the current activation binding.
    for token in (
        'L"已在后台启动"',
        'L"Running in the background"',
        'L" 呼出"',
        "activationHotkey",
    ):
        if token not in launcher_cpp:
            fail(f"v0.8 alpha.5.5 startup notification copy missing: {token}")
    if 'L"ALTRun Next 已启动"' in launcher_cpp:
        fail("v0.8 alpha.5.5 duplicated old startup notification copy returned")
    if 'VALUE "FileDescription", "ALTRun Next\\0"' not in resources:
        fail("v0.8 alpha.5.5 executable FileDescription must be ALTRun Next")
    if "classic lightweight launcher" in resources:
        fail("v0.8 alpha.5.5 old notification source description returned")

    # Keep alpha.5.1 SendTo/runtime behavior intact.
    for token in (
        "FOLDERID_SendTo",
        "ForwardShortcutRequestsToExistingInstance",
        "ApplySendToRegistration(",
    ):
        if token not in app_cpp:
            fail(f"v0.8 alpha.5.5 alpha.5.1 integration regressed: {token}")
    for token in (
        "schemaVersion -ne 10",
        '"launcher.openShortcutManager"',
        '"launcher.exitApplication"',
    ):
        if token not in runtime_smoke:
            fail(f"v0.8 alpha.5.5 packaged runtime smoke regressed: {token}")

    subprocess.run(
        [
            sys.executable,
            str(ROOT / "scripts" / "generate_classic_hidpi_assets.py"),
            "--verify",
        ],
        cwd=ROOT,
        check=True,
    )

    def git_blob_sha(path: str) -> str:
        data = (ROOT / path).read_bytes()
        header = f"blob {len(data)}\0".encode("ascii")
        return hashlib.sha1(header + data).hexdigest()

    frozen_assets = {
        "src/resources/classic_bg.bmp":
            "bbced49d20184051cf8ad48b153b022cecfecd50",
        "src/resources/classic_shortcut.bmp":
            "eee00956b449975f05e63a38e4da4ea29e01c240",
        "src/resources/classic_close.bmp":
            "51732fb285d83c2f13437c26a5f923fec2c33d55",
        "src/resources/classic_shortcut_200.bmp":
            "e4ef3820d0c177575cd7b974dfcd6c3fd6c653e9",
        "src/resources/classic_close_200.bmp":
            "d872f63b63cf698a6477a31c76382e6dc0be98b1",
    }
    for path, expected in frozen_assets.items():
        if git_blob_sha(path) != expected:
            fail(f"v0.8 alpha.5.5 frozen Classic asset changed: {path}")

    for token in (
        "FILEVERSION 0,8,0,175",
        "PRODUCTVERSION 0,8,0,175",
        "0.8.0-alpha.5.5",
    ):
        if token not in resources:
            fail(f"v0.8 alpha.5.5 resource version missing: {token}")
    if 'version="0.8.0.175"' not in manifest:
        fail("v0.8 alpha.5.5 manifest fixed version must be 0.8.0.175")

    for token in (
        '"0.8.0-alpha.5.4"',
        '"0.8.0-alpha.5.5"',
        "UpdateChannel::Stable",
    ):
        if token not in update_tests:
            fail(f"v0.8 alpha.5.5 update-policy coverage missing: {token}")

    for token in (
        "v0.8.0-alpha.5.5 shared ListView validation",
        "old dark hard table border",
        "body columns have no vertical grid lines",
        "Path Conversion Header and normal preview rows",
        "100/125/150/175/200%",
    ):
        if token not in desktop_validation:
            fail(f"v0.8 alpha.5.5 desktop checklist missing: {token}")

    readme = read("README.md")
    changelog = read("CHANGELOG.md")
    roadmap = read("ROADMAP.md")
    for token in (
        "v0.8.0-alpha.5.5 — Shared Next ListView UI",
        "one shared `UiListView` visual foundation",
        "Shortcut Manager and Path Conversion",
        "0.8.0.175",
    ):
        if token not in readme:
            fail(f"v0.8 alpha.5.5 README missing: {token}")
    if "## 0.8.0-alpha.5.5" not in changelog:
        fail("v0.8 alpha.5.5 changelog entry missing")
    if "v0.8.0-alpha.5.5 adds one shared Next ListView visual foundation" not in roadmap:
        fail("v0.8 alpha.5.5 roadmap entry missing")

    print(
        "v0.8.0-alpha.5.5 shared ListView verified:",
        "| Shortcut Manager + Path Conversion share one dense list visual foundation",
        "| alpha.5.4 shared ComboBox retained",
        "| column/checkbox behavior preserved",
        "| Hotkeys + global Alt+S retained",
        "| schema 10 + Classic assets frozen",
    )
    raise SystemExit(0)


if version == "0.8.0-alpha.5.4":
    import hashlib
    import subprocess

    expected_schemas = {
        "kSettingsSchemaVersion": 10,
        "kCommandsSchemaVersion": 2,
        "kUsageSchemaVersion": 1,
    }
    for name, expected in expected_schemas.items():
        actual = cpp_int("src/core/ConfigIO.hpp", name)
        if actual != expected:
            fail(f"v0.8 alpha.5.4 {name}={actual}, expected {expected}")

    if cpp_int("src/core/ProviderCache.cpp", "kProviderCacheSchemaVersion") != 2:
        fail("v0.8 alpha.5.4 must keep provider-cache schemaVersion 2")

    settings_h = read("src/core/Settings.hpp")
    settings_cpp = read("src/core/Settings.cpp")
    settings_window_h = read("src/ui/SettingsWindow.hpp")
    settings_window_cpp = read("src/ui/SettingsWindow.cpp")
    shortcut_editor_cpp = read("src/ui/ShortcutEditorDialog.cpp")
    ui_combo_h = read("src/ui/UiComboBox.hpp")
    ui_combo_cpp = read("src/ui/UiComboBox.cpp")
    cmake = read("CMakeLists.txt")
    app_h = read("src/app/App.hpp")
    app_cpp = read("src/app/App.cpp")
    launcher_cpp = read("src/ui/LauncherWindow.cpp")
    hotkey_cpp = read("src/core/HotkeyRegistry.cpp")
    hotkey_tests = read("tests/HotkeyRegistryTests.cpp")
    resources = read("src/resources.rc")
    manifest = read("src/app.manifest")
    update_tests = read("tests/UpdatePolicyTests.cpp")
    desktop_validation = read("docs/DESKTOP_VALIDATION.md")
    runtime_smoke = read("scripts/verify_runtime_smoke.ps1")

    removed_runtime_tokens = (
        "showOnStartup",
        "hideAfterLaunch",
        "clearQueryOnShow",
        "hideOnFocusLost",
        "wildcardMatching",
        "numericQuickLaunchOrder",
    )
    for path, text_value in (
        ("Settings.hpp", settings_h),
        ("SettingsWindow.hpp", settings_window_h),
        ("SettingsWindow.cpp", settings_window_cpp),
        ("App.hpp", app_h),
        ("App.cpp", app_cpp),
        ("LauncherWindow.cpp", launcher_cpp),
    ):
        for token in removed_runtime_tokens:
            if token in text_value:
                fail(
                    f"v0.8 alpha.5.4 removed pseudo-setting returned "
                    f"in {path}: {token}"
                )

    if "load.schemaVersion >= 10" not in settings_cpp:
        fail("v0.8 alpha.5.4 must keep schema-10 migration behavior")
    if '"startupBehavior"' not in settings_cpp or '"addToSendToMenu"' not in settings_cpp:
        fail("v0.8 alpha.5.4 must keep alpha.5.1 Settings fields")

    # One shared Next ComboBox implementation owns every product dropdown.
    if "src/ui/UiComboBox.cpp" not in cmake:
        fail("v0.8 alpha.5.4 shared UiComboBox source is not built")

    for token in (
        'L"COMBOBOX"',
        "CBS_DROPDOWNLIST",
        "CBS_OWNERDRAWFIXED",
        "SetWindowSubclass(",
        "NextComboSubclassProc",
        "DrawNextComboBoxSurface(",
        "DrawNextComboBoxItem(",
        "ApplyNextComboBoxMetrics(",
        "MeasureNextComboBoxPreferredWidth(",
        "GetTextExtentPoint32W(",
        "TME_LEAVE",
        "WM_MOUSELEAVE",
        "NextComboBoxItemHeight(",
        "ColorNextComboBoxList(",
    ):
        if token not in ui_combo_cpp:
            fail(f"v0.8 alpha.5.4 shared ComboBox implementation missing: {token}")

    for token in (
        "CreateNextComboBox(",
        "ApplyNextComboBoxMetrics(",
        "MeasureNextComboBoxPreferredWidth(",
        "NextComboBoxItemHeight(",
        "DrawNextComboBoxItem(",
        "ColorNextComboBoxList(",
    ):
        if token not in ui_combo_h:
            fail(f"v0.8 alpha.5.4 shared ComboBox API missing: {token}")

    for path, text_value in (
        ("SettingsWindow.cpp", settings_window_cpp),
        ("ShortcutEditorDialog.cpp", shortcut_editor_cpp),
    ):
        for forbidden in (
            'L"COMBOBOX"',
            "CBS_DROPDOWNLIST",
            "CreateThemedComboBox(",
            "ComboSubclassProc",
            "DrawComboSurface(",
        ):
            if forbidden in text_value:
                fail(
                    f"v0.8 alpha.5.4 {path} reintroduced a private/native "
                    f"ComboBox path: {forbidden}"
                )

    for control in (
        "startupBehavior_",
        "popupMonitor_",
        "launcherPlacement_",
        "settingsPlacement_",
        "shortcutManagerPlacement_",
        "uiStyle_",
        "language_",
    ):
        marker = f"{control} =\n        ui::CreateNextComboBox("
        if marker not in settings_window_cpp:
            fail(f"v0.8 alpha.5.4 Settings dropdown is not shared: {control}")

    for control in (
        "type_",
        "runtimeInput_",
    ):
        marker = f"{control} =\n        ui::CreateNextComboBox("
        if marker not in shortcut_editor_cpp:
            fail(f"v0.8 alpha.5.4 Shortcut Editor dropdown is not shared: {control}")

    for token in (
        "ui::DrawNextComboBoxItem(",
        "ui::ColorNextComboBoxList(",
        "ui::NextComboBoxItemHeight(",
        "ui::MeasureNextComboBoxPreferredWidth(",
    ):
        if token not in settings_window_cpp:
            fail(f"v0.8 alpha.5.4 Settings shared ComboBox route missing: {token}")
        if token not in shortcut_editor_cpp:
            fail(f"v0.8 alpha.5.4 Shortcut Editor shared ComboBox route missing: {token}")

    combo_surface = ui_combo_cpp.split(
        "void DrawNextComboBoxSurface(", 1
    )[1].split("LRESULT CALLBACK NextComboSubclassProc(", 1)[0]
    if "HPEN divider" in combo_surface or "arrowLeft" in combo_surface:
        fail("v0.8 alpha.5.4 must not restore a split ComboBox arrow cell")

    if "const int startupComboWidth = Scale(180)" in settings_window_cpp:
        fail("v0.8 alpha.5.4 Startup behavior returned to a fixed long width")
    if "const int comboWidth =\n            Scale(180);" in settings_window_cpp:
        fail("v0.8 alpha.5.4 placement ComboBoxes returned to one fixed width")

    # Hotkey actions scroll independently; the reset-all action stays fixed.
    for token in (
        "hotkeyScrollOffset_",
        "HotkeyScrollViewport()",
        "HotkeyContentBottom()",
        "UpdatePageScrollBar()",
        "ScrollCurrentPage(",
        "ClipHotkeyControlsToViewport()",
        "page_ == Page::Hotkeys",
        "WM_MOUSEWHEEL",
        "WM_VSCROLL",
        "hotkeyResetAll_",
        "client.bottom",
    ):
        if token not in settings_window_cpp and token not in settings_window_h:
            fail(f"v0.8 alpha.5.4 Hotkeys scrolling contract missing: {token}")
    if "UpdateGeneralScrollBar" in settings_window_cpp or "ScrollGeneral(" in settings_window_cpp:
        fail("v0.8 alpha.5.4 must not retain the General-only scroll implementation")

    for token in (
        "HotkeyControlDesiredVisible(",
        "const bool desiredVisible",
        "if (!desiredVisible)",
        "return row.statusVisible;",
        "return row.resetVisible;",
        "row.statusVisible &&",
        "auxiliaryHeight > 0",
    ):
        if token not in settings_window_cpp and token not in settings_window_h:
            fail(f"v0.8 alpha.5.4 Hotkeys visibility contract missing: {token}")

    clip_body = settings_window_cpp.split(
        "ClipHotkeyControlsToViewport()", 1
    )[1].split("RECT SettingsWindow::BehaviorCardRect()", 1)[0]
    if "HotkeyControlDesiredVisible(" not in clip_body:
        fail("v0.8 alpha.5.4 viewport clipping must consult business visibility")

    # Shortcut Manager must be a real global hotkey, not a Launcher-only chord.
    global_manager_descriptor = (
        'kOpenShortcutManager),HotkeyScope::Global,false,true,'
        '{true,{"alt"},"s"}'
    )
    if global_manager_descriptor not in hotkey_cpp:
        fail("v0.8 alpha.5.4 Shortcut Manager default must be global Alt+S")
    if "HotkeyScope::Global,\n            \"s\"" not in hotkey_tests:
        fail("v0.8 alpha.5.4 Hotkey Registry tests do not assert global Alt+S")

    for token in (
        "kShortcutManagerHotkeyId = 0xA173",
        "RebindShortcutManagerHotkey(",
        "shortcutManagerHotkeyRegistered_",
        "shortcutManagerHotkeyLastError_",
    ):
        if token not in app_h:
            fail(f"v0.8 alpha.5.4 App hotkey state missing: {token}")

    for token in (
        "RegisterHotKey(",
        "kShortcutManagerHotkeyId",
        "RebindShortcutManagerHotkey(",
        "ShowShortcutManager();",
        "UnregisterHotKey(",
        "oldShortcutManager",
        "previousShortcutManager",
    ):
        if token not in app_cpp:
            fail(f"v0.8 alpha.5.4 global Shortcut Manager lifecycle missing: {token}")

    # Exit remains an opt-in Launcher action; do not accidentally globalize it.
    if (
        'kExitApplication),HotkeyScope::Launcher,false,false,{false,{},"f12"}'
        not in hotkey_cpp
    ):
        fail("v0.8 alpha.5.4 Exit action scope/default changed unexpectedly")

    # Startup notification is concise and uses the current activation binding.
    for token in (
        'L"已在后台启动"',
        'L"Running in the background"',
        'L" 呼出"',
        "activationHotkey",
    ):
        if token not in launcher_cpp:
            fail(f"v0.8 alpha.5.4 startup notification copy missing: {token}")
    if 'L"ALTRun Next 已启动"' in launcher_cpp:
        fail("v0.8 alpha.5.4 duplicated old startup notification copy returned")
    if 'VALUE "FileDescription", "ALTRun Next\\0"' not in resources:
        fail("v0.8 alpha.5.4 executable FileDescription must be ALTRun Next")
    if "classic lightweight launcher" in resources:
        fail("v0.8 alpha.5.4 old notification source description returned")

    # Keep alpha.5.1 SendTo/runtime behavior intact.
    for token in (
        "FOLDERID_SendTo",
        "ForwardShortcutRequestsToExistingInstance",
        "ApplySendToRegistration(",
    ):
        if token not in app_cpp:
            fail(f"v0.8 alpha.5.4 alpha.5.1 integration regressed: {token}")
    for token in (
        "schemaVersion -ne 10",
        '"launcher.openShortcutManager"',
        '"launcher.exitApplication"',
    ):
        if token not in runtime_smoke:
            fail(f"v0.8 alpha.5.4 packaged runtime smoke regressed: {token}")

    subprocess.run(
        [
            sys.executable,
            str(ROOT / "scripts" / "generate_classic_hidpi_assets.py"),
            "--verify",
        ],
        cwd=ROOT,
        check=True,
    )

    def git_blob_sha(path: str) -> str:
        data = (ROOT / path).read_bytes()
        header = f"blob {len(data)}\0".encode("ascii")
        return hashlib.sha1(header + data).hexdigest()

    frozen_assets = {
        "src/resources/classic_bg.bmp":
            "bbced49d20184051cf8ad48b153b022cecfecd50",
        "src/resources/classic_shortcut.bmp":
            "eee00956b449975f05e63a38e4da4ea29e01c240",
        "src/resources/classic_close.bmp":
            "51732fb285d83c2f13437c26a5f923fec2c33d55",
        "src/resources/classic_shortcut_200.bmp":
            "e4ef3820d0c177575cd7b974dfcd6c3fd6c653e9",
        "src/resources/classic_close_200.bmp":
            "d872f63b63cf698a6477a31c76382e6dc0be98b1",
    }
    for path, expected in frozen_assets.items():
        if git_blob_sha(path) != expected:
            fail(f"v0.8 alpha.5.4 frozen Classic asset changed: {path}")

    for token in (
        "FILEVERSION 0,8,0,174",
        "PRODUCTVERSION 0,8,0,174",
        "0.8.0-alpha.5.4",
    ):
        if token not in resources:
            fail(f"v0.8 alpha.5.4 resource version missing: {token}")
    if 'version="0.8.0.174"' not in manifest:
        fail("v0.8 alpha.5.4 manifest fixed version must be 0.8.0.174")

    for token in (
        '"0.8.0-alpha.5.3"',
        '"0.8.0-alpha.5.4"',
        "UpdateChannel::Stable",
    ):
        if token not in update_tests:
            fail(f"v0.8 alpha.5.4 update-policy coverage missing: {token}")

    for token in (
        "v0.8.0-alpha.5.4 shared ComboBox validation",
        "Target type and Runtime input",
        "no old square Windows arrow button",
        "100/125/150/175/200%",
        "Hotkeys page still has no empty status strip",
    ):
        if token not in desktop_validation:
            fail(f"v0.8 alpha.5.4 desktop checklist missing: {token}")

    readme = read("README.md")
    changelog = read("CHANGELOG.md")
    roadmap = read("ROADMAP.md")
    for token in (
        "v0.8.0-alpha.5.4 — Shared Next ComboBox UI",
        "one shared native Win32 component",
        "Shortcut Editor",
        "0.8.0.174",
    ):
        if token not in readme:
            fail(f"v0.8 alpha.5.4 README missing: {token}")
    if "## 0.8.0-alpha.5.4" not in changelog:
        fail("v0.8 alpha.5.4 changelog entry missing")
    if "v0.8.0-alpha.5.4 promotes the approved ComboBox treatment into one shared native UI component" not in roadmap:
        fail("v0.8 alpha.5.4 roadmap entry missing")

    print(
        "v0.8.0-alpha.5.4 shared ComboBox verified:",
        "| Settings + Shortcut Editor use one native ComboBox component",
        "| alpha.5.3 Settings visual treatment retained",
        "| Hotkeys business visibility fix retained",
        "| global Shortcut Manager Alt+S retained",
        "| schema 10 + Classic assets frozen",
    )
    raise SystemExit(0)


if version == "0.8.0-alpha.5.3":
    import hashlib
    import subprocess

    expected_schemas = {
        "kSettingsSchemaVersion": 10,
        "kCommandsSchemaVersion": 2,
        "kUsageSchemaVersion": 1,
    }
    for name, expected in expected_schemas.items():
        actual = cpp_int("src/core/ConfigIO.hpp", name)
        if actual != expected:
            fail(f"v0.8 alpha.5.3 {name}={actual}, expected {expected}")

    if cpp_int("src/core/ProviderCache.cpp", "kProviderCacheSchemaVersion") != 2:
        fail("v0.8 alpha.5.3 must keep provider-cache schemaVersion 2")

    settings_h = read("src/core/Settings.hpp")
    settings_cpp = read("src/core/Settings.cpp")
    settings_window_h = read("src/ui/SettingsWindow.hpp")
    settings_window_cpp = read("src/ui/SettingsWindow.cpp")
    app_h = read("src/app/App.hpp")
    app_cpp = read("src/app/App.cpp")
    launcher_cpp = read("src/ui/LauncherWindow.cpp")
    hotkey_cpp = read("src/core/HotkeyRegistry.cpp")
    hotkey_tests = read("tests/HotkeyRegistryTests.cpp")
    resources = read("src/resources.rc")
    manifest = read("src/app.manifest")
    update_tests = read("tests/UpdatePolicyTests.cpp")
    desktop_validation = read("docs/DESKTOP_VALIDATION.md")
    runtime_smoke = read("scripts/verify_runtime_smoke.ps1")

    removed_runtime_tokens = (
        "showOnStartup",
        "hideAfterLaunch",
        "clearQueryOnShow",
        "hideOnFocusLost",
        "wildcardMatching",
        "numericQuickLaunchOrder",
    )
    for path, text_value in (
        ("Settings.hpp", settings_h),
        ("SettingsWindow.hpp", settings_window_h),
        ("SettingsWindow.cpp", settings_window_cpp),
        ("App.hpp", app_h),
        ("App.cpp", app_cpp),
        ("LauncherWindow.cpp", launcher_cpp),
    ):
        for token in removed_runtime_tokens:
            if token in text_value:
                fail(
                    f"v0.8 alpha.5.3 removed pseudo-setting returned "
                    f"in {path}: {token}"
                )

    if "load.schemaVersion >= 10" not in settings_cpp:
        fail("v0.8 alpha.5.3 must keep schema-10 migration behavior")
    if '"startupBehavior"' not in settings_cpp or '"addToSendToMenu"' not in settings_cpp:
        fail("v0.8 alpha.5.3 must keep alpha.5.1 Settings fields")

    # Every Settings dropdown must go through one shared native themed path.
    if settings_window_cpp.count("CreateThemedComboBox(") != 8:
        fail(
            "v0.8 alpha.5.3 expected one CreateThemedComboBox definition "
            "plus seven Settings dropdown uses"
        )
    for token in (
        "CBS_OWNERDRAWFIXED",
        "ComboSubclassProc",
        "DrawComboSurface(",
        "DrawComboItem(",
        "ODT_COMBOBOX",
        "CB_SETITEMHEIGHT",
        "kAccent",
        "kBorder",
    ):
        if token not in settings_window_cpp:
            fail(f"v0.8 alpha.5.3 themed ComboBox path missing: {token}")

    for control in (
        "startupBehavior_",
        "popupMonitor_",
        "launcherPlacement_",
        "settingsPlacement_",
        "shortcutManagerPlacement_",
        "uiStyle_",
        "language_",
    ):
        marker = f"{control} =\n        CreateThemedComboBox("
        if marker not in settings_window_cpp:
            fail(f"v0.8 alpha.5.3 Settings dropdown is not themed: {control}")

    # Alpha.5.3 keeps the shared native path but removes fixed-width/split-cell styling.
    for token in (
        "MeasureComboPreferredWidth(",
        "UpdateThemedComboMetrics()",
        "GetTextExtentPoint32W(",
        "WM_MOUSEMOVE",
        "TME_LEAVE",
        "WM_MOUSELEAVE",
        "Scale(30)",
    ):
        if token not in settings_window_cpp and token not in settings_window_h:
            fail(f"v0.8 alpha.5.3 ComboBox polish contract missing: {token}")

    combo_surface = settings_window_cpp.split(
        "void SettingsWindow::DrawComboSurface(", 1
    )[1].split("void SettingsWindow::DrawComboItem(", 1)[0]
    if "HPEN divider" in combo_surface or "arrowLeft" in combo_surface:
        fail("v0.8 alpha.5.3 must not restore the split ComboBox arrow cell")
    if "const int startupComboWidth = Scale(180)" in settings_window_cpp:
        fail("v0.8 alpha.5.3 Startup behavior returned to a fixed long width")
    if "const int comboWidth =\n            Scale(180);" in settings_window_cpp:
        fail("v0.8 alpha.5.3 placement ComboBoxes returned to one fixed width")

    # Hotkey actions scroll independently; the reset-all action stays fixed.
    for token in (
        "hotkeyScrollOffset_",
        "HotkeyScrollViewport()",
        "HotkeyContentBottom()",
        "UpdatePageScrollBar()",
        "ScrollCurrentPage(",
        "ClipHotkeyControlsToViewport()",
        "page_ == Page::Hotkeys",
        "WM_MOUSEWHEEL",
        "WM_VSCROLL",
        "hotkeyResetAll_",
        "client.bottom",
    ):
        if token not in settings_window_cpp and token not in settings_window_h:
            fail(f"v0.8 alpha.5.3 Hotkeys scrolling contract missing: {token}")
    if "UpdateGeneralScrollBar" in settings_window_cpp or "ScrollGeneral(" in settings_window_cpp:
        fail("v0.8 alpha.5.3 must not retain the General-only scroll implementation")

    for token in (
        "HotkeyControlDesiredVisible(",
        "const bool desiredVisible",
        "if (!desiredVisible)",
        "return row.statusVisible;",
        "return row.resetVisible;",
        "row.statusVisible &&",
        "auxiliaryHeight > 0",
    ):
        if token not in settings_window_cpp and token not in settings_window_h:
            fail(f"v0.8 alpha.5.3 Hotkeys visibility contract missing: {token}")

    clip_body = settings_window_cpp.split(
        "ClipHotkeyControlsToViewport()", 1
    )[1].split("RECT SettingsWindow::BehaviorCardRect()", 1)[0]
    if "HotkeyControlDesiredVisible(" not in clip_body:
        fail("v0.8 alpha.5.3 viewport clipping must consult business visibility")

    # Shortcut Manager must be a real global hotkey, not a Launcher-only chord.
    global_manager_descriptor = (
        'kOpenShortcutManager),HotkeyScope::Global,false,true,'
        '{true,{"alt"},"s"}'
    )
    if global_manager_descriptor not in hotkey_cpp:
        fail("v0.8 alpha.5.3 Shortcut Manager default must be global Alt+S")
    if "HotkeyScope::Global,\n            \"s\"" not in hotkey_tests:
        fail("v0.8 alpha.5.3 Hotkey Registry tests do not assert global Alt+S")

    for token in (
        "kShortcutManagerHotkeyId = 0xA173",
        "RebindShortcutManagerHotkey(",
        "shortcutManagerHotkeyRegistered_",
        "shortcutManagerHotkeyLastError_",
    ):
        if token not in app_h:
            fail(f"v0.8 alpha.5.3 App hotkey state missing: {token}")

    for token in (
        "RegisterHotKey(",
        "kShortcutManagerHotkeyId",
        "RebindShortcutManagerHotkey(",
        "ShowShortcutManager();",
        "UnregisterHotKey(",
        "oldShortcutManager",
        "previousShortcutManager",
    ):
        if token not in app_cpp:
            fail(f"v0.8 alpha.5.3 global Shortcut Manager lifecycle missing: {token}")

    # Exit remains an opt-in Launcher action; do not accidentally globalize it.
    if (
        'kExitApplication),HotkeyScope::Launcher,false,false,{false,{},"f12"}'
        not in hotkey_cpp
    ):
        fail("v0.8 alpha.5.3 Exit action scope/default changed unexpectedly")

    # Startup notification is concise and uses the current activation binding.
    for token in (
        'L"已在后台启动"',
        'L"Running in the background"',
        'L" 呼出"',
        "activationHotkey",
    ):
        if token not in launcher_cpp:
            fail(f"v0.8 alpha.5.3 startup notification copy missing: {token}")
    if 'L"ALTRun Next 已启动"' in launcher_cpp:
        fail("v0.8 alpha.5.3 duplicated old startup notification copy returned")
    if 'VALUE "FileDescription", "ALTRun Next\\0"' not in resources:
        fail("v0.8 alpha.5.3 executable FileDescription must be ALTRun Next")
    if "classic lightweight launcher" in resources:
        fail("v0.8 alpha.5.3 old notification source description returned")

    # Keep alpha.5.1 SendTo/runtime behavior intact.
    for token in (
        "FOLDERID_SendTo",
        "ForwardShortcutRequestsToExistingInstance",
        "ApplySendToRegistration(",
    ):
        if token not in app_cpp:
            fail(f"v0.8 alpha.5.3 alpha.5.1 integration regressed: {token}")
    for token in (
        "schemaVersion -ne 10",
        '"launcher.openShortcutManager"',
        '"launcher.exitApplication"',
    ):
        if token not in runtime_smoke:
            fail(f"v0.8 alpha.5.3 packaged runtime smoke regressed: {token}")

    subprocess.run(
        [
            sys.executable,
            str(ROOT / "scripts" / "generate_classic_hidpi_assets.py"),
            "--verify",
        ],
        cwd=ROOT,
        check=True,
    )

    def git_blob_sha(path: str) -> str:
        data = (ROOT / path).read_bytes()
        header = f"blob {len(data)}\0".encode("ascii")
        return hashlib.sha1(header + data).hexdigest()

    frozen_assets = {
        "src/resources/classic_bg.bmp":
            "bbced49d20184051cf8ad48b153b022cecfecd50",
        "src/resources/classic_shortcut.bmp":
            "eee00956b449975f05e63a38e4da4ea29e01c240",
        "src/resources/classic_close.bmp":
            "51732fb285d83c2f13437c26a5f923fec2c33d55",
        "src/resources/classic_shortcut_200.bmp":
            "e4ef3820d0c177575cd7b974dfcd6c3fd6c653e9",
        "src/resources/classic_close_200.bmp":
            "d872f63b63cf698a6477a31c76382e6dc0be98b1",
    }
    for path, expected in frozen_assets.items():
        if git_blob_sha(path) != expected:
            fail(f"v0.8 alpha.5.3 frozen Classic asset changed: {path}")

    for token in (
        "FILEVERSION 0,8,0,173",
        "PRODUCTVERSION 0,8,0,173",
        "0.8.0-alpha.5.3",
    ):
        if token not in resources:
            fail(f"v0.8 alpha.5.3 resource version missing: {token}")
    if 'version="0.8.0.173"' not in manifest:
        fail("v0.8 alpha.5.3 manifest fixed version must be 0.8.0.173")

    for token in (
        '"0.8.0-alpha.5.2"',
        '"0.8.0-alpha.5.3"',
        "UpdateChannel::Stable",
    ):
        if token not in update_tests:
            fail(f"v0.8 alpha.5.3 update-policy coverage missing: {token}")

    for token in (
        "v0.8.0-alpha.5.3 Settings polish validation",
        "content-sized",
        "no empty status strip",
        "恢复全部默认快捷键",
        "100/125/150/175/200%",
    ):
        if token not in desktop_validation:
            fail(f"v0.8 alpha.5.3 desktop checklist missing: {token}")

    readme = read("README.md")
    changelog = read("CHANGELOG.md")
    roadmap = read("ROADMAP.md")
    for token in (
        "v0.8.0-alpha.5.3 — Settings ComboBox Polish & Hotkey Viewport Fix",
        "content-sized ComboBox",
        "business visibility",
        "0.8.0.173",
    ):
        if token not in readme:
            fail(f"v0.8 alpha.5.3 README missing: {token}")
    if "## 0.8.0-alpha.5.3" not in changelog:
        fail("v0.8 alpha.5.3 changelog entry missing")
    if "v0.8.0-alpha.5.3 polishes that Settings path after real-Windows review" not in roadmap:
        fail("v0.8 alpha.5.3 roadmap entry missing")

    print(
        "v0.8.0-alpha.5.3 Settings polish verified:",
        "| content-sized continuous native ComboBoxes",
        "| Hotkeys business visibility preserved through clipping",
        "| global Shortcut Manager Alt+S retained",
        "| concise startup notification retained",
        "| schema 10 + Classic assets frozen",
    )
    raise SystemExit(0)


if version == "0.8.0-alpha.5.2":
    import hashlib
    import subprocess

    expected_schemas = {
        "kSettingsSchemaVersion": 10,
        "kCommandsSchemaVersion": 2,
        "kUsageSchemaVersion": 1,
    }
    for name, expected in expected_schemas.items():
        actual = cpp_int("src/core/ConfigIO.hpp", name)
        if actual != expected:
            fail(f"v0.8 alpha.5.2 {name}={actual}, expected {expected}")

    if cpp_int("src/core/ProviderCache.cpp", "kProviderCacheSchemaVersion") != 2:
        fail("v0.8 alpha.5.2 must keep provider-cache schemaVersion 2")

    settings_h = read("src/core/Settings.hpp")
    settings_cpp = read("src/core/Settings.cpp")
    settings_window_h = read("src/ui/SettingsWindow.hpp")
    settings_window_cpp = read("src/ui/SettingsWindow.cpp")
    app_h = read("src/app/App.hpp")
    app_cpp = read("src/app/App.cpp")
    launcher_cpp = read("src/ui/LauncherWindow.cpp")
    hotkey_cpp = read("src/core/HotkeyRegistry.cpp")
    hotkey_tests = read("tests/HotkeyRegistryTests.cpp")
    resources = read("src/resources.rc")
    manifest = read("src/app.manifest")
    update_tests = read("tests/UpdatePolicyTests.cpp")
    desktop_validation = read("docs/DESKTOP_VALIDATION.md")
    runtime_smoke = read("scripts/verify_runtime_smoke.ps1")

    removed_runtime_tokens = (
        "showOnStartup",
        "hideAfterLaunch",
        "clearQueryOnShow",
        "hideOnFocusLost",
        "wildcardMatching",
        "numericQuickLaunchOrder",
    )
    for path, text_value in (
        ("Settings.hpp", settings_h),
        ("SettingsWindow.hpp", settings_window_h),
        ("SettingsWindow.cpp", settings_window_cpp),
        ("App.hpp", app_h),
        ("App.cpp", app_cpp),
        ("LauncherWindow.cpp", launcher_cpp),
    ):
        for token in removed_runtime_tokens:
            if token in text_value:
                fail(
                    f"v0.8 alpha.5.2 removed pseudo-setting returned "
                    f"in {path}: {token}"
                )

    if "load.schemaVersion >= 10" not in settings_cpp:
        fail("v0.8 alpha.5.2 must keep schema-10 migration behavior")
    if '"startupBehavior"' not in settings_cpp or '"addToSendToMenu"' not in settings_cpp:
        fail("v0.8 alpha.5.2 must keep alpha.5.1 Settings fields")

    # Every Settings dropdown must go through one shared native themed path.
    if settings_window_cpp.count("CreateThemedComboBox(") != 8:
        fail(
            "v0.8 alpha.5.2 expected one CreateThemedComboBox definition "
            "plus seven Settings dropdown uses"
        )
    for token in (
        "CBS_OWNERDRAWFIXED",
        "ComboSubclassProc",
        "DrawComboSurface(",
        "DrawComboItem(",
        "ODT_COMBOBOX",
        "CB_SETITEMHEIGHT",
        "kAccent",
        "kBorder",
    ):
        if token not in settings_window_cpp:
            fail(f"v0.8 alpha.5.2 themed ComboBox path missing: {token}")

    for control in (
        "startupBehavior_",
        "popupMonitor_",
        "launcherPlacement_",
        "settingsPlacement_",
        "shortcutManagerPlacement_",
        "uiStyle_",
        "language_",
    ):
        marker = f"{control} =\n        CreateThemedComboBox("
        if marker not in settings_window_cpp:
            fail(f"v0.8 alpha.5.2 Settings dropdown is not themed: {control}")

    # Hotkey actions scroll independently; the reset-all action stays fixed.
    for token in (
        "hotkeyScrollOffset_",
        "HotkeyScrollViewport()",
        "HotkeyContentBottom()",
        "UpdatePageScrollBar()",
        "ScrollCurrentPage(",
        "ClipHotkeyControlsToViewport()",
        "page_ == Page::Hotkeys",
        "WM_MOUSEWHEEL",
        "WM_VSCROLL",
        "hotkeyResetAll_",
        "client.bottom",
    ):
        if token not in settings_window_cpp and token not in settings_window_h:
            fail(f"v0.8 alpha.5.2 Hotkeys scrolling contract missing: {token}")
    if "UpdateGeneralScrollBar" in settings_window_cpp or "ScrollGeneral(" in settings_window_cpp:
        fail("v0.8 alpha.5.2 must not retain the General-only scroll implementation")

    # Shortcut Manager must be a real global hotkey, not a Launcher-only chord.
    global_manager_descriptor = (
        'kOpenShortcutManager),HotkeyScope::Global,false,true,'
        '{true,{"alt"},"s"}'
    )
    if global_manager_descriptor not in hotkey_cpp:
        fail("v0.8 alpha.5.2 Shortcut Manager default must be global Alt+S")
    if "HotkeyScope::Global,\n            \"s\"" not in hotkey_tests:
        fail("v0.8 alpha.5.2 Hotkey Registry tests do not assert global Alt+S")

    for token in (
        "kShortcutManagerHotkeyId = 0xA173",
        "RebindShortcutManagerHotkey(",
        "shortcutManagerHotkeyRegistered_",
        "shortcutManagerHotkeyLastError_",
    ):
        if token not in app_h:
            fail(f"v0.8 alpha.5.2 App hotkey state missing: {token}")

    for token in (
        "RegisterHotKey(",
        "kShortcutManagerHotkeyId",
        "RebindShortcutManagerHotkey(",
        "ShowShortcutManager();",
        "UnregisterHotKey(",
        "oldShortcutManager",
        "previousShortcutManager",
    ):
        if token not in app_cpp:
            fail(f"v0.8 alpha.5.2 global Shortcut Manager lifecycle missing: {token}")

    # Exit remains an opt-in Launcher action; do not accidentally globalize it.
    if (
        'kExitApplication),HotkeyScope::Launcher,false,false,{false,{},"f12"}'
        not in hotkey_cpp
    ):
        fail("v0.8 alpha.5.2 Exit action scope/default changed unexpectedly")

    # Startup notification is concise and uses the current activation binding.
    for token in (
        'L"已在后台启动"',
        'L"Running in the background"',
        'L" 呼出"',
        "activationHotkey",
    ):
        if token not in launcher_cpp:
            fail(f"v0.8 alpha.5.2 startup notification copy missing: {token}")
    if 'L"ALTRun Next 已启动"' in launcher_cpp:
        fail("v0.8 alpha.5.2 duplicated old startup notification copy returned")
    if 'VALUE "FileDescription", "ALTRun Next\\0"' not in resources:
        fail("v0.8 alpha.5.2 executable FileDescription must be ALTRun Next")
    if "classic lightweight launcher" in resources:
        fail("v0.8 alpha.5.2 old notification source description returned")

    # Keep alpha.5.1 SendTo/runtime behavior intact.
    for token in (
        "FOLDERID_SendTo",
        "ForwardShortcutRequestsToExistingInstance",
        "ApplySendToRegistration(",
    ):
        if token not in app_cpp:
            fail(f"v0.8 alpha.5.2 alpha.5.1 integration regressed: {token}")
    for token in (
        "schemaVersion -ne 10",
        '"launcher.openShortcutManager"',
        '"launcher.exitApplication"',
    ):
        if token not in runtime_smoke:
            fail(f"v0.8 alpha.5.2 packaged runtime smoke regressed: {token}")

    subprocess.run(
        [
            sys.executable,
            str(ROOT / "scripts" / "generate_classic_hidpi_assets.py"),
            "--verify",
        ],
        cwd=ROOT,
        check=True,
    )

    def git_blob_sha(path: str) -> str:
        data = (ROOT / path).read_bytes()
        header = f"blob {len(data)}\0".encode("ascii")
        return hashlib.sha1(header + data).hexdigest()

    frozen_assets = {
        "src/resources/classic_bg.bmp":
            "bbced49d20184051cf8ad48b153b022cecfecd50",
        "src/resources/classic_shortcut.bmp":
            "eee00956b449975f05e63a38e4da4ea29e01c240",
        "src/resources/classic_close.bmp":
            "51732fb285d83c2f13437c26a5f923fec2c33d55",
        "src/resources/classic_shortcut_200.bmp":
            "e4ef3820d0c177575cd7b974dfcd6c3fd6c653e9",
        "src/resources/classic_close_200.bmp":
            "d872f63b63cf698a6477a31c76382e6dc0be98b1",
    }
    for path, expected in frozen_assets.items():
        if git_blob_sha(path) != expected:
            fail(f"v0.8 alpha.5.2 frozen Classic asset changed: {path}")

    for token in (
        "FILEVERSION 0,8,0,172",
        "PRODUCTVERSION 0,8,0,172",
        "0.8.0-alpha.5.2",
    ):
        if token not in resources:
            fail(f"v0.8 alpha.5.2 resource version missing: {token}")
    if 'version="0.8.0.172"' not in manifest:
        fail("v0.8 alpha.5.2 manifest fixed version must be 0.8.0.172")

    for token in (
        '"0.8.0-alpha.5.1"',
        '"0.8.0-alpha.5.2"',
        "UpdateChannel::Stable",
    ):
        if token not in update_tests:
            fail(f"v0.8 alpha.5.2 update-policy coverage missing: {token}")

    for token in (
        "v0.8.0-alpha.5.2 Settings UX validation",
        "恢复全部默认快捷键",
        "Default Alt+S opens Shortcut Manager",
        "Startup behavior, Launcher monitor",
        "100/125/150/175/200%",
    ):
        if token not in desktop_validation:
            fail(f"v0.8 alpha.5.2 desktop checklist missing: {token}")

    readme = read("README.md")
    changelog = read("CHANGELOG.md")
    roadmap = read("ROADMAP.md")
    for token in (
        "v0.8.0-alpha.5.2 — Settings UX & Global Shortcut Manager Hotkey",
        "true Windows global hotkey",
        "All seven Settings dropdowns",
        "0.8.0.172",
    ):
        if token not in readme:
            fail(f"v0.8 alpha.5.2 README missing: {token}")
    if "## 0.8.0-alpha.5.2" not in changelog:
        fail("v0.8 alpha.5.2 changelog entry missing")
    if "v0.8.0-alpha.5.2 fixes real-desktop Settings UX regressions" not in roadmap:
        fail("v0.8 alpha.5.2 roadmap entry missing")

    print(
        "v0.8.0-alpha.5.2 Settings UX verified:",
        "| global Shortcut Manager Alt+S",
        "| Hotkeys independent scroll + fixed reset action",
        "| seven themed native ComboBoxes",
        "| concise startup notification",
        "| schema 10 + Classic assets frozen",
    )
    raise SystemExit(0)


if version == "0.8.0-alpha.5.1":
    import hashlib
    import subprocess

    expected_schemas = {
        "kSettingsSchemaVersion": 10,
        "kCommandsSchemaVersion": 2,
        "kUsageSchemaVersion": 1,
    }
    for name, expected in expected_schemas.items():
        actual = cpp_int("src/core/ConfigIO.hpp", name)
        if actual != expected:
            fail(f"v0.8 alpha.5 {name}={actual}, expected {expected}")

    if cpp_int("src/core/ProviderCache.cpp", "kProviderCacheSchemaVersion") != 2:
        fail("v0.8 alpha.5 must keep provider-cache schemaVersion 2")

    settings_h = read("src/core/Settings.hpp")
    settings_cpp = read("src/core/Settings.cpp")
    settings_window_h = read("src/ui/SettingsWindow.hpp")
    settings_window_cpp = read("src/ui/SettingsWindow.cpp")
    settings_layout = read("src/core/SettingsLayout.cpp")
    app_h = read("src/app/App.hpp")
    app_cpp = read("src/app/App.cpp")
    main_cpp = read("src/main.cpp")
    launcher_h = read("src/ui/LauncherWindow.hpp")
    launcher_cpp = read("src/ui/LauncherWindow.cpp")
    hotkey_h = read("src/core/HotkeyRegistry.hpp")
    hotkey_cpp = read("src/core/HotkeyRegistry.cpp")
    context_h = read("src/core/ContextActions.hpp")
    context_cpp = read("src/core/ContextActions.cpp")
    ipc_h = read("src/platform/InstanceIpc.hpp")
    resources = read("src/resources.rc")
    manifest = read("src/app.manifest")
    ui_tests = read("tests/ConfigCoreTests.cpp")
    update_tests = read("tests/UpdatePolicyTests.cpp")
    desktop_validation = read("docs/DESKTOP_VALIDATION.md")
    generator = read("scripts/generate_classic_hidpi_assets.py")
    runtime_smoke = read("scripts/verify_runtime_smoke.ps1")

    for token in (
        "enum class StartupBehavior",
        "StartupBehavior::Notification",
        "bool addToSendToMenu{false}",
        "SetStartupBehavior(",
        "SetAddToSendToMenu(",
        "SetPopupMonitor(",
    ):
        if token not in settings_h:
            fail(f"v0.8 alpha.5 settings model missing: {token}")

    removed_runtime_tokens = (
        "showOnStartup",
        "hideAfterLaunch",
        "clearQueryOnShow",
        "hideOnFocusLost",
        "wildcardMatching",
        "numericQuickLaunchOrder",
    )
    for path, text in (
        ("Settings.hpp", settings_h),
        ("SettingsWindow.hpp", settings_window_h),
        ("SettingsWindow.cpp", settings_window_cpp),
        ("App.hpp", app_h),
        ("App.cpp", app_cpp),
        ("LauncherWindow.hpp", launcher_h),
        ("LauncherWindow.cpp", launcher_cpp),
    ):
        for token in removed_runtime_tokens:
            if token in text:
                fail(
                    f"v0.8 alpha.5 removed pseudo-setting returned "
                    f"in {path}: {token}"
                )

    for token in (
        '"startupBehavior"',
        '"addToSendToMenu"',
        "StartupBehaviorName(",
        "NormalizeStartupBehavior(",
        "load.schemaVersion >= 10",
        '"showOnStartup"',
    ):
        if token not in settings_cpp:
            fail(f"v0.8 alpha.5 schema migration missing: {token}")

    save_tail = settings_cpp[settings_cpp.find("bool SettingsStore::Save()"):]
    for forbidden in (
        '"hideAfterLaunch"',
        '"clearQueryOnShow"',
        '"hideOnFocusLost"',
        '"wildcardMatching"',
        '"numericQuickLaunchOrder"',
        '"showOnStartup"',
    ):
        if forbidden in save_tail:
            fail(f"v0.8 alpha.5 serializer still writes {forbidden}")

    for token in (
        'T(L"Windows 与启动", L"Windows & startup")',
        'T(L"启动行为", L"Startup behavior")',
        'T(L"添加到“发送到”菜单", L"Add to “Send to” menu")',
        'T(L"搜索与执行", L"Search & execution")',
        'T(L"静默启动", L"Start silently")',
        'T(L"显示启动通知", L"Show startup notification")',
        'T(L"显示启动器", L"Show launcher")',
        "kIdStartupBehavior",
        "kIdAddToSendToMenu",
    ):
        if token not in settings_window_cpp and token not in settings_window_h:
            fail(f"v0.8 alpha.5 General UI contract missing: {token}")

    if "scale(kToggleRowLogical) * 3" not in settings_layout:
        fail("v0.8 alpha.5 Windows/startup card layout is not 3 toggles + combo")
    if "scale(kToggleRowLogical) * 4" not in settings_layout:
        fail("v0.8 alpha.5 Search/execution card layout is not four toggles")

    for token in (
        "kOpenShortcutManager",
        "kExitApplication",
    ):
        if token not in hotkey_h or token not in hotkey_cpp:
            fail(f"v0.8 alpha.5 Hotkey Registry missing {token}")

    for token in (
        'kOpenShortcutManager),HotkeyScope::Launcher,false,false,{true,{"alt"},"s"}',
        'kExitApplication),HotkeyScope::Launcher,false,false,{false,{},"f12"}',
    ):
        if token not in hotkey_cpp:
            fail(f"v0.8 alpha.5 hotkey default changed: {token}")

    for token in (
        "ShortcutSeedFromFileSystemPath(",
        "SuggestShortcutTitle(",
        "InferShortcutCommandType(",
    ):
        if token not in context_h and token not in context_cpp:
            fail(f"v0.8 alpha.5 SendTo shortcut seed missing: {token}")

    for token in (
        'L"--add-shortcut"',
        "shortcutPaths",
    ):
        if token not in main_cpp:
            fail(f"v0.8 alpha.5 command-line SendTo contract missing: {token}")

    for token in (
        "kLauncherWindowClass",
        "kAddShortcutCopyData",
    ):
        if token not in ipc_h:
            fail(f"v0.8 alpha.5 instance IPC contract missing: {token}")

    for token in (
        "FOLDERID_SendTo",
        "CLSID_ShellLink",
        'SetArguments(\n                L"--add-shortcut")',
        "ForwardShortcutRequestsToExistingInstance",
        "SendMessageTimeoutW(",
        "WM_COPYDATA",
        "ApplySendToRegistration(",
        "StartupBehavior::ShowLauncher",
        "StartupBehavior::Notification",
        "ShowStartupNotification(",
    ):
        if token not in app_cpp:
            fail(f"v0.8 alpha.5 App integration missing: {token}")

    for token in (
        "WM_COPYDATA",
        "kAddShortcutCopyData",
        "QueueNewShortcutForPath(",
        "ProcessPendingShortcutPaths(",
        "NIF_INFO",
        "NIIF_NOSOUND",
        "NIN_BALLOONTIMEOUT",
        "NIN_BALLOONHIDE",
        'QuickLaunchIndexForDigit(\n            digit,\n            "one-to-zero")',
        "SetWindowTextW(edit_, L\"\")",
        "case WM_CLOSE:",
    ):
        if token not in launcher_cpp:
            fail(f"v0.8 alpha.5 Launcher behavior missing: {token}")

    if "true,\n            settingsStore_.Data()\n                .pinyinSearch" not in app_cpp:
        fail("v0.8 alpha.5 wildcard matching is not permanently enabled")

    for token in (
        "settings-schema9-behavior-cleanup.json",
        "StartupBehavior::ShowLauncher",
        'schema10Text.find("\\\"showOnStartup\\\"")',
    ):
        if token not in ui_tests:
            fail(f"v0.8 alpha.5 schema-10 migration coverage missing: {token}")

    subprocess.run(
        [
            sys.executable,
            str(ROOT / "scripts" / "generate_classic_hidpi_assets.py"),
            "--verify",
        ],
        cwd=ROOT,
        check=True,
    )

    def git_blob_sha(path: str) -> str:
        data = (ROOT / path).read_bytes()
        header = f"blob {len(data)}\0".encode("ascii")
        return hashlib.sha1(header + data).hexdigest()

    frozen_assets = {
        "src/resources/classic_bg.bmp":
            "bbced49d20184051cf8ad48b153b022cecfecd50",
        "src/resources/classic_shortcut.bmp":
            "eee00956b449975f05e63a38e4da4ea29e01c240",
        "src/resources/classic_close.bmp":
            "51732fb285d83c2f13437c26a5f923fec2c33d55",
        "src/resources/classic_shortcut_200.bmp":
            "e4ef3820d0c177575cd7b974dfcd6c3fd6c653e9",
        "src/resources/classic_close_200.bmp":
            "d872f63b63cf698a6477a31c76382e6dc0be98b1",
    }
    for path, expected in frozen_assets.items():
        if git_blob_sha(path) != expected:
            fail(f"v0.8 alpha.5 frozen Classic asset changed: {path}")

    for token in (
        "FILEVERSION 0,8,0,171",
        "PRODUCTVERSION 0,8,0,171",
        "0.8.0-alpha.5.1",
    ):
        if token not in resources:
            fail(f"v0.8 alpha.5 resource version missing: {token}")
    if 'version="0.8.0.171"' not in manifest:
        fail("v0.8 alpha.5 manifest fixed version must be 0.8.0.171")

    for token in (
        "schemaVersion -ne 10",
        "startupBehavior=silent",
        '"launcher.openShortcutManager"',
        '"launcher.exitApplication"',
        "schema 2 -> 10",
    ):
        if token not in runtime_smoke:
            fail(f"v0.8 alpha.5.1 packaged runtime smoke contract missing: {token}")

    for token in (
        '"0.8.0-alpha.4.9"',
        '"0.8.0-alpha.5.1"',
        "UpdateChannel::Stable",
    ):
        if token not in update_tests:
            fail(f"v0.8 alpha.5 update-policy coverage missing: {token}")

    for token in (
        "v0.8.0-alpha.5.1 Settings behavior validation",
        "添加到“发送到”菜单",
        "静默启动 / 显示启动通知 / 显示启动器",
        "default Alt+S",
        "100/125/150/175/200%",
    ):
        if token not in desktop_validation:
            fail(f"v0.8 alpha.5 desktop checklist missing: {token}")

    readme = read("README.md")
    changelog = read("CHANGELOG.md")
    roadmap = read("ROADMAP.md")
    for token in (
        "v0.8.0-alpha.5.1 — Settings Behavior Cleanup",
        "Settings schema is **10**",
        "WM_COPYDATA",
        "0.8.0.171",
    ):
        if token not in readme:
            fail(f"v0.8 alpha.5 README missing: {token}")
    if "## 0.8.0-alpha.5.1" not in changelog:
        fail("v0.8 alpha.5 changelog entry missing")
    if "v0.8.0-alpha.5.1 cleans the Settings product model" not in roadmap:
        fail("v0.8 alpha.5 roadmap entry missing")

    print(
        "v0.8.0-alpha.5.1 Settings behavior cleanup verified:",
        "| schema 10",
        "| pseudo-settings removed",
        "| startup behavior + notification",
        "| SendTo + single-instance IPC",
        "| Shortcut Manager + Exit hotkeys",
        "| Classic HiDPI assets frozen",
    )
    raise SystemExit(0)


if version == "0.8.0-alpha.4.9":
    import hashlib
    import subprocess

    expected_schemas = {
        "kSettingsSchemaVersion": 9,
        "kCommandsSchemaVersion": 2,
        "kUsageSchemaVersion": 1,
    }
    for name, expected in expected_schemas.items():
        actual = cpp_int("src/core/ConfigIO.hpp", name)
        if actual != expected:
            fail(f"v0.8 alpha.4.9 {name}={actual}, expected {expected}")

    if cpp_int("src/core/ProviderCache.cpp", "kProviderCacheSchemaVersion") != 2:
        fail("v0.8 alpha.4.9 must keep provider-cache schemaVersion 2")

    launcher = read("src/ui/LauncherWindow.cpp")
    launcher_h = read("src/ui/LauncherWindow.hpp")
    metrics = read("src/ui/UiMetrics.hpp")
    typography = read("src/ui/UiTypography.cpp")
    resources = read("src/resources.rc")
    resource_ids = read("src/ResourceIds.h")
    manifest = read("src/app.manifest")
    cmake = read("CMakeLists.txt")
    ui_tests = read("tests/UiFoundationTests.cpp")
    update_tests = read("tests/UpdatePolicyTests.cpp")
    desktop_validation = read("docs/DESKTOP_VALIDATION.md")
    generator = read("scripts/generate_classic_hidpi_assets.py")
    path_cpp = read("src/ui/ShortcutPathConverterDialog.cpp")
    editor_cpp = read("src/ui/ShortcutEditorDialog.cpp")
    app_cpp = read("src/app/App.cpp")
    app_h = read("src/app/App.hpp")

    def git_blob_sha(path: str) -> str:
        data = (ROOT / path).read_bytes()
        header = f"blob {len(data)}\0".encode("ascii")
        return hashlib.sha1(header + data).hexdigest()

    # The approved 100% assets and background remain immutable.
    expected_base_assets = {
        "src/resources/classic_bg.jpg": (
            74289,
            "daf552e9f948335575ae2701ca7ce0308c7c288c",
        ),
        "src/resources/classic_bg.bmp": (
            475578,
            "bbced49d20184051cf8ad48b153b022cecfecd50",
        ),
        "src/resources/classic_shortcut.bmp": (
            2554,
            "eee00956b449975f05e63a38e4da4ea29e01c240",
        ),
        "src/resources/classic_close.bmp": (
            1954,
            "51732fb285d83c2f13437c26a5f923fec2c33d55",
        ),
    }
    for path, (expected_size, expected_sha) in expected_base_assets.items():
        data = (ROOT / path).read_bytes()
        if len(data) != expected_size or git_blob_sha(path) != expected_sha:
            fail(f"v0.8 alpha.4.9 approved Classic base asset changed: {path}")

    expected_hidpi_assets = {
        "src/resources/classic_shortcut_125.bmp": (
            31,
            3898,
            "9462262313ce92a1e9019cbe013df6471ca508fe",
        ),
        "src/resources/classic_close_125.bmp": (
            31,
            3898,
            "19599f1eebcf543fa55c637ed6d3cd63964116ef",
        ),
        "src/resources/classic_shortcut_150.bmp": (
            38,
            5830,
            "58139c2311ac2fab80e01122ec5fbbbdf7736c49",
        ),
        "src/resources/classic_close_150.bmp": (
            38,
            5830,
            "3b8ac72c3cd8e89288e74d9c338b3a2f44ab5073",
        ),
        "src/resources/classic_shortcut_175.bmp": (
            44,
            7798,
            "64d2b11dde8fd2025dc489bb144356dc1e4ac8ce",
        ),
        "src/resources/classic_close_175.bmp": (
            44,
            7798,
            "a85834917ac87e1678ea1bb8be49377c15cc0225",
        ),
        "src/resources/classic_shortcut_200.bmp": (
            50,
            10054,
            "e4ef3820d0c177575cd7b974dfcd6c3fd6c653e9",
        ),
        "src/resources/classic_close_200.bmp": (
            50,
            10054,
            "d872f63b63cf698a6477a31c76382e6dc0be98b1",
        ),
    }

    for path, (expected_side, expected_size, expected_sha) in expected_hidpi_assets.items():
        data = (ROOT / path).read_bytes()
        if len(data) != expected_size:
            fail(
                f"v0.8 alpha.4.9 {path} size={len(data)}, "
                f"expected {expected_size}"
            )
        if git_blob_sha(path) != expected_sha:
            fail(f"v0.8 alpha.4.9 deterministic HiDPI asset changed: {path}")
        if data[:2] != b"BM":
            fail(f"v0.8 alpha.4.9 HiDPI asset is not BMP: {path}")
        width = int.from_bytes(data[18:22], "little", signed=True)
        height = int.from_bytes(data[22:26], "little", signed=True)
        bpp = int.from_bytes(data[28:30], "little")
        if (width, height, bpp) != (
            expected_side,
            expected_side,
            32,
        ):
            fail(
                f"v0.8 alpha.4.9 {path} must be "
                f"{expected_side}x{expected_side}x32"
            )
        pixel_offset = int.from_bytes(data[10:14], "little")
        alpha_values = data[pixel_offset + 3 :: 4]
        if not any(0 < alpha < 255 for alpha in alpha_values):
            fail(f"v0.8 alpha.4.9 {path} has no blended alpha edge")

    subprocess.run(
        [
            sys.executable,
            str(ROOT / "scripts" / "generate_classic_hidpi_assets.py"),
            "--verify",
        ],
        cwd=ROOT,
        check=True,
    )

    for token in (
        "SOURCE_SIZE = 25",
        'TARGETS = (("125", 31), ("150", 38), ("175", 44), ("200", 50))',
        "resize_premultiplied(",
        "write_bmp32(",
        "--verify",
        "Black is treated as the",
    ):
        if token not in generator:
            fail(f"v0.8 alpha.4.9 deterministic generator contract missing: {token}")

    expected_resource_ids = {
        "IDB_CLASSIC_SHORTCUT_125": 204,
        "IDB_CLASSIC_CLOSE_125": 205,
        "IDB_CLASSIC_SHORTCUT_150": 206,
        "IDB_CLASSIC_CLOSE_150": 207,
        "IDB_CLASSIC_SHORTCUT_175": 208,
        "IDB_CLASSIC_CLOSE_175": 209,
        "IDB_CLASSIC_SHORTCUT_200": 210,
        "IDB_CLASSIC_CLOSE_200": 211,
    }
    for name, value in expected_resource_ids.items():
        if f"#define {name} {value}" not in resource_ids:
            fail(f"v0.8 alpha.4.9 resource ID missing: {name}")

    for token in (
        'IDB_CLASSIC_SHORTCUT_125 BITMAP "resources/classic_shortcut_125.bmp"',
        'IDB_CLASSIC_CLOSE_125 BITMAP "resources/classic_close_125.bmp"',
        'IDB_CLASSIC_SHORTCUT_150 BITMAP "resources/classic_shortcut_150.bmp"',
        'IDB_CLASSIC_CLOSE_150 BITMAP "resources/classic_close_150.bmp"',
        'IDB_CLASSIC_SHORTCUT_175 BITMAP "resources/classic_shortcut_175.bmp"',
        'IDB_CLASSIC_CLOSE_175 BITMAP "resources/classic_close_175.bmp"',
        'IDB_CLASSIC_SHORTCUT_200 BITMAP "resources/classic_shortcut_200.bmp"',
        'IDB_CLASSIC_CLOSE_200 BITMAP "resources/classic_close_200.bmp"',
        "FILEVERSION 0,8,0,79",
        "PRODUCTVERSION 0,8,0,79",
        "0.8.0-alpha.4.9",
    ):
        if token not in resources:
            fail(f"v0.8 alpha.4.9 resource/version contract missing: {token}")

    for token in (
        'version="0.8.0.79"',
        ">PerMonitorV2</dpiAwareness>",
    ):
        if token not in manifest:
            fail(f"v0.8 alpha.4.9 manifest contract missing: {token}")

    for token in (
        "kClassicGlyphAssetPixelSizes{",
        "        25,",
        "        31,",
        "        38,",
        "        44,",
        "        50,",
        "ClassicGlyphAssetIndexForTarget(",
        "kClassicSeparatorPhysicalThickness = 1",
    ):
        if token not in metrics:
            fail(f"v0.8 alpha.4.9 glyph-tier metrics contract missing: {token}")

    for token in (
        "classicShortcutBitmaps_",
        "classicCloseBitmaps_",
        "kClassicShortcutResourceIds",
        "kClassicCloseResourceIds",
        "LoadClassicBitmapResources(",
        "ClassicGlyphAssetIndexForTarget(",
        "kClassicGlyphAssetPixelSizes[",
        "assetIndex != 0",
        "AlphaBlend(",
        "AC_SRC_ALPHA",
        "TransparentBlt(",
        "COLORONCOLOR",
    ):
        if token not in launcher and token not in launcher_h:
            fail(f"v0.8 alpha.4.9 Launcher HiDPI wiring missing: {token}")

    for forbidden in (
        "#include <gdiplus.h>",
        "Gdiplus::",
        "CreateStreamOnHGlobal(",
        "LoadClassicJpegResource(",
        "IWIC",
        "Direct2D",
    ):
        if forbidden in launcher or forbidden in launcher_h:
            fail(f"v0.8 alpha.4.9 heavy runtime image path introduced: {forbidden}")

    if "\n        msimg32\n" not in cmake:
        fail("v0.8 alpha.4.9 native AlphaBlend dependency must remain msimg32")

    for token in (
        "ClassicDpiExpectation,\n        5>",
        "168u",
        "735,\n                438",
        "{14, 53, 707, 39}",
        "{14, 98, 707, 287}",
        "{14, 396, 707, 28}",
        "std::pair{168u, 44}",
        "ClassicGlyphAssetIndexForTarget(",
        "64) == 4",
    ):
        if token not in ui_tests:
            fail(f"v0.8 alpha.4.9 175%/asset selection coverage missing: {token}")

    for token in (
        "kClassicLauncherPrimaryLogicalHeight96 = -16",
        "kClassicLauncherAuxiliaryLogicalHeight96 = -13",
        'L"SimSun"',
        "ANSI_CHARSET",
        "DEFAULT_QUALITY",
    ):
        if token not in typography:
            fail(f"v0.8 alpha.4.9 Classic typography baseline regressed: {token}")

    for token in (
        "DT_PATH_ELLIPSIS",
        "classicDpiMetrics_.rowHeight",
        "classicDpiMetrics_.cornerDiameter",
        "ResultIconWorkerLoop()",
        "kIconReadyMessage",
        "MergeLauncherResultsRanked(",
    ):
        if token not in launcher and token not in launcher_h:
            fail(f"v0.8 alpha.4.9 frozen Launcher architecture regressed: {token}")

    for token in ("NM_CLICK", "NM_DBLCLK", "ToggleResultRowSelection("):
        if token not in path_cpp:
            fail(f"v0.8 alpha.4.9 Path Conversion rapid-click regression: {token}")

    for token in ("BN_CLICKED", "BN_DOUBLECLICKED", "toggleActivated", "ToggleAdvanced();"):
        if token not in editor_cpp:
            fail(f"v0.8 alpha.4.9 Shortcut Editor rapid-click regression: {token}")

    for token in (
        'L"ALTRunNext.UpdateDispatch"',
        "UpdateDispatchWindowProc(",
        "CreateUpdateDispatchWindow()",
        "PostUpdateStatusNotification(",
        "HWND_MESSAGE",
        "kUpdateReconcileTimerId",
    ):
        if token not in app_cpp and token not in app_h:
            fail(f"v0.8 alpha.4.9 updater architecture regressed: {token}")

    for token in (
        '"0.8.0-alpha.4.8"',
        '"0.8.0-alpha.4.9"',
        "UpdateChannel::Stable",
    ):
        if token not in update_tests:
            fail(f"v0.8 alpha.4.9 update ordering/default coverage missing: {token}")

    for token in (
        "v0.8.0-alpha.4.9 Classic HiDPI glyph validation",
        "25px original",
        "31px HiDPI",
        "38px HiDPI",
        "44px HiDPI",
        "50px HiDPI",
        "no black halo",
        "Per-Monitor V2",
    ):
        if token not in desktop_validation:
            fail(f"v0.8 alpha.4.9 desktop validation contract missing: {token}")

    readme = read("README.md")
    changelog = read("CHANGELOG.md")
    roadmap = read("ROADMAP.md")

    for token in (
        "v0.8.0-alpha.4.9 — Classic HiDPI Glyph Assets",
        "31 / 38 / 44 / 50 px",
        "premultiplied alpha",
        "AlphaBlend",
        "168-DPI",
        "0.8.0.79",
    ):
        if token not in readme:
            fail(f"v0.8 alpha.4.9 README contract missing: {token}")

    for token in (
        "## 0.8.0-alpha.4.9",
        "31/38/44/50px",
        "AC_SRC_ALPHA",
        "175% / 168-DPI",
        "0.8.0.79",
    ):
        if token not in changelog:
            fail(f"v0.8 alpha.4.9 changelog contract missing: {token}")

    for token in (
        "v0.8.0-alpha.4.9",
        "25/31/38/44/50px",
        "100% remains byte-identical",
        "125/150/175/200%",
    ):
        if token not in roadmap:
            fail(f"v0.8 alpha.4.9 roadmap contract missing: {token}")

    print(
        "v0.8.0-alpha.4.9 Classic HiDPI glyph assets verified:",
        "| original 25px tier immutable",
        "| deterministic 31/38/44/50px premultiplied-alpha assets",
        "| exact standard-DPI tier selection",
        "| 175% geometry covered",
        "| no runtime image decoder or heavy UI stack",
    )
    raise SystemExit(0)


if version == "0.8.0-alpha.4.8":
    import hashlib

    expected_schemas = {
        "kSettingsSchemaVersion": 9,
        "kCommandsSchemaVersion": 2,
        "kUsageSchemaVersion": 1,
    }
    for name, expected in expected_schemas.items():
        actual = cpp_int("src/core/ConfigIO.hpp", name)
        if actual != expected:
            fail(f"v0.8 alpha.4.8 {name}={actual}, expected {expected}")

    if cpp_int("src/core/ProviderCache.cpp", "kProviderCacheSchemaVersion") != 2:
        fail("v0.8 alpha.4.8 must keep provider-cache schemaVersion 2")

    launcher = read("src/ui/LauncherWindow.cpp")
    launcher_h = read("src/ui/LauncherWindow.hpp")
    metrics = read("src/ui/UiMetrics.hpp")
    typography = read("src/ui/UiTypography.cpp")
    resources = read("src/resources.rc")
    manifest = read("src/app.manifest")
    cmake = read("CMakeLists.txt")
    ui_tests = read("tests/UiFoundationTests.cpp")
    update_tests = read("tests/UpdatePolicyTests.cpp")
    desktop_validation = read("docs/DESKTOP_VALIDATION.md")
    path_cpp = read("src/ui/ShortcutPathConverterDialog.cpp")
    editor_cpp = read("src/ui/ShortcutEditorDialog.cpp")
    app_cpp = read("src/app/App.cpp")
    app_h = read("src/app/App.hpp")

    def git_blob_sha(path: str) -> str:
        data = (ROOT / path).read_bytes()
        header = f"blob {len(data)}\0".encode("ascii")
        return hashlib.sha1(header + data).hexdigest()

    expected_assets = {
        "src/resources/classic_bg.jpg": (
            74289,
            "daf552e9f948335575ae2701ca7ce0308c7c288c",
        ),
        "src/resources/classic_bg.bmp": (
            475578,
            "bbced49d20184051cf8ad48b153b022cecfecd50",
        ),
        "src/resources/classic_shortcut.bmp": (
            2554,
            "eee00956b449975f05e63a38e4da4ea29e01c240",
        ),
        "src/resources/classic_close.bmp": (
            1954,
            "51732fb285d83c2f13437c26a5f923fec2c33d55",
        ),
    }
    for path, (expected_size, expected_sha) in expected_assets.items():
        data = (ROOT / path).read_bytes()
        if len(data) != expected_size or git_blob_sha(path) != expected_sha:
            fail(f"v0.8 alpha.4.8 Classic asset contract changed: {path}")

    for token in (
        'IDB_CLASSIC_SHORTCUT BITMAP "resources/classic_shortcut.bmp"',
        'IDB_CLASSIC_CLOSE BITMAP "resources/classic_close.bmp"',
        'IDB_CLASSIC_BACKGROUND BITMAP "resources/classic_bg.bmp"',
        "FILEVERSION 0,8,0,78",
        "PRODUCTVERSION 0,8,0,78",
        "0.8.0-alpha.4.8",
    ):
        if token not in resources:
            fail(f"v0.8 alpha.4.8 resource/version contract missing: {token}")

    for token in (
        'version="0.8.0.78"',
        ">PerMonitorV2</dpiAwareness>",
    ):
        if token not in manifest:
            fail(f"v0.8 alpha.4.8 manifest DPI/version contract missing: {token}")

    for forbidden in (
        "#include <gdiplus.h>",
        "Gdiplus::",
        "LoadClassicJpegResource(",
        "CreateStreamOnHGlobal(",
        "GlobalAlloc(",
        "hint_",
        "UpdateHint",
        "UpdateClassicHintLayout",
        "TextId::ClassicHint",
    ):
        if forbidden in launcher or forbidden in launcher_h:
            fail(f"v0.8 alpha.4.8 removed Classic path returned: {forbidden}")

    if "\n        gdiplus\n" in cmake:
        fail("v0.8 alpha.4.8 ALTRunNext must not link GDI+")

    for token in (
        "struct ClassicLauncherDpiMetrics",
        "ClassicLauncherMetricsForDpi(",
        "Scale(420, dpi)",
        "Scale(250, dpi)",
        "Scale(404, dpi)",
        "Scale(164, dpi)",
        "Scale(226, dpi)",
        "Scale(16, dpi)",
        "Scale(23, dpi)",
        "Scale(230, dpi)",
        "Scale(25, dpi)",
        "kClassicSeparatorPhysicalThickness = 1",
        "kModernCompactLauncherMetrics{\n        620,\n        32,\n        9,",
    ):
        if token not in metrics:
            fail(f"v0.8 alpha.4.8 DPI metrics contract missing: {token}")

    for token in (
        "classicDpiMetrics_{",
        "ClassicLauncherMetricsForDpi(\n                96)",
    ):
        if token not in launcher_h:
            fail(f"v0.8 alpha.4.8 cached DPI declaration missing: {token}")

    if launcher.count("ClassicLauncherMetricsForDpi(") != 2:
        fail(
            "v0.8 alpha.4.8 Classic DPI metrics must be computed "
            "only at create/DPI-change boundaries"
        )

    for token in (
        "classicDpiMetrics_.rowHeight",
        "classicDpiMetrics_.cornerDiameter",
        "classicMetrics.clientWidth",
        "classicMetrics.input.left",
        "classicMetrics.results.height",
        "classicMetrics.command.top",
        "classicDpiMetrics_.glyphSize",
        "classicMetrics.numberDividerX",
        "classicMetrics.shortcutDividerX",
        "ui::kClassicSeparatorPhysicalThickness",
        "WM_DPICHANGED",
        "dpi_ = HIWORD(wParam)",
        "GetDpiForWindow(hwnd_)",
        "ApplyFonts();\n        Layout();\n        UpdateWindowChrome();",
        "SetStretchBltMode(\n        dc,\n        COLORONCOLOR)",
    ):
        if token not in launcher:
            fail(f"v0.8 alpha.4.8 Launcher DPI wiring missing: {token}")

    launcher_compact = re.sub(r"\s+", "", launcher)
    if "classicDpiMetrics_.dragHeight" not in launcher_compact:
        fail("v0.8 alpha.4.8 cached Classic drag-height wiring missing")

    for token in (
        "96u",
        "120u",
        "144u",
        "192u",
        "420,\n                250",
        "525,\n                313",
        "630,\n                375",
        "840,\n                500",
        "ui::Scale(4, expected.dpi)",
        "ui::Scale(6, expected.dpi)",
        "kClassicSeparatorPhysicalThickness",
    ):
        if token not in ui_tests:
            fail(f"v0.8 alpha.4.8 automated DPI coverage missing: {token}")

    for token in (
        "kClassicLauncherPrimaryLogicalHeight96 = -16",
        "kClassicLauncherAuxiliaryLogicalHeight96 = -13",
        'L"SimSun"',
        "ANSI_CHARSET",
        "DEFAULT_QUALITY",
    ):
        if token not in typography:
            fail(f"v0.8 alpha.4.8 Classic typography baseline regressed: {token}")

    for token in (
        "GetSysColorBrush(",
        "GetStockObject(",
        "DC_BRUSH",
        "SetDCBrushColor(",
        "DT_PATH_ELLIPSIS",
        "        255,\n        LWA_ALPHA",
        "ResultIconWorkerLoop()",
        "kIconReadyMessage",
        "MergeLauncherResultsRanked(",
    ):
        if token not in launcher and token not in launcher_h:
            fail(f"v0.8 alpha.4.8 frozen Classic/performance contract regressed: {token}")

    for token in ("NM_CLICK", "NM_DBLCLK", "ToggleResultRowSelection("):
        if token not in path_cpp:
            fail(f"v0.8 alpha.4.8 Path Conversion rapid-click regression: {token}")

    for token in ("BN_CLICKED", "BN_DOUBLECLICKED", "toggleActivated", "ToggleAdvanced();"):
        if token not in editor_cpp:
            fail(f"v0.8 alpha.4.8 Shortcut Editor rapid-click regression: {token}")

    for token in (
        'L"ALTRunNext.UpdateDispatch"',
        "UpdateDispatchWindowProc(",
        "CreateUpdateDispatchWindow()",
        "PostUpdateStatusNotification(",
        "HWND_MESSAGE",
        "kUpdateReconcileTimerId",
    ):
        if token not in app_cpp and token not in app_h:
            fail(f"v0.8 alpha.4.8 updater architecture regressed: {token}")

    for token in (
        '"0.8.0-alpha.4.7"',
        '"0.8.0-alpha.4.8"',
        "UpdateChannel::Stable",
    ):
        if token not in update_tests:
            fail(f"v0.8 alpha.4.8 update ordering/default coverage missing: {token}")

    for token in (
        "v0.8.0-alpha.4.8 Classic DPI audit",
        "525×313",
        "630×375",
        "840×500",
        "one physical pixel",
        "Per-Monitor V2",
        "bitmap/glyph sharpness",
    ):
        if token not in desktop_validation:
            fail(f"v0.8 alpha.4.8 desktop DPI checklist missing: {token}")

    readme = read("README.md")
    changelog = read("CHANGELOG.md")
    roadmap = read("ROADMAP.md")

    for token in (
        "v0.8.0-alpha.4.8 — Classic DPI Audit",
        "ClassicLauncherMetricsForDpi()",
        "96 / 120 / 144 / 192 DPI",
        "one physical pixel",
        "0.8.0.78",
    ):
        if token not in readme:
            fail(f"v0.8 alpha.4.8 README contract missing: {token}")

    for token in (
        "## 0.8.0-alpha.4.8",
        "96/120/144/192 DPI",
        "Per-Monitor V2",
        "0.8.0.78",
    ):
        if token not in changelog:
            fail(f"v0.8 alpha.4.8 changelog contract missing: {token}")

    for token in (
        "v0.8.0-alpha.4.8",
        "Classic DPI audit",
        "100%/125%/150%/200%",
        "real-Windows Per-Monitor V2 visual validation",
    ):
        if token not in roadmap:
            fail(f"v0.8 alpha.4.8 roadmap missing: {token}")

    print(
        "v0.8.0-alpha.4.8 Classic DPI audit verified:",
        "| cached frozen DPI geometry contract",
        "| 96/120/144/192 automated snapshots",
        "| one-physical-pixel dividers",
        "| Per-Monitor V2 manual visual matrix retained",
        "| Modern/search/providers/updater frozen",
    )
    raise SystemExit(0)


if version == "0.8.0-alpha.4.7":
    import hashlib

    expected_schemas = {
        "kSettingsSchemaVersion": 9,
        "kCommandsSchemaVersion": 2,
        "kUsageSchemaVersion": 1,
    }
    for name, expected in expected_schemas.items():
        actual = cpp_int("src/core/ConfigIO.hpp", name)
        if actual != expected:
            fail(f"v0.8 alpha.4.7 {name}={actual}, expected {expected}")

    if cpp_int("src/core/ProviderCache.cpp", "kProviderCacheSchemaVersion") != 2:
        fail("v0.8 alpha.4.7 must keep provider-cache schemaVersion 2")

    launcher = read("src/ui/LauncherWindow.cpp")
    launcher_h = read("src/ui/LauncherWindow.hpp")
    metrics = read("src/ui/UiMetrics.hpp")
    typography = read("src/ui/UiTypography.cpp")
    resources = read("src/resources.rc")
    resource_ids = read("src/ResourceIds.h")
    cmake = read("CMakeLists.txt")
    path_cpp = read("src/ui/ShortcutPathConverterDialog.cpp")
    editor_cpp = read("src/ui/ShortcutEditorDialog.cpp")
    app_cpp = read("src/app/App.cpp")
    app_h = read("src/app/App.hpp")

    def git_blob_sha(path: str) -> str:
        data = (ROOT / path).read_bytes()
        header = f"blob {len(data)}\0".encode("ascii")
        return hashlib.sha1(header + data).hexdigest()

    # Keep the author-approved JPEG as the immutable master/provenance asset,
    # but use a deterministic pre-decoded BMP in the runtime resource table.
    expected_assets = {
        "src/resources/classic_bg.jpg": (
            74289,
            "daf552e9f948335575ae2701ca7ce0308c7c288c",
        ),
        "src/resources/classic_bg.bmp": (
            475578,
            "bbced49d20184051cf8ad48b153b022cecfecd50",
        ),
        "src/resources/classic_shortcut.bmp": (
            2554,
            "eee00956b449975f05e63a38e4da4ea29e01c240",
        ),
        "src/resources/classic_close.bmp": (
            1954,
            "51732fb285d83c2f13437c26a5f923fec2c33d55",
        ),
    }
    for path, (expected_size, expected_sha) in expected_assets.items():
        data = (ROOT / path).read_bytes()
        if len(data) != expected_size:
            fail(
                f"v0.8 alpha.4.7 {path} size={len(data)}, "
                f"expected {expected_size}"
            )
        if git_blob_sha(path) != expected_sha:
            fail(
                f"v0.8 alpha.4.7 {path} changed from the "
                "authorized/generated asset contract"
            )

    bmp = (ROOT / "src/resources/classic_bg.bmp").read_bytes()
    if bmp[:2] != b"BM":
        fail("v0.8 alpha.4.7 Classic runtime background is not BMP")
    width = int.from_bytes(bmp[18:22], "little", signed=True)
    height = int.from_bytes(bmp[22:26], "little", signed=True)
    bpp = int.from_bytes(bmp[28:30], "little")
    if (width, height, bpp) != (444, 357, 24):
        fail(
            "v0.8 alpha.4.7 Classic runtime BMP must remain "
            "444x357x24"
        )

    for token in (
        "#define IDB_CLASSIC_SHORTCUT 201",
        "#define IDB_CLASSIC_CLOSE 202",
        "#define IDB_CLASSIC_BACKGROUND 203",
    ):
        if token not in resource_ids:
            fail(f"v0.8 alpha.4.7 resource ID missing: {token}")

    for token in (
        'IDB_CLASSIC_SHORTCUT BITMAP "resources/classic_shortcut.bmp"',
        'IDB_CLASSIC_CLOSE BITMAP "resources/classic_close.bmp"',
        'IDB_CLASSIC_BACKGROUND BITMAP "resources/classic_bg.bmp"',
        "FILEVERSION 0,8,0,77",
        "PRODUCTVERSION 0,8,0,77",
        "0.8.0-alpha.4.7",
    ):
        if token not in resources:
            fail(f"v0.8 alpha.4.7 resource/version contract missing: {token}")

    for forbidden in (
        'RCDATA "resources/classic_bg.jpg"',
        "IDR_CLASSIC_BACKGROUND",
    ):
        if forbidden in resources or forbidden in resource_ids:
            fail(f"v0.8 alpha.4.7 old JPEG runtime resource survived: {forbidden}")

    # Runtime skin loading must be native RT_BITMAP only. Keep objbase/COM for
    # the independent async result-icon worker, but remove Classic GDI+ decode.
    for token in (
        "IDB_CLASSIC_BACKGROUND",
        "LoadImageW(",
        "LR_CREATEDIBSECTION",
        "classicBackgroundSize_",
        "classicBitmapDc_",
        "CreateCompatibleDC(nullptr)",
        "PaintClassicBackground(",
        "StretchBlt(",
        "#include <objbase.h>",
        "CoInitializeEx(",
    ):
        if token not in launcher and token not in launcher_h:
            fail(f"v0.8 alpha.4.7 native bitmap path missing: {token}")

    for forbidden in (
        "#include <gdiplus.h>",
        "Gdiplus::",
        "LoadClassicJpegResource(",
        "CreateStreamOnHGlobal(",
        "GlobalAlloc(",
        "GlobalLock(",
        "GlobalFree(",
        "std::memcpy(",
        "CreateCompatibleDC(dc)",
    ):
        if forbidden in launcher:
            fail(f"v0.8 alpha.4.7 runtime decode/allocation path survived: {forbidden}")

    if "\n        gdiplus\n" in cmake:
        fail("v0.8 alpha.4.7 ALTRunNext must not link GDI+")

    # Classic repainting should not allocate a brush/pen for every result row.
    for token in (
        "GetSysColorBrush(",
        "GetStockObject(",
        "DC_BRUSH",
        "SetDCBrushColor(",
        "RECT firstSeparator{",
        "RECT secondSeparator{",
        "FillRect(\n            item->hDC,\n            &firstSeparator",
        "FillRect(\n            item->hDC,\n            &secondSeparator",
    ):
        if token not in launcher:
            fail(f"v0.8 alpha.4.7 lean Classic repaint missing: {token}")

    for forbidden in (
        "HPEN separator =\n            CreatePen(",
        "HDC source = CreateCompatibleDC(dc);",
    ):
        if forbidden in launcher:
            fail(f"v0.8 alpha.4.7 per-paint Classic GDI churn survived: {forbidden}")

    # Preserve the real-Windows alpha.4.6 visual/interaction closeout.
    for token in (
        "MoveWindow(\n        edit_,\n        DpiScale(8),\n        DpiScale(30),\n        DpiScale(404),\n        DpiScale(22)",
        "MoveWindow(\n        list_,\n        DpiScale(8),\n        DpiScale(56),\n        DpiScale(404),\n        DpiScale(164)",
        "MoveWindow(\n        classicPreview_,\n        DpiScale(8),\n        DpiScale(226),\n        DpiScale(404),\n        DpiScale(16)",
        "numberColumnLogical = 23",
        "shortcutColumnLogical = 230",
        "DT_END_ELLIPSIS",
        "DT_PATH_ELLIPSIS",
        "        255,\n        LWA_ALPHA",
    ):
        if token not in launcher:
            fail(f"v0.8 alpha.4.7 alpha.4.6 visual baseline regressed: {token}")

    for forbidden in (
        "hint_",
        "UpdateHint",
        "UpdateClassicHintLayout",
        "TextId::ClassicHint",
    ):
        if forbidden in launcher or forbidden in launcher_h:
            fail(f"v0.8 alpha.4.7 deleted Hint code returned: {forbidden}")

    for token in (
        "kClassicLauncherPrimaryLogicalHeight96 = -16",
        "kClassicLauncherAuxiliaryLogicalHeight96 = -13",
        'L"SimSun"',
        "ANSI_CHARSET",
        "DEFAULT_QUALITY",
    ):
        if token not in typography:
            fail(f"v0.8 alpha.4.7 Classic font baseline regressed: {token}")

    for token in (
        "kClassicLauncherMetrics{\n        420,\n        16,\n        10,",
        "kModernCompactLauncherMetrics{\n        620,\n        32,\n        9,",
    ):
        if token not in metrics:
            fail(f"v0.8 alpha.4.7 launcher metrics regressed: {token}")

    # Modern Compact and mature alpha.3 architecture stay frozen.
    for token in (
        "const int margin =\n            DpiScale(12);",
        "const int inputHeight =\n            DpiScale(36);",
        "ResultIconWorkerLoop()",
        "kIconReadyMessage",
        "MergeLauncherResultsRanked(",
    ):
        if token not in launcher and token not in launcher_h:
            fail(f"v0.8 alpha.4.7 Modern/performance contract regressed: {token}")

    for token in ("NM_CLICK", "NM_DBLCLK", "ToggleResultRowSelection("):
        if token not in path_cpp:
            fail(f"v0.8 alpha.4.7 Path Conversion rapid-click regression: {token}")

    for token in ("BN_CLICKED", "BN_DOUBLECLICKED", "toggleActivated", "ToggleAdvanced();"):
        if token not in editor_cpp:
            fail(f"v0.8 alpha.4.7 Shortcut Editor rapid-click regression: {token}")

    for token in (
        'L"ALTRunNext.UpdateDispatch"',
        "UpdateDispatchWindowProc(",
        "CreateUpdateDispatchWindow()",
        "PostUpdateStatusNotification(",
        "HWND_MESSAGE",
        "kUpdateReconcileTimerId",
    ):
        if token not in app_cpp and token not in app_h:
            fail(f"v0.8 alpha.4.7 updater architecture regressed: {token}")

    update_tests = read("tests/UpdatePolicyTests.cpp")
    for token in (
        '"0.8.0-alpha.4.6"',
        '"0.8.0-alpha.4.7"',
        "UpdateChannel::Stable",
    ):
        if token not in update_tests:
            fail(f"v0.8 alpha.4.7 update ordering/default coverage missing: {token}")

    version_script = read("scripts/verify_version.py")
    package_script = read("scripts/verify_package.ps1")
    for token in (
        "revision = 70 + (channel_number - 4) * 100 + channel_patch",
        "revision = 10000 + channel_number",
        "revision = 20000 + channel_number",
        "revision = 30000",
    ):
        if token not in version_script:
            fail(f"v0.8 alpha.4.7 Python fixed-version mapping missing: {token}")
    for token in (
        "(($channelNumber - 4) * 100)",
        "$revision = 10000 + $channelNumber",
        "$revision = 20000 + $channelNumber",
        "$revision = 30000",
    ):
        if token not in package_script:
            fail(f"v0.8 alpha.4.7 package fixed-version mapping missing: {token}")

    readme = read("README.md")
    changelog = read("CHANGELOG.md")
    roadmap = read("ROADMAP.md")
    notices = read("THIRD_PARTY_NOTICES.md")

    for token in (
        "Classic Lean Render Path",
        "0.8.0.77",
        "444×357",
        "24-bit BMP",
        "runtime JPEG decoding is gone",
        "single memory DC",
        "system brushes",
        "DC_BRUSH",
    ):
        if token not in readme:
            fail(f"v0.8 alpha.4.7 README contract missing: {token}")

    for token in (
        "## 0.8.0-alpha.4.7",
        "0.8.0.77",
        "444×357 / 24-bit",
        "LoadImageW",
        "gdiplus",
        "reusable memory DC",
        "system brushes",
        "DC_BRUSH",
    ):
        if token not in changelog:
            fail(f"v0.8 alpha.4.7 changelog contract missing: {token}")

    for token in (
        "v0.8.0-alpha.4.7",
        "lean visual skin",
        "no runtime GDI+ JPEG decode",
        "allocation-free Classic row backgrounds/separators",
        "Modern Compact refinement follows",
    ):
        if token not in roadmap:
            fail(f"v0.8 alpha.4.7 roadmap missing: {token}")

    for token in (
        "classic_bg.jpg",
        "classic_bg.bmp",
        "source/provenance asset",
        "native runtime loading",
    ):
        if token not in notices:
            fail(f"v0.8 alpha.4.7 provenance notice missing: {token}")

    print(
        "v0.8.0-alpha.4.7 Classic lean render path verified:",
        "| pre-decoded 444x357x24 BMP",
        "| no runtime GDI+ JPEG decode",
        "| one cached bitmap DC",
        "| zero per-row Classic brush/pen allocation",
        "| Modern/search/providers/updater frozen",
    )
    raise SystemExit(0)


if version == "0.8.0-alpha.4.6":
    expected_schemas = {
        "kSettingsSchemaVersion": 9,
        "kCommandsSchemaVersion": 2,
        "kUsageSchemaVersion": 1,
    }
    for name, expected in expected_schemas.items():
        actual = cpp_int("src/core/ConfigIO.hpp", name)
        if actual != expected:
            fail(f"v0.8 alpha.4.6 {name}={actual}, expected {expected}")

    if cpp_int("src/core/ProviderCache.cpp", "kProviderCacheSchemaVersion") != 2:
        fail("v0.8 alpha.4.6 must keep provider-cache schemaVersion 2")

    launcher = read("src/ui/LauncherWindow.cpp")
    launcher_h = read("src/ui/LauncherWindow.hpp")
    localization_h = read("src/core/Localization.hpp")
    localization_cpp = read("src/core/Localization.cpp")
    metrics = read("src/ui/UiMetrics.hpp")
    typography = read("src/ui/UiTypography.cpp")
    resources = read("src/resources.rc")
    path_cpp = read("src/ui/ShortcutPathConverterDialog.cpp")
    editor_cpp = read("src/ui/ShortcutEditorDialog.cpp")
    app_cpp = read("src/app/App.cpp")
    app_h = read("src/app/App.hpp")

    # Hint is intentionally gone, not hidden. Keep this as a hard cleanup
    # contract so future Classic work cannot quietly rebuild dead overlay code.
    for forbidden in (
        "hint_",
        "UpdateHint",
        "UpdateClassicHintLayout",
        "TextId::ClassicHint",
        "reinterpret_cast<HMENU>(1004)",
        "item->CtlID == 1004",
        "kHintOriginalLeftLogical",
        "kHintMinWidthLogical",
    ):
        if forbidden in launcher or forbidden in launcher_h:
            fail(f"v0.8 alpha.4.6 stale Hint launcher code survived: {forbidden}")

    if "ClassicHint" in localization_h or "ClassicHint" in localization_cpp:
        fail("v0.8 alpha.4.6 ClassicHint localization entry must be deleted")

    # Input stays deliberately clean and owns the whole Classic strip.
    for token in (
        "MoveWindow(\n        edit_,\n        DpiScale(8),\n        DpiScale(30),\n        DpiScale(404),\n        DpiScale(22)",
        'EM_SETCUEBANNER,\n        TRUE,\n        reinterpret_cast<LPARAM>(\n            IsModern() ? app_.Text(TextId::SearchPlaceholder).data() : L"")',
    ):
        if token not in launcher:
            fail(f"v0.8 alpha.4.6 clean input contract missing: {token}")

    # Correct the source geometry rather than compensating with another line.
    for token in (
        "MoveWindow(\n        list_,\n        DpiScale(8),\n        DpiScale(56),\n        DpiScale(404),\n        DpiScale(164)",
        "MoveWindow(\n        classicPreview_,\n        DpiScale(8),\n        DpiScale(226),\n        DpiScale(404),\n        DpiScale(16)",
    ):
        if token not in launcher:
            fail(f"v0.8 alpha.4.6 bottom geometry contract missing: {token}")

    if "DpiScale(160),\n        TRUE);\n\n    MoveWindow(\n        preview_" in launcher:
        fail("v0.8 alpha.4.6 old 160px Classic ListBox geometry survived")

    # Command line is display-only owner draw with native path-aware middle
    # ellipsis. No extra helper library is needed.
    for token in (
        "classicPreview_ = CreateWindowExW(",
        'L"STATIC"',
        "SS_OWNERDRAW",
        "reinterpret_cast<HMENU>(1005)",
        "item->CtlID == 1005",
        "bottomBrush_",
        "auxiliaryFont_",
        "RGB(128, 128, 128)",
        "DT_SINGLELINE",
        "DT_VCENTER",
        "DT_PATH_ELLIPSIS",
        "DT_NOPREFIX",
    ):
        if token not in launcher:
            fail(f"v0.8 alpha.4.6 command ellipsis contract missing: {token}")

    for forbidden in (
        "ES_AUTOHSCROLL |\n            ES_READONLY",
        "PathCompactPath",
    ):
        if forbidden in launcher:
            fail(f"v0.8 alpha.4.6 old/heavier command path survived: {forbidden}")

    # Preserve Classic+ fixed columns and full-fidelity colors.
    for token in (
        "numberColumnLogical = 23",
        "shortcutColumnLogical = 230",
        "classicTextFlags",
        "DT_END_ELLIPSIS",
        "CreatePen(\n                PS_SOLID,\n                1,",
        "        255,\n        LWA_ALPHA",
        "IDR_CLASSIC_BACKGROUND",
        "DpiScale(420)",
        "DpiScale(250)",
        "DpiScale(12)",
    ):
        if token not in launcher:
            fail(f"v0.8 alpha.4.6 Classic+ baseline regressed: {token}")

    for token in (
        "kClassicLauncherPrimaryLogicalHeight96 = -16",
        "kClassicLauncherAuxiliaryLogicalHeight96 = -13",
        'L"SimSun"',
        "ANSI_CHARSET",
        "DEFAULT_QUALITY",
    ):
        if token not in typography:
            fail(f"v0.8 alpha.4.6 Classic font baseline regressed: {token}")

    for token in (
        'IDB_CLASSIC_SHORTCUT BITMAP "resources/classic_shortcut.bmp"',
        'IDB_CLASSIC_CLOSE BITMAP "resources/classic_close.bmp"',
        'IDR_CLASSIC_BACKGROUND RCDATA "resources/classic_bg.jpg"',
        "FILEVERSION 0,8,0,76",
        "PRODUCTVERSION 0,8,0,76",
        "0.8.0-alpha.4.6",
    ):
        if token not in resources:
            fail(f"v0.8 alpha.4.6 resource/version contract missing: {token}")

    for token in (
        "kClassicLauncherMetrics{\n        420,\n        16,\n        10,",
        "kModernCompactLauncherMetrics{\n        620,\n        32,\n        9,",
    ):
        if token not in metrics:
            fail(f"v0.8 alpha.4.6 launcher metrics regressed: {token}")

    # Modern Compact and mature alpha.3 foundations remain frozen.
    for token in (
        "const int margin =\n            DpiScale(12);",
        "const int inputHeight =\n            DpiScale(36);",
        "ResultIconWorkerLoop()",
        "kIconReadyMessage",
        "MergeLauncherResultsRanked(",
    ):
        if token not in launcher and token not in launcher_h:
            fail(f"v0.8 alpha.4.6 Modern/performance contract regressed: {token}")

    for token in ("NM_CLICK", "NM_DBLCLK", "ToggleResultRowSelection("):
        if token not in path_cpp:
            fail(f"v0.8 alpha.4.6 Path Conversion rapid-click regression: {token}")

    for token in ("BN_CLICKED", "BN_DOUBLECLICKED", "toggleActivated", "ToggleAdvanced();"):
        if token not in editor_cpp:
            fail(f"v0.8 alpha.4.6 Shortcut Editor rapid-click regression: {token}")

    for token in (
        'L"ALTRunNext.UpdateDispatch"',
        "UpdateDispatchWindowProc(",
        "CreateUpdateDispatchWindow()",
        "PostUpdateStatusNotification(",
        "HWND_MESSAGE",
        "kUpdateReconcileTimerId",
    ):
        if token not in app_cpp and token not in app_h:
            fail(f"v0.8 alpha.4.6 updater architecture regressed: {token}")

    update_tests = read("tests/UpdatePolicyTests.cpp")
    for token in (
        '"0.8.0-alpha.4.5"',
        '"0.8.0-alpha.4.6"',
        "UpdateChannel::Stable",
    ):
        if token not in update_tests:
            fail(f"v0.8 alpha.4.6 update ordering/default coverage missing: {token}")

    version_script = read("scripts/verify_version.py")
    package_script = read("scripts/verify_package.ps1")
    for token in (
        "revision = 70 + (channel_number - 4) * 100 + channel_patch",
        "revision = 10000 + channel_number",
        "revision = 20000 + channel_number",
        "revision = 30000",
    ):
        if token not in version_script:
            fail(f"v0.8 alpha.4.6 Python fixed-version mapping missing: {token}")
    for token in (
        "(($channelNumber - 4) * 100)",
        "$revision = 10000 + $channelNumber",
        "$revision = 20000 + $channelNumber",
        "$revision = 30000",
    ):
        if token not in package_script:
            fail(f"v0.8 alpha.4.6 package fixed-version mapping missing: {token}")

    readme = read("README.md")
    changelog = read("CHANGELOG.md")
    roadmap = read("ROADMAP.md")

    for token in (
        "Classic Input Cleanup & Bottom Bar Hardening",
        "0.8.0.76",
        "removed completely",
        "160 to 164 logical pixels",
        "DT_PATH_ELLIPSIS",
        "No additional text-layout library",
    ):
        if token not in readme:
            fail(f"v0.8 alpha.4.6 README contract missing: {token}")

    for token in (
        "## 0.8.0-alpha.4.6",
        "0.8.0.76",
        "Hint implementation completely",
        "404×160 to 404×164",
        "DT_PATH_ELLIPSIS",
        "no new runtime library",
    ):
        if token not in changelog:
            fail(f"v0.8 alpha.4.6 changelog contract missing: {token}")

    for token in (
        "v0.8.0-alpha.4.6",
        "removes the Classic Hint architecture completely",
        "404×164",
        "DT_PATH_ELLIPSIS",
        "Modern Compact refinement follows",
    ):
        if token not in roadmap:
            fail(f"v0.8 alpha.4.6 roadmap missing: {token}")

    print(
        "v0.8.0-alpha.4.6 Classic cleanup verified:",
        "| Hint code/localization fully deleted",
        "| ListBox 404x164",
        "| owner-drawn command DT_PATH_ELLIPSIS",
        "| no new helper dependency",
        "| Modern/search/providers/updater frozen",
    )
    raise SystemExit(0)


if version == "0.8.0-alpha.4.5":
    expected_schemas = {
        "kSettingsSchemaVersion": 9,
        "kCommandsSchemaVersion": 2,
        "kUsageSchemaVersion": 1,
    }
    for name, expected in expected_schemas.items():
        actual = cpp_int("src/core/ConfigIO.hpp", name)
        if actual != expected:
            fail(f"v0.8 alpha.4.5 {name}={actual}, expected {expected}")

    if cpp_int("src/core/ProviderCache.cpp", "kProviderCacheSchemaVersion") != 2:
        fail("v0.8 alpha.4.5 must keep provider-cache schemaVersion 2")

    launcher = read("src/ui/LauncherWindow.cpp")
    launcher_h = read("src/ui/LauncherWindow.hpp")
    metrics = read("src/ui/UiMetrics.hpp")
    typography = read("src/ui/UiTypography.cpp")
    resources = read("src/resources.rc")
    path_cpp = read("src/ui/ShortcutPathConverterDialog.cpp")
    editor_cpp = read("src/ui/ShortcutEditorDialog.cpp")
    app_cpp = read("src/app/App.cpp")
    app_h = read("src/app/App.hpp")

    # Classic+ color fidelity: preserve the original black color key/skin,
    # but do not wash the whole launcher through the legacy 240 alpha.
    for token in (
        "SetLayeredWindowAttributes(",
        "RGB(0, 0, 0)",
        "        255,",
        "LWA_ALPHA",
        "LWA_COLORKEY",
        "PaintClassicBackground(",
        "IDR_CLASSIC_BACKGROUND",
        "DpiScale(420)",
        "DpiScale(250)",
        "DpiScale(12)",
    ):
        if token not in launcher:
            fail(f"v0.8 alpha.4.5 Classic color/skin contract missing: {token}")

    if "        240,\n        LWA_ALPHA" in launcher:
        fail("v0.8 alpha.4.5 must not globally fade Classic through alpha 240")

    # The Hint is a real owner-drawn overlay, not a tiny raw Win32 Edit.
    for token in (
        'L"STATIC"',
        "SS_OWNERDRAW",
        "void LauncherWindow::UpdateClassicHintLayout()",
        "kHintOriginalLeftLogical = 82",
        "kHintTopLogical = 35",
        "kHintRightLogical = 410",
        "kHintHeightLogical = 14",
        "kHintGapLogical = 8",
        "kHintMinWidthLogical = 104",
        "GetTextExtentPoint32W(",
        "EM_GETMARGINS",
        "HWND_TOP",
        "SWP_SHOWWINDOW",
        "item->CtlID == 1004",
        "DT_RIGHT",
        "DT_VCENTER",
        "DT_END_ELLIPSIS",
        "RGB(128, 128, 128)",
    ):
        if token not in launcher:
            fail(f"v0.8 alpha.4.5 Hint overlay contract missing: {token}")

    if "void UpdateClassicHintLayout();" not in launcher_h:
        fail("v0.8 alpha.4.5 Hint overlay declaration is missing")

    # Harden Classic rows with actual fixed columns. String-padding must not
    # be allowed to move visual separators for long/CJK result text.
    for token in (
        "numberColumnLogical = 23",
        "shortcutColumnLogical = 230",
        "textInsetLogical = 4",
        "classicTextFlags",
        "DT_END_ELLIPSIS",
        "DT_NOPREFIX",
        "GetSysColor(\n                      COLOR_HIGHLIGHTTEXT)",
        "CreatePen(\n                PS_SOLID,\n                1,",
        "MoveToEx(",
        "LineTo(",
        "showResultIcons",
        "iconColumnLogical = 18",
    ):
        if token not in launcher:
            fail(f"v0.8 alpha.4.5 fixed-column rendering contract missing: {token}")

    for forbidden in (
        "ClassicResultLine(",
        "kClassicShortcutFieldWidth = 25",
        'line.append(L"| ");',
        "ExtTextOutW(",
    ):
        if forbidden in launcher:
            fail(f"v0.8 alpha.4.5 legacy string-column path survived: {forbidden}")

    # Preserve the alpha.4.4 source-parity baseline underneath Classic+.
    for token in (
        "kClassicLauncherPrimaryLogicalHeight96 = -16",
        "kClassicLauncherAuxiliaryLogicalHeight96 = -13",
        'L"SimSun"',
        "ANSI_CHARSET",
        "DEFAULT_QUALITY",
    ):
        if token not in typography:
            fail(f"v0.8 alpha.4.5 Classic font baseline regressed: {token}")

    for token in (
        'IDB_CLASSIC_SHORTCUT BITMAP "resources/classic_shortcut.bmp"',
        'IDB_CLASSIC_CLOSE BITMAP "resources/classic_close.bmp"',
        'IDR_CLASSIC_BACKGROUND RCDATA "resources/classic_bg.jpg"',
        "FILEVERSION 0,8,0,75",
        "PRODUCTVERSION 0,8,0,75",
        "0.8.0-alpha.4.5",
    ):
        if token not in resources:
            fail(f"v0.8 alpha.4.5 resource/version contract missing: {token}")

    for token in (
        "kClassicLauncherMetrics{\n        420,\n        16,\n        10,",
        "kModernCompactLauncherMetrics{\n        620,\n        32,\n        9,",
    ):
        if token not in metrics:
            fail(f"v0.8 alpha.4.5 launcher metrics regressed: {token}")

    # Mature alpha.3 foundations and Modern Compact stay frozen.
    for token in (
        "const int margin =\n            DpiScale(12);",
        "const int inputHeight =\n            DpiScale(36);",
        "ResultIconWorkerLoop()",
        "kIconReadyMessage",
        "MergeLauncherResultsRanked(",
    ):
        if token not in launcher and token not in launcher_h:
            fail(f"v0.8 alpha.4.5 Modern/performance contract regressed: {token}")

    for token in ("NM_CLICK", "NM_DBLCLK", "ToggleResultRowSelection("):
        if token not in path_cpp:
            fail(f"v0.8 alpha.4.5 Path Conversion rapid-click regression: {token}")

    for token in ("BN_CLICKED", "BN_DOUBLECLICKED", "toggleActivated", "ToggleAdvanced();"):
        if token not in editor_cpp:
            fail(f"v0.8 alpha.4.5 Shortcut Editor rapid-click regression: {token}")

    for token in (
        'L"ALTRunNext.UpdateDispatch"',
        "UpdateDispatchWindowProc(",
        "CreateUpdateDispatchWindow()",
        "PostUpdateStatusNotification(",
        "HWND_MESSAGE",
        "kUpdateReconcileTimerId",
    ):
        if token not in app_cpp and token not in app_h:
            fail(f"v0.8 alpha.4.5 updater architecture regressed: {token}")

    update_tests = read("tests/UpdatePolicyTests.cpp")
    for token in (
        '"0.8.0-alpha.4.4"',
        '"0.8.0-alpha.4.5"',
        "UpdateChannel::Stable",
    ):
        if token not in update_tests:
            fail(f"v0.8 alpha.4.5 update ordering/default coverage missing: {token}")

    version_script = read("scripts/verify_version.py")
    package_script = read("scripts/verify_package.ps1")
    for token in (
        "revision = 70 + (channel_number - 4) * 100 + channel_patch",
        "revision = 10000 + channel_number",
        "revision = 20000 + channel_number",
        "revision = 30000",
    ):
        if token not in version_script:
            fail(f"v0.8 alpha.4.5 Python fixed-version mapping missing: {token}")
    for token in (
        "(($channelNumber - 4) * 100)",
        "$revision = 10000 + $channelNumber",
        "$revision = 20000 + $channelNumber",
        "$revision = 30000",
    ):
        if token not in package_script:
            fail(f"v0.8 alpha.4.5 package fixed-version mapping missing: {token}")

    readme = read("README.md")
    changelog = read("CHANGELOG.md")
    roadmap = read("ROADMAP.md")

    for token in (
        "Classic+ Rendering & Column Hardening",
        "0.8.0.75",
        "240 to 255",
        "owner-drawn overlay",
        "23 px",
        "230 px",
        "DT_END_ELLIPSIS",
        "104 logical pixels",
    ):
        if token not in readme:
            fail(f"v0.8 alpha.4.5 README contract missing: {token}")

    for token in (
        "## 0.8.0-alpha.4.5",
        "0.8.0.75",
        "240 to 255",
        "owner-drawn Classic Hint overlay",
        "x=23",
        "x=230",
        "DT_END_ELLIPSIS",
        "one-physical-pixel",
    ):
        if token not in changelog:
            fail(f"v0.8 alpha.4.5 changelog contract missing: {token}")

    for token in (
        "v0.8.0-alpha.4.5",
        "Classic+",
        "full-opacity Classic color fidelity",
        "fixed 23/230 logical result columns",
        "Modern Compact refinement follows",
    ):
        if token not in roadmap:
            fail(f"v0.8 alpha.4.5 roadmap missing: {token}")

    print(
        "v0.8.0-alpha.4.5 Classic+ rendering verified:",
        "| full-fidelity Classic alpha255",
        "| owner-drawn query-aware Hint",
        "| fixed 23/230 result columns",
        "| per-column ellipsis + 1px separators",
        "| Modern/search/providers/updater frozen",
    )
    raise SystemExit(0)


if version == "0.8.0-alpha.4.4":
    import hashlib

    expected_schemas = {
        "kSettingsSchemaVersion": 9,
        "kCommandsSchemaVersion": 2,
        "kUsageSchemaVersion": 1,
    }
    for name, expected in expected_schemas.items():
        actual = cpp_int("src/core/ConfigIO.hpp", name)
        if actual != expected:
            fail(f"v0.8 alpha.4.4 {name}={actual}, expected {expected}")

    if cpp_int("src/core/ProviderCache.cpp", "kProviderCacheSchemaVersion") != 2:
        fail("v0.8 alpha.4.4 must keep provider-cache schemaVersion 2")

    launcher = read("src/ui/LauncherWindow.cpp")
    launcher_h = read("src/ui/LauncherWindow.hpp")
    metrics = read("src/ui/UiMetrics.hpp")
    theme = read("src/ui/UiTheme.hpp")
    typography = read("src/ui/UiTypography.cpp")
    typography_h = read("src/ui/UiTypography.hpp")
    resources = read("src/resources.rc")
    resource_ids = read("src/ResourceIds.h")
    cmake = read("CMakeLists.txt")
    path_cpp = read("src/ui/ShortcutPathConverterDialog.cpp")
    editor_cpp = read("src/ui/ShortcutEditorDialog.cpp")
    app_cpp = read("src/app/App.cpp")
    app_h = read("src/app/App.hpp")

    def git_blob_sha(path: str) -> str:
        data = (ROOT / path).read_bytes()
        header = f"blob {len(data)}\0".encode("ascii")
        return hashlib.sha1(header + data).hexdigest()

    expected_assets = {
        "src/resources/classic_bg.jpg": (
            74289,
            "daf552e9f948335575ae2701ca7ce0308c7c288c",
        ),
        "src/resources/classic_shortcut.bmp": (
            2554,
            "eee00956b449975f05e63a38e4da4ea29e01c240",
        ),
        "src/resources/classic_close.bmp": (
            1954,
            "51732fb285d83c2f13437c26a5f923fec2c33d55",
        ),
    }
    for path, (expected_size, expected_sha) in expected_assets.items():
        data = (ROOT / path).read_bytes()
        if len(data) != expected_size:
            fail(
                f"v0.8 alpha.4.4 {path} size={len(data)}, "
                f"expected {expected_size}"
            )
        if git_blob_sha(path) != expected_sha:
            fail(
                f"v0.8 alpha.4.4 {path} no longer matches "
                "the authorized original asset"
            )

    bg = (ROOT / "src/resources/classic_bg.jpg").read_bytes()
    if bg[:2] != b"\xff\xd8" or bg[-2:] != b"\xff\xd9":
        fail("v0.8 alpha.4.4 Classic BG resource is not the original JPEG payload")

    for token in (
        "#define IDB_CLASSIC_SHORTCUT 201",
        "#define IDB_CLASSIC_CLOSE 202",
        "#define IDR_CLASSIC_BACKGROUND 203",
    ):
        if token not in resource_ids:
            fail(f"v0.8 alpha.4.4 resource ID missing: {token}")

    for token in (
        'IDB_CLASSIC_SHORTCUT BITMAP "resources/classic_shortcut.bmp"',
        'IDB_CLASSIC_CLOSE BITMAP "resources/classic_close.bmp"',
        'IDR_CLASSIC_BACKGROUND RCDATA "resources/classic_bg.jpg"',
    ):
        if token not in resources:
            fail(f"v0.8 alpha.4.4 resource declaration missing: {token}")

    for token in (
        "kClassicLauncherPrimaryLogicalHeight96 = -16",
        "kClassicLauncherAuxiliaryLogicalHeight96 = -13",
        'L"SimSun"',
        "ANSI_CHARSET",
        "DEFAULT_QUALITY",
        "spec.logicalHeight96",
        "spec.charset",
        "static_cast<int>(dpi),\n                  96",
    ):
        if token not in typography:
            fail(f"v0.8 alpha.4.4 exact Classic font contract missing: {token}")

    for token in (
        "int logicalHeight96{};",
        "BYTE charset{DEFAULT_CHARSET};",
    ):
        if token not in typography_h:
            fail(f"v0.8 alpha.4.4 UiFontSpec contract missing: {token}")

    for token in (
        "RGB(255, 255, 255)",
        "RGB(192, 220, 192)",
        "RGB(0, 0, 128)",
        "RGB(128, 128, 128)",
    ):
        if token not in theme:
            fail(f"v0.8 alpha.4.4 original Classic palette missing: {token}")

    for token in (
        '#include <gdiplus.h>',
        "LoadClassicJpegResource(",
        "IDR_CLASSIC_BACKGROUND",
        "classicBackgroundBitmap_",
        "PaintClassicBackground(",
        "StretchBlt(",
        "DpiScale(420)",
        "DpiScale(250)",
        "DpiScale(404)",
        "DpiScale(22)",
        "DpiScale(82)",
        "DpiScale(35)",
        "DpiScale(328)",
        "DpiScale(14)",
        "DpiScale(56)",
        "DpiScale(160)",
        "DpiScale(226)",
        "DpiScale(16)",
        "ES_RIGHT",
        "ES_READONLY",
        "EnableWindow(\n        hint_,\n        FALSE)",
        "setFrame(list_, 0);",
        "WS_EX_LAYERED",
        "SetLayeredWindowAttributes(",
        "        240,",
        "LWA_COLORKEY",
        "DpiScale(12)",
        "RGB(255, 255, 0)",
        "RGB(255, 0, 0)",
        "RGB(0, 0, 128)",
        "RGB(128, 128, 128)",
        'L"命令="',
        'L"CMD="',
        "ClassicResultLine(",
        "kClassicShortcutFieldWidth = 25",
        "ExtTextOutW(",
        "COLOR_HIGHLIGHT",
        "COLOR_HIGHLIGHTTEXT",
    ):
        if token not in launcher:
            fail(f"v0.8 alpha.4.4 source-parity launcher contract missing: {token}")

    for forbidden in (
        "constexpr int bands = 40",
        "Continuous Classic side rails",
        "railBands = 32",
        "inputWidthLogical = 190",
        "listHeightLogical = 162",
        "previewHeightLogical = 18",
        "MixColor(",
    ):
        if forbidden in launcher:
            fail(f"v0.8 alpha.4.4 screenshot-era Classic approximation survived: {forbidden}")

    if "HWND classicPreview_{};" not in launcher_h:
        fail("v0.8 alpha.4.4 dedicated Classic command Edit is missing")
    if "HBITMAP classicBackgroundBitmap_{};" not in launcher_h:
        fail("v0.8 alpha.4.4 embedded Classic background handle is missing")
    if "std::wstring titleText_{};" not in launcher_h:
        fail("v0.8 alpha.4.4 Classic title should start empty like original")

    if "gdiplus" not in cmake:
        fail("v0.8 alpha.4.4 must link GDI+ for embedded JPEG decoding")

    for token in (
        "kClassicLauncherMetrics{\n        420,\n        16,\n        10,",
        "kModernCompactLauncherMetrics{\n        620,\n        32,\n        9,",
    ):
        if token not in metrics:
            fail(f"v0.8 alpha.4.4 launcher metrics regressed: {token}")

    # Modern Compact and the mature alpha.3 interaction/update foundations
    # stay frozen while Classic is rebuilt from the original source baseline.
    for token in (
        "const int margin =\n            DpiScale(12);",
        "const int inputHeight =\n            DpiScale(36);",
        "ResultIconWorkerLoop()",
        "kIconReadyMessage",
        "showResultIcons",
        "MergeLauncherResultsRanked(",
    ):
        if token not in launcher and token not in launcher_h:
            fail(f"v0.8 alpha.4.4 Modern/performance contract regressed: {token}")

    for token in ("NM_CLICK", "NM_DBLCLK", "ToggleResultRowSelection("):
        if token not in path_cpp:
            fail(f"v0.8 alpha.4.4 Path Conversion rapid-click regression: {token}")

    for token in ("BN_CLICKED", "BN_DOUBLECLICKED", "toggleActivated", "ToggleAdvanced();"):
        if token not in editor_cpp:
            fail(f"v0.8 alpha.4.4 Shortcut Editor rapid-click regression: {token}")

    for token in (
        'L"ALTRunNext.UpdateDispatch"',
        "UpdateDispatchWindowProc(",
        "CreateUpdateDispatchWindow()",
        "PostUpdateStatusNotification(",
        "HWND_MESSAGE",
        "kUpdateReconcileTimerId",
    ):
        if token not in app_cpp and token not in app_h:
            fail(f"v0.8 alpha.4.4 updater architecture regressed: {token}")

    update_tests = read("tests/UpdatePolicyTests.cpp")
    for token in (
        '"0.8.0-alpha.4.3"',
        '"0.8.0-alpha.4.4"',
        "UpdateChannel::Stable",
    ):
        if token not in update_tests:
            fail(f"v0.8 alpha.4.4 update ordering/default coverage missing: {token}")

    version_script = read("scripts/verify_version.py")
    package_script = read("scripts/verify_package.ps1")
    for token in (
        "revision = 70 + (channel_number - 4) * 100 + channel_patch",
        "revision = 10000 + channel_number",
        "revision = 20000 + channel_number",
        "revision = 30000",
    ):
        if token not in version_script:
            fail(f"v0.8 alpha.4.4 Python fixed-version mapping missing: {token}")
    for token in (
        "(($channelNumber - 4) * 100)",
        "$revision = 10000 + $channelNumber",
        "$revision = 20000 + $channelNumber",
        "$revision = 30000",
    ):
        if token not in package_script:
            fail(f"v0.8 alpha.4.4 package fixed-version mapping missing: {token}")

    readme = read("README.md")
    changelog = read("CHANGELOG.md")
    roadmap = read("ROADMAP.md")
    notices = read("THIRD_PARTY_NOTICES.md")

    for token in (
        "Classic Source-Parity Foundation",
        "0.8.0.74",
        "74,289-byte",
        "420×250",
        "-16 @ 96 DPI",
        "-13 @ 96 DPI",
        "240/255",
        "8,30 / 404×22",
        "8,56 / 404×160",
        "8,226 / 404×16",
        "original background and corner glyph assets",
    ):
        if token not in readme:
            fail(f"v0.8 alpha.4.4 README contract missing: {token}")

    for token in (
        "## 0.8.0-alpha.4.4",
        "0.8.0.74",
        "420×250",
        "240/255",
        "-16/-13",
        "74,289-byte",
        "ExtTextOut",
    ):
        if token not in changelog:
            fail(f"v0.8 alpha.4.4 changelog contract missing: {token}")

    for token in (
        "v0.8.0-alpha.4.4",
        "source parity",
        "authorized BG.jpg skin",
        "Modern Compact refinement follows",
    ):
        if token not in roadmap:
            fail(f"v0.8 alpha.4.4 roadmap missing: {token}")

    for token in (
        "imgBackground.Picture.Data",
        "original `BG.jpg` JPEG payload",
        "btnShortCut.Glyph.Data",
        "btnClose.Glyph.Data",
        "These three visual assets",
    ):
        if token not in notices:
            fail(f"v0.8 alpha.4.4 provenance notice missing: {token}")

    print(
        "v0.8.0-alpha.4.4 Classic source parity verified:",
        "| original BG/glyph assets",
        "| 420x250 borderless + alpha240/radius12",
        "| DFM control geometry",
        "| -16/-13 SimSun GDI metrics",
        "| single-run Classic result text",
        "| Modern/search/providers/updater frozen",
    )
    raise SystemExit(0)


if version == "0.8.0-alpha.4.3":
    expected_schemas = {
        "kSettingsSchemaVersion": 9,
        "kCommandsSchemaVersion": 2,
        "kUsageSchemaVersion": 1,
    }
    for name, expected in expected_schemas.items():
        actual = cpp_int("src/core/ConfigIO.hpp", name)
        if actual != expected:
            fail(f"v0.8 alpha.4.3 {name}={actual}, expected {expected}")

    if cpp_int("src/core/ProviderCache.cpp", "kProviderCacheSchemaVersion") != 2:
        fail("v0.8 alpha.4.3 must keep provider-cache schemaVersion 2")

    launcher = read("src/ui/LauncherWindow.cpp")
    launcher_h = read("src/ui/LauncherWindow.hpp")
    metrics = read("src/ui/UiMetrics.hpp")
    typography = read("src/ui/UiTypography.cpp")
    resources = read("src/resources.rc")
    resource_ids = read("src/ResourceIds.h")
    cmake = read("CMakeLists.txt")
    path_cpp = read("src/ui/ShortcutPathConverterDialog.cpp")
    editor_cpp = read("src/ui/ShortcutEditorDialog.cpp")
    app_cpp = read("src/app/App.cpp")
    app_h = read("src/app/App.hpp")

    for token in (
        "kClassicLauncherPrimaryPointSize = 12",
        "kClassicLauncherAuxiliaryPointSize = 11",
        "kModernLauncherBodyPointSize = 10",
        "kModernLauncherTitlePointSize = 10",
        "DEFAULT_QUALITY",
        "CLEARTYPE_QUALITY",
    ):
        if token not in typography:
            fail(f"v0.8 alpha.4.3 typography contract missing: {token}")

    for token in (
        "#define IDB_CLASSIC_SHORTCUT 201",
        "#define IDB_CLASSIC_CLOSE 202",
    ):
        if token not in resource_ids:
            fail(f"v0.8 alpha.4.3 resource ID missing: {token}")

    for token in (
        'IDB_CLASSIC_SHORTCUT BITMAP "resources/classic_shortcut.bmp"',
        'IDB_CLASSIC_CLOSE BITMAP "resources/classic_close.bmp"',
    ):
        if token not in resources:
            fail(f"v0.8 alpha.4.3 bitmap resource declaration missing: {token}")

    def verify_bmp(path: str, expected_size: int, expected_bpp: int) -> None:
        data = (ROOT / path).read_bytes()
        if len(data) != expected_size:
            fail(
                f"v0.8 alpha.4.3 {path} size={len(data)}, "
                f"expected {expected_size}"
            )
        if data[:2] != b"BM":
            fail(f"v0.8 alpha.4.3 {path} is not a BMP")
        width = int.from_bytes(data[18:22], "little", signed=True)
        height = int.from_bytes(data[22:26], "little", signed=True)
        bpp = int.from_bytes(data[28:30], "little")
        declared_size = int.from_bytes(data[2:6], "little")
        if (width, height, bpp) != (25, 25, expected_bpp):
            fail(
                f"v0.8 alpha.4.3 {path} geometry/bpp "
                f"{width}x{height}x{bpp} does not match original glyph"
            )
        if declared_size != expected_size:
            fail(
                f"v0.8 alpha.4.3 {path} BMP header size "
                f"{declared_size} != {expected_size}"
            )

    verify_bmp("src/resources/classic_shortcut.bmp", 2554, 32)
    verify_bmp("src/resources/classic_close.bmp", 1954, 24)

    for token in (
        '#include "../ResourceIds.h"',
        "LoadImageW(",
        "MAKEINTRESOURCEW(",
        "IDB_CLASSIC_SHORTCUT",
        "IDB_CLASSIC_CLOSE",
        "LR_CREATEDIBSECTION",
        "TransparentBlt(",
        "COLORONCOLOR",
        "kClassicGlyphSourceSize = 25",
        "classicShortcutBitmap_",
        "classicCloseBitmap_",
        "const int size = DpiScale(22);",
        "const int rightInset = DpiScale(6);",
        "const int top = DpiScale(4);",
        "PaintClassicLogo(dc, DpiScale(8), DpiScale(2));",
    ):
        if token not in launcher and token not in launcher_h:
            fail(f"v0.8 alpha.4.3 original-glyph integration missing: {token}")

    for forbidden in (
        "Clean-room vector recreation of the old skin",
        "Filled beveled X rather than two crossing strokes",
        "POINT backShadow[7]",
        "POINT star[10]",
    ):
        if forbidden in launcher:
            fail(f"v0.8 alpha.4.3 procedural Classic glyph survived: {forbidden}")

    if "msimg32" not in cmake:
        fail("v0.8 alpha.4.3 must link msimg32 for TransparentBlt")

    for token in (
        "kClassicLauncherMetrics{\n        420,\n        16,\n        10,",
        "kModernCompactLauncherMetrics{\n        620,\n        32,\n        9,",
    ):
        if token not in metrics:
            fail(f"v0.8 alpha.4.3 launcher metrics regressed: {token}")

    for token in (
        "const int inputHeight = DpiScale(36);",
        "const int margin = DpiScale(12);",
        "ResultIconWorkerLoop()",
        "kIconReadyMessage",
        "showResultIcons",
        "MergeLauncherResultsRanked(",
    ):
        if token not in launcher and token not in launcher_h:
            fail(f"v0.8 alpha.4.3 Modern/performance contract regressed: {token}")

    for token in ("NM_CLICK", "NM_DBLCLK", "ToggleResultRowSelection("):
        if token not in path_cpp:
            fail(f"v0.8 alpha.4.3 Path Conversion rapid-click regression: {token}")

    for token in ("BN_CLICKED", "BN_DOUBLECLICKED", "toggleActivated", "ToggleAdvanced();"):
        if token not in editor_cpp:
            fail(f"v0.8 alpha.4.3 Shortcut Editor rapid-click regression: {token}")

    for token in (
        'L"ALTRunNext.UpdateDispatch"',
        "UpdateDispatchWindowProc(",
        "CreateUpdateDispatchWindow()",
        "PostUpdateStatusNotification(",
        "HWND_MESSAGE",
        "kUpdateReconcileTimerId",
    ):
        if token not in app_cpp and token not in app_h:
            fail(f"v0.8 alpha.4.3 updater architecture regressed: {token}")

    update_tests = read("tests/UpdatePolicyTests.cpp")
    for token in (
        '"0.8.0-alpha.4.2"',
        '"0.8.0-alpha.4.3"',
        "UpdateChannel::Stable",
    ):
        if token not in update_tests:
            fail(f"v0.8 alpha.4.3 update ordering/default coverage missing: {token}")

    readme = read("README.md")
    changelog = read("CHANGELOG.md")
    roadmap = read("ROADMAP.md")
    notices = read("THIRD_PARTY_NOTICES.md")

    for token in (
        "Original Classic Glyphs & Auxiliary Typography",
        "0.8.0.73",
        "10 pt to 11 pt",
        "btnShortCut",
        "btnClose",
        "author permission",
    ):
        if token not in readme:
            fail(f"v0.8 alpha.4.3 README contract missing: {token}")

    for token in (
        "## 0.8.0-alpha.4.3",
        "0.8.0.73",
        "10 pt to 11 pt",
        "25×25",
        "22×22",
    ):
        if token not in changelog:
            fail(f"v0.8 alpha.4.3 changelog contract missing: {token}")

    for token in (
        "v0.8.0-alpha.4.3",
        "original authorized Classic shortcut/close glyph bitmaps",
        "Modern Compact refinement follows",
    ):
        if token not in roadmap:
            fail(f"v0.8 alpha.4.3 roadmap missing: {token}")

    for token in (
        "btnShortCut.Glyph.Data",
        "btnClose.Glyph.Data",
        "etworker/ALTRun",
        "permission from the original ALTRun author",
    ):
        if token not in notices:
            fail(f"v0.8 alpha.4.3 provenance notice missing: {token}")

    print(
        "v0.8.0-alpha.4.3 Classic glyph/auxiliary typography verified:",
        "| original 25x25 glyph BMPs embedded",
        "| original corner geometry restored",
        "| Classic auxiliary 11pt",
        "| Modern/search/providers/updater frozen",
    )
    raise SystemExit(0)


if version == "0.8.0-alpha.4.2":
    expected_schemas = {
        "kSettingsSchemaVersion": 9,
        "kCommandsSchemaVersion": 2,
        "kUsageSchemaVersion": 1,
    }
    for name, expected in expected_schemas.items():
        actual = cpp_int("src/core/ConfigIO.hpp", name)
        if actual != expected:
            fail(f"v0.8 alpha.4.2 {name}={actual}, expected {expected}")

    if cpp_int("src/core/ProviderCache.cpp", "kProviderCacheSchemaVersion") != 2:
        fail("v0.8 alpha.4.2 must keep provider-cache schemaVersion 2")

    launcher = read("src/ui/LauncherWindow.cpp")
    launcher_h = read("src/ui/LauncherWindow.hpp")
    metrics = read("src/ui/UiMetrics.hpp")
    typography = read("src/ui/UiTypography.cpp")
    typography_h = read("src/ui/UiTypography.hpp")
    path_cpp = read("src/ui/ShortcutPathConverterDialog.cpp")
    editor_cpp = read("src/ui/ShortcutEditorDialog.cpp")
    app_cpp = read("src/app/App.cpp")
    app_h = read("src/app/App.hpp")

    for token in (
        "kClassicLauncherPrimaryPointSize = 12",
        "kClassicLauncherAuxiliaryPointSize = 10",
        "kModernLauncherBodyPointSize = 10",
        "kModernLauncherTitlePointSize = 10",
        'L"SimSun"',
        'L"Tahoma"',
        "DEFAULT_QUALITY",
        "CLEARTYPE_QUALITY",
        "spec.quality",
        "role == UiFontRole::LauncherTitle",
        "FW_NORMAL",
    ):
        if token not in typography:
            fail(f"v0.8 alpha.4.2 Classic typography contract missing: {token}")

    for token in ("LauncherAuxiliary", "DWORD quality{CLEARTYPE_QUALITY}"):
        if token not in typography_h:
            fail(f"v0.8 alpha.4.2 font role/spec contract missing: {token}")

    for token in (
        "HFONT auxiliaryFont_{};",
        "DeleteObject(auxiliaryFont_)",
        "UiFontRole::LauncherAuxiliary",
        "reinterpret_cast<WPARAM>(auxiliaryFont_)",
    ):
        if token not in launcher and token not in launcher_h:
            fail(f"v0.8 alpha.4.2 auxiliary-font wiring missing: {token}")

    for token in (
        "kClassicLauncherMetrics{\n        420,\n        16,\n        10,",
        "kModernCompactLauncherMetrics{\n        620,\n        32,\n        9,",
    ):
        if token not in metrics:
            fail(f"v0.8 alpha.4.2 launcher metrics regressed: {token}")

    for token in (
        "constexpr int inputHeightLogical = 22;",
        "titleHeight,\n        inputWidth,\n        inputHeight,",
        "RECT classicInputStrip",
        "&classicInputStrip",
        "accentBrush_",
    ):
        if token not in launcher:
            fail(f"v0.8 alpha.4.2 Classic input layout missing: {token}")

    for forbidden in ("inputEditHeightLogical", "inputEditOffset"):
        if forbidden in launcher:
            fail(f"v0.8 alpha.4.2 old 18px Edit compensation survived: {forbidden}")

    for token in (
        "const int inputHeight = DpiScale(36);",
        "const int margin = DpiScale(12);",
        "ResultIconWorkerLoop()",
        "kIconReadyMessage",
        "showResultIcons",
        "MergeLauncherResultsRanked(",
    ):
        if token not in launcher and token not in launcher_h:
            fail(f"v0.8 alpha.4.2 Modern/performance contract regressed: {token}")

    for token in ("NM_CLICK", "NM_DBLCLK", "ToggleResultRowSelection("):
        if token not in path_cpp:
            fail(f"v0.8 alpha.4.2 Path Conversion rapid-click regression: {token}")

    for token in ("BN_CLICKED", "BN_DOUBLECLICKED", "toggleActivated", "ToggleAdvanced();"):
        if token not in editor_cpp:
            fail(f"v0.8 alpha.4.2 Shortcut Editor rapid-click regression: {token}")

    for token in (
        'L"ALTRunNext.UpdateDispatch"',
        "UpdateDispatchWindowProc(",
        "CreateUpdateDispatchWindow()",
        "PostUpdateStatusNotification(",
        "HWND_MESSAGE",
        "kUpdateReconcileTimerId",
    ):
        if token not in app_cpp and token not in app_h:
            fail(f"v0.8 alpha.4.2 updater architecture regressed: {token}")

    version_script = read("scripts/verify_version.py")
    package_script = read("scripts/verify_package.ps1")
    for token in (
        "revision = 70 + (channel_number - 4) * 100 + channel_patch",
        "revision = 10000 + channel_number",
        "revision = 20000 + channel_number",
        "revision = 30000",
    ):
        if token not in version_script:
            fail(f"v0.8 alpha.4.2 Python fixed-version mapping missing: {token}")
    for token in (
        "(($channelNumber - 4) * 100)",
        "$revision = 10000 + $channelNumber",
        "$revision = 20000 + $channelNumber",
        "$revision = 30000",
    ):
        if token not in package_script:
            fail(f"v0.8 alpha.4.2 package fixed-version mapping missing: {token}")

    update_tests = read("tests/UpdatePolicyTests.cpp")
    for token in ('"0.8.0-alpha.4.1"', '"0.8.0-alpha.4.2"', "UpdateChannel::Stable"):
        if token not in update_tests:
            fail(f"v0.8 alpha.4.2 update ordering/default coverage missing: {token}")

    readme = read("README.md")
    changelog = read("CHANGELOG.md")
    roadmap = read("ROADMAP.md")
    for token in (
        "Classic Native Typography Matching",
        "0.8.0.72",
        "12 pt primary font",
        "10 pt auxiliary font",
        "DEFAULT_QUALITY",
        "22 logical pixel",
    ):
        if token not in readme:
            fail(f"v0.8 alpha.4.2 README contract missing: {token}")
    for token in (
        "## 0.8.0-alpha.4.2",
        "0.8.0.72",
        "12 pt",
        "10 pt",
        "DEFAULT_QUALITY",
        "22 logical px",
    ):
        if token not in changelog:
            fail(f"v0.8 alpha.4.2 changelog contract missing: {token}")
    for token in (
        "v0.8.0-alpha.4.2",
        "12 pt primary",
        "10 pt auxiliary",
        "Modern Compact refinement follows",
    ):
        if token not in roadmap:
            fail(f"v0.8 alpha.4.2 roadmap missing: {token}")

    print(
        "v0.8.0-alpha.4.2 Classic native typography verified:",
        "| Classic primary 12pt + auxiliary 10pt",
        "| Classic DEFAULT_QUALITY",
        "| full-height 22px native Edit",
        "| Modern/search/icons/updater frozen",
    )
    raise SystemExit(0)


if version == "0.8.0-alpha.4.1":
    expected_schemas = {
        "kSettingsSchemaVersion": 9,
        "kCommandsSchemaVersion": 2,
        "kUsageSchemaVersion": 1,
    }
    for name, expected in expected_schemas.items():
        actual = cpp_int("src/core/ConfigIO.hpp", name)
        if actual != expected:
            fail(f"v0.8 alpha.4.1 {name}={actual}, expected {expected}")

    if cpp_int("src/core/ProviderCache.cpp", "kProviderCacheSchemaVersion") != 2:
        fail("v0.8 alpha.4.1 must keep provider-cache schemaVersion 2")

    launcher = read("src/ui/LauncherWindow.cpp")
    launcher_h = read("src/ui/LauncherWindow.hpp")
    metrics = read("src/ui/UiMetrics.hpp")
    typography = read("src/ui/UiTypography.cpp")
    path_cpp = read("src/ui/ShortcutPathConverterDialog.cpp")
    editor_cpp = read("src/ui/ShortcutEditorDialog.cpp")
    app_cpp = read("src/app/App.cpp")
    app_h = read("src/app/App.hpp")

    # Classic typography is the only font change in alpha.4.1.
    for token in (
        "kClassicLauncherBodyPointSize = 10",
        "kModernLauncherBodyPointSize = 10",
        "kLauncherTitlePointSize = 10",
        'L"SimSun"',
        'L"Tahoma"',
        "CLEARTYPE_QUALITY",
    ):
        if token not in typography:
            fail(f"v0.8 alpha.4.1 Classic typography contract missing: {token}")

    # Classic geometry/density remains fixed while the native Edit is centered
    # inside the existing 22-logical-pixel green strip.
    for token in (
        "kClassicLauncherMetrics{\n        420,\n        16,\n        10,",
        "kModernCompactLauncherMetrics{\n        620,\n        32,\n        9,",
    ):
        if token not in metrics:
            fail(f"v0.8 alpha.4.1 launcher metrics regressed: {token}")

    for token in (
        "constexpr int inputHeightLogical = 22;",
        "constexpr int inputEditHeightLogical = 18;",
        "const int inputEditOffset =",
        "(inputHeight -\n         inputEditHeight) /\n        2;",
        "titleHeight +\n            inputEditOffset",
        "inputEditHeight,",
        "RECT classicInputStrip",
        "DpiScale(30)",
        "DpiScale(22)",
        "&classicInputStrip",
        "accentBrush_",
    ):
        if token not in launcher:
            fail(f"v0.8 alpha.4.1 Classic input alignment missing: {token}")

    # Modern Compact remains untouched in this first Classic pass.
    for token in (
        "const int inputHeight = DpiScale(36);",
        "const int margin = DpiScale(12);",
        "ResultIconWorkerLoop()",
        "kIconReadyMessage",
        "showResultIcons",
        "MergeLauncherResultsRanked(",
    ):
        if token not in launcher and token not in launcher_h:
            fail(f"v0.8 alpha.4.1 Modern/performance contract regressed: {token}")

    # Preserve alpha.3.40 rapid-click closeout.
    for token in (
        "NM_CLICK",
        "NM_DBLCLK",
        "ToggleResultRowSelection(",
    ):
        if token not in path_cpp:
            fail(f"v0.8 alpha.4.1 Path Conversion rapid-click regression: {token}")

    for token in (
        "BN_CLICKED",
        "BN_DOUBLECLICKED",
        "toggleActivated",
        "ToggleAdvanced();",
    ):
        if token not in editor_cpp:
            fail(f"v0.8 alpha.4.1 Shortcut Editor rapid-click regression: {token}")

    # Preserve the shared updater dispatcher architecture.
    for token in (
        'L"ALTRunNext.UpdateDispatch"',
        "UpdateDispatchWindowProc(",
        "CreateUpdateDispatchWindow()",
        "PostUpdateStatusNotification(",
        "HWND_MESSAGE",
        "kUpdateReconcileTimerId",
    ):
        if token not in app_cpp and token not in app_h:
            fail(f"v0.8 alpha.4.1 updater architecture regressed: {token}")

    version_script = read("scripts/verify_version.py")
    package_script = read("scripts/verify_package.ps1")

    for token in (
        "revision = 70 + (channel_number - 4) * 100 + channel_patch",
        "revision = 10000 + channel_number",
        "revision = 20000 + channel_number",
        "revision = 30000",
        "alpha phase 4+ hotfix component must stay below 100",
    ):
        if token not in version_script:
            fail(f"v0.8 alpha.4.1 Python fixed-version mapping missing: {token}")

    for token in (
        "70 +",
        "(($channelNumber - 4) * 100)",
        "$revision = 10000 + $channelNumber",
        "$revision = 20000 + $channelNumber",
        "$revision = 30000",
        "Alpha phase 4+ hotfix component must stay below 100",
    ):
        if token not in package_script:
            fail(f"v0.8 alpha.4.1 package fixed-version mapping missing: {token}")

    update_tests = read("tests/UpdatePolicyTests.cpp")
    for token in (
        '"0.8.0-alpha.3.40"',
        '"0.8.0-alpha.4.1"',
        "UpdateChannel::Stable",
    ):
        if token not in update_tests:
            fail(f"v0.8 alpha.4.1 update ordering/default coverage missing: {token}")

    readme = read("README.md")
    changelog = read("CHANGELOG.md")
    roadmap = read("ROADMAP.md")

    for token in (
        "## v0.8.0-alpha.4.1 — Classic Typography & Input Alignment",
        "0.8.0.71",
        "9 pt to 10 pt",
        "18 logical pixels",
        "22 logical pixels",
        "Modern Compact remains 10 pt",
    ):
        if token not in readme:
            fail(f"v0.8 alpha.4.1 README contract missing: {token}")

    for token in (
        "## 0.8.0-alpha.4.1",
        "0.8.0.71",
        "Classic launcher body typography",
        "18 logical px",
        "Modern Compact",
    ):
        if token not in changelog:
            fail(f"v0.8 alpha.4.1 changelog contract missing: {token}")

    for token in (
        "v0.8.0-alpha.4.1",
        "Classic ALTRun is polished to maturity first",
        "Modern Compact refinement follows",
    ):
        if token not in roadmap:
            fail(f"v0.8 alpha.4.1 roadmap missing: {token}")

    print(
        "v0.8.0-alpha.4.1 Classic typography/input alignment verified:",
        "| Classic body 10pt",
        "| 22px strip + centered 18px native Edit",
        "| Classic density preserved",
        "| Modern/search/icons/updater frozen",
    )
    raise SystemExit(0)


if version == "0.8.0-alpha.3.40":
    expected_schemas = {
        "kSettingsSchemaVersion": 9,
        "kCommandsSchemaVersion": 2,
        "kUsageSchemaVersion": 1,
    }
    for name, expected in expected_schemas.items():
        actual = cpp_int("src/core/ConfigIO.hpp", name)
        if actual != expected:
            fail(f"v0.8 alpha.3.40 {name}={actual}, expected {expected}")

    if cpp_int("src/core/ProviderCache.cpp", "kProviderCacheSchemaVersion") != 2:
        fail("v0.8 alpha.3.40 must keep provider-cache schemaVersion 2")

    path_cpp = read("src/ui/ShortcutPathConverterDialog.cpp")
    path_h = read("src/ui/ShortcutPathConverterDialog.hpp")
    editor_cpp = read("src/ui/ShortcutEditorDialog.cpp")
    settings_cpp = read("src/ui/SettingsWindow.cpp")
    app_cpp = read("src/app/App.cpp")
    app_h = read("src/app/App.hpp")
    update_cpp = read("src/platform/UpdateManager.cpp")

    # Path Conversion rapid clicks: both single and double-click notifications
    # must enter the exact same owned-checkbox toggle path.
    for token in (
        "NM_CLICK",
        "NM_DBLCLK",
        "GetResultFieldInteractionRect(",
        "PtInRect(",
        "ToggleResultRowSelection(",
    ):
        if token not in path_cpp:
            fail(f"v0.8 alpha.3.40 Path Conversion rapid-click normalization missing: {token}")

    path_click_start = path_cpp.index(
        "if (header->code ==\n                NM_CLICK ||"
    )
    path_click_end = path_cpp.index(
        "if (header->code ==\n            LVN_ITEMCHANGING)",
        path_click_start,
    )
    path_click_region = path_cpp[path_click_start:path_click_end]
    for token in (
        "NM_CLICK",
        "NM_DBLCLK",
        "GetResultFieldInteractionRect(",
        "ToggleResultRowSelection(",
    ):
        if token not in path_click_region:
            fail(f"v0.8 alpha.3.40 click branch missing {token}")

    # Advanced is toggle semantics, so every physical rapid click must flip it.
    advanced_start = editor_cpp.index("case kIdAdvancedToggle: {")
    advanced_end = editor_cpp.index("case kIdBrowseWorkdir:", advanced_start)
    advanced_region = editor_cpp[advanced_start:advanced_end]
    for token in (
        "BN_CLICKED",
        "BN_DOUBLECLICKED",
        "toggleActivated",
        "ToggleAdvanced();",
    ):
        if token not in advanced_region:
            fail(f"v0.8 alpha.3.40 Advanced rapid-click normalization missing: {token}")

    # One-shot action buttons must NOT inherit toggle double-click semantics.
    for start_marker, end_marker in (
        ("case kIdBrowseIcon:", "case kIdAdvancedToggle:"),
        ("case kIdBrowseWorkdir:", "case kIdTest:"),
        ("case kIdTest:", "case kIdSave:"),
        ("case kIdSave:", "case kIdCancel:"),
        ("case kIdCancel:", "default:"),
    ):
        start = editor_cpp.index(start_marker)
        end = editor_cpp.index(end_marker, start)
        if "BN_DOUBLECLICKED" in editor_cpp[start:end]:
            fail(f"v0.8 alpha.3.40 one-shot action gained double-click execution: {start_marker}")

    for start_marker, end_marker in (
        ("case kIdRescan:", "case kIdApply:"),
        ("case kIdApply:", "default:"),
    ):
        start = path_cpp.index(start_marker)
        end = path_cpp.index(end_marker, start)
        if "BN_DOUBLECLICKED" in path_cpp[start:end]:
            fail(f"v0.8 alpha.3.40 Path Conversion one-shot action gained double-click execution: {start_marker}")

    # Settings already established the shared owner-draw toggle policy.
    for token in (
        "notify == BN_CLICKED ||",
        "notify == BN_DOUBLECLICKED",
        "const bool toggleActivated",
    ):
        if token not in settings_cpp:
            fail(f"v0.8 alpha.3.40 Settings rapid-toggle policy regressed: {token}")

    # Preserve alpha.3.39 hit-area and focus fixes.
    for token in (
        "GetResultFieldInteractionRect(",
        "Header_GetItemRect(",
        "&interactionRect",
        "case WM_PARENTNOTIFY:",
        "ChildWindowFromPointEx(",
        "passiveSurface",
        "if (passiveSurface)",
        'L"Static"',
    ):
        if token not in path_cpp and token not in editor_cpp:
            fail(f"v0.8 alpha.3.40 alpha.3.39 interaction contract regressed: {token}")

    # Preserve the fully-owned checkbox visual/state model.
    for forbidden in (
        "LVS_EX_CHECKBOXES",
        "LVSIL_STATE",
        "checkboxStateImageList_",
        "CreateTransparentCheckboxStateImageList",
        "ListView_GetCheckState",
        "ListView_SetCheckState",
        "LVIS_STATEIMAGEMASK",
        "rowHeightImageList_",
        "kResultRowHeightLogical",
    ):
        if forbidden in path_cpp or forbidden in path_h:
            fail(f"v0.8 alpha.3.40 native/hybrid checkbox ownership returned: {forbidden}")

    for token in (
        "bool selected{false}",
        "row.selected = row.exists",
        "DrawResultFieldCell(",
        "CheckboxTemplateForDpi(",
        "kCheckboxSamplesPerAxis = 4",
        "VK_SPACE",
        "LVNI_FOCUSED",
    ):
        if token not in path_cpp and token not in path_h:
            fail(f"v0.8 alpha.3.40 owned checkbox regression: {token}")

    # Freeze selector / geometry / editor layout.
    for token in (
        "struct SelectorRasterTemplate",
        "SelectorTemplateForDpi(",
        "kSamplesPerAxis = 4",
        "return {19, 1.60, 7}",
        "kDefaultWidthLogical = 960",
        "kDefaultHeightLogical = 560",
        "kMinimumWidthLogical = 820",
        "kMinimumHeightLogical = 480",
        "kFieldColumnPercent = 13",
        "kCurrentColumnPercent = 36",
        "kConvertedColumnPercent = 39",
        "ApplyUserCommandPathUpdates(",
        "ResolveOwnedPopupGeometry(",
        "RevealFullyPainted(",
    ):
        if token not in path_cpp:
            fail(f"v0.8 alpha.3.40 frozen Path Conversion contract missing: {token}")

    for token in (
        "kEditorWidthLogical = 590",
        "kControlRowHeightLogical = 28",
        "DrawAdvancedHeader(",
        "UpdateAdvancedVisibility()",
        "RefreshDynamicLayout()",
    ):
        if token not in editor_cpp:
            fail(f"v0.8 alpha.3.40 Shortcut Editor layout contract regressed: {token}")

    # Preserve alpha.3.36 updater architecture.
    for token in (
        'L"ALTRunNext.UpdateDispatch"',
        "UpdateDispatchWindowProc(",
        "CreateUpdateDispatchWindow()",
        "DestroyUpdateDispatchWindow()",
        "PostUpdateStatusNotification(",
        "HWND_MESSAGE",
        "kUpdateReconcileTimerId",
    ):
        if token not in app_cpp and token not in app_h:
            fail(f"v0.8 alpha.3.40 updater dispatcher regressed: {token}")

    for token in (
        "WinHttpSetTimeouts(",
        "std::stop_callback",
        "handles.request.Close();",
    ):
        if token not in update_cpp:
            fail(f"v0.8 alpha.3.40 WinHTTP hardening regressed: {token}")

    update_tests = read("tests/UpdatePolicyTests.cpp")
    for token in (
        '"0.8.0-alpha.3.39"',
        '"0.8.0-alpha.3.40"',
        "UpdateChannel::Stable",
    ):
        if token not in update_tests:
            fail(f"v0.8 alpha.3.40 update ordering/default coverage missing: {token}")

    readme = read("README.md")
    changelog = read("CHANGELOG.md")
    roadmap = read("ROADMAP.md")

    for token in (
        "## v0.8.0-alpha.3.40 — Rapid Click Normalization",
        "0.8.0.70",
        "NM_CLICK",
        "NM_DBLCLK",
        "BN_CLICKED",
        "BN_DOUBLECLICKED",
        "one-shot action buttons",
    ):
        if token not in readme:
            fail(f"v0.8 alpha.3.40 README contract missing: {token}")

    for token in (
        "## 0.8.0-alpha.3.40",
        "0.8.0.70",
        "NM_DBLCLK",
        "BN_DOUBLECLICKED",
        "single-shot",
        "alpha.3.36 updater hardening",
    ):
        if token not in changelog:
            fail(f"v0.8 alpha.3.40 changelog contract missing: {token}")

    for token in (
        "v0.8.0-alpha.3.40",
        "v0.8.0-alpha.4",
    ):
        if token not in roadmap:
            fail(f"v0.8 alpha.3.40 roadmap missing: {token}")

    print(
        "v0.8.0-alpha.3.40 rapid-click normalization verified:",
        "| Path Conversion NM_CLICK + NM_DBLCLK toggle",
        "| Advanced BN_CLICKED + BN_DOUBLECLICKED toggle",
        "| one-shot actions remain single-fire",
        "| alpha.3.39 interaction + updater/visual contracts preserved",
    )
    raise SystemExit(0)


if version == "0.8.0-alpha.3.39":
    expected_schemas = {
        "kSettingsSchemaVersion": 9,
        "kCommandsSchemaVersion": 2,
        "kUsageSchemaVersion": 1,
    }
    for name, expected in expected_schemas.items():
        actual = cpp_int("src/core/ConfigIO.hpp", name)
        if actual != expected:
            fail(f"v0.8 alpha.3.39 {name}={actual}, expected {expected}")

    if cpp_int("src/core/ProviderCache.cpp", "kProviderCacheSchemaVersion") != 2:
        fail("v0.8 alpha.3.39 must keep provider-cache schemaVersion 2")

    path_cpp = read("src/ui/ShortcutPathConverterDialog.cpp")
    path_h = read("src/ui/ShortcutPathConverterDialog.hpp")
    editor_cpp = read("src/ui/ShortcutEditorDialog.cpp")
    app_cpp = read("src/app/App.cpp")
    app_h = read("src/app/App.hpp")
    settings_cpp = read("src/ui/SettingsWindow.cpp")
    settings_h = read("src/ui/SettingsWindow.hpp")
    update_cpp = read("src/platform/UpdateManager.cpp")

    # Preserve the alpha.3.38 owned checkbox model and visual.
    for forbidden in (
        "LVS_EX_CHECKBOXES",
        "LVSIL_STATE",
        "checkboxStateImageList_",
        "CreateTransparentCheckboxStateImageList",
        "ListView_GetCheckState",
        "ListView_SetCheckState",
        "LVIS_STATEIMAGEMASK",
        "BP_CHECKBOX",
        "CBS_CHECKEDNORMAL",
        "CBS_UNCHECKEDNORMAL",
        "rowHeightImageList_",
        "kResultRowHeightLogical",
    ):
        if forbidden in path_cpp or forbidden in path_h:
            fail(f"v0.8 alpha.3.39 native/hybrid checkbox ownership returned: {forbidden}")

    for token in (
        "bool selected{false}",
        "row.selected = row.exists",
        "return row.selected",
        "for (const Row& row :",
        "FieldLabel(",
        "GetResultFieldLayout(",
        "DrawResultFieldCell(",
        "ToggleResultRowSelection(",
        "VK_SPACE",
        "LVNI_FOCUSED",
        "CDDS_ITEMPOSTPAINT",
        "CheckboxTemplateForDpi(",
        "DrawCheckboxRaster(",
        "kCheckboxSamplesPerAxis = 4",
    ):
        if token not in path_cpp and token not in path_h:
            fail(f"v0.8 alpha.3.39 owned checkbox regression: {token}")

    # Alpha.3.39 interaction reliability: first-column cell, not only the
    # painted square, owns pointer toggling.
    for token in (
        "GetResultFieldInteractionRect(",
        "Header_GetItemRect(",
        "interactionRect.top =",
        "interactionRect.bottom =",
        "NM_CLICK",
        "PtInRect(",
        "&interactionRect",
    ):
        if token not in path_cpp and token not in path_h:
            fail(f"v0.8 alpha.3.39 field-cell hit target missing: {token}")

    click_region = path_cpp[
        path_cpp.index("if (header->code ==\n            NM_CLICK)"):
        path_cpp.index("if (header->code ==\n            LVN_ITEMCHANGING)")
    ]
    if "GetResultFieldInteractionRect(" not in click_region:
        fail("v0.8 alpha.3.39 NM_CLICK must use the first-column interaction rect")
    if "&checkboxRect" in click_region:
        fail("v0.8 alpha.3.39 must not regress to exact visual-checkbox hit testing")

    # Shortcut Editor: passive surface clicks may dismiss a lingering ComboBox
    # focus, but interactive child controls must complete their mouse sequence.
    for token in (
        "case WM_PARENTNOTIFY:",
        "ChildWindowFromPointEx(",
        "CWP_SKIPINVISIBLE",
        "CWP_SKIPDISABLED",
        "GetClassNameW(",
        'L"Static"',
        "passiveSurface",
        "if (passiveSurface)",
        "dismissComboFocus();",
        "kIdAdvancedToggle",
        "BN_CLICKED",
        "ToggleAdvanced();",
        "BS_OWNERDRAW",
    ):
        if token not in editor_cpp:
            fail(f"v0.8 alpha.3.39 Shortcut Editor interaction hardening missing: {token}")

    parent_notify_region = editor_cpp[
        editor_cpp.index("case WM_PARENTNOTIFY:"):
        editor_cpp.index("case WM_NCLBUTTONDOWN:")
    ]
    if parent_notify_region.count("dismissComboFocus();") != 1:
        fail("v0.8 alpha.3.39 WM_PARENTNOTIFY must have one guarded ComboBox dismissal")
    if "if (passiveSurface)" not in parent_notify_region:
        fail("v0.8 alpha.3.39 child-control focus dismissal must be passive-surface-only")
    if "SetFocus(hwnd_);" in parent_notify_region:
        fail("v0.8 alpha.3.39 WM_PARENTNOTIFY must not directly steal focus from interactive controls")

    # Parent background and non-client clicks still dismiss ComboBox focus.
    parent_surface_region = editor_cpp[
        editor_cpp.index("case WM_LBUTTONDOWN:"):
        editor_cpp.index("case WM_PARENTNOTIFY:")
    ]
    if "dismissComboFocus();" not in parent_surface_region:
        fail("v0.8 alpha.3.39 parent-surface ComboBox click-away behavior regressed")

    # Freeze selector / Path Conversion geometry and behavior.
    for token in (
        "struct SelectorRasterTemplate",
        "SelectorTemplateForDpi(",
        "CircleCoverage(",
        "DrawSelectorRaster(",
        "kSamplesPerAxis = 4",
        "return {15, 1.35, 5}",
        "return {17, 1.45, 6}",
        "return {19, 1.60, 7}",
        "return {21, 1.75, 8}",
        "return {23, 1.90, 9}",
        "kDefaultWidthLogical = 960",
        "kDefaultHeightLogical = 560",
        "kMinimumWidthLogical = 820",
        "kMinimumHeightLogical = 480",
        "kFieldColumnPercent = 13",
        "kCurrentColumnPercent = 36",
        "kConvertedColumnPercent = 39",
        "SelectedFieldCount() const",
        "UpdateSelectionState(",
        "UpdateColumnWidths(",
        "HandleHeaderNotification(",
        "HandleHeaderCustomDraw(",
        'T(L"当前没有可转换的路径"',
        "ApplyUserCommandPathUpdates(",
        "win::MakePortablePath(",
        "win::ExpandPortablePath(",
        "ResolveOwnedPopupGeometry(",
        "RevealFullyPainted(",
    ):
        if token not in path_cpp:
            fail(f"v0.8 alpha.3.39 frozen Path Conversion contract missing: {token}")

    # Preserve Shortcut Editor layout / dynamic section architecture.
    for token in (
        "kEditorWidthLogical = 590",
        "kControlRowHeightLogical = 28",
        "DrawAdvancedHeader(",
        "UpdateAdvancedVisibility()",
        "RefreshDynamicLayout()",
        'T(L"▾ 高级选项"',
        'T(L"▸ 高级选项"',
    ):
        if token not in editor_cpp:
            fail(f"v0.8 alpha.3.39 Shortcut Editor layout contract regressed: {token}")

    # Preserve alpha.3.36 updater hardening.
    for token in (
        'L"ALTRunNext.UpdateDispatch"',
        "UpdateDispatchWindowProc(",
        "CreateUpdateDispatchWindow()",
        "DestroyUpdateDispatchWindow()",
        "PostUpdateStatusNotification(",
        "HWND_MESSAGE",
        "PostMessageW(",
        "kUpdateReconcileTimerId",
        "HandleUpdateStatusMessage(",
    ):
        if token not in app_cpp and token not in app_h:
            fail(f"v0.8 alpha.3.39 updater dispatcher regressed: {token}")

    for token in (
        "kUpdateStatusTimerId = 0x51692",
        "SyncUpdateStatusTimer()",
        "RefreshUpdateStatus();\n                SyncUpdateStatusTimer();",
        "RDW_UPDATENOW",
    ):
        if token not in settings_cpp and token not in settings_h:
            fail(f"v0.8 alpha.3.39 Settings updater reconciliation regressed: {token}")

    for token in (
        "WinHttpSetTimeouts(",
        "std::stop_callback",
        "handles.request.Close();",
    ):
        if token not in update_cpp:
            fail(f"v0.8 alpha.3.39 WinHTTP hardening regressed: {token}")

    update_tests = read("tests/UpdatePolicyTests.cpp")
    for token in (
        '"0.8.0-alpha.3.38"',
        '"0.8.0-alpha.3.39"',
        "UpdateChannel::Stable",
    ):
        if token not in update_tests:
            fail(f"v0.8 alpha.3.39 update ordering/default coverage missing: {token}")

    readme = read("README.md")
    changelog = read("CHANGELOG.md")
    roadmap = read("ROADMAP.md")

    for token in (
        "## v0.8.0-alpha.3.39 — Interaction Reliability Hardening",
        "0.8.0.69",
        "entire first-column field cell",
        "WM_PARENTNOTIFY",
        "passive surface clicks",
        "interactive Buttons/Edits/ComboBoxes",
    ):
        if token not in readme:
            fail(f"v0.8 alpha.3.39 README contract missing: {token}")

    for token in (
        "## 0.8.0-alpha.3.39",
        "0.8.0.69",
        "GetResultFieldInteractionRect()",
        "WM_PARENTNOTIFY",
        "passive STATIC",
        "alpha.3.36 updater hardening",
    ):
        if token not in changelog:
            fail(f"v0.8 alpha.3.39 changelog contract missing: {token}")

    for token in (
        "v0.8.0-alpha.3.39",
        "v0.8.0-alpha.4",
    ):
        if token not in roadmap:
            fail(f"v0.8 alpha.3.39 roadmap missing: {token}")

    print(
        "v0.8.0-alpha.3.39 interaction reliability verified:",
        "| Path Conversion whole-field-cell checkbox hit target",
        "| Shortcut Editor interactive child clicks no longer lose focus mid-sequence",
        "| passive click-away ComboBox behavior preserved",
        "| selector/columns/updater frozen",
    )
    raise SystemExit(0)


if version == "0.8.0-alpha.3.38":
    expected_schemas = {
        "kSettingsSchemaVersion": 9,
        "kCommandsSchemaVersion": 2,
        "kUsageSchemaVersion": 1,
    }
    for name, expected in expected_schemas.items():
        actual = cpp_int("src/core/ConfigIO.hpp", name)
        if actual != expected:
            fail(f"v0.8 alpha.3.38 {name}={actual}, expected {expected}")

    if cpp_int("src/core/ProviderCache.cpp", "kProviderCacheSchemaVersion") != 2:
        fail("v0.8 alpha.3.38 must keep provider-cache schemaVersion 2")

    path_cpp = read("src/ui/ShortcutPathConverterDialog.cpp")
    path_h = read("src/ui/ShortcutPathConverterDialog.hpp")
    app_cpp = read("src/app/App.cpp")
    app_h = read("src/app/App.hpp")
    settings_cpp = read("src/ui/SettingsWindow.cpp")
    settings_h = read("src/ui/SettingsWindow.hpp")
    update_cpp = read("src/platform/UpdateManager.cpp")

    # The alpha.3.34 selector remains frozen.
    for token in (
        "struct SelectorRasterTemplate",
        "SelectorTemplateForDpi(",
        "CircleCoverage(",
        "DrawSelectorRaster(",
        "kSamplesPerAxis = 4",
        "return {15, 1.35, 5}",
        "return {17, 1.45, 6}",
        "return {19, 1.60, 7}",
        "return {21, 1.75, 8}",
        "return {23, 1.90, 9}",
        "SetPixelV(",
    ):
        if token not in path_cpp:
            fail(f"v0.8 alpha.3.38 validated selector regressed: {token}")

    # Checkbox ownership is entirely ours. No ListView state-image API may
    # participate in rendering, storage or selection anymore.
    for forbidden in (
        "LVS_EX_CHECKBOXES",
        "LVSIL_STATE",
        "checkboxStateImageList_",
        "CreateTransparentCheckboxStateImageList",
        "ListView_GetCheckState",
        "ListView_SetCheckState",
        "LVIS_STATEIMAGEMASK",
        "BP_CHECKBOX",
        "CBS_CHECKEDNORMAL",
        "CBS_UNCHECKEDNORMAL",
        "rowHeightImageList_",
        "kResultRowHeightLogical",
    ):
        if forbidden in path_cpp or forbidden in path_h:
            fail(f"v0.8 alpha.3.38 native/hybrid checkbox ownership returned: {forbidden}")

    for token in (
        "bool selected{false}",
        "row.selected = row.exists",
        "return row.selected",
        "for (const Row& row :",
        "FieldLabel(",
        "GetResultFieldLayout(",
        "Header_GetItemRect(",
        "DrawResultFieldCell(",
        "ToggleResultRowSelection(",
        "NM_CLICK",
        "PtInRect(",
        "VK_SPACE",
        "LVNI_FOCUSED",
        "CDDS_ITEMPOSTPAINT",
        "ListView_GetItemState(",
        "COLOR_HIGHLIGHTTEXT",
    ):
        if token not in path_cpp and token not in path_h:
            fail(f"v0.8 alpha.3.38 owned checkbox state/interaction missing: {token}")

    for token in (
        "struct CheckboxRasterTemplate",
        "CheckboxTemplateForDpi(",
        "BlendCheckboxPixel(",
        "PointInsideRoundedRect(",
        "RoundedRectCoverage(",
        "DistanceSquaredToSegment(",
        "CheckmarkCoverage(",
        "DrawCheckboxRaster(",
        "kCheckboxSamplesPerAxis = 4",
        "return {15, 3.0, 1.35, 1.8, 4}",
        "return {17, 3.5, 1.45, 2.0, 4}",
        "return {19, 4.0, 1.60, 2.2, 5}",
        "return {21, 4.5, 1.75, 2.4, 5}",
        "return {23, 5.0, 1.90, 2.6, 6}",
        "palette.accent",
    ):
        if token not in path_cpp:
            fail(f"v0.8 alpha.3.38 modern checkbox raster missing: {token}")

    # First-column native text stays empty; field labels are ours too.
    for token in (
        'wchar_t emptyText[] = L""',
        'T(L"目标", L"Target")',
        'L"工作目录"',
        'L"Working directory"',
        'L"自定义图标"',
        'L"Custom icon"',
        "DT_VCENTER",
        "DT_SINGLELINE",
        "DT_END_ELLIPSIS",
    ):
        if token not in path_cpp:
            fail(f"v0.8 alpha.3.38 owned field-cell drawing missing: {token}")

    for forbidden in (
        'T(L"    目标"',
        'L"    Target"',
        'T(L"    工作目录"',
        'L"    Working directory"',
        'T(L"    自定义图标"',
        'L"    Custom icon"',
    ):
        if forbidden in path_cpp:
            fail(f"v0.8 alpha.3.38 artificial field indentation returned: {forbidden}")

    # Freeze remaining Path Conversion geometry and behavior.
    for token in (
        "kDefaultWidthLogical = 960",
        "kDefaultHeightLogical = 560",
        "kMinimumWidthLogical = 820",
        "kMinimumHeightLogical = 480",
        "kFieldColumnPercent = 13",
        "kCurrentColumnPercent = 36",
        "kConvertedColumnPercent = 39",
        "kFieldColumnMinimumLogical = 100",
        "kCurrentColumnMinimumLogical = 180",
        "kConvertedColumnMinimumLogical = 180",
        "kStatusColumnMinimumLogical = 100",
        "SelectedFieldCount() const",
        "UpdateSelectionState(",
        "UpdateColumnWidths(",
        "HandleHeaderNotification(",
        "HandleHeaderCustomDraw(",
        'T(L"当前没有可转换的路径"',
        'T(L"已应用 "',
        "listHeight * 44 / 100",
        "VK_ESCAPE",
        "VK_LEFT",
        "VK_RIGHT",
        "WM_GETMINMAXINFO",
        "ApplyUserCommandPathUpdates(",
        "win::MakePortablePath(",
        "win::ExpandPortablePath(",
        "ResolveOwnedPopupGeometry(",
        "RevealFullyPainted(",
    ):
        if token not in path_cpp:
            fail(f"v0.8 alpha.3.38 frozen Path Conversion behavior missing: {token}")

    # Preserve alpha.3.36 updater hardening unchanged.
    for token in (
        'L"ALTRunNext.UpdateDispatch"',
        "UpdateDispatchWindowProc(",
        "CreateUpdateDispatchWindow()",
        "DestroyUpdateDispatchWindow()",
        "PostUpdateStatusNotification(",
        "HWND_MESSAGE",
        "PostMessageW(",
        "kUpdateReconcileTimerId",
        "HandleUpdateStatusMessage(",
    ):
        if token not in app_cpp and token not in app_h:
            fail(f"v0.8 alpha.3.38 updater dispatcher regressed: {token}")

    for token in (
        "kUpdateStatusTimerId = 0x51692",
        "SyncUpdateStatusTimer()",
        "RefreshUpdateStatus();\n                SyncUpdateStatusTimer();",
        "RDW_UPDATENOW",
    ):
        if token not in settings_cpp and token not in settings_h:
            fail(f"v0.8 alpha.3.38 Settings updater reconciliation regressed: {token}")

    for token in (
        "kUpdateCheckWatchdogMs =\n        60ULL * 1000ULL",
        "CheckTimedOut",
        "updateWorkerStartedTick_",
        "request_stop()",
    ):
        if token not in app_cpp:
            fail(f"v0.8 alpha.3.38 updater watchdog/cancellation regressed: {token}")

    for token in (
        "WinHttpSetTimeouts(",
        "5000,",
        "15000,",
        "std::stop_callback",
        "handles.request.Close();",
        "WinHttpReadData(",
    ):
        if token not in update_cpp:
            fail(f"v0.8 alpha.3.38 WinHTTP hardening regressed: {token}")

    runtime_smoke = read("scripts/verify_runtime_smoke.ps1")
    for token in (
        "$migratedSettings.schemaVersion -ne 9",
        "shortcutManagerMode",
        "shortcutManagerLastValid",
    ):
        if token not in runtime_smoke:
            fail(f"v0.8 alpha.3.38 must preserve schema-9 runtime smoke coverage: {token}")

    update_tests = read("tests/UpdatePolicyTests.cpp")
    for token in (
        '"0.8.0-alpha.3.37"',
        '"0.8.0-alpha.3.38"',
        "UpdateChannel::Stable",
    ):
        if token not in update_tests:
            fail(f"v0.8 alpha.3.38 update ordering/default coverage missing: {token}")

    readme = read("README.md")
    changelog = read("CHANGELOG.md")
    roadmap = read("ROADMAP.md")

    for token in (
        "## v0.8.0-alpha.3.38 — Path Conversion Checkbox Ownership",
        "0.8.0.68",
        "Row",
        "GetResultFieldLayout()",
        "Mouse clicks",
        "Space toggles",
        "no state image",
    ):
        if token not in readme:
            fail(f"v0.8 alpha.3.38 README contract missing: {token}")

    for token in (
        "## 0.8.0-alpha.3.38",
        "0.8.0.68",
        "Row::selected",
        "exact custom checkbox hit-testing",
        "Space-key toggling",
        "alpha.3.36 updater dispatch hardening",
    ):
        if token not in changelog:
            fail(f"v0.8 alpha.3.38 changelog contract missing: {token}")

    for token in (
        "v0.8.0-alpha.3.38",
        "v0.8.0-alpha.4",
    ):
        if token not in roadmap:
            fail(f"v0.8 alpha.3.38 roadmap missing: {token}")

    print(
        "v0.8.0-alpha.3.38 Path Conversion checkbox ownership verified:",
        "| no native/state-image checkbox ownership",
        "| Row::selected drives count/apply",
        "| first-column checkbox + field label share real row center",
        "| exact mouse hit-test + Space toggle",
        "| selector/columns/updater frozen",
    )
    raise SystemExit(0)


if version == "0.8.0-alpha.3.37":
    expected_schemas = {
        "kSettingsSchemaVersion": 9,
        "kCommandsSchemaVersion": 2,
        "kUsageSchemaVersion": 1,
    }
    for name, expected in expected_schemas.items():
        actual = cpp_int("src/core/ConfigIO.hpp", name)
        if actual != expected:
            fail(f"v0.8 alpha.3.37 {name}={actual}, expected {expected}")

    if cpp_int("src/core/ProviderCache.cpp", "kProviderCacheSchemaVersion") != 2:
        fail("v0.8 alpha.3.37 must keep provider-cache schemaVersion 2")

    path_cpp = read("src/ui/ShortcutPathConverterDialog.cpp")
    path_h = read("src/ui/ShortcutPathConverterDialog.hpp")
    app_cpp = read("src/app/App.cpp")
    app_h = read("src/app/App.hpp")
    settings_cpp = read("src/ui/SettingsWindow.cpp")
    settings_h = read("src/ui/SettingsWindow.hpp")
    update_cpp = read("src/platform/UpdateManager.cpp")

    # The validated mode selector remains frozen.
    for token in (
        "struct SelectorRasterTemplate",
        "SelectorTemplateForDpi(",
        "CircleCoverage(",
        "DrawSelectorRaster(",
        "kSamplesPerAxis = 4",
        "return {15, 1.35, 5}",
        "return {17, 1.45, 6}",
        "return {19, 1.60, 7}",
        "return {21, 1.75, 8}",
        "return {23, 1.90, 9}",
        "SetPixelV(",
    ):
        if token not in path_cpp:
            fail(f"v0.8 alpha.3.37 validated selector regressed: {token}")

    # Modern checkbox visual: native state/interaction, transparent state
    # images, and our own device-pixel raster tied to the real row center.
    for token in (
        "struct CheckboxRasterTemplate",
        "CheckboxTemplateForDpi(",
        "BlendCheckboxPixel(",
        "PointInsideRoundedRect(",
        "RoundedRectCoverage(",
        "DistanceSquaredToSegment(",
        "CheckmarkCoverage(",
        "DrawCheckboxRaster(",
        "CreateTransparentCheckboxStateImageList(",
        "kCheckboxSamplesPerAxis = 4",
        "return {15, 3.0, 1.35, 1.8, 4}",
        "return {17, 3.5, 1.45, 2.0, 4}",
        "return {19, 4.0, 1.60, 2.2, 5}",
        "return {21, 4.5, 1.75, 2.4, 5}",
        "return {23, 5.0, 1.90, 2.6, 6}",
        "checkboxStateImageList_",
        "LVS_EX_CHECKBOXES",
        "LVSIL_STATE",
        "DrawResultCheckbox(",
        "CDDS_ITEMPOSTPAINT",
        "LVIR_BOUNDS",
        "LVIR_LABEL",
        "ListView_GetCheckState(",
        "LVIS_STATEIMAGEMASK",
        "LVN_ITEMCHANGED",
        "std::lround(",
        "palette.accent",
    ):
        if token not in path_cpp and token not in path_h:
            fail(f"v0.8 alpha.3.37 modern checkbox contract missing: {token}")

    for forbidden in (
        "kResultRowHeightLogical",
        "rowHeightImageList_",
        'T(L"    目标"',
        'L"    Target"',
        'T(L"    工作目录"',
        'L"    Working directory"',
        'T(L"    自定义图标"',
        'L"    Custom icon"',
        "BP_CHECKBOX",
        "CBS_CHECKEDNORMAL",
        "CBS_UNCHECKEDNORMAL",
    ):
        if forbidden in path_cpp or forbidden in path_h:
            fail(f"v0.8 alpha.3.37 legacy checkbox/row layout returned: {forbidden}")

    # Freeze the rest of Path Conversion geometry and behavior.
    for token in (
        "kDefaultWidthLogical = 960",
        "kDefaultHeightLogical = 560",
        "kMinimumWidthLogical = 820",
        "kMinimumHeightLogical = 480",
        "kFieldColumnPercent = 13",
        "kCurrentColumnPercent = 36",
        "kConvertedColumnPercent = 39",
        "kFieldColumnMinimumLogical = 100",
        "kCurrentColumnMinimumLogical = 180",
        "kConvertedColumnMinimumLogical = 180",
        "kStatusColumnMinimumLogical = 100",
        'T(L"目标"',
        'T(L"工作目录"',
        'T(L"自定义图标"',
        "SelectedFieldCount() const",
        "UpdateSelectionState(",
        "UpdateColumnWidths(",
        "HandleHeaderNotification(",
        "HandleHeaderCustomDraw(",
        'T(L"当前没有可转换的路径"',
        'T(L"已应用 "',
        "listHeight * 44 / 100",
        "VK_ESCAPE",
        "VK_LEFT",
        "VK_RIGHT",
        "WM_GETMINMAXINFO",
        "ApplyUserCommandPathUpdates(",
        "win::MakePortablePath(",
        "win::ExpandPortablePath(",
        "ResolveOwnedPopupGeometry(",
        "RevealFullyPainted(",
    ):
        if token not in path_cpp:
            fail(f"v0.8 alpha.3.37 frozen Path Conversion behavior missing: {token}")

    # Preserve the alpha.3.36 systemic updater fix unchanged.
    for token in (
        'L"ALTRunNext.UpdateDispatch"',
        "UpdateDispatchWindowProc(",
        "CreateUpdateDispatchWindow()",
        "DestroyUpdateDispatchWindow()",
        "PostUpdateStatusNotification(",
        "HWND_MESSAGE",
        "PostMessageW(",
        "kUpdateReconcileTimerId",
        "HandleUpdateStatusMessage(",
    ):
        if token not in app_cpp and token not in app_h:
            fail(f"v0.8 alpha.3.37 updater dispatcher regressed: {token}")

    for token in (
        "kUpdateStatusTimerId = 0x51692",
        "SyncUpdateStatusTimer()",
        "RefreshUpdateStatus();\n                SyncUpdateStatusTimer();",
        "RDW_UPDATENOW",
    ):
        if token not in settings_cpp and token not in settings_h:
            fail(f"v0.8 alpha.3.37 Settings updater reconciliation regressed: {token}")

    for token in (
        "kUpdateCheckWatchdogMs =\n        60ULL * 1000ULL",
        "CheckTimedOut",
        "updateWorkerStartedTick_",
        "request_stop()",
    ):
        if token not in app_cpp:
            fail(f"v0.8 alpha.3.37 updater watchdog/cancellation regressed: {token}")

    for token in (
        "WinHttpSetTimeouts(",
        "5000,",
        "15000,",
        "std::stop_callback",
        "handles.request.Close();",
        "WinHttpReadData(",
    ):
        if token not in update_cpp:
            fail(f"v0.8 alpha.3.37 WinHTTP hardening regressed: {token}")

    runtime_smoke = read("scripts/verify_runtime_smoke.ps1")
    for token in (
        "$migratedSettings.schemaVersion -ne 9",
        "shortcutManagerMode",
        "shortcutManagerLastValid",
    ):
        if token not in runtime_smoke:
            fail(f"v0.8 alpha.3.37 must preserve schema-9 runtime smoke coverage: {token}")

    update_tests = read("tests/UpdatePolicyTests.cpp")
    for token in (
        '"0.8.0-alpha.3.36"',
        '"0.8.0-alpha.3.37"',
        "UpdateChannel::Stable",
    ):
        if token not in update_tests:
            fail(f"v0.8 alpha.3.37 update ordering/default coverage missing: {token}")

    readme = read("README.md")
    changelog = read("CHANGELOG.md")
    roadmap = read("ROADMAP.md")

    for token in (
        "## v0.8.0-alpha.3.37 — Path Conversion Checkbox Visual & Alignment Final Fix",
        "0.8.0.67",
        "transparent DPI-sized slots",
        "15 / 17 / 19 / 21 / 23 px",
        "actual visual center",
        "message-only HWND updater hardening",
    ):
        if token not in readme:
            fail(f"v0.8 alpha.3.37 README contract missing: {token}")

    for token in (
        "## 0.8.0-alpha.3.37",
        "0.8.0.67",
        "transparent `LVSIL_STATE` images",
        "4×4 supersampled",
        "actual ListView row rectangle",
        "alpha.3.36 updater dispatch hardening",
    ):
        if token not in changelog:
            fail(f"v0.8 alpha.3.37 changelog contract missing: {token}")

    for token in (
        "v0.8.0-alpha.3.37",
        "v0.8.0-alpha.4",
    ):
        if token not in roadmap:
            fail(f"v0.8 alpha.3.37 roadmap missing: {token}")

    print(
        "v0.8.0-alpha.3.37 Path Conversion checkbox verified:",
        "| native checkbox state/hit-testing preserved",
        "| transparent state images + modern 4x4 device-pixel visual",
        "| row-center/label-rect alignment",
        "| selector/columns/behavior frozen",
        "| alpha.3.36 updater hardening preserved",
    )
    raise SystemExit(0)


if version == "0.8.0-alpha.3.36":
    expected_schemas = {
        "kSettingsSchemaVersion": 9,
        "kCommandsSchemaVersion": 2,
        "kUsageSchemaVersion": 1,
    }
    for name, expected in expected_schemas.items():
        actual = cpp_int("src/core/ConfigIO.hpp", name)
        if actual != expected:
            fail(f"v0.8 alpha.3.36 {name}={actual}, expected {expected}")

    if cpp_int("src/core/ProviderCache.cpp", "kProviderCacheSchemaVersion") != 2:
        fail("v0.8 alpha.3.36 must keep provider-cache schemaVersion 2")

    app_cpp = read("src/app/App.cpp")
    app_h = read("src/app/App.hpp")
    settings_cpp = read("src/ui/SettingsWindow.cpp")
    settings_h = read("src/ui/SettingsWindow.hpp")
    update_cpp = read("src/platform/UpdateManager.cpp")
    path_cpp = read("src/ui/ShortcutPathConverterDialog.cpp")
    path_h = read("src/ui/ShortcutPathConverterDialog.hpp")

    for token in (
        'L"ALTRunNext.UpdateDispatch"',
        "UpdateDispatchWindowProc(",
        "CreateUpdateDispatchWindow()",
        "DestroyUpdateDispatchWindow()",
        "PostUpdateStatusNotification(",
        "HWND_MESSAGE",
        "PostMessageW(",
        "kUpdateReconcileTimerId",
        "SetTimer(\n                updateDispatchWindow_",
        "HandleUpdateStatusMessage(",
    ):
        if token not in app_cpp and token not in app_h:
            fail(f"v0.8 alpha.3.36 message-only update dispatcher missing: {token}")

    update_worker_region = app_cpp[
        app_cpp.index("bool App::StartUpdateCheck("):
        app_cpp.index("void App::SetGeneralSettings(")
    ]
    if "PostThreadMessageW(" in update_worker_region:
        fail("v0.8 alpha.3.36 update workers must not use PostThreadMessageW directly")
    if update_worker_region.count("PostUpdateStatusNotification(") < 4:
        fail("v0.8 alpha.3.36 update worker progress/completion notifications are incomplete")

    for token in (
        "kUpdateStatusTimerId = 0x51692",
        "SyncUpdateStatusTimer()",
        "page_ == Page::About",
        "SetTimer(\n            hwnd_,\n            kUpdateStatusTimerId,\n            250,",
        "RefreshUpdateStatus();\n                SyncUpdateStatusTimer();",
        "KillTimer(\n            hwnd_,\n            kUpdateStatusTimerId)",
        "RDW_UPDATENOW",
    ):
        if token not in settings_cpp and token not in settings_h:
            fail(f"v0.8 alpha.3.36 Settings update self-reconciliation missing: {token}")

    for token in (
        "kUpdateCheckWatchdogMs =\n        60ULL * 1000ULL",
        "CheckTimedOut",
        "updateWorkerStartedTick_",
        "request_stop()",
    ):
        if token not in app_cpp:
            fail(f"v0.8 alpha.3.36 App watchdog/cancellation regressed: {token}")

    for token in (
        "WinHttpSetTimeouts(",
        "5000,",
        "15000,",
        "std::stop_callback",
        "handles.request.Close();",
        "WinHttpReadData(",
    ):
        if token not in update_cpp:
            fail(f"v0.8 alpha.3.36 WinHTTP hardening regressed: {token}")

    for token in (
        "struct SelectorRasterTemplate",
        "kSamplesPerAxis = 4",
        "return {15, 1.35, 5}",
        "return {23, 1.90, 9}",
        "SetPixelV(",
        "LVS_EX_CHECKBOXES",
        'T(L"目标"',
        'T(L"工作目录"',
        'T(L"自定义图标"',
        "kFieldColumnPercent = 13",
        "kCurrentColumnPercent = 36",
        "kConvertedColumnPercent = 39",
    ):
        if token not in path_cpp:
            fail(f"v0.8 alpha.3.36 frozen Path Conversion contract missing: {token}")

    for forbidden in (
        "kResultRowHeightLogical",
        "rowHeightImageList_",
        'T(L"    目标"',
        'T(L"    工作目录"',
        'T(L"    自定义图标"',
    ):
        if forbidden in path_cpp or forbidden in path_h:
            fail(f"v0.8 alpha.3.36 Path Conversion row regression returned: {forbidden}")

    runtime_smoke = read("scripts/verify_runtime_smoke.ps1")
    for token in (
        "$migratedSettings.schemaVersion -ne 9",
        "shortcutManagerMode",
        "shortcutManagerLastValid",
    ):
        if token not in runtime_smoke:
            fail(f"v0.8 alpha.3.36 must preserve schema-9 runtime smoke coverage: {token}")

    update_tests = read("tests/UpdatePolicyTests.cpp")
    for token in (
        '"0.8.0-alpha.3.35"',
        '"0.8.0-alpha.3.36"',
        "UpdateChannel::Stable",
    ):
        if token not in update_tests:
            fail(f"v0.8 alpha.3.36 update ordering/default coverage missing: {token}")

    readme = read("README.md")
    changelog = read("CHANGELOG.md")
    roadmap = read("ROADMAP.md")

    for token in (
        "## v0.8.0-alpha.3.36 — Update Status Dispatch Hardening",
        "0.8.0.66",
        "message-only HWND",
        "Nested Windows message loops",
        "active-only HWND timer",
    ):
        if token not in readme:
            fail(f"v0.8 alpha.3.36 README contract missing: {token}")

    for token in (
        "## 0.8.0-alpha.3.36",
        "0.8.0.66",
        "HWND_MESSAGE",
        "SetTimer(nullptr, ...)",
        "About-page HWND timer",
    ):
        if token not in changelog:
            fail(f"v0.8 alpha.3.36 changelog contract missing: {token}")

    for token in (
        "v0.8.0-alpha.3.36",
        "v0.8.0-alpha.4",
    ):
        if token not in roadmap:
            fail(f"v0.8 alpha.3.36 roadmap missing: {token}")

    print(
        "v0.8.0-alpha.3.36 updater dispatch hardening verified:",
        "| App message-only HWND owns progress/completion + watchdog",
        "| update workers no longer directly PostThreadMessage",
        "| About has active-only HWND self-reconciliation",
        "| WinHTTP timeout/cancellation + owner-draw repaint preserved",
        "| schema9 + Path Conversion alpha.3.35 preserved",
    )
    raise SystemExit(0)


if version == "0.8.0-alpha.3.35":
    expected_schemas = {
        "kSettingsSchemaVersion": 9,
        "kCommandsSchemaVersion": 2,
        "kUsageSchemaVersion": 1,
    }
    for name, expected in expected_schemas.items():
        actual = cpp_int("src/core/ConfigIO.hpp", name)
        if actual != expected:
            fail(f"v0.8 alpha.3.35 {name}={actual}, expected {expected}")

    if cpp_int("src/core/ProviderCache.cpp", "kProviderCacheSchemaVersion") != 2:
        fail("v0.8 alpha.3.35 must keep provider-cache schemaVersion 2")

    path_h = read("src/ui/ShortcutPathConverterDialog.hpp")
    path_cpp = read("src/ui/ShortcutPathConverterDialog.cpp")
    presentation_cpp = read("src/ui/TopLevelWindowPresentation.cpp")
    app_cpp = read("src/app/App.cpp")
    settings_window_cpp = read("src/ui/SettingsWindow.cpp")

    # Keep the alpha.3.34 selector exactly on the final device-pixel coverage path.
    for token in (
        "struct SelectorRasterTemplate",
        "SelectorTemplateForDpi(",
        "BlendSelectorPixel(",
        "CircleCoverage(",
        "DrawSelectorRaster(",
        "kSamplesPerAxis = 4",
        "return {15, 1.35, 5}",
        "return {17, 1.45, 6}",
        "return {19, 1.60, 7}",
        "return {21, 1.75, 8}",
        "return {23, 1.90, 9}",
        "SetPixelV(",
        "selected || focused",
    ):
        if token not in path_cpp:
            fail(f"v0.8 alpha.3.35 validated selector regressed: {token}")

    for forbidden in (
        "void FillCrispDisk(",
        "OddPixelSize(",
        "BP_RADIOBUTTON",
        "RBS_CHECKEDNORMAL",
        "RBS_UNCHECKEDNORMAL",
        "DrawThemeBackground(",
    ):
        if forbidden in path_cpp:
            fail(f"v0.8 alpha.3.35 obsolete selector renderer returned: {forbidden}")

    # Row alignment must be native: no fake small-image-list height and no text padding.
    for forbidden in (
        "kResultRowHeightLogical",
        "rowHeightImageList_",
        'T(L"    目标"',
        'L"    Target"',
        'T(L"    工作目录"',
        'L"    Working directory"',
        'T(L"    自定义图标"',
        'L"    Custom icon"',
    ):
        if forbidden in path_cpp or forbidden in path_h:
            fail(f"v0.8 alpha.3.35 artificial result-row layout remains: {forbidden}")

    for token in (
        "LVS_EX_CHECKBOXES",
        'T(L"目标"',
        'L"Target"',
        'T(L"工作目录"',
        'L"Working directory"',
        'T(L"自定义图标"',
        'L"Custom icon"',
    ):
        if token not in path_cpp:
            fail(f"v0.8 alpha.3.35 native checkbox/field layout missing: {token}")

    # Keep the rest of Path Conversion frozen.
    for token in (
        "kDefaultWidthLogical = 960",
        "kDefaultHeightLogical = 560",
        "kMinimumWidthLogical = 820",
        "kMinimumHeightLogical = 480",
        "kFieldColumnPercent = 13",
        "kCurrentColumnPercent = 36",
        "kConvertedColumnPercent = 39",
        "kFieldColumnMinimumLogical = 100",
        "kCurrentColumnMinimumLogical = 180",
        "kConvertedColumnMinimumLogical = 180",
        "kStatusColumnMinimumLogical = 100",
        "DrawActionButton(",
        "HandleHeaderCustomDraw(",
        "headerBackground =\n        RGB(250, 251, 252)",
        "headerSeparator =\n        RGB(236, 239, 243)",
        'T(L"转换方式"',
        'T(L"转换后路径"',
        "SelectedFieldCount() const",
        "UpdateSelectionState(",
        "UpdateColumnWidths(",
        "HandleHeaderNotification(",
        'T(L"当前没有可转换的路径"',
        'T(L"已应用 "',
        "listHeight * 44 / 100",
        "LVN_ITEMCHANGED",
        "VK_ESCAPE",
        "VK_LEFT",
        "VK_RIGHT",
        "WM_GETMINMAXINFO",
        "ApplyUserCommandPathUpdates(",
        "win::MakePortablePath(",
        "win::ExpandPortablePath(",
        "ResolveOwnedPopupGeometry(",
        "RevealFullyPainted(",
    ):
        if token not in path_cpp:
            fail(f"v0.8 alpha.3.35 frozen Path Conversion behavior missing: {token}")

    for forbidden in (
        "kIdClose",
        "close_",
        "LVS_EX_GRIDLINES",
        'T(L"所选路径已应用。"',
        'T(L"没有选中要应用的路径转换。"',
        "WS_EX_CLIENTEDGE,\n        WC_LISTVIEWW",
    ):
        if forbidden in path_cpp or forbidden in path_h:
            fail(f"v0.8 alpha.3.35 obsolete Path Conversion interaction returned: {forbidden}")

    for token in (
        "DWMWA_CLOAK",
        "DWMWA_TRANSITIONS_FORCEDISABLED",
        "DwmFlush();",
    ):
        if token not in presentation_cpp:
            fail(f"v0.8 alpha.3.35 must preserve top-level presentation barrier: {token}")

    update_cpp = read("src/platform/UpdateManager.cpp")
    for token in (
        "std::stop_callback",
        "CheckTimedOut",
        "kUpdateCheckWatchdogMs",
        "60ULL * 1000ULL",
        "检查更新超时，请重试",
    ):
        if token not in update_cpp and token not in app_cpp and token not in settings_window_cpp:
            fail(f"v0.8 alpha.3.35 must preserve updater hardening: {token}")

    runtime_smoke = read("scripts/verify_runtime_smoke.ps1")
    for token in (
        "$migratedSettings.schemaVersion -ne 9",
        "shortcutManagerMode",
        "shortcutManagerLastValid",
    ):
        if token not in runtime_smoke:
            fail(f"v0.8 alpha.3.35 must preserve schema-9 runtime smoke coverage: {token}")

    update_tests = read("tests/UpdatePolicyTests.cpp")
    for token in (
        '"0.8.0-alpha.3.34"',
        '"0.8.0-alpha.3.35"',
        "UpdateChannel::Stable",
    ):
        if token not in update_tests:
            fail(f"v0.8 alpha.3.35 update ordering/default coverage missing: {token}")

    readme = read("README.md")
    changelog = read("CHANGELOG.md")
    roadmap = read("ROADMAP.md")

    for token in (
        "## v0.8.0-alpha.3.35 — Path Conversion Row Alignment Fix",
        "0.8.0.65",
        "native Explorer ListView",
        "four-space indentation",
        "DPI-bucketed 4×4 coverage selector",
    ):
        if token not in readme:
            fail(f"v0.8 alpha.3.35 README contract missing: {token}")

    for token in (
        "## 0.8.0-alpha.3.35",
        "0.8.0.65",
        "small-image-list row-height shim",
        "native checkbox state-image slot",
        "alpha.3.34 DPI-bucketed 4×4 coverage mode selector",
    ):
        if token not in changelog:
            fail(f"v0.8 alpha.3.35 changelog contract missing: {token}")

    for token in (
        "v0.8.0-alpha.3.35",
        "v0.8.0-alpha.4",
    ):
        if token not in roadmap:
            fail(f"v0.8 alpha.3.35 roadmap missing: {token}")

    print(
        "v0.8.0-alpha.3.35 Path Conversion row alignment verified:",
        "| native Explorer ListView row/checkbox metrics",
        "| redundant field-label indentation removed",
        "| alpha.3.34 coverage selector frozen",
        "| geometry/columns/behavior unchanged",
        "| schema9/presentation/updater preserved",
    )
    raise SystemExit(0)


if version == "0.8.0-alpha.3.34":
    expected_schemas = {
        "kSettingsSchemaVersion": 9,
        "kCommandsSchemaVersion": 2,
        "kUsageSchemaVersion": 1,
    }
    for name, expected in expected_schemas.items():
        actual = cpp_int("src/core/ConfigIO.hpp", name)
        if actual != expected:
            fail(f"v0.8 alpha.3.34 {name}={actual}, expected {expected}")

    if cpp_int("src/core/ProviderCache.cpp", "kProviderCacheSchemaVersion") != 2:
        fail("v0.8 alpha.3.34 must keep provider-cache schemaVersion 2")

    path_h = read("src/ui/ShortcutPathConverterDialog.hpp")
    path_cpp = read("src/ui/ShortcutPathConverterDialog.cpp")
    presentation_cpp = read("src/ui/TopLevelWindowPresentation.cpp")
    app_cpp = read("src/app/App.cpp")
    settings_window_cpp = read("src/ui/SettingsWindow.cpp")

    for token in (
        "struct SelectorRasterTemplate",
        "SelectorTemplateForDpi(",
        "BlendSelectorPixel(",
        "CircleCoverage(",
        "DrawSelectorRaster(",
        "kSamplesPerAxis = 4",
        "return {15, 1.35, 5}",
        "return {17, 1.45, 6}",
        "return {19, 1.60, 7}",
        "return {21, 1.75, 8}",
        "return {23, 1.90, 9}",
        "SetPixelV(",
        "selected || focused",
    ):
        if token not in path_cpp:
            fail(f"v0.8 alpha.3.34 selector final-fix implementation missing: {token}")

    for forbidden in (
        "void FillCrispDisk(",
        "OddPixelSize(",
        "BP_RADIOBUTTON",
        "RBS_CHECKEDNORMAL",
        "RBS_UNCHECKEDNORMAL",
        "DrawThemeBackground(",
    ):
        if forbidden in path_cpp:
            fail(f"v0.8 alpha.3.34 obsolete selector renderer remains: {forbidden}")

    # Everything outside the selector stays frozen from alpha.3.33.
    for token in (
        "HIMAGELIST rowHeightImageList_{}",
    ):
        if token not in path_h:
            fail(f"v0.8 alpha.3.34 frozen Path Conversion state missing: {token}")

    for token in (
        "kDefaultWidthLogical = 960",
        "kDefaultHeightLogical = 560",
        "kMinimumWidthLogical = 820",
        "kMinimumHeightLogical = 480",
        "kResultRowHeightLogical = 24",
        "kFieldColumnPercent = 13",
        "kCurrentColumnPercent = 36",
        "kConvertedColumnPercent = 39",
        "kFieldColumnMinimumLogical = 100",
        "kCurrentColumnMinimumLogical = 180",
        "kConvertedColumnMinimumLogical = 180",
        "kStatusColumnMinimumLogical = 100",
        "DrawActionButton(",
        "HandleHeaderCustomDraw(",
        "headerBackground =\n        RGB(250, 251, 252)",
        "headerSeparator =\n        RGB(236, 239, 243)",
        "ImageList_Create(",
        "ImageList_Destroy(",
        'T(L"转换方式"',
        'T(L"转换后路径"',
        "SelectedFieldCount() const",
        "UpdateSelectionState(",
        "UpdateColumnWidths(",
        "HandleHeaderNotification(",
        'T(L"当前没有可转换的路径"',
        'T(L"已应用 "',
        "listHeight * 44 / 100",
        "LVN_ITEMCHANGED",
        "VK_ESCAPE",
        "VK_LEFT",
        "VK_RIGHT",
        "WM_GETMINMAXINFO",
        "ApplyUserCommandPathUpdates(",
        "win::MakePortablePath(",
        "win::ExpandPortablePath(",
        "ResolveOwnedPopupGeometry(",
        "RevealFullyPainted(",
    ):
        if token not in path_cpp:
            fail(f"v0.8 alpha.3.34 frozen Path Conversion behavior missing: {token}")

    for forbidden in (
        "kIdClose",
        "close_",
        "LVS_EX_GRIDLINES",
        'T(L"所选路径已应用。"',
        'T(L"没有选中要应用的路径转换。"',
        "WS_EX_CLIENTEDGE,\n        WC_LISTVIEWW",
    ):
        if forbidden in path_cpp or forbidden in path_h:
            fail(f"v0.8 alpha.3.34 obsolete Path Conversion interaction returned: {forbidden}")

    for token in (
        "DWMWA_CLOAK",
        "DWMWA_TRANSITIONS_FORCEDISABLED",
        "DwmFlush();",
    ):
        if token not in presentation_cpp:
            fail(f"v0.8 alpha.3.34 must preserve top-level presentation barrier: {token}")

    update_cpp = read("src/platform/UpdateManager.cpp")
    for token in (
        "std::stop_callback",
        "CheckTimedOut",
        "kUpdateCheckWatchdogMs",
        "60ULL * 1000ULL",
        "检查更新超时，请重试",
    ):
        if token not in update_cpp and token not in app_cpp and token not in settings_window_cpp:
            fail(f"v0.8 alpha.3.34 must preserve updater hardening: {token}")

    runtime_smoke = read("scripts/verify_runtime_smoke.ps1")
    for token in (
        "$migratedSettings.schemaVersion -ne 9",
        "shortcutManagerMode",
        "shortcutManagerLastValid",
    ):
        if token not in runtime_smoke:
            fail(f"v0.8 alpha.3.34 must preserve schema-9 runtime smoke coverage: {token}")

    update_tests = read("tests/UpdatePolicyTests.cpp")
    for token in (
        '"0.8.0-alpha.3.33"',
        '"0.8.0-alpha.3.34"',
        "UpdateChannel::Stable",
    ):
        if token not in update_tests:
            fail(f"v0.8 alpha.3.34 update ordering/default coverage missing: {token}")

    readme = read("README.md")
    changelog = read("CHANGELOG.md")
    roadmap = read("ROADMAP.md")

    for token in (
        "## v0.8.0-alpha.3.34 — Path Conversion Selector Final Fix",
        "0.8.0.64",
        "4×4 subpixel coverage",
        "15/17/19/21/23 physical-pixel",
        "960×560 / 820×480 logical pixels",
    ):
        if token not in readme:
            fail(f"v0.8 alpha.3.34 README contract missing: {token}")

    for token in (
        "## 0.8.0-alpha.3.34",
        "0.8.0.64",
        "4×4 subpixel coverage raster",
        "15/17/19/21/23 physical pixels",
        "settings schemaVersion 9",
    ):
        if token not in changelog:
            fail(f"v0.8 alpha.3.34 changelog contract missing: {token}")

    for token in (
        "v0.8.0-alpha.3.34",
        "v0.8.0-alpha.4",
    ):
        if token not in roadmap:
            fail(f"v0.8 alpha.3 roadmap missing: {token}")

    print(
        "v0.8.0-alpha.3.34 Path Conversion selector final fix verified:",
        "| discrete DPI buckets 15/17/19/21/23 px",
        "| 4x4 coverage raster blended directly to final HDC",
        "| no themed/hard-edged legacy selector",
        "| all alpha.3.33 UI/behavior frozen",
        "| schema9/presentation/updater preserved",
    )
    raise SystemExit(0)


if version == "0.8.0-alpha.3.33":
    expected_schemas = {
        "kSettingsSchemaVersion": 9,
        "kCommandsSchemaVersion": 2,
        "kUsageSchemaVersion": 1,
    }
    for name, expected in expected_schemas.items():
        actual = cpp_int("src/core/ConfigIO.hpp", name)
        if actual != expected:
            fail(f"v0.8 alpha.3.33 {name}={actual}, expected {expected}")

    if cpp_int("src/core/ProviderCache.cpp", "kProviderCacheSchemaVersion") != 2:
        fail("v0.8 alpha.3.33 must keep provider-cache schemaVersion 2")

    path_h = read("src/ui/ShortcutPathConverterDialog.hpp")
    path_cpp = read("src/ui/ShortcutPathConverterDialog.cpp")
    presentation_cpp = read("src/ui/TopLevelWindowPresentation.cpp")
    app_cpp = read("src/app/App.cpp")
    settings_window_cpp = read("src/ui/SettingsWindow.cpp")

    for token in (
        "HIMAGELIST rowHeightImageList_{}",
    ):
        if token not in path_h:
            fail(f"v0.8 alpha.3.33 Path Conversion state missing: {token}")

    for token in (
        "kResultRowHeightLogical = 24",
        "void FillCrispDisk(",
        "OddPixelSize(",
        "Scale(13)",
        "Scale(5)",
        "ImageList_Create(",
        "ImageList_Destroy(",
        "headerBackground =\n        RGB(250, 251, 252)",
        "headerSeparator =\n        RGB(236, 239, 243)",
        "rect.top + Scale(6)",
        "rect.bottom - Scale(6)",
        "DrawActionButton(",
        "HandleHeaderCustomDraw(",
        "SetWindowTheme(",
        'L"Explorer"',
    ):
        if token not in path_cpp:
            fail(f"v0.8 alpha.3.33 Path Conversion visual closeout missing: {token}")

    for forbidden in (
        "#include <vssym32.h>",
        "BP_RADIOBUTTON",
        "RBS_CHECKEDNORMAL",
        "RBS_UNCHECKEDNORMAL",
        "DrawThemeBackground(",
        "selected || focused\n                ? Scale(2)",
    ):
        if forbidden in path_cpp:
            fail(f"v0.8 alpha.3.33 obsolete heavy/soft selector path remains: {forbidden}")

    # Keep alpha.3.31/3.32 behavior and geometry frozen.
    for token in (
        "kDefaultWidthLogical = 960",
        "kDefaultHeightLogical = 560",
        "kMinimumWidthLogical = 820",
        "kMinimumHeightLogical = 480",
        "kFieldColumnPercent = 13",
        "kCurrentColumnPercent = 36",
        "kConvertedColumnPercent = 39",
        "kFieldColumnMinimumLogical = 100",
        "kCurrentColumnMinimumLogical = 180",
        "kConvertedColumnMinimumLogical = 180",
        "kStatusColumnMinimumLogical = 100",
        'T(L"转换方式"',
        'T(L"转换后路径"',
        "SelectedFieldCount() const",
        "UpdateSelectionState(",
        "UpdateColumnWidths(",
        "HandleHeaderNotification(",
        'T(L"当前没有可转换的路径"',
        'T(L"已应用 "',
        "listHeight * 44 / 100",
        "LVN_ITEMCHANGED",
        "VK_ESCAPE",
        "VK_LEFT",
        "VK_RIGHT",
        "WM_GETMINMAXINFO",
        "ApplyUserCommandPathUpdates(",
        "win::MakePortablePath(",
        "win::ExpandPortablePath(",
        "ResolveOwnedPopupGeometry(",
        "RevealFullyPainted(",
    ):
        if token not in path_cpp:
            fail(f"v0.8 alpha.3.33 frozen Path Conversion behavior missing: {token}")

    for forbidden in (
        "kIdClose",
        "close_",
        "LVS_EX_GRIDLINES",
        'T(L"所选路径已应用。"',
        'T(L"没有选中要应用的路径转换。"',
        "WS_EX_CLIENTEDGE,\n        WC_LISTVIEWW",
    ):
        if forbidden in path_cpp or forbidden in path_h:
            fail(f"v0.8 alpha.3.33 obsolete Path Conversion interaction returned: {forbidden}")

    for token in (
        "DWMWA_CLOAK",
        "DWMWA_TRANSITIONS_FORCEDISABLED",
        "DwmFlush();",
    ):
        if token not in presentation_cpp:
            fail(f"v0.8 alpha.3.33 must preserve alpha.3.30 presentation barrier: {token}")

    update_cpp = read("src/platform/UpdateManager.cpp")
    for token in (
        "std::stop_callback",
        "CheckTimedOut",
        "kUpdateCheckWatchdogMs",
        "60ULL * 1000ULL",
        "检查更新超时，请重试",
    ):
        if token not in update_cpp and token not in app_cpp and token not in settings_window_cpp:
            fail(f"v0.8 alpha.3.33 must preserve updater hardening: {token}")

    runtime_smoke = read("scripts/verify_runtime_smoke.ps1")
    for token in (
        "$migratedSettings.schemaVersion -ne 9",
        "shortcutManagerMode",
        "shortcutManagerLastValid",
    ):
        if token not in runtime_smoke:
            fail(f"v0.8 alpha.3.33 must preserve schema-9 runtime smoke coverage: {token}")

    update_tests = read("tests/UpdatePolicyTests.cpp")
    for token in (
        '"0.8.0-alpha.3.32"',
        '"0.8.0-alpha.3.33"',
        "UpdateChannel::Stable",
    ):
        if token not in update_tests:
            fail(f"v0.8 alpha.3.33 update ordering/default coverage missing: {token}")

    readme = read("README.md")
    changelog = read("CHANGELOG.md")
    roadmap = read("ROADMAP.md")

    for token in (
        "## v0.8.0-alpha.3.33 — Path Conversion Visual Closeout",
        "0.8.0.63",
        "integer-raster ring",
        "1-pixel accent border",
        "24 logical-pixel",
        "960×560 / 820×480 logical pixels",
    ):
        if token not in readme:
            fail(f"v0.8 alpha.3.33 README contract missing: {token}")

    for token in (
        "## 0.8.0-alpha.3.33",
        "0.8.0.63",
        "integer-raster selector",
        "1 physical pixel",
        "24 logical-pixel",
    ):
        if token not in changelog:
            fail(f"v0.8 alpha.3.33 changelog contract missing: {token}")

    for token in (
        "v0.8.0-alpha.3.33",
        "v0.8.0-alpha.4",
    ):
        if token not in roadmap:
            fail(f"v0.8 alpha.3 roadmap missing: {token}")

    print(
        "v0.8.0-alpha.3.33 Path Conversion visual closeout verified:",
        "| integer-raster selector replaces themed/GDI radio",
        "| mode-card emphasis is 1px",
        "| Header separators softened",
        "| result rows use 24 logical-pixel baseline",
        "| alpha.3.31/3.32 behavior + schema9/presentation/updater preserved",
    )
    raise SystemExit(0)


if version == "0.8.0-alpha.3.32":
    expected_schemas = {
        "kSettingsSchemaVersion": 9,
        "kCommandsSchemaVersion": 2,
        "kUsageSchemaVersion": 1,
    }
    for name, expected in expected_schemas.items():
        actual = cpp_int("src/core/ConfigIO.hpp", name)
        if actual != expected:
            fail(f"v0.8 alpha.3.32 {name}={actual}, expected {expected}")

    if cpp_int("src/core/ProviderCache.cpp", "kProviderCacheSchemaVersion") != 2:
        fail("v0.8 alpha.3.32 must keep provider-cache schemaVersion 2")

    path_h = read("src/ui/ShortcutPathConverterDialog.hpp")
    path_cpp = read("src/ui/ShortcutPathConverterDialog.cpp")
    presentation_cpp = read("src/ui/TopLevelWindowPresentation.cpp")
    app_cpp = read("src/app/App.cpp")
    settings_window_cpp = read("src/ui/SettingsWindow.cpp")

    for token in (
        "#include <uxtheme.h>",
        "#include <vssym32.h>",
        "BP_RADIOBUTTON",
        "RBS_CHECKEDNORMAL",
        "RBS_UNCHECKEDNORMAL",
        "DrawThemeBackground(",
        "DrawFrameControl(",
        "DrawActionButton(",
        "HandleHeaderCustomDraw(",
        "SetWindowTheme(",
        'L"Explorer"',
        "listHeight * 44 / 100",
        "BS_OWNERDRAW",
        "kIdRescan",
        "kIdApply",
    ):
        if token not in path_cpp:
            fail(f"v0.8 alpha.3.32 Path Conversion visual polish missing: {token}")

    for token in (
        "void DrawActionButton(",
        "HandleHeaderCustomDraw(",
    ):
        if token not in path_h:
            fail(f"v0.8 alpha.3.32 Path Conversion declaration missing: {token}")

    if "WS_EX_CLIENTEDGE,\n        WC_LISTVIEWW" in path_cpp:
        fail("v0.8 alpha.3.32 Path Conversion must remove the sunken ListView client edge")

    # Keep alpha.3.31 layout, interaction and conversion behavior frozen.
    for token in (
        "kDefaultWidthLogical = 960",
        "kDefaultHeightLogical = 560",
        "kMinimumWidthLogical = 820",
        "kMinimumHeightLogical = 480",
        "kFieldColumnPercent = 13",
        "kCurrentColumnPercent = 36",
        "kConvertedColumnPercent = 39",
        "kFieldColumnMinimumLogical = 100",
        "kCurrentColumnMinimumLogical = 180",
        "kConvertedColumnMinimumLogical = 180",
        "kStatusColumnMinimumLogical = 100",
        'T(L"转换方式"',
        'T(L"转换后路径"',
        "SelectedFieldCount() const",
        "UpdateSelectionState(",
        "UpdateColumnWidths(",
        "HandleHeaderNotification(",
        'T(L"当前没有可转换的路径"',
        'T(L"已应用 "',
        "LVN_ITEMCHANGED",
        "VK_ESCAPE",
        "VK_LEFT",
        "VK_RIGHT",
        "WM_GETMINMAXINFO",
        "ApplyUserCommandPathUpdates(",
        "win::MakePortablePath(",
        "win::ExpandPortablePath(",
        "ResolveOwnedPopupGeometry(",
        "RevealFullyPainted(",
    ):
        if token not in path_cpp:
            fail(f"v0.8 alpha.3.32 frozen Path Conversion behavior missing: {token}")

    for forbidden in (
        "kIdClose",
        "close_",
        "LVS_EX_GRIDLINES",
        'T(L"所选路径已应用。"',
        'T(L"没有选中要应用的路径转换。"',
    ):
        if forbidden in path_cpp or forbidden in path_h:
            fail(f"v0.8 alpha.3.32 obsolete Path Conversion interaction returned: {forbidden}")

    for token in (
        "DWMWA_CLOAK",
        "DWMWA_TRANSITIONS_FORCEDISABLED",
        "DwmFlush();",
    ):
        if token not in presentation_cpp:
            fail(f"v0.8 alpha.3.32 must preserve alpha.3.30 presentation barrier: {token}")

    update_cpp = read("src/platform/UpdateManager.cpp")
    for token in (
        "std::stop_callback",
        "CheckTimedOut",
        "kUpdateCheckWatchdogMs",
        "60ULL * 1000ULL",
        "检查更新超时，请重试",
    ):
        if token not in update_cpp and token not in app_cpp and token not in settings_window_cpp:
            fail(f"v0.8 alpha.3.32 must preserve updater hardening: {token}")

    runtime_smoke = read("scripts/verify_runtime_smoke.ps1")
    for token in (
        "$migratedSettings.schemaVersion -ne 9",
        "shortcutManagerMode",
        "shortcutManagerLastValid",
    ):
        if token not in runtime_smoke:
            fail(f"v0.8 alpha.3.32 must preserve schema-9 runtime smoke coverage: {token}")

    update_tests = read("tests/UpdatePolicyTests.cpp")
    for token in (
        '"0.8.0-alpha.3.31"',
        '"0.8.0-alpha.3.32"',
        "UpdateChannel::Stable",
    ):
        if token not in update_tests:
            fail(f"v0.8 alpha.3.32 update ordering/default coverage missing: {token}")

    readme = read("README.md")
    changelog = read("CHANGELOG.md")
    roadmap = read("ROADMAP.md")

    for token in (
        "## v0.8.0-alpha.3.32 — Path Conversion Visual Polish",
        "0.8.0.62",
        "BP_RADIOBUTTON",
        "WS_EX_CLIENTEDGE",
        "44% of the result-area height",
        "960×560 / 820×480 logical pixels",
    ):
        if token not in readme:
            fail(f"v0.8 alpha.3.32 README contract missing: {token}")

    for token in (
        "## 0.8.0-alpha.3.32",
        "0.8.0.62",
        "BP_RADIOBUTTON",
        "WS_EX_CLIENTEDGE",
        "44%",
        "应用所选",
    ):
        if token not in changelog:
            fail(f"v0.8 alpha.3.32 changelog contract missing: {token}")

    for token in (
        "v0.8.0-alpha.3.32",
        "v0.8.0-alpha.4",
    ):
        if token not in roadmap:
            fail(f"v0.8 alpha.3 roadmap missing: {token}")

    print(
        "v0.8.0-alpha.3.32 Path Conversion visual polish verified:",
        "| themed DPI-crisp radio glyph",
        "| flat secondary/primary action buttons",
        "| flat custom ListView header + no client edge",
        "| empty state balanced at 44%",
        "| alpha.3.31 behavior/columns + schema9/presentation/updater preserved",
    )
    raise SystemExit(0)


if version == "0.8.0-alpha.3.31":
    expected_schemas = {
        "kSettingsSchemaVersion": 9,
        "kCommandsSchemaVersion": 2,
        "kUsageSchemaVersion": 1,
    }
    for name, expected in expected_schemas.items():
        actual = cpp_int("src/core/ConfigIO.hpp", name)
        if actual != expected:
            fail(f"v0.8 alpha.3.31 {name}={actual}, expected {expected}")

    if cpp_int("src/core/ProviderCache.cpp", "kProviderCacheSchemaVersion") != 2:
        fail("v0.8 alpha.3.31 must keep provider-cache schemaVersion 2")

    path_h = read("src/ui/ShortcutPathConverterDialog.hpp")
    path_cpp = read("src/ui/ShortcutPathConverterDialog.cpp")
    presentation_cpp = read("src/ui/TopLevelWindowPresentation.cpp")
    app_cpp = read("src/app/App.cpp")
    settings_window_cpp = read("src/ui/SettingsWindow.cpp")

    for token in (
        "kDefaultWidthLogical = 960",
        "kDefaultHeightLogical = 560",
        "kMinimumWidthLogical = 820",
        "kMinimumHeightLogical = 480",
        "kFieldColumnPercent = 13",
        "kCurrentColumnPercent = 36",
        "kConvertedColumnPercent = 39",
        "kFieldColumnMinimumLogical = 100",
        "kCurrentColumnMinimumLogical = 180",
        "kConvertedColumnMinimumLogical = 180",
        "kStatusColumnMinimumLogical = 100",
        "BS_OWNERDRAW",
        'T(L"转换方式"',
        'T(L"转换后路径"',
        'T(L"仅转换目标、工作目录和自定义图标；参数、URL、UNC 路径和裸命令保持不变。"',
        "DrawModeCard(",
        "SelectedFieldCount() const",
        "UpdateSelectionState(",
        "UpdateColumnWidths(",
        "HandleHeaderNotification(",
        'T(L"当前没有可转换的路径"',
        'T(L"没有发现需要进行便携化转换的快捷项"',
        'T(L"没有发现需要展开为绝对路径的快捷项"',
        'T(L"已应用 "',
        "LVN_ITEMCHANGED",
        "VK_ESCAPE",
        "VK_LEFT",
        "VK_RIGHT",
        "WM_GETMINMAXINFO",
        "ResolveOwnedPopupGeometry(",
        "RevealFullyPainted(",
    ):
        if token not in path_cpp:
            fail(f"v0.8 alpha.3.31 Path Conversion redesign missing: {token}")

    for token in (
        "modeTitle_",
        "rule_",
        "status_",
        "rebuildingList_",
        "customColumnWidths_",
        "adjustingColumnWidths_",
        "convertibleShortcutCount_",
    ):
        if token not in path_h:
            fail(f"v0.8 alpha.3.31 Path Conversion state declaration missing: {token}")

    for forbidden in (
        "kIdClose",
        "close_",
        "LVS_EX_GRIDLINES",
        'T(L"所选路径已应用。"',
        'T(L"没有选中要应用的路径转换。"',
    ):
        if forbidden in path_cpp or forbidden in path_h:
            fail(f"v0.8 alpha.3.31 Path Conversion obsolete interaction remains: {forbidden}")

    for token in (
        "window_presentation::",
        "HideForDestroy(",
        "RevealFullyPainted(",
    ):
        if token not in path_cpp:
            fail(f"v0.8 alpha.3.31 must preserve shared top-level presentation: {token}")

    for token in (
        "DWMWA_CLOAK",
        "DWMWA_TRANSITIONS_FORCEDISABLED",
        "DwmFlush();",
    ):
        if token not in presentation_cpp:
            fail(f"v0.8 alpha.3.31 must preserve alpha.3.30 presentation barrier: {token}")

    update_cpp = read("src/platform/UpdateManager.cpp")
    for token in (
        "std::stop_callback",
        "CheckTimedOut",
        "kUpdateCheckWatchdogMs",
        "60ULL * 1000ULL",
        "检查更新超时，请重试",
    ):
        if token not in update_cpp and token not in app_cpp and token not in settings_window_cpp:
            fail(f"v0.8 alpha.3.31 must preserve updater hardening: {token}")

    runtime_smoke = read("scripts/verify_runtime_smoke.ps1")
    for token in (
        "$migratedSettings.schemaVersion -ne 9",
        "shortcutManagerMode",
        "shortcutManagerLastValid",
    ):
        if token not in runtime_smoke:
            fail(f"v0.8 alpha.3.31 must preserve schema-9 runtime smoke coverage: {token}")

    update_tests = read("tests/UpdatePolicyTests.cpp")
    for token in (
        '"0.8.0-alpha.3.30"',
        '"0.8.0-alpha.3.31"',
        "UpdateChannel::Stable",
    ):
        if token not in update_tests:
            fail(f"v0.8 alpha.3.31 update ordering/default coverage missing: {token}")

    readme = read("README.md")
    changelog = read("CHANGELOG.md")
    roadmap = read("ROADMAP.md")

    for token in (
        "## v0.8.0-alpha.3.31 — Path Conversion UX Redesign",
        "0.8.0.61",
        "960×560 logical pixels",
        "820×480 logical",
        "便携化",
        "转换后路径",
        "应用所选",
        "Escape",
    ):
        if token not in readme:
            fail(f"v0.8 alpha.3.31 README contract missing: {token}")

    for token in (
        "## 0.8.0-alpha.3.31",
        "0.8.0.61",
        "960×560",
        "820×480",
        "已应用 N 个路径转换",
        "关闭",
    ):
        if token not in changelog:
            fail(f"v0.8 alpha.3.31 changelog contract missing: {token}")

    for token in (
        "v0.8.0-alpha.3.31",
        "v0.8.0-alpha.4",
    ):
        if token not in roadmap:
            fail(f"v0.8 alpha.3 roadmap missing: {token}")

    print(
        "v0.8.0-alpha.3.31 Path Conversion UX redesign verified:",
        "| compact 960x560 default / 820x480 minimum",
        "| mode cards + keyboard switching",
        "| responsive columns + protected Status",
        "| explicit empty state + live selection/apply status",
        "| no redundant Close button / success modal",
        "| shared top-level presentation + schema9 preserved",
    )
    raise SystemExit(0)


if version == "0.8.0-alpha.3.30":
    expected_schemas = {
        "kSettingsSchemaVersion": 9,
        "kCommandsSchemaVersion": 2,
        "kUsageSchemaVersion": 1,
    }
    for name, expected in expected_schemas.items():
        actual = cpp_int("src/core/ConfigIO.hpp", name)
        if actual != expected:
            fail(f"v0.8 alpha.3.30 {name}={actual}, expected {expected}")

    if cpp_int("src/core/ProviderCache.cpp", "kProviderCacheSchemaVersion") != 2:
        fail("v0.8 alpha.3.30 must keep provider-cache schemaVersion 2")

    cmake = read("CMakeLists.txt")
    presentation_h = read("src/ui/TopLevelWindowPresentation.hpp")
    presentation_cpp = read("src/ui/TopLevelWindowPresentation.cpp")
    launcher_h = read("src/ui/LauncherWindow.hpp")
    launcher_cpp = read("src/ui/LauncherWindow.cpp")
    settings_cpp = read("src/ui/SettingsWindow.cpp")
    manager_cpp = read("src/ui/ShortcutManagerWindow.cpp")
    editor_h = read("src/ui/ShortcutEditorDialog.hpp")
    editor_cpp = read("src/ui/ShortcutEditorDialog.cpp")
    path_h = read("src/ui/ShortcutPathConverterDialog.hpp")
    path_cpp = read("src/ui/ShortcutPathConverterDialog.cpp")
    app_cpp = read("src/app/App.cpp")

    if "src/ui/TopLevelWindowPresentation.cpp" not in cmake:
        fail("v0.8 alpha.3.30 must compile shared top-level presentation module")

    for token in (
        "struct CreationGeometry",
        "SetCloaked(",
        "Configure(",
        "ProbeMonitorDpi(",
        "ResolveOwnedPopupGeometry(",
        "CenterExistingWindow(",
        "RevealFullyPainted(",
        "HideForDestroy(",
    ):
        if token not in presentation_h:
            fail(f"v0.8 alpha.3.30 presentation declaration missing: {token}")

    for token in (
        "DWMWA_CLOAK",
        "DWMWA_TRANSITIONS_FORCEDISABLED",
        "DwmFlush();",
        "ResolveOwnerOrCursorMonitor(",
        "ClampRectToWorkArea(",
        "ResolveWindowOrigin(",
        "RDW_ALLCHILDREN",
        "SWP_HIDEWINDOW",
    ):
        if token not in presentation_cpp:
            fail(f"v0.8 alpha.3.30 presentation implementation missing: {token}")

    custom_windows = {
        "Launcher": launcher_cpp,
        "Settings": settings_cpp,
        "Shortcut Manager": manager_cpp,
        "Shortcut Editor": editor_cpp,
        "Path Conversion": path_cpp,
    }

    for name, source in custom_windows.items():
        if "window_presentation::Configure(" not in source:
            fail(f"v0.8 alpha.3.30 {name} must use shared Configure() presentation policy")
        if "RevealFullyPainted(" not in source:
            fail(f"v0.8 alpha.3.30 {name} must use shared first-frame reveal policy")

    for name, source in (
        ("Launcher", launcher_cpp),
        ("Shortcut Editor", editor_cpp),
        ("Path Conversion", path_cpp),
    ):
        if "CW_USEDEFAULT,\n        CW_USEDEFAULT" in source:
            fail(f"v0.8 alpha.3.30 {name} real HWND must not be born at CW_USEDEFAULT")

    for token in (
        "firstRevealPending_{true}",
    ):
        if token not in launcher_h:
            fail(f"v0.8 alpha.3.30 Launcher first-reveal state missing: {token}")

    for token in (
        "ResolveOwnedPopupGeometry(",
        "firstRevealPending_ = false",
        "RevealFullyPainted(",
    ):
        if token not in launcher_cpp:
            fail(f"v0.8 alpha.3.30 Launcher presentation hardening missing: {token}")

    for token in (
        "void ShortcutEditorDialog::CloseWindow()",
        "ResolveOwnedPopupGeometry(",
        "CenterExistingWindow(",
        "RevealFullyPainted(",
        "HideForDestroy(",
    ):
        if token not in editor_cpp:
            fail(f"v0.8 alpha.3.30 Shortcut Editor presentation hardening missing: {token}")

    if "void CloseWindow();" not in editor_h:
        fail("v0.8 alpha.3.30 Shortcut Editor must centralize close/teardown")

    for token in (
        "CloseWindow()",
        "ResolveOwnedPopupGeometry(",
        "CenterExistingWindow(",
        "RevealFullyPainted(",
        "HideForDestroy(",
    ):
        if token not in path_cpp:
            fail(f"v0.8 alpha.3.30 Path Conversion presentation hardening missing: {token}")

    if "void CloseWindow();" not in path_h:
        fail("v0.8 alpha.3.30 Path Conversion must centralize close/teardown")

    for token in (
        "HideForDestroy(",
        "RevealFullyPainted(",
    ):
        if token not in settings_cpp or token not in manager_cpp:
            fail(f"v0.8 alpha.3.30 Settings/Manager must share common presentation primitive: {token}")

    for forbidden in (
        "SetSettingsDwmCloak",
        "ConfigureSettingsDwmPresentation",
    ):
        if forbidden in settings_cpp:
            fail(f"v0.8 alpha.3.30 Settings must not keep private DWM policy: {forbidden}")

    for forbidden in (
        "SetShortcutManagerDwmCloak",
        "ConfigureShortcutManagerDwmPresentation",
    ):
        if forbidden in manager_cpp:
            fail(f"v0.8 alpha.3.30 Manager must not keep private DWM policy: {forbidden}")

    # Keep the placement/column/editor/update closeouts protected.
    for token in (
        "ResolveWindowOrigin(",
        'settings.settingsPlacement ==\n                    "top"',
    ):
        if token not in settings_cpp:
            fail(f"v0.8 alpha.3.30 must preserve Settings placement repair: {token}")

    for token in (
        "kKeywordColumnPercent = 16",
        "kNameColumnPercent = 24",
        "kTypeColumnPercent = 14",
        "kKeywordColumnMinimumLogical = 107",
        "kNameColumnMinimumLogical = 161",
        "kTypeColumnMinimumLogical = 94",
        "kTargetColumnMinimumLogical = 180",
        "RememberShortcutManagerPosition(",
    ):
        if token not in manager_cpp:
            fail(f"v0.8 alpha.3.30 must preserve Manager column/placement closeout: {token}")

    for token in (
        "Deliberately do not draw ODS_FOCUS here",
        "BCM_GETIDEALSIZE",
        "kEditorWidthLogical = 590",
        "kAdvancedRightInsetLogical = 18",
    ):
        if token not in editor_cpp:
            fail(f"v0.8 alpha.3.30 must preserve Editor interaction/layout closeout: {token}")

    update_cpp = read("src/platform/UpdateManager.cpp")
    for token in (
        "std::stop_callback",
        "CheckTimedOut",
        "kUpdateCheckWatchdogMs",
        "60ULL * 1000ULL",
        "检查更新超时，请重试",
    ):
        if token not in update_cpp and token not in app_cpp and token not in settings_cpp:
            fail(f"v0.8 alpha.3.30 must preserve updater hardening: {token}")

    runtime_smoke = read("scripts/verify_runtime_smoke.ps1")
    for token in (
        "$migratedSettings.schemaVersion -ne 9",
        "shortcutManagerMode",
        "shortcutManagerLastValid",
    ):
        if token not in runtime_smoke:
            fail(f"v0.8 alpha.3.30 must preserve schema-9 runtime smoke coverage: {token}")

    update_tests = read("tests/UpdatePolicyTests.cpp")
    for token in (
        '"0.8.0-alpha.3.29"',
        '"0.8.0-alpha.3.30"',
        "UpdateChannel::Stable",
    ):
        if token not in update_tests:
            fail(f"v0.8 alpha.3.30 update ordering/default coverage missing: {token}")

    readme = read("README.md")
    changelog = read("CHANGELOG.md")
    roadmap = read("ROADMAP.md")

    for token in (
        "## v0.8.0-alpha.3.30 — Systematic Top-Level Window Presentation",
        "0.8.0.60",
        "Launcher, Settings, Shortcut Manager, Shortcut Editor and Path Conversion",
        "TopLevelWindowPresentation",
        "CW_USEDEFAULT",
    ):
        if token not in readme:
            fail(f"v0.8 alpha.3.30 README contract missing: {token}")

    for token in (
        "## 0.8.0-alpha.3.30",
        "0.8.0.60",
        "TopLevelWindowPresentation",
        "Launcher, Settings, Shortcut Manager, Shortcut Editor and Path Conversion",
        "CW_USEDEFAULT",
    ):
        if token not in changelog:
            fail(f"v0.8 alpha.3.30 changelog contract missing: {token}")

    for token in (
        "v0.8.0-alpha.3.30",
        "v0.8.0-alpha.4",
    ):
        if token not in roadmap:
            fail(f"v0.8 alpha.3 roadmap missing: {token}")

    print(
        "v0.8.0-alpha.3.30 systematic top-level presentation verified:",
        "| five custom top-level windows share Configure/Reveal policy",
        "| Launcher/Editor/Path no CW_USEDEFAULT birth rectangle",
        "| Editor/Path owner-centered before reveal",
        "| Settings/Manager private DWM copies removed",
        "| schema9 + Manager columns + Editor/update closeouts preserved",
    )
    raise SystemExit(0)


if version == "0.8.0-alpha.3.29":
    expected_schemas = {
        "kSettingsSchemaVersion": 9,
        "kCommandsSchemaVersion": 2,
        "kUsageSchemaVersion": 1,
    }
    for name, expected in expected_schemas.items():
        actual = cpp_int("src/core/ConfigIO.hpp", name)
        if actual != expected:
            fail(f"v0.8 alpha.3.29 {name}={actual}, expected {expected}")

    if cpp_int("src/core/ProviderCache.cpp", "kProviderCacheSchemaVersion") != 2:
        fail("v0.8 alpha.3.29 must keep provider-cache schemaVersion 2")

    layout_h = read("src/core/SettingsLayout.hpp")
    layout_cpp = read("src/core/SettingsLayout.cpp")
    settings_window_cpp = read("src/ui/SettingsWindow.cpp")
    manager_cpp = read("src/ui/ShortcutManagerWindow.cpp")
    manager_h = read("src/ui/ShortcutManagerWindow.hpp")
    config_tests = read("tests/ConfigCoreTests.cpp")
    app_cpp = read("src/app/App.cpp")

    for token in (
        "struct Point",
        "ResolveWindowOrigin(",
        "bool nearTop",
        "int nearTopOffset",
    ):
        if token not in layout_h:
            fail(f"v0.8 alpha.3.29 shared placement declaration missing: {token}")

    for token in (
        "Point ResolveWindowOrigin(",
        "if (!nearTop)",
        "(workHeight -\n             height) / 5",
        "std::clamp(",
    ):
        if token not in layout_cpp:
            fail(f"v0.8 alpha.3.29 shared placement implementation missing: {token}")

    if settings_window_cpp.count("ResolveWindowOrigin(") < 2:
        fail("v0.8 alpha.3.29 Settings creation/show paths must both use ResolveWindowOrigin")

    for token in (
        'settings.settingsPlacement ==\n                        "top"',
        'settings.settingsPlacement ==\n                    "top"',
        "PositionForShow()",
    ):
        if token not in settings_window_cpp:
            fail(f"v0.8 alpha.3.29 Settings top placement repair missing: {token}")

    for token in (
        "ResolveShortcutManagerCreationGeometry(",
        "ResolveShortcutManagerMonitor(",
        "ResolveShortcutManagerRect(",
        "ProbeShortcutManagerMonitorDpi(",
        "DWMWA_CLOAK",
        "DWMWA_TRANSITIONS_FORCEDISABLED",
        "DwmFlush();",
        "fully-painted final frame",
        "Creating at CW_USEDEFAULT",
        "ResolveWindowOrigin(",
        "ClampRectToWorkArea(",
    ):
        if token not in manager_cpp:
            fail(f"v0.8 alpha.3.29 Manager first-frame hardening missing: {token}")

    if "CW_USEDEFAULT,\n        CW_USEDEFAULT" in manager_cpp:
        fail("v0.8 alpha.3.29 real Shortcut Manager HWND must not be born at CW_USEDEFAULT")

    for forbidden in (
        "savedWindowPlacement_",
        "savedWindowPlacementValid_",
        "CaptureWindowState()",
    ):
        if forbidden in manager_cpp or forbidden in manager_h:
            fail(f"v0.8 alpha.3.29 must preserve ephemeral Manager sizing: {forbidden}")

    for token in (
        '#include "core/SettingsLayout.hpp"',
        "ResolveWindowOrigin(",
        "centered.y == 210",
        "nearTop.y == 84",
        "constrained.y == 50",
    ):
        if token not in config_tests:
            fail(f"v0.8 alpha.3.29 placement-math coverage missing: {token}")

    for token in (
        "kKeywordColumnPercent = 16",
        "kNameColumnPercent = 24",
        "kTypeColumnPercent = 14",
        "kKeywordColumnMinimumLogical = 107",
        "kNameColumnMinimumLogical = 161",
        "kTypeColumnMinimumLogical = 94",
        "kTargetColumnMinimumLogical = 180",
        "RememberShortcutManagerPosition(",
    ):
        if token not in manager_cpp:
            fail(f"v0.8 alpha.3.29 must preserve Manager column/placement closeout: {token}")

    editor_cpp = read("src/ui/ShortcutEditorDialog.cpp")
    for token in (
        "Deliberately do not draw ODS_FOCUS here",
        "BCM_GETIDEALSIZE",
        "kEditorWidthLogical = 590",
        "kAdvancedRightInsetLogical = 18",
    ):
        if token not in editor_cpp:
            fail(f"v0.8 alpha.3.29 must preserve Editor closeout: {token}")

    update_cpp = read("src/platform/UpdateManager.cpp")
    for token in (
        "std::stop_callback",
        "CheckTimedOut",
        "kUpdateCheckWatchdogMs",
        "60ULL * 1000ULL",
        "检查更新超时，请重试",
    ):
        if token not in update_cpp and token not in app_cpp and token not in settings_window_cpp:
            fail(f"v0.8 alpha.3.29 must preserve updater hardening: {token}")

    runtime_smoke = read("scripts/verify_runtime_smoke.ps1")
    for token in (
        "$migratedSettings.schemaVersion -ne 9",
        "shortcutManagerMode",
        "shortcutManagerLastValid",
    ):
        if token not in runtime_smoke:
            fail(f"v0.8 alpha.3.29 must preserve schema-9 runtime smoke coverage: {token}")

    update_tests = read("tests/UpdatePolicyTests.cpp")
    for token in (
        '"0.8.0-alpha.3.28"',
        '"0.8.0-alpha.3.29"',
        "UpdateChannel::Stable",
    ):
        if token not in update_tests:
            fail(f"v0.8 alpha.3.29 update ordering/default coverage missing: {token}")

    readme = read("README.md")
    changelog = read("CHANGELOG.md")
    roadmap = read("ROADMAP.md")

    for token in (
        "## v0.8.0-alpha.3.29 — Window Placement & Presentation Hardening",
        "0.8.0.59",
        "ResolveWindowOrigin()",
        "CW_USEDEFAULT",
        "first-frame DWM barrier",
    ):
        if token not in readme:
            fail(f"v0.8 alpha.3.29 README contract missing: {token}")

    for token in (
        "## 0.8.0-alpha.3.29",
        "0.8.0.59",
        "PositionForShow()",
        "ResolveWindowOrigin()",
        "DWMWA_CLOAK",
    ):
        if token not in changelog:
            fail(f"v0.8 alpha.3.29 changelog contract missing: {token}")

    for token in (
        "v0.8.0-alpha.3.29",
        "v0.8.0-alpha.4",
    ):
        if token not in roadmap:
            fail(f"v0.8 alpha.3 roadmap missing: {token}")

    print(
        "v0.8.0-alpha.3.29 placement/presentation hardening verified:",
        "| shared top/center resolver is unit-tested",
        "| Settings create/show both use shared resolver",
        "| Manager HWND is born at final monitor/DPI/rect",
        "| Manager first show/close use DWM cloak barrier",
        "| Manager size/columns + schema9 + Editor/updater closeouts preserved",
    )
    raise SystemExit(0)


if version == "0.8.0-alpha.3.28":
    expected_schemas = {
        "kSettingsSchemaVersion": 9,
        "kCommandsSchemaVersion": 2,
        "kUsageSchemaVersion": 1,
    }
    for name, expected in expected_schemas.items():
        actual = cpp_int("src/core/ConfigIO.hpp", name)
        if actual != expected:
            fail(f"v0.8 alpha.3.28 {name}={actual}, expected {expected}")

    if cpp_int("src/core/ProviderCache.cpp", "kProviderCacheSchemaVersion") != 2:
        fail("v0.8 alpha.3.28 must keep provider-cache schemaVersion 2")

    settings_h = read("src/core/Settings.hpp")
    settings_cpp = read("src/core/Settings.cpp")
    app_h = read("src/app/App.hpp")
    app_cpp = read("src/app/App.cpp")
    settings_window_h = read("src/ui/SettingsWindow.hpp")
    settings_window_cpp = read("src/ui/SettingsWindow.cpp")
    settings_layout_cpp = read("src/core/SettingsLayout.cpp")
    manager_h = read("src/ui/ShortcutManagerWindow.hpp")
    manager_cpp = read("src/ui/ShortcutManagerWindow.cpp")
    editor_cpp = read("src/ui/ShortcutEditorDialog.cpp")

    for token in (
        'std::string shortcutManagerPlacement{"center"}',
        "shortcutManagerLastPositionValid",
        "shortcutManagerLastX",
        "shortcutManagerLastY",
        "RememberShortcutManagerPosition(",
    ):
        if token not in settings_h:
            fail(f"v0.8 alpha.3.28 Settings model missing: {token}")

    for token in (
        "NormalizeShortcutManagerPlacement(",
        '"shortcutManagerMode"',
        '"shortcutManagerLastValid"',
        '"shortcutManagerLastX"',
        '"shortcutManagerLastY"',
        "RememberShortcutManagerPosition(",
        'value == "top"',
    ):
        if token not in settings_cpp:
            fail(f"v0.8 alpha.3.28 Settings persistence missing: {token}")

    for token in (
        "kIdShortcutManagerPlacement = 51109",
        "shortcutManagerPlacementLabel_",
        "shortcutManagerPlacementDescription_",
        "shortcutManagerPlacement_",
    ):
        if token not in settings_window_h:
            fail(f"v0.8 alpha.3.28 Settings UI declaration missing: {token}")

    for token in (
        'T(L"启动器显示器"',
        'T(L"启动器窗口位置"',
        'T(L"设置窗口位置"',
        'T(L"快捷项管理窗口位置"',
        'T(L"靠近屏幕顶部"',
        "std::pair<HWND, HWND>,\n            4>",
        "kIdShortcutManagerPlacement",
        "shortcutManagerPlacementIndex",
        "placementMode(",
        'settings.settingsPlacement ==\n            "top"',
    ):
        if token not in settings_window_cpp:
            fail(f"v0.8 alpha.3.28 unified placement UI missing: {token}")

    if "ui::kSettingsComboRowLogical) *\n            4" not in settings_layout_cpp:
        fail("v0.8 alpha.3.28 General placement card must reserve four rows")

    for token in (
        "ApplyConfiguredPlacement()",
        "shortcutManagerPlacement",
        "shortcutManagerLastPositionValid",
        "RememberShortcutManagerPosition(",
        "720 x 480 logical baseline",
        "Scale(kDefaultWidthLogical)",
        "Scale(kDefaultHeightLogical)",
        '==\n                "top"',
    ):
        if token not in manager_cpp:
            fail(f"v0.8 alpha.3.28 Manager placement lifecycle missing: {token}")

    for forbidden in (
        "savedWindowPlacement_",
        "savedWindowPlacementValid_",
        "CaptureWindowState()",
        "SetWindowPlacement(\n            hwnd_",
    ):
        if forbidden in manager_cpp or forbidden in manager_h:
            fail(f"v0.8 alpha.3.28 Manager must not restore previous window size/state: {forbidden}")

    for token in (
        "SetWindowPlacementSettings(",
        "RememberShortcutManagerPosition(",
    ):
        if token not in app_h or token not in app_cpp:
            fail(f"v0.8 alpha.3.28 App placement bridge missing: {token}")

    config_tests = read("tests/ConfigCoreTests.cpp")
    for token in (
        "settings-schema8-window-placement.json",
        "MigratedFromSchemaVersion() ==\n        8",
        '"shortcutManagerMode"',
        '"center"',
        "shortcutManagerLastValid",
    ):
        if token not in config_tests:
            fail(f"v0.8 alpha.3.28 schema-8 migration coverage missing: {token}")

    manager_tokens = (
        "kKeywordColumnPercent = 16",
        "kNameColumnPercent = 24",
        "kTypeColumnPercent = 14",
        "kKeywordColumnMinimumLogical = 107",
        "kNameColumnMinimumLogical = 161",
        "kTypeColumnMinimumLogical = 94",
        "kTargetColumnMinimumLogical = 180",
        "ApplyLanguage() changes only HDI_TEXT",
    )
    for token in manager_tokens:
        if token not in manager_cpp:
            fail(f"v0.8 alpha.3.28 must preserve Manager column closeout: {token}")

    for token in (
        "Deliberately do not draw ODS_FOCUS here",
        "BCM_GETIDEALSIZE",
        "SM_CXMENUCHECK",
        "kEditorWidthLogical = 590",
        "kAdvancedRightInsetLogical = 18",
    ):
        if token not in editor_cpp:
            fail(f"v0.8 alpha.3.28 must preserve Editor closeout: {token}")

    update_cpp = read("src/platform/UpdateManager.cpp")
    for token in (
        "std::stop_callback",
        "CheckTimedOut",
        "kUpdateCheckWatchdogMs",
        "60ULL * 1000ULL",
        "检查更新超时，请重试",
    ):
        if token not in update_cpp and token not in app_cpp and token not in settings_window_cpp:
            fail(f"v0.8 alpha.3.28 must preserve updater hardening: {token}")

    if "WinHttpQueryDataAvailable(" in update_cpp:
        fail("v0.8 alpha.3.28 must not regress to QueryDataAvailable update reads")

    update_tests = read("tests/UpdatePolicyTests.cpp")
    for token in (
        '"0.8.0-alpha.3.27"',
        '"0.8.0-alpha.3.28"',
        "UpdateChannel::Stable",
    ):
        if token not in update_tests:
            fail(f"v0.8 alpha.3.28 update ordering/default coverage missing: {token}")

    runtime_smoke = read("scripts/verify_runtime_smoke.ps1")
    for token in (
        "$migratedSettings.schemaVersion -ne 9",
        "shortcutManagerMode",
        "shortcutManagerLastValid",
        "launcher=top/settings=center/shortcutManager=center",
    ):
        if token not in runtime_smoke:
            fail(f"v0.8 alpha.3.28 runtime smoke schema-9 coverage missing: {token}")

    example = read("config/settings.example.json")
    docs = read("docs/CONFIG_SCHEMA.md")
    readme = read("README.md")
    changelog = read("CHANGELOG.md")
    roadmap = read("ROADMAP.md")

    for token in (
        '"schemaVersion": 9',
        '"shortcutManagerMode": "center"',
        '"shortcutManagerLastValid": false',
    ):
        if token not in example:
            fail(f"v0.8 alpha.3.28 settings example missing: {token}")

    for token in (
        "settings.json       schemaVersion 9",
        "v0.8.0-alpha.3.28 upgrades **settings.json to schemaVersion 9**",
        "shortcutManagerMode",
        "720 × 480 logical default",
    ):
        if token not in docs:
            fail(f"v0.8 alpha.3.28 config schema docs missing: {token}")

    for token in (
        "## v0.8.0-alpha.3.28 — Unified Window Placement",
        "0.8.0.58",
        "720×480 logical",
        "settings.json advances from schemaVersion 8 to schemaVersion 9",
    ):
        if token not in readme:
            fail(f"v0.8 alpha.3.28 README contract missing: {token}")

    for token in (
        "## 0.8.0-alpha.3.28",
        "0.8.0.58",
        "schemaVersion 9",
        "启动器显示器 / 启动器窗口位置 / 设置窗口位置 / 快捷项管理窗口位置",
    ):
        if token not in changelog:
            fail(f"v0.8 alpha.3.28 changelog contract missing: {token}")

    for token in (
        "v0.8.0-alpha.3.28",
        "v0.8.0-alpha.4",
    ):
        if token not in roadmap:
            fail(f"v0.8 alpha.3 roadmap missing: {token}")

    print(
        "v0.8.0-alpha.3.28 unified window placement verified:",
        "| settings schema=9",
        "| Launcher/Settings/Manager modes=top|center|last",
        "| Manager size resets to 720x480 logical on every recreate",
        "| Manager last mode persists X/Y only",
        "| placement copy/layout unified",
        "| Manager columns + Editor + updater hardening preserved",
    )
    raise SystemExit(0)


if version == "0.8.0-alpha.3.27":
    expected_schemas = {
        "kSettingsSchemaVersion": 8,
        "kCommandsSchemaVersion": 2,
        "kUsageSchemaVersion": 1,
    }
    for name, expected in expected_schemas.items():
        actual = cpp_int("src/core/ConfigIO.hpp", name)
        if actual != expected:
            fail(f"v0.8 alpha.3.27 {name}={actual}, expected {expected}")

    if cpp_int("src/core/ProviderCache.cpp", "kProviderCacheSchemaVersion") != 2:
        fail("v0.8 alpha.3.27 must keep provider-cache schemaVersion 2")

    manager_cpp = read("src/ui/ShortcutManagerWindow.cpp")
    manager_h = read("src/ui/ShortcutManagerWindow.hpp")
    editor_cpp = read("src/ui/ShortcutEditorDialog.cpp")

    for token in (
        "kKeywordColumnPercent = 16",
        "kNameColumnPercent = 24",
        "kTypeColumnPercent = 14",
        "kKeywordColumnMinimumLogical = 107",
        "kNameColumnMinimumLogical = 161",
        "kTypeColumnMinimumLogical = 94",
        "kTargetColumnMinimumLogical = 180",
        "Column widths are intentionally session-local",
        "A recreated Manager always starts from the validated default column",
        "ApplyLanguage() changes only HDI_TEXT",
        "(header->pitem->mask &\n         HDI_WIDTH) != 0",
        "customColumnWidths_",
        "ClampTrackedColumnWidth(",
        "UpdateColumnWidths(",
    ):
        if token not in manager_cpp:
            fail(f"v0.8 alpha.3.27 Manager column lifecycle missing: {token}")

    if "savedColumnWidths" in manager_cpp or "savedColumnWidths" in manager_h:
        fail("v0.8 alpha.3.27 must not persist temporary Manager column widths across reopen")

    for token in (
        "Deliberately do not draw ODS_FOCUS here",
        "BCM_GETIDEALSIZE",
        "SM_CXMENUCHECK",
        "kEditorWidthLogical = 590",
        "kAdvancedRightInsetLogical = 18",
        "ToggleAdvanced();",
    ):
        if token not in editor_cpp:
            fail(f"v0.8 alpha.3.27 must preserve alpha.3.26 Editor interaction repair: {token}")

    update_cpp = read("src/platform/UpdateManager.cpp")
    app_cpp = read("src/app/App.cpp")
    settings_cpp = read("src/ui/SettingsWindow.cpp")
    for token in (
        "std::stop_callback",
        "CheckTimedOut",
        "kUpdateCheckWatchdogMs",
        "60ULL * 1000ULL",
        "检查更新超时，请重试",
    ):
        if token not in update_cpp and token not in app_cpp and token not in settings_cpp:
            fail(f"v0.8 alpha.3.27 must preserve alpha.3.23 update hardening: {token}")

    if "WinHttpQueryDataAvailable(" in update_cpp:
        fail("v0.8 alpha.3.27 must not regress to QueryDataAvailable update reads")

    update_tests = read("tests/UpdatePolicyTests.cpp")
    for token in (
        '"0.8.0-alpha.3.26"',
        '"0.8.0-alpha.3.27"',
        "UpdateChannel::Stable",
    ):
        if token not in update_tests:
            fail(f"v0.8 alpha.3.27 update ordering/default coverage missing: {token}")

    readme = read("README.md")
    changelog = read("CHANGELOG.md")
    roadmap = read("ROADMAP.md")

    for token in (
        "## v0.8.0-alpha.3.27 — Shortcut Manager Column Lifecycle Closeout",
        "0.8.0.57",
        "16% Keywords / 24% Name / 14% Type / 46% Target",
        "107 logical pixels for Keywords, 161 for Name and 94 for Type",
        "Closing the Manager destroys that temporary column state",
    ):
        if token not in readme:
            fail(f"v0.8 alpha.3.27 README contract missing: {token}")

    for token in (
        "## 0.8.0-alpha.3.27",
        "0.8.0.57",
        "107/161/94",
        "reset",
        "saved first-three-column width cache",
    ):
        if token not in changelog:
            fail(f"v0.8 alpha.3.27 changelog contract missing: {token}")

    for token in (
        "v0.8.0-alpha.3.27",
        "v0.8.0-alpha.4",
    ):
        if token not in roadmap:
            fail(f"v0.8 alpha.3 roadmap missing: {token}")

    print(
        "v0.8.0-alpha.3.27 Shortcut Manager column lifecycle verified:",
        "| fresh open=16/24/14/46",
        "| drag minima=107/161/94/180 logical",
        "| temporary drag widths reset on Manager reopen",
        "| window placement persistence retained",
        "| alpha.3.26 Editor fixes + alpha.3.23 updater hardening preserved",
        "| schemas unchanged",
    )
    raise SystemExit(0)


if version == "0.8.0-alpha.3.26":
    expected_schemas = {
        "kSettingsSchemaVersion": 8,
        "kCommandsSchemaVersion": 2,
        "kUsageSchemaVersion": 1,
    }
    for name, expected in expected_schemas.items():
        actual = cpp_int("src/core/ConfigIO.hpp", name)
        if actual != expected:
            fail(f"v0.8 alpha.3.26 {name}={actual}, expected {expected}")

    if cpp_int("src/core/ProviderCache.cpp", "kProviderCacheSchemaVersion") != 2:
        fail("v0.8 alpha.3.26 must keep provider-cache schemaVersion 2")

    editor_cpp = read("src/ui/ShortcutEditorDialog.cpp")
    editor_h = read("src/ui/ShortcutEditorDialog.hpp")
    manager_cpp = read("src/ui/ShortcutManagerWindow.cpp")

    for token in (
        "Deliberately do not draw ODS_FOCUS here",
        "BCM_GETIDEALSIZE",
        "SM_CXMENUCHECK",
        "adminWidth",
        "kEditorWidthLogical = 590",
        "kAdvancedRightInsetLogical = 18",
        "case kIdAdvancedToggle:",
        "ToggleAdvanced();",
    ):
        if token not in editor_cpp:
            fail(f"v0.8 alpha.3.26 Editor interaction repair missing: {token}")

    if "advancedMouseActivation_" in editor_cpp or "advancedMouseActivation_" in editor_h:
        fail("v0.8 alpha.3.26 must remove alpha.3.25 Advanced mouse-origin focus state")

    for token in (
        "kKeywordColumnPercent = 16",
        "kNameColumnPercent = 24",
        "kTypeColumnPercent = 14",
        "kKeywordColumnMinimumLogical = 88",
        "kNameColumnMinimumLogical = 128",
        "kTypeColumnMinimumLogical = 88",
        "kTargetColumnMinimumLogical = 180",
        "ApplyLanguage() changes only HDI_TEXT",
        "(header->pitem->mask &\n         HDI_WIDTH) != 0",
        "customColumnWidths_",
        "ClampTrackedColumnWidth(",
        "UpdateColumnWidths(",
    ):
        if token not in manager_cpp:
            fail(f"v0.8 alpha.3.26 Manager column-state repair missing: {token}")

    for token in (
        "ParseShortcutKeywords(",
        "SuggestShortcutTitle(",
        "InferShortcutCommandType(",
        "CanAcceptRuntimeInput(",
        "HasRuntimeInputPlaceholder(",
        "RuntimeInputMode::Raw",
        "RuntimeInputMode::UrlEncoded",
        "app_.CreateUserCommand(",
        "app_.UpdateUserCommand(",
        "app_.TestCommand(",
        "RefreshDynamicLayout();",
        "RDW_ALLCHILDREN",
    ):
        if token not in editor_cpp:
            fail(f"v0.8 alpha.3.26 frozen Shortcut behavior missing: {token}")

    update_cpp = read("src/platform/UpdateManager.cpp")
    app_cpp = read("src/app/App.cpp")
    settings_cpp = read("src/ui/SettingsWindow.cpp")
    for token in (
        "std::stop_callback",
        "CheckTimedOut",
        "kUpdateCheckWatchdogMs",
        "60ULL * 1000ULL",
        "检查更新超时，请重试",
    ):
        if token not in update_cpp and token not in app_cpp and token not in settings_cpp:
            fail(f"v0.8 alpha.3.26 must preserve alpha.3.23 update hardening: {token}")

    if "WinHttpQueryDataAvailable(" in update_cpp:
        fail("v0.8 alpha.3.26 must not regress to QueryDataAvailable update reads")

    update_tests = read("tests/UpdatePolicyTests.cpp")
    for token in (
        '"0.8.0-alpha.3.25"',
        '"0.8.0-alpha.3.26"',
        "UpdateChannel::Stable",
    ):
        if token not in update_tests:
            fail(f"v0.8 alpha.3.26 update ordering/default coverage missing: {token}")

    readme = read("README.md")
    changelog = read("CHANGELOG.md")
    roadmap = read("ROADMAP.md")

    for token in (
        "## v0.8.0-alpha.3.26 — Shortcut Interaction Reliability & Column Default Repair",
        "0.8.0.56",
        "BCM_GETIDEALSIZE",
        "HDN_ITEMCHANGED",
        "16% Keywords / 24% Name / 14% Type / 46% Target",
    ):
        if token not in readme:
            fail(f"v0.8 alpha.3.26 README contract missing: {token}")

    for token in (
        "## 0.8.0-alpha.3.26",
        "0.8.0.56",
        "classic dotted focus rectangle",
        "HDI_WIDTH",
        "16/24/14/46",
    ):
        if token not in changelog:
            fail(f"v0.8 alpha.3.26 changelog contract missing: {token}")

    for token in (
        "v0.8.0-alpha.3.26",
        "v0.8.0-alpha.4",
    ):
        if token not in roadmap:
            fail(f"v0.8 alpha.3 roadmap missing: {token}")

    print(
        "v0.8.0-alpha.3.26 Shortcut interaction/column repair verified:",
        "| Advanced=native click path, no ODS_FOCUS renderer",
        "| admin checkbox=content-fit hit area",
        "| Manager text-only HDN_ITEMCHANGED no longer enters custom widths",
        "| intended 16/24/14/46 defaults preserved",
        "| alpha.3.24 geometry + alpha.3.23 updater hardening preserved",
        "| schemas/frozen shortcut behavior unchanged",
    )
    raise SystemExit(0)


if version == "0.8.0-alpha.3.25":
    expected_schemas = {
        "kSettingsSchemaVersion": 8,
        "kCommandsSchemaVersion": 2,
        "kUsageSchemaVersion": 1,
    }
    for name, expected in expected_schemas.items():
        actual = cpp_int("src/core/ConfigIO.hpp", name)
        if actual != expected:
            fail(f"v0.8 alpha.3.25 {name}={actual}, expected {expected}")

    if cpp_int("src/core/ProviderCache.cpp", "kProviderCacheSchemaVersion") != 2:
        fail("v0.8 alpha.3.25 must keep provider-cache schemaVersion 2")

    editor_cpp = read("src/ui/ShortcutEditorDialog.cpp")
    editor_h = read("src/ui/ShortcutEditorDialog.hpp")
    manager_cpp = read("src/ui/ShortcutManagerWindow.cpp")

    for token in (
        "advancedMouseActivation_{false}",
        "HIWORD(wParam) ==\n                kIdAdvancedToggle",
        "const bool mouseActivation =",
        "advancedMouseActivation_ =\n                    false;",
        "if (mouseActivation) {",
        "SetFocus(hwnd_);",
        "WM_QUERYUISTATE",
        "UISF_HIDEFOCUS",
        "kEditorWidthLogical = 590",
        "kAdvancedRightInsetLogical = 18",
    ):
        if token not in editor_cpp and token not in editor_h:
            fail(f"v0.8 alpha.3.25 Advanced focus closeout missing: {token}")

    for token in (
        "kKeywordColumnPercent = 16",
        "kNameColumnPercent = 24",
        "kTypeColumnPercent = 14",
        "kKeywordColumnMinimumLogical = 88",
        "kNameColumnMinimumLogical = 128",
        "kTypeColumnMinimumLogical = 88",
        "kTargetColumnMinimumLogical = 180",
        "contentWidth *\n            kKeywordColumnPercent / 100",
        "contentWidth *\n            kNameColumnPercent / 100",
        "contentWidth *\n            kTypeColumnPercent / 100",
        "customColumnWidths_",
        "ClampTrackedColumnWidth(",
        "UpdateColumnWidths(",
    ):
        if token not in manager_cpp:
            fail(f"v0.8 alpha.3.25 Manager column balance missing: {token}")

    for token in (
        "ParseShortcutKeywords(",
        "SuggestShortcutTitle(",
        "InferShortcutCommandType(",
        "CanAcceptRuntimeInput(",
        "HasRuntimeInputPlaceholder(",
        "RuntimeInputMode::Raw",
        "RuntimeInputMode::UrlEncoded",
        "app_.CreateUserCommand(",
        "app_.UpdateUserCommand(",
        "app_.TestCommand(",
        "RefreshDynamicLayout();",
        "RDW_ALLCHILDREN",
    ):
        if token not in editor_cpp:
            fail(f"v0.8 alpha.3.25 frozen Shortcut behavior missing: {token}")

    update_cpp = read("src/platform/UpdateManager.cpp")
    app_cpp = read("src/app/App.cpp")
    settings_cpp = read("src/ui/SettingsWindow.cpp")
    for token in (
        "std::stop_callback",
        "CheckTimedOut",
        "kUpdateCheckWatchdogMs",
        "60ULL * 1000ULL",
        "检查更新超时，请重试",
    ):
        if token not in update_cpp and token not in app_cpp and token not in settings_cpp:
            fail(f"v0.8 alpha.3.25 must preserve alpha.3.23 update hardening: {token}")

    if "WinHttpQueryDataAvailable(" in update_cpp:
        fail("v0.8 alpha.3.25 must not regress to QueryDataAvailable update reads")

    update_tests = read("tests/UpdatePolicyTests.cpp")
    for token in (
        '"0.8.0-alpha.3.24"',
        '"0.8.0-alpha.3.25"',
        "UpdateChannel::Stable",
    ):
        if token not in update_tests:
            fail(f"v0.8 alpha.3.25 update ordering/default coverage missing: {token}")

    readme = read("README.md")
    changelog = read("CHANGELOG.md")
    roadmap = read("ROADMAP.md")

    for token in (
        "## v0.8.0-alpha.3.25 — Shortcut Workflow Focus & Column Balance Closeout",
        "0.8.0.55",
        "16% Keywords / 24% Name / 14% Type / 46% Target",
        "pointer activation from keyboard activation",
        "180 logical pixels for Target",
    ):
        if token not in readme:
            fail(f"v0.8 alpha.3.25 README contract missing: {token}")

    for token in (
        "## 0.8.0-alpha.3.25",
        "0.8.0.55",
        "16/24/14/46",
        "mouse clicks",
        "180 logical pixels for Target",
    ):
        if token not in changelog:
            fail(f"v0.8 alpha.3.25 changelog contract missing: {token}")

    for token in (
        "v0.8.0-alpha.3.25",
        "v0.8.0-alpha.4",
    ):
        if token not in roadmap:
            fail(f"v0.8 alpha.3 roadmap missing: {token}")

    print(
        "v0.8.0-alpha.3.25 Shortcut workflow closeout verified:",
        "| Advanced mouse focus dismissed, keyboard focus preserved",
        "| Manager columns=16/24/14/46 responsive balance",
        "| minimums=88/128/88/180 logical",
        "| alpha.3.24 geometry + alpha.3.23 updater hardening preserved",
        "| schemas/frozen shortcut behavior unchanged",
    )
    raise SystemExit(0)


if version == "0.8.0-alpha.3.24":
    expected_schemas = {
        "kSettingsSchemaVersion": 8,
        "kCommandsSchemaVersion": 2,
        "kUsageSchemaVersion": 1,
    }
    for name, expected in expected_schemas.items():
        actual = cpp_int("src/core/ConfigIO.hpp", name)
        if actual != expected:
            fail(f"v0.8 alpha.3.24 {name}={actual}, expected {expected}")

    if cpp_int("src/core/ProviderCache.cpp", "kProviderCacheSchemaVersion") != 2:
        fail("v0.8 alpha.3.24 must keep provider-cache schemaVersion 2")

    editor_cpp = read("src/ui/ShortcutEditorDialog.cpp")
    for token in (
        "kEditorWidthLogical = 590",
        "kInlineComboMinWidthLogical = 158",
        "kInlineComboMaxWidthLogical = 185",
        "kAdvancedRightInsetLogical = 18",
        "Scale(190)",
        "* 36 / 100",
        "? 76",
        ": 108",
        "GetTextExtentPoint32W(",
        "GetSystemMetricsForDpi(",
        "Scale(28)",
        "const int runtimeWidth =\n        typeWidth;",
        "const int advancedFieldWidth =",
        "const int advancedFieldRight =",
        "WM_QUERYUISTATE",
        "UISF_HIDEFOCUS",
        "DrawFocusRect(",
        "palette.controlBackground",
        "int y = Scale(16);",
        "y += Scale(12);",
        "kControlRowHeightLogical = 28",
        "EM_SETMARGINS",
    ):
        if token not in editor_cpp:
            fail(f"v0.8 alpha.3.24 Editor density contract missing: {token}")

    if "kInlineComboWidthLogical = 190" in editor_cpp:
        fail("v0.8 alpha.3.24 must keep content-measured ComboBox widths")

    for token in (
        "ParseShortcutKeywords(",
        "SuggestShortcutTitle(",
        "InferShortcutCommandType(",
        "CanAcceptRuntimeInput(",
        "HasRuntimeInputPlaceholder(",
        "RuntimeInputMode::Raw",
        "RuntimeInputMode::UrlEncoded",
        "app_.CreateUserCommand(",
        "app_.UpdateUserCommand(",
        "app_.TestCommand(",
        "RefreshDynamicLayout();",
        "RDW_ALLCHILDREN",
    ):
        if token not in editor_cpp:
            fail(f"v0.8 alpha.3.24 frozen Shortcut behavior missing: {token}")

    update_cpp = read("src/platform/UpdateManager.cpp")
    app_cpp = read("src/app/App.cpp")
    settings_cpp = read("src/ui/SettingsWindow.cpp")
    for token in (
        "std::stop_callback",
        "CheckTimedOut",
        "kUpdateCheckWatchdogMs",
        "60ULL * 1000ULL",
        "检查更新超时，请重试",
    ):
        if token not in update_cpp and token not in app_cpp and token not in settings_cpp:
            fail(f"v0.8 alpha.3.24 must preserve alpha.3.23 update hardening: {token}")

    if "WinHttpQueryDataAvailable(" in update_cpp:
        fail("v0.8 alpha.3.24 must not regress to QueryDataAvailable update reads")

    update_tests = read("tests/UpdatePolicyTests.cpp")
    for token in (
        '"0.8.0-alpha.3.23"',
        '"0.8.0-alpha.3.24"',
        "UpdateChannel::Stable",
    ):
        if token not in update_tests:
            fail(f"v0.8 alpha.3.24 update ordering/default coverage missing: {token}")

    readme = read("README.md")
    changelog = read("CHANGELOG.md")
    roadmap = read("ROADMAP.md")

    for token in (
        "## v0.8.0-alpha.3.24 — Shortcut Editor Density Closeout",
        "0.8.0.54",
        "590 logical pixels",
        "158–185 logical pixels",
        "18-logical-pixel right inset",
        "keyboard focus cues",
    ):
        if token not in readme:
            fail(f"v0.8 alpha.3.24 README contract missing: {token}")

    for token in (
        "## 0.8.0-alpha.3.24",
        "0.8.0.54",
        "590 logical pixels",
        "158–185 logical pixels",
        "18 logical pixels",
        "keyboard-only focus",
    ):
        if token not in changelog:
            fail(f"v0.8 alpha.3.24 changelog contract missing: {token}")

    for token in (
        "v0.8.0-alpha.3.24",
        "v0.8.0-alpha.4",
    ):
        if token not in roadmap:
            fail(f"v0.8 alpha.3 roadmap missing: {token}")

    print(
        "v0.8.0-alpha.3.24 Shortcut Editor density closeout verified:",
        "| Editor=590 logical",
        "| inline labels=compact left shift",
        "| ComboBoxes=measured 158-185 logical",
        "| Advanced=18px right inset + keyboard-only focus",
        "| alpha.3.23 update hardening preserved",
        "| schemas/frozen shortcut behavior unchanged",
    )
    raise SystemExit(0)


if version == "0.8.0-alpha.3.23":
    expected_schemas = {
        "kSettingsSchemaVersion": 8,
        "kCommandsSchemaVersion": 2,
        "kUsageSchemaVersion": 1,
    }
    for name, expected in expected_schemas.items():
        actual = cpp_int("src/core/ConfigIO.hpp", name)
        if actual != expected:
            fail(f"v0.8 alpha.3.23 {name}={actual}, expected {expected}")

    if cpp_int("src/core/ProviderCache.cpp", "kProviderCacheSchemaVersion") != 2:
        fail("v0.8 alpha.3.23 must keep provider-cache schemaVersion 2")

    editor_cpp = read("src/ui/ShortcutEditorDialog.cpp")
    for token in (
        "kEditorWidthLogical = 620",
        "kInlineComboMinWidthLogical = 150",
        "kInlineComboMaxWidthLogical = 175",
        "Scale(200)",
        "* 36 / 100",
        "GetTextExtentPoint32W(",
        "GetSystemMetricsForDpi(",
        "const int runtimeWidth =\n        typeWidth;",
        "int y = Scale(16);",
        "y += Scale(12);",
        "kControlRowHeightLogical = 28",
        "EM_SETMARGINS",
    ):
        if token not in editor_cpp:
            fail(f"v0.8 alpha.3.23 compact Editor contract missing: {token}")

    if "kInlineComboWidthLogical = 190" in editor_cpp:
        fail("v0.8 alpha.3.23 must not restore the fixed 190px ComboBox width")

    update_cpp = read("src/platform/UpdateManager.cpp")
    update_h = read("src/platform/UpdateManager.hpp")
    app_cpp = read("src/app/App.cpp")
    app_h = read("src/app/App.hpp")
    settings_cpp = read("src/ui/SettingsWindow.cpp")

    for token in (
        "std::atomic<HINTERNET>",
        "std::stop_callback",
        "handles.request.Close();",
        "if (!WinHttpSetTimeouts(",
        "WinHttpReadData(",
        "IsNetworkTimeout(",
        "CheckTimedOut",
    ):
        if token not in update_cpp and token not in update_h:
            fail(f"v0.8 alpha.3.23 update transport hardening missing: {token}")

    if "WinHttpQueryDataAvailable(" in update_cpp:
        fail("v0.8 alpha.3.23 updater must use bounded direct reads instead of QueryDataAvailable")

    for token in (
        "kUpdateCheckWatchdogMs",
        "60ULL * 1000ULL",
        "updateWorkerStartedTick_",
        "GetTickCount64()",
        "++updateGeneration_;",
        "updateThread_.request_stop();",
        "beginPreparedUpdate",
    ):
        if token not in app_cpp and token not in app_h:
            fail(f"v0.8 alpha.3.23 App update-state hardening missing: {token}")

    for token in (
        "case win::UpdateFailure::CheckTimedOut:",
        "检查更新超时，请重试",
        "update check timed out; try again",
    ):
        if token not in settings_cpp:
            fail(f"v0.8 alpha.3.23 timeout UX missing: {token}")

    update_tests = read("tests/UpdatePolicyTests.cpp")
    for token in (
        '"0.8.0-alpha.3.22"',
        '"0.8.0-alpha.3.23"',
        "UpdateChannel::Stable",
    ):
        if token not in update_tests:
            fail(f"v0.8 alpha.3.23 update ordering/default coverage missing: {token}")

    readme = read("README.md")
    changelog = read("CHANGELOG.md")
    roadmap = read("ROADMAP.md")

    for token in (
        "## v0.8.0-alpha.3.23 — Shortcut Editor Compact Width & Update Check Hardening",
        "0.8.0.53",
        "GetTextExtentPoint32W",
        "60-second App watchdog",
        "WinHttpQueryDataAvailable",
    ):
        if token not in readme:
            fail(f"v0.8 alpha.3.23 README contract missing: {token}")

    for token in (
        "## 0.8.0-alpha.3.23",
        "0.8.0.53",
        "CheckTimedOut",
        "60-second App watchdog",
        "UI/worker data race",
    ):
        if token not in changelog:
            fail(f"v0.8 alpha.3.23 changelog contract missing: {token}")

    for token in (
        "v0.8.0-alpha.3.23",
        "v0.8.0-alpha.4",
    ):
        if token not in roadmap:
            fail(f"v0.8 alpha.3 roadmap missing: {token}")

    print(
        "v0.8.0-alpha.3.23 compact Editor/update hardening verified:",
        "| Editor=620 logical + measured shared ComboBox width",
        "| updater=interruptible WinHTTP direct reads",
        "| check watchdog=60s generation-safe cancellation",
        "| ReadyToInstall handoff=mutex synchronized",
        "| schemas/frozen shortcut behavior unchanged",
    )
    raise SystemExit(0)


if version == "0.8.0-alpha.3.13":
    expected_schemas = {
        "kSettingsSchemaVersion": 8,
        "kCommandsSchemaVersion": 2,
        "kUsageSchemaVersion": 1,
    }
    for name, expected in expected_schemas.items():
        actual = cpp_int("src/core/ConfigIO.hpp", name)
        if actual != expected:
            fail(f"v0.8 alpha.3.13 {name}={actual}, expected {expected}")

    if cpp_int("src/core/ProviderCache.cpp", "kProviderCacheSchemaVersion") != 2:
        fail("v0.8 alpha.3.13 must keep provider-cache schemaVersion 2")

    editor_cpp = read("src/ui/ShortcutEditorDialog.cpp")
    editor_h = read("src/ui/ShortcutEditorDialog.hpp")

    for token in (
        "#include <shobjidl.h>",
        "enum class PickerResult",
        "class ScopedComApartment",
        "CLSID_FileOpenDialog",
        "FOS_FORCEFILESYSTEM",
        "FOS_PICKFOLDERS",
        "SIGDN_FILESYSPATH",
        "SeedShellDialogFromPath(",
        "PickFileModern(",
        "PickFolderModern(",
        'T(L"选择目标文件",',
        'T(L"选择目标文件夹",',
        'T(L"选择工作目录",',
        'T(L"选择图标来源",',
        'T(L"程序和快捷方式",',
        'T(L"图标来源",',
        "SHBrowseForFolderW(",
        "GetOpenFileNameW(",
        "BS_OWNERDRAW",
        "void ShortcutEditorDialog::DrawAdvancedHeader(",
        "case WM_DRAWITEM:",
        "kIdAdvancedToggle",
    ):
        if token not in editor_cpp and token not in editor_h:
            fail(f"v0.8 alpha.3.13 Shortcut Editor polish contract missing: {token}")

    for forbidden in (
        "kIdResetIcon",
        "resetIcon_",
        "ResetIcon()",
        "HandleButtonCustomDraw(",
        'T(L"自动",\n          L"Auto")',
    ):
        if forbidden in editor_cpp or forbidden in editor_h:
            fail(f"v0.8 alpha.3.13 obsolete Shortcut Editor UI remains: {forbidden}")

    # Behavior boundaries remain frozen after the visual/picker polish.
    for token in (
        "ParseShortcutKeywords(",
        "SuggestShortcutTitle(",
        "InferShortcutCommandType(",
        "CanAcceptRuntimeInput(",
        "HasRuntimeInputPlaceholder(",
        "RuntimeInputMode::Raw",
        "RuntimeInputMode::UrlEncoded",
        "app_.CreateUserCommand(",
        "app_.UpdateUserCommand(",
        "app_.TestCommand(",
        "RefreshDynamicLayout();",
        "RDW_ALLCHILDREN",
    ):
        if token not in editor_cpp:
            fail(f"v0.8 alpha.3.13 frozen Shortcut behavior missing: {token}")

    update_tests = read("tests/UpdatePolicyTests.cpp")
    for token in (
        '"0.8.0-alpha.3.12"',
        '"0.8.0-alpha.3.13"',
        "UpdateChannel::Stable",
    ):
        if token not in update_tests:
            fail(f"v0.8 alpha.3.13 update ordering/default coverage missing: {token}")

    readme = read("README.md")
    changelog = read("CHANGELOG.md")
    roadmap = read("ROADMAP.md")

    for token in (
        "## v0.8.0-alpha.3.13 — Shortcut Editor Native Polish & Modern Pickers",
        "0.8.0.43",
        "IFileOpenDialog",
        "FOS_PICKFOLDERS",
        "Auto** button is removed",
        "owner-drawn section header",
    ):
        if token not in readme:
            fail(f"v0.8 alpha.3.13 README contract missing: {token}")

    for token in (
        "## 0.8.0-alpha.3.13",
        "0.8.0.43",
        "redundant Icon **Auto** button",
        "`IFileOpenDialog`",
        "`FOS_PICKFOLDERS`",
        "owner-drawn lightweight section header",
    ):
        if token not in changelog:
            fail(f"v0.8 alpha.3.13 changelog contract missing: {token}")

    for token in (
        "v0.8.0-alpha.3.13",
        "v0.8.0-alpha.3.14",
    ):
        if token not in roadmap:
            fail(f"v0.8 alpha.3 roadmap missing: {token}")

    print(
        "v0.8.0-alpha.3.13 Shortcut Editor native polish verified:",
        "| Icon Auto redundancy removed",
        "| file/folder/icon/workdir prefer IFileOpenDialog",
        "| modern folder picker uses FOS_PICKFOLDERS + legacy fallback",
        "| Advanced=owner-drawn section header",
        "| shortcut/runtime behavior frozen",
    )
    raise SystemExit(0)

if version == "0.8.0-alpha.3.12":
    expected_schemas = {
        "kSettingsSchemaVersion": 8,
        "kCommandsSchemaVersion": 2,
        "kUsageSchemaVersion": 1,
    }
    for name, expected in expected_schemas.items():
        actual = cpp_int("src/core/ConfigIO.hpp", name)
        if actual != expected:
            fail(f"v0.8 alpha.3.12 {name}={actual}, expected {expected}")

    if cpp_int("src/core/ProviderCache.cpp", "kProviderCacheSchemaVersion") != 2:
        fail("v0.8 alpha.3.12 must keep provider-cache schemaVersion 2")

    editor_cpp = read("src/ui/ShortcutEditorDialog.cpp")
    editor_h = read("src/ui/ShortcutEditorDialog.hpp")

    for token in (
        '#include "UiTheme.hpp"',
        "constexpr int kEditorWidthLogical = 720;",
        "constexpr int kCollapsedHeightLogical = 472;",
        "constexpr int kExpandedHeightLogical = 648;",
        "constexpr int kRuntimeTestExtraHeightLogical = 52;",
        "WS_BORDER |\n                    ES_AUTOHSCROLL",
        "ui::UiFontRole::BodySemibold",
        "ui::kApplicationPalette",
        "Name and Keywords share the first row",
        "runtimeSeparatorY_ = y;",
        "footerSeparatorY_ =",
        "void ShortcutEditorDialog::DrawEditorChrome(",
        "HandleButtonCustomDraw(",
        "control == save_",
        "palette.accent",
        "control == keywordHint_ ||",
        "RefreshDynamicLayout();",
        "RDW_ALLCHILDREN",
    ):
        if token not in editor_cpp and token not in editor_h:
            fail(f"v0.8 alpha.3.12 Shortcut Editor visual contract missing: {token}")

    if "WS_EX_CLIENTEDGE" in editor_cpp:
        fail("v0.8 alpha.3.12 Shortcut Editor must keep the consolidated flat native Edit presentation")

    for token in (
        'T(L"工作目录",',
        'T(L"留空时自动使用目标所在目录",',
        'T(L"图标",',
        'T(L"留空时自动跟随目标",',
        'T(L"多个快捷词用逗号分隔，第一个优先级最高。",',
    ):
        if token not in editor_cpp:
            fail(f"v0.8 alpha.3.12 Shortcut Editor labeling contract missing: {token}")

    # Behavior boundaries: alpha.3.12 is presentation-only.
    for token in (
        "ParseShortcutKeywords(",
        "SuggestShortcutTitle(",
        "InferShortcutCommandType(",
        "CanAcceptRuntimeInput(",
        "HasRuntimeInputPlaceholder(",
        "RuntimeInputMode::Raw",
        "RuntimeInputMode::UrlEncoded",
        "app_.CreateUserCommand(",
        "app_.UpdateUserCommand(",
        "app_.TestCommand(",
        "BrowseTargetFile()",
        "BrowseTargetFolder()",
        "BrowseWorkingDirectory()",
        "BrowseIcon()",
    ):
        if token not in editor_cpp:
            fail(f"v0.8 alpha.3.12 frozen Shortcut Editor behavior missing: {token}")

    update_tests = read("tests/UpdatePolicyTests.cpp")
    for token in (
        '"0.8.0-alpha.3.11"',
        '"0.8.0-alpha.3.12"',
        "UpdateChannel::Stable",
    ):
        if token not in update_tests:
            fail(f"v0.8 alpha.3.12 update ordering/default coverage missing: {token}")

    readme = read("README.md")
    changelog = read("CHANGELOG.md")
    roadmap = read("ROADMAP.md")

    for token in (
        "## v0.8.0-alpha.3.12 — Shortcut Editor Visual Consolidation",
        "0.8.0.42",
        "Name** and **快捷词 / Keywords** share the first row",
        "Advanced",
        "Save is the only accent primary action",
        "presentation-only",
    ):
        if token not in readme:
            fail(f"v0.8 alpha.3.12 README contract missing: {token}")

    for token in (
        "## 0.8.0-alpha.3.12",
        "0.8.0.42",
        "Name and Keywords now share the first row",
        "single accent primary action",
        "commands schemaVersion 2",
    ):
        if token not in changelog:
            fail(f"v0.8 alpha.3.12 changelog contract missing: {token}")

    for token in (
        "v0.8.0-alpha.3.12",
        "v0.8.0-alpha.3.13",
    ):
        if token not in roadmap:
            fail(f"v0.8 alpha.3 roadmap missing: {token}")

    print(
        "v0.8.0-alpha.3.12 Shortcut Editor visual consolidation verified:",
        "| 720px compact native form + Name/Keywords first row",
        "| Target full-width + compact type/runtime sections",
        "| Advanced collapsible header + compact native fields",
        "| palette/typography/footer + Save primary",
        "| shortcut/runtime/path behavior frozen",
    )
    raise SystemExit(0)

if version == "0.8.0-alpha.3.11":
    expected_schemas = {
        "kSettingsSchemaVersion": 8,
        "kCommandsSchemaVersion": 2,
        "kUsageSchemaVersion": 1,
    }
    for name, expected in expected_schemas.items():
        actual = cpp_int("src/core/ConfigIO.hpp", name)
        if actual != expected:
            fail(f"v0.8 alpha.3.11 {name}={actual}, expected {expected}")

    if cpp_int("src/core/ProviderCache.cpp", "kProviderCacheSchemaVersion") != 2:
        fail("v0.8 alpha.3.11 must keep provider-cache schemaVersion 2")

    settings_cpp = read("src/ui/SettingsWindow.cpp")
    app_cpp = read("src/app/App.cpp")
    app_h = read("src/app/App.hpp")

    about_start = settings_cpp.find("void SettingsWindow::ShowAbout()")
    show_start = settings_cpp.find("void SettingsWindow::Show()", about_start)
    if about_start < 0 or show_start < 0:
        fail("v0.8 alpha.3.11 ShowAbout/Show definitions were not found")
    about_body = settings_cpp[about_start:show_start]

    for token in (
        "Show();",
        "ShowPage(Page::About);",
        "if (!hwnd_ ||",
        "!IsWindow(hwnd_)",
    ):
        if token not in about_body:
            fail(f"v0.8 alpha.3.11 About show path missing: {token}")

    if about_body.find("Show();") > about_body.find("ShowPage(Page::About);"):
        fail("v0.8 alpha.3.11 About must run shared Show() before selecting Page::About")

    for token in (
        "ShowWindow(\n        hwnd_,\n        SW_HIDE);",
        "PositionForShow();\n\n        ShowWindow(\n            hwnd_,\n            SW_SHOWNORMAL);",
        "DestroyWindow(hwnd_);",
        "RememberSettingsPosition(",
    ):
        if token not in settings_cpp:
            fail(f"v0.8 alpha.3.11 Settings placement/lifecycle regression: {token}")

    for token in (
        "updateReconcileTimer_",
        "StartUpdateReconcileTimer();",
        "StopUpdateReconcileTimer();",
        "HandleUpdateStatusMessage(",
        "kUpdateStatusMessage",
    ):
        if token not in app_cpp and token not in app_h:
            fail(f"v0.8 alpha.3.11 update reconciliation regressed: {token}")

    update_tests = read("tests/UpdatePolicyTests.cpp")
    for token in (
        '"0.8.0-alpha.3.10"',
        '"0.8.0-alpha.3.11"',
        "UpdateChannel::Stable",
    ):
        if token not in update_tests:
            fail(f"v0.8 alpha.3.11 update ordering/default coverage missing: {token}")

    readme = read("README.md")
    changelog = read("CHANGELOG.md")
    roadmap = read("ROADMAP.md")

    for token in (
        "## v0.8.0-alpha.3.11 — About Entry Placement Fix",
        "0.8.0.41",
        "ShowAbout()",
        "calls the exact same `Show()` routine",
    ):
        if token not in readme:
            fail(f"v0.8 alpha.3.11 README contract missing: {token}")

    for token in (
        "## 0.8.0-alpha.3.11",
        "0.8.0.41",
        "Changed About to run `Show()` first",
        "Page::About",
    ):
        if token not in changelog:
            fail(f"v0.8 alpha.3.11 changelog contract missing: {token}")

    for token in (
        "v0.8.0-alpha.3.11",
        "v0.8.0-alpha.3.12",
        "v0.8.0-alpha.3.13",
    ):
        if token not in roadmap:
            fail(f"v0.8 alpha.3 roadmap missing: {token}")

    print(
        "v0.8.0-alpha.3.11 About placement path verified:",
        "| tray Settings/About share top-level Show lifecycle",
        "| About page switch occurs after Show()",
        "| alpha.3.10 centering/Last placement preserved",
        "| destroy-on-close and update reconciliation preserved",
    )
    raise SystemExit(0)

if version == "0.8.0-alpha.3.10":
    expected_schemas = {
        "kSettingsSchemaVersion": 8,
        "kCommandsSchemaVersion": 2,
        "kUsageSchemaVersion": 1,
    }
    for name, expected in expected_schemas.items():
        actual = cpp_int("src/core/ConfigIO.hpp", name)
        if actual != expected:
            fail(f"v0.8 alpha.3.10 {name}={actual}, expected {expected}")

    if cpp_int("src/core/ProviderCache.cpp", "kProviderCacheSchemaVersion") != 2:
        fail("v0.8 alpha.3.10 must keep provider-cache schemaVersion 2")

    settings_cpp = read("src/ui/SettingsWindow.cpp")
    app_cpp = read("src/app/App.cpp")
    app_h = read("src/app/App.hpp")

    for token in (
        "CW_USEDEFAULT,\n        CW_USEDEFAULT,",
        "ShowPage(Page::General);\n    Layout();\n\n    // This apparently redundant first ShowWindow call is intentional.",
        "ShowWindow(\n        hwnd_,\n        SW_HIDE);",
        "PositionForShow();\n\n        ShowWindow(\n            hwnd_,\n            SW_SHOWNORMAL);",
        "RememberSettingsPosition(",
        "DestroyWindow(hwnd_);",
    ):
        if token not in settings_cpp:
            fail(f"v0.8 alpha.3.10 Settings lifecycle/placement contract missing: {token}")

    forbidden = (
        "const bool useLastAnchor =",
        "creationPoint",
        "SWP_SHOWWINDOW",
        "SW_SHOWNORMAL);\n\n        PositionForShow();",
    )
    for token in forbidden:
        if token in settings_cpp:
            fail(f"v0.8 alpha.3.10 obsolete placement workaround still present: {token}")

    for token in (
        "updateReconcileTimer_",
        "StartUpdateReconcileTimer();",
        "StopUpdateReconcileTimer();",
        "HandleUpdateStatusMessage(",
        "PostThreadMessageW(",
        "kUpdateStatusMessage",
    ):
        if token not in app_cpp and token not in app_h:
            fail(f"v0.8 alpha.3.10 alpha.3.8 update reconciliation regressed: {token}")

    update_tests = read("tests/UpdatePolicyTests.cpp")
    for token in (
        '"0.8.0-alpha.3.9"',
        '"0.8.0-alpha.3.10"',
        "UpdateChannel::Stable",
    ):
        if token not in update_tests:
            fail(f"v0.8 alpha.3.10 update ordering/default coverage missing: {token}")

    readme = read("README.md")
    changelog = read("CHANGELOG.md")
    roadmap = read("ROADMAP.md")

    for token in (
        "## v0.8.0-alpha.3.10 — Settings Placement Regression Fix",
        "0.8.0.40",
        "last known-good alpha.3.6",
        "ShowWindow(hwnd_, SW_HIDE)",
        "Up to date",
    ):
        if token not in readme:
            fail(f"v0.8 alpha.3.10 README contract missing: {token}")

    for token in (
        "## 0.8.0-alpha.3.10",
        "0.8.0.40",
        "last known-good alpha.3.6",
        "ShowWindow(hwnd_, SW_HIDE)",
        "single `ShowWindow(SW_SHOWNORMAL)`",
    ):
        if token not in changelog:
            fail(f"v0.8 alpha.3.10 changelog contract missing: {token}")

    for token in (
        "v0.8.0-alpha.3.10",
        "v0.8.0-alpha.3.11",
        "v0.8.0-alpha.3.12",
    ):
        if token not in roadmap:
            fail(f"v0.8 alpha.3 roadmap missing: {token}")

    print(
        "v0.8.0-alpha.3.10 Settings placement regression contract verified:",
        "| root regression=alpha.3.7 removed hidden first ShowWindow",
        "| Create=proven CW_USEDEFAULT + SW_HIDE consumption",
        "| visible open=PositionForShow + single SW_SHOWNORMAL",
        "| destroy-on-close and update reconciliation preserved",
        "| schemas/frozen shortcut behavior unchanged",
    )
    raise SystemExit(0)

if version == "0.8.0-alpha.3.9":
    expected_schemas = {
        "kSettingsSchemaVersion": 8,
        "kCommandsSchemaVersion": 2,
        "kUsageSchemaVersion": 1,
    }
    for name, expected in expected_schemas.items():
        actual = cpp_int("src/core/ConfigIO.hpp", name)
        if actual != expected:
            fail(f"v0.8 alpha.3.9 {name}={actual}, expected {expected}")

    if cpp_int("src/core/ProviderCache.cpp", "kProviderCacheSchemaVersion") != 2:
        fail("v0.8 alpha.3.9 must keep provider-cache schemaVersion 2")

    settings_cpp = read("src/ui/SettingsWindow.cpp")
    app_cpp = read("src/app/App.cpp")
    app_h = read("src/app/App.hpp")

    for token in (
        "const bool useLastAnchor =",
        "creationPoint",
        "MonitorFromPoint(",
        "creationInfo.rcWork.left",
        "settings.settingsLastX",
        "settings.settingsLastY",
        "GetDpiForWindow(hwnd_)",
        "PositionForShow();\n\n        ShowWindow(",
        "SW_SHOWNORMAL",
        "ShowWindow(\n            hwnd_,\n            SW_SHOWNORMAL);\n\n        PositionForShow();",
        "RememberSettingsPosition(",
    ):
        if token not in settings_cpp:
            fail(f"v0.8 alpha.3.9 Settings first-show contract missing: {token}")

    if "CW_USEDEFAULT,\n        CW_USEDEFAULT," in settings_cpp:
        fail("v0.8 alpha.3.9 Settings creation must not use CW_USEDEFAULT coordinates")

    if "SWP_SHOWWINDOW" in settings_cpp:
        fail("v0.8 alpha.3.9 Settings PositionForShow must position only; Show() owns first visibility")

    for token in (
        "updateReconcileTimer_",
        "StartUpdateReconcileTimer();",
        "StopUpdateReconcileTimer();",
        "HandleUpdateStatusMessage(",
        "PostThreadMessageW(",
        "kUpdateStatusMessage",
    ):
        if token not in app_cpp and token not in app_h:
            fail(f"v0.8 alpha.3.9 alpha.3.8 update reconciliation regressed: {token}")

    update_tests = read("tests/UpdatePolicyTests.cpp")
    for token in (
        '"0.8.0-alpha.3.8"',
        '"0.8.0-alpha.3.9"',
        "UpdateChannel::Stable",
    ):
        if token not in update_tests:
            fail(f"v0.8 alpha.3.9 update ordering/default coverage missing: {token}")

    readme = read("README.md")
    changelog = read("CHANGELOG.md")
    roadmap = read("ROADMAP.md")

    for token in (
        "## v0.8.0-alpha.3.9 — Settings First-Show Placement Final Fix",
        "0.8.0.39",
        "removes `CW_USEDEFAULT`",
        "post-show correction",
        "Up to date",
    ):
        if token not in readme:
            fail(f"v0.8 alpha.3.9 README contract missing: {token}")

    for token in (
        "## 0.8.0-alpha.3.9",
        "0.8.0.39",
        "Removed `CW_USEDEFAULT`",
        "post-show position correction",
        "Up to date",
    ):
        if token not in changelog:
            fail(f"v0.8 alpha.3.9 changelog contract missing: {token}")

    for token in (
        "v0.8.0-alpha.3.9",
        "v0.8.0-alpha.3.10",
        "v0.8.0-alpha.3.11",
    ):
        if token not in roadmap:
            fail(f"v0.8 alpha.3 roadmap missing: {token}")

    print(
        "v0.8.0-alpha.3.9 Settings first-show placement verified:",
        "| creation=explicit target-monitor point, no CW_USEDEFAULT",
        "| display=pre-position + native show + post-show correction",
        "| Center/Last semantics preserved",
        "| alpha.3.8 update reconciliation preserved",
        "| schemas/frozen shortcut behavior unchanged",
    )
    raise SystemExit(0)

if version == "0.8.0-alpha.3.8":
    expected_schemas = {
        "kSettingsSchemaVersion": 8,
        "kCommandsSchemaVersion": 2,
        "kUsageSchemaVersion": 1,
    }
    for name, expected in expected_schemas.items():
        actual = cpp_int("src/core/ConfigIO.hpp", name)
        if actual != expected:
            fail(f"v0.8 alpha.3.8 {name}={actual}, expected {expected}")

    if cpp_int("src/core/ProviderCache.cpp", "kProviderCacheSchemaVersion") != 2:
        fail("v0.8 alpha.3.8 must keep provider-cache schemaVersion 2")

    settings_cpp = read("src/ui/SettingsWindow.cpp")
    app_cpp = read("src/app/App.cpp")
    app_h = read("src/app/App.hpp")

    for token in (
        "void SettingsWindow::PositionForShow()",
        "SWP_SHOWWINDOW",
        "settings.settingsPlacement ==",
        '"last"',
        "settings.settingsLastPositionValid",
        "MonitorFromPoint(",
        "MONITOR_DEFAULTTONEAREST",
        "ClampRectToWorkArea",
        "closingRect",
        "RememberSettingsPosition(",
        "else if (IsIconic(hwnd_))",
        "SW_RESTORE",
    ):
        if token not in settings_cpp:
            fail(f"v0.8 alpha.3.8 Settings placement reliability missing: {token}")

    if "ShowWindow(\n        hwnd_,\n        SW_SHOWNORMAL);" in settings_cpp:
        fail("v0.8 alpha.3.8 must not reapply SW_SHOWNORMAL after first-show positioning")

    for token in (
        "void StartUpdateReconcileTimer();",
        "void StopUpdateReconcileTimer();",
        "updateReconcileTimer_",
        "void App::StartUpdateReconcileTimer()",
        "void App::StopUpdateReconcileTimer()",
        "SetTimer(\n            nullptr,\n            0,\n            250,",
        "HandleUpdateStatusMessage(\n                updateGeneration_.load());",
        "StartUpdateReconcileTimer();",
        "StopUpdateReconcileTimer();",
        "const bool workerRunning =\n        updateWorkerRunning_.load();",
        "!status.running &&\n        !workerRunning",
        "PostThreadMessageW(",
        "kUpdateStatusMessage",
    ):
        if token not in app_cpp and token not in app_h:
            fail(f"v0.8 alpha.3.8 update reconciliation contract missing: {token}")

    update_tests = read("tests/UpdatePolicyTests.cpp")
    for token in (
        '"0.8.0-alpha.3.7"',
        '"0.8.0-alpha.3.8"',
        "UpdateChannel::Stable",
    ):
        if token not in update_tests:
            fail(f"v0.8 alpha.3.8 update ordering/default coverage missing: {token}")

    readme = read("README.md")
    changelog = read("CHANGELOG.md")
    roadmap = read("ROADMAP.md")

    for token in (
        "## v0.8.0-alpha.3.8 — Settings Placement & Update Status Reliability",
        "0.8.0.38",
        "SWP_SHOWWINDOW",
        "250 ms UI-thread reconciliation timer",
        "正在检查更新",
    ):
        if token not in readme:
            fail(f"v0.8 alpha.3.8 README contract missing: {token}")

    for token in (
        "## 0.8.0-alpha.3.8",
        "0.8.0.38",
        "SetWindowPos(... SWP_SHOWWINDOW)",
        "250 ms update reconciliation timer",
        "Checking for updates",
    ):
        if token not in changelog:
            fail(f"v0.8 alpha.3.8 changelog contract missing: {token}")

    for token in (
        "v0.8.0-alpha.3.8",
        "v0.8.0-alpha.3.9",
        "v0.8.0-alpha.3.10",
    ):
        if token not in roadmap:
            fail(f"v0.8 alpha.3 roadmap missing: {token}")

    print(
        "v0.8.0-alpha.3.8 Settings/update reliability verified:",
        "| Settings first show=atomic SWP_SHOWWINDOW",
        "| close=remember actual final position",
        "| update worker=posted messages + active-only 250ms reconciliation",
        "| watchdog=terminal-state cleanup",
        "| schemas/frozen shortcut behavior unchanged",
    )
    raise SystemExit(0)


if version == "0.8.0-alpha.3.7":
    expected_schemas = {
        "kSettingsSchemaVersion": 8,
        "kCommandsSchemaVersion": 2,
        "kUsageSchemaVersion": 1,
    }
    for name, expected in expected_schemas.items():
        actual = cpp_int("src/core/ConfigIO.hpp", name)
        if actual != expected:
            fail(f"v0.8 alpha.3.7 {name}={actual}, expected {expected}")

    if cpp_int("src/core/ProviderCache.cpp", "kProviderCacheSchemaVersion") != 2:
        fail("v0.8 alpha.3.7 must keep provider-cache schemaVersion 2")

    settings_cpp = read("src/ui/SettingsWindow.cpp")
    settings_h = read("src/ui/SettingsWindow.hpp")
    manager_cpp = read("src/ui/ShortcutManagerWindow.cpp")
    manager_h = read("src/ui/ShortcutManagerWindow.hpp")
    editor_cpp = read("src/ui/ShortcutEditorDialog.cpp")
    converter_cpp = read("src/ui/ShortcutPathConverterDialog.cpp")

    for token in (
        "bool EnsureCreated();",
        "void ResetWindowInstanceState();",
        "void ReleaseWindowResources();",
        "DestroyWindow(hwnd_);",
        "ShowPage(Page::General);",
        "case WM_NCDESTROY:",
        "ResetWindowInstanceState();\n        ReleaseWindowResources();",
    ):
        if token not in settings_cpp and token not in settings_h:
            fail(f"v0.8 alpha.3.7 Settings lifecycle contract missing: {token}")

    if "case WM_CLOSE:\n        ShowWindow(" in settings_cpp:
        fail("v0.8 alpha.3.7 Settings must destroy, not hide, on close")

    for token in (
        "void CaptureWindowState();",
        "void ReleaseWindowResources();",
        "void CloseWindow();",
        "savedWindowPlacement_",
        "savedColumnWidthsLogical_",
        "savedColumnWidthsValid_",
        "CaptureWindowState();\n    DestroyWindow(hwnd_);",
        "case WM_NCDESTROY:",
        "CloseWindow();",
        "ResetTransientState",
        'T(L"确定要删除“",',
        'T(L"”吗？\\n\\n删除后无法撤销。",',
        'L"\\\"?\\n\\nThis action cannot be undone."',
    ):
        if token not in manager_cpp and token not in manager_h:
            fail(f"v0.8 alpha.3.7 Manager lifecycle/confirmation contract missing: {token}")

    if "commands.json immediately" in manager_cpp or "立即写入 commands.json" in manager_cpp:
        fail("v0.8 alpha.3.7 delete confirmation leaked commands.json implementation details")

    if "case WM_CLOSE:\n        ShowWindow(hwnd_, SW_HIDE);" in manager_cpp:
        fail("v0.8 alpha.3.7 Shortcut Manager must destroy, not hide, on close")

    for token in (
        "case WM_CLOSE:\n        DestroyWindow(hwnd_);",
        "~ShortcutEditorDialog()",
    ):
        if token not in editor_cpp:
            fail(f"v0.8 alpha.3.7 Shortcut Editor destroy lifecycle missing: {token}")

    for token in (
        "case WM_CLOSE:\n        DestroyWindow(hwnd_);",
        "~ShortcutPathConverterDialog()",
    ):
        if token not in converter_cpp:
            fail(f"v0.8 alpha.3.7 Path Conversion destroy lifecycle missing: {token}")

    update_tests = read("tests/UpdatePolicyTests.cpp")
    for token in (
        '"0.8.0-alpha.3.6"',
        '"0.8.0-alpha.3.7"',
        "UpdateChannel::Stable",
    ):
        if token not in update_tests:
            fail(f"v0.8 alpha.3.7 update ordering/default coverage missing: {token}")

    readme = read("README.md")
    changelog = read("CHANGELOG.md")
    roadmap = read("ROADMAP.md")

    for token in (
        "## v0.8.0-alpha.3.7 — Window Lifecycle & Confirmation Polish",
        "0.8.0.37",
        "returns to **常规 / General**",
        "destroy their HWNDs on close",
        "commands.json",
    ):
        if token not in readme:
            fail(f"v0.8 alpha.3.7 README contract missing: {token}")

    for token in (
        "## 0.8.0-alpha.3.7",
        "0.8.0.37",
        "destroying it and recreating it on demand",
        "删除后无法撤销",
        "user-adjusted first-three-column widths",
    ):
        if token not in changelog:
            fail(f"v0.8 alpha.3.7 changelog contract missing: {token}")

    for token in (
        "v0.8.0-alpha.3.7",
        "v0.8.0-alpha.3.8",
        "v0.8.0-alpha.3.9",
    ):
        if token not in roadmap:
            fail(f"v0.8 alpha.3 roadmap missing: {token}")

    print(
        "v0.8.0-alpha.3.7 Window lifecycle polish verified:",
        "| settings close=destroy/recreate + General reset",
        "| Manager close=destroy/recreate + geometry/column preservation",
        "| Editor/Path Conversion=already short-lived",
        "| delete confirmation=no commands.json implementation detail",
        "| frozen shortcut/runtime behavior preserved",
    )
    raise SystemExit(0)


if version == "0.8.0-alpha.3.6":
    expected_schemas = {
        "kSettingsSchemaVersion": 8,
        "kCommandsSchemaVersion": 2,
        "kUsageSchemaVersion": 1,
    }
    for name, expected in expected_schemas.items():
        actual = cpp_int("src/core/ConfigIO.hpp", name)
        if actual != expected:
            fail(f"v0.8 alpha.3.6 {name}={actual}, expected {expected}")

    if cpp_int("src/core/ProviderCache.cpp", "kProviderCacheSchemaVersion") != 2:
        fail("v0.8 alpha.3.6 must keep provider-cache schemaVersion 2")

    manager_cpp = read("src/ui/ShortcutManagerWindow.cpp")
    editor_cpp = read("src/ui/ShortcutEditorDialog.cpp")
    editor_h = read("src/ui/ShortcutEditorDialog.hpp")

    for token in (
        'T(L"新建快捷项…",',
        'T(L"编辑…",',
        'T(L"测试",',
        'T(L"打开所在目录",',
        'T(L"复制目标",',
        'T(L"删除",',
        "SetMenuDefaultItem(\n            menu,\n            kShortcutContextEdit,",
        "kShortcutContextLocate",
        "kShortcutContextCopy",
        "kShortcutContextDelete",
    ):
        if token not in manager_cpp:
            fail(f"v0.8 alpha.3.6 context-menu contract missing: {token}")

    for token in (
        'T(L"编辑快捷项...",',
        'T(L"在资源管理器中定位",',
    ):
        if token in manager_cpp:
            fail(f"v0.8 alpha.3.6 obsolete context-menu wording returned: {token}")

    for token in (
        "void UpdateWindowTitle();",
        "void ShortcutEditorDialog::\nUpdateWindowTitle()",
        'T(L"新建快捷项",',
        'T(L"编辑快捷项",',
        "commandId_ = it->id;\n    UpdateWindowTitle();",
        "commandId_.clear();\n    UpdateWindowTitle();",
        "void ShortcutEditorDialog::ApplyLanguage() {\n    UpdateWindowTitle();",
    ):
        if token not in editor_cpp and token not in editor_h:
            fail(f"v0.8 alpha.3.6 editor mode-title contract missing: {token}")

    if "ApplyLanguage();\n\n    if (commandId.empty())" not in editor_cpp:
        fail("v0.8 alpha.3.6 editor initialization baseline changed unexpectedly")

    for token in (
        "RuntimeInputMode",
        "UpdateAdvancedVisibility",
        "ShortcutPathConverterDialog",
    ):
        combined = editor_cpp + manager_cpp
        if token not in combined:
            fail(f"v0.8 alpha.3.6 frozen shortcut workflow token missing: {token}")

    update_tests = read("tests/UpdatePolicyTests.cpp")
    for token in (
        '"0.8.0-alpha.3.5"',
        '"0.8.0-alpha.3.6"',
        "UpdateChannel::Stable",
    ):
        if token not in update_tests:
            fail(f"v0.8 alpha.3.6 update ordering/default coverage missing: {token}")

    readme = read("README.md")
    changelog = read("CHANGELOG.md")
    roadmap = read("ROADMAP.md")

    for token in (
        "## v0.8.0-alpha.3.6 — Shortcut Workflow Naming Polish",
        "0.8.0.36",
        "编辑… / Edit...",
        "打开所在目录 / Open containing folder",
        "mode-title",
    ):
        if token not in readme:
            fail(f"v0.8 alpha.3.6 README contract missing: {token}")

    for token in (
        "## 0.8.0-alpha.3.6",
        "0.8.0.36",
        "ApplyLanguage previously ran before LoadCommand",
        "新建快捷项 / New shortcut",
        "编辑快捷项 / Edit shortcut",
    ):
        if token not in changelog:
            fail(f"v0.8 alpha.3.6 changelog contract missing: {token}")

    for token in (
        "v0.8.0-alpha.3.6",
        "v0.8.0-alpha.3.7",
        "v0.8.0-alpha.3.8",
    ):
        if token not in roadmap:
            fail(f"v0.8 alpha.3 roadmap missing: {token}")

    print(
        "v0.8.0-alpha.3.6 Shortcut workflow naming polish verified:",
        "| settings=8 commands=2 usage=1 provider-cache=2",
        "| context menu=Edit/Test/Open containing folder/Copy target/Delete",
        "| blank context=New shortcut only",
        "| editor title=explicit New/Edit mode",
        "| Manager layout/interactions unchanged",
        "| Editor form/Runtime Input/Advanced/Path Conversion unchanged",
    )
    raise SystemExit(0)

if version == "0.8.0-alpha.3.5":
    expected_schemas = {
        "kSettingsSchemaVersion": 8,
        "kCommandsSchemaVersion": 2,
        "kUsageSchemaVersion": 1,
    }
    for name, expected in expected_schemas.items():
        actual = cpp_int("src/core/ConfigIO.hpp", name)
        if actual != expected:
            fail(f"v0.8 alpha.3.5 {name}={actual}, expected {expected}")

    if cpp_int("src/core/ProviderCache.cpp", "kProviderCacheSchemaVersion") != 2:
        fail("v0.8 alpha.3.5 must keep provider-cache schemaVersion 2")

    manager_cpp = read("src/ui/ShortcutManagerWindow.cpp")
    manager_h = read("src/ui/ShortcutManagerWindow.hpp")

    for token in (
        "CenterOnCursorMonitor",
        "GetCursorPos",
        "MonitorFromPoint",
        "MONITOR_DEFAULTTONEAREST",
        "GetMonitorInfoW",
        "info.rcWork",
        "ClampTrackedColumnWidth",
        "columnTracking_",
        "trackedColumn_",
        "trackedColumnWidth_",
        "HDN_BEGINTRACKA",
        "HDN_BEGINTRACKW",
        "HDN_TRACKA",
        "HDN_TRACKW",
        "HDN_ENDTRACKA",
        "HDN_ENDTRACKW",
        "header->pitem->cxy =",
        "Reject the Header's final native resize",
        "currentTarget",
        "target < currentTarget",
        "target >= currentTarget",
        "Scale(72)",
        "Scale(96)",
        "Scale(120)",
        "Scale(24)",
        "ResetTransientState",
        "EM_GETRECT",
        "BeginDeferWindowPos(7)",
        "SWP_NOCOPYBITS",
    ):
        if token not in manager_cpp and token not in manager_h:
            fail(f"v0.8 alpha.3.5 Manager interaction polish missing: {token}")

    if "style |\n                HDS_FULLDRAG" in manager_cpp:
        fail("v0.8 alpha.3.5 must not enable HDS_FULLDRAG")

    track_start = manager_cpp.find("if (track &&")
    track_end = manager_cpp.find("\n    if (itemChanging &&", track_start)
    if track_start < 0 or track_end < 0:
        fail("v0.8 alpha.3.5 HDN_TRACK block could not be inspected")
    if "UpdateColumnWidths(" in manager_cpp[track_start:track_end]:
        fail("v0.8 alpha.3.5 HDN_TRACK must not resize ListView columns live")

    for token in (
        "selected = 0;",
        "Scale(30)",
        "EM_SETCUEBANNER",
        "LVS_EX_GRIDLINES",
        "kIdClose",
        "close_",
    ):
        if token in manager_cpp or token in manager_h:
            fail(f"v0.8 alpha.3.5 obsolete Manager behavior returned: {token}")

    if manager_cpp.count("ListView_InsertColumn(") != 1:
        fail("v0.8 alpha.3.5 must keep exactly the four-column creation helper")

    editor_cpp = read("src/ui/ShortcutEditorDialog.cpp")
    path_converter_cpp = read("src/ui/ShortcutPathConverterDialog.cpp")
    if "RuntimeInputMode" not in editor_cpp or "Advanced" not in editor_cpp:
        fail("v0.8 alpha.3.5 must not rewrite Shortcut Editor behavior")
    if "ShortcutPathConverterDialog::Show" not in path_converter_cpp:
        fail("v0.8 alpha.3.5 must keep Path Conversion implementation")

    update_tests = read("tests/UpdatePolicyTests.cpp")
    for token in (
        '"0.8.0-alpha.3.4"',
        '"0.8.0-alpha.3.5"',
        "UpdateChannel::Stable",
    ):
        if token not in update_tests:
            fail(f"v0.8 alpha.3.5 update ordering/default coverage missing: {token}")

    readme = read("README.md")
    changelog = read("CHANGELOG.md")
    roadmap = read("ROADMAP.md")

    for token in (
        "## v0.8.0-alpha.3.5 — Shortcut Manager Interaction Polish",
        "0.8.0.35",
        "tracking guide",
        "current mouse pointer",
    ):
        if token not in readme:
            fail(f"v0.8 alpha.3.5 README contract missing: {token}")

    for token in (
        "## 0.8.0-alpha.3.5",
        "0.8.0.35",
        "Removed HDS_FULLDRAG",
        "horizontal overflow",
        "current mouse monitor",
    ):
        if token not in changelog:
            fail(f"v0.8 alpha.3.5 changelog contract missing: {token}")

    for token in (
        "v0.8.0-alpha.3.5",
        "v0.8.0-alpha.3.6",
        "v0.8.0-alpha.3.7",
    ):
        if token not in roadmap:
            fail(f"v0.8 alpha.3 roadmap missing: {token}")

    print(
        "v0.8.0-alpha.3.5 Shortcut Manager interaction polish verified:",
        "| settings=8 commands=2 usage=1 provider-cache=2",
        "| Header=guide-only drag/no live ListView resize",
        "| commit=overflow-safe one-shot",
        "| first-open=current mouse monitor work-area center",
        "| reopen/search/24px rows preserved",
        "| Editor/Path Conversion untouched",
    )
    raise SystemExit(0)

if version == "0.8.0-alpha.3.4":
    expected_schemas = {
        "kSettingsSchemaVersion": 8,
        "kCommandsSchemaVersion": 2,
        "kUsageSchemaVersion": 1,
    }
    for name, expected in expected_schemas.items():
        actual = cpp_int("src/core/ConfigIO.hpp", name)
        if actual != expected:
            fail(f"v0.8 alpha.3.4 {name}={actual}, expected {expected}")

    if cpp_int("src/core/ProviderCache.cpp", "kProviderCacheSchemaVersion") != 2:
        fail("v0.8 alpha.3.4 must keep provider-cache schemaVersion 2")

    manager_cpp = read("src/ui/ShortcutManagerWindow.cpp")
    manager_h = read("src/ui/ShortcutManagerWindow.hpp")

    for token in (
        "HDS_FULLDRAG",
        "adjustingColumnWidths_",
        "HandleHeaderNotification",
        "HDN_ITEMCHANGINGA",
        "HDN_ITEMCHANGINGW",
        "HDN_TRACKA",
        "HDN_TRACKW",
        "HDN_ITEMCHANGEDA",
        "HDN_ITEMCHANGEDW",
        "HDN_ENDTRACKA",
        "HDN_ENDTRACKW",
        "HDN_DIVIDERDBLCLICKA",
        "HDN_DIVIDERDBLCLICKW",
        "minimumTarget",
        "Scale(72)",
        "Scale(96)",
        "Scale(120)",
        "std::clamp",
        "GetClientRect(\n            header,",
        "notification->idFrom !=",
        "UpdateColumnWidths(\n                column,\n                header->pitem->cxy)",
        "result = TRUE;",
        "Scale(24)",
        "ResetTransientState",
        "EM_GETRECT",
        "BeginDeferWindowPos(7)",
        "SWP_NOCOPYBITS",
    ):
        if token not in manager_cpp and token not in manager_h:
            fail(f"v0.8 alpha.3.4 column hardening missing: {token}")

    for token in (
        "selected = 0;",
        "Scale(30)",
        "EM_SETCUEBANNER",
        "LVS_EX_GRIDLINES",
        "kIdClose",
        "close_",
    ):
        if token in manager_cpp or token in manager_h:
            fail(f"v0.8 alpha.3.4 obsolete Manager behavior returned: {token}")

    if manager_cpp.count("ListView_InsertColumn(") != 1:
        fail("v0.8 alpha.3.4 must keep exactly the four-column creation helper")

    for token in (
        "addColumn(\n        0,",
        "addColumn(\n        1,",
        "addColumn(\n        2,",
        "addColumn(\n        3,",
    ):
        if token not in manager_cpp:
            fail(f"v0.8 alpha.3.4 four-column contract missing: {token}")

    editor_cpp = read("src/ui/ShortcutEditorDialog.cpp")
    path_converter_cpp = read("src/ui/ShortcutPathConverterDialog.cpp")
    if "RuntimeInputMode" not in editor_cpp or "Advanced" not in editor_cpp:
        fail("v0.8 alpha.3.4 must not rewrite Shortcut Editor behavior")
    if "ShortcutPathConverterDialog::Show" not in path_converter_cpp:
        fail("v0.8 alpha.3.4 must keep Path Conversion implementation")

    update_tests = read("tests/UpdatePolicyTests.cpp")
    for token in (
        '"0.8.0-alpha.3.3"',
        '"0.8.0-alpha.3.4"',
        "UpdateChannel::Stable",
    ):
        if token not in update_tests:
            fail(f"v0.8 alpha.3.4 update ordering/default coverage missing: {token}")

    readme = read("README.md")
    changelog = read("CHANGELOG.md")
    roadmap = read("ROADMAP.md")

    for token in (
        "## v0.8.0-alpha.3.4 — Shortcut Manager Column Resize Hardening",
        "0.8.0.34",
        "constrained while the drag is in progress",
        "exact client width",
    ):
        if token not in readme:
            fail(f"v0.8 alpha.3.4 README contract missing: {token}")

    for token in (
        "## 0.8.0-alpha.3.4",
        "0.8.0.34",
        "HDN_ITEMCHANGING",
        "pseudo-fifth",
    ):
        if token not in changelog:
            fail(f"v0.8 alpha.3.4 changelog contract missing: {token}")

    for token in (
        "v0.8.0-alpha.3.4",
        "v0.8.0-alpha.3.5",
        "v0.8.0-alpha.3.6",
    ):
        if token not in roadmap:
            fail(f"v0.8 alpha.3 roadmap contract missing: {token}")

    print(
        "v0.8.0-alpha.3.4 Shortcut Manager column hardening verified:",
        "| settings=8 commands=2 usage=1 provider-cache=2",
        "| Header=ANSI+Unicode live constraints",
        "| first3=min-width/user-resizable",
        "| Target=120px-min/locked/elastic",
        "| four columns=exact Header fill",
        "| alpha.3.3 search/reopen/rows preserved",
        "| Editor/Path Conversion untouched",
    )
    raise SystemExit(0)

if version == "0.8.0-alpha.3.3":
    expected_schemas = {
        "kSettingsSchemaVersion": 8,
        "kCommandsSchemaVersion": 2,
        "kUsageSchemaVersion": 1,
    }
    for name, expected in expected_schemas.items():
        actual = cpp_int("src/core/ConfigIO.hpp", name)
        if actual != expected:
            fail(f"v0.8 alpha.3.3 {name}={actual}, expected {expected}")

    if cpp_int("src/core/ProviderCache.cpp", "kProviderCacheSchemaVersion") != 2:
        fail("v0.8 alpha.3.3 must keep provider-cache schemaVersion 2")

    manager_cpp = read("src/ui/ShortcutManagerWindow.cpp")
    manager_h = read("src/ui/ShortcutManagerWindow.hpp")

    for token in (
        "ResetTransientState",
        "suppressFilterRefresh_",
        "customColumnWidths_",
        "ListView_SetSelectionMark",
        "preferredId.empty()",
        "GetTextMetricsW",
        "metrics.tmHeight",
        "EM_GETRECT",
        "HDN_BEGINTRACKW",
        "HDN_ENDTRACKW",
        "HDN_DIVIDERDBLCLICKW",
        "header->iItem == 3",
        "minimumTarget",
        "UpdateColumnWidths(",
        "RDW_ALLCHILDREN",
        "RDW_UPDATENOW",
        "Scale(24)",
        "BeginDeferWindowPos(7)",
        "SWP_NOCOPYBITS",
    ):
        if token not in manager_cpp and token not in manager_h:
            fail(f"v0.8 alpha.3.3 Shortcut Manager final polish missing: {token}")

    for token in (
        "selected = 0;",
        "Scale(30)",
        "EM_SETCUEBANNER",
        "LVS_EX_GRIDLINES",
        "kIdClose",
        "close_",
    ):
        if token in manager_cpp or token in manager_h:
            fail(f"v0.8 alpha.3.3 obsolete Manager behavior returned: {token}")

    if manager_cpp.count("ListView_InsertColumn(") != 1:
        fail("v0.8 alpha.3.3 must keep exactly the four-column creation helper")

    for token in (
        "addColumn(\n        0,",
        "addColumn(\n        1,",
        "addColumn(\n        2,",
        "addColumn(\n        3,",
    ):
        if token not in manager_cpp:
            fail(f"v0.8 alpha.3.3 four-column contract missing: {token}")

    if "StartUpdateCheck(" in manager_cpp:
        fail("v0.8 alpha.3.3 Shortcut Manager must not touch updater execution")

    editor_cpp = read("src/ui/ShortcutEditorDialog.cpp")
    path_converter_cpp = read("src/ui/ShortcutPathConverterDialog.cpp")
    for token in (
        "RuntimeInputMode",
        "Advanced",
    ):
        if token not in editor_cpp:
            fail(f"v0.8 alpha.3.3 Shortcut Editor baseline missing: {token}")
    if "ShortcutPathConverterDialog::Show" not in path_converter_cpp:
        fail("v0.8 alpha.3.3 Path Conversion implementation missing")

    update_tests = read("tests/UpdatePolicyTests.cpp")
    for token in (
        '"0.8.0-alpha.3.2"',
        '"0.8.0-alpha.3.3"',
        "UpdateChannel::Stable",
    ):
        if token not in update_tests:
            fail(f"v0.8 alpha.3.3 update ordering/default coverage missing: {token}")

    readme = read("README.md")
    changelog = read("CHANGELOG.md")
    roadmap = read("ROADMAP.md")

    for token in (
        "## v0.8.0-alpha.3.3 — Shortcut Manager Final Polish",
        "0.8.0.33",
        "interaction state separately",
        "elastic final column",
        "body-font metrics",
    ):
        if token not in readme:
            fail(f"v0.8 alpha.3.3 README contract missing: {token}")

    for token in (
        "## 0.8.0-alpha.3.3",
        "0.8.0.33",
        "clears the search query",
        "Target as the locked elastic final column",
        "EN_CHANGE",
    ):
        if token not in changelog:
            fail(f"v0.8 alpha.3.3 changelog contract missing: {token}")

    for token in (
        "v0.8.0-alpha.3.3",
        "v0.8.0-alpha.3.4",
        "v0.8.0-alpha.3.5",
    ):
        if token not in roadmap:
            fail(f"v0.8 alpha.3 roadmap contract missing: {token}")

    print(
        "v0.8.0-alpha.3.3 Shortcut Manager final polish verified:",
        "| settings=8 commands=2 usage=1 provider-cache=2",
        "| reopen=geometry-kept/transient-reset",
        "| rows=24px",
        "| columns=3 user-resizable + elastic Target",
        "| target divider=locked",
        "| search=font-derived/edit-rect/erase-on-change",
        "| Editor/Path Conversion semantics untouched",
    )
    raise SystemExit(0)

if version == "0.8.0-alpha.3.2":
    expected_schemas = {
        "kSettingsSchemaVersion": 8,
        "kCommandsSchemaVersion": 2,
        "kUsageSchemaVersion": 1,
    }
    for name, expected in expected_schemas.items():
        actual = cpp_int("src/core/ConfigIO.hpp", name)
        if actual != expected:
            fail(f"v0.8 alpha.3.2 {name}={actual}, expected {expected}")

    if cpp_int("src/core/ProviderCache.cpp", "kProviderCacheSchemaVersion") != 2:
        fail("v0.8 alpha.3.2 must keep provider-cache schemaVersion 2")

    manager_cpp = read("src/ui/ShortcutManagerWindow.cpp")
    manager_h = read("src/ui/ShortcutManagerWindow.hpp")

    for token in (
        "Scale(900)",
        "Scale(560)",
        "Scale(720)",
        "Scale(480)",
        "BeginDeferWindowPos(7)",
        "DeferWindowPos(",
        "SWP_NOCOPYBITS",
        "RDW_ALLCHILDREN",
        "RDW_UPDATENOW",
        "Scale(24)",
        "headerSpec.pointSize - 1",
        "contentWidth * 22 / 100",
        "contentWidth * 26 / 100",
        "contentWidth * 12 / 100",
        "contentWidth -",
        "CDRF_SKIPDEFAULT",
        "palette.selectionBackground",
        "palette.controlBackground",
        "Search shortcuts",
        "hwnd == self->filter_",
        "message == WM_PAINT",
        "WS_BORDER",
    ):
        if token not in manager_cpp and token not in manager_h:
            fail(f"v0.8 alpha.3.2 Shortcut Manager polish missing: {token}")

    for token in (
        "Scale(980)",
        "Scale(650)",
        "Scale(30)",
        "WS_EX_CLIENTEDGE",
        "EM_SETCUEBANNER",
        "const bool primary",
        "palette.accent;",
        "LVS_EX_GRIDLINES",
        "kIdClose",
        "close_",
    ):
        if token in manager_cpp or token in manager_h:
            fail(f"v0.8 alpha.3.2 obsolete Manager behavior returned: {token}")

    if manager_cpp.count("ListView_InsertColumn(") != 1:
        fail("v0.8 alpha.3.2 column creation helper changed unexpectedly")
    for token in (
        "addColumn(\n        0,",
        "addColumn(\n        1,",
        "addColumn(\n        2,",
        "addColumn(\n        3,",
    ):
        if token not in manager_cpp:
            fail(f"v0.8 alpha.3.2 four-column contract missing: {token}")

    editor_cpp = read("src/ui/ShortcutEditorDialog.cpp")
    path_converter_cpp = read("src/ui/ShortcutPathConverterDialog.cpp")
    for token in (
        "RuntimeInputMode",
        "Advanced",
    ):
        if token not in editor_cpp:
            fail(f"v0.8 alpha.3.2 Shortcut Editor baseline missing: {token}")
    if "ShortcutPathConverterDialog::Show" not in path_converter_cpp:
        fail("v0.8 alpha.3.2 Path Conversion implementation missing")

    hotkey_registry = (
        read("src/core/HotkeyRegistry.hpp") +
        read("src/core/HotkeyRegistry.cpp")
    )
    for token in (
        "launcher.activate",
        "launcher.activateSecondary",
        "launcher.openSettings",
        "result.navigateCurrentFileManager",
        "result.copySelectedTarget",
    ):
        if token not in hotkey_registry:
            fail(f"v0.8 alpha.3.2 Hotkey Registry ID changed/missing: {token}")

    update_tests = read("tests/UpdatePolicyTests.cpp")
    for token in (
        '"0.8.0-alpha.3.1"',
        '"0.8.0-alpha.3.2"',
        "UpdateChannel::Stable",
    ):
        if token not in update_tests:
            fail(f"v0.8 alpha.3.2 update ordering/default coverage missing: {token}")

    readme = read("README.md")
    changelog = read("CHANGELOG.md")
    roadmap = read("ROADMAP.md")

    for token in (
        "## v0.8.0-alpha.3.2 — Shortcut Manager Real-world Polish",
        "0.8.0.32",
        "DeferWindowPos",
        "24 logical pixels",
        "exactly four columns",
    ):
        if token not in readme:
            fail(f"v0.8 alpha.3.2 README contract missing: {token}")

    for token in (
        "## 0.8.0-alpha.3.2",
        "0.8.0.32",
        "24 logical pixels",
        "pseudo-fifth header area",
        "custom-drawn",
    ):
        if token not in changelog:
            fail(f"v0.8 alpha.3.2 changelog contract missing: {token}")

    for token in (
        "v0.8.0-alpha.3.2",
        "v0.8.0-alpha.3.3",
        "v0.8.0-alpha.3.4",
    ):
        if token not in roadmap:
            fail(f"v0.8 alpha.3 roadmap contract missing: {token}")

    print(
        "v0.8.0-alpha.3.2 Shortcut Manager polish verified:",
        "| settings=8 commands=2 usage=1 provider-cache=2",
        "| default=900x560 resizable",
        "| rows=24px",
        "| columns=4 exact-fill",
        "| resize=deferred/no-copybits/full-redraw",
        "| selection=custom light accent",
        "| search=custom placeholder",
        "| Editor/Path Conversion semantics untouched",
    )
    raise SystemExit(0)

if version == "0.8.0-alpha.3.1":
    expected_schemas = {
        "kSettingsSchemaVersion": 8,
        "kCommandsSchemaVersion": 2,
        "kUsageSchemaVersion": 1,
    }
    for name, expected in expected_schemas.items():
        actual = cpp_int("src/core/ConfigIO.hpp", name)
        if actual != expected:
            fail(f"v0.8 alpha.3.1 {name}={actual}, expected {expected}")

    if cpp_int("src/core/ProviderCache.cpp", "kProviderCacheSchemaVersion") != 2:
        fail("v0.8 alpha.3.1 must keep provider-cache schemaVersion 2")

    manager_cpp = read("src/ui/ShortcutManagerWindow.cpp")
    manager_h = read("src/ui/ShortcutManagerWindow.hpp")

    for token in (
        'T(L"新建快捷项",',
        'T(L"搜索快捷项",',
        'T(L"路径转换…",',
        "BS_OWNERDRAW",
        "ui::kApplicationPalette",
        "kStandardControlHeightLogical",
        "LVS_EX_FULLROWSELECT",
        "LVS_EX_DOUBLEBUFFER",
        "Scale(30)",
        "CDDS_ITEMPOSTPAINT",
        "palette.separator",
        "LVM_SETEMPTYTEXT",
        'T(L"没有匹配的快捷项",',
        'T(L"还没有快捷项",',
        "SetWindowSubclass",
        "kChildSubclassId",
        "key == L'F'",
        "key == L'N'",
        "key == VK_RETURN",
        "key == VK_ESCAPE",
        "WM_DPICHANGED",
        "Scale(720)",
        "Scale(480)",
        "UpdateColumnWidths",
        "RebuildRowHeightImageList",
        "BodySemibold",
    ):
        if token not in manager_cpp and token not in manager_h:
            fail(f"v0.8 alpha.3.1 Shortcut Manager contract missing: {token}")

    for token in (
        "kIdClose",
        "close_",
        "LVS_EX_GRIDLINES",
        'T(L"筛选快捷项：快捷词、名称或目标",',
    ):
        if token in manager_cpp or token in manager_h:
            fail(f"v0.8 alpha.3.1 obsolete Shortcut Manager surface returned: {token}")

    if manager_cpp.find("MoveWindow(\n        pathConversion_") < 0 or \
       manager_cpp.find("MoveWindow(\n        delete_") < 0 or \
       manager_cpp.find("MoveWindow(\n        edit_") < 0 or \
       manager_cpp.find("MoveWindow(\n        test_") < 0:
        fail("v0.8 alpha.3.1 bottom Shortcut Manager action layout is incomplete")

    hotkey_registry = (
        read("src/core/HotkeyRegistry.hpp") +
        read("src/core/HotkeyRegistry.cpp")
    )
    for token in (
        "launcher.activate",
        "launcher.activateSecondary",
        "launcher.openSettings",
        "result.navigateCurrentFileManager",
        "result.copySelectedTarget",
    ):
        if token not in hotkey_registry:
            fail(f"v0.8 alpha.3.1 Hotkey Registry ID changed/missing: {token}")

    update_tests = read("tests/UpdatePolicyTests.cpp")
    for token in (
        '"0.8.0-alpha.2.14"',
        '"0.8.0-alpha.3"',
        '"0.8.0-alpha.3.1"',
        "UpdateChannel::Stable",
    ):
        if token not in update_tests:
            fail(f"v0.8 alpha.3.1 update ordering/default coverage missing: {token}")

    readme = read("README.md")
    changelog = read("CHANGELOG.md")
    roadmap = read("ROADMAP.md")

    for token in (
        "## v0.8.0-alpha.3.1 — Shortcut Manager Visual Consolidation",
        "0.8.0.31",
        "Ctrl+F",
        "native resizable Win32 ListView",
    ):
        if token not in readme:
            fail(f"v0.8 alpha.3.1 README contract missing: {token}")

    for token in (
        "## 0.8.0-alpha.3.1",
        "0.8.0.31",
        "Removed the redundant Close button",
        "Shortcut Editor and Path Conversion internals unchanged",
    ):
        if token not in changelog:
            fail(f"v0.8 alpha.3.1 changelog contract missing: {token}")

    for token in (
        "v0.8.0-alpha.3.1",
        "v0.8.0-alpha.3.2",
        "v0.8.0-alpha.3.3",
    ):
        if token not in roadmap:
            fail(f"v0.8 alpha.3 roadmap contract missing: {token}")

    print(
        "v0.8.0-alpha.3.1 Shortcut Manager contract verified:",
        "| settings=8 commands=2 usage=1 provider-cache=2",
        "| layout=search/new + dense list + split bottom actions",
        "| close button=removed",
        "| list=30px/no-grid/shared palette",
        "| empty states=native concise",
        "| keyboard=Ctrl+F/Ctrl+N/Ctrl+Enter/Esc",
        "| DPI=PerMonitorV2 resources rebuilt",
        "| Editor/Path Conversion semantics untouched",
    )
    raise SystemExit(0)

if version == "0.8.0-alpha.2.14":
    expected_schemas = {
        "kSettingsSchemaVersion": 8,
        "kCommandsSchemaVersion": 2,
        "kUsageSchemaVersion": 1,
    }
    for name, expected in expected_schemas.items():
        actual = cpp_int("src/core/ConfigIO.hpp", name)
        if actual != expected:
            fail(f"v0.8 alpha.2.14 {name}={actual}, expected {expected}")

    if cpp_int("src/core/ProviderCache.cpp", "kProviderCacheSchemaVersion") != 2:
        fail("v0.8 alpha.2.14 must keep provider-cache schemaVersion 2")

    settings_cpp = read("src/ui/SettingsWindow.cpp")
    settings_h = read("src/ui/SettingsWindow.hpp")

    for token in (
        "const int resetWidth =\n            Scale(84);",
        "const int resetGap =\n            Scale(12);",
        "const int resetX =\n                captureX -",
        "row.reset,\n                resetX,",
        "row.statusVisible;",
        "const bool showReset =\n            page_ == Page::Hotkeys &&\n            modified;",
        "Only status/capture",
        "WM_SETREDRAW",
        "RDW_ALLCHILDREN",
        "CancelHotkeyCapture",
    ):
        if token not in settings_cpp and token not in settings_h:
            fail(f"v0.8 alpha.2.14 Hotkey inline-reset contract missing: {token}")

    aux_start = settings_cpp.find(
        "bool SettingsWindow::\nHotkeyRowHasAuxiliaryContent(")
    aux_end = settings_cpp.find(
        "\nint SettingsWindow::HotkeyAuxiliaryHeight(",
        aux_start)
    if aux_start < 0 or aux_end < 0:
        fail("v0.8 alpha.2.14 auxiliary-state helper could not be inspected")
    aux_body = settings_cpp[aux_start:aux_end]
    if "row.resetVisible" in aux_body:
        fail("v0.8 alpha.2.14 Reset must not contribute to auxiliary row height")
    if "row.statusVisible" not in aux_body:
        fail("v0.8 alpha.2.14 status must remain the auxiliary-height trigger")

    aux_height_start = settings_cpp.find(
        "int SettingsWindow::HotkeyAuxiliaryHeight(")
    aux_height_end = settings_cpp.find(
        "\nint SettingsWindow::HotkeyRowHeight(",
        aux_height_start)
    if aux_height_start < 0 or aux_height_end < 0:
        fail("v0.8 alpha.2.14 HotkeyAuxiliaryHeight could not be inspected")
    if "resetVisible" in settings_cpp[aux_height_start:aux_height_end]:
        fail("v0.8 alpha.2.14 reset-only states must not expand Hotkey rows")

    layout_start = settings_cpp.find(
        "if (page_ == Page::Hotkeys) {",
        settings_cpp.find("void SettingsWindow::Layout()"))
    layout_end = settings_cpp.find(
        "if (page_ == Page::Providers) {",
        layout_start)
    if layout_start < 0 or layout_end < 0:
        fail("v0.8 alpha.2.14 Hotkey layout block could not be inspected")
    layout_body = settings_cpp[layout_start:layout_end]
    for token in (
        "resetX",
        "resetGap",
        "resetWidth",
        "top +\n                    (baseRowHeight -\n                     Scale(26)) / 2",
    ):
        if token not in layout_body:
            fail(f"v0.8 alpha.2.14 inline Reset geometry missing: {token}")
    if "WM_VSCROLL" in layout_body:
        fail("v0.8 alpha.2.14 must not add Hotkey scrolling")

    hotkey_registry = (
        read("src/core/HotkeyRegistry.hpp") +
        read("src/core/HotkeyRegistry.cpp")
    )
    for token in (
        "launcher.activate",
        "launcher.activateSecondary",
        "launcher.openSettings",
        "result.navigateCurrentFileManager",
        "result.copySelectedTarget",
    ):
        if token not in hotkey_registry:
            fail(f"v0.8 alpha.2.14 Hotkey Registry ID changed/missing: {token}")

    update_tests = read("tests/UpdatePolicyTests.cpp")
    for token in (
        '"0.8.0-alpha.2.13"',
        '"0.8.0-alpha.2.14"',
        '"0.8.0-alpha.3"',
        "UpdateChannel::Stable",
    ):
        if token not in update_tests:
            fail(f"v0.8 alpha.2.14 update ordering/default coverage missing: {token}")

    readme = read("README.md")
    changelog = read("CHANGELOG.md")
    roadmap = read("ROADMAP.md")

    for token in (
        "## v0.8.0-alpha.2.14 — Hotkey Inline Reset Polish",
        "0.8.0.34",
        "immediately to the left",
        "No scrolling is added",
    ):
        if token not in readme:
            fail(f"v0.8 alpha.2.14 README contract missing: {token}")

    for token in (
        "## 0.8.0-alpha.2.14",
        "0.8.0.34",
        "inline Reset column",
        "only auxiliary-row content",
    ):
        if token not in changelog:
            fail(f"v0.8 alpha.2.14 changelog contract missing: {token}")

    for token in (
        "v0.8.0-alpha.2.14",
        "v0.8.0-alpha.3",
    ):
        if token not in roadmap:
            fail(f"v0.8 alpha.2.14 roadmap contract missing: {token}")

    print(
        "v0.8.0-alpha.2.14 Hotkey inline-reset contract verified:",
        "| settings=8 commands=2 usage=1 provider-cache=2",
        "| per-item Reset=inline main row",
        "| modified bindings=no row expansion",
        "| auxiliary height=status only",
        "| scrolling=not added",
        "| alpha.2.13 atomic redraw preserved",
    )
    raise SystemExit(0)

if version == "0.8.0-alpha.2.13":
    expected_schemas = {
        "kSettingsSchemaVersion": 8,
        "kCommandsSchemaVersion": 2,
        "kUsageSchemaVersion": 1,
    }
    for name, expected in expected_schemas.items():
        actual = cpp_int("src/core/ConfigIO.hpp", name)
        if actual != expected:
            fail(f"v0.8 alpha.2.13 {name}={actual}, expected {expected}")

    if cpp_int("src/core/ProviderCache.cpp", "kProviderCacheSchemaVersion") != 2:
        fail("v0.8 alpha.2.13 must keep provider-cache schemaVersion 2")

    settings_cpp = read("src/ui/SettingsWindow.cpp")
    settings_h = read("src/ui/SettingsWindow.hpp")

    for token in (
        "bool resetVisible{false};",
        "bool statusVisible{false};",
        "row.statusVisible ||",
        "row.resetVisible;",
        "row.statusVisible =\n            showStatus;",
        "row.resetVisible =\n            showReset;",
        "row->statusVisible =\n        !status.empty();",
        "WM_SETREDRAW",
        "RDW_ALLCHILDREN",
        "RDW_FRAME",
        "RDW_UPDATENOW",
        "CancelHotkeyCapture",
        "page_ == Page::Hotkeys &&\n        page != Page::Hotkeys",
    ):
        if token not in settings_cpp and token not in settings_h:
            fail(f"v0.8 alpha.2.13 Hotkey atomic-layout contract missing: {token}")

    if "RelayoutHotkeyPage" in settings_cpp or "RelayoutHotkeyPage" in settings_h:
        fail("v0.8 alpha.2.13 must remove the alpha.2.12 RelayoutHotkeyPage path")

    if "SendMessageW(\n                control,\n                WM_SETREDRAW" in settings_cpp:
        fail("v0.8 alpha.2.13 must not toggle WM_SETREDRAW on Hotkey child controls")

    aux_start = settings_cpp.find(
        "bool SettingsWindow::\nHotkeyRowHasAuxiliaryContent(")
    aux_end = settings_cpp.find(
        "\nint SettingsWindow::HotkeyAuxiliaryHeight(",
        aux_start)
    if aux_start < 0 or aux_end < 0:
        fail("v0.8 alpha.2.13 auxiliary-state helper could not be inspected")
    aux_body = settings_cpp[aux_start:aux_end]
    if "GetWindowLongPtrW" in aux_body or "WS_VISIBLE" in aux_body:
        fail("v0.8 alpha.2.13 layout must not infer auxiliary visibility from HWND styles")

    refresh_start = settings_cpp.find(
        "void SettingsWindow::RefreshHotkeyPage(")
    refresh_end = settings_cpp.find(
        "\nvoid SettingsWindow::BeginHotkeyCapture(",
        refresh_start)
    if refresh_start < 0 or refresh_end < 0:
        fail("v0.8 alpha.2.13 RefreshHotkeyPage could not be inspected")
    refresh_body = settings_cpp[refresh_start:refresh_end]
    for token in (
        "atomicUpdate",
        "WM_SETREDRAW",
        "ShowWindow(",
        "Layout();",
        "RDW_ALLCHILDREN",
    ):
        if token not in refresh_body:
            fail(f"v0.8 alpha.2.13 atomic refresh missing: {token}")

    hotkey_registry = (
        read("src/core/HotkeyRegistry.hpp") +
        read("src/core/HotkeyRegistry.cpp")
    )
    for token in (
        "launcher.activate",
        "launcher.activateSecondary",
        "launcher.openSettings",
        "result.navigateCurrentFileManager",
        "result.copySelectedTarget",
    ):
        if token not in hotkey_registry:
            fail(f"v0.8 alpha.2.13 Hotkey Registry ID changed/missing: {token}")

    update_tests = read("tests/UpdatePolicyTests.cpp")
    for token in (
        '"0.8.0-alpha.2.12"',
        '"0.8.0-alpha.2.13"',
        '"0.8.0-alpha.3"',
        "UpdateChannel::Stable",
    ):
        if token not in update_tests:
            fail(f"v0.8 alpha.2.13 update ordering/default coverage missing: {token}")

    readme = read("README.md")
    changelog = read("CHANGELOG.md")
    roadmap = read("ROADMAP.md")

    for token in (
        "## v0.8.0-alpha.2.13 — Hotkey Atomic Layout Fix",
        "0.8.0.33",
        "statusVisible/resetVisible",
        "Child controls are never individually sent",
    ):
        if token not in readme:
            fail(f"v0.8 alpha.2.13 README contract missing: {token}")

    for token in (
        "## 0.8.0-alpha.2.13",
        "0.8.0.33",
        "Removed child-level",
        "explicit per-row",
    ):
        if token not in changelog:
            fail(f"v0.8 alpha.2.13 changelog contract missing: {token}")

    for token in (
        "v0.8.0-alpha.2.13",
        "v0.8.0-alpha.3",
    ):
        if token not in roadmap:
            fail(f"v0.8 alpha.2.13 roadmap contract missing: {token}")

    print(
        "v0.8.0-alpha.2.13 Hotkey atomic-layout contract verified:",
        "| settings=8 commands=2 usage=1 provider-cache=2",
        "| auxiliary visibility=explicit state",
        "| redraw=parent-level atomic transaction",
        "| child WM_SETREDRAW=removed",
        "| alpha.2.12 capture lifecycle preserved",
    )
    raise SystemExit(0)

if version == "0.8.0-alpha.2.12":
    expected_schemas = {
        "kSettingsSchemaVersion": 8,
        "kCommandsSchemaVersion": 2,
        "kUsageSchemaVersion": 1,
    }
    for name, expected in expected_schemas.items():
        actual = cpp_int("src/core/ConfigIO.hpp", name)
        if actual != expected:
            fail(f"v0.8 alpha.2.12 {name}={actual}, expected {expected}")

    if cpp_int("src/core/ProviderCache.cpp", "kProviderCacheSchemaVersion") != 2:
        fail("v0.8 alpha.2.12 must keep provider-cache schemaVersion 2")

    settings_cpp = read("src/ui/SettingsWindow.cpp")
    settings_h = read("src/ui/SettingsWindow.hpp")

    for token in (
        "CancelHotkeyCapture",
        "RelayoutHotkeyPage",
        "RefreshHotkeyPage(\n    bool relayout)",
        "WM_SETREDRAW",
        "RDW_ALLCHILDREN",
        "RDW_ERASE",
        "RDW_UPDATENOW",
        "for (HWND control :\n         hotkeyControls_)",
        "page_ == Page::Hotkeys &&\n        page != Page::Hotkeys",
        "RefreshHotkeyPage(false)",
        "case WM_ACTIVATE:",
        "WA_INACTIVE",
        "case WM_NCLBUTTONDOWN:",
        "case WM_PARENTNOTIFY:",
        "WindowFromPoint",
        "isHotkeyCaptureWindow",
        "CancelHotkeyCapture(false);\n        CommitPendingProviderChanges();",
    ):
        if token not in settings_cpp and token not in settings_h:
            fail(f"v0.8 alpha.2.12 Hotkey lifecycle/redraw contract missing: {token}")

    begin_capture_start = settings_cpp.find(
        "void SettingsWindow::BeginHotkeyCapture(")
    begin_capture_end = settings_cpp.find(
        "\nvoid SettingsWindow::CancelHotkeyCapture(",
        begin_capture_start)
    if begin_capture_start < 0 or begin_capture_end < 0:
        fail("v0.8 alpha.2.12 BeginHotkeyCapture could not be inspected")
    begin_capture = settings_cpp[begin_capture_start:begin_capture_end]
    for token in (
        "capturingHotkeyActionId_ ==",
        "CancelHotkeyCapture();",
        "capturingHotkeyActionId_ =",
    ):
        if token not in begin_capture:
            fail(f"v0.8 alpha.2.12 capture-toggle/switch contract missing: {token}")

    show_start = settings_cpp.find("void SettingsWindow::Show()")
    show_end = settings_cpp.find(
        "\nLRESULT CALLBACK SettingsWindow::WindowProc",
        show_start)
    if show_start < 0 or show_end < 0:
        fail("v0.8 alpha.2.12 SettingsWindow::Show could not be inspected")
    if "CancelHotkeyCapture(false);" not in settings_cpp[show_start:show_end]:
        fail("v0.8 alpha.2.12 reopening Settings must clear capture state")

    hotkey_registry = (
        read("src/core/HotkeyRegistry.hpp") +
        read("src/core/HotkeyRegistry.cpp")
    )
    for token in (
        "launcher.activate",
        "launcher.activateSecondary",
        "launcher.openSettings",
        "result.navigateCurrentFileManager",
        "result.copySelectedTarget",
    ):
        if token not in hotkey_registry:
            fail(f"v0.8 alpha.2.12 Hotkey Registry ID changed/missing: {token}")

    update_tests = read("tests/UpdatePolicyTests.cpp")
    for token in (
        '"0.8.0-alpha.2.11"',
        '"0.8.0-alpha.2.12"',
        '"0.8.0-alpha.3"',
        "UpdateChannel::Stable",
    ):
        if token not in update_tests:
            fail(f"v0.8 alpha.2.12 update ordering/default coverage missing: {token}")

    readme = read("README.md")
    changelog = read("CHANGELOG.md")
    roadmap = read("ROADMAP.md")

    for token in (
        "## v0.8.0-alpha.2.12 — Hotkey Capture Lifecycle & Redraw Fix",
        "0.8.0.32",
        "explicit transient session",
        "full redraw transaction",
    ):
        if token not in readme:
            fail(f"v0.8 alpha.2.12 README contract missing: {token}")

    for token in (
        "## 0.8.0-alpha.2.12",
        "0.8.0.32",
        "unified Hotkey capture-cancel lifecycle",
        "full parent + child erase/redraw transaction",
    ):
        if token not in changelog:
            fail(f"v0.8 alpha.2.12 changelog contract missing: {token}")

    for token in (
        "v0.8.0-alpha.2.12",
        "v0.8.0-alpha.3",
    ):
        if token not in roadmap:
            fail(f"v0.8 alpha.2.12 roadmap contract missing: {token}")

    print(
        "v0.8.0-alpha.2.12 Hotkey capture/redraw contract verified:",
        "| settings=8 commands=2 usage=1 provider-cache=2",
        "| capture=session-scoped",
        "| outside/navigation/hide/deactivate=cancel",
        "| dynamic layout=parent+child full redraw",
        "| Hotkey IDs/behavior unchanged",
    )
    raise SystemExit(0)

if version == "0.8.0-alpha.2.11":
    expected_schemas = {
        "kSettingsSchemaVersion": 8,
        "kCommandsSchemaVersion": 2,
        "kUsageSchemaVersion": 1,
    }
    for name, expected in expected_schemas.items():
        actual = cpp_int("src/core/ConfigIO.hpp", name)
        if actual != expected:
            fail(f"v0.8 alpha.2.11 {name}={actual}, expected {expected}")

    if cpp_int("src/core/ProviderCache.cpp", "kProviderCacheSchemaVersion") != 2:
        fail("v0.8 alpha.2.11 must keep provider-cache schemaVersion 2")

    settings_cpp = read("src/ui/SettingsWindow.cpp")
    settings_h = read("src/ui/SettingsWindow.hpp")

    for token in (
        "HotkeyAuxiliaryHeight",
        "DrawHotkeyResetLink",
        "Scale(224)",
        "textHeight +\n                Scale(10)",
        "modified &&\n            !showStatus",
        'T(L"按下新组合键，Esc 取消",',
        "const int auxiliaryWidth =",
        "captureX,\n                auxiliaryTop +\n                    Scale(4)",
        "row.reset,\n                captureX",
        "kIdHotkeyResetBase",
        "IDC_HAND",
        "launcherCardBottom +\n                Scale(14)",
    ):
        if token not in settings_cpp and token not in settings_h:
            fail(f"v0.8 alpha.2.11 Hotkey auxiliary contract missing: {token}")

    if "Scale(54)" not in settings_cpp[
        settings_cpp.find("if (page_ == Page::Hotkeys) {",
                          settings_cpp.find("void SettingsWindow::Layout()")):
        settings_cpp.find("if (page_ == Page::Providers) {",
                          settings_cpp.find("void SettingsWindow::Layout()"))
    ]:
        fail("v0.8 alpha.2.11 must keep 54px normal Hotkey rows")

    hotkey_registry = (
        read("src/core/HotkeyRegistry.hpp") +
        read("src/core/HotkeyRegistry.cpp")
    )
    for token in (
        "launcher.activate",
        "launcher.activateSecondary",
        "launcher.openSettings",
        "result.navigateCurrentFileManager",
        "result.copySelectedTarget",
    ):
        if token not in hotkey_registry:
            fail(f"v0.8 alpha.2.11 Hotkey Registry ID changed/missing: {token}")

    update_tests = read("tests/UpdatePolicyTests.cpp")
    for token in (
        '"0.8.0-alpha.2.10"',
        '"0.8.0-alpha.2.11"',
        '"0.8.0-alpha.3"',
        "UpdateChannel::Stable",
    ):
        if token not in update_tests:
            fail(f"v0.8 alpha.2.11 update ordering/default coverage missing: {token}")

    readme = read("README.md")
    changelog = read("CHANGELOG.md")
    roadmap = read("ROADMAP.md")

    for token in (
        "## v0.8.0-alpha.2.11 — Hotkey Auxiliary State Polish",
        "0.8.0.31",
        "lightweight text action",
        "right-side auxiliary column",
    ):
        if token not in readme:
            fail(f"v0.8 alpha.2.11 README contract missing: {token}")

    for token in (
        "## 0.8.0-alpha.2.11",
        "0.8.0.31",
        "owner-drawn text action",
        "capture/error status priority",
    ):
        if token not in changelog:
            fail(f"v0.8 alpha.2.11 changelog contract missing: {token}")

    for token in (
        "v0.8.0-alpha.2.11",
        "v0.8.0-alpha.3",
    ):
        if token not in roadmap:
            fail(f"v0.8 alpha.2.11 roadmap contract missing: {token}")

    print(
        "v0.8.0-alpha.2.11 Hotkey auxiliary-state contract verified:",
        "| settings=8 commands=2 usage=1 provider-cache=2",
        "| per-item Reset=lightweight link",
        "| helper text=right auxiliary column",
        "| separator-safe measured expansion",
        "| auxiliary priority=status before reset",
        "| hotkey behavior/IDs unchanged",
    )
    raise SystemExit(0)

if version == "0.8.0-alpha.2.10":
    expected_schemas = {
        "kSettingsSchemaVersion": 8,
        "kCommandsSchemaVersion": 2,
        "kUsageSchemaVersion": 1,
    }
    for name, expected in expected_schemas.items():
        actual = cpp_int("src/core/ConfigIO.hpp", name)
        if actual != expected:
            fail(f"v0.8 alpha.2.10 {name}={actual}, expected {expected}")

    if cpp_int("src/core/ProviderCache.cpp", "kProviderCacheSchemaVersion") != 2:
        fail("v0.8 alpha.2.10 must keep provider-cache schemaVersion 2")

    settings_cpp = read("src/ui/SettingsWindow.cpp")
    settings_h = read("src/ui/SettingsWindow.hpp")

    for token in (
        "HotkeyRowHasAuxiliaryContent",
        "HotkeyRowHeight",
        "HotkeyGroupHeight",
        "const int baseRowHeight =\n            Scale(54);",
        "const int auxiliaryHeight =\n            Scale(18);",
        "normalFont_",
        "drawGroupSeparators",
        "rowTop +=\n                            HotkeyRowHeight(row);",
        "const bool resetVisible",
        "Layout();\n        InvalidateRect(",
    ):
        if token not in settings_cpp and token not in settings_h:
            fail(f"v0.8 alpha.2.10 Hotkey density contract missing: {token}")

    hotkey_font_block = settings_cpp[
        settings_cpp.find("for (const auto& row :\n         hotkeyRows_) {",
                          settings_cpp.find("void SettingsWindow::ApplyFonts()")):
        settings_cpp.find("for (HWND control :\n         std::array<HWND, 13>",
                          settings_cpp.find("void SettingsWindow::ApplyFonts()"))
    ]
    if "sectionFont_" in hotkey_font_block:
        fail("v0.8 alpha.2.10 Hotkey action labels must not use section-title typography")
    if "normalFont_" not in hotkey_font_block:
        fail("v0.8 alpha.2.10 Hotkey rows must use normal Settings typography")

    hotkey_registry = (
        read("src/core/HotkeyRegistry.hpp") +
        read("src/core/HotkeyRegistry.cpp")
    )
    for token in (
        "launcher.activate",
        "launcher.activateSecondary",
        "launcher.openSettings",
        "result.navigateCurrentFileManager",
        "result.copySelectedTarget",
    ):
        if token not in hotkey_registry:
            fail(f"v0.8 alpha.2.10 Hotkey Registry ID changed/missing: {token}")

    if "Scale(72)" in settings_cpp[
        settings_cpp.find("if (page_ == Page::Hotkeys) {",
                          settings_cpp.find("void SettingsWindow::Layout()")):
        settings_cpp.find("if (page_ == Page::Providers) {",
                          settings_cpp.find("void SettingsWindow::Layout()"))
    ]:
        fail("v0.8 alpha.2.10 Hotkey layout must not retain the old fixed 72px row height")

    update_tests = read("tests/UpdatePolicyTests.cpp")
    for token in (
        '"0.8.0-alpha.2.9"',
        '"0.8.0-alpha.2.10"',
        '"0.8.0-alpha.3"',
        "UpdateChannel::Stable",
    ):
        if token not in update_tests:
            fail(f"v0.8 alpha.2.10 update ordering/default coverage missing: {token}")

    readme = read("README.md")
    changelog = read("CHANGELOG.md")
    roadmap = read("ROADMAP.md")

    for token in (
        "## v0.8.0-alpha.2.10 — Hotkey Typography & Density Polish",
        "0.8.0.30",
        "54 logical pixels",
        "18 logical pixels",
    ):
        if token not in readme:
            fail(f"v0.8 alpha.2.10 README contract missing: {token}")

    for token in (
        "## 0.8.0-alpha.2.10",
        "0.8.0.30",
        "conditional",
    ):
        if token not in changelog:
            fail(f"v0.8 alpha.2.10 changelog contract missing: {token}")

    for token in (
        "v0.8.0-alpha.2.10",
        "v0.8.0-alpha.3",
    ):
        if token not in roadmap:
            fail(f"v0.8 alpha.2.10 roadmap contract missing: {token}")

    print(
        "v0.8.0-alpha.2.10 Hotkey typography/density contract verified:",
        "| settings=8 commands=2 usage=1 provider-cache=2",
        "| action labels=body typography",
        "| normal row=54",
        "| auxiliary expansion=18",
        "| separators=Settings-consistent",
        "| hotkey behavior/IDs unchanged",
    )
    raise SystemExit(0)

if version == "0.8.0-alpha.2.9":
    expected_schemas = {
        "kSettingsSchemaVersion": 8,
        "kCommandsSchemaVersion": 2,
        "kUsageSchemaVersion": 1,
    }
    for name, expected in expected_schemas.items():
        actual = cpp_int("src/core/ConfigIO.hpp", name)
        if actual != expected:
            fail(f"v0.8 alpha.2.9 {name}={actual}, expected {expected}")

    if cpp_int("src/core/ProviderCache.cpp", "kProviderCacheSchemaVersion") != 2:
        fail("v0.8 alpha.2.9 must keep provider-cache schemaVersion 2")

    settings_cpp = read("src/ui/SettingsWindow.cpp")
    settings_h = read("src/ui/SettingsWindow.hpp")

    for token in (
        "SS_CENTERIMAGE",
        "const int versionRowTop",
        "const int versionRowHeight",
        "DrawUpdateStatus",
        "SS_OWNERDRAW",
        "const int statusRowTop",
        "const int rowCenterOffset",
        "DT_WORDBREAK",
        'L"GitHub ↗"',
        'T(L"接收预发布版本更新",',
        "PageCardRect(\n                    246,\n                    174,\n                    560)",
    ):
        if token not in settings_cpp and token not in settings_h:
            fail(f"v0.8 alpha.2.9 About alignment contract missing: {token}")

    if 'T(L"再次检查",' in settings_cpp or 'L"Check again"' in settings_cpp:
        fail("v0.8 alpha.2.9 must preserve the alpha.2.8 manual-check label")

    app_cpp = read("src/app/App.cpp")
    set_update_start = app_cpp.find("bool App::SetUpdateSettings(")
    set_update_end = app_cpp.find("\nvoid App::InvalidateUpdateCheckForChannelChange()", set_update_start)
    if set_update_start < 0 or set_update_end < 0:
        fail("v0.8 alpha.2.9 SetUpdateSettings function could not be inspected")
    if "StartUpdateCheck(" in app_cpp[set_update_start:set_update_end]:
        fail("v0.8 alpha.2.9 must preserve passive update preferences")

    update_tests = read("tests/UpdatePolicyTests.cpp")
    for token in (
        '"0.8.0-alpha.2.8"',
        '"0.8.0-alpha.2.9"',
        '"0.8.0-alpha.3"',
        "UpdateChannel::Stable",
    ):
        if token not in update_tests:
            fail(f"v0.8 alpha.2.9 update ordering/default coverage missing: {token}")

    readme = read("README.md")
    changelog = read("CHANGELOG.md")
    roadmap = read("ROADMAP.md")

    for token in (
        "## v0.8.0-alpha.2.9 — About Alignment Polish",
        "0.8.0.29",
        "24-logical-pixel metadata row",
    ):
        if token not in readme:
            fail(f"v0.8 alpha.2.9 README contract missing: {token}")

    for token in (
        "## 0.8.0-alpha.2.9",
        "0.8.0.29",
        "Owner-drew the update-status text",
    ):
        if token not in changelog:
            fail(f"v0.8 alpha.2.9 changelog contract missing: {token}")

    for token in (
        "v0.8.0-alpha.2.9",
        "v0.8.0-alpha.3",
    ):
        if token not in roadmap:
            fail(f"v0.8 alpha.2.9 roadmap contract missing: {token}")

    print(
        "v0.8.0-alpha.2.9 About alignment contract verified:",
        "| settings=8 commands=2 usage=1 provider-cache=2",
        "| version/GitHub=row-aligned",
        "| update status/action=center-aligned",
        "| wrapped status preserved",
        "| alpha.2.8 semantics unchanged",
    )
    raise SystemExit(0)

if version == "0.8.0-alpha.2.8":
    expected_schemas = {
        "kSettingsSchemaVersion": 8,
        "kCommandsSchemaVersion": 2,
        "kUsageSchemaVersion": 1,
    }
    for name, expected in expected_schemas.items():
        actual = cpp_int("src/core/ConfigIO.hpp", name)
        if actual != expected:
            fail(f"v0.8 alpha.2.8 {name}={actual}, expected {expected}")

    if cpp_int("src/core/ProviderCache.cpp", "kProviderCacheSchemaVersion") != 2:
        fail("v0.8 alpha.2.8 must keep provider-cache schemaVersion 2")

    settings = json.loads(read("config/settings.example.json"))
    if settings.get("schemaVersion") != 8:
        fail("v0.8 alpha.2.8 settings example must remain schemaVersion 8")
    if settings.get("update") != {"autoCheck": True, "channel": "stable"}:
        fail("v0.8 alpha.2.8 prerelease updates must remain opt-in/off by default")

    settings_cpp = read("src/ui/SettingsWindow.cpp")
    settings_h = read("src/ui/SettingsWindow.hpp")
    app_cpp = read("src/app/App.cpp")
    app_h = read("src/app/App.hpp")

    for token in (
        'T(L"接收预发布版本更新",',
        'L"GitHub ↗"',
        "DrawGitHubLink",
        "IDC_HAND",
        "UpdateSettingsChangedSinceCheck",
        "UpdateWorkerRunning",
        'T(L"尚未按当前设置检查更新。",',
        "updateAutoCheck_,\n        TRUE",
        "updatePrerelease_,\n        TRUE",
        "PageCardRect(\n                    246,\n                    174,\n                    560)",
        "int versionWidth",
        "const auto dismissComboFocus =",
        "std::array<HWND, 6>",
    ):
        if token not in settings_cpp and token not in settings_h:
            fail(f"v0.8 alpha.2.8 About/update UX contract missing: {token}")

    if 'T(L"再次检查",' in settings_cpp or 'L"Check again"' in settings_cpp:
        fail("v0.8 alpha.2.8 must keep the manual action labeled Check for updates")

    set_update_start = app_cpp.find("bool App::SetUpdateSettings(")
    set_update_end = app_cpp.find("\nwin::UpdateSnapshot\nApp::UpdateStatus()", set_update_start)
    if set_update_start < 0 or set_update_end < 0:
        fail("v0.8 alpha.2.8 SetUpdateSettings function could not be inspected")
    set_update_body = app_cpp[set_update_start:set_update_end]
    if "StartUpdateCheck(" in set_update_body:
        fail("v0.8 alpha.2.8 update preferences must not directly start an update check")
    if "previous.updateChannel !=" not in set_update_body or \
       "InvalidateUpdateCheckForChannelChange();" not in set_update_body:
        fail("v0.8 alpha.2.8 channel preference must invalidate without executing a check")

    invalidate_start = app_cpp.find("void App::InvalidateUpdateCheckForChannelChange()")
    invalidate_end = app_cpp.find("\nwin::UpdateSnapshot\nApp::UpdateStatus()", invalidate_start)
    if invalidate_start < 0 or invalidate_end < 0:
        fail("v0.8 alpha.2.8 channel invalidation helper could not be inspected")
    invalidate_body = app_cpp[invalidate_start:invalidate_end]
    for token in (
        "updateSettingsChangedSinceCheck_",
        "updateThread_.request_stop()",
        "preserveActiveUpdate",
        "++updateGeneration_",
    ):
        if token not in invalidate_body:
            fail(f"v0.8 alpha.2.8 passive update-settings contract missing: {token}")

    if "previous.updateChannel !=" not in app_cpp or \
       "settingsStore_.Data()\n            .updateChannel" not in app_cpp:
        fail("v0.8 alpha.2.8 reset-default update-channel invalidation is missing")

    for token in (
        "std::atomic<std::uint64_t>",
        "updateWorkerRunning_",
        "generation !=",
        "updateGeneration_.load()",
    ):
        if token not in app_cpp and token not in app_h:
            fail(f"v0.8 alpha.2.8 stale-update discard contract missing: {token}")

    for token in (
        "aboutProjectTitle_",
        "updateChannelLabel_",
        "updateChannel_",
        "kIdUpdateChannel",
        "updateCheck_",
        "updateInstall_",
        "kIdUpdateCheck",
        "kIdUpdateInstall",
        "RuntimeDiagnosticsSnapshot",
        "ProcessMemory",
        "Page::Diagnostics",
        "kIdNavDiagnostics",
        "kIdDataImportLegacy",
        "dataImportLegacy_",
    ):
        if token in settings_cpp or token in settings_h:
            fail(f"v0.8 alpha.2.8 obsolete/removed Settings surface returned: {token}")

    update_tests = read("tests/UpdatePolicyTests.cpp")
    for token in (
        '"0.8.0-alpha.2.7"',
        '"0.8.0-alpha.2.8"',
        '"0.8.0-alpha.3"',
        "UpdateChannel::Stable",
    ):
        if token not in update_tests:
            fail(f"v0.8 alpha.2.8 update ordering/default coverage missing: {token}")

    readme = read("README.md")
    changelog = read("CHANGELOG.md")
    roadmap = read("ROADMAP.md")

    for token in (
        "## v0.8.0-alpha.2.8 — About UX & Update Semantics",
        "0.8.0.28",
        "接收预发布版本更新",
        "GitHub ↗",
    ):
        if token not in readme:
            fail(f"v0.8 alpha.2.8 README contract missing: {token}")

    for token in (
        "## 0.8.0-alpha.2.8",
        "0.8.0.28",
        "Decoupled update preferences",
    ):
        if token not in changelog:
            fail(f"v0.8 alpha.2.8 changelog contract missing: {token}")

    for token in (
        "v0.8.0-alpha.2.8",
        "v0.8.0-alpha.3",
    ):
        if token not in roadmap:
            fail(f"v0.8 alpha.2.8 roadmap contract missing: {token}")

    print(
        "v0.8.0-alpha.2.8 About/update semantics contract verified:",
        "| settings=8 commands=2 usage=1 provider-cache=2",
        "| update switches=passive/non-blocking",
        "| stale checks discarded",
        "| GitHub=text link",
        "| alpha.2.7 updater safety preserved",
    )
    raise SystemExit(0)

if version == "0.8.0-alpha.2.7":
    expected_schemas = {
        "kSettingsSchemaVersion": 8,
        "kCommandsSchemaVersion": 2,
        "kUsageSchemaVersion": 1,
    }
    for name, expected in expected_schemas.items():
        actual = cpp_int("src/core/ConfigIO.hpp", name)
        if actual != expected:
            fail(f"v0.8 alpha.2.7 {name}={actual}, expected {expected}")

    if cpp_int("src/core/ProviderCache.cpp", "kProviderCacheSchemaVersion") != 2:
        fail("v0.8 alpha.2.7 must keep provider-cache schemaVersion 2")

    settings = json.loads(read("config/settings.example.json"))
    if settings.get("schemaVersion") != 8:
        fail("v0.8 alpha.2.7 settings example must remain schemaVersion 8")
    if settings.get("update") != {"autoCheck": True, "channel": "stable"}:
        fail("v0.8 alpha.2.7 prerelease updates must default to opt-in/off")

    settings_cpp = read("src/ui/SettingsWindow.cpp")
    settings_h = read("src/ui/SettingsWindow.hpp")

    for token in (
        "kIdUpdatePrerelease",
        "updatePrerelease_",
        "kIdUpdateAction",
        "updateAction_",
        'T(L"接收预发布版更新",',
        'L"Get prerelease updates"',
        "TogglePrereleaseUpdates",
        "PageCardRect(\n                    258,\n                    174,\n                    560)",
        "const int actionWidth =\n            Scale(140);",
        "aboutDescription_",
        "openGitHub_",
        "const auto dismissComboFocus =",
        "std::array<HWND, 6>",
    ):
        if token not in settings_cpp and token not in settings_h:
            fail(f"v0.8 alpha.2.7 About/update UX contract missing: {token}")

    for token in (
        "aboutProjectTitle_",
        "updateChannelLabel_",
        "updateChannel_",
        "kIdUpdateChannel",
        "updateCheck_",
        "updateInstall_",
        "kIdUpdateCheck",
        "kIdUpdateInstall",
        "RuntimeDiagnosticsSnapshot",
        "ProcessMemory",
        "Page::Diagnostics",
        "kIdNavDiagnostics",
        "kIdDataImportLegacy",
        "dataImportLegacy_",
    ):
        if token in settings_cpp or token in settings_h:
            fail(f"v0.8 alpha.2.7 obsolete/removed Settings surface returned: {token}")

    update_policy = read("src/core/UpdatePolicy.cpp")
    if "return UpdateChannel::Stable;" not in update_policy:
        fail("v0.8 alpha.2.7 first-run update channel must default to Stable")

    update_tests = read("tests/UpdatePolicyTests.cpp")
    for token in (
        '"0.8.0-alpha.2.6"',
        '"0.8.0-alpha.2.7"',
        '"0.8.0-alpha.3"',
        '"0.7.0-alpha.9"',
        '"0.7.0-beta.12"',
        '"0.7.0-rc.1"',
        "UpdateChannel::Stable",
    ):
        if token not in update_tests:
            fail(f"v0.8 alpha.2.7 update ordering/default coverage missing: {token}")

    config_schema = read("docs/CONFIG_SCHEMA.md")
    for token in (
        "Starting with v0.8.0-alpha.2.7",
        "prerelease updates are always an explicit opt-in",
        "existing persisted channel choices are preserved",
    ):
        if token not in config_schema:
            fail(f"v0.8 alpha.2.7 config documentation missing: {token}")

    readme = read("README.md")
    changelog = read("CHANGELOG.md")
    roadmap = read("ROADMAP.md")

    for token in (
        "## v0.8.0-alpha.2.7 — About Page & Update UX Polish",
        "0.8.0.27",
        "接收预发布版更新",
    ):
        if token not in readme:
            fail(f"v0.8 alpha.2.7 README contract missing: {token}")

    for token in (
        "## 0.8.0-alpha.2.7",
        "0.8.0.27",
        "Get prerelease updates",
    ):
        if token not in changelog:
            fail(f"v0.8 alpha.2.7 changelog contract missing: {token}")

    for token in (
        "v0.8.0-alpha.2.7",
        "v0.8.0-alpha.3",
    ):
        if token not in roadmap:
            fail(f"v0.8 alpha.2.7 roadmap contract missing: {token}")

    print(
        "v0.8.0-alpha.2.7 About/update UX contract verified:",
        "| settings=8 commands=2 usage=1 provider-cache=2",
        "| prerelease updates=opt-in",
        "| About card=560",
        "| single state-driven update action",
        "| alpha.2.6 behavior preserved",
    )
    raise SystemExit(0)

if version == "0.8.0-alpha.2.6":
    expected_schemas = {
        "kSettingsSchemaVersion": 8,
        "kCommandsSchemaVersion": 2,
        "kUsageSchemaVersion": 1,
    }
    for name, expected in expected_schemas.items():
        actual = cpp_int("src/core/ConfigIO.hpp", name)
        if actual != expected:
            fail(f"v0.8 alpha.2.6 {name}={actual}, expected {expected}")

    if cpp_int("src/core/ProviderCache.cpp", "kProviderCacheSchemaVersion") != 2:
        fail("v0.8 alpha.2.6 must keep provider-cache schemaVersion 2")

    settings = json.loads(read("config/settings.example.json"))
    if settings.get("schemaVersion") != 8:
        fail("v0.8 alpha.2.6 settings example must remain schemaVersion 8")

    settings_cpp = read("src/ui/SettingsWindow.cpp")
    settings_h = read("src/ui/SettingsWindow.hpp")

    for token in (
        "const auto dismissComboFocus =",
        "std::array<HWND, 7>",
        "numericQuickLaunchOrder_",
        "popupMonitor_",
        "launcherPlacement_",
        "settingsPlacement_",
        "uiStyle_",
        "language_",
        "updateChannel_",
        "case WM_LBUTTONDOWN:",
        "case WM_PARENTNOTIFY:",
        "dismissComboFocus();",
        "const int comboWidth =\n            Scale(160);",
        "const int comboWidth =\n            Scale(180);",
        "Scale(100)",
        "hotkeyGlobalTitle_",
        "pendingProviderStates_",
    ):
        if token not in settings_cpp and token not in settings_h:
            fail(f"v0.8 alpha.2.6 Settings focus/polish contract missing: {token}")

    for token in (
        "RuntimeDiagnosticsSnapshot",
        "ProcessMemory",
        "Page::Diagnostics",
        "kIdNavDiagnostics",
        "kIdDataImportLegacy",
        "dataImportLegacy_",
    ):
        if token in settings_cpp or token in settings_h:
            fail(f"v0.8 alpha.2.6 removed Settings surface returned: {token}")

    update_tests = read("tests/UpdatePolicyTests.cpp")
    for token in (
        '"0.8.0-alpha.2.5"',
        '"0.8.0-alpha.2.6"',
        '"0.8.0-alpha.3"',
        "UpdateChannel::Development",
    ):
        if token not in update_tests:
            fail(f"v0.8 alpha.2.6 update ordering/default coverage missing: {token}")

    readme = read("README.md")
    changelog = read("CHANGELOG.md")
    roadmap = read("ROADMAP.md")

    for token in (
        "## v0.8.0-alpha.2.6 — Settings Focus & Combo Polish",
        "0.8.0.26",
    ):
        if token not in readme:
            fail(f"v0.8 alpha.2.6 README contract missing: {token}")

    for token in (
        "## 0.8.0-alpha.2.6",
        "0.8.0.26",
    ):
        if token not in changelog:
            fail(f"v0.8 alpha.2.6 changelog contract missing: {token}")

    for token in (
        "v0.8.0-alpha.2.6",
        "v0.8.0-alpha.3",
    ):
        if token not in roadmap:
            fail(f"v0.8 alpha.2.6 roadmap contract missing: {token}")

    print(
        "v0.8.0-alpha.2.6 Settings focus/polish contract verified:",
        "| settings=8 commands=2 usage=1 provider-cache=2",
        "| all Settings combos dismiss on internal clicks",
        "| Appearance combos=160",
        "| alpha.2.5 behavior preserved",
    )
    raise SystemExit(0)

if version == "0.8.0-alpha.2.5":
    expected_schemas = {
        "kSettingsSchemaVersion": 8,
        "kCommandsSchemaVersion": 2,
        "kUsageSchemaVersion": 1,
    }
    for name, expected in expected_schemas.items():
        actual = cpp_int("src/core/ConfigIO.hpp", name)
        if actual != expected:
            fail(f"v0.8 alpha.2.5 {name}={actual}, expected {expected}")

    if cpp_int("src/core/ProviderCache.cpp", "kProviderCacheSchemaVersion") != 2:
        fail("v0.8 alpha.2.5 must keep provider-cache schemaVersion 2")

    settings = json.loads(read("config/settings.example.json"))
    if settings.get("schemaVersion") != 8:
        fail("v0.8 alpha.2.5 settings example must remain schemaVersion 8")

    expected_providers = {
        "windows.startmenu": True,
        "windows.packaged": True,
        "windows.apppaths": True,
        "windows.path": True,
        "everything.filesystem": False,
    }
    if settings.get("providers") != expected_providers:
        fail("v0.8 alpha.2.5 changed frozen provider defaults")

    expected_hotkeys = {
        "launcher.activate",
        "launcher.activateSecondary",
        "launcher.openSettings",
        "result.navigateCurrentFileManager",
        "result.copySelectedTarget",
    }
    bindings = settings.get("hotkeys", {}).get("bindings", {})
    if set(bindings) != expected_hotkeys:
        fail("v0.8 alpha.2.5 changed frozen Hotkey Registry action IDs")

    settings_cpp = read("src/ui/SettingsWindow.cpp")
    settings_h = read("src/ui/SettingsWindow.hpp")

    for token in (
        "Scale(118)",
        "Scale(100)",
        "const int comboWidth =\n            Scale(180);",
        "const int captureX =\n                toggleX -",
        "const int resetAllWidth =\n            Scale(184);",
        "cardRight -\n                inner -\n                resetAllWidth",
        "Scale(158)",
        "Scale(282)",
        "pendingProviderStates_",
        "SetProviderEnabledBatch(",
        "hotkeyGlobalTitle_",
        "hotkeyLauncherTitle_",
    ):
        if token not in settings_cpp and token not in settings_h:
            fail(f"v0.8 alpha.2.5 alignment contract missing: {token}")

    for token in (
        "Scale(168)",
        "const int comboWidth =\n            Scale(220);",
        "row.enabled\n                    ? toggleX",
    ):
        if token in settings_cpp:
            fail(f"v0.8 alpha.2.5 obsolete alignment remains: {token}")

    for token in (
        "RuntimeDiagnosticsSnapshot",
        "ProcessMemory",
        "Page::Diagnostics",
        "kIdNavDiagnostics",
        "kIdDataImportLegacy",
        "dataImportLegacy_",
    ):
        if token in settings_cpp or token in settings_h:
            fail(f"v0.8 alpha.2.5 removed Settings surface returned: {token}")

    update_tests = read("tests/UpdatePolicyTests.cpp")
    for token in (
        '"0.8.0-alpha.2.4"',
        '"0.8.0-alpha.2.5"',
        '"0.8.0-alpha.3"',
        "UpdateChannel::Development",
    ):
        if token not in update_tests:
            fail(f"v0.8 alpha.2.5 update ordering/default coverage missing: {token}")

    readme = read("README.md")
    changelog = read("CHANGELOG.md")
    roadmap = read("ROADMAP.md")

    for token in (
        "## v0.8.0-alpha.2.5 — Settings Alignment Hotfix",
        "0.8.0.25",
    ):
        if token not in readme:
            fail(f"v0.8 alpha.2.5 README contract missing: {token}")

    for token in (
        "## 0.8.0-alpha.2.5",
        "0.8.0.25",
    ):
        if token not in changelog:
            fail(f"v0.8 alpha.2.5 changelog contract missing: {token}")

    for token in (
        "v0.8.0-alpha.2.5",
        "v0.8.0-alpha.3",
    ):
        if token not in roadmap:
            fail(f"v0.8 alpha.2.5 roadmap contract missing: {token}")

    print(
        "v0.8.0-alpha.2.5 Settings alignment contract verified:",
        "| settings=8 commands=2 usage=1 provider-cache=2",
        "| numeric order combo=100",
        "| placement combos=180",
        "| hotkey capture baseline unified",
        "| reset-all lower-right",
        "| Appearance combos vertically corrected",
        "| alpha.2.4 behavior preserved",
    )
    raise SystemExit(0)

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
