// src/core/src/core/Session.cpp — Persistencia de sesión JSON
//
// La sesión vive en la raíz de datos ESCRIBIBLE (Rutas::SesionJson):
// portable = junto al exe; instalado en Program Files =
// %LOCALAPPDATA%\FUSION-HP\runtime\session.json. Las lecturas/escrituras
// usan rutas anchas (la raíz puede contener caracteres no ASCII, p. ej.
// el nombre del usuario en %LOCALAPPDATA%).

#include "fusion/core/Session.h"
#include "fusion/core/Rutas.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "json.hpp"

#include <fstream>
#include <sstream>
#include <string>

namespace fusion {

namespace fs {

static std::string AUtf8(const std::wstring& w) {
    if (w.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(),
                                nullptr, 0, nullptr, nullptr);
    std::string s((size_t)n, '\0');
    if (n > 0)
        WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), &s[0], n,
                            nullptr, nullptr);
    return s;
}

static std::wstring AAncho(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(),
                                nullptr, 0);
    std::wstring w((size_t)n, L'\0');
    if (n > 0)
        MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &w[0], n);
    return w;
}

static bool Leer(const std::string& ruta, std::string* out) {
    // ifstream con ruta ancha: extensión MSVC, necesaria para raíces
    // con caracteres fuera de la página ANSI.
    std::ifstream f(AAncho(ruta).c_str(), std::ios::binary);
    if (!f) return false;
    std::stringstream ss; ss << f.rdbuf();
    *out = ss.str();
    return true;
}

static bool Escribir(const std::string& ruta, const std::string& contenido) {
    std::ofstream f(AAncho(ruta).c_str(), std::ios::binary | std::ios::trunc);
    if (!f) return false;
    f.write(contenido.data(), static_cast<std::streamsize>(contenido.size()));
    return f.good();
}

} // namespace fs

std::string Sesion::RutaPorDefecto() {
    return rutas::SesionJson();
}

bool Sesion::Cargar(const std::string& ruta, Sesion* out) {
    std::string txt;
    if (!fs::Leer(ruta, &txt)) return false;
    try {
        auto j = nlohmann::json::parse(txt);
        out->programa_ruta       = j.value("programa_ruta", "");
        out->escenario_id        = j.value("escenario_id", "");
        out->elemento_id         = j.value("elemento_id", "");
        out->linea_actual        = j.value("linea_actual", 0);
        out->salida_visible      = j.value("salida_visible", false);
        out->negro               = j.value("negro", false);
        out->logo                = j.value("logo", false);
        out->monitor_dispositivo = j.value("monitor_dispositivo", "");
        out->tema_runtime        = j.value("tema_runtime", "");
        out->recientes.clear();
        if (j.contains("recientes") && j["recientes"].is_array()) {
            for (auto& r : j["recientes"]) {
                Reciente rec;
                rec.ruta = r.value("ruta", "");
                rec.veces_usado = r.value("veces_usado", 0);
                out->recientes.push_back(rec);
            }
        }
        return true;
    } catch (...) {
        return false;
    }
}

bool Sesion::Guardar(const std::string& ruta, const Sesion& s) {
    nlohmann::json j;
    j["programa_ruta"]       = s.programa_ruta;
    j["escenario_id"]        = s.escenario_id;
    j["elemento_id"]         = s.elemento_id;
    j["linea_actual"]        = s.linea_actual;
    j["salida_visible"]      = s.salida_visible;
    j["negro"]               = s.negro;
    j["logo"]                = s.logo;
    j["monitor_dispositivo"] = s.monitor_dispositivo;
    j["tema_runtime"]        = s.tema_runtime;
    nlohmann::json recs = nlohmann::json::array();
    for (auto& r : s.recientes) {
        recs.push_back({{"ruta", r.ruta}, {"veces_usado", r.veces_usado}});
    }
    j["recientes"] = recs;

    // Asegurar que existe runtime/
    size_t pos = ruta.find_last_of("\\/");
    if (pos != std::string::npos) {
        std::wstring dir = fs::AAncho(ruta.substr(0, pos));
        CreateDirectoryW(dir.c_str(), nullptr);
    }

    return fs::Escribir(ruta, j.dump(2));
}

} // namespace fusion
