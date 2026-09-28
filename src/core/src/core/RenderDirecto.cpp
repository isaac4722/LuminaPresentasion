// src/core/src/core/RenderDirecto.cpp
// Render directo (9.2): plan portable de dibujo + rasterizador D2D offscreen.
// La parte portable (Utf8AUtf16, ConstruirPlan, DibujarPlan) compila en
// cualquier plataforma; la de Direct2D solo en Windows (MSVC).

#include "fusion/core/RenderDirecto.h"

#include <algorithm>
#include <cmath>

namespace fusion {

// ---------------------------------------------------------------------------
// Portable: Color::DesdeHex (antes en Renderer.cpp; también la necesita el
// render directo para los colores por-run)
// ---------------------------------------------------------------------------

Color Color::DesdeHex(const char* hex) {
    Color c{0,0,0};
    if (!hex) return c;
    std::string s = hex;
    if (!s.empty() && s[0] == '#') s = s.substr(1);
    if (s.size() < 6) return c;
    auto h2 = [](char ch) -> std::uint8_t {
        if (ch >= '0' && ch <= '9') return ch - '0';
        if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
        if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
        return 0;
    };
    c.r = (h2(s[0]) << 4) | h2(s[1]);
    c.g = (h2(s[2]) << 4) | h2(s[3]);
    c.b = (h2(s[4]) << 4) | h2(s[5]);
    return c;
}

// ---------------------------------------------------------------------------
// Portable: UTF-8 → UTF-16 (decodificador propio)
// ---------------------------------------------------------------------------

namespace {

constexpr char16_t kReemplazo = u'\uFFFD';

// Añade un punto de código (≤ U+10FFFF) como UTF-16 (con suplentes si
// hace falta) al buffer.
void AnadirPunto(std::wstring* out, unsigned int cp) {
    if (cp < 0x10000) {
        out->push_back(static_cast<wchar_t>(cp));
    } else {
        cp -= 0x10000;
        out->push_back(static_cast<wchar_t>(0xD800 + (cp >> 10)));
        out->push_back(static_cast<wchar_t>(0xDC00 + (cp & 0x3FF)));
    }
}

} // namespace

std::wstring Utf8AUtf16(const std::string& utf8) {
    std::wstring out;
    out.reserve(utf8.size());
    const unsigned char* p = reinterpret_cast<const unsigned char*>(utf8.data());
    const size_t n = utf8.size();
    size_t i = 0;
    while (i < n) {
        const unsigned char b = p[i];
        // Longitud esperada por el byte guía (RFC 3629).
        int extra = 0;
        unsigned int cp = 0;
        if (b < 0x80) {
            out.push_back(static_cast<wchar_t>(b));
            ++i;
            continue;
        } else if ((b & 0xE0) == 0xC0) {
            extra = 1; cp = b & 0x1F;
        } else if ((b & 0xF0) == 0xE0) {
            extra = 2; cp = b & 0x0F;
        } else if ((b & 0xF8) == 0xF0) {
            extra = 3; cp = b & 0x07;
        } else {
            // Continuación suelta o byte inválido: U+FFFD y sigue.
            out.push_back(kReemplazo);
            ++i;
            continue;
        }
        bool ok = true;
        int k_ok = 0;
        bool truncado = false;
        for (int k = 1; k <= extra; ++k) {
            if (i + static_cast<size_t>(k) >= n) {
                // La secuencia se corta al final de la entrada.
                truncado = true;
                break;
            }
            const unsigned char c = p[i + k];
            if ((c & 0xC0) != 0x80) { ok = false; break; }
            cp = (cp << 6) | (c & 0x3F);
            k_ok = k;
        }
        if (truncado) {
            // U+FFFD y termina: no queda nada más que decodificar.
            out.push_back(kReemplazo);
            break;
        }
        if (!ok) {
            // Secuencia rota: un U+FFFD por subparte máxima y resincronizamos
            // EN el byte ofensor (práctica del estándar Unicode): el byte
            // roto no se traga, se procesa de nuevo (si es ASCII pasa, si es
            // otra guía arranca su propia secuencia).
            out.push_back(kReemplazo);
            i += 1 + static_cast<size_t>(k_ok);
            continue;
        }
        i += 1 + extra;
        // Sobrecodificación y rangos de suplentes inválidos.
        const bool sobre = (extra == 1 && cp < 0x80) ||
                           (extra == 2 && cp < 0x800) ||
                           (extra == 3 && cp < 0x10000);
        if (sobre || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) {
            out.push_back(kReemplazo);
            continue;
        }
        AnadirPunto(&out, cp);
    }
    return out;
}

// ---------------------------------------------------------------------------
// Portable: estimación de ajuste de texto (auto-ajuste determinista)
// ---------------------------------------------------------------------------

// Métricas estimadas del contrato (docs/agent/render_directo.md): letra
// media 0.55 x tamaño (latín); 1.0 x tamaño (CJK/fullwidth); interlineado
// 1.35. La estimación solo guía el auto-ajuste (errar hacia abajo es
// seguro: letra algo menor, nunca recorte); el dibujado final lo hace
// DirectWrite con métricas reales.
constexpr float kInterlineado   = 1.35f;
constexpr float kAnchoMedio     = 0.55f;
constexpr float kAnchoAncho     = 1.00f;
constexpr float kAltoReferencia = 1080.0f;  // altura de calibración del plan
constexpr float kPasoAjuste     = 0.90f;
constexpr float kTamMinAbs      = 9.0f;
constexpr float kTamMinRel      = 0.011f;  // del alto de referencia

bool EsLetraAncha(unsigned int cp) {
    return (cp >= 0x1100 && cp <= 0x11FF) ||  // Hangul Jamo
           (cp >= 0x2E80 && cp <= 0x9FFF) ||  // CJK
           (cp >= 0xAC00 && cp <= 0xD7AF) ||  // Hangul silábico
           (cp >= 0xF900 && cp <= 0xFAFF) ||  // ideogramas de compat.
           (cp >= 0xFF00 && cp <= 0xFF60) ||  // fullwidth
           (cp >= 0xFFE0 && cp <= 0xFFE6) ||
           (cp >= 0x20000 && cp <= 0x3FFFD);  // suplementarios CJK
}

float AnchoEstimado(const std::wstring& s, float tam) {
    float ancho = 0.0f;
    for (wchar_t c : s) {
        const unsigned int cp = static_cast<unsigned int>(c);
        ancho += tam * (EsLetraAncha(cp) ? kAnchoAncho : kAnchoMedio);
    }
    return ancho;
}

// Líneas que ocuparía el texto envuelto por palabras a `tam` en un área
// de `ancho` px. Palabra que sola excede el ancho: se parte por
// caracteres (aproximación conservadora: cuenta líneas enteras).
int ContarLineas(const std::wstring& texto, float tam, float ancho) {
    if (texto.empty() || ancho <= 0.0f) return 0;
    const float espacio = tam * kAnchoMedio;
    int lineas = 0;
    bool hay_linea = false;
    float ancho_linea = 0.0f;
    std::wstring token;
    auto procesar = [&]() {
        if (token.empty()) return;
        const float at = AnchoEstimado(token, tam);
        if (at > ancho) {
            if (hay_linea) { ++lineas; hay_linea = false; ancho_linea = 0.0f; }
            lineas += static_cast<int>(std::ceil(at / ancho));
        } else if (!hay_linea) {
            hay_linea = true;
            ancho_linea = at;
        } else if (ancho_linea + espacio + at <= ancho) {
            ancho_linea += espacio + at;
        } else {
            ++lineas;
            ancho_linea = at;
        }
        token.clear();
    };
    for (wchar_t c : texto) {
        if (c == L' ' || c == L'\t' || c == L'\r' || c == L'\n') procesar();
        else token.push_back(c);
    }
    procesar();
    if (hay_linea) ++lineas;
    return lineas;
}

// Tamaño (<= tam_inicial) tal que el párrafo más exigente cabe en su
// slot (alto_area / n_slots) con el ancho_area dado. Reduce x0.90 por
// intento hasta caber o llegar al mínimo (max(9 px, 1.1 % de 1080)).
float AjustarTamano(const std::vector<std::wstring>& textos,
                    float tam_inicial, float ancho_area, float alto_area,
                    int n_slots) {
    if (textos.empty() || n_slots <= 0 || ancho_area <= 0.0f ||
        alto_area <= 0.0f)
        return tam_inicial;
    const float slot = alto_area / static_cast<float>(n_slots);
    const float tam_min = std::max(kTamMinAbs, kTamMinRel * kAltoReferencia);
    float tam = tam_inicial;
    for (;;) {
        float peor = 0.0f;
        for (const auto& t : textos) {
            const int nl = ContarLineas(t, tam, ancho_area);
            if (nl > 0)
                peor = std::max(peor, static_cast<float>(nl) * tam *
                                          kInterlineado);
        }
        if (peor <= slot || tam <= tam_min) break;
        tam *= kPasoAjuste;
        if (tam < tam_min) tam = tam_min;
    }
    return tam;
}

// ---------------------------------------------------------------------------
// Portable: encaje con bandas (letterbox)
// ---------------------------------------------------------------------------

// Rectángulo de contenido de una diapositiva de relación `aspecto`
// (ancho/alto) dentro de un objetivo ancho x alto: ocupa el máximo
// posible conservando su proporción, centrado (bandas iguales a los
// lados o arriba/abajo). El fondo del tema cubre SIEMPRE el objetivo
// completo (regla 6.1: la salida nunca muestra el escritorio); los
// pasos de texto se remapean dentro de este rectángulo.
struct RectContenido {
    float x = 0, y = 0, w = 0, h = 0;
};

RectContenido Encajar(float aspecto, float ancho, float alto) {
    RectContenido c;
    if (!(aspecto > 0.0f) || !(ancho > 0.0f) || !(alto > 0.0f)) {
        // Sin datos coherentes: objetivo completo (comportamiento v1).
        c.w = ancho;
        c.h = alto;
        return c;
    }
    const float objetivo = ancho / alto;
    if (aspecto >= objetivo) {
        c.w = ancho;
        c.h = ancho / aspecto;
    } else {
        c.h = alto;
        c.w = alto * aspecto;
    }
    c.x = (ancho - c.w) * 0.5f;
    c.y = (alto - c.h) * 0.5f;
    return c;
}

// Renormaliza un paso (coordenadas 0..1 sobre la diapositiva) a
// coordenadas 0..1 del OBJETIVO completo, dejándolo dentro del rect de
// contenido. Con objetivo de la misma relación que la diapositiva, el
// rect cubre todo y el paso queda idéntico (identidad).
PasoDibujo Renormalizar(const PasoDibujo& p, const RectContenido& c,
                        float ancho, float alto) {
    PasoDibujo q = p;
    q.x = (c.x + p.x * c.w) / ancho;
    q.y = (c.y + p.y * c.h) / alto;
    q.w = (p.w * c.w) / ancho;
    q.h = (p.h * c.h) / alto;
    return q;
}

// ---------------------------------------------------------------------------
// Portable: construcción del plan
// ---------------------------------------------------------------------------

// Bandas del layout (coordenadas normalizadas). Documentadas en
// docs/agent/render_directo.md.
constexpr float kBandaX     = 0.08f;
constexpr float kBandaW     = 0.84f;
constexpr float kTituloY    = 0.10f;
constexpr float kTituloH    = 0.18f;
constexpr float kCuerpoY    = 0.34f;
constexpr float kCuerpoH    = 0.56f;
constexpr float kCuerpoSoloY = 0.10f;
constexpr float kCuerpoSoloH = 0.80f;

// Mapa pt → píxeles del plan (referencia 1080). PowerPoint dibuja la
// diapositiva a 96 dpi (alto estándar 7.5" = 720 px); escalando a la
// referencia de 1080: px = pt * (96/72) * (1080 * 914400 / alto_emu / 96)
// = pt * 13716000 / alto_emu. Con alto_emu <= 0 se asume 7.5" (6858000
// EMU → factor 2.0). Clamp defensivo ante paquetes absurdos.
float TamPtAPx1080(float pt, long long alto_emu) {
    if (pt <= 0.0f) return 0.0f;
    float factor = 2.0f;
    if (alto_emu > 0) {
        factor = 13716000.0f / static_cast<float>(alto_emu);
        factor = std::min(std::max(factor, 0.2f), 12.0f);
    }
    return pt * factor;
}

// ¿El run trae estilo explícito (algún campo con valor propio)?
bool RunConEstilo(const EstructuraRun& r) {
    return r.tiene_tamano || r.tiene_color || r.negrita || r.cursiva;
}

// Aplica el estilo explícito del run sobre el estilo base (tema).
void AplicarRunAEstilo(const EstructuraRun& run, long long alto_emu,
                       EstiloTexto* e) {
    if (run.tiene_tamano) {
        const float px = TamPtAPx1080(run.tam_pt, alto_emu);
        if (px > 0.0f) e->tamano = px;
    }
    if (run.tiene_color && run.color_hex.size() >= 6)
        e->color = Color::DesdeHex(run.color_hex.c_str());
    e->negrita = run.negrita;
    e->cursiva = run.cursiva;
}

// ¿Dos estilos difieren en lo que el modo directo puede dibujar?
bool EstilosDistintos(const EstiloTexto& a, const EstiloTexto& b) {
    return a.tamano != b.tamano || a.negrita != b.negrita ||
           a.cursiva != b.cursiva || a.color.r != b.color.r ||
           a.color.g != b.color.g || a.color.b != b.color.b;
}

// Estilo efectivo de un párrafo (runs): el primer run con estilo
// explícito manda; los demás se comparan y una mezcla SE AVISA (nada
// silencioso: el render v2 no dibuja runs alternados dentro de un
// párrafo). Devuelve false si el párrafo no trae estilo propio.
bool EstiloDeParrafo(const ParrafoPptx& par, long long alto_emu,
                     const EstiloTexto& base, EstiloTexto* out,
                     bool* mezcla) {
    *mezcla = false;
    bool aplicado = false;
    for (const auto& run : par.runs) {
        if (!RunConEstilo(run)) continue;
        if (!aplicado) {
            EstiloTexto e = base;
            AplicarRunAEstilo(run, alto_emu, &e);
            *out = e;
            aplicado = true;
        } else {
            EstiloTexto otro = base;
            AplicarRunAEstilo(run, alto_emu, &otro);
            if (EstilosDistintos(otro, *out)) *mezcla = true;
        }
    }
    return aplicado;
}

ResultadoRenderDirecto RenderDirecto::ConstruirPlan(
        const DiapositivaPptx& diapo,
        const ResolucionTema& tema_resuelto,
        float aspecto,
        PlanRenderDirecto* out,
        const OpcionesRenderDirecto& opciones) {
    ResultadoRenderDirecto r;
    if (!out) {
        r.msg_error = "Plan de destino nulo";
        return r;
    }
    *out = PlanRenderDirecto{};

    // Aspecto: <= 0 → 16:9 (los paquetes de PowerPoint siempre lo traen,
    // pero un paquete mínimo podría omitirlo).
    out->aspecto = aspecto > 0.0f ? aspecto : (16.0f / 9.0f);
    // Sanitizado defensivo (diapositivas absurdas no rompen el layout).
    out->aspecto = std::min(std::max(out->aspecto, 0.5f), 4.0f);

    // Estilos del tema resuelto. Fondo por defecto: negro sólido — la
    // salida nunca muestra el escritorio (doc técnico 6.1).
    EstiloTexto estilo;  // Outfit 60 blanco centrado por defecto
    if (!AplicarAEstilos(tema_resuelto.estilo_resuelto, &estilo, &out->fondo)) {
        r.avisos.push_back(
            "El tema activo tiene valores inválidos: esas claves se "
            "ignoraron y se conservó el valor por defecto");
    }
    if (out->fondo.tipo == Fondo::Tipo::Imagen &&
        out->fondo.ruta_imagen.empty()) {
        // Fondo imagen sin ruta: nada que cargar → color1 y aviso.
        r.avisos.push_back(
            "Fondo tipo imagen sin ruta: se usa fondo.color1 del tema");
        out->fondo.tipo = Fondo::Tipo::Solido;
    }

    // -------------------------------------------------------------------------
    // Imágenes internas (p:pic) ANTES del texto: orden de dibujo
    // imágenes → título → cuerpo (el texto nunca queda bajo una imagen).
    // -------------------------------------------------------------------------
    if (!diapo.imagenes.empty()) {
        if (opciones.ancho_emu <= 0 || opciones.alto_emu <= 0) {
            r.avisos.push_back(
                "El paquete no declara p:sldSz: las imágenes de la "
                "diapositiva no se pueden posicionar y se omiten");
        } else {
            for (const auto& img : diapo.imagenes) {
                if (img.rgba.empty() || img.ancho <= 0 || img.alto <= 0) {
                    r.avisos.push_back(
                        "Imagen no decodificada de la diapositiva: omitida");
                    continue;
                }
                if (img.w_emu <= 0 || img.h_emu <= 0) {
                    r.avisos.push_back(
                        "Imagen sin a:xfrm (posición/tamaño): omitida" +
                        std::string(img.parte.empty() ? "" : " (" +
                        img.parte + ")"));
                    continue;
                }
                ImagenDibujo dib;
                dib.rgba = img.rgba;
                dib.ancho = img.ancho;
                dib.alto = img.alto;
                out->imagenes.push_back(std::move(dib));

                PasoDibujo paso;
                paso.tipo = PasoDibujo::Tipo::Imagen;
                paso.imagen_idx =
                    static_cast<int>(out->imagenes.size()) - 1;
                paso.x = static_cast<float>(
                    static_cast<double>(img.x_emu) / opciones.ancho_emu);
                paso.y = static_cast<float>(
                    static_cast<double>(img.y_emu) / opciones.alto_emu);
                paso.w = static_cast<float>(
                    static_cast<double>(img.w_emu) / opciones.ancho_emu);
                paso.h = static_cast<float>(
                    static_cast<double>(img.h_emu) / opciones.alto_emu);
                out->pasos.push_back(std::move(paso));
            }
        }
    }

    // -------------------------------------------------------------------------
    // Título y cuerpo
    // -------------------------------------------------------------------------
    const std::wstring titulo = Utf8AUtf16(diapo.titulo);
    const bool hay_titulo = !titulo.empty();

    // Cuerpo: cada párrafo ocupa un slot vertical igual dentro de la
    // banda (el párrafo queda centrado en su slot por el renderizador).
    const float cuerpo_y = hay_titulo ? kCuerpoY : kCuerpoSoloY;
    const float cuerpo_h = hay_titulo ? kCuerpoH : kCuerpoSoloH;

    // Párrafos efectivos: ricos si el paquete los trae; si no, se
    // sintetizan desde el texto plano (compatibilidad v1, sin estilo).
    std::vector<ParrafoPptx> parrafos;
    if (!diapo.parrafos_ricos.empty()) {
        parrafos = diapo.parrafos_ricos;
    } else {
        parrafos.reserve(diapo.parrafos.size());
        for (const auto& t : diapo.parrafos) {
            ParrafoPptx par;
            EstructuraRun run;
            run.texto_utf8 = t;
            par.runs.push_back(std::move(run));
            parrafos.push_back(std::move(par));
        }
    }
    if (static_cast<int>(parrafos.size()) > opciones.max_parrafos) {
        r.avisos.push_back(
            "La diapositiva tiene " + std::to_string(parrafos.size()) +
            " párrafos; se muestran los primeros " +
            std::to_string(opciones.max_parrafos));
        parrafos.resize(static_cast<size_t>(opciones.max_parrafos));
    }

    // Auto-ajuste del tamaño (referencia 1080p): se reduce x0.90 hasta
    // que el párrafo más exigente cabe en su slot. Los consumidores
    // re-escalan el tamaño al objetivo real (alto / 1080).
    const RectContenido referencia =
        Encajar(out->aspecto, 16.0f * kAltoReferencia / 9.0f,
                kAltoReferencia);
    const float ancho_banda = kBandaW * referencia.w;

    if (hay_titulo) {
        const float ajustado = AjustarTamano(
            {titulo}, estilo.tamano, ancho_banda, kTituloH * referencia.h, 1);
        if (ajustado < estilo.tamano - 0.01f) {
            r.avisos.push_back("El título se redujo a " +
                std::to_string(static_cast<int>(ajustado + 0.5f)) +
                " pt para caber en su banda");
        }
        EstiloTexto estilo_titulo = estilo;
        estilo_titulo.tamano = ajustado;

        PasoDibujo paso;
        paso.tipo   = PasoDibujo::Tipo::Titulo;
        paso.x      = kBandaX;
        paso.y      = kTituloY;
        paso.w      = kBandaW;
        paso.h      = kTituloH;
        paso.texto  = titulo;
        paso.estilo = estilo_titulo;
        out->pasos.push_back(std::move(paso));
    }

    if (!parrafos.empty()) {
        std::vector<std::wstring> cuerpo;
        cuerpo.reserve(parrafos.size());
        for (const auto& par : parrafos)
            cuerpo.push_back(Utf8AUtf16(par.Texto()));

        const float ajustado = AjustarTamano(
            cuerpo, estilo.tamano, ancho_banda,
            cuerpo_h * referencia.h,
            static_cast<int>(cuerpo.size()));
        if (ajustado < estilo.tamano - 0.01f) {
            r.avisos.push_back("El cuerpo se redujo a " +
                std::to_string(static_cast<int>(ajustado + 0.5f)) +
                " pt para caber en la diapositiva");
        }

        const int n = static_cast<int>(cuerpo.size());
        const float slot = cuerpo_h / static_cast<float>(n);
        for (int i = 0; i < n; ++i) {
            EstiloTexto estilo_par = estilo;
            estilo_par.tamano = ajustado;
            bool mezcla = false;
            if (EstiloDeParrafo(parrafos[static_cast<size_t>(i)],
                                opciones.alto_emu, estilo,
                                &estilo_par, &mezcla)) {
                if (mezcla) {
                    r.avisos.push_back(
                        "Párrafo con estilos mezclados: se usa el estilo "
                        "del primer run con estilo explícito");
                }
                // El ajuste por caber NO se reaplica por párrafo: el
                // tamaño del run manda (fidelidad), el auto-ajuste es del
                // tema cuando el run no declara tamaño.
            }
            PasoDibujo paso;
            paso.tipo   = PasoDibujo::Tipo::Cuerpo;
            paso.x      = kBandaX;
            paso.y      = cuerpo_y + static_cast<float>(i) * slot;
            paso.w      = kBandaW;
            paso.h      = slot;
            paso.texto  = cuerpo[static_cast<size_t>(i)];
            paso.estilo = estilo_par;
            out->pasos.push_back(std::move(paso));
        }
    }

    r.ok = true;
    return r;
}

void DibujarPlan(Renderer* r, const PlanRenderDirecto& plan,
                 float ancho, float alto) {
    if (!r || ancho <= 0.0f || alto <= 0.0f) return;
    // El fondo cubre el objetivo completo: las bandas del letterbox
    // quedan con el tema (nunca escritorio ni vacíos negros espurios).
    r->DibujarFondo(plan.fondo);
    // El tamaño del plan está calibrado a 1080p: se re-escala al objetivo.
    const float escala_fuente = alto / kAltoReferencia;
    const RectContenido contenido = Encajar(plan.aspecto, ancho, alto);
    for (const PasoDibujo& paso : plan.pasos) {
        if (paso.tipo == PasoDibujo::Tipo::Imagen) {
            if (paso.imagen_idx < 0 ||
                paso.imagen_idx >=
                    static_cast<int>(plan.imagenes.size()))
                continue;
            const ImagenDibujo& img =
                plan.imagenes[static_cast<size_t>(paso.imagen_idx)];
            if (img.rgba.empty() || img.ancho <= 0 || img.alto <= 0)
                continue;
            const PasoDibujo q =
                Renormalizar(paso, contenido, ancho, alto);
            r->DibujarImagenMemoria(img.rgba.data(), img.ancho, img.alto,
                                    q.x * ancho, q.y * alto,
                                    q.w * ancho, q.h * alto);
            continue;
        }
        PasoDibujo q = Renormalizar(paso, contenido, ancho, alto);
        q.estilo.tamano *= escala_fuente;
        r->DibujarTexto(q.texto, q.estilo,
                        q.x * ancho, q.y * alto,
                        q.w * ancho, q.h * alto);
    }
}

} // namespace fusion

