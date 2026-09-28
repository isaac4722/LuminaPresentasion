// src/core/include/fusion/core/IpcServer.h
// Servidor IPC del núcleo. Pipe con nombre \\.\pipe\FusionHP-ipc.
// Solo conexiones locales del mismo SID de usuario.

#pragma once

#include <functional>
#include <memory>
#include <string>

namespace fusion {

// Mensaje IPC entrante: JSON parseado a string (el núcleo lo vuelve a parsear
// internamente con nlohmann/json para sacar type/payload).
struct MensajeIpc {
    std::string raw_json;
};

// Respuesta IPC saliente: JSON serializado. El servidor añade el salto
// de línea final (una línea por mensaje; los clientes leen con ReadLine).
// `suscribir` es una señal de transporte: el despachador la activa cuando
// el comando es estado.suscribir y el servidor marca la conexión como
// suscrita a eventos (EmitirEvento solo escribe en conexiones suscritas).
struct RespuestaIpc {
    std::string raw_json;
    bool       ok = true;
    bool       suscribir = false;
};

// Handler: recibe el mensaje, devuelve la respuesta.
using IpcHandler = std::function<RespuestaIpc(const MensajeIpc&)>;

class IpcServer {
public:
    IpcServer();
    ~IpcServer();

    IpcServer(const IpcServer&)            = delete;
    IpcServer& operator=(const IpcServer&) = delete;

    bool Iniciar(const std::string& pipe_nombre = "\\\\.\\pipe\\FusionHP-ipc");
    void Detener();

    void SetHandler(IpcHandler handler);

    // Notifica a todos los clientes conectados (eventos sin petición).
    void EmitirEvento(const std::string& evento_tipo,
                      const std::string& payload_json);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace fusion
