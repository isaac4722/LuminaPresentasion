// src/core/include/fusion/data/SqlScript.h — Utilidad interna:
// ejecución resiliente de scripts SQL.
//
// sqlite3_exec aborta el script en el primer error y el valor de la cola
// (tail) tras un prepare fallido no es fiable según la documentación.
// Para el esquema embebido hace falta lo contrario: que un statement
// opcional fallido (p.ej. FTS5 ausente) no impida la semilla de libros.
// Este particionador respeta literales ('...', "..."), corchetes [id],
// comentarios (-- y /* */) y, crítico: los cuerpos BEGIN...END de los
// triggers (contienen ';' internos y partirlos rompe el FTS — el trigger
// nunca se crea y la búsqueda queda sin índice).

#pragma once

#include "sqlite3.h"

#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace fusion::sqlutil {

// ¿Termina 's' con la palabra clave (sin distinguir mayúsculas), con
// frontera de palabra a la izquierda? Se usa para BEGIN y END.
inline bool TerminaConPalabra(const std::string& s, const char* clave) {
    const size_t n = std::strlen(clave);
    if (s.size() < n) return false;
    for (size_t k = 0; k < n; ++k) {
        if (std::tolower(static_cast<unsigned char>(s[s.size() - n + k])) !=
            std::tolower(static_cast<unsigned char>(clave[k])))
            return false;
    }
    const size_t i = s.size() - n;
    if (i == 0) return true;
    const unsigned char antes = static_cast<unsigned char>(s[i - 1]);
    return !(std::isalnum(antes) || antes == '_');
}

inline std::vector<std::string> Partir(const std::string& script) {
    std::vector<std::string> out;
    std::string actual;
    char en_literal = 0;            // 0 · '\'' · '"' · '['
    bool en_trigger = false;        // dentro de un cuerpo BEGIN...END
    auto es_letra = [](unsigned char c) {
        return std::isalpha(c) || c == '_';
    };

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

        // Detección de "BEGIN" al terminar la palabra (sin distinguir
        // mayúsculas): activa el modo trigger solo si el statement es
        // CREATE TRIGGER (así un BEGIN TRANSACTION se parte con
        // normalidad).
        if (en_literal == 0 && c == 'N' &&
            actual.size() >= 4 &&
            std::tolower(static_cast<unsigned char>(actual[actual.size() - 4])) == 'b' &&
            std::tolower(static_cast<unsigned char>(actual[actual.size() - 3])) == 'e' &&
            std::tolower(static_cast<unsigned char>(actual[actual.size() - 2])) == 'g' &&
            (actual.size() == 4 ||
             !es_letra(static_cast<unsigned char>(actual[actual.size() - 5])))) {
            actual += c;
            // CREATE TRIGGER ... BEGIN → el cuerpo lleva ';' internos.
            if (actual.size() >= 14) {
                std::string minusculas;
                minusculas.reserve(actual.size());
                for (char ch : actual)
                    minusculas.push_back(static_cast<char>(
                        std::tolower(static_cast<unsigned char>(ch))));
                en_trigger = minusculas.find("create trigger") != std::string::npos;
            }
            continue;
        }

        if (c == ';') {
            // En un trigger, el ';' solo cierra el statement si cierra
            // el END del cuerpo; los ';' internos se conservan.
            if (en_trigger) {
                std::string sin_espacios = actual;
                while (!sin_espacios.empty() &&
                       std::isspace(static_cast<unsigned char>(sin_espacios.back())))
                    sin_espacios.pop_back();
                if (TerminaConPalabra(sin_espacios, "END")) {
                    en_trigger = false;
                } else {
                    actual += c;
                    continue;
                }
            }
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
