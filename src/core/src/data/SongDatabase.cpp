// src/core/src/data/SongDatabase.cpp — cancionero.fdb (SQLite)

#include "fusion/data/SongDatabase.h"
#include "fusion/importers/HolyricsJson.h"

#include "sqlite3.h"
#include "json.hpp"
#include "esquema_cancionero.h"   // generado por CMake: esquema embebido
#include "fusion/data/SqlScript.h"

#include <cstdio>
#include <string>
#include <sstream>
#include <vector>

namespace fusion {

struct SongDatabase::Impl {
    sqlite3* db = nullptr;

    // Ejecución resiliente del esquema (ver fusion/data/SqlScript.h).
    bool Ejecutar(const char* sql) {
        return sqlutil::EjecutarScript(db, sql);
    }
};

SongDatabase::SongDatabase()  : impl_(std::make_unique<Impl>()) {}
SongDatabase::~SongDatabase() { Cerrar(); }

bool SongDatabase::Abrir(const std::string& ruta_fdb) {
    if (sqlite3_open(ruta_fdb.c_str(), &impl_->db) != SQLITE_OK) return false;
    // Esquema embebido en el binario (generado desde data/schema/
    // cancionero.sql): sin dependencia del CWD; CREATE IF NOT EXISTS.
    impl_->Ejecutar(fusion::esquema::kEsquemaCancionero);
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
        sqlite3_finalize(st);

        // Cargar secciones y líneas del canto.
        out->secciones.clear();
        std::vector<std::int64_t> secciones_ids;
        sqlite3_stmt* ss = nullptr;
        if (sqlite3_prepare_v2(impl_->db,
                "SELECT id, tipo, etiqueta FROM canto_secciones "
                "WHERE canto_id = ?1 ORDER BY orden",
                -1, &ss, nullptr) != SQLITE_OK) return true;   // sin secciones
        sqlite3_bind_int64(ss, 1, id);
        while (sqlite3_step(ss) == SQLITE_ROW) {
            CantoDetalle::Seccion sec;
            const std::int64_t sec_id = sqlite3_column_int64(ss, 0);
            const unsigned char* ti = sqlite3_column_text(ss, 1);
            sec.tipo = ti ? reinterpret_cast<const char*>(ti) : "estrofa";
            const unsigned char* et = sqlite3_column_text(ss, 2);
            sec.etiqueta = et ? reinterpret_cast<const char*>(et) : "";
            sec.orden = static_cast<int>(out->secciones.size());
            out->secciones.push_back(sec);
            secciones_ids.push_back(sec_id);
        }
        sqlite3_finalize(ss);

        // Líneas por sección (mismo orden que secciones_ids).
        for (size_t i = 0; i < secciones_ids.size(); ++i) {
            sqlite3_stmt* sl = nullptr;
            if (sqlite3_prepare_v2(impl_->db,
                    "SELECT texto FROM canto_lineas WHERE seccion_id = ?1 "
                    "ORDER BY orden",
                    -1, &sl, nullptr) != SQLITE_OK) continue;
            sqlite3_bind_int64(sl, 1, secciones_ids[i]);
            while (sqlite3_step(sl) == SQLITE_ROW) {
                const unsigned char* tx = sqlite3_column_text(sl, 0);
                out->secciones[i].lineas.push_back(
                    tx ? reinterpret_cast<const char*>(tx) : "");
            }
            sqlite3_finalize(sl);
        }
        return true;
    }
    sqlite3_finalize(st);
    return false;
}

std::int64_t SongDatabase::BuscarCanto(const std::string& titulo,
                                       const std::string& autor) const {
    if (!impl_->db || titulo.empty()) return -1;
    sqlite3_stmt* sd = nullptr;
    std::int64_t existente = -1;
    if (sqlite3_prepare_v2(impl_->db,
            "SELECT id FROM cantos WHERE LOWER(titulo) = LOWER(?1) "
            "AND IFNULL(LOWER(autor),'') = IFNULL(LOWER(?2),'') LIMIT 1",
            -1, &sd, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(sd, 1, titulo.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(sd, 2, autor.c_str(), -1, SQLITE_TRANSIENT);
        if (sqlite3_step(sd) == SQLITE_ROW)
            existente = sqlite3_column_int64(sd, 0);
    }
    sqlite3_finalize(sd);
    return existente;
}

std::int64_t SongDatabase::InsertarCanto(const CantoDetalle& canto,
                                         const std::string& fuente) {
    if (!impl_->db || canto.titulo.empty()) return -1;

    // Idempotencia básica: no duplicar título+autor ya presentes.
    const std::int64_t existente =
        BuscarCanto(canto.titulo, canto.autor);
    if (existente >= 0) return existente;

    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(impl_->db,
            "INSERT INTO cantos (titulo, autor, tono_origen, bpm, fuente) "
            "VALUES (?1, ?2, ?3, ?4, ?5)",
            -1, &st, nullptr) != SQLITE_OK) return -1;
    sqlite3_bind_text(st, 1, canto.titulo.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, canto.autor.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 3, canto.tono_origen.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(st, 4, canto.bpm);
    sqlite3_bind_text(st, 5, fuente.c_str(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(st) != SQLITE_DONE) { sqlite3_finalize(st); return -1; }
    sqlite3_finalize(st);
    const std::int64_t canto_id = sqlite3_last_insert_rowid(impl_->db);

    for (size_t i = 0; i < canto.secciones.size(); ++i) {
        const CantoDetalle::Seccion& sec = canto.secciones[i];
        sqlite3_stmt* ss = nullptr;
        if (sqlite3_prepare_v2(impl_->db,
                "INSERT INTO canto_secciones (canto_id, orden, tipo, etiqueta) "
                "VALUES (?1, ?2, ?3, ?4)",
                -1, &ss, nullptr) != SQLITE_OK) continue;
        sqlite3_bind_int64(ss, 1, canto_id);
        sqlite3_bind_int(ss, 2, static_cast<int>(i));
        sqlite3_bind_text(ss, 3, sec.tipo.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(ss, 4, sec.etiqueta.c_str(), -1, SQLITE_TRANSIENT);
        const bool ok = sqlite3_step(ss) == SQLITE_DONE;
        sqlite3_finalize(ss);
        if (!ok) continue;
        const std::int64_t sec_id = sqlite3_last_insert_rowid(impl_->db);

        for (size_t j = 0; j < sec.lineas.size(); ++j) {
            sqlite3_stmt* sl = nullptr;
            if (sqlite3_prepare_v2(impl_->db,
                    "INSERT INTO canto_lineas (seccion_id, orden, texto) "
                    "VALUES (?1, ?2, ?3)",
                    -1, &sl, nullptr) != SQLITE_OK) continue;
            sqlite3_bind_int64(sl, 1, sec_id);
            sqlite3_bind_int(sl, 2, static_cast<int>(j));
            sqlite3_bind_text(sl, 3, sec.lineas[j].c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_step(sl);
            sqlite3_finalize(sl);
        }
    }
    return canto_id;
}

int SongDatabase::ImportarHolyricsJson(const std::string& ruta_json) {
    return HolyricsJson::Importar(*this, ruta_json);
}

} // namespace fusion