#ifdef _WIN32

// ---------------------------------------------------------------------------
// Windows: rutinas D2D compartidas (pantalla y offscreen)
// ---------------------------------------------------------------------------

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
// El alias DrawText→DrawTextW de winuser rompe el método DrawText de
// ID2D1RenderTarget (declarado tras el push_macro/undef de d2d1.h solo si
// la macro no está activa). Este proyecto no usa el DrawText de GDI.
#ifdef DrawText
#undef DrawText
#endif
#include <d2d1.h>
#include <dwrite.h>
#include <wincodec.h>
#include <wrl/client.h>

#include <map>
#include <cstdio>

#include "RenderDirectoInterno.h"

using Microsoft::WRL::ComPtr;

namespace fusion::rendirecto {

bool CacheFormatos::Clave::operator<(const Clave& o) const {
    if (familia != o.familia) return familia < o.familia;
    if (tamano_dec != o.tamano_dec) return tamano_dec < o.tamano_dec;
    if (negrita != o.negrita) return negrita < o.negrita;
    if (cursiva != o.cursiva) return cursiva < o.cursiva;
    return centro < o.centro;
}

IDWriteTextFormat* CacheFormatos::Formato(IDWriteFactory* dw,
                                          const EstiloTexto& e) {
    if (!dw || e.familia.empty()) return nullptr;
    Clave clave;
    clave.familia  = e.familia;
    clave.tamano_dec = static_cast<int>(e.tamano * 10.0f + 0.5f);
    clave.negrita  = e.negrita;
    clave.cursiva  = e.cursiva;
    clave.centro   = e.alineacion_centro;
    auto it = cache_.find(clave);
    if (it != cache_.end()) return it->second;

    ComPtr<IDWriteTextFormat> formato;
    HRESULT hr = dw->CreateTextFormat(
        clave.familia.c_str(),
        nullptr,
        clave.negrita ? DWRITE_FONT_WEIGHT_BOLD : DWRITE_FONT_WEIGHT_NORMAL,
        clave.cursiva ? DWRITE_FONT_STYLE_ITALIC : DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        e.tamano,
        L"es-ES",
        formato.GetAddressOf());
    if (FAILED(hr)) return nullptr;

    formato->SetTextAlignment(clave.centro ? DWRITE_TEXT_ALIGNMENT_CENTER
                                           : DWRITE_TEXT_ALIGNMENT_LEADING);
    // El párrafo queda centrado verticalmente en su slot: los slots
    // iguales del cuerpo se ven espaciados de forma uniforme.
    formato->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    formato->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP);

