// src/core/src/data/BibleDatabase.cpp — Una biblia por .fdb (SQLite)

#include "fusion/data/BibleDatabase.h"
#include "fusion/importers/ZefaniaXml.h"
#include "fusion/importers/ESwordBib.h"
#include "fusion/importers/JsonBible.h"
#include "fusion/importers/TsvBible.h"

#include "sqlite3.h"

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace fusion {

struct BibleDatabase::Impl {
    sqlite3* db = nullptr;

    bool Ejecutar(const char* sql) {
        char* err = nullptr;
        int rc = sqlite3_exec(db, sql, nullptr, nullptr, &err);
        if (err) sqlite3_free(err);
        return rc == SQLITE_OK;
    }
};

BibleDatabase::BibleDatabase()  : impl_(std::make_unique<Impl>()) {}
BibleDatabase::~BibleDatabase() { Cerrar(); }

bool BibleDatabase::Abrir(const std::string& ruta_fdb) {
    if (sqlite3_open(ruta_fdb.c_str(), &impl_->db) != SQLITE_OK) return false;
    // Cargar esquema si está vacío (creación inicial, llena libros 1..66)
    std::ifstream f("data/schema/bible.sql");
    if (f) {
        std::stringstream ss; ss << f.rdbuf();
        impl_->Ejecutar(ss.str().c_str());
    }
    return true;
}

void BibleDatabase::Cerrar() {
    if (impl_->db) { sqlite3_close(impl_->db); impl_->db = nullptr; }
}

std::vector<LibroBiblia> BibleDatabase::ListarLibros() const {
    std::vector<LibroBiblia> out;
    if (!impl_->db) return out;
    const char* sql = "SELECT id, nombre, abrev3, abrev2, testamento FROM libros ORDER BY id";
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(impl_->db, sql, -1, &st, nullptr) != SQLITE_OK) return out;
    while (sqlite3_step(st) == SQLITE_ROW) {
        LibroBiblia l;
        l.id = sqlite3_column_int(st, 0);
        const unsigned char* n = sqlite3_column_text(st, 1);
        l.nombre     = n ? reinterpret_cast<const char*>(n) : "";
        const unsigned char* a3 = sqlite3_column_text(st, 2);
        l.abrev3     = a3 ? reinterpret_cast<const char*>(a3) : "";
        const unsigned char* a2 = sqlite3_column_text(st, 3);
        l.abrev2     = a2 ? reinterpret_cast<const char*>(a2) : "";
        const unsigned char* t = sqlite3_column_text(st, 4);
        l.testamento = t ? reinterpret_cast<const char*>(t) : "";
        out.push_back(l);
    }
    sqlite3_finalize(st);
    return out;
}

std::vector<int> BibleDatabase::ListarCapitulos(int libro_id) const {
    std::vector<int> out;
    if (!impl_->db) return out;
    const char* sql = "SELECT numero FROM capitulos WHERE libro_id = ?1 ORDER BY numero";
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(impl_->db, sql, -1, &st, nullptr) != SQLITE_OK) return out;
    sqlite3_bind_int(st, 1, libro_id);
    while (sqlite3_step(st) == SQLITE_ROW) {
        out.push_back(sqlite3_column_int(st, 0));
    }
    sqlite3_finalize(st);
    return out;
}

std::vector<Versiculo> BibleDatabase::ObtenerCapitulo(int libro_id, int capitulo) const {
    std::vector<Versiculo> out;
    if (!impl_->db) return out;
    const char* sql =
        "SELECT l.nombre, c.numero, v.numero, v.texto "
        "FROM versiculos v "
        "JOIN capitulos c ON c.id = v.capitulo_id "
        "JOIN libros    l ON l.id = c.libro_id "
        "WHERE c.libro_id = ?1 AND c.numero = ?2 "
        "ORDER BY v.numero";
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(impl_->db, sql, -1, &st, nullptr) != SQLITE_OK) return out;
    sqlite3_bind_int(st, 1, libro_id);
    sqlite3_bind_int(st, 2, capitulo);
    while (sqlite3_step(st) == SQLITE_ROW) {
        Versiculo v;
        const unsigned char* n = sqlite3_column_text(st, 0);
        v.libro     = n ? reinterpret_cast<const char*>(n) : "";
        v.capitulo  = sqlite3_column_int(st, 1);
        v.versiculo = sqlite3_column_int(st, 2);
        const unsigned char* t = sqlite3_column_text(st, 3);
        v.texto     = t ? reinterpret_cast<const char*>(t) : "";
        out.push_back(v);
    }
    sqlite3_finalize(st);
    return out;
}

