// src/core/include/fusion/core/Exportador.h
// Exportadores reales del núcleo (Sección 9.3): PPTX, PDF e imágenes.
//
// Reglas del producto:
//  - La exportación la dispara SIEMPRE el operador de forma explícita;
//    nada de este archivo se ejecuta solo ni en segundo plano.
//  - Todo es portable y 100 % offline: sin dependencias nuevas.
//      * PPTX : paquete OPC (ISO/IEC-29500) escrito con entradas
//               almacenadas; lo abre PowerPoint/LibreOffice y lo
//               relee nuestro propio LectorPptx (round-trip).
//      * PDF  : PDF 1.4 con fuentes base-14 (Helvetica) y
//               WinAnsiEncoding: acentos españoles correctos sin
//               incrustar fuentes.
//      * PNG  : RGBA → PNG válido (zlib con bloques almacenados).
//               El renderizado del píxel lo aporta el shell (Direct2D
//               en Windows) mediante el callback RasterizadorUnidad;
//               en pruebas basta un buffer sintético.
//  - Los medios (vídeo/imagen) y los paquetes pptx no se exportan como
//    medio en v1: la diapositiva/página lleva su referencia y el
//    resultado acumula un aviso. Nada se pierde en silencio.

#pragma once

#include "fusion/core/PlanExport.h"

#include <functional>
#include <string>
#include <vector>

namespace fusion {

struct ResultadoExport {
    bool        ok = false;
    std::string msg_error;              // vacío si ok
    std::vector<std::string> avisos;    // informativos, no bloquean
    std::vector<std::string> archivos;  // rutas generadas, en orden
};

// El shell aporta los píxeles de cada unidad (1920x1080 recomendado).
// Devuelve false si no pudo rasterizar esa unidad. El tamaño real se
// comunica en `ancho`/`alto` (RGBA, 8 bits por canal).
using RasterizadorUnidad = std::function<bool(
    int indice_unidad, int ancho_objetivo, int alto_objetivo, int* ancho,
    int* alto, std::vector<unsigned char>* rgba)>;

class Exportador {
public:
    // Exporta el programa completo a un paquete .pptx (una diapositiva
    // por unidad). Crea/sobrescribe `ruta_salida`.
    static bool ExportarPptx(const Programa& p, const std::string& ruta_salida,
                             ResultadoExport* out);

    // Exporta el programa completo a un .pdf (una página por unidad,
    // 960x540 pt = 16:9).
    static bool ExportarPdf(const Programa& p, const std::string& ruta_salida,
                            ResultadoExport* out);

    // Exporta las unidades a PNG dentro de `dir_salida` (se crea si no
    // existe): <nombre_base>-01.png, -02.png, ... Pide cada imagen al
    // rasterizador del shell.
    static bool ExportarImagenes(const Programa& p,
                                 const std::string& dir_salida,
                                 const RasterizadorUnidad& rasterizar,
                                 ResultadoExport* out);

    // Codificador PNG directo (útil para el shell): RGBA de 8 bits →
    // bytes PNG válidos (IHDR/IDAT/IEND con CRC verificado). Devuelve
    // "" si los parámetros no son coherentes.
    static std::string CodificarPng(int ancho, int alto,
                                    const unsigned char* rgba);

    // Tamaño objetivo de imagen 1080p (16:9) que se pasa al rasterizador.
    static constexpr int kAncho1080p = 1920;
    static constexpr int kAlto1080p  = 1080;
};

} // namespace fusion
