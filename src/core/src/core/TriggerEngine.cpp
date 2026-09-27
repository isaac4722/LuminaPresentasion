// src/core/src/core/TriggerEngine.cpp — Implementación del motor de triggers
//
// 100% local: sin OBS, sin API HTTP, sin MIDI/DMX. Solo eventos locales
// (escenario/elemento/línea/etiqueta/video/horario) y acciones locales
// (cambiar tema, fondo, mensaje, Stage View, escenario, script sandbox).

#include "fusion/core/TriggerEngine.h"

#include "json.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <mutex>

namespace fusion {

struct TriggerEngine::Impl {
    std::mutex m;
    std::vector<Trigger> triggers;
    EjecutorAccion ejecutor;
};

TriggerEngine::TriggerEngine()  : impl_(std::make_unique<Impl>()) {}
TriggerEngine::~TriggerEngine() = default;

bool TriggerEngine::Cargar(const std::string& ruta_json) {
    std::ifstream f(ruta_json, std::ios::binary);
    if (!f) return false;
    std::stringstream ss; ss << f.rdbuf();
    auto j = nlohmann::json::parse(ss.str(), nullptr, false);
    if (j.is_discarded()) return false;

    std::lock_guard<std::mutex> lk(impl_->m);
    impl_->triggers.clear();
    for (const auto& tj : j.value("triggers", nlohmann::json::array())) {
        Trigger t;
        t.id        = tj.value("id", "");
        t.nombre    = tj.value("nombre", "");
        t.condicion = tj.value("condicion", "");
        t.param     = tj.value("param", "");
        t.activo    = tj.value("activo", true);

        const std::string ev_s = tj.value("evento", "");
        if      (ev_s == "escenario_proyectado") t.evento = EventoTrigger::EscenarioProyectado;
        else if (ev_s == "elemento_proyectado")   t.evento = EventoTrigger::ElementoProyectado;
        else if (ev_s == "linea_cambiada")        t.evento = EventoTrigger::LineaCambiada;
        else if (ev_s == "etiqueta_detectada")    t.evento = EventoTrigger::EtiquetaDetectada;
        else if (ev_s == "video_inicio")          t.evento = EventoTrigger::VideoInicio;
        else if (ev_s == "video_fin")             t.evento = EventoTrigger::VideoFin;
        else if (ev_s == "horario")               t.evento = EventoTrigger::HorarioProgramado;
        else continue;

        const std::string ac_s = tj.value("accion", "");
        if      (ac_s == "cambiar_tema")      t.accion = AccionTrigger::CambiarTema;
        else if (ac_s == "cambiar_fondo")      t.accion = AccionTrigger::CambiarFondo;
        else if (ac_s == "mostrar_mensaje")   t.accion = AccionTrigger::MostrarMensaje;
        else if (ac_s == "activar_stage_view") t.accion = AccionTrigger::ActivarStageView;
        else if (ac_s == "ocultar_stage_view") t.accion = AccionTrigger::OcultarStageView;
        else if (ac_s == "cambiar_escenario") t.accion = AccionTrigger::CambiarEscenario;
        else if (ac_s == "ejecutar_script")    t.accion = AccionTrigger::EjecutarScriptSandbox;
        else continue;

        impl_->triggers.push_back(std::move(t));
    }
    return true;
}

// Evaluación simple de condición: "clave:valor" o "clave:>=N" o "clave:<=N"
static bool EvaluarCondicion(const std::string& cond,
                             const std::map<std::string, std::string>& ctx) {
    if (cond.empty()) return true;
    auto pos = cond.find(':');
    if (pos == std::string::npos) return false;
    std::string clave = cond.substr(0, pos);
    std::string espejo = cond.substr(pos + 1);
    auto it = ctx.find(clave);
    if (it == ctx.end()) return false;
    if (espejo.rfind(">=", 0) == 0) {
        try { return std::stoi(it->second) >= std::stoi(espejo.substr(2)); }
        catch (...) { return false; }
    }
    if (espejo.rfind("<=", 0) == 0) {
        try { return std::stoi(it->second) <= std::stoi(espejo.substr(2)); }
        catch (...) { return false; }
    }
    return it->second == espejo;
}

void TriggerEngine::Disparar(EventoTrigger evento,
                              const std::map<std::string, std::string>& contexto) {
    std::lock_guard<std::mutex> lk(impl_->m);
    if (!impl_->ejecutor) return;
    for (auto& t : impl_->triggers) {
        if (!t.activo) continue;
        if (t.evento != evento) continue;
        if (!EvaluarCondicion(t.condicion, contexto)) continue;
        impl_->ejecutor(t.accion, t.param);
    }
}

std::vector<Trigger> TriggerEngine::Listar() const {
    std::lock_guard<std::mutex> lk(impl_->m);
    return impl_->triggers;
}

bool TriggerEngine::Activar(const std::string& trigger_id) {
    std::lock_guard<std::mutex> lk(impl_->m);
    for (auto& t : impl_->triggers) {
        if (t.id == trigger_id) { t.activo = true; return true; }
    }
    return false;
}

bool TriggerEngine::Desactivar(const std::string& trigger_id) {
    std::lock_guard<std::mutex> lk(impl_->m);
    for (auto& t : impl_->triggers) {
        if (t.id == trigger_id) { t.activo = false; return true; }
    }
    return false;
}

void TriggerEngine::SetEjecutor(EjecutorAccion cb) {
    std::lock_guard<std::mutex> lk(impl_->m);
    impl_->ejecutor = std::move(cb);
}

} // namespace fusion
