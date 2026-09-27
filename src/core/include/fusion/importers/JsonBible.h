// src/core/include/fusion/importers/JsonBible.h
// Importador de biblias desde JSON interno de FUSION-HP.

#pragma once

#include <string>

namespace fusion {

class BibleDatabase;

class JsonBible {
public:
    // Importa una biblia en JSON interno a una BD de biblia.
    // Devuelve número de versículos añadidos.
    static int Importar(BibleDatabase& db, const std::string& ruta_json);
};

} // namespace fusion
