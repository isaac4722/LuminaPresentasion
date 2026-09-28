// src/core/src/core/Engine.cpp — Implementación del motor
//
// El motor es el dueño único del estado de proyección: carga sesión y BDs,
// guarda el programa cargado (navegación real siguiente/anterior y
// GuardarPrograma), y expone monitores. La carcasa solo lo opera por IPC.

#include "fusion/core/Engine.h"
#include "fusion/core/Session.h"
#include "fusion/core/Renderer.h"
#include "fusion/core/ProjectionWindow.h"
#include "fusion/core/IpcServer.h"
#include "fusion/core/Rutas.h"
#include "fusion/data/SongDatabase.h"
#include "fusion/data/BibleDatabase.h"
#include "fusion/data/AhpFormat.h"
#include "Navegacion.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

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

    // Programa cargado (navegación real y GuardarPrograma).
    Programa programa_actual;
    bool     hay_programa = false;

    void Emitir(EventoMotor e) {
        // Ya llega con el mutex del estado tomado (o en fase de salida).
        for (auto& cb : suscriptores) if (cb) cb(e, estado);
    }
};

Engine::Engine()  : impl_(std::make_unique<Impl>()) {}
Engine::~Engine() { Detener(); }

namespace {

using rutas::ExisteArchivo;

// Primer arranque: si RVR1909.fdb no existe en la raíz de datos, se
// siembra desde el asset de solo lectura <exe>\data\bibles\RVR1909.json
// (importador JSON integrado; 100% offline).
void SembrarBibliaRVR1909SiFalta() {
    std::wstring destino = rutas::CarpetaBibliasW() + L"\\RVR1909.fdb";
    if (ExisteArchivo(destino)) return;
    std::string semilla = rutas::SemillaRVR1909Json();
    if (semilla.empty()) {
        rutas::Bitacora("AVISO: sin semilla RVR1909.json junto al ejecutable; "
                        "la biblia quedará vacía");
        return;
    }
    BibleDatabase b;
    if (!b.Abrir(rutas::BibliaFdb("RVR1909.fdb"))) {
        rutas::Bitacora("AVISO: no se pudo crear RVR1909.fdb en la raíz de datos");
        return;
    }
    int n = b.ImportarJson(semilla);
    b.Cerrar();
    rutas::Bitacora("RVR1909.fdb sembrada con " + std::to_string(n) +
                    " versículos");
}

} // namespace

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

    // BDs en la raíz de datos ESCRIBIBLE (portable: junto al exe;
    // instalado: %LOCALAPPDATA%\FUSION-HP). Sin esto, en Program Files
    // sqlite no podía crear los .fdb y las BD arrancaban muertas.
    impl_->canciones = std::make_unique<SongDatabase>();
    if (impl_->canciones->Abrir(rutas::CancioneroFdb())) {
        rutas::Bitacora("cancionero.fdb listo en la raíz de datos");
    } else {
        rutas::Bitacora("AVISO: no se pudo abrir/crear cancionero.fdb en "
                        "la raíz de datos");
    }

    SembrarBibliaRVR1909SiFalta();
    impl_->biblia_activa = std::make_unique<BibleDatabase>();
    if (impl_->biblia_activa->Abrir(rutas::BibliaFdb("RVR1909.fdb"))) {
        rutas::Bitacora("biblia activa: RVR1909 (" +
                        std::to_string(impl_->biblia_activa->TotalVersiculos()) +
                        " versículos)");
    } else {
        rutas::Bitacora("AVISO: no se pudo abrir/crear RVR1909.fdb");
    }

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
    impl_->programa_actual = std::move(p);
    impl_->hay_programa    = true;
    impl_->estado.programa_ruta   = ruta;
    impl_->estado.programa_titulo = impl_->programa_actual.titulo;
    impl_->estado.escenario_id.clear();
    impl_->estado.elemento_id.clear();
    impl_->estado.linea_actual = 0;
    impl_->Emitir(EventoMotor::ProgramaCargado);
    return true;
}

