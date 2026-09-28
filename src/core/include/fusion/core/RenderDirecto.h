// src/core/include/fusion/core/RenderDirecto.h
// Render directo (Sección 9.2, modo "directo"): de DiapositivaPptx + tema
// resuelto a píxeles, sin PowerPoint.
//
// Diseño en dos mitades:
//  1. PORTABLE (se prueba en Linux/MSVC): ConstruirPlan() compone un plan
//     de dibujo con coordenadas normalizadas (0..1) a partir de la
//     diapositiva leída por LectorPptx y del tema resuelto por
//     HerenciaTemas. DibujarPlan() lo ejecuta sobre cualquier Renderer
//     (D2D real en Windows, mock de grabación en pruebas).
//  2. WINDOWS: RasterizadorDirectoD2D ejecuta el plan sobre un bitmap WIC
//     offscreen (Direct2D software) y devuelve RGBA 8 bits recto, listo
//     para Exportador::CodificarPng / ExportarImagenes o para pintar la
//     salida de proyección. Las mismas rutinas de pintado alimentan
//     RendererDirect2D (pantalla) para que proyección y exportación
//     nunca diverjan (una sola fuente de verdad del layout).
//
// Alcance v1 del modo directo (docs/agent/render_directo.md):
//  - Texto: título (placeholder) + párrafos, con el tema activo.
//  - Fondos sólido y gradiente resueltos del tema; fondo tipo imagen →
//    aviso explícito y se usa fondo.color1 (nada silencioso).
//  - Imágenes/formas internas del pptx: pieza siguiente (posición EMU,
//    media del paquete).

#pragma once

#include "fusion/core/HerenciaTemas.h"
#include "fusion/core/PptxDirecto.h"
#include "fusion/core/Renderer.h"

#include <memory>
#include <string>
#include <vector>

namespace fusion {

// Un paso de dibujo en coordenadas NORMALIZADAS (0..1 sobre la
// diapositiva): el rasterizador escala al tamaño objetivo (1080p,
// ventana de proyección, exportación) y el layout no depende de la
// resolución. El fondo NO es un paso: va aparte en PlanRenderDirecto.
struct PasoDibujo {
    enum class Tipo { Titulo, Cuerpo, Imagen };
    Tipo         tipo = Tipo::Titulo;
    float        x = 0, y = 0, w = 0, h = 0;   // rectángulo normalizado
    std::wstring texto;                        // UTF-16 (Titulo/Cuerpo)
    EstiloTexto  estilo;
    int          imagen_idx = -1;              // índice en plan.imagenes
};

// Imagen ya decodificada lista para dibujar (RGBA recto). Vive en el
// plan y los pasos la referencian por índice (los pasos se copian al
// renormalizar; los píxeles no).
struct ImagenDibujo {
    std::vector<unsigned char> rgba;   // ancho*alto*4 bytes
    int ancho = 0, alto = 0;
};

struct PlanRenderDirecto {
    // Relación de aspecto de la diapositiva origen (ancho/alto, de
    // p:sldSz). La consumen DibujarPlan y RasterizadorDirectoD2D para
    // ENCAJAR la diapositiva en el objetivo con bandas (letterbox
    // centrado): el fondo cubre el objetivo completo y los pasos quedan
    // dentro del rect de contenido. Con objetivo de la misma relación,
    // es identidad. 16:9 si el paquete no la declara.
    float aspecto = 16.0f / 9.0f;

    // Fondo resuelto del tema activo. Nunca es "escritorio": si el tema
    // no define fondo, queda negro sólido (regla 6.1 del doc técnico).
    Fondo fondo;

    // Pasos en orden de dibujo (imágenes primero, título después,
    // cuerpo al final: el texto nunca queda bajo una imagen).
    std::vector<PasoDibujo> pasos;

    // Imágenes del plan (orden de inserción = orden de referencia).
    std::vector<ImagenDibujo> imagenes;
};

struct OpcionesRenderDirecto {
    // Límite defensivo de párrafos por diapositiva: los excedentes se
    // descartan con aviso (nada silencioso), el resto se reparte el
    // cuerpo en slots iguales.
    int max_parrafos = 12;

    // Tamaño de la diapositiva origen en EMU (p:sldSz del paquete).
    // Necesario para normalizar las posiciones EMU de las imágenes
    // p:pic. Si es 0, las imágenes se omiten con aviso (nada silencioso).
    long long ancho_emu = 0;
    long long alto_emu  = 0;
};

struct ResultadoRenderDirecto {
    bool        ok = false;
    std::string msg_error;              // vacío si ok
    std::vector<std::string> avisos;    // informativos, no bloquean
};

class RenderDirecto {
public:
    // Construye el plan de una diapositiva leída por LectorPptx con el
    // tema resuelto de la herencia (nivel runtime incluido si existe).
    // `aspecto` = ancho/alto de la diapositiva (InfoPptx::ancho_emu /
    // alto_emu; <= 0 → 16:9). Nunca falla por contenido: una diapositiva
    // vacía produce solo fondo. Falla solo con out==nullptr.
    static ResultadoRenderDirecto ConstruirPlan(
        const DiapositivaPptx& diapo,
        const ResolucionTema& tema_resuelto,
        float aspecto,
        PlanRenderDirecto* out,
        const OpcionesRenderDirecto& opciones = {});
};

// Ejecuta el plan sobre un renderer (portable): fondo primero, luego los
// pasos en orden, escalando los rectángulos normalizados al objetivo de
// ancho x alto en unidades del renderer (píxeles). r==nullptr → no-op.
void DibujarPlan(Renderer* r, const PlanRenderDirecto& plan,
                 float ancho, float alto);

// UTF-8 → UTF-16 portable, decodificador propio (sin API de Windows):
// ASCII, multibyte de 2/3/4 bytes (acentos, ñ, CJK, emoji como par
// suplente) y secuencias sobrecodificadas; byte inválido o truncado →
// U+FFFD y continúa (nunca aborta, nunca se come el resto del texto).
std::wstring Utf8AUtf16(const std::string& utf8);

#ifdef _WIN32
// Rasterizador offscreen del modo directo: Direct2D (software) sobre
// bitmap WIC. El shell lo usa para el pre-render de proyección y para el
// seam del exportador de imágenes 1080p (RasterizadorUnidad).
class RasterizadorDirectoD2D {
public:
    RasterizadorDirectoD2D();
    ~RasterizadorDirectoD2D();

    RasterizadorDirectoD2D(const RasterizadorDirectoD2D&)            = delete;
    RasterizadorDirectoD2D& operator=(const RasterizadorDirectoD2D&) = delete;

    // Rasteriza el plan a RGBA 8 bits recto (exactamente
    // ancho*alto*4 bytes). Devuelve false y msg_error si el objetivo no
    // es coherente o D2D falla (error explícito, no silencioso).
    bool Rasterizar(const PlanRenderDirecto& plan, int ancho, int alto,
                    std::vector<unsigned char>* rgba,
                    std::string* msg_error);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
#endif  // _WIN32

} // namespace fusion
