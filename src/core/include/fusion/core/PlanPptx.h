// src/core/include/fusion/core/PlanPptx.h
// Planificador de proyección PPTX por archivo original (Sección 9.2 del
// doc técnico, elemento pptx de ahp.v1).
//
// Reglas del producto que aquí se aplican y se hacen verificables:
//  - Solo archivos .pptx (ISO/IEC-29500) y .pptm. PPT legacy (.ppt) no.
//  - Un .pptm se proyecta SIN ejecutar macros, siempre (ejecutar_macros
//    es false por construcción; el shell no puede activarlo).
//  - El modo lo decide el operador en el .ahp: "com" (con PowerPoint)
//    o "directo" (sin PowerPoint, render propio). Si el modo es "com"
//    y PowerPoint no está disponible, se devuelve error explícito: no
//    hay retroceso silencioso a "directo" (decisión del operador).
//  - El plan es portable; la ejecución (COM de PowerPoint o el render
//    directo) vive en el shell Windows.

#pragma once

#include "fusion/data/AhpFormat.h"

#include <string>
#include <vector>

namespace fusion {

struct PlanPptxResultado {
    bool        ok              = false;
    ModoPptx    modo_resuelto   = ModoPptx::Com;
    bool        ejecutar_macros = false;   // siempre false (regla .pptm)
    std::string ruta_resuelta;
    std::vector<std::string> avisos;      // informativos, no bloquean
    std::string msg_error;                // vacío si ok
};

class PlanPptx {
public:
    // Planifica la proyección del elemento pptx dado el estado del
    // equipo (¿hay PowerPoint disponible?). No toca disco salvo para
    // comprobar la extensión de la ruta; la existencia del archivo se
    // verifica en el shell al momento de abrir (puede estar en una
    // unidad extraíble).
    static PlanPptxResultado Planificar(const Elemento& e,
                                        bool powerpoint_disponible);
};

} // namespace fusion
