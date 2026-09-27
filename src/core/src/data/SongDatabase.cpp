// src/core/src/data/SongDatabase.cpp — cancionero.fdb (SQLite)

#include "fusion/data/SongDatabase.h"
#include "fusion/importers/HolyricsJson.h"

#include "sqlite3.h"
#include "json.hpp"

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace fusion {

struct SongDatabase::Impl {
    sqlite3* db = nullptr;

    bool Ejecutar(const char* sql) {
        char* err = nullptr;
        int rc = sqlite3_exec(db, sql, nullptr, nullptr, &err);
        if (err) sqlite3_free(err);
        return rc == SQLITE_OK;
    }
};

SongDatabase::SongDatabase()  : impl_(std::make_unique<Impl>()) {}
SongDatabase::~SongDatabase() { Cerrar(); }

bool SongDatabase::Abrir(const std::string& ruta_fdb) {
    if (sqlite3_open(ruta_fdb.c_str(), &impl_->db) != SQLITE_OK) return false;
    // Cargar esquema si está vacío (creación inicial)
    std::ifstream f("data/schema/cancionero.sql");
    if (f) {
        std::stringstream ss; ss << f.rdbuf();
        impl_->Ejecutar(ss.str().c_str());
    }
    return true;
}

void SongDatabase::Cerrar() {
    if (impl_->db) { sqlite3_close(impl_->db); impl_->db = nullptr; }
}

std::vector<Canto> SongDatabase::ListarTodos() const {
    std::vector<Canto> out;
    if (!impl_->db) return out;
    const char* sql = "SELECT id, titulo, autor, tono_origen, bpm, veces_usado "
                      "FROM cantos ORDER BY titulo COLLATE NOCASE";
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(impl_->db, sql, -1, &st, nullptr) != SQLITE_OK) return out;
    while (sqlite3_step(st) == SQLITE_ROW) {
        Canto c;
        c.id           = sqlite3_column_int64(st, 0);
        const unsigned char* t = sqlite3_column_text(st, 1);
        c.titulo       = t ? reinterpret_cast<const char*>(t) : "";
        const unsigned char* a = sqlite3_column_text(st, 2);
        c.autor        = a ? reinterpret_cast<const char*>(a) : "";
        const unsigned char* tono = sqlite3_column_text(st, 3);
        c.tono_origen  = tono ? reinterpret_cast<const char*>(tono) : "";
        c.bpm          = sqlite3_column_int(st, 4);
        c.veces_usado  = sqlite3_column_int(st, 5);
        out.push_back(c);
    }
    sqlite3_finalize(st);
    return out;
}

std::vector<Canto> SongDatabase::Buscar(const std::string& texto, int limite) const {
    std::vector<Canto> out;
    if (!impl_->db) return out;
    const char* sql = "SELECT id, titulo, autor, tono_origen, bpm, veces_usado "
                      "FROM cantos WHERE titulo LIKE ?1 OR autor LIKE ?1 "
                      "ORDER BY veces_usado DESC LIMIT ?2";
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(impl_->db, sql, -1, &st, nullptr) != SQLITE_OK) return out;
    std::string q = "%" + texto + "%";
    sqlite3_bind_text(st, 1, q.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(st, 2, limite);
    while (sqlite3_step(st) == SQLITE_ROW) {
        Canto c;
        c.id           = sqlite3_column_int64(st, 0);
        const unsigned char* t = sqlite3_column_text(st, 1);
        c.titulo       = t ? reinterpret_cast<const char*>(t) : "";
        const unsigned char* a = sqlite3_column_text(st, 2);
        c.autor        = a ? reinterpret_cast<const char*>(a) : "";
        const unsigned char* tono = sqlite3_column_text(st, 3);
        c.tono_origen  = tono ? reinterpret_cast<const char*>(tono) : "";
        c.bpm          = sqlite3_column_int(st, 4);
        c.veces_usado  = sqlite3_column_int(st, 5);
        out.push_back(c);
    }
    sqlite3_finalize(st);
    return out;
}

bool SongDatabase::Obtener(std::int64_t id, CantoDetalle* out) const {
    if (!impl_->db || !out) return false;
    const char* sql = "SELECT titulo, autor, tono_origen, bpm, veces_usado "
                      "FROM cantos WHERE id = ?1";
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(impl_->db, sql, -1, &st, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_int64(st, 1, id);
    if (sqlite3_step(st) == SQLITE_ROW) {
        const unsigned char* t = sqlite3_column_text(st, 0);
        out->titulo      = t ? reinterpret_cast<const char*>(t) : "";
        const unsigned char* a = sqlite3_column_text(st, 1);
        out->autor       = a ? reinterpret_cast<const char*>(a) : "";
        const unsigned char* tono = sqlite3_column_text(st, 2);
        out->tono_origen = tono ? reinterpret_cast<const char*>(tono) : "";
        out->bpm         = sqlite3_column_int(st, 3);
        out->veces_usado = sqlite3_column_int(st, 4);
        out->id = id;
        // TODO(P0): cargar secciones y líneas.
        sqlite3_finalize(st);
        return true;
    }
    sqlite3_finalize(st);
    return false;
}

int SongDatabase::ImportarHolyricsJson(const std::string& ruta_json) {
    return HolyricsJson::Importar(*this, ruta_json);
}

} // namespace fusion
