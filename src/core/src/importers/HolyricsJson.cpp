// src/core/src/importers/HolyricsJson.cpp — Importador Holyrics

#include "fusion/importers/HolyricsJson.h"
#include "fusion/data/SongDatabase.h"

#include "json.hpp"
#include "sqlite3.h"

#include <fstream>
#include <sstream>
#include <string>

namespace fusion {

int HolyricsJson::Importar(SongDatabase& db, const std::string& ruta_json) {
    std::ifstream f(ruta_json, std::ios::binary);
    if (!f) return 0;
    std::stringstream ss; ss << f.rdbuf();
    auto j = nlohmann::json::parse(ss.str(), nullptr, false);
    if (j.is_discarded()) return 0;

    // El sample tiene { "song": { "title":..., "slides":[...] } }
    auto song = j.value("song", nlohmann::json::object());
    std::string titulo = song.value("title", "");
    std::string autor  = song.value("author", "");
    std::string tono   = song.value("key", "");
    int bpm            = song.value("bpm", 0);

    // TODO(P0): insert en cantos + canto_secciones + canto_lineas.
    // Por ahora devolvemos 1 si había título, 0 si no.
    if (titulo.empty()) return 0;
    (void)db; (void)autor; (void)tono; (void)bpm;
    return 1;
}

} // namespace fusion
