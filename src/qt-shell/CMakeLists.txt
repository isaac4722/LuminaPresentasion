# src/qt-shell/CMakeLists.txt — Carcasa Qt 5.15.2 (LGPL, enlace dinámico)

cmake_minimum_required(VERSION 3.20)

project(FusionQtShell VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

if(MSVC)
    add_compile_options(/W4 /permissive- /utf-8 /Zc:__cplusplus /Zc:preprocessor)
    add_compile_definitions(_WIN32_WINNT=0x0601 NTDDI_VERSION=0x06010000)
    add_compile_definitions(WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
endif()

find_package(Qt5 5.15.2 REQUIRED COMPONENTS Core Gui Widgets)

add_executable(FusionQtShell WIN32
    main.cpp
    MainWindow.cpp
    MainWindow.h
)

target_link_libraries(FusionQtShell PRIVATE
    Qt5::Core Qt5::Gui Qt5::Widgets
)

set_target_properties(FusionQtShell PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    WIN32_EXECUTABLE TRUE
)
