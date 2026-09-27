// src/core/include/fusion/data/AhpFormat.h
// Parser y serializador del formato ahp.v1 (programas de culto).
// JSON versionado, ignorable hacia adelante.

#pragma once

#include "fusion/Version.h"
// AjusteImagen se define en fusion/core/Renderer.h y se reutiliza aquí.
#include "fusion/core/Renderer.h"

#include <string>
#include <vector>
#include <memory>
#include <cstdint>

namespace fusion {

// Tipo de elemento. Mismo nombre que en el JSON.
enum class TipoElemento {
    Texto,
    Versiculo,
    Imagen,
    Video,
    LowerThird,
    Pptx,
    Desconocido,
};

// Modo de un versículo.
enum class ModoVersiculo {
    Completo,
    Tercio,           // lower third
};

// Modo de proyección de un elemento pptx (format_ahp_v1.md):
// "com" (con PowerPoint, archivo original) o "directo" (sin PowerPoint,
// render propio). Desconocido = valor inválido que Validar() rechaza.
enum class ModoPptx {
    Com,
    Directo,
    Desconocido,
};

struct LineaTexto {
    std::string texto;
    std::string marca;          // etiqueta de sincronización, opcional
};

struct Elemento {
    std::string id;
    TipoElemento tipo;
    std::string titulo;         // texto/lower_third
    std::vector<LineaTexto> lineas;       // texto
    std::string acordes;        // texto (ChordPro inline)
    std::string tono_origen;
    std::string tono_actual;
    int          bpm = 0;
    std::string cita;           // versiculo
    std::string biblia;
    std::string texto_versiculo;
    ModoVersiculo modo_versiculo = ModoVersiculo::Completo;
    ModoPptx      modo_pptx      = ModoPptx::Com;
    std::string ruta;            // imagen/video/pptx
    AjusteImagen ajuste = AjusteImagen::Cubrir;
    bool         bucle_video = false;
    bool         audio_video = true;
    std::string sub_lower;       // lower_third
    std::string tema_override;
    std::string notas;
    double      tam_fuente_pt = 0.0;  // texto: tamaño uniforme de la caja
                                      // (sz/100 de PresentationML al
                                      // importar PPTX, doc 9.2.3);
                                      // 0 = hereda del tema.
};

struct Escenario {
    std::string id;
    std::string nombre;
    std::string notas;
    std::string tema;
    std::string fondo;   // color sólido del escenario ("#RRGGBB"/"#RGB")
                         // o vacío = hereda del tema. Destino del "fondo
                         // del diseño" al importar PPTX (doc 9.2.6).
    std::vector<Elemento> elementos;
};

struct Programa {
    std::string titulo;
    std::string fecha;
    std::string autor;
    std::string tema_raiz;
    std::string notas;
    std::vector<Escenario> escenarios;
};

struct AhpFormat {
    // Carga un .ahp. Devuelve false y msg_error si falla.
    static bool Cargar(const std::string& ruta, Programa* out,
                      std::string* msg_error);

    // Carga desde string JSON (para tests).
    static bool CargarFromString(const std::string& json, Programa* out,
                                 std::string* msg_error);

    // Guarda un .ahp con BOM y 2 espacios de indentación.
    static bool Guardar(const std::string& ruta, const Programa& p);

    // Serializa a string JSON.
    static std::string Serializar(const Programa& p);

    // Validación en frío: ids únicos, tipos correctos, etc.
    static bool Validar(const Programa& p, std::string* msg_error);

    // Versión del formato leído. Útil para avisar al usuario si el archivo
    // es de una versión futura y se está ignorando parte.
    static std::uint32_t VersionLeida();
};

} // namespace fusion
