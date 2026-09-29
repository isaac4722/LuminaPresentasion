// src/core/src/core/IpcServer.cpp — Servidor IPC con pipe con nombre
//
// Contrato de transporte (docs/agent/format_ipc_v1.md):
//  - Una conexión viva atiende MUCHOS mensajes (el cliente gestiona toda
//    la sesión sobre la misma conexión; cortar al primer mensaje dejaba
//    muertos todos los comandos posteriores).
//  - Cada respuesta es UNA línea JSON terminada en '\n' (los clientes
//    leen con ReadLine).
//  - Detener() no se cuelga: la espera de conexiones se hace troceada y
//    cancelable (antes ConnectNamedPipe bloqueante + join = deadlock que
//    impedía que FusionCore.exe saliera limpio).
//  - EmitirEvento escribe solo en conexiones suscritas (estado.suscribir).

#include "fusion/core/IpcServer.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <atomic>
#include <thread>
#include <vector>
#include <mutex>
#include <memory>
#include <string>

namespace fusion {

namespace {

constexpr DWORD kTamMensaje = 64 * 1024;

// Una conexión aceptada. El mutex serializa WriteFile entre la respuesta
// del hilo lector y los eventos emitidos desde otros hilos.
struct ClienteIpc {
    HANDLE pipe = nullptr;
    std::mutex m_write;
    std::atomic<bool> suscrito{false};   // estado.suscribir (despachador)
};

// Estado compartido que sobrevive al Impl: los hilos de atención capturan
// shared_ptr, así que pueden cerrar la conexión aunque IpcServer ya se
// haya destruido (proceso saliendo) sin tocar memoria muerta.
struct EstadoIpc {
    std::string pipe_nombre;
    std::atomic<bool> corriendo{false};
    std::mutex m_handler;
    IpcHandler handler;

    std::mutex m_clientes;
    std::vector<std::shared_ptr<ClienteIpc>> clientes;
};

// Declaración adelantada: EscribirCliente (abajo) la usa y MSVC exige
// verla declarada antes de la primera llamada.
bool IoSolapado(HANDLE pipe, const void* buf, DWORD tam, bool lectura,
                std::atomic<bool>* parar, DWORD* transferidos_out = nullptr);

bool EscribirCliente(EstadoIpc& e, const std::shared_ptr<ClienteIpc>& cli,
                     const std::string& datos) {
    std::lock_guard<std::mutex> lk(cli->m_write);
    DWORD escritos = 0;
    return IoSolapado(cli->pipe, datos.data(),
                      static_cast<DWORD>(datos.size()),
                      false /*escritura*/, nullptr, &escritos) &&
           escritos == datos.size();
}

// `parar` (opcional): atómico que corta la espera en trozos (parada del
// servidor). `transferidos_out` (opcional) recibe el número REAL de bytes
// leídos/escritos según GetOverlappedResult.
bool IoSolapado(HANDLE pipe, const void* buf, DWORD tam, bool lectura,
                std::atomic<bool>* parar, DWORD* transferidos_out) {
    if (transferidos_out) *transferidos_out = 0;

    OVERLAPPED ov = {};
    ov.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!ov.hEvent) return false;

    BOOL ok;
    if (lectura) {
        // ReadFile quiere LPVOID no-const; todas las llamadas internas de
        // lectura pasan el data() de un buffer propio y escribible.
        ok = ReadFile(pipe, const_cast<void*>(buf), tam, nullptr, &ov);
    } else {
        ok = WriteFile(pipe, buf, tam, nullptr, &ov);
    }

    if (!ok && GetLastError() == ERROR_IO_PENDING) {
        // Espera troceada: permite reaccionar a la parada del servidor.
        for (;;) {
            DWORD w = WaitForSingleObject(ov.hEvent, 150);
            if (w == WAIT_OBJECT_0) { ok = TRUE; break; }
            if (parar && !parar->load()) {
                CancelIoEx(pipe, &ov);
                DWORD dummy = 0;
                GetOverlappedResult(pipe, &ov, &dummy, TRUE);
                ok = FALSE;
                break;
            }
            // Sin `parar` (emisión de eventos): seguir esperando; una
            // escritura de evento no debe quedarse a medias.
        }
    }

