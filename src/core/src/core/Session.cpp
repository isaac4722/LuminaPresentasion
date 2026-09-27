// src/core/src/core/Session.cpp — Persistencia de sesión JSON

#include "fusion/core/Session.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

// Stub de nlohmann::json (en el cimiento se reemplaza por el header real)
#include "json.hpp"

#include <fstream>
#include <sstream>

namespace fusion {

namespace fs {

static std::string ExeDir() {
    wchar_t buf[MAX_PATH] = {0};
    GetModuleFileNameW(nullptr, buf, MAX_PATH);
    std::wstring w(buf);
    size_t pos = w.find_last_of(L"\\/");
    if (pos == std::wstring::npos) return ".";
    std::string s(w.begin(), w.end());
    return s.substr(0, pos);
}

static bool Leer(const std::string& ruta, std::string* out) {
    std::ifstream f(ruta, std::ios::binary);
    if (!f) return false;
    std::stringstream ss; ss << f.rdbuf();
    *out = ss.str();
    return true;
}

static bool Escribir(const std::string& ruta, const std::string& contenido) {
    std::ofstream f(ruta, std::ios::binary | std::ios::trunc);
    if (!f) return false;
    f.write(contenido.data(), static_cast<std::streamsize>(contenido.size()));
    return f.good();
}

} // namespace fs

std::string Sesion::RutaPorDefecto() {
    return fs::ExeDir() + "\\runtime\\session.json";
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
        out->logo                 = j.value("logo", false);
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
    j["logo"]                 = s.logo;
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
        std::string dir = ruta.substr(0, pos);
        CreateDirectoryA(dir.c_str(), nullptr);
    }

    return fs::Escribir(ruta, j.dump(2));
}

} // namespace fusion
