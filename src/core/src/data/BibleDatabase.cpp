// src/core/src/data/BibleDatabase.cpp — Una biblia por .fdb (SQLite)

#include "fusion/data/BibleDatabase.h"
#include "fusion/importers/ZefaniaXml.h"
#include "fusion/importers/ESwordBib.h"
#include "fusion/importers/JsonBible.h"
#include "fusion/importers/TsvBible.h"

#include "sqlite3.h"
#include "esquema_biblia.h"   // generado por CMake: esquema embebido
#include "fusion/data/SqlScript.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <map>
#include <string>
#include <vector>

namespace fusion {

// ---------------------------------------------------------------------------
// Normalización y parser de citas ("Salmo 100:4", "Juan 3:16-18", "Gn 1:1").
// La normalización quita acentos UTF-8, baja a minúsculas ASCII y colapsa
// espacios, de modo que "Génesis", "GENESIS" y "genesis" comparan igual.
// ---------------------------------------------------------------------------
namespace {

char AcentoSimple(const std::string& par) {
    // Solo secuencia UTF-8 de 2 bytes con lead 0xC3 (letras latinas).
    if (par[0] != '\xC3' ) return 0;
    static const std::map<unsigned char, char> tabla = {
        {0xA1,'a'},{0xA9,'e'},{0xAD,'i'},{0xB3,'o'},{0xBA,'u'},{0xBC,'u'},{0xB1,'n'},
        {0x81,'a'},{0x89,'e'},{0x8D,'i'},{0x93,'o'},{0x9A,'u'},{0x9C,'u'},{0x91,'n'},
    };
    auto it = tabla.find(static_cast<unsigned char>(par[1]));
    return it == tabla.end() ? 0 : it->second;
}

std::string Normalizar(const std::string& entrada) {
    std::string out;
    out.reserve(entrada.size());
    bool en_espacio = true;
    for (size_t i = 0; i < entrada.size();) {
        unsigned char c = static_cast<unsigned char>(entrada[i]);
        if (c == 0xC3 && i + 1 < entrada.size()) {
            char simple = AcentoSimple(entrada.substr(i, 2));
            if (simple) { out += simple; en_espacio = false; i += 2; continue; }
        }
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            if (!en_espacio) { out += ' '; en_espacio = true; }
            ++i;
            continue;
        }
        out += (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a')
                                       : entrada[i];
        en_espacio = false;
        ++i;
    }
    while (!out.empty() && out.back() == ' ') out.pop_back();
    return out;
}

bool LeerNumero(const std::string& s, size_t& pos, int* out) {
    if (pos >= s.size() || !std::isdigit(static_cast<unsigned char>(s[pos])))
        return false;
    long v = 0;
    while (pos < s.size() && std::isdigit(static_cast<unsigned char>(s[pos]))) {
        v = v * 10 + (s[pos] - '0');
        if (v > 100000) return false;
        ++pos;
    }
    *out = static_cast<int>(v);
    return true;
}

struct CitaParseada {
    std::string libro;   // normalizado, con prefijo numérico si aplica
    int cap   = 0;
    int desde = 0;
    int hasta = 0;
};

// Acepta: "genesis 1:1" · "1 corintios 13:4-7" · "1corintios 13:4" ·
//         "salmo 100:4" · "juan 3:18-16" (rango invertido se corrige).
bool ParsearCita(const std::string& normalizada, CitaParseada* c) {
    const std::string& s = normalizada;
    size_t pos = 0;
    int prefijo = 0;
    if (LeerNumero(s, pos, &prefijo)) {
        // "1 corintios..." (espacio) o "1corintios..." (pegado). Un solo
        // número sin nombre ("23:15") no es una cita válida.
        if (pos >= s.size()) return false;
        if (s[pos] == ' ') ++pos;
        else if (!std::isalpha(static_cast<unsigned char>(s[pos]))) return false;
    }
    const size_t dos_puntos = s.find(':', pos);
    if (dos_puntos == std::string::npos) return false;

    // Izquierda: "nombre capitulo"
    std::string izq = s.substr(pos, dos_puntos - pos);
    while (!izq.empty() && izq.front() == ' ') izq.erase(izq.begin());
    while (!izq.empty() && izq.back() == ' ') izq.pop_back();
    const size_t sep = izq.find_last_of(' ');
    if (sep == std::string::npos) return false;
    const std::string nombre  = izq.substr(0, sep);
    const std::string cap_str = izq.substr(sep + 1);
    if (nombre.empty() || cap_str.empty()) return false;
    size_t p_cap = 0;
    int cap = 0;
    if (!LeerNumero(cap_str, p_cap, &cap) || p_cap != cap_str.size() || cap <= 0)
        return false;

    // Derecha: "desde[-hasta]"
    size_t p = dos_puntos + 1;
    int desde = 0, hasta = 0;
    if (!LeerNumero(s, p, &desde) || desde <= 0) return false;
    hasta = desde;
    if (p < s.size() && s[p] == '-') {
        ++p;
        if (!LeerNumero(s, p, &hasta) || hasta <= 0) return false;
    }
    if (p != s.size()) return false;   // texto sobrante → no es cita limpia
    if (hasta < desde) std::swap(desde, hasta);

    c->libro = prefijo > 0 ? std::to_string(prefijo) + " " + nombre : nombre;
    c->cap = cap;
    c->desde = desde;
    c->hasta = hasta;
    return true;
}

// Resuelve el id del libro comparando el texto normalizado contra nombre,
// abrev3 y abrev2; si no hay match directo, consulta la tabla de alias.
int ResolverLibroId(sqlite3* db, const std::string& libro_norm) {
    if (!db || libro_norm.empty()) return 0;

    std::map<std::string, int> indice;
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(db, "SELECT id, nombre, abrev3, abrev2 FROM libros",
                           -1, &st, nullptr) != SQLITE_OK)
        return 0;
    while (sqlite3_step(st) == SQLITE_ROW) {
        const int id = sqlite3_column_int(st, 0);
        for (int col = 1; col <= 3; ++col) {
            const unsigned char* t = sqlite3_column_text(st, col);
            if (!t) continue;
            indice[Normalizar(reinterpret_cast<const char*>(t))] = id;
        }
    }
    sqlite3_finalize(st);

