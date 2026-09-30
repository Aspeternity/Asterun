cmake_minimum_required(VERSION 3.24)

project(Asterun VERSION 1.0.2 LANGUAGES C CXX)

file(
    STRINGS
    "${CMAKE_CURRENT_SOURCE_DIR}/VERSION"
    ASTERUN_VERSION
    LIMIT_COUNT 1
)
string(STRIP "${ASTERUN_VERSION}" ASTERUN_VERSION)

if(ASTERUN_VERSION STREQUAL "")
    message(FATAL_ERROR "VERSION must contain the Asterun version.")
endif()

file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated")
configure_file(
    src/core/Version.hpp.in
    "${CMAKE_CURRENT_BINARY_DIR}/generated/Version.hpp"
    @ONLY
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

set(JSON_BuildTests OFF CACHE INTERNAL "")
set(JSON_Install OFF CACHE INTERNAL "")

FetchContent_Declare(
    nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
    URL_HASH SHA256=d6c65aca6b1ed68e7a182f4757257b107ae403032760ed6ef121c9d55e81757d
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(nlohmann_json)

# cpp-pinyin is intentionally NOT added through its upstream CMake project.
# On Windows 1.0.2 can still produce cpp-pinyin.dll despite the parent trying
# to force BUILD_SHARED_LIBS=OFF. Asterun therefore populates the source
# tree only and owns an explicit STATIC target. This guarantees the portable
# EXE never has a runtime dependency on cpp-pinyin.dll.
FetchContent_Declare(
    cpp_pinyin
    URL https://github.com/wolfgitpr/cpp-pinyin/archive/refs/tags/1.0.2.tar.gz
    URL_HASH SHA256=a7b869a1bfdeff066257f3ae903638a1913a0fb17f40fb72433a1a084259b892
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SOURCE_SUBDIR _altrun_source_only
)
FetchContent_MakeAvailable(cpp_pinyin)

# Keep the ZIP reader separate from the dependency's optional tools/tests.
FetchContent_Declare(
    miniz
    URL https://github.com/richgel999/miniz/archive/refs/tags/3.1.2.tar.gz
    URL_HASH SHA256=98468f8924934b723276680f85238b6c78bf1f8b49b4459cc9b7214a20e2e9fb
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SOURCE_SUBDIR _altrun_source_only
)
FetchContent_MakeAvailable(miniz)
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated/miniz")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/generated/miniz/miniz_export.h"
    "#pragma once\n#define MINIZ_EXPORT\n")
add_library(miniz_static STATIC
    "${miniz_SOURCE_DIR}/miniz.c"
    "${miniz_SOURCE_DIR}/miniz_zip.c"
    "${miniz_SOURCE_DIR}/miniz_tinfl.c"
    "${miniz_SOURCE_DIR}/miniz_tdef.c"
)
target_include_directories(miniz_static PUBLIC
    "${miniz_SOURCE_DIR}"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/miniz")

add_library(altrun_archive STATIC src/core/ArchiveExtractor.cpp)
target_include_directories(altrun_archive PUBLIC src)
target_link_libraries(altrun_archive PUBLIC miniz_static)

file(
    GLOB_RECURSE
    CPP_PINYIN_SOURCES
    CONFIGURE_DEPENDS
    "${cpp_pinyin_SOURCE_DIR}/src/*.cpp"
)

add_library(cpp_pinyin_static STATIC
    ${CPP_PINYIN_SOURCES}
)

target_include_directories(
    cpp_pinyin_static
    PUBLIC
        "${cpp_pinyin_SOURCE_DIR}/include"
    PRIVATE
        "${cpp_pinyin_SOURCE_DIR}/src"
        "${cpp_pinyin_SOURCE_DIR}/src/toneUtil"
)

target_compile_definitions(
    cpp_pinyin_static
    PUBLIC CPP_PINYIN_STATIC
)

if(MSVC)
    target_compile_options(
        cpp_pinyin_static
        PRIVATE /utf-8 /EHsc
    )
else()
    target_compile_options(
        cpp_pinyin_static
        PRIVATE -Wall -Wextra -Wpedantic
    )
endif()

add_library(altrun_core STATIC
    src/core/ClassicBehavior.cpp
    src/core/ClipboardAction.cpp
    src/core/CommandTemplate.cpp
    src/core/ContextActions.cpp
    src/core/ShortcutEditorModel.cpp
    src/core/EverythingBootstrapPolicy.cpp
    src/core/EverythingIpcProtocol.cpp
    src/core/ExplorerContextSelection.cpp
    src/core/HotkeyRegistry.cpp
    src/core/LauncherActionPolicy.cpp
    src/core/LauncherResult.cpp
    src/core/LaunchCatalog.cpp
    src/core/LaunchSurface.cpp
    src/core/LaunchCandidate.cpp
    src/core/LaunchRole.cpp
    src/core/RelevancePolicy.cpp
    src/core/ResultMerger.cpp
    src/core/ResultRanking.cpp
    src/core/RuntimeInput.cpp
    src/core/UpdatePolicy.cpp
    src/core/PinyinSearch.cpp
    src/core/SearchEngine.cpp
    src/core/SettingsLayout.cpp
    src/core/WebAction.cpp
)

target_include_directories(altrun_core PUBLIC src)
target_link_libraries(altrun_core PUBLIC cpp_pinyin_static)

add_library(altrun_config STATIC
    src/core/CommandMerge.cpp
    src/core/ConfigIO.cpp
    src/core/ProviderCache.cpp
    src/core/ProviderIndexPolicy.cpp
    src/core/Settings.cpp
    src/core/TextCodec.cpp
    src/core/UpdateManifest.cpp
    src/core/UsageStore.cpp
    src/core/UserCommandStore.cpp
)

target_include_directories(
    altrun_config
    PUBLIC
        src
        "${CMAKE_CURRENT_BINARY_DIR}/generated"
)
target_link_libraries(
    altrun_config
    PUBLIC
        altrun_core
        nlohmann_json::nlohmann_json
)

foreach(target altrun_core altrun_config)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8 /EHsc)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endforeach()

function(altrun_copy_pinyin_runtime target)
    add_custom_command(
        TARGET ${target}
        POST_BUILD
        COMMAND
            ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:${target}>/dict/mandarin"
        COMMAND
            ${CMAKE_COMMAND} -E copy_directory
            "${cpp_pinyin_SOURCE_DIR}/res/dict/mandarin"
            "$<TARGET_FILE_DIR:${target}>/dict/mandarin"
        COMMAND
            ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:${target}>/third_party"
        COMMAND
            ${CMAKE_COMMAND} -E copy_if_different
            "${cpp_pinyin_SOURCE_DIR}/LICENSE"
            "$<TARGET_FILE_DIR:${target}>/third_party/cpp-pinyin-LICENSE.txt"
        COMMAND
            ${CMAKE_COMMAND} -E copy_if_different
            "${miniz_SOURCE_DIR}/LICENSE"
            "$<TARGET_FILE_DIR:${target}>/third_party/miniz-LICENSE.txt"
        VERBATIM
    )
endfunction()

if(WIN32)
    enable_language(RC)

    add_executable(Asterun WIN32
        src/main.cpp
        src/app/App.cpp
        src/app/AppUpdate.cpp
        src/core/AppPathsProvider.cpp
        src/core/CommandStore.cpp
        src/core/EverythingProvider.cpp
        src/core/Localization.cpp
        src/core/PackagedAppProvider.cpp
        src/core/PathProvider.cpp
        src/core/ProviderRegistry.cpp
        src/core/StartMenuProvider.cpp
        src/platform/EverythingBootstrapper.cpp
        src/platform/EverythingIpcClient.cpp
        src/platform/Hotkey.cpp
        src/platform/LaunchTargetInspector.cpp
        src/platform/SecureArchive.cpp
        src/platform/SecureElevation.cpp
        src/platform/ShellActions.cpp
        src/platform/UpdateManager.cpp
        src/platform/WinClipboard.cpp
        src/platform/WindowsContext.cpp
        src/platform/WinUtil.cpp
        src/ui/LauncherWindow.cpp
        src/ui/LauncherWindowLayout.cpp
        src/ui/Feedback.cpp
        src/ui/Feedback.rc
        src/ui/SettingsWindow.cpp
        src/ui/SettingsWindowLayout.cpp
        src/ui/ShortcutEditorDialog.cpp
        src/ui/ShortcutManagerWindow.cpp
        src/ui/ShortcutPathConverterDialog.cpp
        src/ui/TopLevelWindowPresentation.cpp
        src/ui/UiComboBox.cpp
        src/ui/UiListView.cpp
        src/ui/UiTypography.cpp
        src/resources.rc
        src/app.manifest
    )

    # Compile the shared application implementation once for the product and
    # its real-window integration test. Entrypoints and resources stay on the
    # individual executables.
    get_target_property(altrun_app_sources Asterun SOURCES)
    list(REMOVE_ITEM altrun_app_sources
        src/main.cpp src/resources.rc src/ui/Feedback.rc src/app.manifest)
    set_property(TARGET Asterun PROPERTY SOURCES
        src/main.cpp src/resources.rc src/ui/Feedback.rc src/app.manifest)
    add_library(altrun_windows_runtime STATIC ${altrun_app_sources})
    target_include_directories(altrun_windows_runtime PRIVATE
        src "${CMAKE_CURRENT_BINARY_DIR}/generated")
    target_link_libraries(altrun_windows_runtime PRIVATE
        altrun_config altrun_archive)
    target_compile_definitions(altrun_windows_runtime PRIVATE
        UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX
        _WIN32_WINNT=0x0A00 WINVER=0x0A00)
    if(MSVC)
        target_compile_options(altrun_windows_runtime PRIVATE
            /W4 /permissive- /utf-8 /EHsc)
    endif()

    target_include_directories(
        Asterun PRIVATE
        src
        "${CMAKE_CURRENT_BINARY_DIR}/generated"
    )
    target_link_libraries(
        Asterun PRIVATE
        altrun_windows_runtime
        altrun_core
        altrun_config
        altrun_archive
        shell32
        ole32
        oleaut32
        comctl32
        winmm
        comdlg32
        advapi32
        user32
        gdi32
        msimg32
        dwmapi
        uxtheme
        uuid
        version
        msi
        winhttp
        bcrypt
        wintrust
        crypt32
    )
    target_compile_definitions(
        Asterun PRIVATE
        UNICODE
        _UNICODE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        _WIN32_WINNT=0x0A00
        WINVER=0x0A00
    )

    if(MSVC)
        target_compile_options(Asterun PRIVATE /W4 /permissive- /utf-8 /EHsc)
        target_link_options(Asterun PRIVATE /SUBSYSTEM:WINDOWS)
    endif()

    set_target_properties(Asterun PROPERTIES
        OUTPUT_NAME "Asterun"
    )

    altrun_copy_pinyin_runtime(Asterun)

    add_executable(
        AsterunUpdater
        WIN32
        src/updater/UpdaterMain.cpp
        src/updater/UpdaterTransaction.cpp
        src/updater_resources.rc
        src/platform/SecureArchive.cpp
        src/platform/SecureElevation.cpp
    )

    target_compile_definitions(
        AsterunUpdater
        PRIVATE
            UNICODE
            _UNICODE
            WIN32_LEAN_AND_MEAN
            NOMINMAX
            _WIN32_WINNT=0x0A00
            WINVER=0x0A00
    )

    target_link_libraries(
        AsterunUpdater
        PRIVATE
            altrun_archive
            advapi32
            bcrypt
            ole32
            oleaut32
            shell32
            user32
    )

    if(MSVC)
        target_compile_options(
            AsterunUpdater
            PRIVATE
                /W4
                /permissive-
                /utf-8
                /EHsc
        )
        target_link_options(
            AsterunUpdater
            PRIVATE
                /SUBSYSTEM:WINDOWS
        )
    endif()

    set_target_properties(
        AsterunUpdater
        PROPERTIES
            OUTPUT_NAME
                "Update"
    )

    add_executable(
        AsterunUninstaller
        WIN32
        src/uninstaller/UninstallMain.cpp
        src/uninstaller/uninstaller.manifest
        src/uninstaller_resources.rc
        src/platform/SecureElevation.cpp
        src/ui/Feedback.cpp
        src/ui/Feedback.rc
    )

    target_include_directories(AsterunUninstaller PRIVATE src)

    target_compile_definitions(
        AsterunUninstaller
        PRIVATE
            UNICODE
            _UNICODE
            WIN32_LEAN_AND_MEAN
            NOMINMAX
            _WIN32_WINNT=0x0A00
            WINVER=0x0A00
    )

    target_link_libraries(
        AsterunUninstaller
        PRIVATE
            advapi32
            bcrypt
            shell32
            shlwapi
            ole32
            oleaut32
            rstrtmgr
            comctl32
            winmm
            user32
    )

    if(MSVC)
        target_compile_options(
            AsterunUninstaller
            PRIVATE
                /W4
                /permissive-
                /utf-8
                /EHsc
        )
        target_link_options(
            AsterunUninstaller
            PRIVATE
                /SUBSYSTEM:WINDOWS
        )
    endif()

    set_target_properties(
        AsterunUninstaller
        PROPERTIES
            OUTPUT_NAME
                "Uninstall"
    )
endif()

include(CTest)

function(altrun_enable_test_assertions target)
    # CI runs test binaries in Release configuration. Keep assert() active so
    # a green run cannot be produced merely because NDEBUG removed checks.
    if(MSVC)
        target_compile_options(${target} PRIVATE /UNDEBUG)
    else()
        target_compile_options(${target} PRIVATE -UNDEBUG)
    endif()
endfunction()

if(BUILD_TESTING)
    add_executable(archive_extractor_tests tests/ArchiveExtractorTests.cpp)
    target_link_libraries(archive_extractor_tests PRIVATE altrun_archive)
    altrun_enable_test_assertions(archive_extractor_tests)
    add_test(NAME archive_extractor_tests COMMAND archive_extractor_tests)

    add_executable(windows_command_line_tests tests/WindowsCommandLineTests.cpp)
    target_include_directories(windows_command_line_tests PRIVATE src)
    altrun_enable_test_assertions(windows_command_line_tests)
    add_test(NAME windows_command_line_tests COMMAND windows_command_line_tests)

    add_executable(
        update_runtime_tests
        tests/UpdateRuntimeTests.cpp
        src/updater/UpdaterTransaction.cpp
    )
    target_include_directories(
        update_runtime_tests
        PRIVATE src
    )
    if(WIN32)
        target_sources(
            update_runtime_tests
            PRIVATE
                src/platform/SecureArchive.cpp
        )
        target_link_libraries(
            update_runtime_tests
            PRIVATE
                altrun_archive
                bcrypt
                ole32
                oleaut32
                shell32
        )
    endif()
    altrun_enable_test_assertions(
        update_runtime_tests
    )
    add_test(
        NAME update_runtime_tests
        COMMAND update_runtime_tests
    )

    add_executable(
        update_policy_tests
        tests/UpdatePolicyTests.cpp
    )
    target_link_libraries(
        update_policy_tests
        PRIVATE altrun_config
    )
    target_include_directories(
        update_policy_tests
        PRIVATE
            src
            "${CMAKE_CURRENT_BINARY_DIR}/generated"
    )
    altrun_enable_test_assertions(
        update_policy_tests
    )
    add_test(
        NAME update_policy_tests
        COMMAND update_policy_tests
    )

    add_executable(
        ui_foundation_tests
        tests/UiFoundationTests.cpp
    )
    target_include_directories(
        ui_foundation_tests
        PRIVATE src
    )
    altrun_enable_test_assertions(
        ui_foundation_tests
    )
    add_test(
        NAME ui_foundation_tests
        COMMAND ui_foundation_tests
    )

    add_executable(
        classic_behavior_tests
        tests/ClassicBehaviorTests.cpp
    )
    target_link_libraries(
        classic_behavior_tests
        PRIVATE altrun_core
    )
    target_include_directories(
        classic_behavior_tests
        PRIVATE src
    )
    altrun_enable_test_assertions(
        classic_behavior_tests
    )
    add_test(
        NAME classic_behavior_tests
        COMMAND classic_behavior_tests
    )

    add_executable(
        launch_surface_tests
        tests/LaunchSurfaceTests.cpp
    )
    target_link_libraries(
        launch_surface_tests
        PRIVATE altrun_core
    )
    target_include_directories(
        launch_surface_tests
        PRIVATE src
    )
    altrun_enable_test_assertions(
        launch_surface_tests
    )
    add_test(
        NAME launch_surface_tests
        COMMAND launch_surface_tests
    )

    add_executable(
        launch_candidate_tests
        tests/LaunchCandidateTests.cpp
    )
    target_link_libraries(
        launch_candidate_tests
        PRIVATE altrun_core
    )
    target_include_directories(
        launch_candidate_tests
        PRIVATE src
    )
    altrun_enable_test_assertions(
        launch_candidate_tests
    )
    add_test(
        NAME launch_candidate_tests
        COMMAND launch_candidate_tests
    )

    add_executable(
        launch_catalog_tests
        tests/LaunchCatalogTests.cpp
    )
    target_link_libraries(
        launch_catalog_tests
        PRIVATE altrun_core
    )
    target_include_directories(
        launch_catalog_tests
        PRIVATE src
    )
    altrun_enable_test_assertions(
        launch_catalog_tests
    )
    add_test(
        NAME launch_catalog_tests
        COMMAND launch_catalog_tests
    )

    add_executable(
        launch_role_tests
        tests/LaunchRoleTests.cpp
    )
    target_link_libraries(
        launch_role_tests
        PRIVATE altrun_core
    )
    target_include_directories(
        launch_role_tests
        PRIVATE src
    )
    altrun_enable_test_assertions(
        launch_role_tests
    )
    add_test(
        NAME launch_role_tests
        COMMAND launch_role_tests
    )

    add_executable(
        provider_index_policy_tests
        tests/ProviderIndexPolicyTests.cpp
    )
    target_link_libraries(
        provider_index_policy_tests
        PRIVATE altrun_config
    )
    target_include_directories(
        provider_index_policy_tests
        PRIVATE src
    )
    altrun_enable_test_assertions(
        provider_index_policy_tests
    )
    add_test(
        NAME provider_index_policy_tests
        COMMAND provider_index_policy_tests
    )

    add_executable(search_engine_tests tests/SearchEngineTests.cpp)
    target_link_libraries(search_engine_tests PRIVATE altrun_core)
    target_include_directories(search_engine_tests PRIVATE src)
    altrun_enable_test_assertions(search_engine_tests)
    altrun_copy_pinyin_runtime(search_engine_tests)
    add_test(NAME search_engine_tests COMMAND search_engine_tests)

    add_executable(
        everything_bootstrap_policy_tests
        tests/EverythingBootstrapPolicyTests.cpp
    )
    target_link_libraries(
        everything_bootstrap_policy_tests
        PRIVATE altrun_core
    )
    target_include_directories(
        everything_bootstrap_policy_tests
        PRIVATE src
    )
    altrun_enable_test_assertions(
        everything_bootstrap_policy_tests
    )
    add_test(
        NAME everything_bootstrap_policy_tests
        COMMAND everything_bootstrap_policy_tests
    )

    add_executable(
        everything_ipc_protocol_tests
        tests/EverythingIpcProtocolTests.cpp
    )
    target_link_libraries(
        everything_ipc_protocol_tests
        PRIVATE altrun_core
    )
    target_include_directories(
        everything_ipc_protocol_tests
        PRIVATE src
    )
    altrun_enable_test_assertions(
        everything_ipc_protocol_tests
    )
    add_test(
        NAME everything_ipc_protocol_tests
        COMMAND everything_ipc_protocol_tests
    )

    add_executable(
        result_merger_tests
        tests/ResultMergerTests.cpp
    )
    target_link_libraries(
        result_merger_tests
        PRIVATE altrun_core
    )
    target_include_directories(
        result_merger_tests
        PRIVATE src
    )
    altrun_enable_test_assertions(
        result_merger_tests
    )
    add_test(
        NAME result_merger_tests
        COMMAND result_merger_tests
    )

    add_executable(
        result_ranking_tests
        tests/ResultRankingTests.cpp
    )
    target_link_libraries(
        result_ranking_tests
        PRIVATE altrun_core
    )
    target_include_directories(
        result_ranking_tests
        PRIVATE src
    )
    altrun_enable_test_assertions(
        result_ranking_tests
    )
    add_test(
        NAME result_ranking_tests
        COMMAND result_ranking_tests
    )

    add_executable(
        web_action_tests
        tests/WebActionTests.cpp
    )
    target_link_libraries(
        web_action_tests
        PRIVATE altrun_core
    )
    target_include_directories(
        web_action_tests
        PRIVATE src
    )
    altrun_enable_test_assertions(
        web_action_tests
    )
    add_test(
        NAME web_action_tests
        COMMAND web_action_tests
    )

    add_executable(
        clipboard_action_tests
        tests/ClipboardActionTests.cpp
    )
    target_link_libraries(
        clipboard_action_tests
        PRIVATE altrun_core
    )
    target_include_directories(
        clipboard_action_tests
        PRIVATE src
    )
    altrun_enable_test_assertions(
        clipboard_action_tests
    )
    add_test(
        NAME clipboard_action_tests
        COMMAND clipboard_action_tests
    )

    add_executable(
        command_template_tests
        tests/CommandTemplateTests.cpp
    )
    target_link_libraries(
        command_template_tests
        PRIVATE altrun_core
    )
    target_include_directories(
        command_template_tests
        PRIVATE src
    )
    altrun_enable_test_assertions(
        command_template_tests
    )
    add_test(
        NAME command_template_tests
        COMMAND command_template_tests
    )

    add_executable(
        shortcut_editor_model_tests
        tests/ShortcutEditorModelTests.cpp
    )
    target_link_libraries(
        shortcut_editor_model_tests
        PRIVATE altrun_core
    )
    target_include_directories(
        shortcut_editor_model_tests
        PRIVATE src
    )
    altrun_enable_test_assertions(
        shortcut_editor_model_tests
    )
    add_test(
        NAME shortcut_editor_model_tests
        COMMAND shortcut_editor_model_tests
    )

    add_executable(
        runtime_input_tests
        tests/RuntimeInputTests.cpp
    )
    target_link_libraries(
        runtime_input_tests
        PRIVATE altrun_core
    )
    target_include_directories(
        runtime_input_tests
        PRIVATE src
    )
    altrun_enable_test_assertions(
        runtime_input_tests
    )
    add_test(
        NAME runtime_input_tests
        COMMAND runtime_input_tests
    )

    add_executable(
        context_actions_tests
        tests/ContextActionsTests.cpp
    )
    target_link_libraries(
        context_actions_tests
        PRIVATE altrun_core
    )
    target_include_directories(
        context_actions_tests
        PRIVATE src
    )
    altrun_enable_test_assertions(
        context_actions_tests
    )
    add_test(
        NAME context_actions_tests
        COMMAND context_actions_tests
    )

    add_executable(
        launcher_action_policy_tests
        tests/LauncherActionPolicyTests.cpp
    )
    target_link_libraries(
        launcher_action_policy_tests
        PRIVATE altrun_core
    )
    target_include_directories(
        launcher_action_policy_tests
        PRIVATE src
    )
    altrun_enable_test_assertions(
        launcher_action_policy_tests
    )
    add_test(
        NAME launcher_action_policy_tests
        COMMAND launcher_action_policy_tests
    )

    add_executable(
        explorer_context_selection_tests
        tests/ExplorerContextSelectionTests.cpp
    )
    target_link_libraries(
        explorer_context_selection_tests
        PRIVATE altrun_core
    )
    target_include_directories(
        explorer_context_selection_tests
        PRIVATE src
    )
    altrun_enable_test_assertions(
        explorer_context_selection_tests
    )
    add_test(
        NAME explorer_context_selection_tests
        COMMAND explorer_context_selection_tests
    )

    add_executable(config_core_tests tests/ConfigCoreTests.cpp)
    target_link_libraries(config_core_tests PRIVATE altrun_config)
    target_include_directories(config_core_tests PRIVATE src)
    altrun_enable_test_assertions(config_core_tests)
    add_test(NAME config_core_tests COMMAND config_core_tests)

    add_executable(
        user_command_schema_tests
        tests/UserCommandSchemaTests.cpp
    )
    target_link_libraries(
        user_command_schema_tests
        PRIVATE altrun_config
    )
    target_include_directories(
        user_command_schema_tests
        PRIVATE src
    )
    altrun_enable_test_assertions(
        user_command_schema_tests
    )
    add_test(
        NAME user_command_schema_tests
        COMMAND user_command_schema_tests
    )

    add_executable(
        shortcut_compatibility_tests
        tests/ShortcutCompatibilityTests.cpp
    )
    target_link_libraries(
        shortcut_compatibility_tests
        PRIVATE altrun_config
    )
    target_include_directories(
        shortcut_compatibility_tests
        PRIVATE src
    )
    altrun_enable_test_assertions(
        shortcut_compatibility_tests
    )
    add_test(
        NAME shortcut_compatibility_tests
        COMMAND shortcut_compatibility_tests
    )

    add_executable(
        user_command_path_update_tests
        tests/UserCommandPathUpdateTests.cpp
    )
    target_link_libraries(
        user_command_path_update_tests
        PRIVATE altrun_config
    )
    target_include_directories(
        user_command_path_update_tests
        PRIVATE src
    )
    altrun_enable_test_assertions(
        user_command_path_update_tests
    )
    add_test(
        NAME user_command_path_update_tests
        COMMAND user_command_path_update_tests
    )

    add_executable(
        upgrade_matrix_tests
        tests/UpgradeMatrixTests.cpp
    )
    target_link_libraries(
        upgrade_matrix_tests
        PRIVATE altrun_config
    )
    target_include_directories(
        upgrade_matrix_tests
        PRIVATE src
    )
    altrun_enable_test_assertions(
        upgrade_matrix_tests
    )
    add_test(
        NAME upgrade_matrix_tests
        COMMAND upgrade_matrix_tests
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/upgrade"
    )

    add_executable(
        desktop_validation_tests
        tests/DesktopValidationTests.cpp
    )
    target_link_libraries(
        desktop_validation_tests
        PRIVATE altrun_core
    )
    target_include_directories(
        desktop_validation_tests
        PRIVATE src
    )
    altrun_enable_test_assertions(
        desktop_validation_tests
    )
    add_test(
        NAME desktop_validation_tests
        COMMAND desktop_validation_tests
    )

    add_executable(command_merge_tests tests/CommandMergeTests.cpp)
    target_link_libraries(command_merge_tests PRIVATE altrun_config)
    target_include_directories(command_merge_tests PRIVATE src)
    altrun_enable_test_assertions(command_merge_tests)
    add_test(NAME command_merge_tests COMMAND command_merge_tests)

    add_executable(
        hotkey_registry_tests
        tests/HotkeyRegistryTests.cpp
    )
    target_link_libraries(
        hotkey_registry_tests
        PRIVATE altrun_core
    )
    target_include_directories(
        hotkey_registry_tests
        PRIVATE src
    )
    altrun_enable_test_assertions(
        hotkey_registry_tests
    )
    add_test(
        NAME hotkey_registry_tests
        COMMAND hotkey_registry_tests
    )

    if(WIN32)
        # Exercise the same compiled application implementation as the product.
        get_target_property(window_runtime_libraries Asterun LINK_LIBRARIES)
        get_target_property(window_runtime_includes Asterun INCLUDE_DIRECTORIES)
        get_target_property(window_runtime_definitions Asterun COMPILE_DEFINITIONS)
        add_executable(window_presentation_runtime_tests
            tests/WindowPresentationRuntimeTests.cpp
            src/resources.rc src/ui/Feedback.rc src/app.manifest)
        target_include_directories(window_presentation_runtime_tests PRIVATE ${window_runtime_includes})
        target_link_libraries(window_presentation_runtime_tests PRIVATE ${window_runtime_libraries})
        target_compile_definitions(window_presentation_runtime_tests PRIVATE ${window_runtime_definitions})
        if(MSVC)
            target_compile_options(window_presentation_runtime_tests PRIVATE /W4 /permissive- /utf-8 /EHsc)
        endif()
        altrun_enable_test_assertions(window_presentation_runtime_tests)
        add_test(NAME window_presentation_runtime_tests COMMAND window_presentation_runtime_tests)
        set_tests_properties(window_presentation_runtime_tests PROPERTIES TIMEOUT 30 RUN_SERIAL TRUE)

        # Fault injection stays in the test translation units; the product has
        # no test switches. Other application objects are reused from runtime.
        add_executable(app_lifecycle_runtime_tests
            tests/AppLifecycleRuntimeTests.cpp
            src/resources.rc src/ui/Feedback.rc src/app.manifest)
        target_include_directories(app_lifecycle_runtime_tests PRIVATE ${window_runtime_includes})
        target_link_libraries(app_lifecycle_runtime_tests PRIVATE ${window_runtime_libraries})
        target_compile_definitions(app_lifecycle_runtime_tests PRIVATE ${window_runtime_definitions})

        add_executable(launcher_resource_runtime_tests
            tests/LauncherResourceRuntimeTests.cpp
            src/resources.rc src/ui/Feedback.rc src/app.manifest)
        target_include_directories(launcher_resource_runtime_tests PRIVATE ${window_runtime_includes})
        target_link_libraries(launcher_resource_runtime_tests PRIVATE ${window_runtime_libraries} psapi)
        target_compile_definitions(launcher_resource_runtime_tests PRIVATE ${window_runtime_definitions})

        add_executable(ui_listview_failure_tests
            tests/UiListViewFailureTests.cpp src/app.manifest)
        target_include_directories(ui_listview_failure_tests PRIVATE src)
        target_link_libraries(ui_listview_failure_tests PRIVATE user32 gdi32 comctl32 uxtheme)
        target_compile_definitions(ui_listview_failure_tests PRIVATE ${window_runtime_definitions})
        foreach(target app_lifecycle_runtime_tests ui_listview_failure_tests launcher_resource_runtime_tests)
            if(MSVC)
                target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8 /EHsc)
            endif()
            altrun_enable_test_assertions(${target})
            add_test(NAME ${target} COMMAND ${target})
            set_tests_properties(${target} PROPERTIES TIMEOUT 30 RUN_SERIAL TRUE)
        endforeach()

        add_executable(uninstall_runtime_tests tests/UninstallRuntimeTests.cpp src/platform/SecureElevation.cpp src/ui/Feedback.cpp)
        get_target_property(uninstall_libraries AsterunUninstaller LINK_LIBRARIES)
        get_target_property(uninstall_definitions AsterunUninstaller COMPILE_DEFINITIONS)
        target_link_libraries(uninstall_runtime_tests PRIVATE ${uninstall_libraries})
        target_compile_definitions(uninstall_runtime_tests PRIVATE ${uninstall_definitions})
        target_include_directories(uninstall_runtime_tests PRIVATE src)
        if(MSVC)
            target_compile_options(uninstall_runtime_tests PRIVATE /W4 /permissive- /utf-8 /EHsc)
        endif()
        altrun_enable_test_assertions(uninstall_runtime_tests)
        add_test(NAME uninstall_runtime_tests COMMAND uninstall_runtime_tests)
        set_tests_properties(uninstall_runtime_tests PROPERTIES TIMEOUT 60 RUN_SERIAL TRUE)

        add_executable(feedback_runtime_tests
            tests/FeedbackRuntimeTests.cpp
            src/ui/Feedback.cpp
            src/ui/Feedback.rc
        )
        target_include_directories(feedback_runtime_tests PRIVATE src)
        target_link_libraries(feedback_runtime_tests PRIVATE user32 comctl32 winmm)
        target_compile_definitions(feedback_runtime_tests PRIVATE
            UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX
            _WIN32_WINNT=0x0A00 WINVER=0x0A00)
        if(MSVC)
            target_compile_options(feedback_runtime_tests PRIVATE /W4 /permissive- /utf-8 /EHsc)
        endif()
        altrun_enable_test_assertions(feedback_runtime_tests)
        add_test(NAME feedback_runtime_tests COMMAND feedback_runtime_tests)
        set_tests_properties(feedback_runtime_tests PROPERTIES TIMEOUT 15)

        add_executable(windows_provider_smoke_tests
            tests/WindowsProviderSmokeTests.cpp
            src/core/AppPathsProvider.cpp
            src/core/PackagedAppProvider.cpp
            src/core/PathProvider.cpp
            src/core/ProviderRegistry.cpp
            src/core/StartMenuProvider.cpp
            src/platform/LaunchTargetInspector.cpp
            src/platform/WinUtil.cpp
        )

        target_include_directories(
            windows_provider_smoke_tests
            PRIVATE src
        )

        target_link_libraries(
            windows_provider_smoke_tests
            PRIVATE
                altrun_config
                shell32
                ole32
                advapi32
                uuid
                version
                msi
        )

        target_compile_definitions(
            windows_provider_smoke_tests
            PRIVATE
                UNICODE
                _UNICODE
                WIN32_LEAN_AND_MEAN
                NOMINMAX
                _WIN32_WINNT=0x0A00
                WINVER=0x0A00
        )

        if(MSVC)
            target_compile_options(
                windows_provider_smoke_tests
                PRIVATE /W4 /permissive- /utf-8 /EHsc
            )
        endif()

        altrun_enable_test_assertions(
            windows_provider_smoke_tests
        )

        add_test(
            NAME windows_provider_smoke_tests
            COMMAND windows_provider_smoke_tests
        )

        add_executable(
            launch_target_inspector_tests
            tests/LaunchTargetInspectorTests.cpp
            src/platform/LaunchTargetInspector.cpp
        )

        target_include_directories(
            launch_target_inspector_tests
            PRIVATE src
        )

        target_link_libraries(
            launch_target_inspector_tests
            PRIVATE
                altrun_core
                shell32
                ole32
                uuid
                version
                msi
        )

        target_compile_definitions(
            launch_target_inspector_tests
            PRIVATE
                UNICODE
                _UNICODE
                WIN32_LEAN_AND_MEAN
                NOMINMAX
                _WIN32_WINNT=0x0A00
                WINVER=0x0A00
        )

        if(MSVC)
            target_compile_options(
                launch_target_inspector_tests
                PRIVATE
                    /W4
                    /permissive-
                    /utf-8
                    /EHsc
            )
        endif()

        altrun_enable_test_assertions(
            launch_target_inspector_tests
        )

        add_test(
            NAME launch_target_inspector_tests
            COMMAND launch_target_inspector_tests
        )

        add_executable(
            path_portability_tests
            tests/PathPortabilityTests.cpp
            src/platform/WinUtil.cpp
        )

        target_include_directories(
            path_portability_tests
            PRIVATE src
        )

        target_compile_definitions(
            path_portability_tests
            PRIVATE
                UNICODE
                _UNICODE
                WIN32_LEAN_AND_MEAN
                NOMINMAX
                _WIN32_WINNT=0x0A00
                WINVER=0x0A00
        )

        if(MSVC)
            target_compile_options(
                path_portability_tests
                PRIVATE
                    /W4
                    /permissive-
                    /utf-8
                    /EHsc
            )
        endif()

        altrun_enable_test_assertions(
            path_portability_tests
        )

        add_test(
            NAME path_portability_tests
            COMMAND path_portability_tests
        )

        add_executable(
            hotkey_tests
            tests/HotkeyTests.cpp
            src/platform/Hotkey.cpp
        )

        target_include_directories(
            hotkey_tests
            PRIVATE src
        )

        target_compile_definitions(
            hotkey_tests
            PRIVATE
                UNICODE
                _UNICODE
                WIN32_LEAN_AND_MEAN
                NOMINMAX
                _WIN32_WINNT=0x0A00
                WINVER=0x0A00
        )

        if(MSVC)
            target_compile_options(
                hotkey_tests
                PRIVATE
                    /W4
                    /permissive-
                    /utf-8
                    /EHsc
            )
        endif()

        altrun_enable_test_assertions(
            hotkey_tests
        )

        add_test(
            NAME hotkey_tests
            COMMAND hotkey_tests
        )

        add_executable(
            hotkey_runtime_tests
            tests/HotkeyRuntimeTests.cpp
        )

        target_compile_definitions(
            hotkey_runtime_tests
            PRIVATE
                UNICODE
                _UNICODE
                WIN32_LEAN_AND_MEAN
                NOMINMAX
                _WIN32_WINNT=0x0A00
                WINVER=0x0A00
        )

        if(MSVC)
            target_compile_options(
                hotkey_runtime_tests
                PRIVATE
                    /W4
                    /permissive-
                    /utf-8
                    /EHsc
            )
        endif()

        altrun_enable_test_assertions(
            hotkey_runtime_tests
        )

        add_test(
            NAME hotkey_runtime_tests
            COMMAND hotkey_runtime_tests
        )

        add_executable(
            clipboard_runtime_tests
            tests/ClipboardRuntimeTests.cpp
            src/platform/WinClipboard.cpp
        )

        target_include_directories(
            clipboard_runtime_tests
            PRIVATE src
        )

        target_link_libraries(
            clipboard_runtime_tests
            PRIVATE user32
        )

        target_compile_definitions(
            clipboard_runtime_tests
            PRIVATE
                UNICODE
                _UNICODE
                WIN32_LEAN_AND_MEAN
                NOMINMAX
                _WIN32_WINNT=0x0A00
                WINVER=0x0A00
        )

        if(MSVC)
            target_compile_options(
                clipboard_runtime_tests
                PRIVATE
                    /W4
                    /permissive-
                    /utf-8
                    /EHsc
            )
        endif()

        altrun_enable_test_assertions(
            clipboard_runtime_tests
        )

        add_test(
            NAME clipboard_runtime_tests
            COMMAND clipboard_runtime_tests
        )

        add_executable(
            windows_context_runtime_tests
            tests/WindowsContextRuntimeTests.cpp
            src/platform/WindowsContext.cpp
        )

        target_include_directories(
            windows_context_runtime_tests
            PRIVATE src
        )

        target_link_libraries(
            windows_context_runtime_tests
            PRIVATE
                altrun_core
                shell32
                ole32
                oleaut32
                user32
                uuid
        )

        target_compile_definitions(
            windows_context_runtime_tests
            PRIVATE
                UNICODE
                _UNICODE
                WIN32_LEAN_AND_MEAN
                NOMINMAX
                _WIN32_WINNT=0x0A00
                WINVER=0x0A00
        )

        if(MSVC)
            target_compile_options(
                windows_context_runtime_tests
                PRIVATE
                    /W4
                    /permissive-
                    /utf-8
                    /EHsc
            )
        endif()

        altrun_enable_test_assertions(
            windows_context_runtime_tests
        )

        add_test(
            NAME windows_context_runtime_tests
            COMMAND windows_context_runtime_tests
        )

        add_executable(
            everything_lifecycle_tests
            tests/EverythingLifecycleTests.cpp
            src/platform/EverythingBootstrapper.cpp
            src/platform/SecureArchive.cpp
            src/platform/SecureElevation.cpp
        )

        target_include_directories(
            everything_lifecycle_tests
            PRIVATE src
        )

        target_link_libraries(
            everything_lifecycle_tests
            PRIVATE
                altrun_core
                altrun_archive
                shell32
                ole32
                oleaut32
                advapi32
                user32
                uuid
                winhttp
                bcrypt
                wintrust
                crypt32
        )

        target_compile_definitions(
            everything_lifecycle_tests
            PRIVATE
                UNICODE
                _UNICODE
                WIN32_LEAN_AND_MEAN
                NOMINMAX
                _WIN32_WINNT=0x0A00
                WINVER=0x0A00
        )

        if(MSVC)
            target_compile_options(
                everything_lifecycle_tests
                PRIVATE
                    /W4
                    /permissive-
                    /utf-8
                    /EHsc
            )
        endif()

        altrun_enable_test_assertions(
            everything_lifecycle_tests
        )

        add_test(
            NAME everything_lifecycle_tests
            COMMAND everything_lifecycle_tests
        )

        # Everything-only transport: deterministic callback races plus real
        # WinHTTP loopback cancellation, independent of external network speed.
        add_executable(everything_http_transport_tests
            tests/EverythingHttpTransportTests.cpp
            src/platform/SecureArchive.cpp src/platform/SecureElevation.cpp)
        get_target_property(everything_test_libraries everything_lifecycle_tests LINK_LIBRARIES)
        get_target_property(everything_test_definitions everything_lifecycle_tests COMPILE_DEFINITIONS)
        target_link_libraries(everything_http_transport_tests PRIVATE ${everything_test_libraries})
        add_executable(everything_http_runtime_tests tests/EverythingHttpRuntimeTests.cpp)
        target_link_libraries(everything_http_runtime_tests PRIVATE winhttp ws2_32)
        foreach(target everything_http_transport_tests everything_http_runtime_tests)
            target_include_directories(${target} PRIVATE src)
            target_compile_definitions(${target} PRIVATE ${everything_test_definitions})
            if(MSVC)
                target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8 /EHsc)
            endif()
            altrun_enable_test_assertions(${target})
            add_test(NAME ${target} COMMAND ${target})
            set_tests_properties(${target} PROPERTIES TIMEOUT 30 RUN_SERIAL TRUE)
        endforeach()

        # UpdateManager retains its own fixed-read transport and policy tests.
        add_executable(update_http_transport_tests tests/UpdateHttpTransportTests.cpp
            src/platform/SecureArchive.cpp src/platform/SecureElevation.cpp)
        target_link_libraries(update_http_transport_tests PRIVATE ${everything_test_libraries} altrun_config)
        add_executable(update_http_runtime_tests tests/UpdateHttpRuntimeTests.cpp)
        target_link_libraries(update_http_runtime_tests PRIVATE winhttp ws2_32)
        foreach(target update_http_transport_tests update_http_runtime_tests)
            target_include_directories(${target} PRIVATE src)
            target_compile_definitions(${target} PRIVATE ${everything_test_definitions})
            if(MSVC)
                target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8 /EHsc)
            endif()
            altrun_enable_test_assertions(${target})
            add_test(NAME ${target} COMMAND ${target})
            set_tests_properties(${target} PROPERTIES TIMEOUT 30 RUN_SERIAL TRUE)
        endforeach()

        add_executable(
            everything_ipc_runtime_tests
            tests/EverythingIpcRuntimeTests.cpp
            src/core/EverythingProvider.cpp
        )

        target_include_directories(
            everything_ipc_runtime_tests
            PRIVATE src
        )

        target_link_libraries(
            everything_ipc_runtime_tests
            PRIVATE
                altrun_core
                user32
        )

        target_compile_definitions(
            everything_ipc_runtime_tests
            PRIVATE
                UNICODE
                _UNICODE
                WIN32_LEAN_AND_MEAN
                NOMINMAX
                _WIN32_WINNT=0x0A00
                WINVER=0x0A00
        )

        if(MSVC)
            target_compile_options(
                everything_ipc_runtime_tests
                PRIVATE
                    /W4
                    /permissive-
                    /utf-8
                    /EHsc
            )
        endif()

        altrun_enable_test_assertions(
            everything_ipc_runtime_tests
        )

        add_test(
            NAME everything_ipc_runtime_tests
            COMMAND everything_ipc_runtime_tests
        )
    endif()
endif()
