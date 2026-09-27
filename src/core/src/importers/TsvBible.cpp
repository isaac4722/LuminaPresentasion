// src/core/src/importers/TsvBible.cpp — Importador TSV
// Formato: libro<TAB>capitulo<TAB>versiculo<TAB>texto

#include "fusion/importers/TsvBible.h"
#include "fusion/data/BibleDatabase.h"

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace fusion {

int TsvBible::Importar(BibleDatabase& db, const std::string& ruta_tsv) {
    std::ifstream f(ruta_tsv);
    if (!f) return 0;
    std::string linea;
    int n = 0;
    while (std::getline(f, linea, '\n')) {
        if (linea.empty()) continue;
        std::vector<std::string> campos;
        std::stringstream ls(linea);
        std::string c;
        while (std::getline(ls, c, '\t')) campos.push_back(c);
        if (campos.size() < 4) continue;
        // libro = campos[0], cap = campos[1], ver = campos[2], texto = campos[3]
        // TODO(P0): buscar libro por abrev3, crear/obtener capítulo, insertar versículo.
        ++n;
    }
    (void)db;
    return n;
}

} // namespace fusion