    auto it = indice.find(libro_norm);
    if (it != indice.end()) return it->second;

    // Alias frecuentes (normalizados, sin acentos). Cubre formas coloquiales
    // en español y nombres/abreviaturas inglesas comunes en archivos
    // importados (Zefania, e-Sword).
    static const std::map<std::string, std::string> alias = {
        {"salmo", "salmos"},     {"ps", "salmos"},
        {"psalm", "salmos"},     {"psalms", "salmos"},
        {"cantar", "cantares"},  {"cantar de los cantares", "cantares"},
        {"song of solomon", "cantares"},
        {"genesis", "genesis"},  {"exodus", "exodo"},
        {"leviticus", "levitico"}, {"numbers", "numeros"},
        {"deuteronomy", "deuteronomio"}, {"joshua", "josue"},
        {"judges", "jueces"},    {"ruth", "rut"},
        {"ezra", "esdras"},      {"nehemiah", "nehemias"},
        {"esther", "ester"},     {"proverbs", "proverbios"},
        {"ecclesiastes", "eclesiastes"},
        {"isaiah", "isaias"},    {"jeremiah", "jeremias"},
        {"lamentations", "lamentaciones"}, {"ezekiel", "ezequiel"},
        {"hosea", "oseas"},      {"amos", "amos"},
        {"obadiah", "abdias"},   {"jonah", "jonas"},
        {"micah", "miqueas"},    {"habakkuk", "habacuc"},
        {"zephaniah", "sofonias"}, {"haggai", "hageo"},
        {"zechariah", "zacarias"}, {"malachi", "malaquias"},
        {"matthew", "mateo"},    {"matt", "mateo"},
        {"mark", "marcos"},      {"luke", "lucas"},
        {"john", "juan"},        {"acts", "hechos"},
        {"romans", "romanos"},   {"galatians", "galatas"},
        {"ephesians", "efesios"}, {"philippians", "filipenses"},
        {"colossians", "colosenses"},
        {"1 corinthians", "1 corintios"},
        {"2 corinthians", "2 corintios"},
        {"1 thessalonians", "1 tesalonicenses"},
        {"2 thessalonians", "2 tesalonicenses"},
        {"1 timothy", "1 timoteo"}, {"2 timothy", "2 timoteo"},
        {"titus", "tito"},       {"philemon", "filemon"},
        {"hebrews", "hebreos"},  {"james", "santiago"},
        {"1 peter", "1 pedro"},  {"2 peter", "2 pedro"},
        {"1 john", "1 juan"},    {"2 john", "2 juan"},
        {"3 john", "3 juan"},    {"jude", "judas"},
        {"revelation", "apocalipsis"}, {"rev", "apocalipsis"},
        {"exo", "exodo"},        {"gen", "genesis"},
        {"deu", "deuteronomio"}, {"mt", "mateo"}, {"mk", "marcos"},
        {"lk", "lucas"},         {"jn", "juan"},
        {"ro", "romanos"},       {"ap", "apocalipsis"},
        // Formas numeradas con abreviatura: el parser separa el prefijo
        // numérico y deja "1 sa" / "1co" → "1 co", etc.
        {"1 sa", "1 samuel"},    {"2 sa", "2 samuel"},
        {"1 re", "1 reyes"},     {"2 re", "2 reyes"},
        {"1 cr", "1 cronicas"},  {"2 cr", "2 cronicas"},
        {"1 co", "1 corintios"}, {"2 co", "2 corintios"},
        {"1 te", "1 tesalonicenses"}, {"2 te", "2 tesalonicenses"},
        {"1 ti", "1 timoteo"},   {"2 ti", "2 timoteo"},
        {"1 pe", "1 pedro"},     {"2 pe", "2 pedro"},
        {"1 jn", "1 juan"},      {"2 jn", "2 juan"},
        {"3 jn", "3 juan"},
    };
    auto ia = alias.find(libro_norm);
    if (ia != alias.end()) {
        it = indice.find(ia->second);
        if (it != indice.end()) return it->second;
    }
    return 0;
}

} // namespace

