// tests/native/test_ipc_despacho.cpp — Tests del despachador ipc.v1
// (IpcDespacho con IServicioNucleo falso). La mitad que faltaba del
// protocolo: el núcleo nunca respondía a ningún comando.

#include "doctest.h"
#include "fusion/core/IpcDespacho.h"

#include <string>
#include <vector>

using namespace fusion;

namespace {

// Servicio falso que graba las llamadas y devuelve datos conocidos.
struct ServicioFalso : IServicioNucleo {
    // grabaciones
    std::string ultima_ruta_abrir;
    std::string ultimo_titulo_nuevo;
    std::string ultima_ruta_guardar;
    bool ultimo_negro = false;
    bool ultimo_logo  = false;
    std::string ultimo_escenario, ultimo_elemento, ultimo_monitor;
    int ultima_linea = -1;
    std::int64_t ultimo_canto_id = -1;
    int abrir_resultado = 1;      // configurable
    int guardar_resultado = 1;

    EstadoMotor estado;
    bool proyeccion_disponible = true;

    // IServicioNucleo
    EstadoMotor Estado() override { return estado; }
    bool AbrirPrograma(const std::string& ruta) override {
        ultima_ruta_abrir = ruta;
        return abrir_resultado == 1;
    }
    bool NuevoPrograma(const std::string& titulo) override {
        ultimo_titulo_nuevo = titulo;
        return true;
    }
    bool GuardarPrograma(const std::string& ruta) override {
        ultima_ruta_guardar = ruta;
        return guardar_resultado == 1;
    }
    bool CerrarPrograma() override { return true; }
    std::vector<std::string> Recientes() override {
        return {"culto1.ahp", "culto2.ahp"};
    }
    bool IniciarProyeccion() override { return proyeccion_disponible; }
    bool DetenerProyeccion() override { return proyeccion_disponible; }
    bool IrEscenario(const std::string& e) override {
        ultimo_escenario = e; return true;
    }
    bool IrElemento(const std::string& e, const std::string& el) override {
        ultimo_escenario = e; ultimo_elemento = el; return true;
    }
    bool IrLinea(const std::string& e, const std::string& el, int l) override {
        ultimo_escenario = e; ultimo_elemento = el; ultima_linea = l;
        return true;
    }
    bool Siguiente() override { return false; }
    bool Anterior()  override { return false; }
    bool SetNegro(bool a) override { ultimo_negro = a; return true; }
    bool SetLogo(bool a)  override { ultimo_logo = a;  return true; }
    bool OcultarSalida()  override { return true; }
    std::vector<MonitorIpc> ListarMonitores() override {
        return {MonitorIpc{"\\\\.\\DISPLAY1", "Integrada", 0, 0, 1920, 1080, true}};
    }
    bool SeleccionarMonitor(const std::string& d) override {
        ultimo_monitor = d; return true;
    }
    std::vector<std::string> ListarBiblias() override {
        return {"RVR1909"};
    }
    bool ObtenerVersiculo(const std::string&, const std::string& cita,
                          std::string* texto_out) override {
        if (cita == "Juan 3:16") {
            *texto_out = "Porque de tal manera amó Dios...";
            return true;
        }
        return false;
    }
    std::vector<VersiculoIpc> BuscarBiblia(const std::string& texto,
                                           int limite) override {
        (void)limite;
        if (texto == "puertas")
            return {VersiculoIpc{"Salmos", 100, 4,
                                 "Entrad por sus puertas con acción de gracias"}};
        return {};
    }
    std::vector<std::string> FavoritosBiblia() override {
        return {"Salmo 100:4"};
    }
    std::vector<std::string> ListarCantos() override {
        return {"Cuán grande es Él"};
    }
    CantoIpc ObtenerCanto(std::int64_t id, bool* ok) override {
        ultimo_canto_id = id;
        CantoIpc c;
        if (id == 5) {
            *ok = true;
            c.id = 5; c.titulo = "Cuán grande es Él"; c.autor = "Anónimo";
            c.secciones.push_back(SeccionCantoIpc{1, "verso", "Verso 1",
                                                  {"El Señor mi pastor es"}});
        } else {
            *ok = false;
        }
        return c;
    }
    std::vector<CantoIpc> BuscarCantos(const std::string& texto,
                                       int limite) override {
        (void)limite;
        CantoIpc c;
        c.id = 5; c.titulo = texto;
        return {c};
    }
    std::vector<PruebaDiag> Autotest() override {
        return {PruebaDiag{"biblia", true, "ok"},
                PruebaDiag{"cancionero", false, "vacío"}};
    }
    std::vector<std::string> ColaLog(int n) override {
        (void)n;
        return {"linea-a", "linea-b"};
    }
};

RespuestaIpc Enviar(ServicioFalso& s, const std::string& raw) {
    MensajeIpc m;
    m.raw_json = raw;
    return ProcesarIpc(s, m);
}

bool Tiene(const std::string& haystack, const std::string& aguja) {
    return haystack.find(aguja) != std::string::npos;
}

} // namespace

