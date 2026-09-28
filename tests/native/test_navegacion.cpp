// tests/native/test_navegacion.cpp — Tests de Navegacion.h: navegación
// real de un programa (proyeccion.siguiente / .anterior del motor).

#include "doctest.h"
#include "Navegacion.h"

#include <string>

using namespace fusion;

namespace {

Elemento Texto(const std::string& id, int lineas) {
    Elemento e;
    e.id = id;
    e.tipo = TipoElemento::Texto;
    e.titulo = "T-" + id;
    for (int i = 0; i < lineas; ++i)
        e.lineas.push_back(LineaTexto{"línea " + std::to_string(i + 1), ""});
    return e;
}

Elemento Uno(const std::string& id, TipoElemento tipo) {
    Elemento e;
    e.id = id;
    e.tipo = tipo;
    e.titulo = "T-" + id;
    return e;
}

Escenario Esc(const std::string& id, std::vector<Elemento> els) {
    Escenario e;
    e.id = id;
    e.nombre = "N-" + id;
    e.elementos = std::move(els);
    return e;
}

Programa TresEscenarios() {
    Programa p;
    p.titulo = "Culto";
    // esc-1: texto de 3 líneas + imagen (1 paso) + texto de 2 líneas
    p.escenarios.push_back(
        Esc("esc-1", {Texto("a1", 3), Uno("a2", TipoElemento::Imagen),
                      Texto("a3", 2)}));
    // esc-2: solo un versículo (1 paso)
    p.escenarios.push_back(
        Esc("esc-2", {Uno("b1", TipoElemento::Versiculo)}));
    // esc-3: sin elementos (se salta en la navegación)
    p.escenarios.push_back(Esc("esc-3", {}));
    // esc-4: un elemento de texto de 1 línea
    p.escenarios.push_back(Esc("esc-4", {Texto("c1", 1)}));
    return p;
}

} // namespace

TEST_CASE("AvanzarPos: recorre líneas, elementos y escenarios en orden") {
    Programa p = TresEscenarios();
    PosPrograma pos;   // sin posición: empieza en el primer elemento

    CHECK(AvanzarPos(p, &pos));
    CHECK(pos.escenario_id == "esc-1");
    CHECK(pos.elemento_id == "a1");
    CHECK(pos.linea == 0);

    CHECK(AvanzarPos(p, &pos)); CHECK(pos.elemento_id == "a1"); CHECK(pos.linea == 1);
    CHECK(AvanzarPos(p, &pos)); CHECK(pos.elemento_id == "a1"); CHECK(pos.linea == 2);
    // a1 tenía 3 líneas; la 4.ª ya no existe → pasa a a2 (imagen, 1 paso)
    CHECK(AvanzarPos(p, &pos));
    CHECK(pos.elemento_id == "a2");
    CHECK(pos.linea == 0);
    // a2 → a3 (2 líneas)
    CHECK(AvanzarPos(p, &pos));
    CHECK(pos.elemento_id == "a3");
    CHECK(pos.linea == 0);
    CHECK(AvanzarPos(p, &pos));
    CHECK(pos.elemento_id == "a3");
    CHECK(pos.linea == 1);
    // esc-2 (versículo, 1 paso)
    CHECK(AvanzarPos(p, &pos));
    CHECK(pos.escenario_id == "esc-2");
    CHECK(pos.elemento_id == "b1");
    // esc-3 está vacío: se salta
    CHECK(AvanzarPos(p, &pos));
    CHECK(pos.escenario_id == "esc-4");
    CHECK(pos.elemento_id == "c1");
    // fin del programa: ya no se mueve
    CHECK_FALSE(AvanzarPos(p, &pos));
    CHECK(pos.elemento_id == "c1");
}

TEST_CASE("RetrocederPos: recorre en reversa exacta") {
    Programa p = TresEscenarios();
    PosPrograma pos;
    // ir al final
    while (AvanzarPos(p, &pos)) {}
    CHECK(pos.elemento_id == "c1");
    // reversa completa hasta el principio
    CHECK(RetrocederPos(p, &pos));
    CHECK(pos.escenario_id == "esc-2");
    CHECK(pos.elemento_id == "b1");
    CHECK(RetrocederPos(p, &pos));
    CHECK(pos.escenario_id == "esc-1");
    CHECK(pos.elemento_id == "a3");
    CHECK(pos.linea == 1);
    CHECK(RetrocederPos(p, &pos)); CHECK(pos.elemento_id == "a3"); CHECK(pos.linea == 0);
    CHECK(RetrocederPos(p, &pos)); CHECK(pos.elemento_id == "a2");
    CHECK(RetrocederPos(p, &pos)); CHECK(pos.elemento_id == "a1"); CHECK(pos.linea == 2);
    CHECK(RetrocederPos(p, &pos)); CHECK(pos.linea == 1);
    CHECK(RetrocederPos(p, &pos)); CHECK(pos.linea == 0);
    CHECK_FALSE(RetrocederPos(p, &pos));   // principio: no se mueve
    CHECK(pos.elemento_id == "a1");
    CHECK(pos.linea == 0);
}

TEST_CASE("Posición con ids inexistentes: se recoloca al principio") {
    Programa p = TresEscenarios();
    PosPrograma pos;
    pos.escenario_id = "fantasma";
    pos.elemento_id = "otro";
    pos.valida = true;
    CHECK(AvanzarPos(p, &pos));
    CHECK(pos.escenario_id == "esc-1");
    CHECK(pos.elemento_id == "a1");
    CHECK(pos.linea == 0);
}

TEST_CASE("Programa vacío o sin elementos: no se mueve") {
    Programa vacio;
    PosPrograma pos;
    CHECK_FALSE(AvanzarPos(vacio, &pos));
    CHECK_FALSE(RetrocederPos(vacio, &pos));

    Programa solo_vacios;
    solo_vacios.escenarios.push_back(Esc("x", {}));
    CHECK_FALSE(AvanzarPos(solo_vacios, &pos));
}

TEST_CASE("Elemento de texto sin líneas: cuenta como un paso") {
    Programa p;
    p.escenarios.push_back(Esc("e", {Texto("t0", 0), Texto("t1", 1)}));
    PosPrograma pos;
    CHECK(AvanzarPos(p, &pos));   // materializa t0
    CHECK(pos.elemento_id == "t0");
    CHECK(AvanzarPos(p, &pos));   // t0 → t1
    CHECK(pos.elemento_id == "t1");
    CHECK_FALSE(AvanzarPos(p, &pos));   // fin del programa
}

TEST_CASE("LineasDeElemento: texto por líneas, el resto un paso") {
    CHECK(LineasDeElemento(Texto("t", 5)) == 5);
    CHECK(LineasDeElemento(Texto("t", 0)) == 1);
    CHECK(LineasDeElemento(Uno("v", TipoElemento::Video)) == 1);
    CHECK(LineasDeElemento(Uno("p", TipoElemento::Pptx)) == 1);
}