struct BibleDatabase::Impl {
    sqlite3* db = nullptr;

    // Ejecución resiliente del esquema (ver fusion/data/SqlScript.h):
    // un statement fallido no aborta el resto y el error queda en stderr.
    bool Ejecutar(const char* sql) {
        return sqlutil::EjecutarScript(db, sql);
    }
};

BibleDatabase::BibleDatabase()  : impl_(std::make_unique<Impl>()) {}
BibleDatabase::~BibleDatabase() { Cerrar(); }

bool BibleDatabase::Abrir(const std::string& ruta_fdb) {
    if (sqlite3_open(ruta_fdb.c_str(), &impl_->db) != SQLITE_OK) return false;
    // Esquema embebido en el binario (generado desde data/schema/bible.sql):
    // sin dependencia del CWD; CREATE IF NOT EXISTS + libros semilla 1..66.
    impl_->Ejecutar(fusion::esquema::kEsquemaBiblia);
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
    out->clear();
    if (!impl_->db) return false;

    CitaParseada c;
    if (!ParsearCita(Normalizar(cita), &c)) return false;
    const int libro_id = ResolverLibroId(impl_->db, c.libro);
    if (libro_id <= 0) return false;

    const char* sql =
        "SELECT l.nombre, c.numero, v.numero, v.texto "
        "FROM versiculos v "
        "JOIN capitulos c ON c.id = v.capitulo_id "
        "JOIN libros    l ON l.id = c.libro_id "
        "WHERE c.libro_id = ?1 AND c.numero = ?2 AND v.numero BETWEEN ?3 AND ?4 "
        "ORDER BY v.numero";
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(impl_->db, sql, -1, &st, nullptr) != SQLITE_OK)
        return false;
    sqlite3_bind_int(st, 1, libro_id);
    sqlite3_bind_int(st, 2, c.cap);
    sqlite3_bind_int(st, 3, c.desde);
    sqlite3_bind_int(st, 4, c.hasta);
    while (sqlite3_step(st) == SQLITE_ROW) {
        Versiculo v;
        const unsigned char* n = sqlite3_column_text(st, 0);
        v.libro     = n ? reinterpret_cast<const char*>(n) : "";
        v.capitulo  = sqlite3_column_int(st, 1);
        v.versiculo = sqlite3_column_int(st, 2);
        const unsigned char* t = sqlite3_column_text(st, 3);
        v.texto     = t ? reinterpret_cast<const char*>(t) : "";
        out->push_back(v);
    }
    sqlite3_finalize(st);
    return !out->empty();
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

