// src/core/src/importers/JsonBible.cpp — Importador JSON interno

#include "fusion/importers/JsonBible.h"
#include "fusion/data/BibleDatabase.h"

#include "json.hpp"
#include "sqlite3.h"

#include <fstream>
#include <sstream>
#include <string>

namespace fusion {

int JsonBible::Importar(BibleDatabase& db, const std::string& ruta_json) {
    std::ifstream f(ruta_json, std::ios::binary);
    if (!f) return 0;
    std::stringstream ss; ss << f.rdbuf();
    auto j = nlohmann::json::parse(ss.str(), nullptr, false);
    if (j.is_discarded()) return 0;

    int n = 0;
    // Estructura esperada (ver data/samples/bible_sample.json):
    // { biblia: {...}, libros: [ { abrev3, nombre, capitulos: [ { numero, versiculos: [...] } ] } ] }
    const auto& libros = j.value("libros", nlohmann::json::array());
    for (const auto& lj : libros) {
        // TODO(P0): buscar libro por abrev3, crear capítulo, insertar versículos.
        for (const auto& cj : lj.value("capitulos", nlohmann::json::array())) {
            for (const auto& vj : cj.value("versiculos", nlohmann::json::array())) {
                (void)vj;
                ++n;
            }
        }
    }
    (void)db;
    return n;
}

} // namespace fusion
