// src/core/include/fusion/core/Engine.h
// Motor de proyección: dueño del estado. Sobrevive a cierres de la carcasa.

#pragma once

#include "fusion/Version.h"
#include "fusion/core/Session.h"
#include "fusion/data/BibleDatabase.h"
#include "fusion/data/SongDatabase.h"

#include <cstdint>
#include <memory>
#include <string>
#include <functional>

namespace fusion {

// Snapshot del estado actual del motor. Se devuelve por IPC y se persiste.
struct EstadoMotor {
    std::string programa_ruta;        // path del .ahp cargado (vacío si ninguno)
    std::string programa_titulo;
    std::string escenario_id;
    std::string escenario_nombre;
    std::string elemento_id;
    std::string elemento_titulo;
    int         linea_actual   = 0;
    bool        salida_visible = false;
    bool        negro           = false;
    bool        logo            = false;
    std::string monitor_dispositivo;
};

// Tipo de evento emitido por el motor. Los clientes IPC se suscriben.
enum class EventoMotor {
    EstadoCambiado,      // cambió escenario/elemento/línea
    SalidaCambiada,       // visible/oculta/negro/logo
    ProgramaCargado,
    ProgramaCerrado,
    MonitorCambiado,
};

using SuscripcionEventos = std::function<void(EventoMotor, const EstadoMotor&)>;

// Engine es el dueño único del estado de proyección.
// La carcasa (C#/Qt) lo opera exclusivamente por IPC.
class Engine {
public:
    Engine();
    ~Engine();

    Engine(const Engine&)            = delete;
    Engine& operator=(const Engine&) = delete;

    // Ciclo de vida ----------------------------------------------------
    bool Iniciar();          // Abre BDs, crea ventana de proyección oculta.
    void Detener();           // Persiste sesión y cierra limpio.

    // Estado -----------------------------------------------------------
    EstadoMotor Estado() const;
    void Suscribir(SuscripcionEventos cb);

    // Programa ---------------------------------------------------------
    bool AbrirPrograma(const std::string& ruta);
    bool NuevoPrograma(const std::string& titulo);
    bool GuardarPrograma(const std::string& ruta /* opcional */);
    bool CerrarPrograma();
    std::vector<std::string> Recientes() const;

    // Proyección -------------------------------------------------------
    bool IniciarProyeccion();
    bool DetenerProyeccion();
    bool IrEscenario(const std::string& escenario_id);
    bool IrElemento(const std::string& escenario_id,
                    const std::string& elemento_id);
    bool IrLinea(const std::string& escenario_id,
                 const std::string& elemento_id,
                 int linea);
    bool Siguiente();
    bool Anterior();
    bool SetNegro(bool activo);
    bool SetLogo(bool activo);
    bool OcultarSalida();

    // Monitor ----------------------------------------------------------
    struct Monitor {
        std::string dispositivo;
        std::string nombre;
        int x, y, w, h;
        bool primario;
    };
    std::vector<Monitor> ListarMonitores() const;
    bool SeleccionarMonitor(const std::string& dispositivo);

    // Biblia -----------------------------------------------------------
    std::vector<std::string> ListarBiblias() const;
    bool ObtenerVersiculo(const std::string& biblia,
                          const std::string& cita,
                          std::string* texto_out) const;
    // Búsqueda libre (FTS5) sobre la biblia activa.
    std::vector<Versiculo> BuscarEnBiblia(const std::string& texto,
                                          int limite) const;
    std::vector<std::string> FavoritosBiblia() const;

    // Cantos -----------------------------------------------------------
    // (la BD los sirve directamente; no se generan PPTX ni archivos intermedios)
    std::vector<std::string> ListarCantos() const;
    std::vector<Canto> BuscarCantos(const std::string& texto, int limite) const;
    bool ObtenerCanto(std::int64_t id, CantoDetalle* out) const;

    // Sesión -----------------------------------------------------------
    bool CargarSesion();      // Lee runtime/session.json
    bool GuardarSesion();     // Escribe runtime/session.json

    // Diagnóstico --------------------------------------------------------
    // ¿Hay biblia activa con contenido? (para diag.autotest y avisos)
    bool BibliaLista() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace fusion
