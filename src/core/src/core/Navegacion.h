// src/core/src/core/Navegacion.h — Navegación portable de un Programa
//
// Algoritmo puro (sin Windows) para proyeccion.siguiente / .anterior:
// avanza línea a línea dentro del elemento, luego elemento a elemento del
// escenario, luego escenario a escenario del programa. Engine la aplica
// sobre su estado; los tests la ejercitan directamente con Programa de
// AhpFormat. Header-only para que el arnés local (gcc) la compile igual
// que MSVC.

#pragma once

#include "fusion/data/AhpFormat.h"

#include <string>

namespace fusion {

// Posición dentro del programa, tal como vive en EstadoMotor.
struct PosPrograma {
    std::string escenario_id, escenario_nombre;
    std::string elemento_id, elemento_titulo;
    int  linea = 0;
    bool valida = false;   // los ids resuelven en el programa actual
};

// Líneas navegables de un elemento: los de texto navegan por líneas;
// los demás (versículo, imagen, video, pptx, lower_third) son de un paso.
inline int LineasDeElemento(const Elemento& el) {
    if (el.tipo != TipoElemento::Texto) return 1;
    int n = static_cast<int>(el.lineas.size());
    return n > 0 ? n : 1;
}

namespace nav {

// Localiza (escenario, elemento) por id. Devuelve false si el programa no
// tiene escenarios con elementos.
inline bool Localizar(const Programa& p, const PosPrograma& pos,
                      int* ie_out, int* iel_out) {
    if (p.escenarios.empty()) return false;
    int ie = -1, iel = -1;
    if (!pos.valida) {
        // Sin posición válida: primer escenario con al menos un elemento.
        for (int i = 0; i < static_cast<int>(p.escenarios.size()); ++i) {
            if (!p.escenarios[i].elementos.empty()) { ie = i; iel = 0; break; }
        }
        if (ie < 0) { ie = 0; iel = -1; }   // solo escenarios vacíos
    } else {
        for (int i = 0; i < static_cast<int>(p.escenarios.size()); ++i) {
            if (p.escenarios[i].id == pos.escenario_id) { ie = i; break; }
        }
        if (ie < 0) {   // escenario desaparecido: el primero con elementos
            for (int i = 0; i < static_cast<int>(p.escenarios.size()); ++i) {
                if (!p.escenarios[i].elementos.empty()) { ie = i; iel = 0; break; }
            }
            if (ie < 0) return false;
        } else {
            const Escenario& e = p.escenarios[ie];
            for (int j = 0; j < static_cast<int>(e.elementos.size()); ++j) {
                if (e.elementos[j].id == pos.elemento_id) { iel = j; break; }
            }
            if (iel < 0) iel = e.elementos.empty() ? -1 : 0;
        }
    }
    *ie_out = ie; *iel_out = iel;
    return true;
}

// ¿La posición resuelve EXACTAMENTE en el programa (ids válidos)?
inline bool ResuelveExacto(const Programa& p, const PosPrograma& pos,
                           int* ie_out, int* iel_out) {
    if (!pos.valida) return false;
    int ie = -1;
    for (int i = 0; i < static_cast<int>(p.escenarios.size()); ++i) {
        if (p.escenarios[i].id == pos.escenario_id) { ie = i; break; }
    }
    if (ie < 0) return false;
    const Escenario& e = p.escenarios[ie];
    if (e.elementos.empty()) return false;
    int iel = -1;
    for (int j = 0; j < static_cast<int>(e.elementos.size()); ++j) {
        if (e.elementos[j].id == pos.elemento_id) { iel = j; break; }
    }
    if (iel < 0) return false;
    *ie_out = ie; *iel_out = iel;
    return true;
}

inline void Copiar(const Programa& p, int ie, int iel, int linea,
                   PosPrograma* pos) {
    const Escenario& e = p.escenarios[ie];
    pos->escenario_id     = e.id;
    pos->escenario_nombre = e.nombre;
    pos->valida = true;
    pos->linea = linea;
    if (iel >= 0 && iel < static_cast<int>(e.elementos.size())) {
        pos->elemento_id     = e.elementos[iel].id;
        pos->elemento_titulo = e.elementos[iel].titulo;
    } else {
        pos->elemento_id.clear();
        pos->elemento_titulo.clear();
    }
}

} // namespace nav

// Avanza una posición. Devuelve true si se movió; false si ya estaba al
// final del programa. Si la posición no resuelve (vacía o ids de otra
// versión del programa), se RECOLOCA al primer elemento y devuelve true.
inline bool AvanzarPos(const Programa& p, PosPrograma* pos) {
    int ie = -1, iel = -1;
    if (!nav::ResuelveExacto(p, *pos, &ie, &iel)) {
        if (!nav::Localizar(p, *pos, &ie, &iel) || iel < 0) return false;
        nav::Copiar(p, ie, iel, 0, pos);
        return true;
    }
    const Escenario& e = p.escenarios[ie];

    const int lineas = LineasDeElemento(e.elementos[iel]);
    if (pos->linea + 1 < lineas) {
        nav::Copiar(p, ie, iel, pos->linea + 1, pos);
        return true;
    }
    // Siguiente elemento del escenario.
    if (iel + 1 < static_cast<int>(e.elementos.size())) {
        nav::Copiar(p, ie, iel + 1, 0, pos);
        return true;
    }
    // Siguiente escenario con elementos.
    for (int i = ie + 1; i < static_cast<int>(p.escenarios.size()); ++i) {
        if (!p.escenarios[i].elementos.empty()) {
            nav::Copiar(p, i, 0, 0, pos);
            return true;
        }
    }
    return false;   // fin del programa
}

// Retrocede una posición. Devuelve true si se movió; false si ya estaba
// al principio. Si la posición no resuelve, se RECOLOCA al primer
// elemento sin moverse hacia atrás (false).
inline bool RetrocederPos(const Programa& p, PosPrograma* pos) {
    int ie = -1, iel = -1;
    if (!nav::ResuelveExacto(p, *pos, &ie, &iel)) {
        if (!nav::Localizar(p, *pos, &ie, &iel) || iel < 0) return false;
        nav::Copiar(p, ie, iel, 0, pos);
        return false;
    }
    const Escenario& e = p.escenarios[ie];

    if (pos->linea > 0) {
        nav::Copiar(p, ie, iel, pos->linea - 1, pos);
        return true;
    }
    // Elemento anterior del escenario.
    if (iel - 1 >= 0) {
        const int lineas = LineasDeElemento(e.elementos[iel - 1]);
        nav::Copiar(p, ie, iel - 1, lineas - 1, pos);
        return true;
    }
    // Escenario anterior con elementos: su último elemento, última línea.
    for (int i = ie - 1; i >= 0; --i) {
        if (!p.escenarios[i].elementos.empty()) {
            int ult = static_cast<int>(p.escenarios[i].elementos.size()) - 1;
            int lineas = LineasDeElemento(p.escenarios[i].elementos[ult]);
            nav::Copiar(p, i, ult, lineas - 1, pos);
            return true;
        }
    }
    return false;   // principio del programa
}

} // namespace fusion
