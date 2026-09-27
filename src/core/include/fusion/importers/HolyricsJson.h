// src/core/include/fusion/importers/HolyricsJson.h
// Importador de cantos desde respaldos Holyrics (.json).

#pragma once

#include <string>

namespace fusion {

class SongDatabase;

class HolyricsJson {
public:
    // Importa cantos desde un backup Holyrics a la BD de cantos.
    // Devuelve número de cantos añadidos.
    static int Importar(SongDatabase& db, const std::string& ruta_json);
};

} // namespace fusion
