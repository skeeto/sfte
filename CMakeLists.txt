cmake_minimum_required(VERSION 3.20)

project(sfte C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(NOT WIN32)
    message(FATAL_ERROR
        "This CMake build currently targets Windows (MinGW GCC). "
        "Use nob on Wayland/Linux.")
endif()

if(NOT EXISTS "${CMAKE_SOURCE_DIR}/config.c")
    message(STATUS "config.c not found, generating from config.def.win32.c")
    configure_file("${CMAKE_SOURCE_DIR}/config.def.win32.c" "${CMAKE_SOURCE_DIR}/config.c" COPYONLY)
endif()

add_executable(sfte config.c)

# Compile the dev header directly so CMake builds always track sfte_dev.h.
target_compile_definitions(sfte PRIVATE
    SFTE_DEV_ENV
    _WIN32_WINNT=0x0A00
    NTDDI_VERSION=0x0A000006
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

target_include_directories(sfte PRIVATE "${CMAKE_SOURCE_DIR}")

if(MSVC)
    target_compile_options(sfte PRIVATE /W4)
else()
    target_compile_options(sfte PRIVATE -O2 -Wall -Wextra)
endif()

target_link_libraries(sfte PRIVATE user32 gdi32 shell32 ole32 dwmapi dwrite)

option(SFTE_WIN32_CONSOLE "Build as a console-subsystem executable (debugging aid)" OFF)

# GUI subsystem, but keep the conventional main() entry point.
if(MINGW AND NOT SFTE_WIN32_CONSOLE)
    target_link_options(sfte PRIVATE -mwindows -Wl,-e,mainCRTStartup)
endif()

option(SFTE_BUILD_BENCH "Build the headless font benchmark" OFF)

if(SFTE_BUILD_BENCH)
    add_executable(sfte_bench bench/bench_font.c)
    target_compile_definitions(sfte_bench PRIVATE
        _WIN32_WINNT=0x0A00
        NTDDI_VERSION=0x0A000006
        WIN32_LEAN_AND_MEAN
        NOMINMAX
    )
    target_include_directories(sfte_bench PRIVATE "${CMAKE_SOURCE_DIR}")
    target_compile_options(sfte_bench PRIVATE -O2 -Wall -Wextra)
    target_link_libraries(sfte_bench PRIVATE user32 dwrite ole32)

    add_executable(sfte_bench_stb bench/bench_font.c)
    target_compile_definitions(sfte_bench_stb PRIVATE
        _WIN32_WINNT=0x0A00
        NTDDI_VERSION=0x0A000006
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        BENCH_STB
    )
    target_include_directories(sfte_bench_stb PRIVATE "${CMAKE_SOURCE_DIR}")
    target_compile_options(sfte_bench_stb PRIVATE -O2 -Wall -Wextra -Wno-unused-function)
    target_link_libraries(sfte_bench_stb PRIVATE user32)
endif()