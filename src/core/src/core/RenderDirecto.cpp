// src/core/src/core/RenderDirecto.cpp
// Render directo (9.2): plan portable de dibujo + rasterizador D2D offscreen.
// La parte portable (Utf8AUtf16, ConstruirPlan, DibujarPlan) compila en
// cualquier plataforma; la de Direct2D solo en Windows (MSVC).

#include "fusion/core/RenderDirecto.h"

#include <algorithm>
#include <cmath>

namespace fusion {

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
    if (out->fondo.tipo == Fondo::Tipo::Imagen) {
        r.avisos.push_back(
            "Fondo tipo imagen no soportado aún en el modo directo: se "
            "usa fondo.color1 del tema");
        out->fondo.tipo = Fondo::Tipo::Solido;
    }

    // Título (placeholder de la diapositiva).
    const std::wstring titulo = Utf8AUtf16(diapo.titulo);
    const bool hay_titulo = !titulo.empty();
    if (hay_titulo) {
        PasoDibujo paso;
        paso.tipo   = PasoDibujo::Tipo::Titulo;
        paso.x      = kBandaX;
        paso.y      = kTituloY;
        paso.w      = kBandaW;
        paso.h      = kTituloH;
        paso.texto  = titulo;
        paso.estilo = estilo;
        out->pasos.push_back(std::move(paso));
    }

    // Cuerpo: cada párrafo ocupa un slot vertical igual dentro de la
    // banda (el párrafo queda centrado en su slot por el renderizador).
    const float cuerpo_y = hay_titulo ? kCuerpoY : kCuerpoSoloY;
    const float cuerpo_h = hay_titulo ? kCuerpoH : kCuerpoSoloH;

    std::vector<std::string> parrafos = diapo.parrafos;
    if (static_cast<int>(parrafos.size()) > opciones.max_parrafos) {
        r.avisos.push_back(
            "La diapositiva tiene " + std::to_string(parrafos.size()) +
            " párrafos; se muestran los primeros " +
            std::to_string(opciones.max_parrafos));
        parrafos.resize(static_cast<size_t>(opciones.max_parrafos));
    }

    if (!parrafos.empty()) {
        const int n = static_cast<int>(parrafos.size());
        const float slot = cuerpo_h / static_cast<float>(n);
        for (int i = 0; i < n; ++i) {
            PasoDibujo paso;
            paso.tipo   = PasoDibujo::Tipo::Cuerpo;
            paso.x      = kBandaX;
            paso.y      = cuerpo_y + static_cast<float>(i) * slot;
            paso.w      = kBandaW;
            paso.h      = slot;
            paso.texto  = Utf8AUtf16(parrafos[static_cast<size_t>(i)]);
            paso.estilo = estilo;
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
    const RectContenido contenido = Encajar(plan.aspecto, ancho, alto);
    for (const PasoDibujo& paso : plan.pasos) {
        const PasoDibujo q = Renormalizar(paso, contenido, ancho, alto);
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
    for (const PasoDibujo& paso : plan.pasos) {
        const PasoDibujo q = Renormalizar(paso, contenido,
                                          static_cast<float>(ancho),
                                          static_cast<float>(alto));
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
