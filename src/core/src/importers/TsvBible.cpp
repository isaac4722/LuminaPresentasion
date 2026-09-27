// src/core/src/importers/TsvBible.cpp — Importador TSV
// Formato: libro<TAB>capitulo<TAB>versiculo<TAB>texto
// Tolerante: BOM UTF-8, \r\n, encabezado (book|libro) y líneas vacías.

#include "fusion/importers/TsvBible.h"
#include "fusion/data/BibleDatabase.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace fusion {

namespace {

// Minúsculas ASCII para reconocer el encabezado (los textos reales no se
// tocan aquí: la normalización de acentos vive en BibleDatabase).
std::string AsciiMin(const std::string& s) {
    std::string out = s;
    for (char& c : out)
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    return out;
}

std::string QuitarBom(const std::string& s) {
    if (s.size() >= 3 &&
        static_cast<unsigned char>(s[0]) == 0xEF &&
        static_cast<unsigned char>(s[1]) == 0xBB &&
        static_cast<unsigned char>(s[2]) == 0xBF)
        return s.substr(3);
    return s;
}

std::string Recortar(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\r')) ++a;
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r')) --b;
    return s.substr(a, b - a);
}

} // namespace

int TsvBible::Importar(BibleDatabase& db, const std::string& ruta_tsv) {
    std::ifstream f(ruta_tsv, std::ios::binary);
    if (!f) return 0;

    int n = 0;
    std::string linea;
    bool primera = true;
    while (std::getline(f, linea, '\n')) {
        if (primera) {
            linea = QuitarBom(linea);
            primera = false;
        }
        if (linea.empty()) continue;

        std::vector<std::string> campos;
        std::stringstream ls(linea);
        std::string c;
        while (std::getline(ls, c, '\t')) campos.push_back(c);
        if (campos.size() < 4) continue;

        const std::string libro = Recortar(campos[0]);
        const std::string cap_s = Recortar(campos[1]);
        const std::string ver_s = Recortar(campos[2]);
        // El texto puede contener tabuladores internos: reensambla el resto.
        std::string texto = campos[3];
        for (size_t i = 4; i < campos.size(); ++i) texto += "\t" + campos[i];
        texto = Recortar(texto);
        if (libro.empty() || texto.empty()) continue;

        // Encabezado opcional del archivo.
        const std::string l0 = AsciiMin(libro);
        if (l0 == "book" || l0 == "libro") continue;

        const int cap = std::atoi(cap_s.c_str());
        const int ver = std::atoi(ver_s.c_str());
        if (cap <= 0 || ver <= 0) continue;

        if (db.InsertarVersiculo(libro, cap, ver, texto)) ++n;
    }
    return n;
}

} // namespace fusion
