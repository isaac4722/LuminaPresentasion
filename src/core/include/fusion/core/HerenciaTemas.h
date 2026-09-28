// src/core/include/fusion/core/HerenciaTemas.h
// Herencia de temas con informe de fidelidad.
//
// Niveles, de menor a mayor prioridad (doc técnico 5.4 + extensión):
//   1. Tema raíz del programa     (meta.tema_raiz)
//   2. Plantilla del escenario    (escenario.tema)
//   3. Capa del Escenario         (escenario.fondo + escenario.tema_escenario)
//   4. Tema override del elemento (elemento.tema_override)
//   5. Tema runtime aplicado en caliente (Sesion.tema_runtime)
//
// El nivel 3 (Escenario) es la cascada del doc 5.4: lo que el operador
// fija para TODO el escenario por encima de su plantilla. El campo
// `fondo` (color sólido, destino del "fondo del diseño" al importar
// PPTX, doc 9.2.6) se pliega en esta capa vía CapaEscenario(); las
// claves del bolso inline `tema_escenario` ganan sobre el plegado.
// El nivel 5 (runtime) es extensión propia, documentada como desviación
// en docs/agent/themes.md.
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
    Raiz      = 0,
    Plantilla = 1,   // escenario.tema (plantilla de escenario, doc 5.4)
    Escenario = 2,   // capa inline del escenario (fondo + tema_escenario)
    Elemento  = 3,
    Runtime   = 4,
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
    // Resuelve los 5 niveles en orden de prioridad creciente
    // (doc 5.4: Tema → Plantilla → Escenario → Elemento, + runtime).
    // Una capa nula se ignora (el nivel no participa). Devuelve el estilo
    // resuelto y el informe de fidelidad correspondiente.
    static ResolucionTema Resolver(const CapaTema* raiz,
                                   const CapaTema* plantilla,
                                   const CapaTema* escenario,
                                   const CapaTema* elemento,
                                   const CapaTema* runtime);
};

// Construye la capa del nivel Escenario (doc 5.4) a partir de los campos
// del escenario en ahp.v1:
//   - `fondo_hex` (escenario.fondo, "#RGB"/"#RRGGBB" o vacío) se pliega
//     como { "fondo.tipo": "solido", "fondo.color1": fondo_hex }.
//   - `tema_escenario_inline` (escenario.tema_escenario, bolso de claves
//     planas) se aplica ENCIMA del plegado: si define una clave que el
//     fondo ya aportó, gana la del bolso inline (lo explícito manda).
// Un color vacío y un bolso vacío devuelven una capa vacía (el nivel no
// participa en la resolución). No valida el formato del color: eso lo
// hace AhpFormat::Validar; aquí un valor inválido simplemente queda en
// la capa y AplicarAEstilos lo ignorará conservando el anterior.
CapaTema CapaEscenario(const std::string& fondo_hex,
                       const CapaTema& tema_escenario_inline);

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
