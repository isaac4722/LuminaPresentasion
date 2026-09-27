// src/core/src/core/Renderer.cpp — Implementación Direct2D + DirectWrite + GDI+
// Stub fundacional: parámetros stub con dibujo mínimo negro.

#include "fusion/core/Renderer.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>
#include <wincodec.h>
#include <gdiplus.h>
#include <wrl/client.h>

#include <string>

using Microsoft::WRL::ComPtr;

namespace fusion {

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

// -------------------------------------------------------------------------
// RendererNull — para tests
// -------------------------------------------------------------------------
class RendererNull : public Renderer {
public:
    bool Inicializar(void*) override { return true; }
    void Liberar() override {}
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
        return SUCCEEDED(hr);
    }

    void Liberar() override {
        rt_.Reset();
        dw_.Reset();
        d2d_.Reset();
    }

    void Limpiar() override {
        if (rt_) rt_->Clear(D2D1::ColorF(D2D1::ColorF::Black));
    }

    void Presentar() override {
        if (rt_) rt_->Flush();
    }

    void DibujarFondo(const Fondo& f) override {
        if (!rt_) return;
        ComPtr<ID2D1SolidColorBrush> brush;
        D2D1_COLOR_F col = D2D1::ColorF(f.color1.r/255.0f, f.color1.g/255.0f, f.color1.b/255.0f, 1.0f);
        rt_->CreateSolidColorBrush(col, brush.GetAddressOf());
        D2D1_SIZE_F size = rt_->GetSize();
        rt_->FillRectangle(D2D1::RectF(0,0,size.width,size.height), brush.Get());
    }

    void DibujarTexto(const std::wstring& texto, const EstiloTexto& estilo,
                       float x, float y, float w, float h) override {
        // TODO(P0): usar IDWriteTextFormat + ID2D1SolidColorBrush.
        (void)texto; (void)estilo; (void)x; (void)y; (void)w; (void)h;
    }

    void DibujarImagen(const std::wstring& ruta, float x, float y, float w, float h,
                        AjusteImagen) override {
        // TODO(P0): usar WIC para cargar bitmap, crear ID2D1Bitmap, dibujar.
        (void)ruta; (void)x; (void)y; (void)w; (void)h;
    }

private:
    HWND hwnd_ = nullptr;
    ComPtr<ID2D1Factory> d2d_;
    ComPtr<ID2D1HwndRenderTarget> rt_;
    ComPtr<IDWriteFactory> dw_;
};

std::unique_ptr<Renderer> CrearRendererDirect2D() {
    return std::make_unique<RendererDirect2D>();
}

} // namespace fusion