    IDWriteTextFormat* crudo = formato.Get();
    cache_.emplace(clave, crudo);
    return crudo;
}

namespace {

D2D1_COLOR_F ColorDe(const Color& c) {
    return D2D1::ColorF(c.r / 255.0f, c.g / 255.0f, c.b / 255.0f, 1.0f);
}

// Rellena el target completo con un color (crea el brush puntual).
void Rellenar(ID2D1RenderTarget* rt, const Color& c) {
    ComPtr<ID2D1SolidColorBrush> brush;
    if (FAILED(rt->CreateSolidColorBrush(ColorDe(c), brush.GetAddressOf())))
        return;
    const D2D1_SIZE_F s = rt->GetSize();
    rt->FillRectangle(D2D1::RectF(0, 0, s.width, s.height), brush.Get());
}

} // namespace

void PintarFondoEnRT(ID2D1RenderTarget* rt, IDWriteFactory* dw,
                     IWICImagingFactory* wic,
                     const Fondo& f, CacheFormatos* /*cache_fmt*/,
                     float w, float h,
                     std::vector<std::string>* avisos) {
    (void)dw;
    if (!rt) return;
    switch (f.tipo) {
        case Fondo::Tipo::Solido:
            Rellenar(rt, f.color1);
            break;
        case Fondo::Tipo::Gradiente: {
            ComPtr<ID2D1GradientStopCollection> paradas;
            const D2D1_GRADIENT_STOP ps[] = {
                { 0.0f, ColorDe(f.color1) },
                { 1.0f, ColorDe(f.color2) },
            };
            if (FAILED(rt->CreateGradientStopCollection(
                    ps, 2, D2D1_GAMMA_2_2, D2D1_EXTEND_MODE_CLAMP,
                    paradas.GetAddressOf()))) {
                Rellenar(rt, f.color1);
                break;
            }
            ComPtr<ID2D1LinearGradientBrush> brush;
            if (FAILED(rt->CreateLinearGradientBrush(
                    D2D1::LinearGradientBrushProperties(
                        D2D1::Point2F(0, 0), D2D1::Point2F(0, h)),
                    paradas.Get(), brush.GetAddressOf()))) {
                Rellenar(rt, f.color1);
                break;
            }
            rt->FillRectangle(D2D1::RectF(0, 0, w, h), brush.Get());
            break;
        }
        case Fondo::Tipo::Imagen:
            if (!DibujarImagenEnRT(rt, wic, f.ruta_imagen, f.ajuste,
                                   0.0f, 0.0f, w, h)) {
                // Explícito, no silencioso: color1 y aviso.
                Rellenar(rt, f.color1);
                if (avisos) {
                    avisos->push_back(
                        "No se pudo cargar el fondo de imagen: se usó "
                        "fondo.color1");
                }
            }
            break;
    }
}

