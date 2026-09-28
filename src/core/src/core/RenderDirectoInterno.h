// src/core/src/core/RenderDirectoInterno.h
// Rutinas de pintado Direct2D compartidas por RendererDirect2D (ventana
// de proyección) y RasterizadorDirectoD2D (bitmap offscreen). Viven aquí
// para que pantalla y exportación NUNCA diverjan: una sola fuente de
// verdad del pintado.
//
// Solo compila en Windows (MSVC); el arnés de pruebas portable (gcc)
// no incluye nada de esto.

#pragma once

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <d2d1.h>
#include <dwrite.h>
#include <wincodec.h>

#include <map>
#include <string>

#include "fusion/core/Renderer.h"

namespace fusion::rendirecto {

// Cache de IDWriteTextFormat por clave de estilo (familia|tamaño|negrita|
// cursiva|alineación). Evita recrear formatos por cada párrafo: la
// proyección re-renderiza en caliente y cada CreateTextFormat cuesta.
class CacheFormatos {
public:
    // Devuelve el formato para el estilo (creándolo si hace falta) o
    // nullptr si el estilo es inservible (familia vacía).
    IDWriteTextFormat* Formato(IDWriteFactory* dw, const EstiloTexto& e);

private:
    struct Clave {
        std::wstring familia;
        int tamano_dec = 0;       // tamano * 10
        bool negrita = false;
        bool cursiva = false;
        bool centro = false;
        bool operator<(const Clave& o) const;
    };
    // Los formatos se crean contra la factoría viva; si Renderer/Lector
    // liberan y recrean la factoría, el cache se invalida vía Reset().
    // Los IDs viven tanto como el cache; las factorías del caller viven
    // más (estático por hilo de render), así que no guardamos refs.
    std::map<Clave, IDWriteTextFormat*> cache_;
};

// Pinta un Fondo sobre un render target D2D (sólido/gradiente/imagen).
// Para Imagen: si no se puede cargar (falta el archivo/WIC), rellena con
// color1 y acumula aviso — nunca pinta nada distinto de lo esperado sin
// decirlo. w/h en unidades del target.
void PintarFondoEnRT(ID2D1RenderTarget* rt, IDWriteFactory* dw,
                     IWICImagingFactory* wic,
                     const Fondo& f, CacheFormatos* cache_fmt,
                     float w, float h,
                     std::vector<std::string>* avisos);

// Dibuja un paso (título/cuerpo) sobre el target: rectángulo escalado a
// w/h, texto con wrap, alineación y color del estilo. Lleva el texto en
// UTF-16 (ya convertido por ConstruirPlan).
void DibujarPasoEnRT(ID2D1RenderTarget* rt, IDWriteFactory* dw,
                     const PasoDibujo& paso, CacheFormatos* cache_fmt,
                     float w, float h);

// Carga la imagen (WIC) y la dibuja dentro del rectángulo destino
// (dx,dy,dw,dh) con el ajuste pedido. Devuelve false si no se pudo
// cargar (el caller decide el fallback y su aviso).
bool DibujarImagenEnRT(ID2D1RenderTarget* rt, IWICImagingFactory* wic,
                       const std::wstring& ruta, AjusteImagen ajuste,
                       float dx, float dy, float dw, float dh);

// Dibuja una imagen ya decodificada (RGBA 8 bits recto, memoria del
// núcleo: media de un paquete PPTX) estirada al rectángulo destino.
// Devuelve false si WIC/D2D falla (el caller decide el aviso).
bool DibujarImagenMemoriaEnRT(ID2D1RenderTarget* rt, IWICImagingFactory* wic,
                              const unsigned char* rgba, int ancho_px,
                              int alto_px, float dx, float dy, float dw,
                              float dh);

} // namespace fusion::rendirecto

#endif  // _WIN32
