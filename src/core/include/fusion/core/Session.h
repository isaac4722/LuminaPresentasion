// src/core/include/fusion/core/Session.h
// Sesión persistente. Sobrevive a cierres de la carcasa; al reabrirla,
// el motor y la carcasa se resincronizan.

#pragma once

#include <string>
#include <vector>

namespace fusion {

struct Sesion {
    // Estado de proyección
    std::string programa_ruta;
    std::string escenario_id;
    std::string elemento_id;
    int         linea_actual   = 0;
    bool        salida_visible = false;
    bool        negro           = false;
    bool        logo            = false;

    // Monitor seleccionado
    std::string monitor_dispositivo;

    // Tema aplicado en caliente (vacío = tema raíz del programa)
    std::string tema_runtime;

    // Recientes (rutas de .ahp abiertos antes)
    struct Reciente {
        std::string ruta;
        int          veces_usado;
    };
    std::vector<Reciente> recientes;

    // Persistencia
    static bool Cargar(const std::string& ruta, Sesion* out);
    static bool Guardar(const std::string& ruta, const Sesion& s);

    // Ruta por defecto: runtime/session.json junto al ejecutable.
    static std::string RutaPorDefecto();
};

} // namespace fusion
