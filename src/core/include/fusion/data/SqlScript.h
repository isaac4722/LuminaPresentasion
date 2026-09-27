// src/core/include/fusion/data/SqlScript.h — Utilidad interna:
// ejecución resiliente de scripts SQL.
//
// sqlite3_exec aborta el script en el primer error y el valor de la cola
// (tail) tras un prepare fallido no es fiable según la documentación.
// Para el esquema embebido hace falta lo contrario: que un statement
// opcional fallido (p.ej. FTS5 ausente) no impida la semilla de libros.
// Este particionador propio respeta literales ('...', "..."), corchetes
// [id] y comentarios (-- y /* */) antes de dividir por ';'.

#pragma once

#include "sqlite3.h"

#include <cstdio>
#include <string>
#include <vector>

namespace fusion::sqlutil {

inline std::vector<std::string> Partir(const std::string& script) {
    std::vector<std::string> out;
    std::string actual;
    char en_literal = 0;            // 0 · '\'' · '"' · '['
    for (size_t i = 0; i < script.size(); ++i) {
        const char c = script[i];
        if (en_literal == 0 && c == '-' && i + 1 < script.size() &&
            script[i + 1] == '-') {
            while (i < script.size() && script[i] != '\n') ++i;
            actual += ' ';
            continue;
        }
        if (en_literal == 0 && c == '/' && i + 1 < script.size() &&
            script[i + 1] == '*') {
            i += 2;
            while (i + 1 < script.size() &&
                   !(script[i] == '*' && script[i + 1] == '/'))
                ++i;
            i = (i + 1 < script.size()) ? i + 1 : i;
            actual += ' ';
            continue;
        }
        if (en_literal == '\'') {
            actual += c;
            if (c == '\'') {
                if (i + 1 < script.size() && script[i + 1] == '\'') {
                    actual += '\''; ++i;      // '' escapado
                } else {
                    en_literal = 0;
                }
            }
            continue;
        }
        if (en_literal == '"') {
            actual += c;
            if (c == '"') en_literal = 0;
            continue;
        }
        if (en_literal == '[') {
            actual += c;
            if (c == ']') en_literal = 0;
            continue;
        }
        if (c == '\'' || c == '"' || c == '[') {
            en_literal = c;
            actual += c;
            continue;
        }
        if (c == ';') {
            if (!actual.empty() &&
                actual.find_first_not_of(" \t\r\n") != std::string::npos)
                out.push_back(actual);
            actual.clear();
            continue;
        }
        actual += c;
    }
    if (!actual.empty() &&
        actual.find_first_not_of(" \t\r\n") != std::string::npos)
        out.push_back(actual);
    return out;
}

inline bool EjecutarScript(sqlite3* db, const char* sql) {
    if (!db || !sql) return false;
    bool ok = true;
    for (const std::string& stmt : Partir(sql)) {
        sqlite3_stmt* st = nullptr;
        if (sqlite3_prepare_v2(db, stmt.c_str(), -1, &st, nullptr) != SQLITE_OK) {
            ok = false;
            std::fprintf(stderr, "FusionHP esquema: %s | %.60s\n",
                         sqlite3_errmsg(db), stmt.c_str());
            continue;                       // sigue con el resto del script
        }
        if (!st) continue;                  // statement vacío
        while (sqlite3_step(st) == SQLITE_ROW) {}
        if (sqlite3_finalize(st) != SQLITE_OK) ok = false;
    }
    return ok;
}

} // namespace fusion::sqlutil
