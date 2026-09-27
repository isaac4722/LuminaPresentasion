// src/core/src/importers/HolyricsJson.cpp — Importador de cantos Holyrics
//
// Acepta tres formas (tolerante):
//   1. Canto único:        { "song": { "title", "author", "key", "bpm",
//                                      "slides": [ { "name", "type", "text" } ] } }
//   2. Lista de cantos:    { "songs": [ ...canto... ] }
//   3. Respaldo completo:  { "Data": { "Songs": [ ...canto... ] } }
// Los campos alternan mayúscula inicial según la forma del respaldo
// (title/Title, slides/Slides...); el lector prueba ambas.
//
// Cada slide → una sección (Verso/Coro/...). El texto se parte por líneas.
// La inserción usa SongDatabase::InsertarCanto (idempotente por
// título+autor), así que reimportar no duplica cantos.

#include "fusion/importers/HolyricsJson.h"
#include "fusion/data/SongDatabase.h"

#include "json.hpp"

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace fusion {

namespace {

std::string CampoStr(const nlohmann::json& j, std::initializer_list<const char*> claves) {
    for (const char* c : claves) {
        auto it = j.find(c);
        if (it != j.end() && it->is_string())
            return it->get<std::string>();
    }
    return "";
}

int CampoInt(const nlohmann::json& j, std::initializer_list<const char*> claves) {
    for (const char* c : claves) {
        auto it = j.find(c);
        if (it != j.end() && it->is_number_integer())
            return it->get<int>();
    }
    return 0;
}

// Normaliza el tipo de slide al CHECK de canto_secciones:
// verso | coro | puente | intro | outro | instrumental | tag | estrofa
std::string TipoSeccion(const std::string& tipo_bruto, const std::string& etiqueta) {
    std::string t;
    for (char c : tipo_bruto) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
        if (c != ' ' && c != '-') t += c;
    }
    if (t == "verse" || t == "verso" || t == "verso1") return "verso";
    if (t == "chorus" || t == "coro") return "coro";
    if (t == "bridge" || t == "puente") return "puente";
    if (t == "intro") return "intro";
    if (t == "outro") return "outro";
    if (t == "instrumental") return "instrumental";
    if (t == "tag") return "tag";
    // Fallback por la etiqueta ("Coro", "Pre-coros", ...).
    std::string e;
    for (char c : etiqueta) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
        e += c;
    }
    if (e.find("coro") != std::string::npos) return "coro";
    if (e.find("puente") != std::string::npos) return "puente";
    if (e.find("verso") != std::string::npos || e.find("vers") != std::string::npos)
        return "verso";
    return "estrofa";
}

bool InsertarDesdeJson(SongDatabase& db, const nlohmann::json& raiz,
                       int* insertados) {
    CantoDetalle canto;
    canto.titulo = CampoStr(raiz, {"title", "Title"});
    if (canto.titulo.empty()) return false;
    canto.autor       = CampoStr(raiz, {"author", "Author", "artist", "Artist"});
    canto.tono_origen = CampoStr(raiz, {"key", "Key"});
    canto.bpm         = CampoInt(raiz, {"bpm", "BPM", "Bpm"});

    const auto slides = [&raiz]() -> nlohmann::json {
        for (const char* c : {"slides", "Slides"}) {
            auto it = raiz.find(c);
            if (it != raiz.end() && it->is_array()) return *it;
        }
        return nlohmann::json::array();
    }();

    int orden = 0;
    for (const auto& sl : slides) {
        std::string texto = CampoStr(sl, {"text", "Text"});
        if (texto.empty()) continue;
        CantoDetalle::Seccion sec;
        sec.etiqueta = CampoStr(sl, {"name", "Name"});
        sec.tipo     = TipoSeccion(CampoStr(sl, {"type", "Type"}), sec.etiqueta);
        sec.orden    = orden++;

        std::string linea;
        std::istringstream ls(texto);
        while (std::getline(ls, linea, '\n')) {
            while (!linea.empty() && (linea.back() == '\r' || linea.back() == ' '))
                linea.pop_back();
            if (!linea.empty()) sec.lineas.push_back(linea);
        }
        if (!sec.lineas.empty()) canto.secciones.push_back(sec);
    }

    // Sin slides: intentar campo "text"/"Text" plano, partiéndolo en
    // secciones por líneas en blanco.
    if (canto.secciones.empty()) {
        std::string plano = CampoStr(raiz, {"text", "Text", "lyrics", "Lyrics"});
        if (!plano.empty()) {
            CantoDetalle::Seccion sec;
            sec.etiqueta = "Letra";
            sec.tipo     = "estrofa";
            sec.orden    = orden++;
            std::string linea;
            std::istringstream ls(plano);
            while (std::getline(ls, linea, '\n')) {
                while (!linea.empty() && (linea.back() == '\r' || linea.back() == ' '))
                    linea.pop_back();
                if (!linea.empty()) sec.lineas.push_back(linea);
            }
            if (!sec.lineas.empty()) canto.secciones.push_back(sec);
        }
    }

    if (canto.secciones.empty()) return false;
    // No contar como nuevo el canto que ya estaba en la BD.
    if (db.BuscarCanto(canto.titulo, canto.autor) >= 0) return false;
    const std::int64_t id = db.InsertarCanto(canto, "Holyrics");
    if (id < 0) return false;
    ++*insertados;
    return true;
}

} // namespace

int HolyricsJson::Importar(SongDatabase& db, const std::string& ruta_json) {
    std::ifstream f(ruta_json, std::ios::binary);
    if (!f) return 0;
    std::stringstream ss; ss << f.rdbuf();
    auto j = nlohmann::json::parse(ss.str(), nullptr, false);
    if (j.is_discarded() || !j.is_object()) return 0;

    int insertados = 0;

    // Forma 1: canto único.
    auto it_song = j.find("song");
    if (it_song != j.end() && it_song->is_object()) {
        InsertarDesdeJson(db, *it_song, &insertados);
        return insertados;
    }

    // Forma 2: { "songs": [...] }  ·  Forma 3: { "Data": { "Songs": [...] } }
    const nlohmann::json* lista = nullptr;
    auto it_songs = j.find("songs");
    if (it_songs != j.end() && it_songs->is_array()) {
        lista = &*it_songs;
    } else {
        auto it_data = j.find("Data");
        if (it_data != j.end() && it_data->is_object()) {
            auto it_back = it_data->find("Songs");
            if (it_back != it_data->end() && it_back->is_array())
                lista = &*it_back;
        }
    }
    if (lista) {
        for (const auto& item : *lista)
            InsertarDesdeJson(db, item, &insertados);
        return insertados;
    }

    return insertados;
}

} // namespace fusion
