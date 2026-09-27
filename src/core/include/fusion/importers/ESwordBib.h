// src/core/include/fusion/importers/ESwordBib.h
// Importador de biblias desde e-Sword .bib/.bblx (cifrado Twofish).
// Usa BouncyCastle en la capa gestionada; aquí delega si hace falta.

#pragma once

#include <string>

namespace fusion {

class BibleDatabase;

class ESwordBib {
public:
    // Importa una biblia e-Sword .bib a una BD de biblia.
    // Devuelve número de versículos añadidos.
    static int Importar(BibleDatabase& db, const std::string& ruta_bib);

    // Descifra Twofish. La clave depende de la versión de e-Sword.
    static bool DescifrarTwofish(const std::string& entrada,
                                 std::string* salida,
                                 std::string* msg_error);
};

} // namespace fusion