void DibujarPasoEnRT(ID2D1RenderTarget* rt, IDWriteFactory* dw,
                     const PasoDibujo& paso, CacheFormatos* cache_fmt,
                     float w, float h) {
    if (!rt || !dw || !cache_fmt) return;
    IDWriteTextFormat* formato = cache_fmt->Formato(dw, paso.estilo);
    if (!formato) return;
    ComPtr<ID2D1SolidColorBrush> brush;
    if (FAILED(rt->CreateSolidColorBrush(ColorDe(paso.estilo.color),
                                         brush.GetAddressOf())))
        return;
    const D2D1_RECT_F recto = D2D1::RectF(
        paso.x * w, paso.y * h, (paso.x + paso.w) * w, (paso.y + paso.h) * h);
    rt->DrawText(paso.texto.c_str(),
                  static_cast<UINT32>(paso.texto.size()),
                  formato, recto, brush.Get(),
                  D2D1_DRAW_TEXT_OPTIONS_CLIP,
                  DWRITE_MEASURING_MODE_NATURAL);
}

// Carga una imagen desde disco con WIC y la dibuja dentro del rectángulo
// destino con el ajuste pedido. Devuelve false si no se pudo (el caller
// pinta color1 y avisa).
bool DibujarImagenEnRT(ID2D1RenderTarget* rt, IWICImagingFactory* wic,
                       const std::wstring& ruta, AjusteImagen ajuste,
                       float dx, float dy, float dw, float dh) {
    if (!wic || dw <= 0 || dh <= 0) return false;
    ComPtr<IWICBitmapDecoder> decod;
    HRESULT hr = wic->CreateDecoderFromFilename(
        ruta.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand,
        decod.GetAddressOf());
    if (FAILED(hr)) return false;
    ComPtr<IWICBitmapFrameDecode> marco;
    hr = decod->GetFrame(0, marco.GetAddressOf());
    if (FAILED(hr)) return false;
    ComPtr<ID2D1Bitmap> bitmap;
    hr = rt->CreateBitmapFromWicBitmap(marco.Get(), bitmap.GetAddressOf());
    if (FAILED(hr)) return false;

    const D2D1_SIZE_F i = bitmap->GetSize();
    if (i.width <= 0 || i.height <= 0) return false;
    float escala;
    switch (ajuste) {
        case AjusteImagen::Contener:
            escala = std::min(dw / i.width, dh / i.height);
            break;
        case AjusteImagen::Estirar:
            rt->DrawBitmap(bitmap.Get(), D2D1::RectF(dx, dy, dx + dw, dy + dh));
            return true;
        case AjusteImagen::Cubrir:
        default:
            escala = std::max(dw / i.width, dh / i.height);
            break;
    }
    const float ancho = i.width * escala;
    const float alto  = i.height * escala;
    const float ox = dx + (dw - ancho) / 2.0f;
    const float oy = dy + (dh - alto) / 2.0f;
    // "Cubrir" se sale del rectángulo: recortar a lo visible.
    if (ajuste == AjusteImagen::Cubrir) {
        rt->PushAxisAlignedClip(
            D2D1::RectF(dx, dy, dx + dw, dy + dh),
            D2D1_ANTIALIAS_MODE_ALIASED);
    }
    rt->DrawBitmap(bitmap.Get(), D2D1::RectF(ox, oy, ox + ancho, oy + alto));
    if (ajuste == AjusteImagen::Cubrir) rt->PopAxisAlignedClip();
    return true;
}

