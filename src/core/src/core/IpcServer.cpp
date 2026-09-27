// src/core/src/core/IpcServer.cpp — Servidor IPC con pipe con nombre

#include "fusion/core/IpcServer.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <thread>
#include <atomic>
#include <vector>
#include <mutex>
#include <string>

namespace fusion {

struct IpcServer::Impl {
    std::string pipe_nombre;
    std::thread hilo;
    std::atomic<bool> corriendo{false};
    IpcHandler handler;
    std::mutex m_handler;

    // Lista de clientes conectados para emitir eventos.
    std::mutex m_clientes;
    std::vector<HANDLE> clientes;

    void Loop();
    void Atender(HANDLE pipe);
};

void IpcServer::Impl::Loop() {
    while (corriendo) {
        HANDLE pipe = CreateNamedPipeA(
            pipe_nombre.c_str(),
            PIPE_ACCESS_DUPLEX,
            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
            PIPE_UNLIMITED_INSTANCES,
            64 * 1024, 64 * 1024, 0, nullptr);
        if (pipe == INVALID_HANDLE_VALUE) {
            Sleep(200);
            continue;
        }
        if (ConnectNamedPipe(pipe, nullptr) || GetLastError() == ERROR_PIPE_CONNECTED) {
            std::thread t(&Impl::Atender, this, pipe);
            t.detach();
        } else {
            CloseHandle(pipe);
        }
    }
}

void IpcServer::Impl::Atender(HANDLE pipe) {
    char buf[64 * 1024];
    DWORD leidos = 0;
    BOOL ok = ReadFile(pipe, buf, sizeof(buf) - 1, &leidos, nullptr);
    if (ok && leidos > 0) {
        buf[leidos] = 0;
        MensajeIpc msg{std::string(buf, leidos)};
        RespuestaIpc r;
        {
            std::lock_guard<std::mutex> lk(m_handler);
            if (handler) r = handler(msg);
        }
        DWORD escritos = 0;
        WriteFile(pipe, r.raw_json.data(),
                  static_cast<DWORD>(r.raw_json.size()),
                  &escritos, nullptr);
    }
    FlushFileBuffers(pipe);
    DisconnectNamedPipe(pipe);
    CloseHandle(pipe);
}

IpcServer::IpcServer()  : impl_(std::make_unique<Impl>()) {}
IpcServer::~IpcServer() { Detener(); }

bool IpcServer::Iniciar(const std::string& pipe_nombre) {
    impl_->pipe_nombre = pipe_nombre;
    impl_->corriendo = true;
    impl_->hilo = std::thread(&Impl::Loop, impl_.get());
    return true;
}

void IpcServer::Detener() {
    impl_->corriendo = false;
    // Cerrar potenciales pipes en espera conectando un dummy
    // (en producción se cancela con CancelIoEx).
    if (impl_->hilo.joinable()) impl_->hilo.join();
}

void IpcServer::SetHandler(IpcHandler handler) {
    std::lock_guard<std::mutex> lk(impl_->m_handler);
    impl_->handler = std::move(handler);
}

void IpcServer::EmitirEvento(const std::string& evento_tipo,
                              const std::string& payload_json) {
    // TODO(P0): mantener lista de clientes suscritos y escribir el evento.
    (void)evento_tipo; (void)payload_json;
}

} // namespace fusion
