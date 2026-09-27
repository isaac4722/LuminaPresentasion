// src/core/include/fusion/core/StageView.h
// Stage View (monitor de retorno de músicos) — Sección 8.5 del doc técnico,
// pieza no-contradictoria: 100% local, sin red ni nube.
//
// Hasta 3 salidas simultáneas independientes:
//   1. Pantalla Pública (la congregación)
//   2. Pantalla de Retorno (músicos: letra, línea activa, cronómetro)
//   3. Pantalla de Notas (notas internas del director)
//
// Implementado sobre ProjectionWindow (borderless) con render Direct2D.

#pragma once

#include "fusion/core/ProjectionWindow.h"
#include <memory>
#include <string>

namespace fusion {

enum class TipoSalida {
    Publica,
    Retorno,
    Notas,
};

struct EstadoStageView {
    std::wstring titulo_canto;
    std::wstring linea_activa;
    std::wstring linea_siguiente;
    std::wstring cita_actual;
    std::wstring tono_actual;        // p.ej. "C", "Am", "F#"
    int         bpm = 0;
    int         segundos_restantes = 0;  // temporizador regresivo
    bool        alerta_dorada = false;   // alerta visible al músico
};

class StageView {
public:
    StageView();
    ~StageView();

    StageView(const StageView&)            = delete;
    StageView& operator=(const StageView&) = delete;

    // Crear las 3 salidas en los monitores indicados (vacío = no usar).
    bool Crear(const MonitorId& publica,
               const MonitorId& retorno,
               const MonitorId& notas);

    // Actualizar el estado que las salidas de retorno y notas muestran.
    void Actualizar(const EstadoStageView& estado);

    // Mostrar / ocultar las 3 salidas.
    void Mostrar(TipoSalida tipo);
    void Ocultar(TipoSalida tipo);
    void OcultarTodas();

    // ¿Está activa una salida?
    bool EsVisible(TipoSalida tipo) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace fusion