// Dibuja una imagen ya decodificada (RGBA recto, memoria) estirada al
// rectángulo destino: RGBA → PBGRA premultiplicada, bitmap D2D y
// DrawBitmap. El rect del modo directo ya respeta el aspecto original
// (viene del a:xfrm del autor), así que el ajuste es "estirar" exacto.
bool DibujarImagenMemoriaEnRT(ID2D1RenderTarget* rt, IWICImagingFactory* wic,
                              const unsigned char* rgba, int ancho_px,
                              int alto_px, float dx, float dy, float dw,
                              float dh) {
    (void)wic;  // el bitmap se crea directo sobre el target (sin WIC)
    if (!rt || !rgba || ancho_px <= 0 || alto_px <= 0 || dw <= 0 || dh <= 0)
        return false;
    const UINT px = static_cast<UINT>(ancho_px) *
                    static_cast<UINT>(alto_px);
    if (px == 0 || px > 64u * 1024 * 1024) return false;
    std::vector<unsigned char> pbgra(static_cast<size_t>(px) * 4);
    for (UINT i = 0; i < px; ++i) {
        const unsigned char* p = rgba + static_cast<size_t>(i) * 4;
        unsigned char* q = pbgra.data() + static_cast<size_t>(i) * 4;
        const unsigned a = p[3];
        if (a == 255) {
            q[0] = p[2]; q[1] = p[1]; q[2] = p[0]; q[3] = 255;
        } else if (a == 0) {
            q[0] = q[1] = q[2] = q[3] = 0;
        } else {
            q[0] = static_cast<unsigned char>((p[2] * a + 127) / 255);
            q[1] = static_cast<unsigned char>((p[1] * a + 127) / 255);
            q[2] = static_cast<unsigned char>((p[0] * a + 127) / 255);
            q[3] = static_cast<unsigned char>(a);
        }
    }
    D2D1_BITMAP_PROPERTIES props = D2D1::BitmapProperties(
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,
                          D2D1_ALPHA_MODE_PREMULTIPLIED));
    ComPtr<ID2D1Bitmap> bitmap;
    HRESULT hr = rt->CreateBitmap(
        D2D1::SizeU(static_cast<UINT32>(ancho_px),
                    static_cast<UINT32>(alto_px)),
        pbgra.data(), static_cast<UINT32>(ancho_px) * 4, props,
        bitmap.GetAddressOf());
    if (FAILED(hr)) return false;
    rt->DrawBitmap(bitmap.Get(),
                   D2D1::RectF(dx, dy, dx + dw, dy + dh));
    return true;
}

} // namespace fusion::rendirecto

