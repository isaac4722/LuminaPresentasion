// src/core/src/core/Engine.cpp — Implementación del motor (stub fundacional)

#include "fusion/core/Engine.h"
#include "fusion/core/Session.h"
#include "fusion/core/Renderer.h"
#include "fusion/core/ProjectionWindow.h"
#include "fusion/core/IpcServer.h"
#include "fusion/data/SongDatabase.h"
#include "fusion/data/BibleDatabase.h"
#include "fusion/data/AhpFormat.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <vector>

namespace fusion {

struct Engine::Impl {
    std::mutex m;
    EstadoMotor estado;
    std::vector<SuscripcionEventos> suscriptores;
    std::unique_ptr<ProjectionWindow> ventana;
    std::unique_ptr<Renderer> renderer;
    std::unique_ptr<SongDatabase> canciones;
    std::unique_ptr<BibleDatabase> biblia_activa;
    std::atomic<bool> corriendo{false};

    void Emitir(EventoMotor e) {
        std::lock_guard<std::mutex> lk(m);
        for (auto& cb : suscriptores) if (cb) cb(e, estado);
    }
};

Engine::Engine()  : impl_(std::make_unique<Impl>()) {}
Engine::~Engine() { Detener(); }

bool Engine::Iniciar() {
    if (impl_->corriendo.exchange(true)) return true;

    // Cargar sesión persistente (runtime/session.json)
    Sesion s;
    if (Sesion::Cargar(Sesion::RutaPorDefecto(), &s)) {
        impl_->estado.programa_ruta       = s.programa_ruta;
        impl_->estado.escenario_id        = s.escenario_id;
        impl_->estado.elemento_id         = s.elemento_id;
        impl_->estado.linea_actual        = s.linea_actual;
        impl_->estado.salida_visible      = s.salida_visible;
        impl_->estado.negro               = s.negro;
        impl_->estado.logo                 = s.logo;
        impl_->estado.monitor_dispositivo = s.monitor_dispositivo;
    }

    // Crear ventana de proyección oculta
    impl_->ventana = std::make_unique<ProjectionWindow>();
    if (!impl_->estado.monitor_dispositivo.empty()) {
        MonitorId m{impl_->estado.monitor_dispositivo, ""};
        impl_->ventana->Crear(m);
    } else {
        auto mons = ProjectionWindow::ListarMonitores();
        if (!mons.empty()) impl_->ventana->Crear(mons.front());
    }

    // Renderer Direct2D enganchado a la ventana
    impl_->renderer = CrearRendererDirect2D();
    if (impl_->ventana && impl_->ventana->Hwnd()) {
        impl_->renderer->Inicializar(impl_->ventana->Hwnd());
    }

    // BDs
    impl_->canciones = std::make_unique<SongDatabase>();
    impl_->canciones->Abrir("data/cancionero.fdb");

    impl_->biblia_activa = std::make_unique<BibleDatabase>();
    impl_->biblia_activa->Abrir("data/bibles/RVR1909.fdb");

    return true;
}

void Engine::Detener() {
    if (!impl_->corriendo.exchange(false)) return;
    GuardarSesion();
    if (impl_->ventana) impl_->ventana->Ocultar();
    if (impl_->renderer) impl_->renderer->Liberar();
    if (impl_->canciones) impl_->canciones->Cerrar();
    if (impl_->biblia_activa) impl_->biblia_activa->Cerrar();
}

EstadoMotor Engine::Estado() const {
    std::lock_guard<std::mutex> lk(impl_->m);
    return impl_->estado;
}

void Engine::Suscribir(SuscripcionEventos cb) {
    std::lock_guard<std::mutex> lk(impl_->m);
    impl_->suscriptores.push_back(std::move(cb));
}

// --- Programa ----------------------------------------------------------
bool Engine::AbrirPrograma(const std::string& ruta) {
    Programa p;
    std::string err;
    if (!AhpFormat::Cargar(ruta, &p, &err)) return false;
    std::lock_guard<std::mutex> lk(impl_->m);
    impl_->estado.programa_ruta   = ruta;
    impl_->estado.programa_titulo = p.titulo;
    impl_->estado.escenario_id.clear();
    impl_->estado.elemento_id.clear();
    impl_->estado.linea_actual = 0;
    impl_->Emitir(EventoMotor::ProgramaCargado);
    return true;
}

bool Engine::NuevoPrograma(const std::string& titulo) {
    std::lock_guard<std::mutex> lk(impl_->m);
    impl_->estado.programa_ruta.clear();
    impl_->estado.programa_titulo = titulo;
    impl_->estado.escenario_id.clear();
    impl_->estado.elemento_id.clear();
    impl_->Emitir(EventoMotor::ProgramaCargado);
    return true;
}

bool Engine::GuardarPrograma(const std::string& ruta) {
    // TODO(P0): serializar Programa actual con AhpFormat::Guardar.
    return !ruta.empty();
}

bool Engine::CerrarPrograma() {
    std::lock_guard<std::mutex> lk(impl_->m);
    impl_->estado.programa_ruta.clear();
    impl_->estado.programa_titulo.clear();
    impl_->estado.escenario_id.clear();
    impl_->estado.elemento_id.clear();
    impl_->Emitir(EventoMotor::ProgramaCerrado);
    return true;
}

std::vector<std::string> Engine::Recientes() const {
    Sesion s;
    if (!Sesion::Cargar(Sesion::RutaPorDefecto(), &s)) return {};
    std::vector<std::string> r;
    r.reserve(s.recientes.size());
    for (auto& rec : s.recientes) r.push_back(rec.ruta);
    return r;
}

// --- Proyección --------------------------------------------------------
bool Engine::IniciarProyeccion() {
    if (!impl_->ventana) return false;
    impl_->ventana->Mostrar();
    std::lock_guard<std::mutex> lk(impl_->m);
    impl_->estado.salida_visible = true;
    impl_->Emitir(EventoMotor::SalidaCambiada);
    return true;
}

bool Engine::DetenerProyeccion() {
    if (!impl_->ventana) return false;
    impl_->ventana->Ocultar();
    std::lock_guard<std::mutex> lk(impl_->m);
    impl_->estado.salida_visible = false;
    impl_->Emitir(EventoMotor::SalidaCambiada);
    return true;
}

bool Engine::IrEscenario(const std::string& escenario_id) {
    std::lock_guard<std::mutex> lk(impl_->m);
    impl_->estado.escenario_id = escenario_id;
    impl_->estado.elemento_id.clear();
    impl_->estado.linea_actual = 0;
    impl_->Emitir(EventoMotor::EstadoCambiado);
    return true;
}

bool Engine::IrElemento(const std::string& escenario_id,
                        const std::string& elemento_id) {
    std::lock_guard<std::mutex> lk(impl_->m);
    impl_->estado.escenario_id = escenario_id;
    impl_->estado.elemento_id  = elemento_id;
    impl_->estado.linea_actual = 0;
    impl_->Emitir(EventoMotor::EstadoCambiado);
    return true;
}

bool Engine::IrLinea(const std::string& escenario_id,
                      const std::string& elemento_id,
                      int linea) {
    std::lock_guard<std::mutex> lk(impl_->m);
    impl_->estado.escenario_id = escenario_id;
    impl_->estado.elemento_id  = elemento_id;
    impl_->estado.linea_actual = linea;
    impl_->Emitir(EventoMotor::EstadoCambiado);
    return true;
}

bool Engine::Siguiente() {
    // TODO(P0): navegar dentro del programa cargado.
    impl_->Emitir(EventoMotor::EstadoCambiado);
    return true;
}

bool Engine::Anterior() {
    impl_->Emitir(EventoMotor::EstadoCambiado);
    return true;
}

bool Engine::SetNegro(bool activo) {
    std::lock_guard<std::mutex> lk(impl_->m);
    impl_->estado.negro = activo;
    impl_->Emitir(EventoMotor::SalidaCambiada);
    return true;
}

bool Engine::SetLogo(bool activo) {
    std::lock_guard<std::mutex> lk(impl_->m);
    impl_->estado.logo = activo;
    impl_->Emitir(EventoMotor::SalidaCambiada);
    return true;
}

bool Engine::OcultarSalida() {
    return DetenerProyeccion();
}

// --- Monitor -----------------------------------------------------------
std::vector<Engine::Monitor> Engine::ListarMonitores() const {
    auto ids = ProjectionWindow::ListarMonitores();
    std::vector<Monitor> out;
    out.reserve(ids.size());
    for (auto& id : ids) out.push_back({id.dispositivo, id.nombre, 0,0,0,0,false});
    return out;
}

bool Engine::SeleccionarMonitor(const std::string& dispositivo) {
    if (!impl_->ventana) return false;
    MonitorId m{dispositivo, ""};
    impl_->ventana->CambiarMonitor(m);
    std::lock_guard<std::mutex> lk(impl_->m);
    impl_->estado.monitor_dispositivo = dispositivo;
    impl_->Emitir(EventoMotor::MonitorCambiado);
    return true;
}

// --- Biblia ------------------------------------------------------------
std::vector<std::string> Engine::ListarBiblias() const {
    // TODO(P0): barrer data/bibles/*.fdb
    return { "RVR1909", "RV1960", "NVI", "RVG" };
}

bool Engine::ObtenerVersiculo(const std::string& biblia,
                              const std::string& cita,
                              std::string* texto_out) const {
    if (!impl_->biblia_activa) return false;
    std::vector<Versiculo> v;
    if (!impl_->biblia_activa->ObtenerCita(cita, &v)) return false;
    for (auto& vv : v) { *texto_out += vv.texto + " "; }
    return true;
}

// --- Cantos ------------------------------------------------------------
std::vector<std::string> Engine::ListarCantos() const {
    if (!impl_->canciones) return {};
    auto lista = impl_->canciones->ListarTodos();
    std::vector<std::string> out;
    out.reserve(lista.size());
    for (auto& c : lista) out.push_back(c.titulo);
    return out;
}

// --- Sesión ------------------------------------------------------------
bool Engine::CargarSesion() {
    Sesion s;
    if (!Sesion::Cargar(Sesion::RutaPorDefecto(), &s)) return false;
    std::lock_guard<std::mutex> lk(impl_->m);
    impl_->estado.programa_ruta       = s.programa_ruta;
    impl_->estado.escenario_id        = s.escenario_id;
    impl_->estado.elemento_id         = s.elemento_id;
    impl_->estado.linea_actual        = s.linea_actual;
    impl_->estado.salida_visible      = s.salida_visible;
    impl_->estado.negro               = s.negro;
    impl_->estado.logo                 = s.logo;
    impl_->estado.monitor_dispositivo = s.monitor_dispositivo;
    return true;
}

bool Engine::GuardarSesion() {
    Sesion s;
    s.programa_ruta       = impl_->estado.programa_ruta;
    s.escenario_id        = impl_->estado.escenario_id;
    s.elemento_id         = impl_->estado.elemento_id;
    s.linea_actual        = impl_->estado.linea_actual;
    s.salida_visible      = impl_->estado.salida_visible;
    s.negro               = impl_->estado.negro;
    s.logo                 = impl_->estado.logo;
    s.monitor_dispositivo = impl_->estado.monitor_dispositivo;
    return Sesion::Guardar(Sesion::RutaPorDefecto(), s);
}

} // namespace fusion
