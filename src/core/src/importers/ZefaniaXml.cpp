// src/core/src/importers/ZefaniaXml.cpp — Importador Zefania XML
//
// Formato Zefania (zef2005):
//   <XMLBIBLE biblename="...">
//     <BIBLEBOOK bnumber="1" bname="Génesis" ...>
//       <CHAPTER cnumber="1">
//         <VERS vnumber="1">texto <NOTE>...</NOTE> texto</VERS>
//
// Parser tolerante sin DOM: escanea tags, mantiene estado
// libro → capítulo → versículo, decodifica entidades y elimina tags
// internos (STYLE/NOTE/red letters). Cubre variantes de atributos
// (bname/bookname, bnumber/booknumber, cnumber, vnumber).

#include "fusion/importers/ZefaniaXml.h"
#include "fusion/data/BibleDatabase.h"

#include <cctype>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace fusion {

namespace {

const char* const kNombresCanon[67] = { nullptr,
    "Génesis", "Éxodo", "Levítico", "Números", "Deuteronomio", "Josué",
    "Jueces", "Rut", "1 Samuel", "2 Samuel", "1 Reyes", "2 Reyes",
    "1 Crónicas", "2 Crónicas", "Esdras", "Nehemías", "Ester", "Job",
    "Salmos", "Proverbios", "Eclesiastés", "Cantares", "Isaías",
    "Jeremías", "Lamentaciones", "Ezequiel", "Daniel", "Oseas", "Joel",
    "Amós", "Abdías", "Jonás", "Miqueas", "Nahúm", "Habacuc", "Sofonías",
    "Hageo", "Zacarías", "Malaquías", "Mateo", "Marcos", "Lucas", "Juan",
    "Hechos", "Romanos", "1 Corintios", "2 Corintios", "Gálatas",
    "Efesios", "Filipenses", "Colosenses", "1 Tesalonicenses",
    "2 Tesalonicenses", "1 Timoteo", "2 Timoteo", "Tito", "Filemón",
    "Hebreos", "Santiago", "1 Pedro", "2 Pedro", "1 Juan", "2 Juan",
    "3 Juan", "Judas", "Apocalipsis" };

std::string Minusculas(const std::string& s) {
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

// Nombre del tag sin < > ni atributos: "biblebook", "/chapter", "vers".
// Los tags de cierre conservan la "/" inicial ("chapter" ≠ "/chapter"),
// de modo que EsTag solo reconoce aperturas.
std::string NombreTag(const std::string& tag) {
    size_t i = 1;                       // salta '<'
    std::string nombre;
    if (i < tag.size() && tag[i] == '/') { nombre += '/'; ++i; }
    while (i < tag.size() && tag[i] != '>' && tag[i] != ' ' &&
           tag[i] != '\t' && tag[i] != '\n' && tag[i] != '\r') {
        nombre += tag[i];
        ++i;
    }
    return Minusculas(nombre);
}

bool EsTag(const std::string& tag, const char* nombre) {
    return NombreTag(tag) == Minusculas(nombre);
}

bool EsAutoCerrado(const std::string& tag) {
    return tag.size() >= 2 && tag[tag.size() - 2] == '/';
}

// Devuelve el valor del primer atributo de la lista que exista en el tag
// (comparación de nombre insensible a mayúsculas).
std::string Atributo(const std::string& tag,
                     const std::vector<std::string>& nombres) {
    const std::string t = Minusculas(tag);
    for (const std::string& nombre : nombres) {
        const std::string patron = Minusculas(nombre) + "=";
        size_t p = t.find(patron);
        while (p != std::string::npos) {
            // Debe ser un nombre de atributo (inicio o precedido de espacio).
            if (p == 0 || t[p - 1] == ' ' || t[p - 1] == '\t' ||
                t[p - 1] == '\n' || t[p - 1] == '\r') {
                p += patron.size();
                if (p >= t.size()) return "";
                const char comilla = t[p];
                if (comilla != '"' && comilla != '\'') { p = t.find(patron, p); continue; }
                const size_t cierra = t.find(comilla, p + 1);
                if (cierra == std::string::npos) return "";
                return tag.substr(p + 1, cierra - p - 1);  // original, no min
            }
            p = t.find(patron, p + 1);
        }
    }
    return "";
}

int AtributoInt(const std::string& tag,
                const std::vector<std::string>& nombres) {
    return std::atoi(Atributo(tag, nombres).c_str());
}

// Elimina spans <NOTE ...>...</NOTE> completos (incluye su contenido).
void QuitarNotas(std::string& texto) {
    for (;;) {
        const size_t ini = Minusculas(texto).find("<note");
        if (ini == std::string::npos) break;
        const size_t fin = Minusculas(texto).find("</note>", ini);
        const size_t hasta = fin == std::string::npos ? texto.size() : fin + 7;
        texto.erase(ini, hasta - ini);
    }
}

// Elimina tags internos restantes (<STYLE ...>, <S>...</S>, etc.).
void QuitarTagsInternos(std::string& texto) {
    for (;;) {
        const size_t ini = texto.find('<');
        if (ini == std::string::npos) break;
        const size_t fin = texto.find('>', ini);
        if (fin == std::string::npos) { texto.erase(ini); break; }
        texto.erase(ini, fin - ini + 1);
    }
}

std::string DecodificarEntidades(std::string texto) {
    struct Par { const char* de; const char* a; };
    static const Par tabla[] = {
        {"&amp;", "&"}, {"&lt;", "<"}, {"&gt;", ">"},
        {"&quot;", "\""}, {"&apos;", "'"}, {"&nbsp;", " "},
    };
    for (const Par& p : tabla) {
        size_t pos = 0;
        while ((pos = texto.find(p.de, pos)) != std::string::npos) {
            texto.replace(pos, std::string(p.de).size(), p.a);
            pos += std::string(p.a).size();
        }
    }
    // Entidades numéricas decimales: &#233; → UTF-8 del codepoint (< 256).
    for (;;) {
        const size_t ini = texto.find("&#");
        if (ini == std::string::npos) break;
        const size_t fin = texto.find(';', ini);
        if (fin == std::string::npos || fin - ini > 8) break;
        const int cp = std::atoi(texto.substr(ini + 2, fin - ini - 2).c_str());
        if (cp <= 0) { texto.erase(ini, fin - ini + 1); continue; }
        std::string utf8;
        if (cp < 0x80) utf8 += static_cast<char>(cp);
        else if (cp < 0x800) {
            utf8 += static_cast<char>(0xC0 | (cp >> 6));
            utf8 += static_cast<char>(0x80 | (cp & 0x3F));
        } else {
            utf8 += static_cast<char>(0xE0 | (cp >> 12));
            utf8 += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            utf8 += static_cast<char>(0x80 | (cp & 0x3F));
        }
        texto.replace(ini, fin - ini + 1, utf8);
    }
    return texto;
}

std::string Recortar(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
    return s.substr(a, b - a);
}

} // namespace

int ZefaniaXml::Importar(BibleDatabase& db, const std::string& ruta_xml) {
    std::ifstream f(ruta_xml, std::ios::binary);
    if (!f) return 0;
    std::stringstream ss; ss << f.rdbuf();
    const std::string xml = QuitarBom(ss.str());
    if (xml.find("XMLBIBLE") == std::string::npos) return 0;

    std::string libro_nombre;
    int  libro_num  = 0;
    int  cap_actual = 0;
    int  n          = 0;

    size_t pos = 0;
    while (true) {
        const size_t t = xml.find('<', pos);
        if (t == std::string::npos) break;
        const size_t fin_tag = xml.find('>', t);
        if (fin_tag == std::string::npos) break;
        const std::string tag = xml.substr(t, fin_tag - t + 1);
        pos = fin_tag + 1;

        if (EsTag(tag, "BIBLEBOOK") && !EsAutoCerrado(tag)) {
            libro_num   = AtributoInt(tag, {"bnumber", "booknumber"});
            libro_nombre = Atributo(tag, {"bname", "bookname", "bookabbrev"});
            cap_actual  = 0;
        } else if (EsTag(tag, "CHAPTER")) {
            cap_actual = AtributoInt(tag, {"cnumber", "chapternumber"});
        } else if (EsTag(tag, "VERS") && !EsAutoCerrado(tag)) {
            const int ver = AtributoInt(tag, {"vnumber", "versenumber"});

            // Texto del versículo: hasta el cierre </VERS>.
            size_t fin_texto = pos;
            while (fin_texto != std::string::npos) {
                fin_texto = xml.find("</VERS", fin_texto);
                if (fin_texto == std::string::npos) break;
                // Confirmar que no es "</VERSOS" u otro tag con prefijo.
                const char sig = xml.size() > fin_texto + 6 ? xml[fin_texto + 6] : '>';
                if (sig == '>' || sig == ' ' || sig == '\t' ||
                    sig == '\r' || sig == '\n') break;
                ++fin_texto;
            }
            if (fin_texto == std::string::npos) break;
            std::string texto = xml.substr(pos, fin_texto - pos);
            pos = fin_texto;

            QuitarNotas(texto);
            QuitarTagsInternos(texto);
            texto = Recortar(DecodificarEntidades(texto));

            if (ver <= 0 || cap_actual <= 0 || texto.empty()) continue;
            const char* canon = (libro_num >= 1 && libro_num <= 66)
                                    ? kNombresCanon[libro_num] : nullptr;
            const std::string nombre = canon ? canon : libro_nombre;
            if (nombre.empty()) continue;
            if (db.InsertarVersiculo(nombre, cap_actual, ver, texto)) ++n;
        }
    }
    return n;
}

} // namespace fusion
