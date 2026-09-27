// src/core/include/fusion/Version.h
// Versión del núcleo. Single source of truth.

#pragma once

#include <cstdint>

#ifndef FUSION_VERSION_MAJOR
  #define FUSION_VERSION_MAJOR 1
#endif
#ifndef FUSION_VERSION_MINOR
  #define FUSION_VERSION_MINOR 0
#endif
#ifndef FUSION_VERSION_PATCH
  #define FUSION_VERSION_PATCH 0
#endif

namespace fusion {

constexpr std::uint32_t kVersionMajor = FUSION_VERSION_MAJOR;
constexpr std::uint32_t kVersionMinor = FUSION_VERSION_MINOR;
constexpr std::uint32_t kVersionPatch = FUSION_VERSION_PATCH;

constexpr std::uint32_t kAhpVersionActual   = 1;  // formato ahp.v1
constexpr std::uint32_t kIpcVersionActual   = 1;  // protocolo ipc.v1

// Devuelve "1.0.0"
const char* VersionString();

// Devuelve "ahp.v1"
const char* AhpVersionString();

// Devuelve "ipc.v1"
const char* IpcVersionString();

} // namespace fusion
