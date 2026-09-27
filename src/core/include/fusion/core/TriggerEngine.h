// src/core/include/fusion/core/TriggerEngine.h
// Motor de Triggers — Sección 8.3 del doc técnico.
//
// Pieza NO contradictoria con el prompt original (100% offline):
//   - Eventos: locales (cambio de escenario/elemento/línea, etiqueta
//     semántica, inicio/fin de video, horario programado).
//   - Acciones: locales (cambio de tema/fondo, envío de mensaje a pantallas,
//     activación de salida Stage View, ejecución de script JS sandbox).
//
// Piezas OMITIDAS del doc porque SÍ contradicen el prompt original:
//   - MIDI/DMX (requiere hardware que no está garantizado en piso Win7)
//   - OBS WebSocket (es conexión de red)
//   - Llamadas API (es servidor HTTP, explícitamente prohibido)

#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <map>

namespace fusion {

enum class EventoTrigger {
    EscenarioProyectado,
    ElementoProyectado,
    LineaCambiada,
    EtiquetaDetectada,        // marca textual del .ahp ("lento", "calma"...)
    VideoInicio,
    VideoFin,
    HorarioProgramado,
};

enum class AccionTrigger {
    CambiarTema,
    CambiarFondo,
    MostrarMensaje,
    ActivarStageView,
    OcultarStageView,
    CambiarEscenario,
    EjecutarScriptSandbox,
};

struct Trigger {
    std::string id;
    std::string nombre;
    EventoTrigger evento;
    // Condición textual simple, p.ej. "etiqueta:lento" o "linea:>=3".
    // El motor evalúa la condición contra el contexto del evento.
    std::string condicion;
    AccionTrigger accion;
    // Parámetros de la acción (dependen del tipo):
    //   CambiarTema -> nombre del tema
    //   CambiarFondo -> ruta de imagen
    //   MostrarMensaje -> texto a mostrar
    //   CambiarEscenario -> id del escenario destino
    //   EjecutarScriptSandbox -> código JS
    std::string param;
    bool activo = true;
};

class TriggerEngine {
public:
    TriggerEngine();
    ~TriggerEngine();

    TriggerEngine(const TriggerEngine&)            = delete;
    TriggerEngine& operator=(const TriggerEngine&) = delete;

    // Cargar triggers desde JSON (modo offline: archivo local).
    bool Cargar(const std::string& ruta_json);

    // Disparar un evento. El motor evalúa las condiciones de los triggers
    // activos y ejecuta las acciones que correspondan.
    void Disparar(EventoTrigger evento,
                  const std::map<std::string, std::string>& contexto);

    // Listar / activar / desactivar triggers.
    std::vector<Trigger> Listar() const;
    bool Activar(const std::string& trigger_id);
    bool Desactivar(const std::string& trigger_id);

    // Callback que el motor invoca cuando una acción debe ejecutarse.
    // El núcleo (Engine) lo registra y lo enruta a su sistema interno.
    using EjecutorAccion = std::function<void(AccionTrigger, const std::string&)>;
    void SetEjecutor(EjecutorAccion cb);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace fusion