// ---------------------------------------------------------------------------
// Windows: rasterizador offscreen
// ---------------------------------------------------------------------------

namespace fusion {

struct RasterizadorDirectoD2D::Impl {
    ComPtr<ID2D1Factory>      d2d;
    ComPtr<IDWriteFactory>    dw;
    ComPtr<IWICImagingFactory> wic;
    rendirecto::CacheFormatos formatos;

    bool Iniciar(std::string* msg_error) {
        if (!d2d) {
            HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,
                                           d2d.GetAddressOf());
            if (FAILED(hr)) {
                if (msg_error) *msg_error = "No se pudo crear D2D1Factory";
                return false;
            }
        }
        if (!dw) {
            HRESULT hr = DWriteCreateFactory(
                DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                reinterpret_cast<IUnknown**>(dw.GetAddressOf()));
            if (FAILED(hr)) {
                if (msg_error) *msg_error = "No se pudo crear DWriteFactory";
                return false;
            }
        }
        if (!wic) {
            HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr,
                                          CLSCTX_INPROC_SERVER,
                                          IID_PPV_ARGS(wic.GetAddressOf()));
            if (FAILED(hr)) {
                if (msg_error) *msg_error = "No se pudo crear WICFactory";
                return false;
            }
        }
        return true;
    }
};

