# src/qt-shell/CMakeLists.txt — Carcasa Qt 5.15.2 (LGPL, enlace dinámico)

cmake_minimum_required(VERSION 3.20)

project(FusionQtShell VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

if(MSVC)
    # CRT estática: el portable y el instalador no necesitan VC redist
    # (misma regla que el núcleo y el launcher; las DLL de Qt llevan el
    # runtime dinámico copiado app-local por el CI).
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    add_compile_options(/W4 /permissive- /utf-8 /Zc:__cplusplus /Zc:preprocessor)
    add_compile_definitions(_WIN32_WINNT=0x0601 NTDDI_VERSION=0x06010000)
    add_compile_definitions(WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
endif()

find_package(Qt5 5.15.2 REQUIRED COMPONENTS Core Gui Widgets)

# ---------------------------------------------------------------------------
# Núcleo FUSION-HP (modo directo en vivo): fuentes del núcleo compiladas
# dentro del shell — PptxDirecto (lector OPC) + RenderDirecto (plan y
# rasterizador D2D offscreen) + HerenciaTemas (tema resuelto). Sin
# SQLite ni IPC: el shell muestra el paquete con EXACTAMENTE el mismo
# motor de pintado que la proyección y la exportación.
# ---------------------------------------------------------------------------
set(FUSION_CORE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../core)
set(FUSION_SRC_DIR  ${FUSION_CORE_DIR}/src)
set(FUSION_TP_DIR   ${FUSION_CORE_DIR}/third_party)

set(FUSION_CORE_FUENTES
    ${FUSION_SRC_DIR}/core/PptxDirecto.cpp
    ${FUSION_SRC_DIR}/core/StbImagen.cpp
    ${FUSION_SRC_DIR}/core/RenderDirecto.cpp
    ${FUSION_SRC_DIR}/core/HerenciaTemas.cpp
    ${FUSION_TP_DIR}/pugixml/pugixml.cpp
)

add_executable(FusionQtShell WIN32
    main.cpp
    MainWindow.cpp
    MainWindow.h
    ${FUSION_CORE_FUENTES}
)

target_include_directories(FusionQtShell PRIVATE
    ${FUSION_CORE_DIR}/include
    ${FUSION_SRC_DIR}/core
    ${FUSION_TP_DIR}
    ${FUSION_TP_DIR}/pugixml
    ${FUSION_TP_DIR}/stb
)

target_link_libraries(FusionQtShell PRIVATE
    Qt5::Core Qt5::Gui Qt5::Widgets
)

if(MSVC)
    # Mitad D2D del RenderDirecto (rasterizador offscreen) y WIC para
    # los fondos/medios.
    target_link_libraries(FusionQtShell PRIVATE d2d1 dwrite windowscodecs ole32)
    # Terceros sin ruido de warnings (mismo criterio que el núcleo).
    set_source_files_properties(${FUSION_TP_DIR}/pugixml/pugixml.cpp
        PROPERTIES COMPILE_OPTIONS "/W0")
    set_source_files_properties(${FUSION_SRC_DIR}/core/StbImagen.cpp
        PROPERTIES COMPILE_OPTIONS "/W0")
endif()

set_target_properties(FusionQtShell PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    WIN32_EXECUTABLE TRUE
)
