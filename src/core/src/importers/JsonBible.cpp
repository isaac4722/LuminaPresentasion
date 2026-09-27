// src/core/src/importers/JsonBible.cpp — Importador JSON interno de biblias
//
// Formato de entrada (ver data/samples/bible_sample.json y
// data/bibles/RVR1909.json — mismo formato):
// {
//   "biblia": { "nombre", "abrev", "idioma", "derechos", "fuente" },
//   "libros": [
//     { "abrev3": "Gén", "nombre": "Génesis", "capitulos": [
//         { "numero": 1, "versiculos": [ { "numero": 1, "texto": "..." } ] }
//     ] }
//   ]
// }
//
// La inserción usa BibleDatabase::InsertarVersiculo (INSERT OR REPLACE),
// de modo que reimportar la misma biblia no duplica versículos.

#include "fusion/importers/JsonBible.h"
#include "fusion/data/BibleDatabase.h"

#include "json.hpp"

#include <fstream>
#include <sstream>
#include <string>

namespace fusion {

int JsonBible::Importar(BibleDatabase& db, const std::string& ruta_json) {
    std::ifstream f(ruta_json, std::ios::binary);
    if (!f) return 0;
    std::stringstream ss; ss << f.rdbuf();
    auto j = nlohmann::json::parse(ss.str(), nullptr, false);
    if (j.is_discarded() || !j.is_object()) return 0;

    int n = 0;
    const auto libros = j.value("libros", nlohmann::json::array());
    if (!libros.is_array()) return 0;

    for (const auto& lj : libros) {
        const std::string abrev3 = lj.value("abrev3", "");
        const std::string nombre = lj.value("nombre", "");
        const auto capitulos = lj.value("capitulos", nlohmann::json::array());
        if (!capitulos.is_array()) continue;

        for (const auto& cj : capitulos) {
            const int cap = cj.value("numero", 0);
            const auto versiculos = cj.value("versiculos", nlohmann::json::array());
            if (!versiculos.is_array()) continue;

            for (const auto& vj : versiculos) {
                const int num = vj.value("numero", 0);
                const std::string texto = vj.value("texto", "");
                if (cap <= 0 || num <= 0 || texto.empty()) continue;
                // Resuelve por abrev3; si el archivo usa otra forma, intenta
                // el nombre completo (el resolver acepta alias y acentos).
                if (db.InsertarVersiculo(abrev3, cap, num, texto) ||
                    (!nombre.empty() &&
                     db.InsertarVersiculo(nombre, cap, num, texto))) {
                    ++n;
                }
            }
        }
    }
    return n;
}

} // namespace fusion
