// src/core/src/core/Diagnostic.cpp — Implementación del autotest

#include "fusion/core/Diagnostic.h"
#include "fusion/data/SongDatabase.h"
#include "fusion/data/BibleDatabase.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d2d1.h>

#include <fstream>
#include <sstream>
#include <string>

namespace fusion {

namespace {

bool ProbarRender() {
    ID2D1Factory* f = nullptr;
    HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,
                                    IID_PPV_ARGS(&f));
    if (FAILED(hr) || !f) return false;
    f->Release();
    return true;
}

bool ProbarPermisosRuntime() {
    wchar_t buf[MAX_PATH] = {0};
    GetModuleFileNameW(nullptr, buf, MAX_PATH);
    std::wstring p(buf);
    size_t pos = p.find_last_of(L"\\/");
    if (pos == std::wstring::npos) return false;
    std::wstring dir = p.substr(0, pos) + L"\\runtime";
    CreateDirectoryW(dir.c_str(), nullptr);
    std::wstring archivo = dir + L"\\autotest_probe.tmp";
    HANDLE h = CreateFileW(archivo.c_str(), GENERIC_WRITE, 0, nullptr,
                            CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    const char* msg = "probe";
    DWORD escritos = 0;
    WriteFile(h, msg, 5, &escritos, nullptr);
    CloseHandle(h);
    DeleteFileW(archivo.c_str());
    return true;
}

bool ProbarBdCantos() {
    SongDatabase db;
    // Abrir en memoria para no requerir el archivo real
    if (!db.Abrir(":memory:")) return false;
    auto lista = db.ListarTodos();
    db.Cerrar();
    return true;  // OK aunque esté vacía
}

bool ProbarBdBiblia() {
    BibleDatabase db;
    if (!db.Abrir(":memory:")) return false;
    auto libros = db.ListarLibros();
    db.Cerrar();
    return libros.size() == 66;
}

} // namespace

std::vector<ResultadoAutotest> Diagnostic::EjecutarAutotest() {
    std::vector<ResultadoAutotest> out;

    ResultadoAutotest r_render;
    r_render.nombre = "Render Direct2D";
    r_render.ok = ProbarRender();
    r_render.detalle = r_render.ok ? "Factoría D2D1 creada" : "Fallo al crear factoría D2D1";
    out.push_back(r_render);

    ResultadoAutotest r_perm;
    r_perm.nombre = "Permisos runtime/";
    r_perm.ok = ProbarPermisosRuntime();
    r_perm.detalle = r_perm.ok ? "Lectura/escritura OK" : "Sin permisos en runtime/";
    out.push_back(r_perm);

    ResultadoAutotest r_cantos;
    r_cantos.nombre = "BD cantos";
    r_cantos.ok = ProbarBdCantos();
    r_cantos.detalle = r_cantos.ok ? "SQLite operativo" : "SQLite no abre";
    out.push_back(r_cantos);

    ResultadoAutotest r_biblia;
    r_biblia.nombre = "BD biblia";
    r_biblia.ok = ProbarBdBiblia();
    r_biblia.detalle = r_biblia.ok ? "66 libros cargados" : "Biblia no carga";
    out.push_back(r_biblia);

    return out;
}

std::vector<std::string> Diagnostic::LogTail(int n) {
    std::vector<std::string> out;
    wchar_t buf[MAX_PATH] = {0};
    GetModuleFileNameW(nullptr, buf, MAX_PATH);
    std::wstring p(buf);
    size_t pos = p.find_last_of(L"\\/");
    if (pos == std::wstring::npos) return out;
    std::wstring archivo = p.substr(0, pos) + L"\\logs\\fusion.log";

    std::ifstream f(archivo, std::ios::binary);
    if (!f) return out;

    // Cargar todas las líneas y devolver las últimas n
    std::vector<std::string> todas;
    std::string linea;
    while (std::getline(f, linea, '\n')) todas.push_back(linea);
    int start = static_cast<int>(todas.size()) - n;
    if (start < 0) start = 0;
    for (int i = start; i < static_cast<int>(todas.size()); ++i)
        out.push_back(todas[i]);
    return out;
}

bool Diagnostic::VerificarEntorno(std::vector<ResultadoAutotest>* out) {
    auto r = EjecutarAutotest();
    if (out) *out = r;
    for (auto& x : r) if (!x.ok) return false;
    return true;
}

} // namespace fusion
