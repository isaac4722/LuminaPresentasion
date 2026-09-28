// src/core/include/fusion/core/IpcDespacho.h
// Despachador del protocolo ipc.v1 (docs/agent/format_ipc_v1.md).
//
// El despacho es PORTABLE (nlohmann + std): depende de la interfaz
// IServicioNucleo, no del motor directamente, para poder probarlo con un
// arnés falso (tests/native/test_ipc_despacho.cpp). El núcleo real lo
// implementa con AdaptadorEngine sobre Engine.
//
// El núcleo nunca respondía a ningún comando: main.cpp jamás llamó a
// IpcServer::SetHandler — toda respuesta llegaba vacía. Este módulo es
// la mitad que faltaba del protocolo.

#pragma once

#include "fusion/core/IpcServer.h"
#include "fusion/core/Engine.h"   // EstadoMotor (y el motor para el adaptador)

#include <cstdint>
#include <string>
#include <vector>

namespace fusion {

class Engine;   // fwd (solo lo usa AdaptadorEngine)

// DTOs portable del despachador (sin tipos de Windows ni de las BD).
struct MonitorIpc {
    std::string dispositivo, nombre;
    int  x = 0, y = 0, w = 0, h = 0;
    bool primario = false;
};

struct VersiculoIpc {
    std::string libro;          // nombre del libro ("Génesis")
    int         capitulo = 0;
    int         versiculo = 0;
    std::string texto;
};

struct SeccionCantoIpc {
    int          orden = 0;
    std::string  tipo;          // verso, coro, puente...
    std::string  etiqueta;      // "Verso 1"
    std::vector<std::string> lineas;
};

struct CantoIpc {
    std::int64_t id = 0;
    std::string  titulo, autor, tono_origen;
    int          bpm = 0;
    std::vector<SeccionCantoIpc> secciones;   // vacío en listados
};

struct PruebaDiag {
    std::string nombre;
    bool        ok = false;
    std::string detalle;
};

// Interfaz del núcleo vista por el despachador (testeable con un falso).
struct IServicioNucleo {
    virtual ~IServicioNucleo() = default;

    // Estado ----------------------------------------------------------
    virtual EstadoMotor Estado() = 0;

    // Programa ---------------------------------------------------------
    virtual bool AbrirPrograma(const std::string& ruta) = 0;
    virtual bool NuevoPrograma(const std::string& titulo) = 0;
    virtual bool GuardarPrograma(const std::string& ruta) = 0;
    virtual bool CerrarPrograma() = 0;
    virtual std::vector<std::string> Recientes() = 0;

    // Proyección -------------------------------------------------------
    virtual bool IniciarProyeccion() = 0;
    virtual bool DetenerProyeccion() = 0;
    virtual bool IrEscenario(const std::string& escenario_id) = 0;
    virtual bool IrElemento(const std::string& escenario_id,
                            const std::string& elemento_id) = 0;
    virtual bool IrLinea(const std::string& escenario_id,
                         const std::string& elemento_id, int linea) = 0;
    virtual bool Siguiente() = 0;
    virtual bool Anterior() = 0;
    virtual bool SetNegro(bool activo) = 0;
    virtual bool SetLogo(bool activo) = 0;
    virtual bool OcultarSalida() = 0;

    // Monitor ----------------------------------------------------------
    virtual std::vector<MonitorIpc> ListarMonitores() = 0;
    virtual bool SeleccionarMonitor(const std::string& dispositivo) = 0;

    // Biblia -----------------------------------------------------------
    virtual std::vector<std::string> ListarBiblias() = 0;
    virtual bool ObtenerVersiculo(const std::string& biblia,
                                  const std::string& cita,
                                  std::string* texto_out) = 0;
    virtual std::vector<VersiculoIpc> BuscarBiblia(const std::string& texto,
                                                   int limite) = 0;
    virtual std::vector<std::string> FavoritosBiblia() = 0;

    // Cantos -----------------------------------------------------------
    virtual std::vector<std::string> ListarCantos() = 0;
    virtual CantoIpc ObtenerCanto(std::int64_t id, bool* ok) = 0;
    virtual std::vector<CantoIpc> BuscarCantos(const std::string& texto,
                                               int limite) = 0;

    // Diagnóstico --------------------------------------------------------
    virtual std::vector<PruebaDiag> Autotest() = 0;
    virtual std::vector<std::string> ColaLog(int n) = 0;
};

// Adaptador real: delega en el motor de FUSION-HP.
class AdaptadorEngine : public IServicioNucleo {
public:
    explicit AdaptadorEngine(Engine& motor) : motor_(motor) {}

    EstadoMotor Estado() override;

    bool AbrirPrograma(const std::string& ruta) override;
    bool NuevoPrograma(const std::string& titulo) override;
    bool GuardarPrograma(const std::string& ruta) override;
    bool CerrarPrograma() override;
    std::vector<std::string> Recientes() override;

    bool IniciarProyeccion() override;
    bool DetenerProyeccion() override;
    bool IrEscenario(const std::string& escenario_id) override;
    bool IrElemento(const std::string& escenario_id,
                    const std::string& elemento_id) override;
    bool IrLinea(const std::string& escenario_id,
                 const std::string& elemento_id, int linea) override;
    bool Siguiente() override;
    bool Anterior() override;
    bool SetNegro(bool activo) override;
    bool SetLogo(bool activo) override;
    bool OcultarSalida() override;

    std::vector<MonitorIpc> ListarMonitores() override;
    bool SeleccionarMonitor(const std::string& dispositivo) override;

    std::vector<std::string> ListarBiblias() override;
    bool ObtenerVersiculo(const std::string& biblia,
                          const std::string& cita,
                          std::string* texto_out) override;
    std::vector<VersiculoIpc> BuscarBiblia(const std::string& texto,
                                           int limite) override;
    std::vector<std::string> FavoritosBiblia() override;

    std::vector<std::string> ListarCantos() override;
    CantoIpc ObtenerCanto(std::int64_t id, bool* ok) override;
    std::vector<CantoIpc> BuscarCantos(const std::string& texto,
                                       int limite) override;

    std::vector<PruebaDiag> Autotest() override;
    std::vector<std::string> ColaLog(int n) override;

private:
    Engine& motor_;
};

// Procesa un mensaje ipc.v1 y produce la respuesta (SIN el '\n' final:
// el transporte lo añade). Nunca lanza: cualquier fallo sale como
// respuesta de error con el id repetido (o vacío si no llegó id).
RespuestaIpc ProcesarIpc(IServicioNucleo& servicio, const MensajeIpc& entrada);

} // namespace fusion
