// src/core/src/core/ProjectionWindow.cpp — Ventana borderless FS

#include "fusion/core/ProjectionWindow.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <vector>
#include <memory>
#include <string>
#include <functional>

namespace fusion {

namespace {

struct EnumData {
    std::vector<MonitorId> mons;
};

BOOL CALLBACK MonEnumProc(HMONITOR hMon, HDC, LPRECT lprcMonitor, LPARAM dwData) {
    MONITORINFOEXW mi; mi.cbSize = sizeof(mi);
    if (GetMonitorInfoW(hMon, &mi)) {
        EnumData* d = reinterpret_cast<EnumData*>(dwData);
        std::wstring name(mi.szDevice);
        std::string s(name.begin(), name.end());
        d->mons.push_back({s, s});
    }
    (void)lprcMonitor;
    return TRUE;
}

LRESULT CALLBACK WndProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        case WM_KEYDOWN:
            if (wp == VK_ESCAPE) {
                ShowWindow(h, SW_HIDE);
                return 0;
            }
            break;
        case WM_PAINT: {
            // SIN FillRect negro: aquí el dueño dibuja el contenido real.
            // El fondo de clase es nullptr para que el sistema no borre
            // con negro antes de repintar.
            PAINTSTRUCT ps;
            BeginPaint(h, &ps);
            EndPaint(h, &ps);
            // LONG_PTR → puntero: SOLO reinterpret_cast (static_cast de
            // entero a puntero no es válido y MSVC lo rechaza).
            auto cb = reinterpret_cast<std::function<void()>*>(
                GetWindowLongPtrW(h, GWLP_USERDATA));
            if (cb && *cb) (*cb)();
            return 0;
        }
    }
    return DefWindowProcW(h, msg, wp, lp);
}

const wchar_t* kClass = L"FusionHPProjectionWindow";

void RegistrarClase() {
    WNDCLASSEXW wc = {};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = GetModuleHandleW(nullptr);
    wc.lpszClassName = kClass;
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;   // el repintado dibuja TODO (nada de negro)
    static ATOM atom = RegisterClassExW(&wc);
    (void)atom;
}

} // namespace

struct ProjectionWindow::Impl {
    HWND hwnd = nullptr;
    MonitorId monitor_actual;
    bool visible = false;
    std::function<void()> repintado;

    ~Impl() { if (hwnd) DestroyWindow(hwnd); }
};

ProjectionWindow::ProjectionWindow()  : impl_(std::make_unique<Impl>()) {}
ProjectionWindow::~ProjectionWindow() = default;

void ProjectionWindow::SetRepintado(std::function<void()> cb) {
    impl_->repintado = std::move(cb);
    if (impl_->hwnd) {
        // WndProc no tiene acceso al Impl: el callback viaja por
        // GWLP_USERDATA (creado antes en Crear()).
        SetWindowLongPtrW(impl_->hwnd, GWLP_USERDATA,
                          reinterpret_cast<LONG_PTR>(&impl_->repintado));
    }
}

bool ProjectionWindow::Crear(const MonitorId& monitor) {
    RegistrarClase();
    impl_->monitor_actual = monitor;

    DWORD style = WS_POPUP;  // borderless
    DWORD ex    = WS_EX_TOPMOST;
    impl_->hwnd = CreateWindowExW(
        ex, kClass, L"FUSION-HP",
        style, 0, 0, 100, 100,
        nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (!impl_->hwnd) return false;

    if (impl_->repintado) {
        SetWindowLongPtrW(impl_->hwnd, GWLP_USERDATA,
                          reinterpret_cast<LONG_PTR>(&impl_->repintado));
    }

    // Empezar oculta
    ShowWindow(impl_->hwnd, SW_HIDE);
    return true;
}

bool ProjectionWindow::Mostrar() {
    if (!impl_->hwnd) return false;
    // SetWindowPos al monitor actual en pantalla completa borderless
    auto mons = ListarMonitores();
    for (auto& m : mons) {
        if (m.dispositivo == impl_->monitor_actual.dispositivo) {
            // TODO(P0): usar EnumDisplayMonitors para obtener el rect exacto.
            // Por ahora, simplemente maximizamos en el monitor actual.
            break;
        }
    }
    SetWindowPos(impl_->hwnd, HWND_TOPMOST, 0, 0,
                  GetSystemMetrics(SM_CXSCREEN),
                  GetSystemMetrics(SM_CYSCREEN),
                  SWP_SHOWWINDOW);
    impl_->visible = true;
    InvalidateRect(impl_->hwnd, nullptr, FALSE);
    return true;
}

void ProjectionWindow::Ocultar() {
    if (!impl_->hwnd) return;
    ShowWindow(impl_->hwnd, SW_HIDE);
    impl_->visible = false;
}

bool ProjectionWindow::CambiarMonitor(const MonitorId& monitor) {
    impl_->monitor_actual = monitor;
    if (impl_->visible) {
        Ocultar();
        Mostrar();
    }
    return true;
}

bool ProjectionWindow::EsVisible() const { return impl_->visible; }
void* ProjectionWindow::Hwnd() const { return static_cast<void*>(impl_->hwnd); }

std::vector<MonitorId> ProjectionWindow::ListarMonitores() {
    EnumData d;
    EnumDisplayMonitors(nullptr, nullptr, MonEnumProc, reinterpret_cast<LPARAM>(&d));
    return d.mons;
}

} // namespace fusion
