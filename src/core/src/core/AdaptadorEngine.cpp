// src/core/src/core/AdaptadorEngine.cpp — IServicioNucleo sobre el motor
//
// Solo compila para el FusionCore.exe real (depende de Engine, Windows):
// el despachador (IpcDespacho.cpp) es portable y se prueba con un falso
// IServicioNucleo en el arnés local.

#include "fusion/core/IpcDespacho.h"
#include "fusion/core/Engine.h"
#include "fusion/core/Session.h"

#include <fstream>
#include <string>
#include <vector>

namespace fusion {

EstadoMotor AdaptadorEngine::Estado() { return motor_.Estado(); }

bool AdaptadorEngine::AbrirPrograma(const std::string& ruta) {
    return motor_.AbrirPrograma(ruta);
}
bool AdaptadorEngine::NuevoPrograma(const std::string& titulo) {
    return motor_.NuevoPrograma(titulo);
}
bool AdaptadorEngine::GuardarPrograma(const std::string& ruta) {
    return motor_.GuardarPrograma(ruta);
}
bool AdaptadorEngine::CerrarPrograma() { return motor_.CerrarPrograma(); }

std::vector<std::string> AdaptadorEngine::Recientes() {
    return motor_.Recientes();
}

bool AdaptadorEngine::IniciarProyeccion() { return motor_.IniciarProyeccion(); }
bool AdaptadorEngine::DetenerProyeccion() { return motor_.DetenerProyeccion(); }
bool AdaptadorEngine::IrEscenario(const std::string& e) {
    return motor_.IrEscenario(e);
}
bool AdaptadorEngine::IrElemento(const std::string& e, const std::string& el) {
    return motor_.IrElemento(e, el);
}
bool AdaptadorEngine::IrLinea(const std::string& e, const std::string& el,
                              int linea) {
    return motor_.IrLinea(e, el, linea);
}
bool AdaptadorEngine::Siguiente() { return motor_.Siguiente(); }
bool AdaptadorEngine::Anterior()  { return motor_.Anterior(); }
bool AdaptadorEngine::SetNegro(bool a) { return motor_.SetNegro(a); }
bool AdaptadorEngine::SetLogo(bool a)  { return motor_.SetLogo(a); }
bool AdaptadorEngine::OcultarSalida()  { return motor_.OcultarSalida(); }

std::vector<MonitorIpc> AdaptadorEngine::ListarMonitores() {
    std::vector<MonitorIpc> out;
    for (const auto& m : motor_.ListarMonitores())
        out.push_back(MonitorIpc{m.dispositivo, m.nombre,
                                 m.x, m.y, m.w, m.h, m.primario});
    return out;
}
bool AdaptadorEngine::SeleccionarMonitor(const std::string& d) {
    return motor_.SeleccionarMonitor(d);
}

std::vector<std::string> AdaptadorEngine::ListarBiblias() {
    return motor_.ListarBiblias();
}
bool AdaptadorEngine::ObtenerVersiculo(const std::string& biblia,
                                       const std::string& cita,
                                       std::string* texto_out) {
    return motor_.ObtenerVersiculo(biblia, cita, texto_out);
}
std::vector<VersiculoIpc> AdaptadorEngine::BuscarBiblia(const std::string& texto,
                                                        int limite) {
    std::vector<VersiculoIpc> out;
    for (const auto& v : motor_.BuscarEnBiblia(texto, limite))
        out.push_back(VersiculoIpc{v.libro, v.capitulo, v.versiculo, v.texto});
    return out;
}
std::vector<std::string> AdaptadorEngine::FavoritosBiblia() {
    return motor_.FavoritosBiblia();
}

std::vector<std::string> AdaptadorEngine::ListarCantos() {
    return motor_.ListarCantos();
}
CantoIpc AdaptadorEngine::ObtenerCanto(std::int64_t id, bool* ok) {
    CantoDetalle d;
    *ok = motor_.ObtenerCanto(id, &d);
    CantoIpc c;
    if (*ok) {
        c.id = d.id; c.titulo = d.titulo; c.autor = d.autor;
        c.tono_origen = d.tono_origen; c.bpm = d.bpm;
        for (const auto& s : d.secciones)
            c.secciones.push_back(SeccionCantoIpc{s.orden, s.tipo,
                                                  s.etiqueta, s.lineas});
    }
    return c;
}
std::vector<CantoIpc> AdaptadorEngine::BuscarCantos(const std::string& texto,
                                                    int limite) {
    std::vector<CantoIpc> out;
    for (const auto& c : motor_.BuscarCantos(texto, limite)) {
        CantoIpc ci;
        ci.id = c.id; ci.titulo = c.titulo; ci.autor = c.autor;
        ci.tono_origen = c.tono_origen; ci.bpm = c.bpm;
        out.push_back(ci);
    }
    return out;
}

std::vector<PruebaDiag> AdaptadorEngine::Autotest() {
    std::vector<PruebaDiag> ps;

    // Biblia: cita conocida sobre la biblia activa (sembrada al arrancar).
    {
        std::string texto;
        bool ok = motor_.ObtenerVersiculo("", "Juan 3:16", &texto);
        ps.push_back(PruebaDiag{"biblia", ok,
                                ok ? "Juan 3:16 respondió (" +
                                     std::to_string(texto.size()) + " caracteres)"
                                   : "la biblia activa no respondió a Juan 3:16"});
    }
    // Cancionero: la BD abre y responde (0 cantos también es válido).
    {
        auto cantos = motor_.ListarCantos();
        ps.push_back(PruebaDiag{"cancionero", true,
                                std::to_string(cantos.size()) + " cantos"});
    }
    // Monitores: al menos uno para proyectar.
    {
        auto ms = ListarMonitores();
        ps.push_back(PruebaDiag{"monitores", !ms.empty(),
                                std::to_string(ms.size()) + " monitores"});
    }
    return ps;
}

std::vector<std::string> AdaptadorEngine::ColaLog(int n) {
    // La bitácora del núcleo vive junto a la sesión (runtime/nucleo.log).
    std::string ruta = Sesion::RutaPorDefecto();
    size_t pos = ruta.find_last_of("\\/");
    ruta = (pos != std::string::npos) ? ruta.substr(0, pos) : std::string(".");
    ruta += "\\nucleo.log";

    std::vector<std::string> todas;
    std::ifstream f(ruta, std::ios::binary);
    if (!f) return todas;
    std::string linea;
    while (std::getline(f, linea)) {
        while (!linea.empty() && (linea.back() == '\r' || linea.back() == ' '))
            linea.pop_back();
        todas.push_back(linea);
    }
    if (n <= 0 || static_cast<size_t>(n) >= todas.size()) return todas;
    return std::vector<std::string>(todas.end() - n, todas.end());
}

} // namespace fusion
