# src/core/CMakeLists.txt — Núcleo nativo C++17 de FUSION-HP
# Compila dual x86+x64 con MSVC, /MT (CRT estática), sin dependencias de red.

cmake_minimum_required(VERSION 3.20)

project(FusionCore
    VERSION 1.0.0
    DESCRIPTION "Núcleo nativo de FUSION-HP"
    LANGUAGES C CXX)

# ---------------------------------------------------------------------------
# Opciones
# ---------------------------------------------------------------------------
option(FUSION_STATIC_CRT   "Usar CRT estática (/MT)"                       ON)
option(FUSION_BUILD_TESTS  "Compilar tests con doctest"                    OFF)
option(FUSION_BUILD_LAUNCHER "Construir el launcher también"               OFF)

# ---------------------------------------------------------------------------
# Configuración de MSVC
# ---------------------------------------------------------------------------
if(MSVC)
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)

    if(FUSION_STATIC_CRT)
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    endif()

    add_compile_options(/W4 /permissive- /utf-8 /Zc:__cplusplus /Zc:preprocessor)
    add_compile_options(/wd4127)  # conditional expression is constant (doctest)
    add_compile_options(/wd4459)  # declaration hides global
    add_compile_definitions(_WIN32_WINNT=0x0601 NTDDI_VERSION=0x06010000)  # Win7 SP1
    add_compile_definitions(WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
    add_compile_definitions(FUSION_VERSION_MAJOR=1 FUSION_VERSION_MINOR=0 FUSION_VERSION_PATCH=0)
endif()

# ---------------------------------------------------------------------------
# Rutas
# ---------------------------------------------------------------------------
set(FUSION_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)
set(FUSION_SRC_DIR     ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(FUSION_THIRD_PARTY ${CMAKE_CURRENT_SOURCE_DIR}/third_party)

# ---------------------------------------------------------------------------
# Terceros (single-header + SQLite amalgama)
# ---------------------------------------------------------------------------
add_library(fusion_third_party STATIC
    ${FUSION_THIRD_PARTY}/sqlite/sqlite3.c
)
target_include_directories(fusion_third_party PUBLIC
    ${FUSION_THIRD_PARTY}
    ${FUSION_THIRD_PARTY}/sqlite
)
# Forzar detección de lenguaje C para el target (sqlite3.c es C, no C++)
set_target_properties(fusion_third_party PROPERTIES LINKER_LANGUAGE C)
if(MSVC)
    target_compile_options(fusion_third_party PRIVATE
        /W0
        /D SQLITE_THREADSAFE=1
        /D SQLITE_DEFAULT_WAL_SYNCHRONOUS=1
        /D SQLITE_OMIT_LOAD_EXTENSION=1
        /D SQLITE_USE_URI=0
        /D SQLITE_DQS=0
        /D SQLITE_ENABLE_FTS5=1
    )
endif()

# ---------------------------------------------------------------------------
# Núcleo
# ---------------------------------------------------------------------------
add_executable(FusionCore WIN32
    ${FUSION_SRC_DIR}/main.cpp

    ${FUSION_SRC_DIR}/core/Engine.cpp
    ${FUSION_SRC_DIR}/core/Session.cpp
    ${FUSION_SRC_DIR}/core/Renderer.cpp
    ${FUSION_SRC_DIR}/core/IpcServer.cpp
    ${FUSION_SRC_DIR}/core/ProjectionWindow.cpp
    ${FUSION_SRC_DIR}/core/VideoPlayer.cpp
    ${FUSION_SRC_DIR}/core/StageView.cpp
    ${FUSION_SRC_DIR}/core/TriggerEngine.cpp
    ${FUSION_SRC_DIR}/core/Diagnostic.cpp

    ${FUSION_SRC_DIR}/data/SongDatabase.cpp
    ${FUSION_SRC_DIR}/data/BibleDatabase.cpp
    ${FUSION_SRC_DIR}/data/AhpFormat.cpp

    ${FUSION_SRC_DIR}/importers/HolyricsJson.cpp
    ${FUSION_SRC_DIR}/importers/ZefaniaXml.cpp
    ${FUSION_SRC_DIR}/importers/ESwordBib.cpp
    ${FUSION_SRC_DIR}/importers/JsonBible.cpp
    ${FUSION_SRC_DIR}/importers/TsvBible.cpp
)

target_include_directories(FusionCore PRIVATE
    ${FUSION_INCLUDE_DIR}
)

target_link_libraries(FusionCore PRIVATE
    fusion_third_party
    d2d1 dwrite dwrite_1 d2d1_1
    windowscodecs
    d3d11
    strmiids ole32 oleaut32
    gdiplus
    shlwapi
    user32 gdi32 advapi32
# spdlog se incluye como submodule cuando se necesite; por ahora el núcleo
# usa OutputDebugString + NLog en la capa gestionada.
)

# ---------------------------------------------------------------------------
# Tests nativos (doctest)
# ---------------------------------------------------------------------------
# Compila los .cpp del núcleo necesarios para los tests (sin WinMain).
if(FUSION_BUILD_TESTS)
    add_executable(fusion_tests
        ${CMAKE_SOURCE_DIR}/../../tests/native/main.cpp
        ${CMAKE_SOURCE_DIR}/../../tests/native/test_ahp_format.cpp
        ${CMAKE_SOURCE_DIR}/../../tests/native/test_importers.cpp

        ${FUSION_SRC_DIR}/Version.cpp
        ${FUSION_SRC_DIR}/data/AhpFormat.cpp
        ${FUSION_SRC_DIR}/data/SongDatabase.cpp
        ${FUSION_SRC_DIR}/data/BibleDatabase.cpp

        ${FUSION_SRC_DIR}/importers/HolyricsJson.cpp
        ${FUSION_SRC_DIR}/importers/ZefaniaXml.cpp
        ${FUSION_SRC_DIR}/importers/ESwordBib.cpp
        ${FUSION_SRC_DIR}/importers/JsonBible.cpp
        ${FUSION_SRC_DIR}/importers/TsvBible.cpp
    )
    target_include_directories(fusion_tests PRIVATE
        ${FUSION_INCLUDE_DIR}
        ${FUSION_THIRD_PARTY}
    )
    target_link_libraries(fusion_tests PRIVATE
        fusion_third_party
    )
    if(MSVC)
        target_compile_options(fusion_tests PRIVATE /W0)
        target_compile_definitions(fusion_tests PRIVATE
            WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE
            _WIN32_WINNT=0x0601 NTDDI_VERSION=0x06010000
        )
    endif()
endif()

# ---------------------------------------------------------------------------
# Output
# ---------------------------------------------------------------------------
set_target_properties(FusionCore PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
)
