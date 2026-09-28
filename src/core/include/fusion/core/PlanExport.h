// src/core/include/fusion/core/PlanExport.h
// Planificador de exportaciones (Sección 9.3 del doc técnico):
// PPTX, PDF e imágenes 1080p.
//
// Reglas del producto:
//  - La exportación la dispara SIEMPRE el operador de forma explícita;
//    no hay exportación automática ni en segundo plano.
//  - Este plan es la pieza portable que decide QUÉ se exportaría y con
//    qué nombre; la ejecución real (PPTX con DocumentFormat.OpenXml,
//    PDF con PdfSharp en la capa net48, PNG 1080p con Direct2D) vive
//    en el shell Windows y recibe este plan.

#pragma once

#include "fusion/data/AhpFormat.h"

#include <string>
#include <vector>

namespace fusion {

enum class FormatoExportacion {
    Pptx,           // paquete .pptx (una diapositiva por unidad)
    Pdf,            // documento .pdf (una página por unidad)
    Imagenes1080p,  // PNG 1920x1080 (un archivo por unidad)
};

struct PlanExportResultado {
    bool        ok = false;
    FormatoExportacion formato = FormatoExportacion::Pptx;
    int         unidades = 0;        // diapositivas/páginas/archivos
    std::string nombre_base;         // sin extensión, listo para el shell
    std::vector<std::string> avisos; // informativos, no bloquean
    std::string msg_error;           // vacío si ok
};

// Una unidad exportable concreta (diapositiva/página/archivo 1:1).
// Es la enumeración exacta de lo que ContarUnidades cuenta, para que
// planificar y ejecutar jamás discrepen.
struct UnidadExport {
    enum class Tipo { Texto, Medio, Pptx };

    Tipo tipo = Tipo::Texto;
    std::string titulo;              // título del elemento (o vacío)
    std::vector<std::string> lineas; // Texto: la línea proyectable
                                     // (los medios/pptx no llevan líneas)
    std::string ruta;                // Medio/Pptx: archivo de origen
    std::string escenario;           // nombre del escenario de origen
    int indice_escenario = 0;        // 1-based
    int indice_elemento = 0;         // 1-based dentro del escenario
};

class PlanExport {
public:
    // Unidades proyectables de un elemento: los elementos de texto
    // (texto y versículo) proyectan una diapositiva por línea (así
    // navega el operador con B/C/L y Espacio); los medios y pptx son
    // una unidad por elemento.
    static int ContarUnidades(const Programa& p);

    // Enumera las unidades con su contenido. El número de unidades
    // devueltas SIEMPRE coincide con ContarUnidades.
    static std::vector<UnidadExport> EnumerarUnidades(const Programa& p);

    // Planifica la exportación de un programa completo en el formato
    // dado. Devuelve false y msg_error si no hay nada exportable.
    static PlanExportResultado Planificar(const Programa& p,
                                          FormatoExportacion formato);
};

} // namespace fusion