// ---------------------------------------------------------------------------
// Validación del sobre (ipc / version / type / id)
// ---------------------------------------------------------------------------

TEST_CASE("ProcesarIpc: JSON roto responde E_BAD_PAYLOAD sin id") {
    ServicioFalso s;
    auto r = Enviar(s, "{no es json");
    CHECK_FALSE(r.ok);
    CHECK(Tiene(r.raw_json, "E_BAD_PAYLOAD"));
    CHECK_FALSE(Tiene(r.raw_json, "\"id\":"));
}

TEST_CASE("ProcesarIpc: mensaje no objeto responde E_BAD_PAYLOAD") {
    ServicioFalso s;
    auto r = Enviar(s, "[1,2,3]");
    CHECK_FALSE(r.ok);
    CHECK(Tiene(r.raw_json, "E_BAD_PAYLOAD"));
}

TEST_CASE("ProcesarIpc: ipc distinto de fusion responde con id eco") {
    ServicioFalso s;
    auto r = Enviar(s, R"({"ipc":"otro","version":1,"type":"estado.lector","id":"m-1","payload":{}})");
    CHECK_FALSE(r.ok);
    CHECK(Tiene(r.raw_json, "E_BAD_PAYLOAD"));
    CHECK(Tiene(r.raw_json, "\"id\":\"m-1\""));
}

TEST_CASE("ProcesarIpc: version futura responde E_UNSUPPORTED") {
    ServicioFalso s;
    auto r = Enviar(s, R"({"ipc":"fusion","version":2,"type":"estado.lector","id":"m-2","payload":{}})");
    CHECK_FALSE(r.ok);
    CHECK(Tiene(r.raw_json, "E_UNSUPPORTED"));
}

TEST_CASE("ProcesarIpc: comando desconocido responde E_UNSUPPORTED") {
    ServicioFalso s;
    auto r = Enviar(s, R"({"ipc":"fusion","version":1,"type":"lo.que.sea","id":"m-3","payload":{}})");
    CHECK_FALSE(r.ok);
    CHECK(Tiene(r.raw_json, "E_UNSUPPORTED"));
    CHECK(Tiene(r.raw_json, "lo.que.sea"));
}

// ---------------------------------------------------------------------------
// Estado y suscripción
// ---------------------------------------------------------------------------

TEST_CASE("ProcesarIpc: estado.lector devuelve el snapshot con id eco") {
    ServicioFalso s;
    s.estado.programa_ruta = "culto.ahp";
    s.estado.linea_actual = 3;
    s.estado.negro = true;
    auto r = Enviar(s, R"({"ipc":"fusion","version":1,"type":"estado.lector","id":"m-4","payload":{}})");
    REQUIRE(r.ok);
    CHECK(Tiene(r.raw_json, "\"id\":\"m-4\""));
    CHECK(Tiene(r.raw_json, "culto.ahp"));
    CHECK(Tiene(r.raw_json, "\"linea_actual\":3"));
    CHECK(Tiene(r.raw_json, "\"negro\":true"));
}

