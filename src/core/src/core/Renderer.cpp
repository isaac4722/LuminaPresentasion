// src/core/src/core/Renderer.cpp — Implementación Direct2D + DirectWrite + GDI+
// El pintado real (fondo sólido/gradiente/imagen, texto con DirectWrite,
// imágenes con WIC) vive en las rutinas compartidas de
// RenderDirectoInterno.h: pantalla y rasterizador offscreen del modo
// directo usan EXACTAMENTE la misma ruta (una sola fuente de verdad).

#include "fusion/core/Renderer.h"
#include "fusion/core/RenderDirecto.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
// El alias DrawText→DrawTextW de winuser rompe el método DrawText de
// ID2D1RenderTarget. Este proyecto no usa el DrawText de GDI.
#ifdef DrawText
#undef DrawText
#endif
#include <d2d1.h>
#include <dwrite.h>
#include <wincodec.h>
#include <gdiplus.h>
#include <wrl/client.h>

#include "RenderDirectoInterno.h"

#include <string>

using Microsoft::WRL::ComPtr;

namespace fusion {

// Color::DesdeHex vive en RenderDirecto.cpp (parte portable): la usa
// tanto el render directo (estilos por-run) como RendererDirect2D.

// -------------------------------------------------------------------------
// RendererNull — para tests
// -------------------------------------------------------------------------
class RendererNull : public Renderer {
public:
    bool Inicializar(void*) override { return true; }
    void Liberar() override {}
    bool Redimensionar(int, int) override { return true; }
    void Limpiar() override {}
    void Presentar() override {}
    void DibujarFondo(const Fondo&) override {}
    void DibujarTexto(const std::wstring&, const EstiloTexto&, float, float, float, float) override {}
    void DibujarImagen(const std::wstring&, float, float, float, float, AjusteImagen) override {}
};

std::unique_ptr<Renderer> CrearRendererNull() {
    return std::make_unique<RendererNull>();
}

// -------------------------------------------------------------------------
// RendererDirect2D — implementación real (stub inicial con factorías creadas)
// -------------------------------------------------------------------------
class RendererDirect2D : public Renderer {
public:
    RendererDirect2D() = default;
    ~RendererDirect2D() override { Liberar(); }

    bool Inicializar(void* hwnd_salida) override {
        hwnd_ = static_cast<HWND>(hwnd_salida);

        HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,
                                        IID_PPV_ARGS(&d2d_));
        if (FAILED(hr)) return false;

        hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,
                                  __uuidof(IDWriteFactory),
                                  reinterpret_cast<IUnknown**>(dw_.GetAddressOf()));
        if (FAILED(hr)) return false;

        // Crear render target sobre hwnd_. En el cimiento, basta con
        // tener las factorías; el render target se crea con BindDC.
        RECT rc; GetClientRect(hwnd_, &rc);
        D2D1_SIZE_U size = D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top);
        auto props = D2D1::RenderTargetProperties(
            D2D1_RENDER_TARGET_TYPE_HARDWARE,
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
        hr = d2d_->CreateHwndRenderTarget(
            props,
            D2D1::HwndRenderTargetProperties(hwnd_, size),
            &rt_);
        if (FAILED(hr)) return false;

        // WIC para fondos/imágenes de elemento. No es fatal si falla:
        // sólido/gradiente siguen; imagen caerá a color1 con aviso.
        CoCreateInstance(CLSID_WICImagingFactory, nullptr,
                         CLSCTX_INPROC_SERVER,
                         IID_PPV_ARGS(wic_.GetAddressOf()));
        return true;
    }

    void Liberar() override {
        rt_.Reset();
        dw_.Reset();
        d2d_.Reset();
    }

    bool Redimensionar(int w, int h) override {
        if (!rt_ || w <= 0 || h <= 0) return false;
        return SUCCEEDED(rt_->Resize(D2D1::SizeU(
            static_cast<UINT32>(w), static_cast<UINT32>(h))));
    }

    void Limpiar() override {
        if (rt_) rt_->Clear(D2D1::ColorF(D2D1::ColorF::Black));
    }

    void Presentar() override {
        if (rt_) rt_->Flush();
    }

    void DibujarFondo(const Fondo& f) override {
        if (!rt_) return;
        const D2D1_SIZE_F s = rt_->GetSize();
        rendirecto::PintarFondoEnRT(rt_.Get(), dw_.Get(), wic_.Get(), f,
                                    &formatos_, s.width, s.height, nullptr);
    }

    void DibujarTexto(const std::wstring& texto, const EstiloTexto& estilo,
                       float x, float y, float w, float h) override {
        if (!rt_) return;
        const D2D1_SIZE_F s = rt_->GetSize();
        if (s.width <= 0 || s.height <= 0) return;
        // Coordenadas absolutas del caller → normalizadas para las
        // rutinas compartidas (misma ruta que el modo directo).
        PasoDibujo paso;
        paso.x = x / s.width;  paso.y = y / s.height;
        paso.w = w / s.width;  paso.h = h / s.height;
        paso.texto  = texto;
        paso.estilo = estilo;
        rendirecto::DibujarPasoEnRT(rt_.Get(), dw_.Get(), paso, &formatos_,
                                    s.width, s.height);
    }

    void DibujarImagen(const std::wstring& ruta, float x, float y, float w, float h,
                        AjusteImagen ajuste) override {
        if (!rt_) return;
        rendirecto::DibujarImagenEnRT(rt_.Get(), wic_.Get(), ruta, ajuste,
                                      x, y, w, h);
    }

    void DibujarImagenMemoria(const unsigned char* rgba, int ancho_px,
                              int alto_px, float x, float y, float w,
                              float h) override {
        if (!rt_) return;
        rendirecto::DibujarImagenMemoriaEnRT(rt_.Get(), wic_.Get(), rgba,
                                             ancho_px, alto_px, x, y, w, h);
    }

private:
    HWND hwnd_ = nullptr;
    ComPtr<ID2D1Factory> d2d_;
    ComPtr<ID2D1HwndRenderTarget> rt_;
    ComPtr<IDWriteFactory> dw_;
    ComPtr<IWICImagingFactory> wic_;
    rendirecto::CacheFormatos formatos_;
};

std::unique_ptr<Renderer> CrearRendererDirect2D() {
    return std::make_unique<RendererDirect2D>();
}

} // namespace fusion
