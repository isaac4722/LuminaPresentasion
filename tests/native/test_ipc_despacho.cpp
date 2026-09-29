// tests/native/test_ipc_despacho.cpp — Tests del despachador ipc.v1
// (IpcDespacho con IServicioNucleo falso). La mitad que faltaba del
// protocolo: el núcleo nunca respondía a ningún comando.

#include "doctest.h"
#include "fusion/core/IpcDespacho.h"

#include "json.hpp"

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
    std::vector<LibroBiblia> ListarLibros() override {
        return {LibroBiblia{1, "Génesis", "Gen", "Ge", "AT"},
                LibroBiblia{66, "Apocalipsis", "Apo", "Ap", "NT"}};
    }
    std::vector<int> ListarCapitulos(int libro_id) override {
        return libro_id == 1 ? std::vector<int>{1, 2, 3}
                             : std::vector<int>{1};
    }
    std::vector<VersiculoIpc> ObtenerCapitulo(int libro_id,
                                              int capitulo) override {
        (void)capitulo;
        if (libro_id == 1)
            return {VersiculoIpc{"Génesis", 1, 1,
                                "En el principio creó Dios los cielos"}};
        return {};
    }
    bool AgregarFavorito(const std::string& cita) override {
        ultima_favorito = cita;
        return !cita.empty();
    }
    std::string ultima_favorito;
    bool SeleccionarBiblia(const std::string& nombre) override {
        ultima_biblia = nombre;
        return nombre == "RVR1909";
    }
    std::string BibliaActiva() override { return "RVR1909"; }

    // grabaciones del programa construible
    std::string ultima_biblia;
    std::string ultimo_titulo_texto;
    std::vector<std::string> ultimas_lineas;
    std::int64_t ultimo_canto_agregado = -1;
    std::string ultima_cita_agregada;
    bool ultimo_tercio = false;
    std::string ultima_ruta_pptx;
    std::string ultimo_elemento_quitado;
    std::string programa_estado_ahp;   // vacío = sin programa

    bool AgregarTexto(const std::string& titulo,
                      const std::vector<std::string>& lineas,
                      std::string* escenario_out,
                      std::string* elemento_out) override {
        if (lineas.empty()) return false;
        ultimo_titulo_texto = titulo;
        ultimas_lineas = lineas;
        if (escenario_out) *escenario_out = "esc-1";
        if (elemento_out)  *elemento_out  = "el-1";
        return true;
    }
    bool AgregarCanto(std::int64_t canto_id, std::string* escenario_out,
                      std::string* primer_elemento_out) override {
        if (canto_id != 5) return false;
        ultimo_canto_agregado = canto_id;
        if (escenario_out) *escenario_out = "esc-canto";
        if (primer_elemento_out) *primer_elemento_out = "el-canto-1";
        return true;
    }
    bool AgregarVersiculo(const std::string& cita,
                          const std::string& biblia, bool tercio,
                          std::string* escenario_out,
                          std::string* elemento_out) override {
        if (cita != "Juan 3:16") return false;
        ultima_cita_agregada = cita;
        ultimo_tercio = tercio;
        (void)biblia;
        if (escenario_out) *escenario_out = "esc-vers";
        if (elemento_out)  *elemento_out  = "el-vers-1";
        return true;
    }
    bool AgregarPptx(const std::string& ruta, std::string* escenario_out,
                     int* diapositivas_out) override {
        if (ruta != "clase.pptx") return false;
        ultima_ruta_pptx = ruta;
        if (escenario_out) *escenario_out = "esc-pptx";
        if (diapositivas_out) *diapositivas_out = 12;
        return true;
    }
    bool QuitarElemento(const std::string& escenario_id,
                        const std::string& elemento_id) override {
        (void)escenario_id;
        ultimo_elemento_quitado = elemento_id;
        return elemento_id == "el-1";
    }
    std::string ProgramaEstado() override {
        return programa_estado_ahp;
    }
    std::vector<CantoIpc> ListarCantos() override {
        CantoIpc c;
        c.id = 5; c.titulo = "Cuán grande es Él"; c.autor = "Anónimo";
        c.tono_origen = "C"; c.bpm = 72;
        return {c};
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

// ---------------------------------------------------------------------------
// Arranque en dos fases: el pipe existe antes de que el motor esté listo.
// Mientras carga, todo comando recibe E_CARGANDO con el id repetido.
// ---------------------------------------------------------------------------

TEST_CASE("RespuestaCargando: E_CARGANDO con id repetido y estructura de error") {
    auto r = RespuestaCargando("arranque-7");
    CHECK_FALSE(r.ok);
    CHECK_FALSE(r.suscribir);
    CHECK(Tiene(r.raw_json, "\"id\":\"arranque-7\""));
    CHECK(Tiene(r.raw_json, "E_CARGANDO"));
    CHECK(Tiene(r.raw_json, "\"ok\":false"));
    CHECK(Tiene(r.raw_json, "\"ipc\":\"fusion\""));
    CHECK(Tiene(r.raw_json, "\"version\":1"));
}

TEST_CASE("RespuestaCargando: sin id no rompe el JSON") {
    auto r = RespuestaCargando("");
    CHECK_FALSE(r.ok);
    CHECK(Tiene(r.raw_json, "E_CARGANDO"));
    // JSON válido: el despachador lo podría volver a parsear.
    nlohmann::json j = nlohmann::json::parse(r.raw_json, nullptr, false);
    CHECK_FALSE(j.is_discarded());
    CHECK(j.contains("error"));
}

// ---------------------------------------------------------------------------
// Programa construible por IPC (la consola puede ARMAR el culto)
// ---------------------------------------------------------------------------

TEST_CASE("ProcesarIpc: programa.estado con programa devuelve ahp parseado") {
    ServicioFalso s;
    s.programa_estado_ahp = R"({"formato":"ahp","version":1,)"
                            R"("meta":{"titulo":"Culto"},"escenarios":[]})";
    auto r = Enviar(s, R"({"ipc":"fusion","version":1,"type":"programa.estado","id":"p-1","payload":{}})");
    REQUIRE(r.ok);
    CHECK(Tiene(r.raw_json, "\"titulo\":\"Culto\""));
}

TEST_CASE("ProcesarIpc: programa.estado sin programa devuelve null") {
    ServicioFalso s;
    auto r = Enviar(s, R"({"ipc":"fusion","version":1,"type":"programa.estado","id":"p-2","payload":{}})");
    REQUIRE(r.ok);
    CHECK(Tiene(r.raw_json, "\"programa\":null"));
}

TEST_CASE("ProcesarIpc: programa.agregar_texto pasa titulo y lineas") {
    ServicioFalso s;
    auto r = Enviar(s, R"({"ipc":"fusion","version":1,"type":"programa.agregar_texto","id":"p-3","payload":{"titulo":"Bienvenida","lineas":["Bienvenidos","en el nombre del Señor"]}})");
    REQUIRE(r.ok);
    CHECK(s.ultimo_titulo_texto == "Bienvenida");
    REQUIRE(s.ultimas_lineas.size() == 2);
    CHECK(s.ultimas_lineas[1] == "en el nombre del Señor");
    CHECK(Tiene(r.raw_json, "\"escenario_id\":\"esc-1\""));
    CHECK(Tiene(r.raw_json, "\"elemento_id\":\"el-1\""));
}

TEST_CASE("ProcesarIpc: programa.agregar_texto sin lineas responde E_BAD_PAYLOAD") {
    ServicioFalso s;
    auto r = Enviar(s, R"({"ipc":"fusion","version":1,"type":"programa.agregar_texto","id":"p-4","payload":{"titulo":"vacio"}})");
    CHECK_FALSE(r.ok);
    CHECK(Tiene(r.raw_json, "E_BAD_PAYLOAD"));

    auto r2 = Enviar(s, R"({"ipc":"fusion","version":1,"type":"programa.agregar_texto","id":"p-5","payload":{"lineas":[1,2]}})");
    CHECK_FALSE(r2.ok);
    CHECK(Tiene(r2.raw_json, "E_BAD_PAYLOAD"));
}

TEST_CASE("ProcesarIpc: programa.agregar_canto pasa el id y falla honesto") {
    ServicioFalso s;
    auto r = Enviar(s, R"({"ipc":"fusion","version":1,"type":"programa.agregar_canto","id":"p-6","payload":{"id":5}})");
    REQUIRE(r.ok);
    CHECK(s.ultimo_canto_agregado == 5);

    auto r2 = Enviar(s, R"({"ipc":"fusion","version":1,"type":"programa.agregar_canto","id":"p-7","payload":{"id":999}})");
    CHECK_FALSE(r2.ok);
    CHECK(Tiene(r2.raw_json, "E_NOT_FOUND"));
}

TEST_CASE("ProcesarIpc: programa.agregar_versiculo distingue modo tercio") {
    ServicioFalso s;
    auto r = Enviar(s, R"({"ipc":"fusion","version":1,"type":"programa.agregar_versiculo","id":"p-8","payload":{"cita":"Juan 3:16"}})");
    REQUIRE(r.ok);
    CHECK(s.ultima_cita_agregada == "Juan 3:16");
    CHECK_FALSE(s.ultimo_tercio);

    auto r2 = Enviar(s, R"({"ipc":"fusion","version":1,"type":"programa.agregar_versiculo","id":"p-9","payload":{"cita":"Juan 3:16","modo":"tercio"}})");
    REQUIRE(r2.ok);
    CHECK(s.ultimo_tercio);

    auto r3 = Enviar(s, R"({"ipc":"fusion","version":1,"type":"programa.agregar_versiculo","id":"p-10","payload":{"cita":"Inexistente 1:1"}})");
    CHECK_FALSE(r3.ok);
    CHECK(Tiene(r3.raw_json, "E_NOT_FOUND"));
}

TEST_CASE("ProcesarIpc: programa.agregar_pptx reporta diapositivas") {
    ServicioFalso s;
    auto r = Enviar(s, R"({"ipc":"fusion","version":1,"type":"programa.agregar_pptx","id":"p-11","payload":{"ruta":"clase.pptx"}})");
    REQUIRE(r.ok);
    CHECK(s.ultima_ruta_pptx == "clase.pptx");
    CHECK(Tiene(r.raw_json, "\"diapositivas\":12"));

    auto r2 = Enviar(s, R"({"ipc":"fusion","version":1,"type":"programa.agregar_pptx","id":"p-12","payload":{"ruta":"roto.pptx"}})");
    CHECK_FALSE(r2.ok);
    CHECK(Tiene(r2.raw_json, "E_IO"));
}

TEST_CASE("ProcesarIpc: programa.quitar_elemento pasa los ids") {
    ServicioFalso s;
    auto r = Enviar(s, R"({"ipc":"fusion","version":1,"type":"programa.quitar_elemento","id":"p-13","payload":{"escenario_id":"esc-1","elemento_id":"el-1"}})");
    REQUIRE(r.ok);
    CHECK(s.ultimo_elemento_quitado == "el-1");

    auto r2 = Enviar(s, R"({"ipc":"fusion","version":1,"type":"programa.quitar_elemento","id":"p-14","payload":{"escenario_id":"esc-1","elemento_id":"el-404"}})");
    CHECK_FALSE(r2.ok);
    CHECK(Tiene(r2.raw_json, "E_NOT_FOUND"));
}

// ---------------------------------------------------------------------------
// Biblia completa: selección, árbol de libros, capítulos
// ---------------------------------------------------------------------------

TEST_CASE("ProcesarIpc: biblia.listar incluye la biblia activa") {
    ServicioFalso s;
    auto r = Enviar(s, R"({"ipc":"fusion","version":1,"type":"biblia.listar","id":"b-1","payload":{}})");
    REQUIRE(r.ok);
    CHECK(Tiene(r.raw_json, "\"activa\":\"RVR1909\""));
    CHECK(Tiene(r.raw_json, "RVR1909"));
}

TEST_CASE("ProcesarIpc: biblia.seleccionar pasa el nombre y valida") {
    ServicioFalso s;
    auto r = Enviar(s, R"({"ipc":"fusion","version":1,"type":"biblia.seleccionar","id":"b-2","payload":{"nombre":"RVR1909"}})");
    REQUIRE(r.ok);
    CHECK(s.ultima_biblia == "RVR1909");

    auto r2 = Enviar(s, R"({"ipc":"fusion","version":1,"type":"biblia.seleccionar","id":"b-3","payload":{"nombre":"NoExiste"}})");
    CHECK_FALSE(r2.ok);
    CHECK(Tiene(r2.raw_json, "E_NOT_FOUND"));
}

TEST_CASE("ProcesarIpc: biblia.libros da el árbol con testamento") {
    ServicioFalso s;
    auto r = Enviar(s, R"({"ipc":"fusion","version":1,"type":"biblia.libros","id":"b-4","payload":{}})");
    REQUIRE(r.ok);
    CHECK(Tiene(r.raw_json, "Génesis"));
    CHECK(Tiene(r.raw_json, "Apocalipsis"));
    CHECK(Tiene(r.raw_json, "\"testamento\":\"AT\""));
    CHECK(Tiene(r.raw_json, "\"testamento\":\"NT\""));
}

TEST_CASE("ProcesarIpc: biblia.capitulos valida rango y devuelve enteros") {
    ServicioFalso s;
    auto r = Enviar(s, R"({"ipc":"fusion","version":1,"type":"biblia.capitulos","id":"b-5","payload":{"libro":1}})");
    REQUIRE(r.ok);
    CHECK(Tiene(r.raw_json, "\"capitulos\":[1,2,3]"));

    auto r2 = Enviar(s, R"({"ipc":"fusion","version":1,"type":"biblia.capitulos","id":"b-6","payload":{"libro":99}})");
    CHECK_FALSE(r2.ok);
    CHECK(Tiene(r2.raw_json, "E_BAD_PAYLOAD"));
}

TEST_CASE("ProcesarIpc: canto.listar devuelve estructura con id") {
    ServicioFalso s;
    auto r = Enviar(s, R"({"ipc":"fusion","version":1,"type":"canto.listar","id":"c-1","payload":{}})");
    REQUIRE(r.ok);
    CHECK(Tiene(r.raw_json, "\"id\":5"));
    CHECK(Tiene(r.raw_json, "Cuán grande es Él"));
    CHECK(Tiene(r.raw_json, "\"tono_origen\":\"C\""));
}

TEST_CASE("ProcesarIpc: biblia.capitulo da los versículos del capítulo") {
    ServicioFalso s;
    auto r = Enviar(s, R"({"ipc":"fusion","version":1,"type":"biblia.capitulo","id":"b-7","payload":{"libro":1,"capitulo":1}})");
    REQUIRE(r.ok);
    CHECK(Tiene(r.raw_json, "En el principio"));

    auto r2 = Enviar(s, R"({"ipc":"fusion","version":1,"type":"biblia.capitulo","id":"b-8","payload":{"libro":1}})");
    CHECK_FALSE(r2.ok);
    CHECK(Tiene(r2.raw_json, "E_BAD_PAYLOAD"));
}

TEST_CASE("ProcesarIpc: biblia.favorito.agregar pasa la cita") {
    ServicioFalso s;
    auto r = Enviar(s, R"({"ipc":"fusion","version":1,"type":"biblia.favorito.agregar","id":"b-9","payload":{"cita":"Salmo 23:1"}})");
    REQUIRE(r.ok);
    CHECK(s.ultima_favorito == "Salmo 23:1");
}

TEST_CASE("ProcesarIpc: estado.lector incluye biblia_activa") {
    ServicioFalso s;
    s.estado.biblia_activa = "RVR1909";
    auto r = Enviar(s, R"({"ipc":"fusion","version":1,"type":"estado.lector","id":"e-1","payload":{}})");
    REQUIRE(r.ok);
    CHECK(Tiene(r.raw_json, "\"biblia_activa\":\"RVR1909\""));
}
