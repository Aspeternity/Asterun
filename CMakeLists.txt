cmake_minimum_required(VERSION 3.24)

project(ALTRunNext VERSION 0.7.0 LANGUAGES CXX)

file(
    STRINGS
    "${CMAKE_CURRENT_SOURCE_DIR}/VERSION"
    ALTRUN_VERSION
    LIMIT_COUNT 1
)
string(STRIP "${ALTRUN_VERSION}" ALTRUN_VERSION)

if(ALTRUN_VERSION STREQUAL "")
    message(FATAL_ERROR "VERSION must contain the ALTRun Next version.")
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
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(nlohmann_json)

# cpp-pinyin is intentionally NOT added through its upstream CMake project.
# On Windows 1.0.2 can still produce cpp-pinyin.dll despite the parent trying
# to force BUILD_SHARED_LIBS=OFF. ALTRun Next therefore populates the source
# tree only and owns an explicit STATIC target. This guarantees the portable
# EXE never has a runtime dependency on cpp-pinyin.dll.
FetchContent_Declare(
    cpp_pinyin
    URL https://github.com/wolfgitpr/cpp-pinyin/archive/refs/tags/1.0.2.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SOURCE_SUBDIR _altrun_source_only
)
FetchContent_MakeAvailable(cpp_pinyin)

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
    src/core/EverythingIpcProtocol.cpp
    src/core/ExplorerContextSelection.cpp
    src/core/HotkeyRegistry.cpp
    src/core/LauncherActionPolicy.cpp
    src/core/LauncherResult.cpp
    src/core/ResultMerger.cpp
    src/core/ResultRanking.cpp
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
    src/core/Settings.cpp
    src/core/TextCodec.cpp
    src/core/UsageStore.cpp
    src/core/UserCommandStore.cpp
)

target_include_directories(altrun_config PUBLIC src)
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
        VERBATIM
    )
endfunction()

if(WIN32)
    enable_language(RC)

    add_executable(ALTRunNext WIN32
        src/main.cpp
        src/app/App.cpp
        src/core/AppPathsProvider.cpp
        src/core/CommandStore.cpp
        src/core/EverythingProvider.cpp
        src/core/Localization.cpp
        src/core/PackagedAppProvider.cpp
        src/core/PathProvider.cpp
        src/core/ProviderRegistry.cpp
        src/core/StartMenuProvider.cpp
        src/platform/EverythingIpcClient.cpp
        src/platform/Hotkey.cpp
        src/platform/WinClipboard.cpp
        src/platform/WindowsContext.cpp
        src/platform/WinUtil.cpp
        src/ui/LauncherWindow.cpp
        src/ui/SettingsWindow.cpp
        src/ui/ShortcutEditorDialog.cpp
        src/ui/ShortcutManagerWindow.cpp
        src/ui/ShortcutPathConverterDialog.cpp
        src/resources.rc
        src/app.manifest
    )

    target_include_directories(
        ALTRunNext PRIVATE
        src
        "${CMAKE_CURRENT_BINARY_DIR}/generated"
    )
    target_link_libraries(
        ALTRunNext PRIVATE
        altrun_core
        altrun_config
        shell32
        ole32
        oleaut32
        comctl32
        comdlg32
        advapi32
        user32
        gdi32
        dwmapi
        uxtheme
        uuid
    )
    target_compile_definitions(
        ALTRunNext PRIVATE
        UNICODE
        _UNICODE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        _WIN32_WINNT=0x0A00
        WINVER=0x0A00
    )

    if(MSVC)
        target_compile_options(ALTRunNext PRIVATE /W4 /permissive- /utf-8 /EHsc)
        target_link_options(ALTRunNext PRIVATE /SUBSYSTEM:WINDOWS)
    endif()

    set_target_properties(ALTRunNext PROPERTIES
        OUTPUT_NAME "ALTRunNext"
    )

    altrun_copy_pinyin_runtime(ALTRunNext)
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
    add_executable(search_engine_tests tests/SearchEngineTests.cpp)
    target_link_libraries(search_engine_tests PRIVATE altrun_core)
    target_include_directories(search_engine_tests PRIVATE src)
    altrun_enable_test_assertions(search_engine_tests)
    altrun_copy_pinyin_runtime(search_engine_tests)
    add_test(NAME search_engine_tests COMMAND search_engine_tests)

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
        add_executable(windows_provider_smoke_tests
            tests/WindowsProviderSmokeTests.cpp
            src/core/AppPathsProvider.cpp
            src/core/PackagedAppProvider.cpp
            src/core/PathProvider.cpp
            src/core/ProviderRegistry.cpp
            src/core/StartMenuProvider.cpp
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
            everything_ipc_runtime_tests
            tests/EverythingIpcRuntimeTests.cpp
            src/core/EverythingProvider.cpp
            src/platform/EverythingIpcClient.cpp
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
