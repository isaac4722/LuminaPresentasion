// src/core/src/core/IpcDespacho.cpp — Despachador ipc.v1 (portable)
//
// Implementa la tabla de comandos de docs/agent/format_ipc_v1.md:
// estado.*, programa.*, proyeccion.*, monitor.*, biblia.*, canto.*,
// diag.*. Errores explícitos con códigos del documento (nada silencioso).

#include "fusion/core/IpcDespacho.h"

#include "json.hpp"

#include <cstdio>
#include <algorithm>

namespace fusion {

// -------------------------------------------------------------------------
// Helpers de respuesta
// -------------------------------------------------------------------------
namespace {

using nlohmann::json;

RespuestaIpc RespuestaOk(const std::string& id, json result,
                         bool suscribir = false) {
    RespuestaIpc r;
    r.ok = true;
    r.suscribir = suscribir;
    json j;
    j["ipc"] = "fusion";
    j["version"] = 1;
    if (!id.empty()) j["id"] = id;
    j["ok"] = true;
    j["result"] = result.is_object() ? result : json::object();
    r.raw_json = j.dump();
    return r;
}

RespuestaIpc RespuestaError(const std::string& id, const char* code,
                            const std::string& mensaje) {
    RespuestaIpc r;
    r.ok = false;
    json j;
    j["ipc"] = "fusion";
    j["version"] = 1;
    if (!id.empty()) j["id"] = id;
    j["ok"] = false;
    j["error"] = { {"code", code}, {"message", mensaje} };
    r.raw_json = j.dump();
    return r;
}

bool Campo(const json& p, const char* nombre, std::string* out) {
    if (p.contains(nombre) && p[nombre].is_string() &&
        !p[nombre].get<std::string>().empty()) {
        *out = p[nombre].get<std::string>();
        return true;
    }
    return false;
}

json EstadoJson(const EstadoMotor& e) {
    json r;
    r["programa_ruta"]       = e.programa_ruta;
    r["programa_titulo"]     = e.programa_titulo;
    r["escenario_id"]        = e.escenario_id;
    r["escenario_nombre"]    = e.escenario_nombre;
    r["elemento_id"]         = e.elemento_id;
    r["elemento_titulo"]     = e.elemento_titulo;
    r["linea_actual"]        = e.linea_actual;
    r["salida_visible"]      = e.salida_visible;
    r["negro"]               = e.negro;
    r["logo"]                = e.logo;
    r["monitor_dispositivo"] = e.monitor_dispositivo;
    return r;
}

json CantoJson(const CantoIpc& c, bool con_secciones) {
    json r;
    r["id"] = c.id;
    r["titulo"] = c.titulo;
    r["autor"] = c.autor;
    r["tono_origen"] = c.tono_origen;
    r["bpm"] = c.bpm;
    if (con_secciones) {
        json ss = json::array();
        for (const auto& s : c.secciones) {
            ss.push_back({ {"orden", s.orden},
                           {"tipo", s.tipo},
                           {"etiqueta", s.etiqueta},
                           {"lineas", s.lineas} });
        }
        r["secciones"] = ss;
    }
    return r;
}

json VersiculosJson(const std::vector<VersiculoIpc>& vs) {
    json a = json::array();
    for (const auto& v : vs)
        a.push_back({ {"libro", v.libro},
                      {"capitulo", v.capitulo},
                      {"versiculo", v.versiculo},
                      {"texto", v.texto} });
    return a;
}

json MonitoresJson(const std::vector<MonitorIpc>& ms) {
    json a = json::array();
    for (const auto& m : ms)
        a.push_back({ {"dispositivo", m.dispositivo},
                      {"nombre", m.nombre},
                      {"x", m.x}, {"y", m.y},
                      {"w", m.w}, {"h", m.h},
                      {"primario", m.primario} });
    return a;
}

json DiagJson(const std::vector<PruebaDiag>& ps) {
    json a = json::array();
    for (const auto& p : ps)
        a.push_back({ {"nombre", p.nombre},
                      {"ok", p.ok},
                      {"detalle", p.detalle} });
    return a;
}

} // namespace

// -------------------------------------------------------------------------
// Despacho principal
// -------------------------------------------------------------------------
namespace {

RespuestaIpc Despachar(IServicioNucleo& s, const std::string& tipo,
                       const std::string& id, const json& p);

RespuestaIpc Despachar(IServicioNucleo& s, const std::string& tipo,
                       const std::string& id, const json& p) {
    // --- Estado y sesión -------------------------------------------------
    if (tipo == "estado.lector") {
        return RespuestaOk(id, EstadoJson(s.Estado()));
    }
    if (tipo == "estado.suscribir") {
        return RespuestaOk(id, json::object(), /*suscribir=*/true);
    }
    if (tipo == "session.guardar") {
        if (!s.GuardarPrograma("")) return RespuestaError(id, "E_IO",
                                    "no se pudo guardar la sesión");
        return RespuestaOk(id, json::object());
    }

    // --- Programa --------------------------------------------------------
    if (tipo == "programa.abrir") {
        std::string ruta;
        if (!Campo(p, "ruta", &ruta))
            return RespuestaError(id, "E_BAD_PAYLOAD", "falta 'ruta'");
        if (!s.AbrirPrograma(ruta))
            return RespuestaError(id, "E_IO",
                                  "no se pudo abrir el programa: " + ruta);
        return RespuestaOk(id, json{ {"ruta", ruta} });
    }
    if (tipo == "programa.nuevo") {
        std::string titulo = p.value("titulo", "");
        if (titulo.empty() && p.contains("meta") && p["meta"].is_object())
            titulo = p["meta"].value("titulo", "");
        if (titulo.empty())
            return RespuestaError(id, "E_BAD_PAYLOAD", "falta 'titulo'");
        s.NuevoPrograma(titulo);
        return RespuestaOk(id, json{ {"titulo", titulo} });
    }
    if (tipo == "programa.guardar") {
        std::string ruta = p.value("ruta", "");
        if (!s.GuardarPrograma(ruta))
            return RespuestaError(id, "E_IO",
                                  "no se pudo guardar el programa");
        return RespuestaOk(id, json{ {"ruta", ruta} });
    }
    if (tipo == "programa.cerrar") {
        s.CerrarPrograma();
        return RespuestaOk(id, json::object());
    }
    if (tipo == "programa.recientes") {
        json res;
        res["recientes"] = s.Recientes();
        return RespuestaOk(id, res);
    }

    // --- Proyección --------------------------------------------------------
    if (tipo == "proyeccion.iniciar") {
        if (!s.IniciarProyeccion())
            return RespuestaError(id, "E_INTERNAL",
                                  "no hay ventana de proyección disponible");
        EstadoMotor e = s.Estado();
        json res;
        res["monitor"] = e.monitor_dispositivo;
        return RespuestaOk(id, res);
    }
    if (tipo == "proyeccion.detener" || tipo == "proyeccion.ocultar") {
        if (!s.DetenerProyeccion())
            return RespuestaError(id, "E_INTERNAL",
                                  "la proyección no estaba disponible");
        return RespuestaOk(id, json::object());
    }
    if (tipo == "proyeccion.escenario") {
        std::string esc;
        if (!Campo(p, "escenario_id", &esc))
            return RespuestaError(id, "E_BAD_PAYLOAD", "falta 'escenario_id'");
        if (!s.IrEscenario(esc))
            return RespuestaError(id, "E_NOT_FOUND", "escenario: " + esc);
        return RespuestaOk(id, json::object());
    }
    if (tipo == "proyeccion.elemento") {
        std::string esc, el;
        if (!Campo(p, "escenario_id", &esc) || !Campo(p, "elemento_id", &el))
            return RespuestaError(id, "E_BAD_PAYLOAD",
                                  "faltan 'escenario_id' o 'elemento_id'");
        if (!s.IrElemento(esc, el))
            return RespuestaError(id, "E_NOT_FOUND",
                                  "elemento: " + el);
        return RespuestaOk(id, json::object());
    }
    if (tipo == "proyeccion.linea") {
        std::string esc, el;
        if (!Campo(p, "escenario_id", &esc) || !Campo(p, "elemento_id", &el) ||
            !p.contains("linea") || !p["linea"].is_number_integer() ||
            p["linea"].get<int>() < 0)
            return RespuestaError(id, "E_BAD_PAYLOAD",
                                  "faltan 'escenario_id', 'elemento_id' o 'linea' (entero ≥ 0)");
        if (!s.IrLinea(esc, el, p["linea"].get<int>()))
            return RespuestaError(id, "E_NOT_FOUND", "línea fuera de rango");
        return RespuestaOk(id, json::object());
    }
    if (tipo == "proyeccion.siguiente" || tipo == "proyeccion.anterior") {
        bool movio = (tipo == "proyeccion.siguiente") ? s.Siguiente()
                                                      : s.Anterior();
        if (!movio && s.Estado().programa_ruta.empty())
            return RespuestaError(id, "E_NO_PROGRAM", "no hay programa cargado");
        EstadoMotor e = s.Estado();
        json res;
        res["elemento_id"] = e.elemento_id;
        res["linea"] = e.linea_actual;
        return RespuestaOk(id, res);
    }
    if (tipo == "proyeccion.negro" || tipo == "proyeccion.logo") {
        if (!p.contains("activar") || !p["activar"].is_boolean())
            return RespuestaError(id, "E_BAD_PAYLOAD", "falta 'activar' (bool)");
        bool activar = p["activar"].get<bool>();
        bool ok = (tipo == "proyeccion.negro") ? s.SetNegro(activar)
                                               : s.SetLogo(activar);
        if (!ok) return RespuestaError(id, "E_INTERNAL", "no se pudo aplicar");
        return RespuestaOk(id, json{ {"activar", activar} });
    }

    // --- Monitor -----------------------------------------------------------
    if (tipo == "monitor.listar") {
        json res;
        res["monitores"] = MonitoresJson(s.ListarMonitores());
        return RespuestaOk(id, res);
    }
    if (tipo == "monitor.seleccionar") {
        std::string disp;
        if (!Campo(p, "dispositivo", &disp))
            return RespuestaError(id, "E_BAD_PAYLOAD", "falta 'dispositivo'");
        if (!s.SeleccionarMonitor(disp))
            return RespuestaError(id, "E_NOT_FOUND", "monitor: " + disp);
        return RespuestaOk(id, json::object());
    }

    // --- Biblia ------------------------------------------------------------
    if (tipo == "biblia.listar") {
        json res;
        res["biblias"] = s.ListarBiblias();
        return RespuestaOk(id, res);
    }
    if (tipo == "biblia.obtener") {
        std::string biblia = p.value("biblia", "");
        std::string cita;
        if (!Campo(p, "cita", &cita))
            return RespuestaError(id, "E_BAD_PAYLOAD", "falta 'cita'");
        std::string texto;
        if (!s.ObtenerVersiculo(biblia, cita, &texto))
            return RespuestaError(id, "E_NOT_FOUND",
                                  "cita no encontrada: " + cita);
        return RespuestaOk(id, json{ {"cita", cita}, {"texto", texto} });
    }
    if (tipo == "biblia.buscar") {
        std::string texto;
        if (!Campo(p, "texto", &texto))
            return RespuestaError(id, "E_BAD_PAYLOAD", "falta 'texto'");
        int limite = p.value("limite", 50);
        if (limite <= 0 || limite > 500) limite = 50;
        json res;
        res["resultados"] = VersiculosJson(s.BuscarBiblia(texto, limite));
        return RespuestaOk(id, res);
    }
    if (tipo == "biblia.favoritos") {
        json res;
        res["favoritos"] = s.FavoritosBiblia();
        return RespuestaOk(id, res);
    }

    // --- Cantos ------------------------------------------------------------
    if (tipo == "canto.listar") {
        json res;
        res["cantos"] = s.ListarCantos();
        return RespuestaOk(id, res);
    }
    if (tipo == "canto.obtener") {
        if (!p.contains("id") || !(p["id"].is_number_integer() ||
                                   p["id"].is_string()))
            return RespuestaError(id, "E_BAD_PAYLOAD", "falta 'id'");
        std::int64_t cid = 0;
        if (p["id"].is_string())
            cid = std::atoll(p["id"].get<std::string>().c_str());
        else
            cid = p["id"].get<std::int64_t>();
        bool ok = false;
        CantoIpc c = s.ObtenerCanto(cid, &ok);
        if (!ok)
            return RespuestaError(id, "E_NOT_FOUND", "canto no encontrado");
        json res;
        res["canto"] = CantoJson(c, true);
        return RespuestaOk(id, res);
    }
    if (tipo == "canto.buscar") {
        std::string texto;
        if (!Campo(p, "texto", &texto))
            return RespuestaError(id, "E_BAD_PAYLOAD", "falta 'texto'");
        int limite = p.value("limite", 50);
        if (limite <= 0 || limite > 500) limite = 50;
        json a = json::array();
        for (const auto& c : s.BuscarCantos(texto, limite))
            a.push_back(CantoJson(c, false));
        return RespuestaOk(id, json{ {"resultados", a} });
    }

    // --- Temas -------------------------------------------------------------
    // La gestión de temas por IPC (tema.listar/aplicar_*) requiere exponer
    // la biblioteca de temas en el motor: pieza separada, error explícito
    // (nunca silencio).
    if (tipo.rfind("tema.", 0) == 0) {
        return RespuestaError(id, "E_UNSUPPORTED",
                              "gestión de temas por IPC pendiente (" + tipo + ")");
    }

    // --- Diagnóstico ---------------------------------------------------------
    if (tipo == "diag.autotest") {
        auto ps = s.Autotest();
        bool todo_ok = true;
        for (const auto& t : ps) if (!t.ok) todo_ok = false;
        json res;
        res["ok"] = todo_ok;
        res["resultados"] = DiagJson(ps);
        return RespuestaOk(id, res);
    }
    if (tipo == "diag.log_tail") {
        int n = p.value("n", 50);
        if (n <= 0 || n > 1000) n = 50;
        json res;
        res["lineas"] = s.ColaLog(n);
        return RespuestaOk(id, res);
    }

    return RespuestaError(id, "E_UNSUPPORTED", "comando desconocido: " + tipo);
}

} // namespace

RespuestaIpc ProcesarIpc(IServicioNucleo& servicio, const MensajeIpc& entrada) {
    json j;
    try {
        j = json::parse(entrada.raw_json);
    } catch (...) {
        return RespuestaError("", "E_BAD_PAYLOAD",
                              "el mensaje no es JSON válido");
    }
    if (!j.is_object())
        return RespuestaError("", "E_BAD_PAYLOAD",
                              "el mensaje no es un objeto JSON");

    std::string id = j.value("id", "");
    if (j.value("ipc", "") != "fusion")
        return RespuestaError(id, "E_BAD_PAYLOAD",
                              "mensaje ajeno a FUSION (campo 'ipc' != 'fusion')");
    if (j.value("version", 0) != 1)
        return RespuestaError(id, "E_UNSUPPORTED",
                              "versión de protocolo no soportada (solo ipc.v1)");
    std::string tipo = j.value("type", "");
    if (tipo.empty())
        return RespuestaError(id, "E_BAD_PAYLOAD", "falta 'type'");

    const json payload = (j.contains("payload") && j["payload"].is_object())
                             ? j["payload"] : json::object();

    try {
        return Despachar(servicio, tipo, id, payload);
    } catch (const std::exception& ex) {
        return RespuestaError(id, "E_INTERNAL",
                              std::string("excepción del núcleo: ") + ex.what());
    } catch (...) {
        return RespuestaError(id, "E_INTERNAL", "excepción desconocida del núcleo");
    }
}

} // namespace fusion
