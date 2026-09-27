// src/core/src/core/HerenciaTemas.cpp
// Resolución de herencia de temas en 4 niveles + informe de fidelidad.
// Portable: no toca API de Windows (los tests nativos corren en cualquier
// plataforma que compile C++17).

#include "fusion/core/HerenciaTemas.h"

#include "fusion/core/Renderer.h"
#include "json.hpp"

#include <cstdlib>
#include <fstream>

namespace fusion {

// ---------------------------------------------------------------------------
// Utilidades
// ---------------------------------------------------------------------------
namespace {

// Decodificador UTF-8 → wchar_t (portable, sin API de Windows).
std::wstring AmpliarUtf8(const std::string& utf8) {
    std::wstring salida;
    salida.reserve(utf8.size());
    for (std::size_t i = 0; i < utf8.size();) {
        const unsigned char c = static_cast<unsigned char>(utf8[i]);
        std::uint32_t cp = 0;
        std::size_t extra = 0;
        if (c < 0x80)                { cp = c;        extra = 0; }
        else if ((c & 0xE0) == 0xC0) { cp = c & 0x1F; extra = 1; }
        else if ((c & 0xF0) == 0xE0) { cp = c & 0x0F; extra = 2; }
        else if ((c & 0xF8) == 0xF0) { cp = c & 0x07; extra = 3; }
        else { ++i; continue; }  // byte suelto: se descarta
        if (i + extra >= utf8.size()) break;  // secuencia truncada: se para
        bool valida = true;
        for (std::size_t k = 1; k <= extra; ++k) {
            const unsigned char cc = static_cast<unsigned char>(utf8[i + k]);
            if ((cc & 0xC0) != 0x80) { valida = false; break; }
            cp = (cp << 6) | (cc & 0x3F);
        }
        if (!valida) { ++i; continue; }
        // UTF-16 con pares sustitutos cuando cp > 0xFFFF (sizeof wchar_t==2)
        if (cp >= 0x10000 && sizeof(wchar_t) == 2) {
            cp -= 0x10000;
            salida.push_back(static_cast<wchar_t>(0xD800 + (cp >> 10)));
            salida.push_back(static_cast<wchar_t>(0xDC00 + (cp & 0x3FF)));
        } else if (cp > 0x10FFFF) {
            // fuera de rango: se descarta
        } else {
            salida.push_back(static_cast<wchar_t>(cp));
        }
        i += extra + 1;
    }
    return salida;
}

// Parser portable de "#RRGGBB". Acepta también 3 dígitos "#RGB".
bool ColorDesdeHex(const std::string& hex, Color* out) {
    if (hex.empty() || hex[0] != '#') return false;
    const std::string cuerpo = hex.substr(1);
    if (cuerpo.size() != 6 && cuerpo.size() != 3) return false;
    for (char c : cuerpo) {
        const bool digito = (c >= '0' && c <= '9');
        const bool letra  = (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
        if (!digito && !letra) return false;
    }
    auto canal = [&](std::size_t i) -> std::uint8_t {
        const std::string par = (cuerpo.size() == 6)
            ? cuerpo.substr(i * 2, 2)
            : cuerpo.substr(i, 1) + cuerpo.substr(i, 1);
        return static_cast<std::uint8_t>(std::strtol(par.c_str(), nullptr, 16));
    };
    out->r = canal(0);
    out->g = canal(1);
    out->b = canal(2);
    return true;
}

bool BoolDesdeTexto(const std::string& s, bool* out) {
    if (s == "true"  || s == "1") { *out = true;  return true; }
    if (s == "false" || s == "0") { *out = false; return true; }
    return false;
}

bool FloatDesdeTexto(const std::string& s, float* out) {
    if (s.empty()) return false;
    char* fin = nullptr;
    const double v = std::strtod(s.c_str(), &fin);
    if (fin == s.c_str() || *fin != '\0') return false;
    *out = static_cast<float>(v);
    return true;
}

// Aplica una propiedad individual al EstiloTexto. Devuelve false si el
// valor es inválido (el estilo conserva su valor anterior).
bool AplicarPropiedadTexto(const std::string& clave, const std::string& valor,
                           EstiloTexto* t) {
    if (clave == "texto.familia") {
        t->familia = AmpliarUtf8(valor);
        return true;
    }
    if (clave == "texto.tamano") {
        float v = 0;
        if (!FloatDesdeTexto(valor, &v) || v <= 0 || v > 2000) return false;
        t->tamano = v;
        return true;
    }
    if (clave == "texto.color") {
        Color c{};
        if (!ColorDesdeHex(valor, &c)) return false;
        t->color = c;
        return true;
    }
    if (clave == "texto.negrita") {
        bool v = false;
        if (!BoolDesdeTexto(valor, &v)) return false;
        t->negrita = v;
        return true;
    }
    if (clave == "texto.cursiva") {
        bool v = false;
        if (!BoolDesdeTexto(valor, &v)) return false;
        t->cursiva = v;
        return true;
    }
    if (clave == "texto.alineacion_centro") {
        bool v = false;
        if (!BoolDesdeTexto(valor, &v)) return false;
        t->alineacion_centro = v;
        return true;
    }
    return false;  // no es una propiedad de texto
}

// Aplica una propiedad individual al Fondo.
bool AplicarPropiedadFondo(const std::string& clave, const std::string& valor,
                           Fondo* f) {
    if (clave == "fondo.tipo") {
        if      (valor == "solido")    f->tipo = Fondo::Tipo::Solido;
        else if (valor == "gradiente") f->tipo = Fondo::Tipo::Gradiente;
        else if (valor == "imagen")    f->tipo = Fondo::Tipo::Imagen;
        else return false;
        return true;
    }
    if (clave == "fondo.color1") {
        Color c{};
        if (!ColorDesdeHex(valor, &c)) return false;
        f->color1 = c;
        return true;
    }
    if (clave == "fondo.color2") {
        Color c{};
        if (!ColorDesdeHex(valor, &c)) return false;
        f->color2 = c;
        return true;
    }
    if (clave == "fondo.ruta_imagen") {
        f->ruta_imagen = AmpliarUtf8(valor);
        return true;
    }
    if (clave == "fondo.ajuste") {
        if      (valor == "cubrir")   f->ajuste = AjusteImagen::Cubrir;
        else if (valor == "contener") f->ajuste = AjusteImagen::Contener;
        else if (valor == "estirar")  f->ajuste = AjusteImagen::Estirar;
        else return false;
        return true;
    }
    return false;  // no es una propiedad de fondo
}

} // namespace

// ---------------------------------------------------------------------------
// Resolución
// ---------------------------------------------------------------------------
const char* NombreNivel(NivelTema n) {
    switch (n) {
        case NivelTema::Raiz:     return "raiz";
        case NivelTema::Escenario: return "escenario";
        case NivelTema::Elemento: return "elemento";
        case NivelTema::Runtime:  return "runtime";
    }
    return "?";
}

const std::string* ResolucionTema::Buscar(const std::string& clave) const {
    auto it = estilo_resuelto.find(clave);
    return it == estilo_resuelto.end() ? nullptr : &it->second;
}

ResolucionTema HerenciaTemas::Resolver(const CapaTema* raiz,
                                       const CapaTema* escenario,
                                       const CapaTema* elemento,
                                       const CapaTema* runtime) {
    const std::pair<NivelTema, const CapaTema*> niveles[] = {
        { NivelTema::Raiz,     raiz },
        { NivelTema::Escenario, escenario },
        { NivelTema::Elemento, elemento },
        { NivelTema::Runtime,  runtime },
    };

    ResolucionTema r;
    for (const auto& [nivel, capa] : niveles) {
        if (!capa) continue;
        for (const auto& [clave, valor] : *capa) {
            auto it = r.estilo_resuelto.find(clave);
            if (it == r.estilo_resuelto.end()) {
                // Propiedad nueva (no venía de un nivel inferior).
                r.estilo_resuelto[clave] = valor;
                r.informe.push_back({ nivel, clave, "", valor });
            } else if (it->second != valor) {
                // Sobreescritura efectiva.
                const std::string anterior = it->second;
                it->second = valor;
                r.informe.push_back({ nivel, clave, anterior, valor });
            }
            // Mismo valor reafirmado: no entra al informe (decisión
            // documentada en docs/agent/themes.md).
        }
    }
    return r;
}

// ---------------------------------------------------------------------------
// Aplicación a los estilos del renderizador
// ---------------------------------------------------------------------------
bool AplicarAEstilos(const CapaTema& resuelto,
                     EstiloTexto* texto, Fondo* fondo) {
    if (!texto || !fondo) return false;
    bool todo_ok = true;
    for (const auto& [clave, valor] : resuelto) {
        const bool es_texto = clave.rfind("texto.", 0) == 0;
        const bool es_fondo = clave.rfind("fondo.", 0) == 0;
        if (!es_texto && !es_fondo) continue;  // clave ajena: se conserva
        const bool ok = es_texto
            ? AplicarPropiedadTexto(clave, valor, texto)
            : AplicarPropiedadFondo(clave, valor, fondo);
        if (!ok) todo_ok = false;  // valor inválido: se ignora esa clave
    }
    return todo_ok;
}

// ---------------------------------------------------------------------------
// Biblioteca de temas
// ---------------------------------------------------------------------------
bool BibliotecaTemas::Cargar(const std::string& ruta,
                             std::map<std::string, CapaTema>* out,
                             std::string* msg_error) {
    std::ifstream f(ruta, std::ios::binary);
    if (!f) {
        if (msg_error) *msg_error = "no se pudo abrir " + ruta;
        return false;
    }
    std::string contenido((std::istreambuf_iterator<char>(f)),
                          std::istreambuf_iterator<char>());
    return CargarFromString(contenido, out, msg_error);
}

bool BibliotecaTemas::CargarFromString(const std::string& json,
                                       std::map<std::string, CapaTema>* out,
                                       std::string* msg_error) {
    if (!out) return false;
    out->clear();
    nlohmann::json j = nlohmann::json::parse(json, nullptr, false);
    if (j.is_discarded()) {
        if (msg_error) *msg_error = "JSON invalido";
        return false;
    }
    if (!j.is_object()) {
        if (msg_error) *msg_error = "raiz no es un objeto";
        return false;
    }
    // Formato y versión: "temas" v1. Versión futura → error explícito
    // (misma política que ahp.v1).
    if (j.contains("version")) {
        if (!j["version"].is_number_integer()) {
            if (msg_error) *msg_error = "version no es entero";
            return false;
        }
        const int version = j["version"].get<int>();
        if (version > 1) {
            if (msg_error) {
                *msg_error = "version de temas futura no soportada: " +
                             std::to_string(version);
            }
            return false;
        }
    }
    if (!j.contains("temas") || !j["temas"].is_object()) {
        if (msg_error) *msg_error = "falta el objeto temas";
        return false;
    }
    for (auto it = j["temas"].begin(); it != j["temas"].end(); ++it) {
        if (!it.value().is_object()) {
            if (msg_error) {
                *msg_error = "el tema '" + it.key() + "' no es un objeto";
            }
            out->clear();
            return false;
        }
        CapaTema capa;
        for (auto p = it.value().begin(); p != it.value().end(); ++p) {
            if (!p.value().is_string()) {
                if (msg_error) {
                    *msg_error = "propiedad '" + p.key() + "' de '" +
                                 it.key() + "' no es string";
                }
                out->clear();
                return false;
            }
            capa[p.key()] = p.value().get<std::string>();
        }
        (*out)[it.key()] = std::move(capa);
    }
    return true;
}

} // namespace fusion
