// tests/native/test_importers.cpp — Tests de importadores (stubs)

#include "doctest.h"
#include "fusion/importers/HolyricsJson.h"
#include "fusion/importers/ZefaniaXml.h"
#include "fusion/importers/JsonBible.h"
#include "fusion/importers/TsvBible.h"
#include "fusion/data/SongDatabase.h"
#include "fusion/data/BibleDatabase.h"

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
