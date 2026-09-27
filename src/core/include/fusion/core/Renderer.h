// src/core/include/fusion/core/Renderer.h
// Pipeline de renderizado: Direct2D + DirectWrite + GDI+.
// Sin parpadeo (doble buffer + composición por hardware cuando esté disponible).

#pragma once

#include <cstdint>
#include <string>
#include <memory>

namespace fusion {

// Modos de ajuste para imágenes.
enum class AjusteImagen {
    Cubrir,
    Contener,
    Estirar,
};

// Color RGB 24-bit.
struct Color {
    std::uint8_t r, g, b;
    static Color DesdeHex(const char* hex);  // "#RRGGBB"
};

// Estilo tipográfico.
struct EstiloTexto {
    std::wstring familia           = L"Outfit";
    float        tamano            = 60.0f;
    Color        color             = { 255, 255, 255 };
    bool         negrita           = false;
    bool         cursiva           = false;
    bool         alineacion_centro = true;
};

// Fondo a dibujar.
struct Fondo {
    enum class Tipo { Solido, Gradiente, Imagen };
    Tipo        tipo = Tipo::Solido;
    Color       color1 = { 0, 0, 0 };
    Color       color2 = { 0, 0, 0 };
    std::wstring ruta_imagen;  // si tipo == Imagen
    AjusteImagen ajuste = AjusteImagen::Cubrir;
};

// Renderer abstracto. En producción hay una implementación Direct2D.
// Las pruebas pueden inyectar un RendererNull para no tocar la GPU.
class Renderer {
public:
    virtual ~Renderer() = default;

    virtual bool Inicializar(void* hwnd_salida) = 0;
    virtual void Liberar() = 0;

    virtual void Limpiar() = 0;
    virtual void Presentar() = 0;

    virtual void DibujarFondo(const Fondo& f) = 0;
    virtual void DibujarTexto(const std::wstring& texto,
                              const EstiloTexto& estilo,
                              float x, float y, float w, float h) = 0;
    virtual void DibujarImagen(const std::wstring& ruta,
                               float x, float y, float w, float h,
                               AjusteImagen ajuste) = 0;
};

std::unique_ptr<Renderer> CrearRendererDirect2D();
std::unique_ptr<Renderer> CrearRendererNull();

} // namespace fusion