bool BibleDatabase::InsertarVersiculo(const std::string& libro_abrev,
                                      int capitulo, int versiculo,
                                      const std::string& texto) {
    if (!impl_->db || capitulo <= 0 || versiculo <= 0) return false;

    const int libro_id =
        ResolverLibroId(impl_->db, Normalizar(libro_abrev));
    if (libro_id <= 0) return false;

    // Obtener o crear el capítulo.
    sqlite3_stmt* st = nullptr;
    std::int64_t cap_id = 0;
    if (sqlite3_prepare_v2(impl_->db,
            "SELECT id FROM capitulos WHERE libro_id = ?1 AND numero = ?2",
            -1, &st, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(st, 1, libro_id);
        sqlite3_bind_int(st, 2, capitulo);
        if (sqlite3_step(st) == SQLITE_ROW)
            cap_id = sqlite3_column_int64(st, 0);
    }
    sqlite3_finalize(st);

    if (cap_id == 0) {
        if (sqlite3_prepare_v2(impl_->db,
                "INSERT INTO capitulos (libro_id, numero) VALUES (?1, ?2)",
                -1, &st, nullptr) != SQLITE_OK)
            return false;
        sqlite3_bind_int(st, 1, libro_id);
        sqlite3_bind_int(st, 2, capitulo);
        if (sqlite3_step(st) != SQLITE_DONE) { sqlite3_finalize(st); return false; }
        sqlite3_finalize(st);
        cap_id = sqlite3_last_insert_rowid(impl_->db);
    }

    // INSERT OR REPLACE: reimportar la misma biblia no duplica versículos.
    if (sqlite3_prepare_v2(impl_->db,
            "INSERT OR REPLACE INTO versiculos (capitulo_id, numero, texto) "
            "VALUES (?1, ?2, ?3)",
            -1, &st, nullptr) != SQLITE_OK)
        return false;
    sqlite3_bind_int64(st, 1, cap_id);
    sqlite3_bind_int(st, 2, versiculo);
    sqlite3_bind_text(st, 3, texto.c_str(), -1, SQLITE_TRANSIENT);
    const bool ok = sqlite3_step(st) == SQLITE_DONE;
    sqlite3_finalize(st);
    return ok;
}

std::int64_t BibleDatabase::TotalVersiculos() const {
    if (!impl_->db) return 0;
    sqlite3_stmt* st = nullptr;
    std::int64_t n = 0;
    if (sqlite3_prepare_v2(impl_->db, "SELECT COUNT(*) FROM versiculos",
                           -1, &st, nullptr) == SQLITE_OK) {
        if (sqlite3_step(st) == SQLITE_ROW) n = sqlite3_column_int64(st, 0);
    }
    sqlite3_finalize(st);
    return n;
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
