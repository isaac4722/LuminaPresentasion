// tests/native/test_citas.cpp — Tests del parser de citas bíblicas
// y de la API de inserción de versículos.

#include "doctest.h"
#include "fusion/data/BibleDatabase.h"

#include <string>
#include <vector>

using namespace fusion;

namespace {

// Siembra versículos mínimos para ejercitar el parser: usa la API pública
// InsertarVersiculo (la misma que usarán los importadores reales).
void Sembrar(BibleDatabase& db) {
    REQUIRE(db.InsertarVersiculo("Salmos", 100, 4,
                                 "Entrad por sus puertas con acción de gracias."));
    REQUIRE(db.InsertarVersiculo("Juan", 3, 16,
                                 "Porque de tal manera amó Dios al mundo..."));
    REQUIRE(db.InsertarVersiculo("Juan", 3, 17, "Porque no envió Dios a su Hijo..."));
    REQUIRE(db.InsertarVersiculo("Juan", 3, 18, "El que en él cree, no es condenado."));
    REQUIRE(db.InsertarVersiculo("1 Corintios", 13, 4, "El amor es sufrido, es benigno..."));
    REQUIRE(db.InsertarVersiculo("1 Corintios", 13, 5, "...no busca lo suyo."));
    REQUIRE(db.InsertarVersiculo("Génesis", 1, 1, "En el principio creó Dios los cielos."));
    REQUIRE(db.InsertarVersiculo("Apocalipsis", 22, 20, "Ciertamente vengo en breve."));
}

} // namespace

TEST_CASE("Cita simple con nombre completo y acentos") {
    BibleDatabase db;
    CHECK(db.Abrir(":memory:"));
    Sembrar(db);

    std::vector<Versiculo> v;
    REQUIRE(db.ObtenerCita("Salmo 100:4", &v));
    REQUIRE(v.size() == 1);
    CHECK(v[0].versiculo == 4);
    CHECK(v[0].libro == "Salmos");
    CHECK(v[0].texto.find("acción de gracias") != std::string::npos);
}

TEST_CASE("Alias del libro: Salmos/Salmo/ps/Psalm resuelven igual") {
    BibleDatabase db;
    CHECK(db.Abrir(":memory:"));
    Sembrar(db);

    std::vector<Versiculo> v;
    CHECK(db.ObtenerCita("Salmos 100:4", &v));
    CHECK(v.size() == 1);
    v.clear();
    CHECK(db.ObtenerCita("salmo 100:4", &v));
    CHECK(v.size() == 1);
    v.clear();
    CHECK(db.ObtenerCita("Ps 100:4", &v));
    CHECK(v.size() == 1);
}

TEST_CASE("Rango Juan 3:16-18 devuelve tres versículos") {
    BibleDatabase db;
    CHECK(db.Abrir(":memory:"));
    Sembrar(db);

    std::vector<Versiculo> v;
    REQUIRE(db.ObtenerCita("Juan 3:16-18", &v));
    REQUIRE(v.size() == 3);
    CHECK(v[0].versiculo == 16);
    CHECK(v[1].versiculo == 17);
    CHECK(v[2].versiculo == 18);
}

TEST_CASE("Rango invertido Juan 3:18-16 se corrige") {
    BibleDatabase db;
    CHECK(db.Abrir(":memory:"));
    Sembrar(db);

    std::vector<Versiculo> v;
    REQUIRE(db.ObtenerCita("Juan 3:18-16", &v));
    REQUIRE(v.size() == 3);
}

TEST_CASE("Libro con prefijo numérico: 1 Corintios y 1corintios") {
    BibleDatabase db;
    CHECK(db.Abrir(":memory:"));
    Sembrar(db);

    std::vector<Versiculo> v;
    CHECK(db.ObtenerCita("1 Corintios 13:4-5", &v));
    CHECK(v.size() == 2);
    v.clear();
    CHECK(db.ObtenerCita("1corintios 13:4", &v));
    CHECK(v.size() == 1);
}

TEST_CASE("Abreviaturas del esquema: Gn, Jn, Ap") {
    BibleDatabase db;
    CHECK(db.Abrir(":memory:"));
    Sembrar(db);

    std::vector<Versiculo> v;
    REQUIRE(db.ObtenerCita("Gn 1:1", &v));
    REQUIRE(v.size() == 1);
    CHECK(v[0].libro == "Génesis");
    v.clear();
    CHECK(db.ObtenerCita("Jn 3:16", &v));
    CHECK(v.size() == 1);
    v.clear();
    CHECK(db.ObtenerCita("Ap 22:20", &v));
    CHECK(v.size() == 1);
}

TEST_CASE("Mayúsculas y espacios extra no rompen el parser") {
    BibleDatabase db;
    CHECK(db.Abrir(":memory:"));
    Sembrar(db);

    std::vector<Versiculo> v;
    CHECK(db.ObtenerCita("  GÉNESIS   1:1  ", &v));
    CHECK(v.size() == 1);
}

TEST_CASE("Citas inválidas devuelven false") {
    BibleDatabase db;
    CHECK(db.Abrir(":memory:"));
    Sembrar(db);

    std::vector<Versiculo> v;
    CHECK_FALSE(db.ObtenerCita("Xyz 1:1", &v));          // libro inexistente
    CHECK_FALSE(db.ObtenerCita("Juan", &v));             // sin capítulo:versículo
    CHECK_FALSE(db.ObtenerCita("Juan 3:", &v));          // versículo vacío
    CHECK_FALSE(db.ObtenerCita("Juan 0:0", &v));         // capítulo 0
    CHECK_FALSE(db.ObtenerCita("Cronicas 99:9", &v));    // capítulo sin datos
    CHECK_FALSE(db.ObtenerCita("Juan 3:16 y más", &v));  // texto sobrante
    CHECK(db.ObtenerCita("", &v) == false);
}

TEST_CASE("InsertarVersiculo reimporta sin duplicar (OR REPLACE)") {
    BibleDatabase db;
    CHECK(db.Abrir(":memory:"));
    REQUIRE(db.InsertarVersiculo("Juan", 3, 16, "texto A"));
    REQUIRE(db.InsertarVersiculo("Juan", 3, 16, "texto B"));
    CHECK(db.TotalVersiculos() == 1);

    std::vector<Versiculo> v;
    REQUIRE(db.ObtenerCita("Juan 3:16", &v));
    REQUIRE(v.size() == 1);
    CHECK(v[0].texto == "texto B");
}

TEST_CASE("InsertarVersiculo rechaza libro desconocido") {
    BibleDatabase db;
    CHECK(db.Abrir(":memory:"));
    CHECK_FALSE(db.InsertarVersiculo("Libro Inventado", 1, 1, "x"));
    CHECK(db.TotalVersiculos() == 0);
}
