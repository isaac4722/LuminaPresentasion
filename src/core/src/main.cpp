// src/core/src/core/main.cpp — Punto de entrada del núcleo FUSION-HP
//
// WinMain, sin consola (/SUBSYSTEM:windows). El núcleo es el dueño del
// estado de proyección: si la carcasa (C#/Qt) se cierra, el núcleo sigue
// proyectando y atendiendo el teclado sobre la salida.
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

#include <cstdio>
#include <string>

namespace {

// Bitácora del núcleo: <exe>\runtime\nucleo.log (texto UTF-8).
void Bitacora(const std::string& linea) {
    wchar_t buf[MAX_PATH] = {0};
    GetModuleFileNameW(nullptr, buf, MAX_PATH);
    std::wstring exe(buf);
    size_t pos = exe.find_last_of(L"\\/");
    std::wstring dir = (pos != std::wstring::npos) ? exe.substr(0, pos) : L".";
    CreateDirectoryW((dir + L"\\runtime").c_str(), nullptr);
    HANDLE h = CreateFileW((dir + L"\\runtime\\nucleo.log").c_str(),
                           FILE_APPEND_DATA, FILE_SHARE_READ, nullptr,
                           OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return;
    SYSTEMTIME st;
    GetLocalTime(&st);
    char sello[64];
    swprintf_s(sello, 64, "[%04u-%02u-%02u %02u:%02u:%02u] ",
               st.wYear, st.wMonth, st.wDay,
               st.wHour, st.wMinute, st.wSecond);
    DWORD escritos = 0;
    WriteFile(h, sello, static_cast<DWORD>(strlen(sello)), &escritos, nullptr);
    WriteFile(h, linea.data(), static_cast<DWORD>(linea.size()),
              &escritos, nullptr);
    WriteFile(h, "\r\n", 2, &escritos, nullptr);
    CloseHandle(h);
}

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
    try {
        Bitacora(std::string("FusionCore ") + fusion::VersionString() +
                 " — arranque");
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

        // Modo normal: arrancar el motor y el servidor IPC
        fusion::Engine motor;
        if (!motor.Iniciar()) {
            Bitacora("ERROR: fallo al iniciar el motor (código 2)");
            return 2;
        }
        Bitacora("motor iniciado (ventana de proyección oculta + BDs)");

        fusion::IpcServer ipc;
        // El despachador ipc.v1 sobre el motor: TODOS los comandos de la
        // consola y del shell llegan aquí (antes no había handler y toda
        // respuesta volvía vacía).
        ipc.SetHandler([&motor](const fusion::MensajeIpc& m) {
            fusion::AdaptadorEngine adaptador(motor);
            return fusion::ProcesarIpc(adaptador, m);
        });
        if (!ipc.Iniciar()) {
            Bitacora("ERROR: no se pudo iniciar el servidor IPC (código 4)");
            return 4;
        }
        Bitacora("servidor IPC escuchando en \\\\.\\pipe\\FusionHP-ipc");

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
