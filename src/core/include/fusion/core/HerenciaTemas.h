// src/core/include/fusion/core/HerenciaTemas.h
// Herencia de temas en 4 niveles con informe de fidelidad.
//
// Niveles, de menor a mayor prioridad (docs/agent/format_ahp_v1.md):
//   1. Tema raíz del programa     (meta.tema_raiz)
//   2. Plantilla del escenario    (escenario.tema)
//   3. Tema override del elemento (elemento.tema_override)
//   4. Tema runtime aplicado en caliente (Sesion.tema_runtime)
//
// Cada nivel puede sobreescribir selectivamente propiedades del anterior.
// El informe de fidelidad enumera qué propiedades fueron sobreescritas en
// cada nivel, para que el operador entienda qué se le aplicó al elemento.
//
// Esquema de un tema: docs/agent/themes.md.

#pragma once

#include "fusion/core/Renderer.h"

#include <map>
#include <string>
#include <vector>

namespace fusion {

// Un tema (o capa de tema) es un bolso de propiedades planas.
// Claves reconocidas por el renderizador:
//   texto.familia           (string utf8, p.ej. "Outfit")
//   texto.tamano            (número en puntos, p.ej. "60")
//   texto.color             ("#RRGGBB")
//   texto.negrita           ("true"/"false")
//   texto.cursiva           ("true"/"false")
//   texto.alineacion_centro ("true"/"false")
//   fondo.tipo              ("solido"/"gradiente"/"imagen")
//   fondo.color1            ("#RRGGBB")
//   fondo.color2            ("#RRGGBB")
//   fondo.ruta_imagen       (string utf8)
//   fondo.ajuste            ("cubrir"/"contener"/"estirar")
// Las claves desconocidas se conservan (tolerancia hacia adelante) y
// también entran al informe de fidelidad si cambian.
using CapaTema = std::map<std::string, std::string>;

// Niveles de la herencia, de menor a mayor prioridad.
enum class NivelTema {
    Raiz     = 0,
    Escenario = 1,
    Elemento = 2,
    Runtime  = 3,
};

// Nombre legible del nivel (para el informe y los logs).
const char* NombreNivel(NivelTema n);

// Una entrada del informe de fidelidad: qué nivel cambió qué propiedad,
// con el valor que había antes y el que quedó después.
struct RegistroFidelidad {
    NivelTema   nivel;
    std::string propiedad;
    std::string valor_anterior;   // "" si la propiedad no existía antes
    std::string valor_nuevo;
};

// Resultado de la resolución de la herencia.
struct ResolucionTema {
    // Propiedades finales (solo las que algún nivel definió).
    CapaTema estilo_resuelto;

    // Solo cambios efectivos: reafirmar el mismo valor no entra al
    // informe (decisión documentada en themes.md).
    std::vector<RegistroFidelidad> informe;

    const std::string* Buscar(const std::string& clave) const;
};

class HerenciaTemas {
public:
    // Resuelve los 4 niveles en orden de prioridad creciente.
    // Una capa nula se ignora (el nivel no participa). Devuelve el estilo
    // resuelto y el informe de fidelidad correspondiente.
    static ResolucionTema Resolver(const CapaTema* raiz,
                                   const CapaTema* escenario,
                                   const CapaTema* elemento,
                                   const CapaTema* runtime);
};

// Aplica un bolso de propiedades resueltas a los estilos del renderizador
// (EstiloTexto + Fondo de Renderer.h). Las claves desconocidas se
// conservan sin efecto; los valores inválidos se ignoran y conservan el
// valor anterior del estilo. Devuelve false si alguna clave de texto/fondo
// tenía un valor inválido (el estilo queda aplicado parcialmente).
bool AplicarAEstilos(const CapaTema& resuelto,
                     EstiloTexto* texto, Fondo* fondo);

// Biblioteca de temas nombrados, cargada desde JSON:
//   { "formato": "temas", "version": 1,
//     "temas": { "Nombre": { "texto.color": "#FFFFFF", ... }, ... } }
// El archivo vive junto al ejecutable (runtime/temas.json); nunca se
// modifica por el programa salvo edición explícita del operador.
class BibliotecaTemas {
public:
    // Carga la biblioteca. Devuelve false y msg_error si el JSON es
    // inválido (versión futura, clave no-objeto, etc.).
    static bool Cargar(const std::string& ruta,
                       std::map<std::string, CapaTema>* out,
                       std::string* msg_error);

    // Variante desde string (para tests).
    static bool CargarFromString(const std::string& json,
                                 std::map<std::string, CapaTema>* out,
                                 std::string* msg_error);
};

} // namespace fusion
