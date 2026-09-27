# src/launcher/CMakeLists.txt — Launcher nativo (x86, sin consola)

cmake_minimum_required(VERSION 3.20)

project(FusionLauncher VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    add_compile_options(/W4 /permissive- /utf-8 /Zc:__cplusplus /Zc:preprocessor)
    add_compile_options(/wd4459)
    add_compile_definitions(_WIN32_WINNT=0x0601 NTDDI_VERSION=0x06010000)
    add_compile_definitions(WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
    # /SUBSYSTEM:windows => sin consola
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:windows /ENTRY:wWinMainCRTStartup")
endif()

add_executable(FusionHP WIN32
    main.cpp
    FusionHP.rc
)

set_target_properties(FusionHP PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
