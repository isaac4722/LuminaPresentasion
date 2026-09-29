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

// ---------------------------------------------------------------------------
// TSV — libro\tcapitulo\tversiculo\ttexto
// ---------------------------------------------------------------------------
TEST_CASE("TsvBible.Importar inserta versículos y respeta encabezado") {
    BibleDatabase db;
    REQUIRE(db.Abrir(":memory:"));

    const char* ruta = "tmp_tsv_bible_test.tsv";
    {
        std::ofstream f(ruta, std::ios::binary);
        f << "book\tchapter\tverse\ttext\r\n"
          << "Juan\t3\t16\tPorque de tal manera amó Dios al mundo...\r\n"
          << "1 Corintios\t13\t4\tEl amor es sufrido, es benigno...\r\n"
          << "\r\n"
          << "Gn\t1\t1\tEn el principio creó Dios los cielos.\r\n";
    }

    const int n = TsvBible::Importar(db, ruta);
    std::remove(ruta);
    CHECK(n == 3);
    CHECK(db.TotalVersiculos() == 3);

    std::vector<Versiculo> v;
    REQUIRE(db.ObtenerCita("Juan 3:16", &v));
    REQUIRE(v.size() == 1);
    v.clear();
    REQUIRE(db.ObtenerCita("1 Co 13:4", &v));
    REQUIRE(v.size() == 1);
    v.clear();
    REQUIRE(db.ObtenerCita("Génesis 1:1", &v));
    REQUIRE(v.size() == 1);
}

TEST_CASE("TsvBible.Importar salta líneas malformadas") {
    BibleDatabase db;
    REQUIRE(db.Abrir(":memory:"));

    const char* ruta = "tmp_tsv_bible_mal.tsv";
    {
        std::ofstream f(ruta, std::ios::binary);
        f << "Juan\t3\t16\tok\n"
          << "solo tres campos\t1\t2\n"          // <4 campos → fuera
          << "Juan\tcero\t16\tcap no numérico\n" // cap inválido → fuera
          << "Juan\t3\t17\tok también\n";
    }
    const int n = TsvBible::Importar(db, ruta);
    std::remove(ruta);
    CHECK(n == 2);
}

// ---------------------------------------------------------------------------
// Zefania XML — namespaces, entidades y tags internos
// ---------------------------------------------------------------------------
TEST_CASE("ZefaniaXml.Importar inserta versículos con namespace y entidades") {
    BibleDatabase db;
    REQUIRE(db.Abrir(":memory:"));

    const char* ruta = "tmp_zefania_test.xml";
    {
        std::ofstream f(ruta, std::ios::binary);
        f << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
          << "<XMLBIBLE xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" "
          << "biblename=\"Reina-Valera 1909\" revision=\"1909\">\n"
          << "  <BIBLEBOOK bookname=\"Génesis\" bnumber=\"1\">\n"
          << "    <CHAPTER cnumber=\"1\">\n"
          << "      <VERS vnumber=\"1\">En el principio creó &amp;vea Dios los cielos.</VERS>\n"
          << "      <VERS vnumber=\"2\"><STYLE Remarks=\"rojo\">Y la tierra</STYLE> estaba desordenada.</VERS>\n"
          << "    </CHAPTER>\n"
          << "  </BIBLEBOOK>\n"
          << "  <BIBLEBOOK bookname=\"Juan\" booknumber=\"43\">\n"
          << "    <CHAPTER cnumber=\"3\">\n"
          << "      <VERS vnumber=\"16\">Porque de tal manera am&#243; Dios al mundo\n"
          << "        <NOTE tipo=\"estudio\">nota de estudio que no va al texto</NOTE>\n"
          << "      </VERS>\n"
          << "      <VERS vnumber=\"18\">El que en &#233;l cree, no es condenado.</VERS>\n"
          << "    </CHAPTER>\n"
          << "  </BIBLEBOOK>\n"
          << "</XMLBIBLE>\n";
    }

    const int n = ZefaniaXml::Importar(db, ruta);
    std::remove(ruta);
    CHECK(n == 4);
    CHECK(db.TotalVersiculos() == 4);

    std::vector<Versiculo> v;
    REQUIRE(db.ObtenerCita("Génesis 1:2", &v));
    REQUIRE(v.size() == 1);
    CHECK(v[0].texto.find("STYLE") == std::string::npos);
    CHECK(v[0].texto.find("Y la tierra") != std::string::npos);

    v.clear();
    REQUIRE(db.ObtenerCita("Juan 3:16", &v));
    REQUIRE(v.size() == 1);
    CHECK(v[0].texto.find("nota de estudio") == std::string::npos);
    CHECK(v[0].texto.find("manera amó") != std::string::npos);
    CHECK(v[0].texto.find("&#") == std::string::npos);

    v.clear();
    REQUIRE(db.ObtenerCita("Juan 3:18", &v));
    REQUIRE(v.size() == 1);
    CHECK(v[0].texto.find("en él cree") != std::string::npos);
}

