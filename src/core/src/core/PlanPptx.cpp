// src/core/src/core/PlanPptx.cpp — Planificador PPTX por archivo original.
// Portable: no toca COM ni API de Windows (verificable en los tests nativos).

#include "fusion/core/PlanPptx.h"

#include <algorithm>
#include <cctype>

namespace fusion {

namespace {

// Extensión en minúsculas, con punto ("" si la ruta no tiene).
std::string ExtensionEnMinusculas(const std::string& ruta) {
    const auto pos = ruta.find_last_of('.');
    if (pos == std::string::npos || pos == 0 || pos + 1 == ruta.size())
        return "";
    std::string ext = ruta.substr(pos);
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) {
                       return static_cast<char>(std::tolower(c));
                   });
    return ext;
}

} // namespace

PlanPptxResultado PlanPptx::Planificar(const Elemento& e,
                                       bool powerpoint_disponible) {
    PlanPptxResultado r;

    if (e.tipo != TipoElemento::Pptx) {
        r.msg_error = "el elemento no es de tipo pptx";
        return r;
    }
    if (e.ruta.empty()) {
        r.msg_error = "el elemento pptx no tiene ruta";
        return r;
    }

    const std::string ext = ExtensionEnMinusculas(e.ruta);
    if (ext != ".pptx" && ext != ".pptm") {
        r.msg_error = "formato no soportado para proyección por archivo "
                      "original: '" + ext + "' (solo .pptx y .pptm)";
        return r;
    }

    if (e.modo_pptx == ModoPptx::Desconocido) {
        r.msg_error = "modo pptx inválido (use \"com\" o \"directo\")";
        return r;
    }

    r.ruta_resuelta = e.ruta;
    r.modo_resuelto = e.modo_pptx;

    // Regla del producto: las macros NO se ejecutan nunca. En un .pptm
    // se informa al operador para que sepa qué esperar.
    r.ejecutar_macros = false;
    if (ext == ".pptm") {
        r.avisos.push_back(
            "el archivo tiene macros (.pptm): se proyecta sin ejecutarlas");
    }

    if (e.modo_pptx == ModoPptx::Com && !powerpoint_disponible) {
        r.msg_error = "el modo \"com\" requiere PowerPoint y no está "
                      "disponible en este equipo; el operador debe elegir "
                      "el modo \"directo\"";
        return r;
    }

    if (e.modo_pptx == ModoPptx::Directo) {
        r.avisos.push_back(
            "render propio sin PowerPoint (cada diapositiva se rasteriza "
            "por el núcleo)");
    }

    r.ok = true;
    return r;
}

} // namespace fusion