TEST_CASE("ProcesarIpc: estado.suscribir marca la respuesta para el transporte") {
    ServicioFalso s;
    auto r = Enviar(s, R"({"ipc":"fusion","version":1,"type":"estado.suscribir","id":"m-5","payload":{}})");
    CHECK(r.ok);
    CHECK(r.suscribir);   // IpcServer usa esta señal para registrar eventos
}

// ---------------------------------------------------------------------------
// Programa y proyección
// ---------------------------------------------------------------------------

TEST_CASE("ProcesarIpc: programa.abrir pasa la ruta al servicio") {
    ServicioFalso s;
    auto r = Enviar(s, R"({"ipc":"fusion","version":1,"type":"programa.abrir","id":"m-6","payload":{"ruta":"data/culto.ahp"}})");
    CHECK(s.ultima_ruta_abrir == "data/culto.ahp");
    CHECK(r.ok);
}

TEST_CASE("ProcesarIpc: programa.abrir falla con E_IO honesto") {
    ServicioFalso s;
    s.abrir_resultado = 0;
    auto r = Enviar(s, R"({"ipc":"fusion","version":1,"type":"programa.abrir","id":"m-7","payload":{"ruta":"no existe.ahp"}})");
    CHECK_FALSE(r.ok);
    CHECK(Tiene(r.raw_json, "E_IO"));
}

TEST_CASE("ProcesarIpc: programa.abrir sin ruta responde E_BAD_PAYLOAD") {
    ServicioFalso s;
    auto r = Enviar(s, R"({"ipc":"fusion","version":1,"type":"programa.abrir","id":"m-8","payload":{}})");
    CHECK_FALSE(r.ok);
    CHECK(Tiene(r.raw_json, "E_BAD_PAYLOAD"));
    CHECK(Tiene(r.raw_json, "ruta"));
}

TEST_CASE("ProcesarIpc: proyeccion.negro pasa el valor exacto") {
    ServicioFalso s;
    auto r = Enviar(s, R"({"ipc":"fusion","version":1,"type":"proyeccion.negro","id":"m-9","payload":{"activar":true}})");
    CHECK(r.ok);
    CHECK(s.ultimo_negro == true);

    auto r2 = Enviar(s, R"({"ipc":"fusion","version":1,"type":"proyeccion.logo","id":"m-10","payload":{"activar":true}})");
    CHECK(r2.ok);
    CHECK(s.ultimo_logo == true);
}

TEST_CASE("ProcesarIpc: proyeccion.negro sin 'activar' responde E_BAD_PAYLOAD") {
    ServicioFalso s;
    auto r = Enviar(s, R"({"ipc":"fusion","version":1,"type":"proyeccion.negro","id":"m-11","payload":{}})");
    CHECK_FALSE(r.ok);
    CHECK(Tiene(r.raw_json, "E_BAD_PAYLOAD"));
}

TEST_CASE("ProcesarIpc: proyeccion.iniciar sin ventana responde error explícito") {
    ServicioFalso s;
    s.proyeccion_disponible = false;
    auto r = Enviar(s, R"({"ipc":"fusion","version":1,"type":"proyeccion.iniciar","id":"m-12","payload":{}})");
    CHECK_FALSE(r.ok);
    CHECK(Tiene(r.raw_json, "E_INTERNAL"));
}

TEST_CASE("ProcesarIpc: proyeccion.linea valida tipos y pasa el entero") {
    ServicioFalso s;
    auto r = Enviar(s, R"({"ipc":"fusion","version":1,"type":"proyeccion.linea","id":"m-13","payload":{"escenario_id":"esc-1","elemento_id":"el-2","linea":4}})");
    CHECK(r.ok);
    CHECK(s.ultima_linea == 4);

    auto r2 = Enviar(s, R"({"ipc":"fusion","version":1,"type":"proyeccion.linea","id":"m-14","payload":{"escenario_id":"esc-1","elemento_id":"el-2","linea":-1}})");
    CHECK_FALSE(r2.ok);
    CHECK(Tiene(r2.raw_json, "E_BAD_PAYLOAD"));
}

