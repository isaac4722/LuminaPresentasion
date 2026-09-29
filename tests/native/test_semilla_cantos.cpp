// tests/native/test_semilla_cantos.cpp — La app abre con contenido REAL
//
// El cancionero nacía vacío: la Biblioteca mostraba una lista vacía y
// "lo que creaste no sirve". La semilla debe ser válida, insertarse en
// una BD limpia y no duplicar al reinsertar.

#include "doctest.h"
#include "fusion/data/SemillaCantos.h"

#include <algorithm>
#include <set>

using namespace fusion;

namespace {

bool TieneContenido(const CantoDetalle& c) {
    if (c.titulo.empty()) return false;
    if (c.secciones.empty()) return false;
    for (const auto& s : c.secciones) {
        if (s.lineas.empty()) return false;
        for (const auto& l : s.lineas)
            if (l.empty()) return false;
    }
    return true;
}

} // namespace

TEST_CASE("SemillaCantos::Generar trae himnos válidos y sin títulos repetidos") {
    auto semilla = semillacantos::Generar();
    REQUIRE_FALSE(semilla.empty());

    std::set<std::string> titulos;
    for (const auto& c : semilla) {
        INFO("canto inválido: ", c.titulo);
        CHECK(TieneContenido(c));
        CHECK(titulos.insert(c.titulo).second);   // sin duplicados
        CHECK(c.bpm > 0);
    }
    // Al menos uno con coro además de versos (estructura himnística real).
    bool hay_coro = false;
    for (const auto& c : semilla)
        for (const auto& s : c.secciones)
            if (s.tipo == "coro") hay_coro = true;
    CHECK(hay_coro);
}

TEST_CASE("SemillaCantos: los cantos insertan en una BD limpia con secciones y líneas") {
    auto semilla = semillacantos::Generar();
    REQUIRE_FALSE(semilla.empty());

    SongDatabase db;
    REQUIRE(db.Abrir(":memory:"));
    for (const auto& c : semilla)
        CHECK(db.InsertarCanto(c, "semilla") > 0);

    auto todos = db.ListarTodos();
    CHECK(todos.size() == semilla.size());

    // Detalle: primer canto trae sus secciones con líneas.
    CantoDetalle detalle;
    REQUIRE(db.Obtener(todos.front().id, &detalle));
    CHECK(detalle.titulo == todos.front().titulo);
    CHECK_FALSE(detalle.secciones.empty());
    CHECK_FALSE(detalle.secciones.front().lineas.empty());
}

TEST_CASE("SemillaCantos: reinsertar no duplica (idempotente por título+autor)") {
    auto semilla = semillacantos::Generar();
    REQUIRE_FALSE(semilla.empty());

    SongDatabase db;
    REQUIRE(db.Abrir(":memory:"));
    for (const auto& c : semilla) (void)db.InsertarCanto(c, "semilla");
    for (const auto& c : semilla) (void)db.InsertarCanto(c, "semilla");

    auto todos = db.ListarTodos();
    CHECK(todos.size() == semilla.size());
}
