// src/core/src/core/AdaptadorEngine.cpp — IServicioNucleo sobre el motor
//
// Solo compila para el FusionCore.exe real (depende de Engine, Windows):
// el despachador (IpcDespacho.cpp) es portable y se prueba con un falso
// IServicioNucleo en el arnés local.

#include "fusion/core/IpcDespacho.h"
#include "fusion/core/Engine.h"
#include "fusion/core/Rutas.h"

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

std::vector<LibroBiblia> AdaptadorEngine::ListarLibros() {
    return motor_.ListarLibros();
}
std::vector<int> AdaptadorEngine::ListarCapitulos(int libro_id) {
    return motor_.ListarCapitulos(libro_id);
}
bool AdaptadorEngine::SeleccionarBiblia(const std::string& nombre) {
    return motor_.SeleccionarBiblia(nombre);
}
std::string AdaptadorEngine::BibliaActiva() {
    return motor_.BibliaActiva();
}

bool AdaptadorEngine::AgregarTexto(const std::string& titulo,
                                   const std::vector<std::string>& lineas,
                                   std::string* escenario_out,
                                   std::string* elemento_out) {
    return motor_.AgregarTexto(titulo, lineas, escenario_out, elemento_out);
}
bool AdaptadorEngine::AgregarCanto(std::int64_t canto_id,
                                   std::string* escenario_out,
                                   std::string* primer_elemento_out) {
    return motor_.AgregarCanto(canto_id, escenario_out, primer_elemento_out);
}
bool AdaptadorEngine::AgregarVersiculo(const std::string& cita,
                                       const std::string& biblia,
                                       bool tercio,
                                       std::string* escenario_out,
                                       std::string* elemento_out) {
    return motor_.AgregarVersiculo(cita, biblia, tercio, escenario_out,
                                   elemento_out);
}
bool AdaptadorEngine::AgregarPptx(const std::string& ruta,
                                  std::string* escenario_out,
                                  int* diapositivas_out) {
    return motor_.AgregarPptx(ruta, escenario_out, diapositivas_out);
}
bool AdaptadorEngine::QuitarElemento(const std::string& escenario_id,
                                     const std::string& elemento_id) {
    return motor_.QuitarElemento(escenario_id, elemento_id);
}
std::string AdaptadorEngine::ProgramaEstado() {
    return motor_.ProgramaEstado();
}

std::vector<CantoIpc> AdaptadorEngine::ListarCantos() {
    std::vector<CantoIpc> out;
    for (const auto& c : motor_.ListarCantos()) {
        CantoIpc ci;
        ci.id = c.id; ci.titulo = c.titulo; ci.autor = c.autor;
        ci.tono_origen = c.tono_origen; ci.bpm = c.bpm;
        out.push_back(ci);
    }
    return out;
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
    // La bitácora del núcleo vive en la raíz de datos (runtime/nucleo.log).
    const std::string ruta = rutas::BitacoraNucleo();

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