// ---------------------------------------------------------------------------
// Biblia, cantos, monitores y diagnóstico
// ---------------------------------------------------------------------------

TEST_CASE("ProcesarIpc: biblia.obtener devuelve el texto y E_NOT_FOUND si falta") {
    ServicioFalso s;
    auto r = Enviar(s, R"({"ipc":"fusion","version":1,"type":"biblia.obtener","id":"m-15","payload":{"cita":"Juan 3:16"}})");
    CHECK(r.ok);
    CHECK(Tiene(r.raw_json, "amó Dios"));

    auto r2 = Enviar(s, R"({"ipc":"fusion","version":1,"type":"biblia.obtener","id":"m-16","payload":{"cita":"Hechos 99:99"}})");
    CHECK_FALSE(r2.ok);
    CHECK(Tiene(r2.raw_json, "E_NOT_FOUND"));
}

TEST_CASE("ProcesarIpc: biblia.buscar da forma de resultados") {
    ServicioFalso s;
    auto r = Enviar(s, R"({"ipc":"fusion","version":1,"type":"biblia.buscar","id":"m-17","payload":{"texto":"puertas"}})");
    CHECK(r.ok);
    CHECK(Tiene(r.raw_json, "Salmos"));
    CHECK(Tiene(r.raw_json, "\"capitulo\":100"));
}

TEST_CASE("ProcesarIpc: canto.obtener serializa secciones y falla honesto") {
    ServicioFalso s;
    auto r = Enviar(s, R"({"ipc":"fusion","version":1,"type":"canto.obtener","id":"m-18","payload":{"id":5}})");
    CHECK(r.ok);
    CHECK(s.ultimo_canto_id == 5);
    CHECK(Tiene(r.raw_json, "Cuán grande es Él"));
    CHECK(Tiene(r.raw_json, "Verso 1"));

    auto r2 = Enviar(s, R"({"ipc":"fusion","version":1,"type":"canto.obtener","id":"m-19","payload":{"id":777}})");
    CHECK_FALSE(r2.ok);
    CHECK(Tiene(r2.raw_json, "E_NOT_FOUND"));
}

TEST_CASE("ProcesarIpc: monitor.listar y monitor.seleccionar") {
    ServicioFalso s;
    auto r = Enviar(s, R"({"ipc":"fusion","version":1,"type":"monitor.listar","id":"m-20","payload":{}})");
    CHECK(r.ok);
    CHECK(Tiene(r.raw_json, "DISPLAY1"));
    CHECK(Tiene(r.raw_json, "\"primario\":true"));

    auto r2 = Enviar(s, R"({"ipc":"fusion","version":1,"type":"monitor.seleccionar","id":"m-21","payload":{"dispositivo":"\\\\.\\DISPLAY1"}})");
    CHECK(r2.ok);
    CHECK(s.ultimo_monitor == "\\\\.\\DISPLAY1");
}

TEST_CASE("ProcesarIpc: diag.autotest reporta fallos y diag.log_tail da líneas") {
    ServicioFalso s;
    auto r = Enviar(s, R"({"ipc":"fusion","version":1,"type":"diag.autotest","id":"m-22","payload":{}})");
    CHECK(r.ok);
    CHECK(Tiene(r.raw_json, "\"ok\":false"));   // cancionero=false en el falso
    CHECK(Tiene(r.raw_json, "biblia"));

    auto r2 = Enviar(s, R"({"ipc":"fusion","version":1,"type":"diag.log_tail","id":"m-23","payload":{"n":10}})");
    CHECK(r2.ok);
    CHECK(Tiene(r2.raw_json, "linea-a"));
    CHECK(Tiene(r2.raw_json, "linea-b"));
}

TEST_CASE("ProcesarIpc: tema.* responde E_UNSUPPORTED explícito (sin silencio)") {
    ServicioFalso s;
    auto r = Enviar(s, R"({"ipc":"fusion","version":1,"type":"tema.listar","id":"m-24","payload":{}})");
    CHECK_FALSE(r.ok);
    CHECK(Tiene(r.raw_json, "E_UNSUPPORTED"));
}