    DWORD transferidos = 0;
    if (ok) {
        // Con solapado, el número de bytes SIEMPRE se lee así, incluso si
        // la operación completó al primer intento.
        if (!GetOverlappedResult(pipe, &ov, &transferidos, FALSE))
            ok = FALSE;
    }
    CloseHandle(ov.hEvent);
    if (transferidos_out) *transferidos_out = transferidos;
    return ok != FALSE;
}

// Hilo de atención de una conexión: lee mensajes hasta que el cliente se
// va (ERROR_BROKEN_PIPE / lectura vacía) o el servidor se detiene.
void AtenderConexion(std::shared_ptr<EstadoIpc> e,
                     std::shared_ptr<ClienteIpc> cli) {
    for (;;) {
        std::vector<char> buf(kTamMensaje);
        DWORD leidos = 0;
        const bool leyo = IoSolapado(cli->pipe, buf.data(),
                                     static_cast<DWORD>(buf.size() - 1),
                                     true /*lectura*/, &e->corriendo,
                                     &leidos);
        if (!leyo || leidos == 0) break;   // cliente desconectado o parada

        RespuestaIpc r;
        {
            std::lock_guard<std::mutex> lk(e->m_handler);
            if (e->handler) {
                r = e->handler(MensajeIpc{std::string(buf.data(), leidos)});
            } else {
                r.ok = false;
                r.raw_json =
                    "{\"ipc\":\"fusion\",\"version\":1,\"ok\":false,"
                    "\"error\":{\"code\":\"E_INTERNAL\","
                    "\"message\":\"nucleo sin despachador instalado\"}}";
            }
        }
        if (r.suscribir) cli->suscrito = true;   // transporte: eventos aquí
        // Framing: una línea por respuesta (clientes con ReadLine).
        if (r.raw_json.empty() || r.raw_json.back() != '\n')
            r.raw_json.push_back('\n');
        if (!EscribirCliente(*e, cli, r.raw_json)) break;
    }

    // Quitar la conexión de la lista de clientes.
    {
        std::lock_guard<std::mutex> lk(e->m_clientes);
        for (auto it = e->clientes.begin(); it != e->clientes.end(); ++it) {
            if (it->get() == cli.get()) { e->clientes.erase(it); break; }
        }
    }
    FlushFileBuffers(cli->pipe);
    DisconnectNamedPipe(cli->pipe);
    CloseHandle(cli->pipe);
}

} // namespace

struct IpcServer::Impl {
    std::shared_ptr<EstadoIpc> e = std::make_shared<EstadoIpc>();
    std::thread hilo;
};

namespace {

// Bucle de aceptación: crea una instancia del pipe, espera cliente
// (en trozos de 150 ms para poder salir limpio), se lo entrega a un hilo
// y vuelve a crear la siguiente instancia.
void BucleAceptacion(std::shared_ptr<EstadoIpc> e) {
    while (e->corriendo) {
        HANDLE pipe = CreateNamedPipeA(
            e->pipe_nombre.c_str(),
            PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
            PIPE_UNLIMITED_INSTANCES,
            kTamMensaje, kTamMensaje, 0, nullptr);
        if (pipe == INVALID_HANDLE_VALUE) {
            Sleep(200);
            continue;
        }

        OVERLAPPED ov = {};
        ov.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        if (!ov.hEvent) { CloseHandle(pipe); Sleep(50); continue; }

        BOOL conectado = ConnectNamedPipe(pipe, &ov);
        DWORD err = GetLastError();
        if (!conectado && err == ERROR_IO_PENDING) {
            bool senalado = false;
            while (e->corriendo) {
                DWORD w = WaitForSingleObject(ov.hEvent, 150);
                if (w == WAIT_OBJECT_0) { senalado = true; break; }
            }
            conectado = senalado ? TRUE : FALSE;
        } else if (!conectado && err == ERROR_PIPE_CONNECTED) {
            conectado = TRUE;   // el cliente llegó antes de la espera
        }
        CloseHandle(ov.hEvent);

        if (!e->corriendo) {
            if (!conectado) CancelIoEx(pipe, nullptr);
            CloseHandle(pipe);
            break;
        }
        if (!conectado) { CloseHandle(pipe); continue; }

        auto cli = std::make_shared<ClienteIpc>();
        cli->pipe = pipe;
        {
            std::lock_guard<std::mutex> lk(e->m_clientes);
            e->clientes.push_back(cli);
        }
        std::thread(AtenderConexion, e, cli).detach();
    }
}

} // namespace

IpcServer::IpcServer()  : impl_(std::make_unique<Impl>()) {}
IpcServer::~IpcServer() { Detener(); }

bool IpcServer::Iniciar(const std::string& pipe_nombre) {
    impl_->e->pipe_nombre = pipe_nombre;
    impl_->e->corriendo = true;
    impl_->hilo = std::thread(BucleAceptacion, impl_->e);
    return true;
}

void IpcServer::Detener() {
    auto e = impl_->e;
    e->corriendo = false;
    if (impl_->hilo.joinable()) impl_->hilo.join();   // ≤150 ms (espera troceada)

    std::vector<std::shared_ptr<ClienteIpc>> copia;
    {
        std::lock_guard<std::mutex> lk(e->m_clientes);
        copia.swap(e->clientes);
    }
    // Despertar a los hilos de atención: su lectura fallará y saldrán.
    for (auto& cli : copia) {
        CancelIoEx(cli->pipe, nullptr);
        DisconnectNamedPipe(cli->pipe);
    }
}

void IpcServer::SetHandler(IpcHandler handler) {
    std::lock_guard<std::mutex> lk(impl_->e->m_handler);
    impl_->e->handler = std::move(handler);
}

void IpcServer::EmitirEvento(const std::string& evento_tipo,
                              const std::string& payload_json) {
    auto e = impl_->e;
    std::string ev = "{\"ipc\":\"fusion\",\"version\":1,\"type\":\"evento." +
                     evento_tipo + "\",\"payload\":" + payload_json + "}\n";
    std::vector<std::shared_ptr<ClienteIpc>> copia;
    {
        std::lock_guard<std::mutex> lk(e->m_clientes);
        copia = e->clientes;
    }
    for (auto& cli : copia)
        if (cli->suscrito) EscribirCliente(*e, cli, ev);
}

} // namespace fusion
