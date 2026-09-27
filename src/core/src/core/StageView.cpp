// src/core/src/core/StageView.cpp — Implementación Stage View (3 salidas locales)
//
// Respeta la regla #1 (100% offline): ninguna de las 3 salidas abre socket
// ni escucha puerto. Son 3 ventanas borderless locales.

#include "fusion/core/StageView.h"
#include "fusion/core/Renderer.h"
#include "fusion/core/ProjectionWindow.h"

#include <memory>
#include <mutex>
#include <map>

namespace fusion {

struct StageView::Impl {
    std::mutex m;
    EstadoStageView estado;
    std::map<TipoSalida, std::unique_ptr<ProjectionWindow>> ventanas;
    std::map<TipoSalida, bool> visibles;
};

StageView::StageView()  : impl_(std::make_unique<Impl>()) {}
StageView::~StageView() { OcultarTodas(); }

bool StageView::Crear(const MonitorId& publica,
                      const MonitorId& retorno,
                      const MonitorId& notas) {
    auto crear = [](TipoSalida t, const MonitorId& m, Impl* p) {
        if (m.dispositivo.empty()) return;
        auto w = std::make_unique<ProjectionWindow>();
        if (w->Crear(m)) {
            p->ventanas[t] = std::move(w);
            p->visibles[t] = false;
        }
    };
    crear(TipoSalida::Publica,  publica, impl_.get());
    crear(TipoSalida::Retorno,  retorno, impl_.get());
    crear(TipoSalida::Notas,    notas,   impl_.get());
    return !impl_->ventanas.empty();
}

void StageView::Actualizar(const EstadoStageView& estado) {
    std::lock_guard<std::mutex> lk(impl_->m);
    impl_->estado = estado;
    // TODO(P1): pedir repintado a los renderers de retorno y notas.
}

void StageView::Mostrar(TipoSalida tipo) {
    auto it = impl_->ventanas.find(tipo);
    if (it == impl_->ventanas.end() || !it->second) return;
    it->second->Mostrar();
    impl_->visibles[tipo] = true;
}

void StageView::Ocultar(TipoSalida tipo) {
    auto it = impl_->ventanas.find(tipo);
    if (it == impl_->ventanas.end() || !it->second) return;
    it->second->Ocultar();
    impl_->visibles[tipo] = false;
}

void StageView::OcultarTodas() {
    for (auto& kv : impl_->ventanas) {
        if (kv.second) {
            kv.second->Ocultar();
            impl_->visibles[kv.first] = false;
        }
    }
}

bool StageView::EsVisible(TipoSalida tipo) const {
    auto it = impl_->visibles.find(tipo);
    return it != impl_->visibles.end() && it->second;
}

} // namespace fusion