bool Engine::NuevoPrograma(const std::string& titulo) {
    std::lock_guard<std::mutex> lk(impl_->m);
    impl_->programa_actual = Programa{};
    impl_->programa_actual.titulo = titulo;
    impl_->hay_programa = true;
    impl_->estado.programa_ruta.clear();
    impl_->estado.programa_titulo = titulo;
    impl_->estado.escenario_id.clear();
    impl_->estado.elemento_id.clear();
    impl_->Emitir(EventoMotor::ProgramaCargado);
    return true;
}

bool Engine::GuardarPrograma(const std::string& ruta) {
    std::lock_guard<std::mutex> lk(impl_->m);
    if (!impl_->hay_programa) return false;
    std::string destino = ruta.empty() ? impl_->estado.programa_ruta : ruta;
    if (destino.empty()) return false;
    if (!AhpFormat::Guardar(destino, impl_->programa_actual)) return false;
    impl_->estado.programa_ruta = destino;
    return true;
}

bool Engine::CerrarPrograma() {
    std::lock_guard<std::mutex> lk(impl_->m);
    impl_->programa_actual = Programa{};
    impl_->hay_programa = false;
    impl_->estado.programa_ruta.clear();
    impl_->estado.programa_titulo.clear();
    impl_->estado.escenario_id.clear();
    impl_->estado.elemento_id.clear();
    impl_->estado.linea_actual = 0;
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
namespace {

PosPrograma PosDesdeEstado(const EstadoMotor& e) {
    PosPrograma pos;
    pos.escenario_id     = e.escenario_id;
    pos.escenario_nombre = e.escenario_nombre;
    pos.elemento_id      = e.elemento_id;
    pos.elemento_titulo  = e.elemento_titulo;
    pos.linea            = e.linea_actual;
    pos.valida           = !e.escenario_id.empty();
    return pos;
}

void EstadoDesdePos(EstadoMotor* e, const PosPrograma& pos) {
    e->escenario_id     = pos.escenario_id;
    e->escenario_nombre = pos.escenario_nombre;
    e->elemento_id      = pos.elemento_id;
    e->elemento_titulo  = pos.elemento_titulo;
    e->linea_actual     = pos.linea;
}

} // namespace

bool Engine::IniciarProyeccion() {
    if (!impl_->ventana) return false;
    impl_->ventana->Mostrar();
    {
        std::lock_guard<std::mutex> lk(impl_->m);
        impl_->estado.salida_visible = true;
        impl_->Emitir(EventoMotor::SalidaCambiada);
    }
    return true;
}

bool Engine::DetenerProyeccion() {
    if (!impl_->ventana) return false;
    impl_->ventana->Ocultar();
    {
        std::lock_guard<std::mutex> lk(impl_->m);
        impl_->estado.salida_visible = false;
        impl_->Emitir(EventoMotor::SalidaCambiada);
    }
    return true;
}

bool Engine::IrEscenario(const std::string& escenario_id) {
    {
        std::lock_guard<std::mutex> lk(impl_->m);
        impl_->estado.escenario_id = escenario_id;
        impl_->estado.elemento_id.clear();
        impl_->estado.linea_actual = 0;
        impl_->Emitir(EventoMotor::EstadoCambiado);
    }
    return true;
}

bool Engine::IrElemento(const std::string& escenario_id,
                        const std::string& elemento_id) {
    {
        std::lock_guard<std::mutex> lk(impl_->m);
        impl_->estado.escenario_id = escenario_id;
        impl_->estado.elemento_id  = elemento_id;
        impl_->estado.linea_actual = 0;
        impl_->Emitir(EventoMotor::EstadoCambiado);
    }
    return true;
}

bool Engine::IrLinea(const std::string& escenario_id,
                      const std::string& elemento_id,
                      int linea) {
    {
        std::lock_guard<std::mutex> lk(impl_->m);
        impl_->estado.escenario_id = escenario_id;
        impl_->estado.elemento_id  = elemento_id;
        impl_->estado.linea_actual = linea;
        impl_->Emitir(EventoMotor::EstadoCambiado);
    }
    return true;
}

bool Engine::Siguiente() {
    bool se_movio = false;
    {
        std::lock_guard<std::mutex> lk(impl_->m);
        if (impl_->hay_programa) {
            PosPrograma pos = PosDesdeEstado(impl_->estado);
            se_movio = AvanzarPos(impl_->programa_actual, &pos);
            EstadoDesdePos(&impl_->estado, pos);
        }
    }
    impl_->Emitir(EventoMotor::EstadoCambiado);
    return se_movio;
}

bool Engine::Anterior() {
    bool se_movio = false;
    {
        std::lock_guard<std::mutex> lk(impl_->m);
        if (impl_->hay_programa) {
            PosPrograma pos = PosDesdeEstado(impl_->estado);
            se_movio = RetrocederPos(impl_->programa_actual, &pos);
            EstadoDesdePos(&impl_->estado, pos);
        }
    }
    impl_->Emitir(EventoMotor::EstadoCambiado);
    return se_movio;
}

bool Engine::SetNegro(bool activo) {
    {
        std::lock_guard<std::mutex> lk(impl_->m);
        impl_->estado.negro = activo;
        impl_->Emitir(EventoMotor::SalidaCambiada);
    }
    return true;
}

bool Engine::SetLogo(bool activo) {
    {
        std::lock_guard<std::mutex> lk(impl_->m);
        impl_->estado.logo = activo;
        impl_->Emitir(EventoMotor::SalidaCambiada);
    }
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
    {
        std::lock_guard<std::mutex> lk(impl_->m);
        impl_->estado.monitor_dispositivo = dispositivo;
        impl_->Emitir(EventoMotor::MonitorCambiado);
    }
    return true;
}

// --- Biblia ------------------------------------------------------------
std::vector<std::string> Engine::ListarBiblias() const {
    // Biblias realmente presentes: barrer *.fdb en la carpeta de biblias
    // de la raíz de datos (antes devolvía una lista inventada).
    std::vector<std::string> out;
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(
        (rutas::CarpetaBibliasW() + L"\\*.fdb").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return out;
    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        std::wstring nombre(fd.cFileName);
        size_t punto = nombre.rfind(L'.');
        if (punto != std::wstring::npos) nombre.resize(punto);
        if (!nombre.empty()) {
            std::string s(nombre.begin(), nombre.end());
            out.push_back(s);
        }
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    return out;
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

std::vector<Versiculo> Engine::BuscarEnBiblia(const std::string& texto,
                                              int limite) const {
    if (!impl_->biblia_activa) return {};
    return impl_->biblia_activa->Buscar(texto, limite);
}

std::vector<std::string> Engine::FavoritosBiblia() const {
    if (!impl_->biblia_activa) return {};
    return impl_->biblia_activa->Favoritos();
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

std::vector<Canto> Engine::BuscarCantos(const std::string& texto,
                                        int limite) const {
    if (!impl_->canciones) return {};
    return impl_->canciones->Buscar(texto, limite);
}

bool Engine::ObtenerCanto(std::int64_t id, CantoDetalle* out) const {
    if (!impl_->canciones) return false;
    return impl_->canciones->Obtener(id, out);
}

bool Engine::BibliaLista() const {
    return impl_->biblia_activa &&
           impl_->biblia_activa->TotalVersiculos() > 0;
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
    {
        std::lock_guard<std::mutex> lk(impl_->m);
        s.programa_ruta       = impl_->estado.programa_ruta;
        s.escenario_id        = impl_->estado.escenario_id;
        s.elemento_id         = impl_->estado.elemento_id;
        s.linea_actual        = impl_->estado.linea_actual;
        s.salida_visible      = impl_->estado.salida_visible;
        s.negro               = impl_->estado.negro;
        s.logo                 = impl_->estado.logo;
        s.monitor_dispositivo = impl_->estado.monitor_dispositivo;
    }
    return Sesion::Guardar(Sesion::RutaPorDefecto(), s);
}

} // namespace fusion
