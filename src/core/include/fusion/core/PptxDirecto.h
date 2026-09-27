// src/core/include/fusion/core/PptxDirecto.h
// Lector directo de paquetes .pptx/.pptm (ISO/IEC-29500) — Sección 9.2,
// modo "directo": proyectar el archivo original SIN PowerPoint con el
// render propio del núcleo.
//
// Alcance v1 (decisión documentada en docs/agent/pptx_directo.md):
//  - Lee la estructura del paquete OPC: ZIP + [Content_Types].xml +
//    relaciones (_rels) + presentation.xml + ppt/slides/slideN.xml.
//  - Extrae por diapositiva, en el orden real de presentación
//    (p:sldIdLst): título (placeholder title/ctrTitle) y párrafos del
//    cuerpo en orden (a:p → a:t, incluyendo tablas y cuadros de texto).
//  - Expone el tamaño de diapositiva en EMU (p:sldSz) para que el shell
//    calcule la relación de aspecto antes de rasterizar.
//  - El inflador RFC 1951, el lector ZIP y el decodificador XML son
//    propios (cero dependencias nuevas; regla del producto).
//  - Un .pptm se LEE igual que un .pptx: leer no ejecuta nada, las
//    macros jamás se ejecutan (regla del producto).
//
// Fuera de alcance v1 (siguiente pieza del render directo): imágenes y
// formas vectoriales dentro de las diapositivas (posicionamiento EMU,
// media del paquete). El texto es lo que se proyecta con el tema actual.

#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace fusion {

// Una diapositiva del paquete, lista para el render directo.
struct DiapositivaPptx {
    int indice = 0;                     // 1-based, orden de p:sldIdLst
    std::string titulo;                 // vacío si la diapositiva no tiene
    std::vector<std::string> parrafos;  // cuerpo en orden de lectura
};

// Resultado de la lectura del paquete.
struct InfoPptx {
    bool        ok = false;
    std::string msg_error;              // vacío si ok
    std::vector<std::string> avisos;    // informativos, no bloquean
    long long   ancho_emu = 0;          // p:sldSz cx (914400 EMU = 1")
    long long   alto_emu = 0;           // p:sldSz cy
    int         total_diapositivas = 0; // == salida->size() si se pidió
};

class LectorPptx {
public:
    // Lee un paquete desde memoria (el shell puede ya tenerlo en RAM;
    // los tests lo embeben). Si `salida` no es nula, recibe las
    // diapositivas en orden. Leer no ejecuta nada del paquete.
    static InfoPptx LeerDesdeMemoria(const unsigned char* bytes, size_t tam,
                                     std::vector<DiapositivaPptx>* salida);

    // Lee un paquete desde archivo (stdio clásico, binario). Rutas no
    // ASCII en Windows: preferir LeerDesdeMemoria desde el shell.
    static InfoPptx LeerArchivo(const std::string& ruta,
                                std::vector<DiapositivaPptx>* salida);

    // ¿El nombre de archivo termina en .pptx/.pptm (case-insensitive)?
    // Útil para el shell antes de intentar abrir.
    static bool EsPaquetePptx(const std::string& ruta);
};

} // namespace fusion
