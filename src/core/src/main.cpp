// src/core/src/main.cpp — Punto de entrada del núcleo FUSION-HP
//
// WinMain, sin consola (/SUBSYSTEM:windows). El núcleo es el dueño del
// estado de proyección: si la carcasa (C#/Qt) se cierra, el núcleo sigue
// proyectando y atendiendo el teclado sobre la salida.

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>   // CommandLineToArgvW

#include "fusion/Version.h"
#include "fusion/core/Engine.h"
#include "fusion/core/IpcServer.h"

#include <cstdio>
#include <string>

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

} // namespace

int APIENTRY wWinMain(HINSTANCE, HINSTANCE, PWSTR cmd_line, int) {
    int argc;
    LPWSTR* argv = CommandLineToArgvW(cmd_line, &argc);
    if (!argv) return 1;

    CliArgs args = ParseCli(argc, argv);
    LocalFree(argv);

    // Modo CLI: importar biblia a un .fdb
    if (args.importar_biblia) {
        // TODO(P0): delegar al importador correspondiente.
        // Por ahora, modo "cimientos": registra y sale.
        std::string msg = "FusionCore " + std::string(fusion::VersionString())
                        + " — importar biblia (formato=" + args.formato
                        + ", entrada=" + args.entrada
                        + ", salida=" + args.salida + ") — PENDIENTE\n";
        OutputDebugStringA(msg.c_str());
        return 0;
    }

    // Modo normal: arrancar el motor y el servidor IPC
    fusion::Engine motor;
    if (!motor.Iniciar()) {
        OutputDebugStringA("FusionCore: fallo al iniciar motor\n");
        return 2;
    }

    fusion::IpcServer ipc;
    ipc.Iniciar();

    // Loop de mensajes Win32: la ventana de proyección y el pipe IPC
    // manejan sus propios hilos. El hilo principal solo bombea mensajes.
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    ipc.Detener();
    motor.Detener();
    return static_cast<int>(msg.wParam);
}