TEST_CASE("ZefaniaXml.Importar archivo sin XMLBIBLE devuelve 0") {
    BibleDatabase db;
    REQUIRE(db.Abrir(":memory:"));
    const char* ruta = "tmp_zefania_invalido.xml";
    {
        std::ofstream f(ruta, std::ios::binary);
        f << "<otro-documento><p>hola</p></otro-documento>";
    }
    const int n = ZefaniaXml::Importar(db, ruta);
    std::remove(ruta);
    CHECK(n == 0);
}

// ---------------------------------------------------------------------------
// Holyrics JSON — canto único, lista y deduplicación
// ---------------------------------------------------------------------------
TEST_CASE("HolyricsJson.Importar canto único con slides") {
    SongDatabase db;
    REQUIRE(db.Abrir(":memory:"));

    const char* ruta = "tmp_holyrics_single.json";
    {
        std::ofstream f(ruta, std::ios::binary);
        f << "{\n"
          << "  \"song\": {\n"
          << "    \"title\": \"Cuán Grande Es Él\",\n"
          << "    \"author\": \"Carl Boberg (trad.)\",\n"
          << "    \"key\": \"G\",\n"
          << "    \"bpm\": 90,\n"
          << "    \"slides\": [\n"
          << "      {\"name\":\"Verso 1\",\"type\":\"verse\","
          << "\"text\":\"Señor mi Dios, al contemplar los cielos\\nel firmamento y las estrellas mil\"},\n"
          << "      {\"name\":\"Coro\",\"type\":\"chorus\","
          << "\"text\":\"Cuán grande es Él, cuán grande es Él\\nY mi alma cantará\"}\n"
          << "    ]\n"
          << "  }\n"
          << "}\n";
    }
    const int n = HolyricsJson::Importar(db, ruta);
    std::remove(ruta);
    REQUIRE(n == 1);

    std::vector<Canto> lista = db.ListarTodos();
    REQUIRE(lista.size() == 1);
    CHECK(lista[0].titulo == "Cuán Grande Es Él");
    CHECK(lista[0].tono_origen == "G");
    CHECK(lista[0].bpm == 90);

    CantoDetalle det;
    REQUIRE(db.Obtener(lista[0].id, &det));
    REQUIRE(det.secciones.size() == 2);
    CHECK(det.secciones[0].tipo == "verso");
    CHECK(det.secciones[0].etiqueta == "Verso 1");
    REQUIRE(det.secciones[0].lineas.size() == 2);
    CHECK(det.secciones[0].lineas[0].find("Señor mi Dios") != std::string::npos);
    CHECK(det.secciones[1].tipo == "coro");
    CHECK(det.secciones[1].lineas.size() == 2);
}

TEST_CASE("HolyricsJson.Importar reimportar no duplica") {
    SongDatabase db;
    REQUIRE(db.Abrir(":memory:"));

    const char* ruta = "tmp_holyrics_dup.json";
    const char* contenido =
        "{\"song\":{\"title\":\"Test Canto\",\"author\":\"Anónimo\","
        "\"slides\":[{\"name\":\"Coro\",\"type\":\"chorus\",\"text\":\"línea 1\\nlínea 2\"}]}}";
    {
        std::ofstream f(ruta, std::ios::binary);
        f << contenido;
    }
    REQUIRE(HolyricsJson::Importar(db, ruta) == 1);
    REQUIRE(HolyricsJson::Importar(db, ruta) == 0);   // ya existía
    std::remove(ruta);
    CHECK(db.ListarTodos().size() == 1);
}

