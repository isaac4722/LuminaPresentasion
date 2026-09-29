// src/core/src/core/main.cpp — Punto de entrada del núcleo FUSION-HP
//
// WinMain, sin consola (/SUBSYSTEM:windows). El núcleo es el dueño del
// estado de proyección: si la carcasa (C#/Qt) se cierra, el núcleo sigue
// proyectando y atendiendo el teclado sobre la salida.
//
// Orden de arranque (lección del "no se puede conectar con el Core"):
//   1. El servidor IPC se crea PRIMERO: el pipe \\.\pipe\FusionHP-ipc
//      existe en milisegundos, pase lo que pase después.
//   2. El motor carga después (ventana, D2D, BDs, siembra). Mientras
//      carga, los comandos reciben E_CARGANDO (código explícito, nada
//      silencioso); la consola reintenta hasta conectar y hasta que el
//      núcleo responda con el motor listo.
//   3. Loop de mensajes Win32.
//
// Arranque observable: cada paso queda en runtime/nucleo.log (sin
// Registro, solo disco) y toda excepción atrapada deja la causa escrita
// antes de salir con código distinto de 0. Nada de cierres mudos.

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>   // CommandLineToArgvW

#include "fusion/Version.h"
#include "fusion/core/Engine.h"
#include "fusion/core/IpcServer.h"
#include "fusion/core/IpcDespacho.h"
#include "fusion/core/Rutas.h"

#include <atomic>
#include <cstdio>
#include <cstring>
#include <string>

#include "json.hpp"   // extraer el id del comando mientras el motor carga

namespace {

// Manejo simple de línea de comandos:
//   FusionCore.exe --importar-biblia --formato json --entrada x.json --salida y.fdb
struct CliArgs {
    bool        importar_biblia = false;
    std::string formato;
    std::string entrada;
    std::string salida;
};

CliArgs ParseCli(int argc, wchar_t** argv) {
    CliArgs a;
    for (int i = 1; i < argc; ++i) {
        std::wstring w = argv[i];
        std::string s(w.begin(), w.end());
        if (s == "--importar-biblia")      a.importar_biblia = true;
        else if (s.rfind("--formato=", 0) == 0)  a.formato = s.substr(10);
        else if (s.rfind("--entrada=", 0) == 0)  a.entrada = s.substr(10);
        else if (s.rfind("--salida=", 0)  == 0)   a.salida  = s.substr(9);
    }
    return a;
}

// (RespuestaCargando vive en el despachador portable IpcDespacho.cpp:
// testeable en el arnés local, misma estructura de error que el resto.)

} // namespace

int APIENTRY wWinMain(HINSTANCE, HINSTANCE, PWSTR cmd_line, int) {
    using fusion::rutas::Bitacora;
    try {
        Bitacora(std::string("FusionCore ") + fusion::VersionString() +
                 " — arranque; raíz de datos: " +
                 std::string(fusion::rutas::CarpetaEscribible(
                                 fusion::rutas::RaizInstalacion())
                                 ? "portable (junto al exe)"
                                 : "%LOCALAPPDATA%\\FUSION-HP"));
        int argc;
        LPWSTR* argv = CommandLineToArgvW(cmd_line, &argc);
        if (!argv) {
            Bitacora("ERROR: CommandLineToArgvW falló");
            return 1;
        }

        CliArgs args = ParseCli(argc, argv);
        LocalFree(argv);

        // Modo CLI: importar biblia a un .fdb
        if (args.importar_biblia) {
            Bitacora("modo CLI importar-biblia (PENDIENTE de delegar al "
                     "importador; usar la consola gestionada)");
            return 0;
        }

        // Modo normal: PRIMERO el pipe IPC (existe en milisegundos).
        fusion::Engine motor;
        std::atomic<bool> motor_listo{false};

        fusion::IpcServer ipc;
        ipc.SetHandler([&motor, &motor_listo](const fusion::MensajeIpc& m) {
            if (!motor_listo.load()) {
                // Extraer el id del mensaje para repetirlo en el error.
                std::string id;
                {
                    nlohmann::json j = nlohmann::json::parse(
                        m.raw_json, nullptr, false);
                    if (!j.is_discarded() && j.contains("id") &&
                        j["id"].is_string())
                        id = j["id"].get<std::string>();
                }
                return fusion::RespuestaCargando(id);
            }
            fusion::AdaptadorEngine adaptador(motor);
            return fusion::ProcesarIpc(adaptador, m);
        });
        if (!ipc.Iniciar()) {
            Bitacora("ERROR: no se pudo iniciar el servidor IPC (código 4)");
            return 4;
        }
        Bitacora("servidor IPC escuchando en \\\\.\\pipe\\FusionHP-ipc "
                 "(el motor carga ahora en segundo plano del arranque)");

        // DESPUÉS el motor: ventana de proyección oculta + BDs (+ siembra
        // de la RVR1909 en el primer arranque, hoy en una sola transacción
        // de ~2,5 s; antes, minuto por minuto sin el pipe creado).
        if (!motor.Iniciar()) {
            Bitacora("ERROR: fallo al iniciar el motor (código 2)");
            return 2;
        }
        motor_listo.store(true);
        Bitacora("motor iniciado (ventana de proyección oculta + BDs): "
                 "el IPC ya atiende comandos completos");

        // Loop de mensajes Win32: la ventana de proyección y el pipe IPC
        // manejan sus propios hilos. El hilo principal solo bombea mensajes.
        MSG msg;
        while (GetMessage(&msg, nullptr, 0, 0) > 0) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        Bitacora("loop de mensajes terminado: cerrando IPC y motor");
        ipc.Detener();
        motor.Detener();
        Bitacora("salida limpia (código 0)");
        return static_cast<int>(msg.wParam);
    } catch (const std::exception& ex) {
        Bitacora(std::string("EXCEPCIÓN FATAL: ") + ex.what() + " (código 3)");
        return 3;
    } catch (...) {
        Bitacora("EXCEPCIÓN FATAL DESCONOCIDA (código 3)");
        return 3;
    }
}
