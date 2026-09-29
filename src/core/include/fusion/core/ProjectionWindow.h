// src/core/include/fusion/core/ProjectionWindow.h
// Ventana de proyección borderless a pantalla completa.
// Nace OCULTA y solo aparece al iniciar la presentación. Esc la oculta.
// Nunca se destruye entre usos -> cero parpadeo.

#pragma once

#include <functional>
#include <string>
#include <memory>
#include <vector>

namespace fusion {

class Renderer;

// Identifica un monitor por nombre de dispositivo (GDI DISPLAY).
struct MonitorId {
    std::string dispositivo;   // ej. "\\.\DISPLAY1"
    std::string nombre;        // friendly name
};

class ProjectionWindow {
public:
    ProjectionWindow();
    ~ProjectionWindow();

    ProjectionWindow(const ProjectionWindow&)            = delete;
    ProjectionWindow& operator=(const ProjectionWindow&) = delete;

    // Crea la ventana oculta. No la muestra.
    bool Crear(const MonitorId& monitor);

    // Muestra en pantalla completa borderless.
    bool Mostrar();

    // Oculta (sin destruir). Cero parpadeo.
    void Ocultar();

    // Cambia de monitor en caliente (mantiene la misma ventana).
    bool CambiarMonitor(const MonitorId& monitor);

    bool EsVisible() const;
    void* Hwnd() const;  // HWND para que Renderer se enganche

    // Callback de repintado: WM_PAINT lo invoca para que el dueño
    // (Engine) vuelva a dibujar el contenido actual. Sin esto, cualquier
    // invalidación del sistema borraba la proyección a negro.
    void SetRepintado(std::function<void()> cb);

    // Lista los monitores disponibles (con nombre amigable).
    static std::vector<MonitorId> ListarMonitores();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace fusion