TEST_CASE("HolyricsJson.Importar lista de cantos") {
    SongDatabase db;
    REQUIRE(db.Abrir(":memory:"));

    const char* ruta = "tmp_holyrics_lista.json";
    {
        std::ofstream f(ruta, std::ios::binary);
        f << "{\"songs\":["
          << "{\"title\":\"Uno\",\"slides\":[{\"name\":\"V1\",\"type\":\"verse\",\"text\":\"a\"}]},"
          << "{\"title\":\"Dos\",\"slides\":[{\"name\":\"C\",\"type\":\"chorus\",\"text\":\"b\"}]},"
          << "{\"title\":\"Sin secciones\"}"
          << "]}";
    }
    const int n = HolyricsJson::Importar(db, ruta);
    std::remove(ruta);
    CHECK(n == 2);
    CHECK(db.ListarTodos().size() == 2);
}

// ---------------------------------------------------------------------------
// Regresión "no se puede conectar con el Core": las importaciones masivas
// van en UNA transacción (BEGIN IMMEDIATE ... COMMIT) y un fallo a mitad
// de camino deja la BD intacta (ROLLBACK en la destrucción de la guardia).
// ---------------------------------------------------------------------------

TEST_CASE("Transaccion: importacion grande en una transaccion mantiene la BD consistente") {
    BibleDatabase db;
    REQUIRE(db.Abrir(":memory:"));

    const char* ruta = "tmp_grande.json";
    {
        std::ofstream f(ruta, std::ios::binary);
        f << "{\"libros\":[{\"abrev3\":\"Gén\",\"nombre\":\"Génesis\",\"capitulos\":[";
        f << "{\"numero\":1,\"versiculos\":[";
        for (int i = 1; i <= 500; ++i) {
            if (i > 1) f << ",";
            f << "{\"numero\":" << i << ",\"texto\":\"v" << i << "\"}";
        }
        f << "]}]}]}";
    }
    const int n = JsonBible::Importar(db, ruta);
    std::remove(ruta);
    CHECK(n == 500);
    CHECK(db.TotalVersiculos() == 500);

    // Sin transacción abierta colgando: una escritura posterior funciona.
    CHECK(db.InsertarVersiculo("Gén", 2, 1, "después del COMMIT"));
    CHECK(db.TotalVersiculos() == 501);
}

TEST_CASE("Transaccion: ROLLBACK deja la BD intacta si Confirma nunca se llama") {
    BibleDatabase db;
    REQUIRE(db.Abrir(":memory:"));
    REQUIRE(db.InsertarVersiculo("Gén", 1, 1, "previo"));

    {
        REQUIRE(db.IniciarTransaccion());
        REQUIRE(db.InsertarVersiculo("Gén", 1, 2, "en transacción"));
        // Sin Confirmar: la destrucción implícita aquí es del scope; usamos
        // DescartarTransaccion directamente para simular el guardia.
        db.DescartarTransaccion();
    }
    CHECK(db.TotalVersiculos() == 1);   // el insert de la transacción se fue

    // La BD sigue plenamente operativa tras el ROLLBACK.
    CHECK(db.InsertarVersiculo("Gén", 1, 3, "posterior al rollback"));
    CHECK(db.TotalVersiculos() == 2);
}

TEST_CASE("Transaccion: API idempotente y orden correcto") {
    BibleDatabase db;
    REQUIRE(db.Abrir(":memory:"));
    CHECK(db.IniciarTransaccion());
    CHECK(db.InsertarVersiculo("Gén", 1, 1, "a"));
    CHECK(db.ConfirmarTransaccion());
    // COMMIT sin BEGIN posterior: los pares BEGIN/COMMIT anidados no existen.
    CHECK(db.InsertarVersiculo("Gén", 1, 2, "b"));
    CHECK(db.TotalVersiculos() == 2);
    // Descartar sin transacción: silencioso, no rompe nada.
    db.DescartarTransaccion();
    CHECK(db.TotalVersiculos() == 2);
}

TEST_CASE("Transaccion: los importadores siguen funcionando tras transaccion manual cerrada") {
    SongDatabase db;
    REQUIRE(db.Abrir(":memory:"));
    REQUIRE(db.IniciarTransaccion());
    REQUIRE(db.ConfirmarTransaccion());

    const char* ruta = "tmp_holyrics_post_tx.json";
    {
        std::ofstream f(ruta, std::ios::binary);
        f << "{\"song\":{\"title\":\"Tx\",\"artist\":\"T\",\"slides\":"
          << "[{\"name\":\"V1\",\"type\":\"verse\",\"text\":\"hola\"}]}}";
    }
    const int n = HolyricsJson::Importar(db, ruta);
    std::remove(ruta);
    CHECK(n == 1);
}