RasterizadorDirectoD2D::RasterizadorDirectoD2D() : impl_(new Impl()) {}
RasterizadorDirectoD2D::~RasterizadorDirectoD2D() = default;

bool RasterizadorDirectoD2D::Rasterizar(const PlanRenderDirecto& plan,
                                        int ancho, int alto,
                                        std::vector<unsigned char>* rgba,
                                        std::string* msg_error) {
    if (!rgba) {
        if (msg_error) *msg_error = "Buffer de salida nulo";
        return false;
    }
    rgba->clear();
    if (ancho <= 0 || alto <= 0 || ancho > 8192 || alto > 8192) {
        if (msg_error) *msg_error = "Tamaño de rasterizado fuera de rango";
        return false;
    }
    const long long px = static_cast<long long>(ancho) * alto;
    if (px > 67108864LL) {  // 8192 x 8192, acotado de todos modos
        if (msg_error) *msg_error = "Tamaño de rasterizado fuera de rango";
        return false;
    }
    if (!impl_->Iniciar(msg_error)) return false;

    // Bitmap WIC PBGRA + render target software.
    ComPtr<IWICBitmap> bitmap;
    HRESULT hr = impl_->wic->CreateBitmap(
        static_cast<UINT>(ancho), static_cast<UINT>(alto),
        GUID_WICPixelFormat32bppPBGRA, WICBitmapCacheOnDemand,
        bitmap.GetAddressOf());
    if (FAILED(hr)) {
        if (msg_error) *msg_error = "No se pudo crear el bitmap WIC";
        return false;
    }
    auto props = D2D1::RenderTargetProperties(
        D2D1_RENDER_TARGET_TYPE_SOFTWARE,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,
                          D2D1_ALPHA_MODE_PREMULTIPLIED));
    ComPtr<ID2D1RenderTarget> rt;
    hr = impl_->d2d->CreateWicBitmapRenderTarget(bitmap.Get(), props,
                                                rt.GetAddressOf());
    if (FAILED(hr)) {
        if (msg_error) *msg_error = "No se pudo crear el render target offscreen";
        return false;
    }

    rt->BeginDraw();
    rendirecto::PintarFondoEnRT(rt.Get(), impl_->dw.Get(), impl_->wic.Get(),
                                plan.fondo, &impl_->formatos,
                                static_cast<float>(ancho),
                                static_cast<float>(alto), nullptr);
    const RectContenido contenido = Encajar(
        plan.aspecto, static_cast<float>(ancho), static_cast<float>(alto));
    const float escala_fuente = static_cast<float>(alto) / kAltoReferencia;
    for (const PasoDibujo& paso : plan.pasos) {
        if (paso.tipo == PasoDibujo::Tipo::Imagen) {
            if (paso.imagen_idx < 0 ||
                paso.imagen_idx >= static_cast<int>(plan.imagenes.size()))
                continue;
            const ImagenDibujo& img =
                plan.imagenes[static_cast<size_t>(paso.imagen_idx)];
            if (img.rgba.empty() || img.ancho <= 0 || img.alto <= 0)
                continue;
            const PasoDibujo q = Renormalizar(
                paso, contenido, static_cast<float>(ancho),
                static_cast<float>(alto));
            rendirecto::DibujarImagenMemoriaEnRT(
                rt.Get(), impl_->wic.Get(), img.rgba.data(), img.ancho,
                img.alto, q.x * ancho, q.y * alto, q.w * ancho,
                q.h * alto);
            continue;
        }
        PasoDibujo q = Renormalizar(paso, contenido,
                                    static_cast<float>(ancho),
                                    static_cast<float>(alto));
        q.estilo.tamano *= escala_fuente;
        rendirecto::DibujarPasoEnRT(rt.Get(), impl_->dw.Get(), q,
                                    &impl_->formatos,
                                    static_cast<float>(ancho),
                                    static_cast<float>(alto));
    }
    hr = rt->EndDraw();
    if (FAILED(hr)) {
        if (msg_error)
            *msg_error = "Falló el dibujado offscreen (hr=0x" +
                         [] (HRESULT v) {
                             char b[16];
                             snprintf(b, sizeof(b), "%08lX",
                                      static_cast<unsigned long>(v));
                             return std::string(b);
                         }(hr) + ")";
        return false;
    }

    // Copia y conversión PBGRA (premultiplicada) → RGBA recto. El fondo
    // cubre todo, así que en la práctica alfa==255 y basta permutar;
    // la conversión general queda por robustez.
    ComPtr<IWICBitmapLock> lock;
    hr = bitmap->Lock(nullptr, WICBitmapLockRead, lock.GetAddressOf());
    if (FAILED(hr)) {
        if (msg_error) *msg_error = "No se pudo bloquear el bitmap";
        return false;
    }
    UINT tam_buf = 0;
    BYTE* datos = nullptr;
    hr = lock->GetDataPointer(&tam_buf, &datos);
    if (FAILED(hr) || !datos ||
        tam_buf < static_cast<UINT>(px) * 4) {
        if (msg_error) *msg_error = "Bitmap con tamaño de datos inesperado";
        return false;
    }
    rgba->resize(static_cast<size_t>(px) * 4);
    for (long long i = 0; i < px; ++i) {
        const BYTE* p = datos + i * 4;
        const unsigned a = p[3];
        unsigned char r8, g8, b8;
        if (a == 0) {
            r8 = g8 = b8 = 0;
        } else if (a == 255) {
            b8 = p[0]; g8 = p[1]; r8 = p[2];
        } else {
            r8 = static_cast<unsigned char>(std::min(
                255, static_cast<int>((p[2] * 255u + a / 2) / a)));
            g8 = static_cast<unsigned char>(std::min(
                255, static_cast<int>((p[1] * 255u + a / 2) / a)));
            b8 = static_cast<unsigned char>(std::min(
                255, static_cast<int>((p[0] * 255u + a / 2) / a)));
        }
        (*rgba)[i * 4 + 0] = r8;
        (*rgba)[i * 4 + 1] = g8;
        (*rgba)[i * 4 + 2] = b8;
        (*rgba)[i * 4 + 3] = static_cast<unsigned char>(a);
    }
    return true;
}

} // namespace fusion

#endif  // _WIN32
