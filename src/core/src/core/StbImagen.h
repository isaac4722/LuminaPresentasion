// src/core/src/core/StbImagen.h — Include único de stb_image con la
// configuración compartida. Tanto la TU de implementación (StbImagen.cpp)
// como los consumidores de las declaraciones (PptxDirecto.cpp) incluyen
// ESTE archivo: los defines STBI_* deben ser idénticos en todas las TUs
// porque cambian las declaraciones del encabezado.
//
// Configuración: solo memoria (sin stdio), solo los formatos que
// PowerPoint incrusta por defecto (PNG/JPEG/BMP/GIF). Recortar el resto
// reduce binario y superficie de ataque.

#pragma once

#ifndef STBI_NO_STDIO
#define STBI_NO_STDIO
#endif
#ifndef STBI_NO_PSD
#define STBI_NO_PSD
#endif
#ifndef STBI_NO_TGA
#define STBI_NO_TGA
#endif
#ifndef STBI_NO_HDR
#define STBI_NO_HDR
#endif
#ifndef STBI_NO_LINEAR
#define STBI_NO_LINEAR
#endif
#ifndef STBI_NO_PIC
#define STBI_NO_PIC
#endif
#ifndef STBI_NO_PNM
#define STBI_NO_PNM
#endif
// Sin vector CRT estático de MSVC: compilar y enlazar siempre como C++.
#ifndef STBI_NO_SIMD
#define STBI_NO_SIMD
#endif

#include "stb_image.h"
