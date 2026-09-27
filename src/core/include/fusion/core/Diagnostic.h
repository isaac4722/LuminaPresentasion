// src/core/include/fusion/core/Diagnostic.h
// Autotest — Sección 11.3 del doc técnico.
//
// Verifica: render, núcleo, permisos, BDs, IPC, log.
// 100% local: sin red, sin llamadas externas.

#pragma once

#include <string>
#include <vector>

namespace fusion {

struct ResultadoAutotest {
    std::string nombre;
    bool        ok;
    std::string detalle;
};

class Diagnostic {
public:
    // Ejecuta todos los autotests.
    static std::vector<ResultadoAutotest> EjecutarAutotest();

    // Lee las últimas N líneas del log estructurado (Sección 11.1).
    static std::vector<std::string> LogTail(int n = 50);

    // Verifica que el entorno es capaz de arrancar:
    //   - render Direct2D inicializable
    //   - núcleo vivo (responde a IPC ping)
    //   - permisos de escritura en runtime/
    //   - BD cancionero.fdb abrible
    //   - al menos 1 biblia instalada
    //   - log estructurado accesible
    static bool VerificarEntorno(std::vector<ResultadoAutotest>* out = nullptr);
};

} // namespace fusion
