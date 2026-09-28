// src/core/include/fusion/core/PptxDirecto.h
// Lector directo de paquetes .pptx/.pptm (ISO/IEC-29500) — Sección 9.2,
// modo "directo": proyectar el archivo original SIN PowerPoint con el
// render propio del núcleo.
//
// Alcance v2 (docs/agent/pptx_directo.md):
//  - Estructura OPC: ZIP (lector propio, CRC32 + ZIP64 defensivo) +
//    [Content_Types].xml + relaciones (_rels) + presentation.xml +
//    ppt/slides/slideN.xml, en el orden real (p:sldIdLst).
//  - Texto: título (placeholder title/ctrTitle) y párrafos del cuerpo
//    con sus RUNS y estilos (a:rPr: tamaño sz, negrita b, cursiva i,
//    subrayado u, color a:srgbClr), incluidos tablas y cuadros de texto.
//    El parseo XML usa pugixml (vendido en third_party, MIT).
//  - Imágenes internas: p:pic con posición/tamaño EMU (a:xfrm) y blip
//    r:embed → relaciones → parte media, decodificadas a RGBA con
//    stb_image (vendido, dominio público; PNG/JPEG/BMP/GIF).
//  - Tamaño de diapositiva en EMU (p:sldSz) para el letterbox.
//  - Leer no ejecuta nada: un .pptm se lee igual que un .pptx y las
//    macros JAMÁS se ejecutan (regla del producto).
//  - Defensas: límite antizip-bomb del lector ZIP, límite de memoria de
//    decode por imagen, avisos explícitos (nada silencioso).
//
// Fuera de alcance v2: formas vectoriales (a:custGeom/prstGeom) y
// estilos mezclados DENTRO de un párrafo (se aplica el estilo del primer
// run con estilo explícito y se avisa; nada silencioso).

#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace fusion {

// Un run: fragmento de texto con estilo propio (opcional). Los campos
// "tiene_*" distinguen "sin estilo explícito" (hereda del tema) de
// "estilo explícito" (manda sobre el tema, fidelidad con el archivo).
struct EstructuraRun {
    std::string texto_utf8;
    bool        tiene_tamano = false;
    float       tam_pt       = 0.0f;  // sz en centésimas de punto → pt
    bool        negrita      = false;
    bool        cursiva      = false;
    bool        subrayado    = false; // u="sng"; se conserva, el render v2 no lo aplica
    bool        tiene_color  = false;
    std::string color_hex;            // "#RRGGBB" desde a:srgbClr val
};

// Párrafo: secuencia de runs (fragmentos de un mismo párrafo).
struct ParrafoPptx {
    std::vector<EstructuraRun> runs;

    // Texto plano del párrafo (runs unidos sin separador).
    std::string Texto() const;
};

// Imagen embebida en la diapositiva (p:pic), ya decodificada a RGBA 8
// bits recto. Posición y tamaño en EMU tal como los puso el autor
// (914400 EMU = 1"). Sin a:xfrm declarado → rectángulo 0 y aviso del
// lector (la imagen se informa, no se proyecta a ciegas).
struct ImagenPptx {
    std::vector<unsigned char> rgba;      // ancho*alto*4 bytes
    int        ancho = 0, alto = 0;       // píxeles decodificados
    long long  x_emu = 0, y_emu = 0;      // a:off
    long long  w_emu = 0, h_emu = 0;      // a:ext
    std::string parte;                    // "ppt/media/image1.png" (diagnóstico)
};

// Una diapositiva del paquete, lista para el render directo.
struct DiapositivaPptx {
    int indice = 0;                     // 1-based, orden de p:sldIdLst
    std::string titulo;                 // vacío si la diapositiva no tiene
    std::vector<std::string> parrafos;  // cuerpo en orden de lectura
                                        // (texto plano, compatibilidad v1)
    std::vector<ParrafoPptx> parrafos_ricos; // mismo orden, con runs
    std::vector<ImagenPptx> imagenes;   // p:pic decodificadas, en orden
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
