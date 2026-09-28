// src/core/src/core/Rutas.cpp — Raíces de datos escribibles (Windows)

#include "fusion/core/Rutas.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <cstdio>
#include <cstring>
#include <string>

namespace fusion {
namespace rutas {

namespace {

std::wstring ExeDirInterno() {
    wchar_t buf[MAX_PATH] = {0};
    GetModuleFileNameW(nullptr, buf, MAX_PATH);
    std::wstring p(buf);
    size_t pos = p.find_last_of(L"\\/");
    return (pos != std::wstring::npos) ? p.substr(0, pos) : L".";
}

std::wstring LocalAppData() {
    wchar_t buf[MAX_PATH] = {0};
    DWORD n = GetEnvironmentVariableW(L"LOCALAPPDATA", buf, MAX_PATH);
    if (n > 0 && n < MAX_PATH) return buf;
    return L"";
}

// Asegura una cadena de carpetas (a\b\c) sin filesystem.
void CrearArbolCarpetas(const std::wstring& base, const wchar_t* relativo) {
    std::wstring actual = base;
    std::wstring rel(relativo);
    size_t i = 0;
    while (i < rel.size()) {
        size_t barra = rel.find(L'\\', i);
        if (barra == std::wstring::npos) barra = rel.size();
        if (barra > i) {
            actual += L"\\" + rel.substr(i, barra - i);
            CreateDirectoryW(actual.c_str(), nullptr);
        }
        i = barra + 1;
    }
}

std::string AUtf8(const std::wstring& w) {
    if (w.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(),
                                static_cast<int>(w.size()),
                                nullptr, 0, nullptr, nullptr);
    std::string s(static_cast<size_t>(n), '\0');
    if (n > 0)
        WideCharToMultiByte(CP_UTF8, 0, w.c_str(), static_cast<int>(w.size()),
                            &s[0], n, nullptr, nullptr);
    return s;
}

} // namespace

std::wstring RaizInstalacion() { return ExeDirInterno(); }

bool CarpetaEscribible(const std::wstring& dir) {
    std::wstring prueba = dir + L"\\fusion_escritura.prueba";
    HANDLE h = CreateFileW(prueba.c_str(), GENERIC_WRITE, 0, nullptr,
                           CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    CloseHandle(h);
    DeleteFileW(prueba.c_str());
    return true;
}

const std::wstring& RaizDatosW() {
    static std::wstring raiz = [] {
        std::wstring exe = ExeDirInterno();
        if (CarpetaEscribible(exe)) return exe;   // portable de verdad
        std::wstring lad = LocalAppData();
        if (!lad.empty()) {
            std::wstring dir = lad + L"\\FUSION-HP";
            CreateDirectoryW(dir.c_str(), nullptr);
            CrearArbolCarpetas(dir, L"data\\bibles");
            CrearArbolCarpetas(dir, L"runtime");
            return dir;
        }
        return exe;   // último recurso: fallará la escritura (como antes)
    }();
    return raiz;
}

std::string CancioneroFdb() {
    return AUtf8(RaizDatosW() + L"\\data\\cancionero.fdb");
}

std::wstring CarpetaBibliasW() {
    return RaizDatosW() + L"\\data\\bibles";
}

std::string CarpetaBiblias() { return AUtf8(CarpetaBibliasW()); }

std::string BibliaFdb(const std::string& nombre_fdb) {
    // nombre_fdb ya viene en ASCII (RVR1909.fdb); ruta UTF-8 lista.
    return AUtf8(CarpetaBibliasW()) + "\\" + nombre_fdb;
}

std::string SesionJson() {
    return AUtf8(RaizDatosW() + L"\\runtime\\session.json");
}

std::string BitacoraNucleo() {
    return AUtf8(RaizDatosW() + L"\\runtime\\nucleo.log");
}

std::string SemillaRVR1909Json() {
    std::wstring ruta = ExeDirInterno() + L"\\data\\bibles\\RVR1909.json";
    if (!ExisteArchivo(ruta)) return {};
    return AUtf8(ruta);
}

bool ExisteArchivo(const std::wstring& ruta) {
    DWORD a = GetFileAttributesW(ruta.c_str());
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

void Bitacora(const std::string& linea) {
    std::wstring dir = RaizDatosW() + L"\\runtime";
    CreateDirectoryW(dir.c_str(), nullptr);
    HANDLE h = CreateFileW((dir + L"\\nucleo.log").c_str(),
                           FILE_APPEND_DATA, FILE_SHARE_READ, nullptr,
                           OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return;
    SYSTEMTIME st;
    GetLocalTime(&st);
    char sello[64];
    snprintf(sello, sizeof(sello), "[%04u-%02u-%02u %02u:%02u:%02u] ",
             st.wYear, st.wMonth, st.wDay,
             st.wHour, st.wMinute, st.wSecond);
    DWORD escritos = 0;
    WriteFile(h, sello, static_cast<DWORD>(strlen(sello)), &escritos, nullptr);
    WriteFile(h, linea.data(), static_cast<DWORD>(linea.size()),
              &escritos, nullptr);
    WriteFile(h, "\r\n", 2, &escritos, nullptr);
    CloseHandle(h);
}

} // namespace rutas
} // namespace fusion