bool BibleDatabase::ObtenerCita(const std::string& cita,
                                std::vector<Versiculo>* out) const {
    // TODO(P0): parser "Salmo 100:4" y "Juan 3:16-18".
    (void)cita; (void)out;
    return false;
}

std::vector<Versiculo> BibleDatabase::Buscar(const std::string& texto, int limite) const {
    std::vector<Versiculo> out;
    if (!impl_->db) return out;
    const char* sql =
        "SELECT l.nombre, c.numero, v.numero, v.texto "
        "FROM versiculos_fts f "
        "JOIN versiculos v ON v.id = f.rowid "
        "JOIN capitulos c ON c.id = v.capitulo_id "
        "JOIN libros    l ON l.id = c.libro_id "
        "WHERE versiculos_fts MATCH ?1 "
        "LIMIT ?2";
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(impl_->db, sql, -1, &st, nullptr) != SQLITE_OK) return out;
    sqlite3_bind_text(st, 1, texto.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(st, 2, limite);
    while (sqlite3_step(st) == SQLITE_ROW) {
        Versiculo v;
        const unsigned char* n = sqlite3_column_text(st, 0);
        v.libro     = n ? reinterpret_cast<const char*>(n) : "";
        v.capitulo  = sqlite3_column_int(st, 1);
        v.versiculo = sqlite3_column_int(st, 2);
        const unsigned char* t = sqlite3_column_text(st, 3);
        v.texto     = t ? reinterpret_cast<const char*>(t) : "";
        out.push_back(v);
    }
    sqlite3_finalize(st);
    return out;
}

std::vector<std::string> BibleDatabase::Favoritos() const {
    std::vector<std::string> out;
    if (!impl_->db) return out;
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(impl_->db, "SELECT cita FROM favoritos ORDER BY fecha DESC", -1, &st, nullptr) != SQLITE_OK) return out;
    while (sqlite3_step(st) == SQLITE_ROW) {
        const unsigned char* c = sqlite3_column_text(st, 0);
        out.push_back(c ? reinterpret_cast<const char*>(c) : "");
    }
    sqlite3_finalize(st);
    return out;
}

bool BibleDatabase::AgregarFavorito(const std::string& cita) {
    if (!impl_->db) return false;
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(impl_->db,
        "INSERT OR REPLACE INTO favoritos (cita) VALUES (?1)", -1, &st, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_text(st, 1, cita.c_str(), -1, SQLITE_TRANSIENT);
    bool ok = sqlite3_step(st) == SQLITE_DONE;
    sqlite3_finalize(st);
    return ok;
}

bool BibleDatabase::QuitarFavorito(const std::string& cita) {
    if (!impl_->db) return false;
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(impl_->db,
        "DELETE FROM favoritos WHERE cita = ?1", -1, &st, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_text(st, 1, cita.c_str(), -1, SQLITE_TRANSIENT);
    bool ok = sqlite3_step(st) == SQLITE_DONE;
    sqlite3_finalize(st);
    return ok;
}

int BibleDatabase::ImportarZefaniaXml(const std::string& ruta_xml) {
    return ZefaniaXml::Importar(*this, ruta_xml);
}
int BibleDatabase::ImportarESwordBib(const std::string& ruta_bib) {
    return ESwordBib::Importar(*this, ruta_bib);
}
int BibleDatabase::ImportarJson(const std::string& ruta_json) {
    return JsonBible::Importar(*this, ruta_json);
}
int BibleDatabase::ImportarTsv(const std::string& ruta_tsv) {
    return TsvBible::Importar(*this, ruta_tsv);
}

} // namespace fusion
