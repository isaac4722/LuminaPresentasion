// tests/native/test_importers.cpp — Tests de importadores
// (JSON interno, TSV, Zefania XML, Holyrics)

#include "doctest.h"
#include "fusion/importers/HolyricsJson.h"
#include "fusion/importers/ZefaniaXml.h"
#include "fusion/importers/JsonBible.h"
#include "fusion/importers/TsvBible.h"
#include "fusion/data/SongDatabase.h"
#include "fusion/data/BibleDatabase.h"

#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

using namespace fusion;

TEST_CASE("HolyricsJson.Importar en archivo inexistente devuelve 0") {
    SongDatabase db;
    db.Abrir(":memory:");
    CHECK(HolyricsJson::Importar(db, "no_existe.json") == 0);
}

TEST_CASE("ZefaniaXml.Importar en archivo inexistente devuelve 0") {
    BibleDatabase db;
    db.Abrir(":memory:");
    CHECK(ZefaniaXml::Importar(db, "no_existe.xml") == 0);
}

TEST_CASE("JsonBible.Importar en archivo inexistente devuelve 0") {
    BibleDatabase db;
    db.Abrir(":memory:");
    CHECK(JsonBible::Importar(db, "no_existe.json") == 0);
}

TEST_CASE("TsvBible.Importar en archivo inexistente devuelve 0") {
    BibleDatabase db;
    db.Abrir(":memory:");
    CHECK(TsvBible::Importar(db, "no_existe.tsv") == 0);
}

// ---------------------------------------------------------------------------
// JSON interno — inserta de verdad y luego la cita es consultable
// ---------------------------------------------------------------------------
TEST_CASE("JsonBible.Importar inserta versículos y la cita se consulta") {
    BibleDatabase db;
    REQUIRE(db.Abrir(":memory:"));

    const char* ruta = "tmp_json_bible_test.json";
    {
        std::ofstream f(ruta, std::ios::binary);
        f << "{\n"
          << "  \"biblia\": {\"nombre\":\"Test\",\"abrev\":\"TST\"},\n"
          << "  \"libros\": [\n"
          << "    {\"abrev3\":\"Jua\",\"nombre\":\"Juan\",\"capitulos\":[\n"
          << "      {\"numero\":3,\"versiculos\":[\n"
          << "        {\"numero\":16,\"texto\":\"Porque de tal manera am\\u00f3 Dios al mundo...\"},\n"
          << "        {\"numero\":17,\"texto\":\"Porque no envi\\u00f3 Dios a su Hijo...\"}\n"
          << "      ]}\n"
          << "    ]}\n"
          << "  ]\n"
          << "}\n";
    }

    const int n = JsonBible::Importar(db, ruta);
    std::remove(ruta);

    REQUIRE(n == 2);
    CHECK(db.TotalVersiculos() == 2);

    std::vector<Versiculo> v;
    REQUIRE(db.ObtenerCita("Juan 3:16-17", &v));
    REQUIRE(v.size() == 2);
    CHECK(v[0].texto.find("manera am") != std::string::npos);
}

TEST_CASE("JsonBible.Importar reimportar no duplica") {
    BibleDatabase db;
    REQUIRE(db.Abrir(":memory:"));

    const char* ruta = "tmp_json_bible_duplicado.json";
    const char* contenido =
        "{\"libros\":[{\"abrev3\":\"Gn\",\"nombre\":\"Génesis\",\"capitulos\":["
        "{\"numero\":1,\"versiculos\":[{\"numero\":1,\"texto\":\"En el principio\"}]}"
        "]}]}";
    {
        std::ofstream f(ruta, std::ios::binary);
        f << contenido;
    }
    REQUIRE(JsonBible::Importar(db, ruta) == 1);
    REQUIRE(JsonBible::Importar(db, ruta) == 1);
    std::remove(ruta);
    CHECK(db.TotalVersiculos() == 1);
}

TEST_CASE("JsonBible.Importar JSON corrupto devuelve 0") {
    BibleDatabase db;
    REQUIRE(db.Abrir(":memory:"));
    const char* ruta = "tmp_json_bible_corrupto.json";
    {
        std::ofstream f(ruta, std::ios::binary);
        f << "{ no es json ]{";
    }
    const int n = JsonBible::Importar(db, ruta);
    std::remove(ruta);
    CHECK(n == 0);
}
