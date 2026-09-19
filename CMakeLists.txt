cmake_minimum_required(VERSION 3.24)

project(ALTRunNext VERSION 0.1.7 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(altrun_core STATIC
    src/core/SearchEngine.cpp
)

target_include_directories(altrun_core PUBLIC src)

if(MSVC)
    target_compile_options(altrun_core PRIVATE /W4 /permissive- /utf-8 /EHsc)
else()
    target_compile_options(altrun_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(WIN32)
    enable_language(RC)

    add_executable(ALTRunNext WIN32
        src/main.cpp
        src/app/App.cpp
        src/core/CommandStore.cpp
        src/core/Localization.cpp
        src/core/Settings.cpp
        src/core/UsageStore.cpp
        src/platform/WinUtil.cpp
        src/ui/LauncherWindow.cpp
        src/resources.rc
        src/app.manifest
    )

    target_include_directories(ALTRunNext PRIVATE src)
    target_link_libraries(
        ALTRunNext PRIVATE
        altrun_core
        shell32
        ole32
        comctl32
        user32
        gdi32
        dwmapi
        uxtheme
    )
    target_compile_definitions(ALTRunNext PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)

    if(MSVC)
        target_compile_options(ALTRunNext PRIVATE /W4 /permissive- /utf-8 /EHsc)
        target_link_options(ALTRunNext PRIVATE /SUBSYSTEM:WINDOWS)
    endif()

    set_target_properties(ALTRunNext PROPERTIES
        OUTPUT_NAME "ALTRunNext"
    )
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(search_engine_tests tests/SearchEngineTests.cpp)
    target_link_libraries(search_engine_tests PRIVATE altrun_core)
    target_include_directories(search_engine_tests PRIVATE src)
    add_test(NAME search_engine_tests COMMAND search_engine_tests)
endif()
