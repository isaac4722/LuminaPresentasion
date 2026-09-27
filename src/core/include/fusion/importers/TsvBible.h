// src/core/include/fusion/importers/TsvBible.h
// Importador de biblias desde TSV (libro\tcapitulo\tversiculo\ttexto).

#pragma once

#include <string>

namespace fusion {

class BibleDatabase;

class TsvBible {
public:
    // Importa una biblia en TSV a una BD de biblia.
    // Devuelve número de versículos añadidos.
    static int Importar(BibleDatabase& db, const std::string& ruta_tsv);
};

} // namespace fusion
