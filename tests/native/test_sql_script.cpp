// tests/native/test_sql_script.cpp — Tests del particionador SQL.
// Regresión: partir los cuerpos BEGIN...END de los triggers dejaba el FTS
// sin sincronización (la búsqueda sobre versiculos_fts devolvía vacío).

#include "doctest.h"
#include "fusion/data/SqlScript.h"
#include "esquema_biblia.h"

#include <sqlite3.h>

#include <string>
#include <vector>

using namespace fusion::sqlutil;

TEST_CASE("Partir: el cuerpo BEGIN...END de un trigger se conserva entero") {
    const std::string script = R"(
CREATE TABLE t (id INTEGER PRIMARY KEY, v TEXT);
CREATE TABLE f (rowid INTEGER, v TEXT);
CREATE TRIGGER trg_ai AFTER INSERT ON t BEGIN
    INSERT INTO f (rowid, v) VALUES (new.id, new.v);
    INSERT INTO f (rowid, v) VALUES (new.id + 1, new.v);
END;
CREATE INDEX idx_t ON t (v);
)";
    const std::vector<std::string> partes = Partir(script);
    REQUIRE(partes.size() == 4);
    CHECK(partes[0].find("CREATE TABLE t") != std::string::npos);
    CHECK(partes[2].find("CREATE TRIGGER") != std::string::npos);
    CHECK(partes[2].find("BEGIN") != std::string::npos);
    CHECK(partes[2].find("END") != std::string::npos);
    CHECK(partes[2].find("new.id + 1") != std::string::npos);
    CHECK(partes[3].find("CREATE INDEX") != std::string::npos);
}

TEST_CASE("Partir: BEGIN TRANSACTION no se confunde con un trigger") {
    const std::vector<std::string> partes = Partir(
        "BEGIN TRANSACTION; INSERT INTO t VALUES (1, 'x'); COMMIT;");
    REQUIRE(partes.size() == 3);
    CHECK(partes[0].find("BEGIN") != std::string::npos);
    CHECK(partes[1].find("INSERT") != std::string::npos);
    CHECK(partes[2].find("COMMIT") != std::string::npos);
}

TEST_CASE("Partir: literales y comentarios con ';' no parten") {
    const std::vector<std::string> partes = Partir(
        "INSERT INTO t VALUES (1, 'a;b'); -- comentario; con ;\n"
        "/* bloque; con ; */ INSERT INTO t VALUES (2, 'c');");
    REQUIRE(partes.size() == 2);
    CHECK(partes[0].find("'a;b'") != std::string::npos);
    CHECK(partes[1].find("'c'") != std::string::npos);
}

TEST_CASE("Partir: identificador que contiene BEGIN no activa el modo trigger") {
    const std::vector<std::string> partes = Partir(
        "INSERT INTO t VALUES (1, 'APPEND;NOPE');");
    REQUIRE(partes.size() == 1);
}

TEST_CASE("EjecutarScript: el esquema real crea los triggers del FTS") {
    sqlite3* db = nullptr;
    REQUIRE(sqlite3_open(":memory:", &db) == SQLITE_OK);

    // El esquema embebido debe ejecutarse sin errores ahora.
    CHECK(EjecutarScript(db, fusion::esquema::kEsquemaBiblia));

    sqlite3_stmt* st = nullptr;
    REQUIRE(sqlite3_prepare_v2(db,
        "SELECT COUNT(*) FROM sqlite_master WHERE type='trigger' "
        "AND name LIKE 'trg_versiculos_%'",
        -1, &st, nullptr) == SQLITE_OK);
    REQUIRE(sqlite3_step(st) == SQLITE_ROW);
    CHECK(sqlite3_column_int(st, 0) == 3);
    sqlite3_finalize(st);

    sqlite3_close(db);
}

TEST_CASE("EjecutarScript: INSERT en versiculos alimenta el FTS (búsqueda)") {
    sqlite3* db = nullptr;
    REQUIRE(sqlite3_open(":memory:", &db) == SQLITE_OK);
    REQUIRE(EjecutarScript(db, fusion::esquema::kEsquemaBiblia));

    // Semilla mínima. El esquema ya siembra los 66 libros, así que se
    // usa un id fuera del árbol canónico.
    REQUIRE(EjecutarScript(db,
        "INSERT INTO libros (id, nombre, abrev3, abrev2, testamento) "
        "VALUES (99, 'TestLibro', 'Tst', 'Ts', 'AT');"
        "INSERT INTO capitulos (id, libro_id, numero) VALUES (1, 99, 100);"
        "INSERT INTO versiculos (capitulo_id, numero, texto) "
        "VALUES (1, 4, 'Entrad por sus puertas con accion de gracias');"));

    // El trigger trg_versiculos_ai debe haber indexado el texto.
    sqlite3_stmt* st = nullptr;
    REQUIRE(sqlite3_prepare_v2(db,
        "SELECT COUNT(*) FROM versiculos_fts WHERE versiculos_fts MATCH ?1",
        -1, &st, nullptr) == SQLITE_OK);
    sqlite3_bind_text(st, 1, "puertas", -1, SQLITE_STATIC);
    REQUIRE(sqlite3_step(st) == SQLITE_ROW);
    CHECK(sqlite3_column_int(st, 0) == 1);
    sqlite3_finalize(st);

    // UPDATE y DELETE también deben mantener el índice sincronizado.
    REQUIRE(EjecutarScript(db,
        "UPDATE versiculos SET texto = 'Alabadle y bendecid su nombre' "
        "WHERE capitulo_id = 1 AND numero = 4;"));
    REQUIRE(sqlite3_prepare_v2(db,
        "SELECT COUNT(*) FROM versiculos_fts WHERE versiculos_fts MATCH ?1",
        -1, &st, nullptr) == SQLITE_OK);
    sqlite3_bind_text(st, 1, "bendecid", -1, SQLITE_STATIC);
    REQUIRE(sqlite3_step(st) == SQLITE_ROW);
    CHECK(sqlite3_column_int(st, 0) == 1);
    sqlite3_finalize(st);
    // El texto viejo ya no debe estar indexado.
    REQUIRE(sqlite3_prepare_v2(db,
        "SELECT COUNT(*) FROM versiculos_fts WHERE versiculos_fts MATCH ?1",
        -1, &st, nullptr) == SQLITE_OK);
    sqlite3_bind_text(st, 1, "puertas", -1, SQLITE_STATIC);
    REQUIRE(sqlite3_step(st) == SQLITE_ROW);
    CHECK(sqlite3_column_int(st, 0) == 0);
    sqlite3_finalize(st);

    sqlite3_close(db);
}
