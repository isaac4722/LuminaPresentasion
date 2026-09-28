// src/core/src/core/PlanExport.cpp — Planificador de exportaciones.
// Portable: calcula unidades y nombre base; no escribe archivos.

#include "fusion/core/PlanExport.h"

#include <algorithm>
#include <cctype>
#include <cstdio>

namespace fusion {

namespace {

// ¿El elemento proyecta una diapositiva por línea?
bool EsTextoProyectable(TipoElemento t) {
    return t == TipoElemento::Texto || t == TipoElemento::Versiculo;
}

// Texto de respaldo para un elemento de texto sin líneas: el versículo
// en sí (o su cita), o el título del texto.
std::string RespaldoTexto(const Elemento& el) {
    if (el.tipo == TipoElemento::Versiculo)
        return !el.texto_versiculo.empty() ? el.texto_versiculo : el.cita;
    return el.titulo;
}

UnidadExport UnidadBase(const Escenario& e, const Elemento& el,
                        int idx_esc, int idx_el) {
    UnidadExport u;
    u.titulo = el.titulo;
    u.ruta = el.ruta;
    u.escenario = e.nombre;
    u.indice_escenario = idx_esc;
    u.indice_elemento = idx_el;
    if (!EsTextoProyectable(el.tipo)) {
        u.tipo = el.tipo == TipoElemento::Pptx ? UnidadExport::Tipo::Pptx
                                               : UnidadExport::Tipo::Medio;
    }
    return u;
}

// Nombre de archivo seguro: alfanuméricos, espacios, '-', '_' y '.';
// el resto se sustituye por '_' (nada de rutas ni caracteres raros).
std::string NombreBaseDesde(const Programa& p) {
    std::string base = p.titulo.empty() ? std::string("programa") : p.titulo;
    std::string out;
    out.reserve(base.size());
    for (unsigned char c : base) {
        if (std::isalnum(c) || c == ' ' || c == '-' || c == '_' || c == '.') {
            out.push_back(static_cast<char>(c));
        } else {
            out.push_back('_');
        }
    }
    // Sin puntos al final ni doble espacio inicial
    while (!out.empty() && out.back() == '.') out.pop_back();
    if (out.empty()) out = "programa";
    // Fecha del programa como sufijo (si viene)
    if (!p.fecha.empty()) {
        std::string fecha_ok;
        for (unsigned char c : p.fecha) {
            if (std::isdigit(c) || c == '-') fecha_ok.push_back(static_cast<char>(c));
        }
        if (!fecha_ok.empty()) out += " " + fecha_ok;
    }
    return out;
}

} // namespace

std::vector<UnidadExport> PlanExport::EnumerarUnidades(const Programa& p) {
    std::vector<UnidadExport> out;
    int idx_esc = 0;
    for (const auto& e : p.escenarios) {
        ++idx_esc;
        int idx_el = 0;
        for (const auto& el : e.elementos) {
            ++idx_el;
            if (EsTextoProyectable(el.tipo)) {
                if (el.lineas.empty()) {
                    UnidadExport u = UnidadBase(e, el, idx_esc, idx_el);
                    u.lineas.push_back(RespaldoTexto(el));
                    out.push_back(std::move(u));
                } else {
                    for (const auto& l : el.lineas) {
                        UnidadExport u = UnidadBase(e, el, idx_esc, idx_el);
                        u.lineas.push_back(l.texto);
                        out.push_back(std::move(u));
                    }
                }
            } else {
                out.push_back(UnidadBase(e, el, idx_esc, idx_el));
            }
        }
    }
    return out;
}

int PlanExport::ContarUnidades(const Programa& p) {
    // Fuente única: el conteo es el tamaño de la enumeración, así el
    // plan y la ejecución jamás discrepen.
    return static_cast<int>(EnumerarUnidades(p).size());
}

PlanExportResultado PlanExport::Planificar(const Programa& p,
                                           FormatoExportacion formato) {
    PlanExportResultado r;
    r.formato = formato;

    if (p.escenarios.empty()) {
        r.msg_error = "el programa no tiene escenarios que exportar";
        return r;
    }

    r.unidades = ContarUnidades(p);
    if (r.unidades <= 0) {
        r.msg_error = "el programa no tiene elementos que exportar";
        return r;
    }

    r.nombre_base = NombreBaseDesde(p);

    switch (formato) {
        case FormatoExportacion::Pptx:
            r.avisos.push_back(
                "se genera un paquete .pptx con una diapositiva por unidad");
            break;
        case FormatoExportacion::Pdf:
            r.avisos.push_back(
                "se genera un .pdf con una página por unidad; los vídeos "
                "no se exportan");
            break;
        case FormatoExportacion::Imagenes1080p:
            r.avisos.push_back(
                "se genera un PNG 1920x1080 por unidad");
            break;
    }

    r.ok = true;
    return r;
}

} // namespace fusion
